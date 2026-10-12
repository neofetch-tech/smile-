#pragma once

#include <cstdint>

namespace smile::systems::gameboy {

// Minimal bus contract used by the CPU. PPU/Timer/DMA live behind MemoryBus.
class IBus {
public:
    virtual ~IBus() = default;

    [[nodiscard]] virtual uint8_t read8(uint16_t address) = 0;
    virtual void write8(uint16_t address, uint8_t value) = 0;

    [[nodiscard]] uint16_t read16(uint16_t address) {
        const uint16_t lo = read8(address);
        const uint16_t hi = read8(static_cast<uint16_t>(address + 1));
        return static_cast<uint16_t>((hi << 8) | lo);
    }

    virtual void write16(uint16_t address, uint16_t value) {
        write8(address, static_cast<uint8_t>(value & 0xFF));
        write8(static_cast<uint16_t>(address + 1), static_cast<uint8_t>((value >> 8) & 0xFF));
    }
};

} // namespace smile::systems::gameboy
