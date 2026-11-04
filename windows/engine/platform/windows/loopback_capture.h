#pragma once
/**
 * Loopback Capture — WASAPI loopback capture from the default render endpoint.
 *
 * Captures system audio output and broadcasts it to output devices.
 * Uses WASAPI in shared loopback mode — no virtual driver needed.
 */

#ifndef AURAMINUS_LOOPBACK_CAPTURE_H
#define AURAMINUS_LOOPBACK_CAPTURE_H

#include <atomic>
#include <thread>
#include <cstdint>
#include <functional>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

namespace auraminus {

using AudioDataCallback = std::function<void(const float* data, uint32_t frame_count)>;

class LoopbackCapture {
public:
    LoopbackCapture();
    ~LoopbackCapture();

    LoopbackCapture(const LoopbackCapture&) = delete;
    LoopbackCapture& operator=(const LoopbackCapture&) = delete;

    /**
     * Initialize loopback capture on the default render endpoint.
     */
    HRESULT initialize(AudioDataCallback callback);

    /**
     * Start the capture thread.
     */
    HRESULT start();

    /**
     * Stop the capture thread and release resources.
     */
    void stop();

    bool is_running() const { return running_.load(std::memory_order_acquire); }
    uint32_t sample_rate() const { return sample_rate_; }
    uint32_t channels() const { return channels_; }
    uint64_t frames_captured() const { return frames_captured_.load(std::memory_order_relaxed); }

private:
    void capture_thread_func();

    AudioDataCallback callback_;

    IMMDevice* device_ = nullptr;
    IAudioClient* audio_client_ = nullptr;
    IAudioCaptureClient* capture_client_ = nullptr;
    HANDLE capture_event_ = nullptr;

    std::thread capture_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> should_stop_{false};

    uint32_t sample_rate_ = 48000;
    uint32_t channels_ = 2;
    uint32_t buffer_frames_ = 0;
    bool is_float_ = true;

    std::vector<float> silence_buffer_;

    std::atomic<uint64_t> frames_captured_{0};
};

} // namespace auraminus

#endif // AURAMINUS_LOOPBACK_CAPTURE_H
