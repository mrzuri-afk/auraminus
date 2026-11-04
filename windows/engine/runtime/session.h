#pragma once
/**
 * Engine — Core audio engine orchestrator.
 */

#ifndef AURAMINUS_CORE_ENGINE_H
#define AURAMINUS_CORE_ENGINE_H

#include "platform/windows/device_manager.h"
#include "platform/windows/loopback_capture.h"
#include "platform/windows/output_device.h"
#include "memory/ring_buffer.h"
#include "public/auraminus_engine.h"

#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <deque>
#include <functional>

namespace auraminus {

class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    AuraminusResult initialize();
    AuraminusResult start();
    AuraminusResult stop();

    AuraminusResult get_devices(AuraminusDeviceInfo* devices, int32_t capacity, int32_t* out_count);
    AuraminusResult refresh_devices();
    AuraminusResult add_output(const char* device_id);
    AuraminusResult remove_output(const char* device_id);

    AuraminusResult set_volume(const char* device_id, float volume);
    AuraminusResult set_delay(const char* device_id, uint32_t delay_ms);
    AuraminusResult get_delay(const char* device_id, uint32_t* out_delay_ms);
    AuraminusResult get_status(AuraminusEngineStatus* status);

    void set_device_event_callback(AuraminusDeviceEventCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        external_event_callback_ = callback;
    }

    bool is_running() const { return running_.load(std::memory_order_acquire); }

private:
    using OutputDeviceList = std::vector<std::shared_ptr<OutputDevice>>;

    struct OutputEntry {
        std::wstring device_id;
        std::shared_ptr<OutputDevice> device;
    };

    struct SelectedDevice {
        std::wstring id;
        float volume = 1.0f;
        uint32_t delay_ms = 0;
    };

    void on_device_event(DeviceEventType type, const std::wstring& device_id);
    void publish_outputs_locked();
    void fanout_audio(const float* data, uint32_t frames);
    AuraminusResult activate_output_locked(const SelectedDevice& sel, uint32_t channels, uint32_t rate);
    void resync_outputs_for_routing_locked();
    void restart_capture_and_resync_locked();
    void enqueue_routing_refresh(bool restart_capture);
    void worker_loop();
    OutputDevice* find_output(const std::wstring& device_id);

    static std::wstring utf8_to_wide(const char* str);
    static std::string wide_to_utf8(const std::wstring& wstr);

    DeviceManager device_manager_;
    std::unique_ptr<LoopbackCapture> capture_;
    std::wstring capture_default_id_;

    std::vector<OutputEntry> outputs_;
    std::atomic<std::shared_ptr<OutputDeviceList>> published_outputs_;

    std::vector<SelectedDevice> selected_devices_;

    std::mutex mutex_;
    std::atomic<bool> running_{false};
    bool initialized_ = false;

    // Deferred work — never do WASAPI Activate/Start on the IMMNotificationClient thread.
    std::mutex work_mutex_;
    std::condition_variable work_cv_;
    std::deque<std::function<void()>> work_queue_;
    std::thread worker_;
    std::atomic<bool> worker_stop_{false};
    std::atomic<bool> routing_refresh_pending_{false};
    std::atomic<bool> capture_restart_pending_{false};

    AuraminusDeviceEventCallback external_event_callback_ = nullptr;
};

} // namespace auraminus

#endif // AURAMINUS_CORE_ENGINE_H
