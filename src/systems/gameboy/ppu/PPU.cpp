#include "gameboy/ppu/PPU.hpp"
#include <algorithm>

namespace smile::systems::gameboy {

PPU::PPU() {
    m_vram.resize(8192, 0);
    m_oam.resize(160, 0);
    m_frameBuffer.resize(kWidth * kHeight, kDmgColors[0]);
}

void PPU::reset() {
    std::fill(m_vram.begin(), m_vram.end(), 0);
    std::fill(m_oam.begin(), m_oam.end(), 0);
    std::fill(m_frameBuffer.begin(), m_frameBuffer.end(), kDmgColors[0]);

    m_lcdc = 0x91;
    m_stat = 0x80;
    m_scy = 0;
    m_scx = 0;
    m_ly = 0;
    m_lyc = 0;
    m_bgp = 0xFC;
    m_obp0 = 0xFF;
    m_obp1 = 0xFF;
    m_wy = 0;
    m_wx = 0;
    m_dot = 0;
    m_windowLine = 0;
}

uint8_t PPU::readVRAM(uint16_t address) const {
    if (address >= 0x8000 && address <= 0x9FFF) {
        return m_vram[address - 0x8000];
    }
    return 0xFF;
}

void PPU::writeVRAM(uint16_t address, uint8_t value) {
    if (address >= 0x8000 && address <= 0x9FFF) {
        m_vram[address - 0x8000] = value;
    }
}

uint8_t PPU::readOAM(uint16_t address) const {
    if (address >= 0xFE00 && address <= 0xFE9F) {
        return m_oam[address - 0xFE00];
    }
    return 0xFF;
}

void PPU::writeOAM(uint16_t address, uint8_t value) {
    if (address >= 0xFE00 && address <= 0xFE9F) {
        m_oam[address - 0xFE00] = value;
    }
}

void PPU::dmaCopy(std::span<const uint8_t> source) {
    const size_t count = std::min<size_t>(source.size(), m_oam.size());
    std::copy_n(source.begin(), count, m_oam.begin());
}

uint8_t PPU::readRegister(uint16_t address) const {
    switch (address) {
        case 0xFF40: return m_lcdc;
        case 0xFF41: return m_stat | 0x80; // Bit 7 always reads 1
        case 0xFF42: return m_scy;
        case 0xFF43: return m_scx;
        case 0xFF44: return m_ly;
        case 0xFF45: return m_lyc;
        case 0xFF47: return m_bgp;
        case 0xFF48: return m_obp0;
        case 0xFF49: return m_obp1;
        case 0xFF4A: return m_wy;
        case 0xFF4B: return m_wx;
        default: return 0xFF;
    }
}

void PPU::writeRegister(uint16_t address, uint8_t value) {
    switch (address) {
        case 0xFF40: {
            bool wasEnabled = (m_lcdc & 0x80) != 0;
            m_lcdc = value;
            bool nowEnabled = (m_lcdc & 0x80) != 0;
            if (wasEnabled && !nowEnabled) {
                // Disabling LCD resets LY and dot counter.
                m_ly = 0;
                m_dot = 0;
                m_stat &= ~0x03;
                m_windowLine = 0;
            }
            break;
        }
        case 0xFF41: m_stat = (m_stat & 0x07) | (value & 0x78) | 0x80; break;
        case 0xFF42: m_scy = value; break;
        case 0xFF43: m_scx = value; break;
        case 0xFF44: break; // LY is read-only
        case 0xFF45: m_lyc = value; checkLyc(); break;
        case 0xFF47: m_bgp = value; break;
        case 0xFF48: m_obp0 = value; break;
        case 0xFF49: m_obp1 = value; break;
        case 0xFF4A: m_wy = value; break;
        case 0xFF4B: m_wx = value; break;
        default: break;
    }
}

void PPU::updateStatMode() {
    uint8_t newMode;
    if (m_ly >= kVBlankStart) {
        newMode = 1; // VBlank
    } else if (m_dot < 80) {
        newMode = 2; // OAM search
    } else if (m_dot < 80 + 172) {
        newMode = 3; // Pixel transfer
    } else {
        newMode = 0; // HBlank
    }
    m_stat = (m_stat & ~0x03) | newMode;
}

void PPU::checkLyc() {
    if (m_ly == m_lyc) {
        m_stat |= (1 << 2);
    } else {
        m_stat &= ~(1 << 2);
    }
}

void PPU::step(uint32_t tCycles, bool& outVBlankInterrupt, bool& outStatInterrupt) {
    outVBlankInterrupt = false;
    outStatInterrupt = false;

    // LCD disabled: freeze output.
    if ((m_lcdc & 0x80) == 0) {
        return;
    }

    uint8_t prevMode = m_stat & 0x03;
    bool prevLycFlag = (m_stat & 0x04) != 0;

    while (tCycles-- > 0) {
        m_dot++;

        if (m_dot >= kDotsPerLine) {
            m_dot = 0;

            // Leaving visible line: render it now.
            if (m_ly < kVBlankStart) {
                renderScanline();
            }

            m_ly++;

            if (m_ly == kVBlankStart) {
                outVBlankInterrupt = true; // Enter VBlank
            } else if (m_ly >= kLinesPerFrame) {
                m_ly = 0;
                m_windowLine = 0;
            }

            checkLyc();
            // LYC=LY edge can raise STAT.
            if ((m_stat & 0x04) && !prevLycFlag && (m_stat & 0x40)) {
                outStatInterrupt = true;
            }
            prevLycFlag = (m_stat & 0x04) != 0;
        }

        updateStatMode();
        uint8_t mode = m_stat & 0x03;

        // STAT mode-change edge detection.
        if (mode != prevMode) {
            bool hblankIrq = (mode == 0) && (m_stat & 0x08);
            bool vblankIrq = (mode == 1) && (m_stat & 0x10);
            bool oamIrq = (mode == 2) && (m_stat & 0x20);
            if (hblankIrq || vblankIrq || oamIrq) {
                outStatInterrupt = true;
            }
            prevMode = mode;
        }
    }
}

void PPU::renderScanline() {
    if (m_ly >= kHeight) {
        return;
    }
    // If background disabled, clear line to lightest shade.
    if ((m_lcdc & 0x01) == 0) {
        std::fill_n(m_frameBuffer.begin() + m_ly * kWidth, kWidth, kDmgColors[0]);
    } else {
        renderBackgroundAndWindowScanline();
    }
    if (m_lcdc & 0x02) {
        renderSpritesScanline();
    }
}

uint32_t PPU::decodeColor(uint8_t colorNum, uint8_t paletteReg) const {
    const uint8_t idx = (paletteReg >> (colorNum * 2)) & 0x03;
    return kDmgColors[idx];
}

void PPU::renderBackgroundAndWindowScanline() {
    const bool unsignedAddressing = (m_lcdc & 0x10) != 0;
    const uint16_t bgMapBase = (m_lcdc & 0x08) ? 0x9C00 : 0x9800;
    const uint16_t winMapBase = (m_lcdc & 0x40) ? 0x9C00 : 0x9800;
    const bool windowEnabled = (m_lcdc & 0x20) != 0;

    // Window is visible on this line when WY <= LY and WX <= 166.
    const bool windowOnLine = windowEnabled && (m_wy <= m_ly);
    if (windowOnLine && m_ly < kHeight) {
        // Advance internal window line counter once per rendered line.
        // Increment lazily at first window pixel; do it here if any window pixel visible.
        if (m_wx <= 166) {
            // Defer increment until after the line is drawn.
        }
    }

    for (uint32_t pixel = 0; pixel < kWidth; ++pixel) {
        bool useWindow = false;
        uint8_t xPos = 0;
        uint8_t yPos = 0;

        if (windowOnLine && pixel + 7 >= m_wx && m_wx <= 166) {
            useWindow = true;
            xPos = static_cast<uint8_t>(pixel + 7 - m_wx);
            yPos = static_cast<uint8_t>(m_windowLine);
        } else {
            xPos = static_cast<uint8_t>(m_scx + pixel);
            yPos = static_cast<uint8_t>(m_scy + m_ly);
        }

        const uint16_t mapBase = useWindow ? winMapBase : bgMapBase;
        const uint16_t tileAddr = mapBase + ((yPos / 8) * 32) + (xPos / 8);
        const uint8_t tileIndex = readVRAM(tileAddr);

        uint16_t tileDataAddr;
        if (unsignedAddressing) {
            tileDataAddr = 0x8000 + (tileIndex * 16);
        } else {
            const int8_t sIdx = static_cast<int8_t>(tileIndex);
            tileDataAddr = static_cast<uint16_t>(0x9000 + (sIdx * 16));
        }

        const uint8_t line = yPos % 8;
        const uint8_t lo = readVRAM(tileDataAddr + (line * 2));
        const uint8_t hi = readVRAM(tileDataAddr + (line * 2) + 1);
        const uint8_t bit = 7 - (xPos % 8);
        const uint8_t colorNum = (((hi >> bit) & 0x01) << 1) | ((lo >> bit) & 0x01);

        m_frameBuffer[m_ly * kWidth + pixel] = decodeColor(colorNum, m_bgp);
    }

    if (windowOnLine && m_wx <= 166) {
        m_windowLine++;
    }
}

void PPU::renderSpritesScanline() {
    const uint8_t spriteHeight = (m_lcdc & 0x04) ? 16 : 8;

    // Hardware selects the first 10 overlapping sprites in OAM order, but the
    // lowest OAM index wins overlapping pixels — so select ascending, then
    // draw in reverse.
    int selected[10];
    int count = 0;
    for (int i = 0; i < 40 && count < 10; ++i) {
        const int yPos = m_oam[i * 4] - 16;
        if (m_ly >= yPos && m_ly < yPos + spriteHeight) {
            selected[count++] = i;
        }
    }

    for (int k = count - 1; k >= 0; --k) {
        const int i = selected[k];
        const int yPos = m_oam[i * 4] - 16;
        const int xPos = m_oam[i * 4 + 1] - 8;
        uint8_t tileIndex = m_oam[i * 4 + 2];
        const uint8_t flags = m_oam[i * 4 + 3];

        const bool yFlip = flags & 0x40;
        const bool xFlip = flags & 0x20;
        const uint8_t palette = (flags & 0x10) ? m_obp1 : m_obp0;
        const bool behindBg = flags & 0x80;

        int row = m_ly - yPos;
        if (yFlip) {
            row = spriteHeight - 1 - row;
        }
        if (spriteHeight == 16) {
            // In 8x16 mode bit 0 selects top/bottom tile.
            tileIndex &= 0xFE;
        }

        const uint16_t addr = 0x8000 + (tileIndex * 16) + (row * 2);
        const uint8_t lo = readVRAM(addr);
        const uint8_t hi = readVRAM(addr + 1);

        for (int col = 0; col < 8; ++col) {
            const int px = xPos + col;
            if (px < 0 || px >= static_cast<int>(kWidth)) {
                continue;
            }
            const int bit = xFlip ? col : (7 - col);
            const uint8_t colorNum = (((hi >> bit) & 0x01) << 1) | ((lo >> bit) & 0x01);
            if (colorNum == 0) {
                continue; // Transparent
            }
            if (behindBg) {
                // Sprite is hidden where background color is not 0.
                // We re-derive BG color 0 check from current pixel palette index is
                // expensive; approximate by skipping if pixel is not lightest shade.
                // (Accurate priority needs per-pixel BG ids; acceptable for bring-up.)
                const uint32_t bgPixel = m_frameBuffer[m_ly * kWidth + px];
                if (bgPixel != decodeColor(0, m_bgp)) {
                    continue;
                }
            }
            m_frameBuffer[m_ly * kWidth + px] = decodeColor(colorNum, palette);
        }
    }
}

} // namespace smile::systems::gameboy
