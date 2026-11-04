/**
 * Output Device — WASAPI render implementation with Dedicated Buffer & Jitter Control
 */

#include "output_device.h"
#include <avrt.h>
#include <algorithm>
#include <cstring>

#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif
#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000
#endif

namespace auraminus {

OutputDevice::OutputDevice() = default;

OutputDevice::~OutputDevice() {
    stop();
}

HRESULT OutputDevice::initialize(
    IMMDevice* device,
    const std::wstring& device_id,
    uint32_t capture_channels,
    uint32_t capture_sample_rate
) {
    if (!device) return E_POINTER;
    if (capture_channels == 0 || capture_sample_rate == 0) return E_INVALIDARG;

    device_id_ = device_id;

    auto activate_client = [&]() -> HRESULT {
        if (audio_client_) {
            audio_client_->Release();
            audio_client_ = nullptr;
        }
        return device->Activate(
            __uuidof(IAudioClient),
            CLSCTX_ALL,
            nullptr,
            reinterpret_cast<void**>(&audio_client_)
        );
    };

    HRESULT hr = activate_client();
    if (FAILED(hr) || !audio_client_) return hr;

    // Normalize to the capture pipeline format (float32). WASAPI converts to the
    // device mix format via AUTOCONVERTPCM so every output shares one layout.
    sample_rate_ = capture_sample_rate;
    channels_ = capture_channels;
    bits_per_sample_ = 32;
    is_float_ = true;

    WAVEFORMATEX desired{};
    desired.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    desired.nChannels = static_cast<WORD>(channels_);
    desired.nSamplesPerSec = sample_rate_;
    desired.wBitsPerSample = 32;
    desired.nBlockAlign = static_cast<WORD>(channels_ * sizeof(float));
    desired.nAvgBytesPerSec = sample_rate_ * desired.nBlockAlign;
    desired.cbSize = 0;

    uint32_t preroll = (sample_rate_ * 50) / 1000;
    if (preroll < 480) preroll = 480;
    buffer_ = std::make_unique<RingBuffer>(sample_rate_, channels_, preroll);

    render_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!render_event_) {
        return E_FAIL;
    }

    REFERENCE_TIME buffer_duration = 200000; // 20ms in 100ns units
    hr = audio_client_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
            AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
            AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
        buffer_duration,
        0,
        &desired,
        nullptr
    );

    // Fallback: recreate client and use device mix format when rates already match
    if (FAILED(hr)) {
        WAVEFORMATEX* mix_format = nullptr;

        hr = activate_client();
        if (FAILED(hr) || !audio_client_) return hr;

        hr = audio_client_->GetMixFormat(&mix_format);
        if (FAILED(hr) || !mix_format) return hr;

        sample_rate_ = mix_format->nSamplesPerSec;
        channels_ = mix_format->nChannels;
        bits_per_sample_ = mix_format->wBitsPerSample;

        is_float_ = false;
        if (mix_format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
            is_float_ = true;
        } else if (mix_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            auto* ex = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mix_format);
            if (ex->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
                is_float_ = true;
            }
        }

        if (channels_ != capture_channels || sample_rate_ != capture_sample_rate) {
            CoTaskMemFree(mix_format);
            return AUDCLNT_E_UNSUPPORTED_FORMAT;
        }

        preroll = (sample_rate_ * 50) / 1000;
        if (preroll < 480) preroll = 480;
        buffer_ = std::make_unique<RingBuffer>(sample_rate_, channels_, preroll);

        hr = audio_client_->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            buffer_duration,
            0,
            mix_format,
            nullptr
        );
        CoTaskMemFree(mix_format);
        if (FAILED(hr)) return hr;
    }

    hr = audio_client_->SetEventHandle(render_event_);
    if (FAILED(hr)) return hr;

    hr = audio_client_->GetBufferSize(&buffer_frames_);
    if (FAILED(hr)) return hr;

    hr = audio_client_->GetService(
        __uuidof(IAudioRenderClient),
        reinterpret_cast<void**>(&render_client_)
    );
    if (FAILED(hr)) return hr;

    return S_OK;
}

HRESULT OutputDevice::start() {
    if (running_.load(std::memory_order_acquire)) return S_OK;
    if (!audio_client_ || !render_client_ || !render_event_) return E_FAIL;

    if (buffer_) {
        buffer_->reset();
        uint32_t delay_ms = delay_ms_.load(std::memory_order_relaxed);
        if (delay_ms > 0) {
            uint32_t delay_frames = (sample_rate_ * delay_ms) / 1000;
            buffer_->set_delay_frames(delay_frames);
        }
    }

    should_stop_.store(false, std::memory_order_release);

    // Pre-fill WASAPI buffer with silence so the hardware starts playing cleanly
    BYTE* initial_buffer = nullptr;
    HRESULT hr = render_client_->GetBuffer(buffer_frames_, &initial_buffer);
    if (SUCCEEDED(hr)) {
        render_client_->ReleaseBuffer(buffer_frames_, AUDCLNT_BUFFERFLAGS_SILENT);
    }

    hr = audio_client_->Start();
    if (FAILED(hr)) return hr;

    running_.store(true, std::memory_order_release);
    render_thread_ = std::thread(&OutputDevice::render_thread_func, this);

    return S_OK;
}

