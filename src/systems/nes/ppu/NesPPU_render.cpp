#include "nes/ppu/NesPPU.hpp"
#include "nes/cartridge/NesCartridge.hpp"

namespace smile::systems::nes {

namespace {
// Classic 64-entry NES palette (PVM-style), stored as ARGB8888.
constexpr uint32_t kNesPalette[64] = {
    0xFF666666, 0xFF002A88, 0xFF1412A7, 0xFF3B00A4, 0xFF5C007E, 0xFF6E0040, 0xFF6C0700, 0xFF573300,
    0xFF305400, 0xFF0B6700, 0xFF005200, 0xFF004F43, 0xFF001C6B, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFADADAD, 0xFF155FD9, 0xFF4240FF, 0xFF7527FE, 0xFFA01ACC, 0xFFE045C4, 0xFFE057A2, 0xFFE57553,
    0xFFB97A18, 0xFF0B9300, 0xFF108800, 0xFF00A681, 0xFF0098AA, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFFFFFFF, 0xFF64B0FF, 0xFF9290FF, 0xFFC676FF, 0xFFF36AFF, 0xFFFE6ECC, 0xFFFE8170, 0xFFFE9A4B,
    0xFFE8AB00, 0xFF5BC211, 0xFF18D800, 0xFF1BCB96, 0xFF00B9C8, 0xFF3C3C3C, 0xFF000000, 0xFF000000,
    0xFFFFFFFF, 0xFFC4D4FF, 0xFFD4D4FF, 0xFFF0D0FF, 0xFFF8B8F8, 0xFFFEC0C8, 0xFFFEC4A8, 0xFFFEC8B0,
    0xFFE8E0A8, 0xFFD8F078, 0xFFB8F8B8, 0xFFB0F0CF, 0xFF00E8D8, 0xFF787878, 0xFF000000, 0xFF000000,
};
} // namespace

uint32_t NesPPU::nesColor(uint8_t paletteIndex) const {
    uint8_t index = static_cast<uint8_t>(paletteIndex & 0x3F);
    if (m_mask & 0x01) { // Grayscale bit forces the hue bits off
        index &= 0x30;
    }
    return kNesPalette[index];
}

void NesPPU::renderScanline() {
    if (m_scanline < 0 || m_scanline >= static_cast<int>(kHeight)) {
        return;
    }
    const bool showBg = (m_mask & 0x08) != 0;
    const bool showSprites = (m_mask & 0x10) != 0;
    if (!showBg && !showSprites) {
        // Rendering disabled: the whole line is the backdrop color.
        const uint32_t backdrop = nesColor(paletteRead(0x3F00));
        std::fill_n(m_frame.begin() + m_scanline * kWidth, kWidth, backdrop);
        return;
    }
    // Per-line sprite-0 hit tracking needs a clean background layer first.
    renderBackground();
    if (showSprites) {
        renderSprites();
    }
}

void NesPPU::renderBackground() {
    const bool showBg = (m_mask & 0x08) != 0;
    const uint32_t backdrop = nesColor(paletteRead(0x3F00));
    uint32_t* line = m_frame.data() + m_scanline * kWidth;

    if (!showBg) {
        std::fill_n(line, kWidth, backdrop);
        return;
    }

    const uint16_t tableBase = (m_ctrl & 0x10) ? 0x1000 : 0x0000;
    const uint8_t fineY = static_cast<uint8_t>((m_v >> 12) & 0x07);
    const bool clipLeft = (m_mask & 0x02) == 0;

    uint16_t v = m_v; // Local copy: coarse X steps as tiles are drawn
    int screenX = -m_x;
    for (int tile = 0; tile < 33; ++tile) {
        const uint16_t ntAddr = static_cast<uint16_t>(0x2000 | (v & 0x0FFF));
        const uint8_t tileIndex = vramRead(ntAddr);
        const uint16_t attrAddr =
            static_cast<uint16_t>(0x23C0 | (v & 0x0C00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07));
        const uint8_t attr = vramRead(attrAddr);
        const uint8_t shift = static_cast<uint8_t>(((v >> 4) & 0x04) | (v & 0x02));
        const uint8_t palette = static_cast<uint8_t>((attr >> shift) & 0x03);

        const uint16_t patternAddr =
            static_cast<uint16_t>(tableBase + tileIndex * 16 + fineY);
        const uint8_t lo = chrRead(patternAddr);
        const uint8_t hi = chrRead(static_cast<uint16_t>(patternAddr + 8));

        for (int px = 0; px < 8; ++px) {
            const int x = screenX + px;
            if (x < 0 || x >= static_cast<int>(kWidth)) {
                continue;
            }
            if (clipLeft && x < 8) {
                line[x] = backdrop;
                continue;
            }
            const uint8_t bit = static_cast<uint8_t>(7 - px);
            const uint8_t color =
                static_cast<uint8_t>((((hi >> bit) & 1) << 1) | ((lo >> bit) & 1));
            line[x] = (color == 0) ? backdrop
                : nesColor(paletteRead(static_cast<uint16_t>(0x3F00 | (palette << 2) | color)));
        }

        screenX += 8;
        // Coarse X increment (mirrors incrementX without touching m_v).
        if ((v & 0x001F) == 31) {
            v &= ~0x001F;
            v ^= 0x0400;
        } else {
            v++;
        }
    }
}

void NesPPU::renderSprites() {
    const bool tall = (m_ctrl & 0x20) != 0; // 8x16 mode
    const uint16_t table8 = (m_ctrl & 0x08) ? 0x1000 : 0x0000;
    const bool clipLeft = (m_mask & 0x04) == 0;
    const bool showBg = (m_mask & 0x08) != 0;
    uint32_t* line = m_frame.data() + m_scanline * kWidth;

    // Snapshot background pixels for priority + sprite-0 hit tests.
    uint32_t bgLine[kWidth];
    for (uint32_t x = 0; x < kWidth; ++x) {
        bgLine[x] = line[x];
    }
    const uint32_t backdrop = nesColor(paletteRead(0x3F00));

    bool pixelTaken[kWidth] = {};
    int found = 0;
    for (int i = 0; i < 64; ++i) {
        const uint8_t yTop = static_cast<uint8_t>(m_oam[i * 4] + 1);
        const uint8_t height = tall ? 16 : 8;
        if (m_scanline < yTop || m_scanline >= yTop + height) {
            continue;
        }
        if (found >= 8) {
            m_status |= 0x20; // Sprite overflow (simplified, no hardware bug)
            break;
        }
        found++;

        uint8_t tileIndex = m_oam[i * 4 + 1];
        const uint8_t attrs = m_oam[i * 4 + 2];
        const int xPos = m_oam[i * 4 + 3];
        const uint8_t palette = static_cast<uint8_t>((attrs & 0x03) + 4);
        const bool behind = (attrs & 0x20) != 0;
        const bool flipH = (attrs & 0x40) != 0;
        const bool flipV = (attrs & 0x80) != 0;

        int row = m_scanline - yTop;
        if (flipV) {
            row = height - 1 - row;
        }
        uint16_t tableBase = table8;
        if (tall) {
            // 8x16: pattern table from tile bit 0, tile pair from bit 0 clear.
            tableBase = (tileIndex & 0x01) ? 0x1000 : 0x0000;
            tileIndex &= 0xFE;
            if (row >= 8) {
                tileIndex++;
                row -= 8;
            }
        }
        const uint16_t patternAddr =
            static_cast<uint16_t>(tableBase + tileIndex * 16 + row);
        const uint8_t lo = chrRead(patternAddr);
        const uint8_t hi = chrRead(static_cast<uint16_t>(patternAddr + 8));

        for (int col = 0; col < 8; ++col) {
            const int x = xPos + col;
            if (x < 0 || x >= static_cast<int>(kWidth)) {
                continue;
            }
            if (clipLeft && x < 8) {
                continue;
            }
            const uint8_t bit = static_cast<uint8_t>(flipH ? col : (7 - col));
            const uint8_t color =
                static_cast<uint8_t>((((hi >> bit) & 1) << 1) | ((lo >> bit) & 1));
            if (color == 0) {
                continue; // Transparent
            }
            const bool bgOpaque = showBg && bgLine[x] != backdrop;
            // Sprite-0 hit: opaque sprite-0 pixel over an opaque BG pixel.
            // (Evaluated even when the sprite is behind the background.)
            if (i == 0 && bgOpaque) {
                if (x >= 8 || ((m_mask & 0x02) && (m_mask & 0x04))) {
                    m_status |= 0x40;
                }
            }
            if (pixelTaken[x]) {
                continue; // Lower OAM index wins
            }
            pixelTaken[x] = true;
            if (behind && bgOpaque) {
                continue;
            }
                line[x] = nesColor(paletteRead(static_cast<uint16_t>(0x3F10 | (palette << 2) | color)));
        }
    }
}

} // namespace smile::systems::nes
