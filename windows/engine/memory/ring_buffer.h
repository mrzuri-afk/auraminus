#pragma once
/**
 * Ring Buffer — Lock-free Single-Producer Single-Consumer (SPSC) circular buffer.
 *
 * Each OutputDevice has its own dedicated RingBuffer.
 * The capture thread writes into each device's buffer independently.
 * The device's render thread reads from its own buffer.
 *
 * Features:
 *   - Lock-free SPSC with atomic acquire-release semantics
 *   - Completely independent: a slow Bluetooth speaker cannot starve or block a fast onboard speaker
 *   - Built-in jitter/preroll buffering: avoids buffer starvation stutter
 */

#ifndef AURAMINUS_RING_BUFFER_H
#define AURAMINUS_RING_BUFFER_H

#include <atomic>
#include <cstdint>
#include <vector>

namespace auraminus {

class RingBuffer {
public:
    /**
     * @param capacity_frames  Total capacity in audio frames (e.g. 48000 = 1 sec at 48kHz).
     * @param channels         Audio channel count (e.g. 2).
     * @param preroll_frames   Minimum frames required before reading starts (e.g. 2400 = 50ms).
     */
    RingBuffer(uint32_t capacity_frames, uint32_t channels, uint32_t preroll_frames = 2400);
    ~RingBuffer() = default;

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    /**
     * Write audio frames into the buffer (producer/capture thread only).
     */
    uint32_t write(const float* data, uint32_t frame_count);

    /**
     * Read audio frames from the buffer (consumer/render thread only).
     * Returns 0 if pre-roll threshold has not been reached yet.
     */
    uint32_t read(float* data, uint32_t frame_count);

    /**
     * Frames currently available to read.
     */
    uint32_t available_read() const;

    /**
     * Frames currently available to write.
     */
    uint32_t available_write() const;

    /**
     * Has the pre-roll threshold been satisfied?
     */
    bool is_prerolled() const { return prerolled_.load(std::memory_order_relaxed); }

    /**
     * Set target delay in frames (e.g. 2400 frames = 50ms at 48kHz).
     */
    void set_delay_frames(uint32_t delay_frames);
    uint32_t delay_frames() const { return target_delay_frames_.load(std::memory_order_relaxed); }

    void reset();

    uint32_t capacity_frames() const { return capacity_frames_; }
    uint32_t channels() const { return channels_; }
    uint64_t overruns() const { return overruns_.load(std::memory_order_relaxed); }

private:
    uint32_t capacity_frames_;
    uint32_t channels_;
    uint32_t capacity_samples_;
    uint32_t preroll_frames_;

    std::vector<float> buffer_;

    // SPSC atomics: write_pos_ modified only by producer, read_pos_ modified only by consumer
    alignas(64) std::atomic<uint32_t> write_pos_{0}; // In samples
    alignas(64) std::atomic<uint32_t> read_pos_{0};  // In samples
    std::atomic<bool> prerolled_{false};
    std::atomic<uint32_t> target_delay_frames_{0};
    std::atomic<uint32_t> current_delay_frames_{0};
    std::atomic<uint64_t> overruns_{0};
};

} // namespace auraminus

#endif // AURAMINUS_RING_BUFFER_H
