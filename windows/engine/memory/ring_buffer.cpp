/**
 * Ring Buffer — SPSC Implementation with Jitter Buffering
 */

#include "ring_buffer.h"
#include <algorithm>
#include <cstring>

namespace auraminus {

RingBuffer::RingBuffer(uint32_t capacity_frames, uint32_t channels, uint32_t preroll_frames)
    : capacity_frames_(capacity_frames)
    , channels_(channels)
    , capacity_samples_(capacity_frames * channels)
    , preroll_frames_(preroll_frames)
{
    buffer_.resize(capacity_samples_, 0.0f);
}

uint32_t RingBuffer::write(const float* data, uint32_t frame_count) {
    if (!data || frame_count == 0) return 0;

    uint32_t samples_to_write = frame_count * channels_;
    uint32_t write = write_pos_.load(std::memory_order_relaxed);
    uint32_t read = read_pos_.load(std::memory_order_acquire);

    // Calculate free space
    uint32_t used;
    if (write >= read) {
        used = write - read;
    } else {
        used = capacity_samples_ - read + write;
    }

    // Leave 1 frame margin so write_pos never equals read_pos when full
    uint32_t margin = channels_;
    uint32_t space = (used + margin < capacity_samples_) ? (capacity_samples_ - used - margin) : 0;

    // SPSC: only the consumer may advance read_pos_. On overrun, drop the newest
    // frames by truncating this write (never touch read_pos_ from the producer).
    if (samples_to_write > space) {
        overruns_.fetch_add(1, std::memory_order_relaxed);
        samples_to_write = space;
    }

    samples_to_write = (samples_to_write / channels_) * channels_;

    if (samples_to_write == 0) return 0;

    // Write circular buffer
    uint32_t pos = write;
    uint32_t first = std::min(samples_to_write, capacity_samples_ - pos);
    std::memcpy(&buffer_[pos], data, first * sizeof(float));

    if (first < samples_to_write) {
        uint32_t second = samples_to_write - first;
        std::memcpy(&buffer_[0], data + first, second * sizeof(float));
    }

    uint32_t new_write = (pos + samples_to_write) % capacity_samples_;
    write_pos_.store(new_write, std::memory_order_release);

    // Enable reading once we reach the preroll + delay threshold
    if (!prerolled_.load(std::memory_order_relaxed)) {
        uint32_t target_cushion = preroll_frames_ + target_delay_frames_.load(std::memory_order_relaxed);
        if (available_read() >= target_cushion) {
            prerolled_.store(true, std::memory_order_release);
        }
    }

    return samples_to_write / channels_;
}

void RingBuffer::set_delay_frames(uint32_t delay_frames) {
    uint32_t max_delay = capacity_frames_ / 2;
    if (delay_frames > max_delay) delay_frames = max_delay;
    target_delay_frames_.store(delay_frames, std::memory_order_release);
}

uint32_t RingBuffer::read(float* data, uint32_t frame_count) {
    if (!data || frame_count == 0) return 0;

    uint32_t target_delay = target_delay_frames_.load(std::memory_order_relaxed);
    uint32_t current_delay = current_delay_frames_.load(std::memory_order_relaxed);

    // Adjust delay smoothly if changed live
    if (target_delay != current_delay && prerolled_.load(std::memory_order_relaxed)) {
        if (target_delay > current_delay) {
            uint32_t needed_cushion = preroll_frames_ + target_delay;
            if (available_read() < needed_cushion) {
                // Buffer more frames to establish the new longer delay
                return 0;
            }
            current_delay_frames_.store(target_delay, std::memory_order_relaxed);
        } else {
            // Shorten delay: advance read cursor
            uint32_t diff = current_delay - target_delay;
            uint32_t skip_samples = diff * channels_;
            uint32_t r = read_pos_.load(std::memory_order_relaxed);
            read_pos_.store((r + skip_samples) % capacity_samples_, std::memory_order_release);
            current_delay_frames_.store(target_delay, std::memory_order_relaxed);
        }
    }

    uint32_t target_cushion = preroll_frames_ + target_delay;

    // If we haven't reached pre-roll cushion yet, return 0 (render silence)
    if (!prerolled_.load(std::memory_order_acquire)) {
        if (available_read() < target_cushion) {
            return 0;
        }
        current_delay_frames_.store(target_delay, std::memory_order_relaxed);
        prerolled_.store(true, std::memory_order_release);
    }

    uint32_t samples_to_read = frame_count * channels_;
    uint32_t read = read_pos_.load(std::memory_order_relaxed);
    uint32_t write = write_pos_.load(std::memory_order_acquire);

    uint32_t available;
    if (write >= read) {
        available = write - read;
    } else {
        available = capacity_samples_ - read + write;
    }

    samples_to_read = std::min(samples_to_read, available);
    samples_to_read = (samples_to_read / channels_) * channels_;

    if (samples_to_read == 0) {
        // Starved: temporarily drop preroll to let cushion re-accumulate
        prerolled_.store(false, std::memory_order_release);
        return 0;
    }

    // Read circular buffer
    uint32_t pos = read;
    uint32_t first = std::min(samples_to_read, capacity_samples_ - pos);
    std::memcpy(data, &buffer_[pos], first * sizeof(float));

    if (first < samples_to_read) {
        uint32_t second = samples_to_read - first;
        std::memcpy(data + first, &buffer_[0], second * sizeof(float));
    }

    uint32_t new_read = (pos + samples_to_read) % capacity_samples_;
    read_pos_.store(new_read, std::memory_order_release);

    return samples_to_read / channels_;
}

uint32_t RingBuffer::available_read() const {
    uint32_t read = read_pos_.load(std::memory_order_acquire);
    uint32_t write = write_pos_.load(std::memory_order_acquire);

    if (write >= read) {
        return (write - read) / channels_;
    } else {
        return (capacity_samples_ - read + write) / channels_;
    }
}

uint32_t RingBuffer::available_write() const {
    uint32_t read = read_pos_.load(std::memory_order_acquire);
    uint32_t write = write_pos_.load(std::memory_order_acquire);

    uint32_t used;
    if (write >= read) {
        used = write - read;
    } else {
        used = capacity_samples_ - read + write;
    }

    uint32_t margin = channels_;
    if (used + margin >= capacity_samples_) return 0;
    return (capacity_samples_ - used - margin) / channels_;
}

void RingBuffer::reset() {
    write_pos_.store(0, std::memory_order_release);
    read_pos_.store(0, std::memory_order_release);
    prerolled_.store(false, std::memory_order_release);
    current_delay_frames_.store(0, std::memory_order_release);
    overruns_.store(0, std::memory_order_release);
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
}

} // namespace auraminus
