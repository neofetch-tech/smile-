#include "gameboy/bus/MemoryBus.hpp"

namespace smile::systems::gameboy {

void MemoryBus::saveState(StateWriter& out) const {
    out.putBytes(m_wram);
    out.putBytes(m_hram);
    out.putU8(m_if);
    out.putU8(m_ie);
    out.putU8(m_joypadSelect);
    out.putU8(m_joypadState);
    out.putU8(m_serialData);
    out.putU8(m_serialCtrl);
    m_cartridge.saveState(out);
    m_ppu.saveState(out);
    m_timer.saveState(out);
    m_apu.saveState(out);
}

bool MemoryBus::loadState(StateReader& in) {
    if (!in.readBytes(m_wram) || !in.readBytes(m_hram)) {
        return false;
    }

    uint8_t irqFlags = 0;
    uint8_t irqEnable = 0;
    uint8_t joySelect = 0;
    uint8_t joyState = 0;
    uint8_t serialData = 0;
    uint8_t serialCtrl = 0;
    if (!in.readU8(irqFlags) || !in.readU8(irqEnable) ||
        !in.readU8(joySelect) || !in.readU8(joyState) ||
        !in.readU8(serialData) || !in.readU8(serialCtrl)) {
        return false;
    }

    // ROM is not part of the state: it is identified by the loaded cartridge.
    if (!m_cartridge.loadState(in) || !m_ppu.loadState(in) || !m_timer.loadState(in) ||
        !m_apu.loadState(in)) {
        return false;
    }

    m_if = irqFlags;
    m_ie = irqEnable;
    m_joypadSelect = static_cast<uint8_t>(joySelect & 0x30);
    m_joypadState = joyState;
    m_serialData = serialData;
    m_serialCtrl = serialCtrl;
    return true;
}

} // namespace smile::systems::gameboy
