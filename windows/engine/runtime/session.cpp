/**
 * Engine — Core orchestrator implementation.
 */

#include "session.h"
#include <cstring>
#include <algorithm>
#include <cmath>

namespace auraminus {

Engine::Engine() {
    worker_ = std::thread(&Engine::worker_loop, this);
}

Engine::~Engine() {
    worker_stop_.store(true, std::memory_order_release);
    work_cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    stop();
    device_manager_.shutdown();
}

void Engine::worker_loop() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    while (!worker_stop_.load(std::memory_order_acquire)) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(work_mutex_);
            work_cv_.wait(lock, [this] {
                return worker_stop_.load(std::memory_order_acquire) || !work_queue_.empty();
            });
            if (worker_stop_.load(std::memory_order_acquire) && work_queue_.empty()) {
                break;
            }
            job = std::move(work_queue_.front());
            work_queue_.pop_front();
        }
        if (job) {
            job();
        }
    }

    CoUninitialize();
}

void Engine::enqueue_routing_refresh(bool restart_capture) {
    if (restart_capture) {
        capture_restart_pending_.store(true, std::memory_order_release);
    }
    routing_refresh_pending_.store(true, std::memory_order_release);

    {
        std::lock_guard<std::mutex> lock(work_mutex_);
            // Coalesce bursts (eMultimedia + eConsole DefaultChanged, plus StateChanged).
            work_queue_.clear();
            work_queue_.push_back([this] {
            // Brief settle time so Windows finishes flipping the default endpoint
            // (BT connect often raises StateChanged before DefaultChanged).
            Sleep(300);

            const bool restart = capture_restart_pending_.exchange(false, std::memory_order_acq_rel);
            routing_refresh_pending_.store(false, std::memory_order_release);

            std::lock_guard<std::mutex> engine_lock(mutex_);
            if (!running_.load(std::memory_order_acquire)) {
                return;
            }

            device_manager_.refresh_devices();
            std::wstring current_default = device_manager_.get_default_device_id();
            const bool default_changed = (current_default != capture_default_id_);

            if (restart || default_changed) {
                restart_capture_and_resync_locked();
            } else {
                resync_outputs_for_routing_locked();
            }
        });
    }
    work_cv_.notify_one();
}

void Engine::publish_outputs_locked() {
    auto snap = std::make_shared<OutputDeviceList>();
    snap->reserve(outputs_.size());
    for (auto& entry : outputs_) {
        snap->push_back(entry.device);
    }
    published_outputs_.store(std::move(snap), std::memory_order_release);
}

void Engine::fanout_audio(const float* data, uint32_t frames) {
    auto snap = published_outputs_.load(std::memory_order_acquire);
    if (!snap) return;
    for (auto& device : *snap) {
        if (device) {
            device->write(data, frames);
        }
    }
}

AuraminusResult Engine::activate_output_locked(const SelectedDevice& sel, uint32_t channels, uint32_t rate) {
    std::wstring default_id = device_manager_.get_default_device_id();
    if (!default_id.empty() && sel.id == default_id) {
        return AURAMINUS_OK; // Default plays natively — skip to avoid feedback
    }

    for (const auto& entry : outputs_) {
        if (entry.device_id == sel.id) {
            return AURAMINUS_OK; // Already active
        }
    }

    IMMDevice* mm_device = nullptr;
    HRESULT hr = device_manager_.get_device(sel.id, &mm_device);
    if (FAILED(hr) || !mm_device) {
        return AURAMINUS_ERROR_NOT_FOUND;
    }

    auto output = std::make_shared<OutputDevice>();
    hr = output->initialize(mm_device, sel.id, channels, rate);
    mm_device->Release();
    if (FAILED(hr)) {
        return (hr == AUDCLNT_E_UNSUPPORTED_FORMAT) ? AURAMINUS_ERROR_FORMAT : AURAMINUS_ERROR_AUDIO_CLIENT;
    }

    output->set_volume(sel.volume);
    output->set_delay_ms(sel.delay_ms);
    hr = output->start();
    if (FAILED(hr)) {
        return AURAMINUS_ERROR_AUDIO_CLIENT;
    }

    OutputEntry entry;
    entry.device_id = sel.id;
    entry.device = std::move(output);
    outputs_.push_back(std::move(entry));
    publish_outputs_locked();
    return AURAMINUS_OK;
}

