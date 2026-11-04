/**
 * Minimal automated tests for RingBuffer SPSC contracts.
 * Build: cmake --build . --target AuraminusRingBufferTests
 * Run:   bin/AuraminusRingBufferTests.exe
 */

#include "memory/ring_buffer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>

using auraminus::RingBuffer;

static int g_failures = 0;

#define EXPECT_TRUE(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        ++g_failures; \
    } \
} while (0)

#define EXPECT_EQ(a, b) do { \
    auto _a = (a); auto _b = (b); \
    if (_a != _b) { \
        std::fprintf(stderr, "FAIL %s:%d: %s == %s (%llu != %llu)\n", \
            __FILE__, __LINE__, #a, #b, \
            (unsigned long long)_a, (unsigned long long)_b); \
        ++g_failures; \
    } \
} while (0)

static void test_basic_write_read() {
    RingBuffer rb(4800, 2, 0);
    float in[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    EXPECT_EQ(rb.write(in, 4), 4u);

    float out[8] = {};
    EXPECT_EQ(rb.read(out, 4), 4u);
    EXPECT_TRUE(std::memcmp(in, out, sizeof(in)) == 0);
}

static void test_producer_never_moves_read_on_overrun() {
    // Tiny buffer: 8 frames, 2 ch, no preroll
    RingBuffer rb(8, 2, 0);
    float block[16];
    for (int i = 0; i < 16; ++i) block[i] = static_cast<float>(i);

    // Fill almost full
    EXPECT_EQ(rb.write(block, 6), 6u);
    uint32_t before = rb.available_read();

    // This write must truncate (not advance read_pos_)
    uint32_t written = rb.write(block, 8);
    EXPECT_TRUE(written < 8);
    EXPECT_TRUE(rb.overruns() >= 1);
    EXPECT_TRUE(rb.available_read() >= before); // consumer cursor untouched by producer
}

static void test_preroll_blocks_read() {
    RingBuffer rb(4800, 2, 100);
    float in[40] = {};
    rb.write(in, 20); // 20 frames < 100 preroll
    float out[40] = {};
    EXPECT_EQ(rb.read(out, 20), 0u);

    rb.write(in, 100);
    EXPECT_TRUE(rb.read(out, 20) > 0);
}

static void test_spsc_stress() {
    RingBuffer rb(48000, 2, 0);
    std::atomic<bool> done{false};
    std::atomic<uint64_t> frames_written{0};
    std::atomic<uint64_t> frames_read{0};

    std::thread producer([&] {
        float buf[256] = {};
        while (!done.load(std::memory_order_relaxed)) {
            frames_written.fetch_add(rb.write(buf, 128), std::memory_order_relaxed);
        }
    });

    std::thread consumer([&] {
        float buf[256] = {};
        for (int i = 0; i < 2000; ++i) {
            frames_read.fetch_add(rb.read(buf, 128), std::memory_order_relaxed);
            std::this_thread::yield();
        }
        done.store(true, std::memory_order_relaxed);
    });

    consumer.join();
    producer.join();

    EXPECT_TRUE(frames_written.load() > 0);
    EXPECT_TRUE(frames_read.load() > 0);
}

int main() {
    test_basic_write_read();
    test_producer_never_moves_read_on_overrun();
    test_preroll_blocks_read();
    test_spsc_stress();

    if (g_failures == 0) {
        std::printf("AuraminusRingBufferTests: ALL PASSED\n");
        return 0;
    }
    std::printf("AuraminusRingBufferTests: %d FAILURE(S)\n", g_failures);
    return 1;
}
