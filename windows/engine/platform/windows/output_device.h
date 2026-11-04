#pragma once
/**
 * Output Device — WASAPI render output for a single device.
 *
 * Each output device has its own dedicated RingBuffer, preventing
 * slower devices (like Bluetooth) from blocking faster devices (like Realtek).
 */

#ifndef AURAMINUS_OUTPUT_DEVICE_H
#define AURAMINUS_OUTPUT_DEVICE_H

#include <atomic>
#include <thread>
#include <string>
#include <cstdint>
#include <memory>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

#include "memory/ring_buffer.h"

namespace auraminus {

class OutputDevice {
public:
    OutputDevice();
    ~OutputDevice();

    OutputDevice(const OutputDevice&) = delete;
    OutputDevice& operator=(const OutputDevice&) = delete;

    /**
     * Initialize WASAPI render output for the given device.
     */
    HRESULT initialize(IMMDevice* device, const std::wstring& device_id, uint32_t capture_channels, uint32_t capture_sample_rate);

    /**
     * Start the render thread.
     */
    HRESULT start();

    /**
     * Stop the render thread and release resources.
     */
    void stop();

    /**
     * Write captured audio frames into this device's dedicated ring buffer.
     */
    void write(const float* data, uint32_t frame_count);

    bool is_running() const { return running_.load(std::memory_order_acquire); }
    const std::wstring& device_id() const { return device_id_; }

    void set_volume(float volume);
    float get_volume() const { return volume_.load(std::memory_order_relaxed); }

    void set_delay_ms(uint32_t delay_ms);
    uint32_t get_delay_ms() const { return delay_ms_.load(std::memory_order_relaxed); }

    uint64_t underruns() const { return underruns_.load(std::memory_order_relaxed); }
    uint64_t overruns() const;

private:
    void render_thread_func();

    std::wstring device_id_;
    std::unique_ptr<RingBuffer> buffer_;

    IAudioClient* audio_client_ = nullptr;
    IAudioRenderClient* render_client_ = nullptr;
    HANDLE render_event_ = nullptr;

    std::thread render_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> should_stop_{false};
    std::atomic<float> volume_{1.0f};
    std::atomic<uint32_t> delay_ms_{0};

    uint32_t sample_rate_ = 48000;
    uint32_t channels_ = 2;
    uint32_t buffer_frames_ = 0;
    bool is_float_ = true;
    WORD bits_per_sample_ = 32;

    std::atomic<uint64_t> underruns_{0};
};

} // namespace auraminus

#endif // AURAMINUS_OUTPUT_DEVICE_H
