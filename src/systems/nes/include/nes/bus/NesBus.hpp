#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>
#include "nes/ppu/NesPPU.hpp"
#include "nes/cartridge/NesCartridge.hpp"

namespace smile::systems::nes {

// NTSC NES bus: 2 KB RAM (mirrored), PPU registers, joypads, cartridge
// (NROM/UNROM) at $4020-FFFF, OAM DMA with CPU stall accounting.
class NesBus {
public:
    NesBus();

    void reset();
    NesCartridge::LoadResult loadCartridge(std::span<const uint8_t> romData);

    [[nodiscard]] uint8_t read8(uint16_t address);
    void write8(uint16_t address, uint8_t value);

    // OAM DMA ($4014) stalls the CPU for 513/514 cycles; the CPU collects
    // the debt at the start of its next step.
    [[nodiscard]] uint16_t takeDmaStall() noexcept {
        const uint16_t stall = m_dmaStall;
        m_dmaStall = 0;
        return stall;
    }

    [[nodiscard]] NesPPU& ppu() noexcept { return m_ppu; }
    [[nodiscard]] const NesPPU& ppu() const noexcept { return m_ppu; }

    // NES button order: bit0 A, bit1 B, bit2 Select, bit3 Start,
    //                   bit4 Up, bit5 Down, bit6 Left, bit7 Right.
    void setButtons(uint8_t pad1, uint8_t pad2 = 0);

private:
    void doOamDma(uint8_t page);

    std::array<uint8_t, 2048> m_ram{};
    NesCartridge m_cart;
    NesPPU m_ppu;

    uint8_t m_buttons[2]{0, 0};
    uint8_t m_shift[2]{0, 0};
    uint8_t m_shiftCount[2]{0, 0};
    bool m_strobe{false};

    uint16_t m_dmaStall{0};
};

} // namespace smile::systems::nes
