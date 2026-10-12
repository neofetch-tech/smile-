#include "gameboy/cpu/CPU.hpp"

namespace smile::systems::gameboy {

void CPU::saveState(StateWriter& out) const {
    out.putU16(m_regs.af);
    out.putU16(m_regs.bc);
    out.putU16(m_regs.de);
    out.putU16(m_regs.hl);
    out.putU16(m_regs.sp);
    out.putU16(m_regs.pc);
    out.putU8(m_ime ? 1 : 0);
    out.putU8(m_halted ? 1 : 0);
    out.putU8(m_eiDelay);
}

bool CPU::loadState(StateReader& in) {
    uint16_t af = 0;
    uint16_t bc = 0;
    uint16_t de = 0;
    uint16_t hl = 0;
    uint16_t sp = 0;
    uint16_t pc = 0;
    uint8_t ime = 0;
    uint8_t halted = 0;
    uint8_t eiDelay = 0;

    if (!in.readU16(af) || !in.readU16(bc) || !in.readU16(de) ||
        !in.readU16(hl) || !in.readU16(sp) || !in.readU16(pc) ||
        !in.readU8(ime) || !in.readU8(halted) || !in.readU8(eiDelay)) {
        return false;
    }

    m_regs.af = static_cast<uint16_t>(af & 0xFFF0); // Low nibble of F is always zero
    m_regs.bc = bc;
    m_regs.de = de;
    m_regs.hl = hl;
    m_regs.sp = sp;
    m_regs.pc = pc;
    m_ime = ime != 0;
    m_halted = halted != 0;
    m_eiDelay = eiDelay;
    return true;
}

} // namespace smile::systems::gameboy