void Engine::resync_outputs_for_routing_locked() {
    // Capture mix format may have changed with the new default endpoint, so rebuild
    // every Auraminus render pipeline against the current capture format.
    auto dying = std::move(outputs_);
    outputs_.clear();
    publish_outputs_locked();
    for (auto& entry : dying) {
        if (entry.device) {
            entry.device->stop();
        }
    }

    if (!capture_ || !running_.load(std::memory_order_acquire)) {
        return;
    }

    uint32_t channels = capture_->channels();
    uint32_t rate = capture_->sample_rate();
    for (const auto& sel : selected_devices_) {
        activate_output_locked(sel, channels, rate);
    }
}

void Engine::restart_capture_and_resync_locked() {
    if (capture_) {
        capture_->stop();
        capture_.reset();
    }

    // Drop existing outputs before capture format may change.
    auto dying = std::move(outputs_);
    outputs_.clear();
    publish_outputs_locked();
    for (auto& entry : dying) {
        if (entry.device) {
            entry.device->stop();
        }
    }

    capture_ = std::make_unique<LoopbackCapture>();
    HRESULT hr = capture_->initialize([this](const float* data, uint32_t frames) {
        fanout_audio(data, frames);
    });

    if (FAILED(hr) || FAILED(capture_->start())) {
        capture_.reset();
        capture_default_id_.clear();
        running_.store(false, std::memory_order_release);
        return;
    }

    capture_default_id_ = device_manager_.get_default_device_id();
    resync_outputs_for_routing_locked();
}

AuraminusResult Engine::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) return AURAMINUS_OK;

    HRESULT hr = device_manager_.initialize();
    if (FAILED(hr)) {
        return AURAMINUS_ERROR_COM;
    }

    device_manager_.set_event_callback([this](DeviceEventType type, const std::wstring& id) {
        on_device_event(type, id);
    });

    published_outputs_.store(std::make_shared<OutputDeviceList>(), std::memory_order_release);
    initialized_ = true;
    return AURAMINUS_OK;
}

AuraminusResult Engine::start() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) return AURAMINUS_ERROR_COM;
    if (running_.load(std::memory_order_acquire)) return AURAMINUS_ERROR_ALREADY_RUNNING;

    if (selected_devices_.empty()) {
        return AURAMINUS_ERROR_INVALID_ARG;
    }

    capture_ = std::make_unique<LoopbackCapture>();
    HRESULT hr = capture_->initialize([this](const float* data, uint32_t frames) {
        fanout_audio(data, frames);
    });
    if (FAILED(hr)) {
        capture_.reset();
        return (hr == AUDCLNT_E_UNSUPPORTED_FORMAT) ? AURAMINUS_ERROR_FORMAT : AURAMINUS_ERROR_AUDIO_CLIENT;
    }

    uint32_t cap_channels = capture_->channels();
    uint32_t cap_rate = capture_->sample_rate();
    std::wstring default_id = device_manager_.get_default_device_id();

    outputs_.clear();
    publish_outputs_locked();

    for (const auto& sel : selected_devices_) {
        if (!default_id.empty() && sel.id == default_id) {
            continue;
        }

        IMMDevice* mm_device = nullptr;
        hr = device_manager_.get_device(sel.id, &mm_device);
        if (FAILED(hr) || !mm_device) {
            continue;
        }

        auto output = std::make_shared<OutputDevice>();
        hr = output->initialize(mm_device, sel.id, cap_channels, cap_rate);
        mm_device->Release();

        if (SUCCEEDED(hr)) {
            output->set_volume(sel.volume);
            output->set_delay_ms(sel.delay_ms);
            OutputEntry entry;
            entry.device_id = sel.id;
            entry.device = std::move(output);
            outputs_.push_back(std::move(entry));
        }
    }

    publish_outputs_locked();

    hr = capture_->start();
    if (FAILED(hr)) {
        outputs_.clear();
        publish_outputs_locked();
        capture_.reset();
        return AURAMINUS_ERROR_AUDIO_CLIENT;
    }

    for (auto& entry : outputs_) {
        entry.device->start();
    }

    capture_default_id_ = default_id;
    running_.store(true, std::memory_order_release);
    return AURAMINUS_OK;
}

AuraminusResult Engine::stop() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_.load(std::memory_order_acquire)) {
        selected_devices_.clear();
        return AURAMINUS_OK;
    }

    // Drop capture first so fan-out cannot touch devices being stopped.
    if (capture_) {
        capture_->stop();
        capture_.reset();
    }

    auto dying = outputs_;
    outputs_.clear();
    publish_outputs_locked();

    for (auto& entry : dying) {
        if (entry.device) {
            entry.device->stop();
        }
    }

    selected_devices_.clear();
    capture_default_id_.clear();
    running_.store(false, std::memory_order_release);
    return AURAMINUS_OK;
}

