/**
 * Loopback Capture — Implementation
 */

#include "loopback_capture.h"
#include <avrt.h>
#include <vector>
#include <cstring>

namespace auraminus {

LoopbackCapture::LoopbackCapture() = default;

LoopbackCapture::~LoopbackCapture() {
    stop();
}

HRESULT LoopbackCapture::initialize(AudioDataCallback callback) {
    callback_ = std::move(callback);

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return hr;
    }

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&enumerator)
    );
    if (FAILED(hr)) return hr;

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device_);
    enumerator->Release();
    if (FAILED(hr) || !device_) return hr;

    hr = device_->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        reinterpret_cast<void**>(&audio_client_)
    );
    if (FAILED(hr) || !audio_client_) return hr;

    WAVEFORMATEX* mix_format = nullptr;
    hr = audio_client_->GetMixFormat(&mix_format);
    if (FAILED(hr) || !mix_format) return hr;

    sample_rate_ = mix_format->nSamplesPerSec;
    channels_ = mix_format->nChannels;

    is_float_ = false;
    if (mix_format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        is_float_ = true;
    } else if (mix_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto* ex = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mix_format);
        if (ex->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
            is_float_ = true;
        }
    }

    if (!is_float_) {
        CoTaskMemFree(mix_format);
        return AUDCLNT_E_UNSUPPORTED_FORMAT;
    }

    capture_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!capture_event_) {
        CoTaskMemFree(mix_format);
        return E_FAIL;
    }

    REFERENCE_TIME buffer_duration = 200000; // 20ms
    hr = audio_client_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        buffer_duration,
        0,
        mix_format,
        nullptr
    );
    CoTaskMemFree(mix_format);
    if (FAILED(hr)) return hr;

    hr = audio_client_->SetEventHandle(capture_event_);
    if (FAILED(hr)) return hr;

    hr = audio_client_->GetBufferSize(&buffer_frames_);
    if (FAILED(hr)) return hr;

    hr = audio_client_->GetService(
        __uuidof(IAudioCaptureClient),
        reinterpret_cast<void**>(&capture_client_)
    );
    if (FAILED(hr)) return hr;

    // Preallocate silence so the RT path never resizes.
    silence_buffer_.assign(static_cast<size_t>(buffer_frames_) * channels_ * 2, 0.0f);

    return S_OK;
}

HRESULT LoopbackCapture::start() {
    if (running_.load(std::memory_order_acquire)) return S_OK;
    if (!audio_client_ || !capture_client_ || !capture_event_) return E_FAIL;

    should_stop_.store(false, std::memory_order_release);

    HRESULT hr = audio_client_->Start();
    if (FAILED(hr)) return hr;

    running_.store(true, std::memory_order_release);
    capture_thread_ = std::thread(&LoopbackCapture::capture_thread_func, this);

    return S_OK;
}

void LoopbackCapture::stop() {
    should_stop_.store(true, std::memory_order_release);

    if (capture_event_) {
        SetEvent(capture_event_);
    }

    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }

    running_.store(false, std::memory_order_release);

    if (audio_client_) {
        audio_client_->Stop();
    }

    if (capture_client_) {
        capture_client_->Release();
        capture_client_ = nullptr;
    }

    if (audio_client_) {
        audio_client_->Release();
        audio_client_ = nullptr;
    }

    if (device_) {
        device_->Release();
        device_ = nullptr;
    }

    if (capture_event_) {
        CloseHandle(capture_event_);
        capture_event_ = nullptr;
    }
}

void LoopbackCapture::capture_thread_func() {
    DWORD task_index = 0;
    HANDLE avrt_handle = AvSetMmThreadCharacteristicsW(L"Audio", &task_index);

    while (!should_stop_.load(std::memory_order_acquire)) {
        DWORD wait_result = WaitForSingleObject(capture_event_, 100);

        if (should_stop_.load(std::memory_order_acquire)) break;
        if (wait_result != WAIT_OBJECT_0) continue;

        UINT32 packet_length = 0;
        HRESULT hr = capture_client_->GetNextPacketSize(&packet_length);

        while (SUCCEEDED(hr) && packet_length > 0) {
            BYTE* data = nullptr;
            UINT32 frames_available = 0;
            DWORD flags = 0;

            hr = capture_client_->GetBuffer(&data, &frames_available, &flags, nullptr, nullptr);
            if (FAILED(hr)) break;

            if (frames_available > 0 && callback_) {
                const float* float_data = nullptr;
                UINT32 frames_to_deliver = frames_available;

                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    size_t samples = static_cast<size_t>(frames_available) * channels_;
                    if (samples > silence_buffer_.size()) {
                        frames_to_deliver = static_cast<UINT32>(silence_buffer_.size() / channels_);
                        samples = static_cast<size_t>(frames_to_deliver) * channels_;
                    }
                    std::fill_n(silence_buffer_.begin(), samples, 0.0f);
                    float_data = silence_buffer_.data();
                } else {
                    float_data = reinterpret_cast<const float*>(data);
                }

                if (frames_to_deliver > 0) {
                    callback_(float_data, frames_to_deliver);
                    frames_captured_.fetch_add(frames_to_deliver, std::memory_order_relaxed);
                }
            }

            hr = capture_client_->ReleaseBuffer(frames_available);
            if (FAILED(hr)) break;

            hr = capture_client_->GetNextPacketSize(&packet_length);
        }
    }

    if (avrt_handle) {
        AvRevertMmThreadCharacteristics(avrt_handle);
    }
}

} // namespace auraminus
