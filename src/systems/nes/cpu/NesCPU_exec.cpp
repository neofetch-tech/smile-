#include "nes/cpu/NesCPU.hpp"
#include "nes/bus/NesBus.hpp"

namespace smile::systems::nes {

namespace {
// Extra cycle when indexed addressing crosses a page boundary.
[[nodiscard]] uint8_t crossCost(bool crossed, uint8_t base) {
    return crossed ? static_cast<uint8_t>(base + 1) : base;
}
} // namespace

uint16_t NesCPU::step() {
    // OAM DMA debt from the bus is served before anything else.
    if (const uint16_t stall = m_bus.takeDmaStall(); stall != 0) {
        return stall;
    }
    if (m_nmi) {
        m_nmi = false;
        serviceNmi();
        return 7;
    }

    const uint8_t op = m_bus.read8(m_regs.pc++);
    bool crossed = false;

    switch (op) {
        // BRK / control
        case 0x00: { // BRK
            m_regs.pc++;
            push(static_cast<uint8_t>(m_regs.pc >> 8));
            push(static_cast<uint8_t>(m_regs.pc & 0xFF));
            push(static_cast<uint8_t>(m_regs.status | F_B | F_U));
            setFlag(F_I, true);
            m_regs.pc = static_cast<uint16_t>(m_bus.read8(0xFFFE) |
                                              (m_bus.read8(0xFFFF) << 8));
            return 7;
        }
        case 0x40: { // RTI
            m_regs.status = static_cast<uint8_t>((pull() & ~F_B) | F_U);
            const uint8_t lo = pull();
            const uint8_t hi = pull();
            m_regs.pc = static_cast<uint16_t>(lo | (hi << 8));
            return 6;
        }
        case 0x60: { // RTS
            const uint8_t lo = pull();
            const uint8_t hi = pull();
            m_regs.pc = static_cast<uint16_t>((lo | (hi << 8)) + 1);
            return 6;
        }
        case 0x20: { // JSR
            const uint16_t target = fetch16();
            const uint16_t ret = static_cast<uint16_t>(m_regs.pc - 1);
            push(static_cast<uint8_t>(ret >> 8));
            push(static_cast<uint8_t>(ret & 0xFF));
            m_regs.pc = target;
            return 6;
        }
        case 0x4C: m_regs.pc = fetchAddr(Mode::Abs, crossed); return 3; // JMP abs
        case 0x6C: m_regs.pc = fetchAddr(Mode::Ind, crossed); return 5; // JMP ind

        // Branches
        case 0x10: return branch((m_regs.status & F_N) == 0); // BPL
        case 0x30: return branch((m_regs.status & F_N) != 0); // BMI
        case 0x50: return branch((m_regs.status & F_V) == 0); // BVC
        case 0x70: return branch((m_regs.status & F_V) != 0); // BVS
        case 0x90: return branch((m_regs.status & F_C) == 0); // BCC
        case 0xB0: return branch((m_regs.status & F_C) != 0); // BCS
        case 0xD0: return branch((m_regs.status & F_Z) == 0); // BNE
        case 0xF0: return branch((m_regs.status & F_Z) != 0); // BEQ

        // Flags
        case 0x18: setFlag(F_C, false); return 2; // CLC
        case 0x38: setFlag(F_C, true); return 2;  // SEC
        case 0x58: setFlag(F_I, false); return 2; // CLI
        case 0x78: setFlag(F_I, true); return 2;  // SEI
        case 0xB8: setFlag(F_V, false); return 2; // CLV
        case 0xD8: setFlag(F_D, false); return 2; // CLD
        case 0xF8: setFlag(F_D, true); return 2;  // SED
        case 0xEA: return 2;                      // NOP

        // Stack
        case 0x08: push(static_cast<uint8_t>(m_regs.status | F_B | F_U)); return 3; // PHP
        case 0x28: m_regs.status = static_cast<uint8_t>((pull() & ~F_B) | F_U); return 4; // PLP
        case 0x48: push(m_regs.a); return 3; // PHA
        case 0x68: m_regs.a = pull(); setNZ(m_regs.a); return 4; // PLA

        // Transfers
        case 0xAA: m_regs.x = m_regs.a; setNZ(m_regs.x); return 2; // TAX
        case 0x8A: m_regs.a = m_regs.x; setNZ(m_regs.a); return 2; // TXA
        case 0xA8: m_regs.y = m_regs.a; setNZ(m_regs.y); return 2; // TAY
        case 0x98: m_regs.a = m_regs.y; setNZ(m_regs.a); return 2; // TYA
        case 0xBA: m_regs.x = m_regs.sp; setNZ(m_regs.x); return 2; // TSX
        case 0x9A: m_regs.sp = m_regs.x; return 2; // TXS

        // Increments / decrements
        case 0xE8: m_regs.x++; setNZ(m_regs.x); return 2; // INX
        case 0xC8: m_regs.y++; setNZ(m_regs.y); return 2; // INY
        case 0xCA: m_regs.x--; setNZ(m_regs.x); return 2; // DEX
        case 0x88: m_regs.y--; setNZ(m_regs.y); return 2; // DEY

        // ORA
        case 0x09: m_regs.a |= readOp(Mode::Imm, crossed); setNZ(m_regs.a); return 2;
        case 0x05: m_regs.a |= readOp(Mode::Zp, crossed); setNZ(m_regs.a); return 3;
        case 0x15: m_regs.a |= readOp(Mode::ZpX, crossed); setNZ(m_regs.a); return 4;
        case 0x0D: m_regs.a |= readOp(Mode::Abs, crossed); setNZ(m_regs.a); return 4;
        case 0x1D: m_regs.a |= readOp(Mode::AbsX, crossed); setNZ(m_regs.a); return crossCost(crossed, 4);
        case 0x19: m_regs.a |= readOp(Mode::AbsY, crossed); setNZ(m_regs.a); return crossCost(crossed, 4);
        case 0x01: m_regs.a |= readOp(Mode::IndX, crossed); setNZ(m_regs.a); return 6;
        case 0x11: m_regs.a |= readOp(Mode::IndY, crossed); setNZ(m_regs.a); return crossCost(crossed, 5);

        // AND
        case 0x29: m_regs.a &= readOp(Mode::Imm, crossed); setNZ(m_regs.a); return 2;
        case 0x25: m_regs.a &= readOp(Mode::Zp, crossed); setNZ(m_regs.a); return 3;
        case 0x35: m_regs.a &= readOp(Mode::ZpX, crossed); setNZ(m_regs.a); return 4;
        case 0x2D: m_regs.a &= readOp(Mode::Abs, crossed); setNZ(m_regs.a); return 4;
        case 0x3D: m_regs.a &= readOp(Mode::AbsX, crossed); setNZ(m_regs.a); return crossCost(crossed, 4);
        case 0x39: m_regs.a &= readOp(Mode::AbsY, crossed); setNZ(m_regs.a); return crossCost(crossed, 4);
        case 0x21: m_regs.a &= readOp(Mode::IndX, crossed); setNZ(m_regs.a); return 6;
        case 0x31: m_regs.a &= readOp(Mode::IndY, crossed); setNZ(m_regs.a); return crossCost(crossed, 5);

        // EOR
        case 0x49: m_regs.a ^= readOp(Mode::Imm, crossed); setNZ(m_regs.a); return 2;
        case 0x45: m_regs.a ^= readOp(Mode::Zp, crossed); setNZ(m_regs.a); return 3;
        case 0x55: m_regs.a ^= readOp(Mode::ZpX, crossed); setNZ(m_regs.a); return 4;
        case 0x4D: m_regs.a ^= readOp(Mode::Abs, crossed); setNZ(m_regs.a); return 4;
        case 0x5D: m_regs.a ^= readOp(Mode::AbsX, crossed); setNZ(m_regs.a); return crossCost(crossed, 4);
        case 0x59: m_regs.a ^= readOp(Mode::AbsY, crossed); setNZ(m_regs.a); return crossCost(crossed, 4);
        case 0x41: m_regs.a ^= readOp(Mode::IndX, crossed); setNZ(m_regs.a); return 6;
        case 0x51: m_regs.a ^= readOp(Mode::IndY, crossed); setNZ(m_regs.a); return crossCost(crossed, 5);

        // ADC
        case 0x69: adc(readOp(Mode::Imm, crossed)); return 2;
        case 0x65: adc(readOp(Mode::Zp, crossed)); return 3;
        case 0x75: adc(readOp(Mode::ZpX, crossed)); return 4;
        case 0x6D: adc(readOp(Mode::Abs, crossed)); return 4;
        case 0x7D: adc(readOp(Mode::AbsX, crossed)); return crossCost(crossed, 4);
        case 0x79: adc(readOp(Mode::AbsY, crossed)); return crossCost(crossed, 4);
        case 0x61: adc(readOp(Mode::IndX, crossed)); return 6;
        case 0x71: adc(readOp(Mode::IndY, crossed)); return crossCost(crossed, 5);

        // SBC
        case 0xE9: sbc(readOp(Mode::Imm, crossed)); return 2;
        case 0xE5: sbc(readOp(Mode::Zp, crossed)); return 3;
        case 0xF5: sbc(readOp(Mode::ZpX, crossed)); return 4;
        case 0xED: sbc(readOp(Mode::Abs, crossed)); return 4;
        case 0xFD: sbc(readOp(Mode::AbsX, crossed)); return crossCost(crossed, 4);
        case 0xF9: sbc(readOp(Mode::AbsY, crossed)); return crossCost(crossed, 4);
        case 0xE1: sbc(readOp(Mode::IndX, crossed)); return 6;
        case 0xF1: sbc(readOp(Mode::IndY, crossed)); return crossCost(crossed, 5);

        // CMP
        case 0xC9: cmpReg(m_regs.a, readOp(Mode::Imm, crossed)); return 2;
        case 0xC5: cmpReg(m_regs.a, readOp(Mode::Zp, crossed)); return 3;
        case 0xD5: cmpReg(m_regs.a, readOp(Mode::ZpX, crossed)); return 4;
        case 0xCD: cmpReg(m_regs.a, readOp(Mode::Abs, crossed)); return 4;
        case 0xDD: cmpReg(m_regs.a, readOp(Mode::AbsX, crossed)); return crossCost(crossed, 4);
        case 0xD9: cmpReg(m_regs.a, readOp(Mode::AbsY, crossed)); return crossCost(crossed, 4);
        case 0xC1: cmpReg(m_regs.a, readOp(Mode::IndX, crossed)); return 6;
        case 0xD1: cmpReg(m_regs.a, readOp(Mode::IndY, crossed)); return crossCost(crossed, 5);

        // CPX / CPY
        case 0xE0: cmpReg(m_regs.x, readOp(Mode::Imm, crossed)); return 2;
        case 0xE4: cmpReg(m_regs.x, readOp(Mode::Zp, crossed)); return 3;
        case 0xEC: cmpReg(m_regs.x, readOp(Mode::Abs, crossed)); return 4;
        case 0xC0: cmpReg(m_regs.y, readOp(Mode::Imm, crossed)); return 2;
        case 0xC4: cmpReg(m_regs.y, readOp(Mode::Zp, crossed)); return 3;
        case 0xCC: cmpReg(m_regs.y, readOp(Mode::Abs, crossed)); return 4;

        // LDA
        case 0xA9: m_regs.a = readOp(Mode::Imm, crossed); setNZ(m_regs.a); return 2;
        case 0xA5: m_regs.a = readOp(Mode::Zp, crossed); setNZ(m_regs.a); return 3;
        case 0xB5: m_regs.a = readOp(Mode::ZpX, crossed); setNZ(m_regs.a); return 4;
        case 0xAD: m_regs.a = readOp(Mode::Abs, crossed); setNZ(m_regs.a); return 4;
        case 0xBD: m_regs.a = readOp(Mode::AbsX, crossed); setNZ(m_regs.a); return crossCost(crossed, 4);
        case 0xB9: m_regs.a = readOp(Mode::AbsY, crossed); setNZ(m_regs.a); return crossCost(crossed, 4);
        case 0xA1: m_regs.a = readOp(Mode::IndX, crossed); setNZ(m_regs.a); return 6;
        case 0xB1: m_regs.a = readOp(Mode::IndY, crossed); setNZ(m_regs.a); return crossCost(crossed, 5);

        // LDX
        case 0xA2: m_regs.x = readOp(Mode::Imm, crossed); setNZ(m_regs.x); return 2;
        case 0xA6: m_regs.x = readOp(Mode::Zp, crossed); setNZ(m_regs.x); return 3;
        case 0xB6: m_regs.x = readOp(Mode::ZpY, crossed); setNZ(m_regs.x); return 4;
        case 0xAE: m_regs.x = readOp(Mode::Abs, crossed); setNZ(m_regs.x); return 4;
        case 0xBE: m_regs.x = readOp(Mode::AbsY, crossed); setNZ(m_regs.x); return crossCost(crossed, 4);

        // LDY
        case 0xA0: m_regs.y = readOp(Mode::Imm, crossed); setNZ(m_regs.y); return 2;
        case 0xA4: m_regs.y = readOp(Mode::Zp, crossed); setNZ(m_regs.y); return 3;
        case 0xB4: m_regs.y = readOp(Mode::ZpX, crossed); setNZ(m_regs.y); return 4;
        case 0xAC: m_regs.y = readOp(Mode::Abs, crossed); setNZ(m_regs.y); return 4;
        case 0xBC: m_regs.y = readOp(Mode::AbsX, crossed); setNZ(m_regs.y); return crossCost(crossed, 4);

        // STA
        case 0x85: m_bus.write8(fetchAddr(Mode::Zp, crossed), m_regs.a); return 3;
        case 0x95: m_bus.write8(fetchAddr(Mode::ZpX, crossed), m_regs.a); return 4;
        case 0x8D: m_bus.write8(fetchAddr(Mode::Abs, crossed), m_regs.a); return 4;
        case 0x9D: m_bus.write8(fetchAddr(Mode::AbsX, crossed), m_regs.a); return 5;
        case 0x99: m_bus.write8(fetchAddr(Mode::AbsY, crossed), m_regs.a); return 5;
        case 0x81: m_bus.write8(fetchAddr(Mode::IndX, crossed), m_regs.a); return 6;
        case 0x91: m_bus.write8(fetchAddr(Mode::IndY, crossed), m_regs.a); return 6;

        // STX / STY
        case 0x86: m_bus.write8(fetchAddr(Mode::Zp, crossed), m_regs.x); return 3;
        case 0x96: m_bus.write8(fetchAddr(Mode::ZpY, crossed), m_regs.x); return 4;
        case 0x8E: m_bus.write8(fetchAddr(Mode::Abs, crossed), m_regs.x); return 4;
        case 0x84: m_bus.write8(fetchAddr(Mode::Zp, crossed), m_regs.y); return 3;
        case 0x94: m_bus.write8(fetchAddr(Mode::ZpX, crossed), m_regs.y); return 4;
        case 0x8C: m_bus.write8(fetchAddr(Mode::Abs, crossed), m_regs.y); return 4;

        // BIT
        case 0x24: bitOp(readOp(Mode::Zp, crossed)); return 3;
        case 0x2C: bitOp(readOp(Mode::Abs, crossed)); return 4;

        // ASL
        case 0x0A: m_regs.a = asl(m_regs.a); setNZ(m_regs.a); return 2;
        case 0x06: { const uint16_t a = fetchAddr(Mode::Zp, crossed);
                     const uint8_t v = asl(m_bus.read8(a));
                     m_bus.write8(a, v); setNZ(v); return 5; }
        case 0x16: { const uint16_t a = fetchAddr(Mode::ZpX, crossed);
                     const uint8_t v = asl(m_bus.read8(a));
                     m_bus.write8(a, v); setNZ(v); return 6; }
        case 0x0E: { const uint16_t a = fetchAddr(Mode::Abs, crossed);
                     const uint8_t v = asl(m_bus.read8(a));
                     m_bus.write8(a, v); setNZ(v); return 6; }
        case 0x1E: { const uint16_t a = fetchAddr(Mode::AbsX, crossed);
                     const uint8_t v = asl(m_bus.read8(a));
                     m_bus.write8(a, v); setNZ(v); return 7; }

        // LSR
        case 0x4A: m_regs.a = lsr(m_regs.a); setNZ(m_regs.a); return 2;
        case 0x46: { const uint16_t a = fetchAddr(Mode::Zp, crossed);
                     const uint8_t v = lsr(m_bus.read8(a));
                     m_bus.write8(a, v); setNZ(v); return 5; }
        case 0x56: { const uint16_t a = fetchAddr(Mode::ZpX, crossed);
                     const uint8_t v = lsr(m_bus.read8(a));
                     m_bus.write8(a, v); setNZ(v); return 6; }
        case 0x4E: { const uint16_t a = fetchAddr(Mode::Abs, crossed);
                     const uint8_t v = lsr(m_bus.read8(a));
                     m_bus.write8(a, v); setNZ(v); return 6; }
        case 0x5E: { const uint16_t a = fetchAddr(Mode::AbsX, crossed);
                     const uint8_t v = lsr(m_bus.read8(a));
                     m_bus.write8(a, v); setNZ(v); return 7; }

        // ROL
        case 0x2A: m_regs.a = rol(m_regs.a); setNZ(m_regs.a); return 2;
        case 0x26: { const uint16_t a = fetchAddr(Mode::Zp, crossed);
                     const uint8_t v = rol(m_bus.read8(a));
                     m_bus.write8(a, v); setNZ(v); return 5; }
        case 0x36: { const uint16_t a = fetchAddr(Mode::ZpX, crossed);
                     const uint8_t v = rol(m_bus.read8(a));
                     m_bus.write8(a, v); setNZ(v); return 6; }
        case 0x2E: { const uint16_t a = fetchAddr(Mode::Abs, crossed);
                     const uint8_t v = rol(m_bus.read8(a));
                     m_bus.write8(a, v); setNZ(v); return 6; }
        case 0x3E: { const uint16_t a = fetchAddr(Mode::AbsX, crossed);
                     const uint8_t v = rol(m_bus.read8(a));
                     m_bus.write8(a, v); setNZ(v); return 7; }

        // ROR
        case 0x6A: m_regs.a = ror(m_regs.a); setNZ(m_regs.a); return 2;
        case 0x66: { const uint16_t a = fetchAddr(Mode::Zp, crossed);
                     const uint8_t v = ror(m_bus.read8(a));
                     m_bus.write8(a, v); setNZ(v); return 5; }
        case 0x76: { const uint16_t a = fetchAddr(Mode::ZpX, crossed);
                     const uint8_t v = ror(m_bus.read8(a));
                     m_bus.write8(a, v); setNZ(v); return 6; }
        case 0x6E: { const uint16_t a = fetchAddr(Mode::Abs, crossed);
                     const uint8_t v = ror(m_bus.read8(a));
                     m_bus.write8(a, v); setNZ(v); return 6; }
        case 0x7E: { const uint16_t a = fetchAddr(Mode::AbsX, crossed);
                     const uint8_t v = ror(m_bus.read8(a));
                     m_bus.write8(a, v); setNZ(v); return 7; }

        // INC / DEC
        case 0xE6: { const uint16_t a = fetchAddr(Mode::Zp, crossed);
                     const uint8_t v = static_cast<uint8_t>(m_bus.read8(a) + 1);
                     m_bus.write8(a, v); setNZ(v); return 5; }
        case 0xF6: { const uint16_t a = fetchAddr(Mode::ZpX, crossed);
                     const uint8_t v = static_cast<uint8_t>(m_bus.read8(a) + 1);
                     m_bus.write8(a, v); setNZ(v); return 6; }
        case 0xEE: { const uint16_t a = fetchAddr(Mode::Abs, crossed);
                     const uint8_t v = static_cast<uint8_t>(m_bus.read8(a) + 1);
                     m_bus.write8(a, v); setNZ(v); return 6; }
        case 0xFE: { const uint16_t a = fetchAddr(Mode::AbsX, crossed);
                     const uint8_t v = static_cast<uint8_t>(m_bus.read8(a) + 1);
                     m_bus.write8(a, v); setNZ(v); return 7; }
        case 0xC6: { const uint16_t a = fetchAddr(Mode::Zp, crossed);
                     const uint8_t v = static_cast<uint8_t>(m_bus.read8(a) - 1);
                     m_bus.write8(a, v); setNZ(v); return 5; }
        case 0xD6: { const uint16_t a = fetchAddr(Mode::ZpX, crossed);
                     const uint8_t v = static_cast<uint8_t>(m_bus.read8(a) - 1);
                     m_bus.write8(a, v); setNZ(v); return 6; }
        case 0xCE: { const uint16_t a = fetchAddr(Mode::Abs, crossed);
                     const uint8_t v = static_cast<uint8_t>(m_bus.read8(a) - 1);
                     m_bus.write8(a, v); setNZ(v); return 6; }
        case 0xDE: { const uint16_t a = fetchAddr(Mode::AbsX, crossed);
                     const uint8_t v = static_cast<uint8_t>(m_bus.read8(a) - 1);
                     m_bus.write8(a, v); setNZ(v); return 7; }

        default:
            // Illegal opcode: safe single-byte NOP so bring-up never wedges.
            return 2;
    }
}

} // namespace smile::systems::nes