AuraminusResult Engine::get_devices(AuraminusDeviceInfo* devices, int32_t capacity, int32_t* out_count) {
    if (!devices || capacity <= 0 || !out_count) return AURAMINUS_ERROR_INVALID_ARG;
    if (!initialized_) return AURAMINUS_ERROR_COM;

    auto device_list = device_manager_.get_devices();
    int32_t count = std::min(static_cast<int32_t>(device_list.size()), capacity);

    for (int32_t i = 0; i < count; ++i) {
        const auto& dev = device_list[i];

        std::string id_utf8 = wide_to_utf8(dev.id);
        std::string name_utf8 = wide_to_utf8(dev.name);

        std::memset(&devices[i], 0, sizeof(AuraminusDeviceInfo));
        strncpy(devices[i].id, id_utf8.c_str(), sizeof(devices[i].id) - 1);
        strncpy(devices[i].name, name_utf8.c_str(), sizeof(devices[i].name) - 1);
        devices[i].is_default = dev.is_default ? 1 : 0;
        devices[i].is_active = dev.is_active ? 1 : 0;
        devices[i].supports_render = dev.supports_render ? 1 : 0;
    }

    *out_count = count;
    return AURAMINUS_OK;
}

AuraminusResult Engine::refresh_devices() {
    if (!initialized_) return AURAMINUS_ERROR_COM;

    HRESULT hr = device_manager_.refresh_devices();
    return SUCCEEDED(hr) ? AURAMINUS_OK : AURAMINUS_ERROR_COM;
}

AuraminusResult Engine::add_output(const char* device_id) {
    if (!device_id) return AURAMINUS_ERROR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(mutex_);

    std::wstring wid = utf8_to_wide(device_id);

    for (const auto& sel : selected_devices_) {
        if (sel.id == wid) {
            if (running_.load(std::memory_order_acquire) && capture_) {
                SelectedDevice existing = sel;
                return activate_output_locked(existing, capture_->channels(), capture_->sample_rate());
            }
            return AURAMINUS_OK;
        }
    }

    if (!device_manager_.is_device_available(wid)) {
        return AURAMINUS_ERROR_NOT_FOUND;
    }

    SelectedDevice sel{wid, 1.0f, 0};
    selected_devices_.push_back(sel);

    if (running_.load(std::memory_order_acquire) && capture_) {
        return activate_output_locked(sel, capture_->channels(), capture_->sample_rate());
    }

    return AURAMINUS_OK;
}

AuraminusResult Engine::remove_output(const char* device_id) {
    if (!device_id) return AURAMINUS_ERROR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(mutex_);

    std::wstring wid = utf8_to_wide(device_id);

    auto it = std::find_if(selected_devices_.begin(), selected_devices_.end(),
        [&wid](const SelectedDevice& s) { return s.id == wid; });
    if (it == selected_devices_.end()) {
        return AURAMINUS_ERROR_NOT_FOUND;
    }

    selected_devices_.erase(it);

    auto out_it = std::find_if(outputs_.begin(), outputs_.end(),
        [&wid](const OutputEntry& e) { return e.device_id == wid; });
    if (out_it != outputs_.end()) {
        auto dying = out_it->device;
        outputs_.erase(out_it);
        publish_outputs_locked();
        if (dying) {
            dying->stop();
        }
    }

    return AURAMINUS_OK;
}

AuraminusResult Engine::set_volume(const char* device_id, float volume) {
    if (!device_id) return AURAMINUS_ERROR_INVALID_ARG;
    if (!std::isfinite(volume)) return AURAMINUS_ERROR_INVALID_ARG;

    volume = std::clamp(volume, 0.0f, 1.0f);

    std::lock_guard<std::mutex> lock(mutex_);

    std::wstring wid = utf8_to_wide(device_id);
    bool found = false;

    for (auto& sel : selected_devices_) {
        if (sel.id == wid) {
            sel.volume = volume;
            found = true;
            break;
        }
    }

    OutputDevice* output = find_output(wid);
    if (output) {
        output->set_volume(volume);
        found = true;
    }

    return found ? AURAMINUS_OK : AURAMINUS_ERROR_NOT_FOUND;
}

