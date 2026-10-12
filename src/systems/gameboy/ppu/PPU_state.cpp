#include "gameboy/ppu/PPU.hpp"

namespace smile::systems::gameboy {

void PPU::saveState(StateWriter& out) const {
    out.putBytes(m_vram);
    out.putBytes(m_oam);
    out.putU8(m_lcdc);
    out.putU8(m_stat);
    out.putU8(m_scy);
    out.putU8(m_scx);
    out.putU8(m_ly);
    out.putU8(m_lyc);
    out.putU8(m_bgp);
    out.putU8(m_obp0);
    out.putU8(m_obp1);
    out.putU8(m_wy);
    out.putU8(m_wx);
    out.putU32(static_cast<uint32_t>(m_dot));
    out.putU32(static_cast<uint32_t>(m_windowLine));
}

bool PPU::loadState(StateReader& in) {
    if (!in.readBytes(m_vram) || !in.readBytes(m_oam)) {
        return false;
    }

    uint8_t regs[11] = {};
    for (auto& reg : regs) {
        if (!in.readU8(reg)) {
            return false;
        }
    }
    uint32_t dot = 0;
    uint32_t windowLine = 0;
    if (!in.readU32(dot) || !in.readU32(windowLine)) {
        return false;
    }

    m_lcdc = regs[0];
    m_stat = regs[1];
    m_scy = regs[2];
    m_scx = regs[3];
    m_ly = regs[4];
    m_lyc = regs[5];
    m_bgp = regs[6];
    m_obp0 = regs[7];
    m_obp1 = regs[8];
    m_wy = regs[9];
    m_wx = regs[10];
    // Clamp the dot counter so a corrupt state cannot wedge the PPU.
    m_dot = (dot < static_cast<uint32_t>(kDotsPerLine)) ? static_cast<int>(dot) : 0;
    m_windowLine = static_cast<int>(windowLine);
    checkLyc();
    updateStatMode();
    return true;
}

} // namespace smile::systems::gameboy
