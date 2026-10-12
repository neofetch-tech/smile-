#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace smile::systems::gameboy {

// Minimal explicit-layout (little-endian) serializer for save states.
// Each component appends its fields in a fixed order; no versioning inside.
class StateWriter {
public:
    void putU8(uint8_t value) { m_data.push_back(value); }

    void putU16(uint16_t value) {
        putU8(static_cast<uint8_t>(value & 0xFF));
        putU8(static_cast<uint8_t>(value >> 8));
    }

    void putU32(uint32_t value) {
        putU16(static_cast<uint16_t>(value & 0xFFFF));
        putU16(static_cast<uint16_t>(value >> 16));
    }

    void putBytes(std::span<const uint8_t> bytes) {
        m_data.insert(m_data.end(), bytes.begin(), bytes.end());
    }

    [[nodiscard]] std::vector<uint8_t> finish() && { return std::move(m_data); }

private:
    std::vector<uint8_t> m_data;
};

class StateReader {
public:
    explicit StateReader(std::span<const uint8_t> data) : m_data(data) {}

    bool readU8(uint8_t& out) {
        if (m_pos >= m_data.size()) {
            m_ok = false;
            return false;
        }
        out = m_data[m_pos++];
        return true;
    }

    bool readU16(uint16_t& out) {
        uint8_t lo = 0;
        uint8_t hi = 0;
        if (!readU8(lo) || !readU8(hi)) {
            return false;
        }
        out = static_cast<uint16_t>(static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8));
        return true;
    }

    bool readU32(uint32_t& out) {
        uint16_t lo = 0;
        uint16_t hi = 0;
        if (!readU16(lo) || !readU16(hi)) {
            return false;
        }
        out = static_cast<uint32_t>(lo) | (static_cast<uint32_t>(hi) << 16);
        return true;
    }

    bool readBytes(std::span<uint8_t> out) {
        if (m_pos + out.size() > m_data.size()) {
            m_ok = false;
            return false;
        }
        std::copy_n(m_data.begin() + static_cast<std::ptrdiff_t>(m_pos), out.size(), out.begin());
        m_pos += out.size();
        return true;
    }

    [[nodiscard]] bool ok() const noexcept { return m_ok; }
    [[nodiscard]] bool exhausted() const noexcept { return m_ok && m_pos == m_data.size(); }

private:
    std::span<const uint8_t> m_data;
    size_t m_pos{0};
    bool m_ok{true};
};

} // namespace smile::systems::gameboy
