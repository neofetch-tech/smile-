#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include <array>
#include "gameboy/StateCodec.hpp"

namespace smile::systems::gameboy {

// Dot-accurate DMG PPU (background + window + sprites, 160x144, ARGB8888).
// Timing: 456 dots per scanline, 154 scanlines per frame (144 visible + 10 VBlank).
// Modes: 2 = OAM search (80 dots), 3 = pixel transfer (~172 dots), 0 = HBlank, 1 = VBlank.
class PPU {
public:
    static constexpr uint32_t kWidth = 160;
    static constexpr uint32_t kHeight = 144;
    static constexpr int kDotsPerLine = 456;
    static constexpr int kVBlankStart = 144;
    static constexpr int kLinesPerFrame = 154;

    PPU();
    ~PPU() = default;

    void reset();

    // Advance by tCycles (dots). Raises VBlank / STAT interrupt flags as needed.
    void step(uint32_t tCycles, bool& outVBlankInterrupt, bool& outStatInterrupt);

    [[nodiscard]] uint8_t readVRAM(uint16_t address) const;
    void writeVRAM(uint16_t address, uint8_t value);

    [[nodiscard]] uint8_t readOAM(uint16_t address) const;
    void writeOAM(uint16_t address, uint8_t value);

    [[nodiscard]] uint8_t readRegister(uint16_t address) const;
    void writeRegister(uint16_t address, uint8_t value);

    // OAM DMA source copy: copies 160 bytes from (page << 8) into OAM.
    void dmaCopy(std::span<const uint8_t> source);

    void saveState(StateWriter& out) const;
    bool loadState(StateReader& in);

    [[nodiscard]] std::span<const uint32_t> getFrameBuffer() const noexcept { return m_frameBuffer; }
    [[nodiscard]] uint8_t getLY() const noexcept { return m_ly; }

private:
    void renderScanline();
    void renderBackgroundAndWindowScanline();
    void renderSpritesScanline();

    [[nodiscard]] uint32_t decodeColor(uint8_t colorNum, uint8_t paletteReg) const;
    void updateStatMode();
    void checkLyc();

    std::vector<uint8_t> m_vram; // 8 KB VRAM
    std::vector<uint8_t> m_oam;  // 160 B OAM
    std::vector<uint32_t> m_frameBuffer; // 160x144 ARGB8888

    // I/O registers
    uint8_t m_lcdc{0x91};
    uint8_t m_stat{0x80};
    uint8_t m_scy{0};
    uint8_t m_scx{0};
    uint8_t m_ly{0};
    uint8_t m_lyc{0};
    uint8_t m_bgp{0xFC};
    uint8_t m_obp0{0xFF};
    uint8_t m_obp1{0xFF};
    uint8_t m_wy{0};
    uint8_t m_wx{0};

    int m_dot{0}; // Dot counter within current scanline [0, 456)
    int m_windowLine{0}; // Internal window line counter

    // Classic DMG greenscale palette (ARGB).
    static constexpr std::array<uint32_t, 4> kDmgColors = {
        0xFFE0F8D0, // 0: lightest
        0xFF88C070, // 1: light
        0xFF346856, // 2: dark
        0xFF081820  // 3: darkest
    };
};

} // namespace smile::systems::gameboy
