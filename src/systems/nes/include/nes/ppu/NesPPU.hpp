#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace smile::systems::nes {

class NesCartridge;

// 2C02 picture processing unit: registers, loopy scroll, background +
// sprites scanline renderer, VBlank NMI edge. Dots: 341 per scanline,
// scanlines 0-239 visible, 240 post, 241-260 VBlank, 261 pre-render.
class NesPPU {
public:
    static constexpr uint32_t kWidth = 256;
    static constexpr uint32_t kHeight = 240;
    static constexpr int kDotsPerLine = 341;
    static constexpr int kVBlankLine = 241;
    static constexpr int kPreRenderLine = 261;
    static constexpr int kLinesPerFrame = 262;

    NesPPU();

    void reset();
    void attachCartridge(NesCartridge* cart) noexcept { m_cart = cart; }

    // Advance by PPU dots (3 per CPU cycle). nmiOut pulses on the NMI edge.
    void stepDots(uint32_t dots, bool& nmiOut);

    uint8_t readRegister(uint16_t address);
    void writeRegister(uint16_t address, uint8_t value);

    void writeOamData(uint8_t value); // $2004 path (uses OAMADDR, post-increments)
    [[nodiscard]] uint8_t readOamData() const;
    void oamDma(std::span<const uint8_t> page); // $4014 path: 256 bytes

    [[nodiscard]] std::span<const uint32_t> frame() const noexcept { return m_frame; }
    [[nodiscard]] int dot() const noexcept { return m_dot; }

private:
    // $2000-3FFF: nametables (mirrored VRAM) and palette RAM.
    [[nodiscard]] uint8_t vramRead(uint16_t address) const;
    void vramWrite(uint16_t address, uint8_t value);
    [[nodiscard]] uint8_t paletteRead(uint16_t address) const;
    void paletteWrite(uint16_t address, uint8_t value);
    // Pattern tables live in the cartridge (CHR-ROM or CHR-RAM).
    [[nodiscard]] uint8_t chrRead(uint16_t address) const;

    void renderScanline();
    void renderBackground();
    void renderSprites();
    [[nodiscard]] uint32_t nesColor(uint8_t paletteIndex) const;

    void incrementX();
    void incrementY();
    void copyX();
    void copyY();

    // Registers
    uint8_t m_ctrl{0};
    uint8_t m_mask{0};
    uint8_t m_status{0};
    uint8_t m_oamAddr{0};
    uint8_t m_openBus{0};

    // Loopy scroll state
    uint16_t m_v{0}; // Current VRAM address (15 bits)
    uint16_t m_t{0}; // Temporary VRAM address (15 bits)
    uint8_t m_x{0};  // Fine X scroll (3 bits)
    bool m_w{false}; // Write latch (false = first write)

    int m_dot{0};
    int m_scanline{0};
    bool m_prevNmiLine{false};

    std::array<uint8_t, 2048> m_vram{};   // Nametables (mirrored)
    std::array<uint8_t, 32> m_palette{};  // Palette RAM
    std::array<uint8_t, 256> m_oam{};     // Sprite memory
    std::vector<uint32_t> m_frame;        // 256x240 ARGB8888

    NesCartridge* m_cart{nullptr};
};

} // namespace smile::systems::nes
