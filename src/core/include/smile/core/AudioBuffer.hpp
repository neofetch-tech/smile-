#pragma once

#include <vector>
#include <atomic>
#include <span>
#include <algorithm>
#include <cstddef>

namespace smile::core {

/**
 * @brief Single-producer single-consumer lock-free ring buffer for audio samples.
 * Bridges the emulation core (audio producer) and the frontend (audio consumer).
 */
class AudioRingBuffer {
public:
    explicit AudioRingBuffer(std::size_t capacity = 48000 * 2) // Default: ~1 second of 48 kHz stereo
        : m_buffer(capacity), m_capacity(capacity), m_readPos(0), m_writePos(0) {}

    // Called from the emulator (producer)
    bool write(float sample) noexcept {
        const auto write = m_writePos.load(std::memory_order_relaxed);
        const auto read = m_readPos.load(std::memory_order_acquire);

        const auto nextWrite = (write + 1) % m_capacity;
        if (nextWrite == read) {
            return false; // Buffer full (overflow)
        }

        m_buffer[write] = sample;
        m_writePos.store(nextWrite, std::memory_order_release);
        return true;
    }

    // Called from the frontend audio driver (consumer)
    std::size_t read(std::span<float> outBuffer) noexcept {
        const auto write = m_writePos.load(std::memory_order_acquire);
        auto read = m_readPos.load(std::memory_order_relaxed);

        std::size_t samplesRead = 0;
        while (read != write && samplesRead < outBuffer.size()) {
            outBuffer[samplesRead++] = m_buffer[read];
            read = (read + 1) % m_capacity;
        }

        m_readPos.store(read, std::memory_order_release);
        return samplesRead;
    }

    [[nodiscard]] std::size_t availableRead() const noexcept {
        const auto write = m_writePos.load(std::memory_order_acquire);
        const auto read = m_readPos.load(std::memory_order_relaxed);
        if (write >= read) {
            return write - read;
        }
        return m_capacity - (read - write);
    }

    void clear() noexcept {
        m_readPos.store(0, std::memory_order_relaxed);
        m_writePos.store(0, std::memory_order_relaxed);
    }

private:
    std::vector<float> m_buffer;
    std::size_t m_capacity;
    alignas(64) std::atomic<std::size_t> m_readPos;
    alignas(64) std::atomic<std::size_t> m_writePos;
};

} // namespace smile::core