AuraminusResult Engine::set_delay(const char* device_id, uint32_t delay_ms) {
    if (!device_id) return AURAMINUS_ERROR_INVALID_ARG;

    delay_ms = std::min(delay_ms, 500u);

    std::lock_guard<std::mutex> lock(mutex_);

    std::wstring wid = utf8_to_wide(device_id);
    bool found = false;

    for (auto& sel : selected_devices_) {
        if (sel.id == wid) {
            sel.delay_ms = delay_ms;
            found = true;
            break;
        }
    }

    OutputDevice* output = find_output(wid);
    if (output) {
        output->set_delay_ms(delay_ms);
        found = true;
    }

    return found ? AURAMINUS_OK : AURAMINUS_ERROR_NOT_FOUND;
}

AuraminusResult Engine::get_delay(const char* device_id, uint32_t* out_delay_ms) {
    if (!device_id || !out_delay_ms) return AURAMINUS_ERROR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(mutex_);

    std::wstring wid = utf8_to_wide(device_id);

    OutputDevice* output = find_output(wid);
    if (output) {
        *out_delay_ms = output->get_delay_ms();
        return AURAMINUS_OK;
    }

    for (const auto& sel : selected_devices_) {
        if (sel.id == wid) {
            *out_delay_ms = sel.delay_ms;
            return AURAMINUS_OK;
        }
    }

    return AURAMINUS_ERROR_NOT_FOUND;
}

AuraminusResult Engine::get_status(AuraminusEngineStatus* status) {
    if (!status) return AURAMINUS_ERROR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(mutex_);

    std::memset(status, 0, sizeof(AuraminusEngineStatus));
    status->is_running = running_.load(std::memory_order_acquire) ? 1 : 0;

    if (capture_) {
        status->capture_sample_rate = static_cast<int32_t>(capture_->sample_rate());
        status->capture_channels = static_cast<int32_t>(capture_->channels());
        status->frames_captured = capture_->frames_captured();
    }

    status->output_count = static_cast<int32_t>(outputs_.size());

    uint64_t total_underruns = 0;
    uint64_t total_overruns = 0;
    for (const auto& entry : outputs_) {
        total_underruns += entry.device->underruns();
        total_overruns += entry.device->overruns();
    }
    status->buffer_underruns = total_underruns;
    status->buffer_overruns = total_overruns;

    return AURAMINUS_OK;
}

OutputDevice* Engine::find_output(const std::wstring& device_id) {
    for (auto& entry : outputs_) {
        if (entry.device_id == device_id) {
            return entry.device.get();
        }
    }
    return nullptr;
}

std::wstring Engine::utf8_to_wide(const char* str) {
    if (!str || !*str) return L"";

    int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
    if (len <= 0) return L"";

    std::wstring result(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str, -1, &result[0], len);
    return result;
}

std::string Engine::wide_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";

    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";

    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], len, nullptr, nullptr);
    return result;
}

void Engine::on_device_event(DeviceEventType type, const std::wstring& device_id) {
    AuraminusDeviceEventCallback cb = nullptr;
    std::string utf8_id;
    int32_t event_code = static_cast<int32_t>(type);
    bool schedule_routing = false;
    bool restart_capture = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (type == DeviceEventType::Removed || type == DeviceEventType::StateChanged) {
            bool is_active = device_manager_.is_device_available(device_id);
            if (!is_active) {
                auto it = std::find_if(outputs_.begin(), outputs_.end(),
                    [&device_id](const OutputEntry& e) { return e.device_id == device_id; });
                if (it != outputs_.end()) {
                    auto dying = it->device;
                    outputs_.erase(it);
                    publish_outputs_locked();
                    if (dying) {
                        dying->stop();
                    }
                }
            } else if (running_.load(std::memory_order_acquire)) {
                // Reconnect — default may flip to this device; rebuild off the COM thread.
                schedule_routing = true;
                restart_capture = true; // safe: worker compares capture_default_id_
            }
        }

        if (type == DeviceEventType::Added && running_.load(std::memory_order_acquire)) {
            schedule_routing = true;
            restart_capture = true;
        }

        if (type == DeviceEventType::DefaultChanged &&
            running_.load(std::memory_order_acquire)) {
            schedule_routing = true;
            restart_capture = true;
        }

        cb = external_event_callback_;
        utf8_id = wide_to_utf8(device_id);
    }

    // Never call into managed code while holding mutex_.
    if (cb) {
        cb(utf8_id.c_str(), event_code);
    }

    if (schedule_routing) {
        enqueue_routing_refresh(restart_capture);
    }
}

} // namespace auraminus
