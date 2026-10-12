#include "nes/ppu/NesPPU.hpp"
#include "nes/cartridge/NesCartridge.hpp"
#include <algorithm>

namespace smile::systems::nes {

namespace {
// PPUSTATUS flag bits.
constexpr uint8_t kVBlank = 0x80;
constexpr uint8_t kSprite0Hit = 0x40;
constexpr uint8_t kSpriteOverflow = 0x20;
} // namespace

NesPPU::NesPPU() {
    m_frame.resize(kWidth * kHeight, 0xFF000000);
    reset();
}

void NesPPU::reset() {
    m_ctrl = 0;
    m_mask = 0;
    m_status = 0;
    m_oamAddr = 0;
    m_openBus = 0;
    m_v = 0;
    m_t = 0;
    m_x = 0;
    m_w = false;
    m_dot = 0;
    m_scanline = 0;
    m_prevNmiLine = false;
    m_vram.fill(0);
    m_palette.fill(0);
    m_oam.fill(0xFF); // $FF hides every sprite (Y = 255 + 1 wrap)
    std::fill(m_frame.begin(), m_frame.end(), 0xFF000000);
}

uint8_t NesPPU::chrRead(uint16_t address) const {
    if (m_cart) {
        return m_cart->chrRead(address & 0x1FFF);
    }
    return 0xFF;
}

uint8_t NesPPU::vramRead(uint16_t address) const {
    address &= 0x3FFF;
    if (address < 0x2000) { // Pattern tables: cartridge CHR
        return chrRead(address);
    }
    if (address < 0x3F00) { // Nametables: 2 KB VRAM with mirroring
        const uint16_t offset = static_cast<uint16_t>(address & 0x0FFF);
        const NesMirror mirror =
            m_cart ? m_cart->mirror() : NesMirror::Horizontal;
        uint16_t index = 0;
        if (mirror == NesMirror::Horizontal) {
            // $2000=$2400 page 0, $2800=$2C00 page 1 (selected by bit 11).
            index = static_cast<uint16_t>(((offset & 0x0800) >> 1) | (offset & 0x03FF));
        } else {
            // $2000=$2800 page 0, $2400=$2C00 page 1 (selected by bit 10).
            index = static_cast<uint16_t>((offset & 0x0400) | (offset & 0x03FF));
        }
        return m_vram[index & 0x07FF];
    }
    return paletteRead(address); // $3F00-3FFF: palette RAM
}

void NesPPU::vramWrite(uint16_t address, uint8_t value) {
    address &= 0x3FFF;
    if (address < 0x2000) { // CHR-RAM writes go to the cartridge
        if (m_cart) {
            m_cart->chrWrite(address, value);
        }
        return;
    }
    if (address < 0x3F00) {
        const uint16_t offset = static_cast<uint16_t>(address & 0x0FFF);
        const NesMirror mirror =
            m_cart ? m_cart->mirror() : NesMirror::Horizontal;
        uint16_t index = 0;
        if (mirror == NesMirror::Horizontal) {
            index = static_cast<uint16_t>(((offset & 0x0800) >> 1) | (offset & 0x03FF));
        } else {
            index = static_cast<uint16_t>((offset & 0x0400) | (offset & 0x03FF));
        }
        m_vram[index & 0x07FF] = value;
        return;
    }
    paletteWrite(address, value);
}

uint8_t NesPPU::paletteRead(uint16_t address) const {
    uint8_t index = static_cast<uint8_t>(address & 0x1F);
    // $3F10/$3F14/$3F18/$3F1C mirror the universal background color.
    if ((index & 0x03) == 0) {
        index &= 0x0F;
    }
    return m_palette[index];
}

void NesPPU::paletteWrite(uint16_t address, uint8_t value) {
    uint8_t index = static_cast<uint8_t>(address & 0x1F);
    if ((index & 0x03) == 0) {
        index &= 0x0F;
    }
    m_palette[index] = value;
}

uint8_t NesPPU::readRegister(uint16_t address) {
    switch (address & 0x07) {
        case 2: { // PPUSTATUS
            const uint8_t result =
                static_cast<uint8_t>((m_status & 0xE0) | (m_openBus & 0x1F));
            m_status &= ~kVBlank; // Reading clears VBlank...
            m_w = false;          // ...and the write latch.
            m_openBus = result;
            return result;
        }
        case 4: { // OAMDATA
            const uint8_t result = m_oam[m_oamAddr];
            m_openBus = result;
            return result;
        }
        case 7: { // PPUDATA (buffered, except palette)
            uint8_t result = 0;
            if ((m_v & 0x3FFF) < 0x3F00) {
                result = m_openBus; // Stale buffer contents...
                m_openBus = vramRead(m_v); // ...then refill from memory.
            } else {
                result = vramRead(m_v);
                m_openBus = vramRead(static_cast<uint16_t>(m_v & 0x2FFF));
            }
            m_v = static_cast<uint16_t>(m_v + ((m_ctrl & 0x04) ? 32 : 1));
            return result;
        }
        default:
            return m_openBus; // Write-only registers return open bus
    }
}

void NesPPU::writeRegister(uint16_t address, uint8_t value) {
    m_openBus = value;
    switch (address & 0x07) {
        case 0: // PPUCTRL (+ NMI edge handled by the per-dot line level)
            m_ctrl = value;
            m_t = static_cast<uint16_t>((m_t & 0xF3FF) | ((value & 0x03) << 10));
            break;
        case 1: // PPUMASK
            m_mask = value;
            break;
        case 3: // OAMADDR
            m_oamAddr = value;
            break;
        case 4: // OAMDATA
            writeOamData(value);
            break;
        case 5: // PPUSCROLL (two writes: X then Y)
            if (!m_w) {
                m_t = static_cast<uint16_t>((m_t & 0xFFE0) | (value >> 3));
                m_x = static_cast<uint8_t>(value & 0x07);
                m_w = true;
            } else {
                m_t = static_cast<uint16_t>((m_t & 0x0C1F) |
                                            ((value & 0xF8) << 2) |
                                            ((value & 0x07) << 12));
                m_w = false;
            }
            break;
        case 6: // PPUADDR (two writes: high then low, v = t)
            if (!m_w) {
                m_t = static_cast<uint16_t>((m_t & 0x00FF) | ((value & 0x3F) << 8));
                m_w = true;
            } else {
                m_t = static_cast<uint16_t>((m_t & 0xFF00) | value);
                m_v = m_t;
                m_w = false;
            }
            break;
        case 7: // PPUDATA
            vramWrite(m_v, value);
            m_v = static_cast<uint16_t>(m_v + ((m_ctrl & 0x04) ? 32 : 1));
            break;
        default:
            break;
    }
}

void NesPPU::writeOamData(uint8_t value) {
    m_oam[m_oamAddr] = value;
    m_oamAddr++;
}

uint8_t NesPPU::readOamData() const {
    return m_oam[m_oamAddr];
}

void NesPPU::oamDma(std::span<const uint8_t> page) {
    // Hardware wraps within OAM starting at OAMADDR (which is ~always 0).
    const size_t count = std::min<size_t>(page.size(), m_oam.size());
    for (size_t i = 0; i < count; ++i) {
        m_oam[(m_oamAddr + i) & 0xFF] = page[i];
    }
}

void NesPPU::incrementX() {
    if ((m_v & 0x001F) == 31) {
        m_v &= ~0x001F; // Wrap coarse X...
        m_v ^= 0x0400;  // ...and switch horizontal nametable.
    } else {
        m_v++;
    }
}

void NesPPU::incrementY() {
    if ((m_v & 0x7000) != 0x7000) {
        m_v += 0x1000; // Fine Y++ (no carry out of the row).
    } else {
        m_v &= ~0x7000; // Fine Y wraps...
        uint16_t y = static_cast<uint16_t>((m_v & 0x03E0) >> 5);
        if (y == 29) {
            y = 0;           // Wrap coarse Y...
            m_v ^= 0x0800;   // ...and switch vertical nametable.
        } else if (y == 31) {
            y = 0; // Coarse Y wraps without switching (attribute overflow).
        } else {
            y++;
        }
        m_v = static_cast<uint16_t>((m_v & ~0x03E0) | (y << 5));
    }
}

void NesPPU::copyX() {
    // hori(v) = hori(t): coarse X + horizontal nametable bit.
    m_v = static_cast<uint16_t>((m_v & ~0x041F) | (m_t & 0x041F));
}

void NesPPU::copyY() {
    // vert(v) = vert(t): fine Y, coarse Y, vertical nametable bit.
    m_v = static_cast<uint16_t>((m_v & ~0x7BE0) | (m_t & 0x7BE0));
}

void NesPPU::stepDots(uint32_t dots, bool& nmiOut) {
    nmiOut = false;

    while (dots-- > 0) {
        m_dot++;

        // NMI line level: PPUCTRL bit 7 AND VBlank flag. Any rising edge
        // pulses the CPU (covers both VBlank entry and $2000 toggles).
        const bool nmiLine = (m_ctrl & 0x80) && (m_status & kVBlank);
        if (nmiLine && !m_prevNmiLine) {
            nmiOut = true;
        }
        m_prevNmiLine = nmiLine;

        if (m_scanline < 240) { // Visible lines
            if (m_dot == 256) {
                renderScanline(); // With this line's scroll state...
                incrementY();     // ...then advance to the next row.
            } else if (m_dot == 257) {
                copyX();
            }
        } else if (m_scanline == kPreRenderLine) {
            if (m_dot == 1) {
                // Pre-render: clear VBlank, sprite 0 hit, overflow.
                m_status &= ~(kVBlank | kSprite0Hit | kSpriteOverflow);
            } else if (m_dot >= 280 && m_dot <= 304) {
                copyY();
            }
        }

        if (m_dot == 1 && m_scanline == kVBlankLine) {
            m_status |= kVBlank; // Enter VBlank
        }

        if (m_dot >= kDotsPerLine) {
            m_dot = 0;
            m_scanline++;
            if (m_scanline >= kLinesPerFrame) {
                m_scanline = 0; // Odd-frame skip omitted (harmless 1-dot jitter)
            }
        }
    }
}

} // namespace smile::systems::nes
