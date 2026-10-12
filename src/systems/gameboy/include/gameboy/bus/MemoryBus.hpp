#pragma once

#include "gameboy/bus/IBus.hpp"
#include "gameboy/ppu/PPU.hpp"
#include "gameboy/timer/Timer.hpp"
#include "gameboy/cartridge/Cartridge.hpp"
#include "gameboy/apu/APU.hpp"
#include "gameboy/StateCodec.hpp"
#include "smile/core/AudioBuffer.hpp"
#include <vector>
#include <span>

namespace smile::systems::gameboy {

// Interrupt flag bits (IF 0xFF0F / IE 0xFFFF).
enum class Interrupt : uint8_t {
    VBlank = 1 << 0,
    Stat   = 1 << 1,
    Timer  = 1 << 2,
    Serial = 1 << 3,
    Joypad = 1 << 4,
};

class MemoryBus final : public IBus {
public:
    explicit MemoryBus(smile::core::AudioRingBuffer& audio);
    ~MemoryBus() override = default;

    // Returns false when the cartridge controller is unsupported.
    bool loadRom(std::span<const uint8_t> romData);
    void reset();

    [[nodiscard]] uint8_t read8(uint16_t address) override;
    void write8(uint16_t address, uint8_t value) override;

    // Advance PPU + Timer by tCycles and fold their IRQs into IF.
    void stepComponents(uint32_t tCycles);

    [[nodiscard]] PPU& getPPU() noexcept { return m_ppu; }
    [[nodiscard]] const PPU& getPPU() const noexcept { return m_ppu; }
    [[nodiscard]] Cartridge& cartridge() noexcept { return m_cartridge; }
    [[nodiscard]] const Cartridge& cartridge() const noexcept { return m_cartridge; }

    [[nodiscard]] uint8_t getIF() const noexcept { return m_if; }
    void setIF(uint8_t val) noexcept { m_if = val; }
    void requestInterrupt(Interrupt irq) noexcept {
        m_if |= static_cast<uint8_t>(irq);
    }

    [[nodiscard]] uint8_t getIE() const noexcept { return m_ie; }
    void setIE(uint8_t val) noexcept { m_ie = val; }

    // Bitmask: bit0 A, bit1 B, bit2 Select, bit3 Start,
    //          bit4 Right, bit5 Left, bit6 Up, bit7 Down (1 = pressed).
    void setJoypadState(uint8_t buttonMask) noexcept { m_joypadState = buttonMask; }

    void saveState(StateWriter& out) const;
    bool loadState(StateReader& in);

private:
    void doDma(uint8_t page);

    Cartridge m_cartridge;
    std::vector<uint8_t> m_wram;
    std::vector<uint8_t> m_hram;

    PPU m_ppu;
    Timer m_timer;
    APU m_apu;

    uint8_t m_if{0xE0};
    uint8_t m_ie{0x00};
    uint8_t m_joypadSelect{0xFF};
    uint8_t m_joypadState{0x00};
    uint8_t m_serialData{0xFF};
    uint8_t m_serialCtrl{0x7E};
};

} // namespace smile::systems::gameboy