void OutputDevice::stop() {
    should_stop_.store(true, std::memory_order_release);

    if (render_event_) {
        SetEvent(render_event_);
    }

    if (render_thread_.joinable()) {
        render_thread_.join();
    }

    running_.store(false, std::memory_order_release);

    if (audio_client_) {
        audio_client_->Stop();
    }

    if (render_client_) {
        render_client_->Release();
        render_client_ = nullptr;
    }

    if (audio_client_) {
        audio_client_->Release();
        audio_client_ = nullptr;
    }

    if (render_event_) {
        CloseHandle(render_event_);
        render_event_ = nullptr;
    }

    if (buffer_) {
        buffer_->reset();
    }
}

void OutputDevice::set_volume(float volume) {
    volume_.store(std::clamp(volume, 0.0f, 1.0f), std::memory_order_release);
}

void OutputDevice::set_delay_ms(uint32_t delay_ms) {
    delay_ms = std::clamp(delay_ms, 0u, 500u);
    delay_ms_.store(delay_ms, std::memory_order_release);
    if (buffer_) {
        uint32_t delay_frames = (sample_rate_ * delay_ms) / 1000;
        buffer_->set_delay_frames(delay_frames);
    }
}

void OutputDevice::write(const float* data, uint32_t frame_count) {
    if (buffer_ && running_.load(std::memory_order_relaxed)) {
        buffer_->write(data, frame_count);
    }
}

uint64_t OutputDevice::overruns() const {
    return buffer_ ? buffer_->overruns() : 0;
}

void OutputDevice::render_thread_func() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    DWORD task_index = 0;
    HANDLE avrt_handle = AvSetMmThreadCharacteristicsW(L"Audio", &task_index);

    std::vector<float> read_buffer(buffer_frames_ * channels_ + 1024, 0.0f);

    while (!should_stop_.load(std::memory_order_acquire)) {
        DWORD wait_result = WaitForSingleObject(render_event_, 100);

        if (should_stop_.load(std::memory_order_acquire)) break;
        if (wait_result != WAIT_OBJECT_0) continue;

        UINT32 padding = 0;
        HRESULT hr = audio_client_->GetCurrentPadding(&padding);
        if (FAILED(hr)) continue;

        UINT32 frames_needed = buffer_frames_ - padding;
        if (frames_needed == 0) continue;

        uint32_t frames_read = buffer_->read(read_buffer.data(), frames_needed);

        BYTE* render_data = nullptr;
        hr = render_client_->GetBuffer(frames_needed, &render_data);
        if (FAILED(hr)) continue;

        if (frames_read == 0) {
            render_client_->ReleaseBuffer(frames_needed, AUDCLNT_BUFFERFLAGS_SILENT);
            underruns_.fetch_add(1, std::memory_order_relaxed);
            continue;
        }

        float vol = volume_.load(std::memory_order_relaxed);

        if (is_float_) {
            float* out = reinterpret_cast<float*>(render_data);
            uint32_t copy_samples = frames_read * channels_;

            if (vol >= 0.999f) {
                std::memcpy(out, read_buffer.data(), copy_samples * sizeof(float));
            } else if (vol <= 0.001f) {
                std::memset(out, 0, copy_samples * sizeof(float));
            } else {
                for (uint32_t i = 0; i < copy_samples; ++i) {
                    out[i] = read_buffer[i] * vol;
                }
            }

            if (frames_read < frames_needed) {
                uint32_t remain_samples = (frames_needed - frames_read) * channels_;
                std::memset(out + copy_samples, 0, remain_samples * sizeof(float));
                underruns_.fetch_add(1, std::memory_order_relaxed);
            }
        } else if (bits_per_sample_ == 16) {
            int16_t* out = reinterpret_cast<int16_t*>(render_data);
            uint32_t copy_samples = frames_read * channels_;

            for (uint32_t i = 0; i < copy_samples; ++i) {
                float sample = read_buffer[i] * vol;
                sample = std::clamp(sample, -1.0f, 1.0f);
                out[i] = static_cast<int16_t>(sample * 32767.0f);
            }

            if (frames_read < frames_needed) {
                uint32_t remain_samples = (frames_needed - frames_read) * channels_;
                std::memset(out + copy_samples, 0, remain_samples * sizeof(int16_t));
                underruns_.fetch_add(1, std::memory_order_relaxed);
            }
        } else {
            std::memset(render_data, 0, frames_needed * channels_ * (bits_per_sample_ / 8));
        }

        render_client_->ReleaseBuffer(frames_needed, 0);
    }

    if (avrt_handle) {
        AvRevertMmThreadCharacteristics(avrt_handle);
    }

    CoUninitialize();
}

} // namespace auraminus
