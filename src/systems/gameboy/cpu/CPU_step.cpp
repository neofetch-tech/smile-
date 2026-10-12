#include "gameboy/cpu/CPU.hpp"
#include "gameboy/bus/MemoryBus.hpp"

namespace smile::systems::gameboy {

uint8_t CPU::executeCb(uint8_t cb) {
    // CB prefix: x = group, y = bit/sub-op, z = register id.
    const uint8_t x = cb >> 6;
    const uint8_t y = (cb >> 3) & 0x07;
    const uint8_t z = cb & 0x07;
    const bool isHl = (z == 6);

    auto readReg = [&](uint8_t id) -> uint8_t {
        switch (id) {
            case 0: return m_regs.b;
            case 1: return m_regs.c;
            case 2: return m_regs.d;
            case 3: return m_regs.e;
            case 4: return m_regs.h;
            case 5: return m_regs.l;
            case 6: return m_bus.read8(m_regs.hl);
            default: return m_regs.a;
        }
    };
    auto writeReg = [&](uint8_t id, uint8_t v) {
        switch (id) {
            case 0: m_regs.b = v; break;
            case 1: m_regs.c = v; break;
            case 2: m_regs.d = v; break;
            case 3: m_regs.e = v; break;
            case 4: m_regs.h = v; break;
            case 5: m_regs.l = v; break;
            case 6: m_bus.write8(m_regs.hl, v); break;
            default: m_regs.a = v; break;
        }
    };

    if (x == 1) { // BIT y, r
        setFlag(Flag_Z, ((readReg(z) >> y) & 1) == 0);
        setFlag(Flag_N, false);
        setFlag(Flag_H, true);
        return isHl ? 3 : 2;
    }
    if (x == 2) { // RES y, r
        writeReg(z, static_cast<uint8_t>(readReg(z) & ~(1 << y)));
        return isHl ? 4 : 2;
    }
    if (x == 3) { // SET y, r
        writeReg(z, static_cast<uint8_t>(readReg(z) | (1 << y)));
        return isHl ? 4 : 2;
    }

    // x == 0: rotates and shifts.
    const uint8_t v = readReg(z);
    uint8_t result = v;
    switch (y) {
        case 0: { // RLC
            const bool c = (v & 0x80) != 0;
            result = static_cast<uint8_t>((v << 1) | (c ? 1 : 0));
            setFlag(Flag_Z, result == 0);
            setFlag(Flag_N, false);
            setFlag(Flag_H, false);
            setFlag(Flag_C, c);
            break;
        }
        case 1: { // RRC
            const bool c = (v & 0x01) != 0;
            result = static_cast<uint8_t>((v >> 1) | (c ? 0x80 : 0));
            setFlag(Flag_Z, result == 0);
            setFlag(Flag_N, false);
            setFlag(Flag_H, false);
            setFlag(Flag_C, c);
            break;
        }
        case 2: { // RL through carry
            const bool c = (v & 0x80) != 0;
            result = static_cast<uint8_t>((v << 1) | (getFlag(Flag_C) ? 1 : 0));
            setFlag(Flag_Z, result == 0);
            setFlag(Flag_N, false);
            setFlag(Flag_H, false);
            setFlag(Flag_C, c);
            break;
        }
        case 3: { // RR through carry
            const bool c = (v & 0x01) != 0;
            result = static_cast<uint8_t>((v >> 1) | (getFlag(Flag_C) ? 0x80 : 0));
            setFlag(Flag_Z, result == 0);
            setFlag(Flag_N, false);
            setFlag(Flag_H, false);
            setFlag(Flag_C, c);
            break;
        }
        case 4: { // SLA
            const bool c = (v & 0x80) != 0;
            result = static_cast<uint8_t>(v << 1);
            setFlag(Flag_Z, result == 0);
            setFlag(Flag_N, false);
            setFlag(Flag_H, false);
            setFlag(Flag_C, c);
            break;
        }
        case 5: { // SRA
            const bool c = (v & 0x01) != 0;
            result = static_cast<uint8_t>((v >> 1) | (v & 0x80));
            setFlag(Flag_Z, result == 0);
            setFlag(Flag_N, false);
            setFlag(Flag_H, false);
            setFlag(Flag_C, c);
            break;
        }
        case 6: { // SWAP
            result = static_cast<uint8_t>(((v & 0x0F) << 4) | ((v & 0xF0) >> 4));
            setFlag(Flag_Z, result == 0);
            setFlag(Flag_N, false);
            setFlag(Flag_H, false);
            setFlag(Flag_C, false);
            break;
        }
        default: { // SRL
            const bool c = (v & 0x01) != 0;
            result = static_cast<uint8_t>(v >> 1);
            setFlag(Flag_Z, result == 0);
            setFlag(Flag_N, false);
            setFlag(Flag_H, false);
            setFlag(Flag_C, c);
            break;
        }
    }
    writeReg(z, result);
    return isHl ? 4 : 2;
}

uint8_t CPU::step() {
    // A pending EI takes effect after the following instruction.
    if (m_eiDelay > 0 && --m_eiDelay == 0) {
        m_ime = true;
    }

    // Service interrupts before fetching (also wakes the CPU from HALT).
    if (const uint8_t irqCycles = serviceInterrupts(); irqCycles != 0) {
        return irqCycles;
    }
    if (m_halted) {
        return 1; // Idle tick while halted with no enabled interrupt
    }

    const uint8_t opcode = m_bus.read8(m_regs.pc++);

    switch (opcode) {
        case 0x00: return 1; // NOP
        case 0x10: (void)m_bus.read8(m_regs.pc++); return 1; // STOP: skip next byte
        case 0x76: m_halted = true; return 1; // HALT
        case 0xF3: m_ime = false; m_eiDelay = 0; return 1; // DI
        case 0xFB: m_eiDelay = 2; return 1; // EI (delayed)
        case 0xCB: return executeCb(m_bus.read8(m_regs.pc++));

        case 0x06: m_regs.b = m_bus.read8(m_regs.pc++); return 2;
        case 0x0E: m_regs.c = m_bus.read8(m_regs.pc++); return 2;
        case 0x16: m_regs.d = m_bus.read8(m_regs.pc++); return 2;
        case 0x1E: m_regs.e = m_bus.read8(m_regs.pc++); return 2;
        case 0x26: m_regs.h = m_bus.read8(m_regs.pc++); return 2;
        case 0x2E: m_regs.l = m_bus.read8(m_regs.pc++); return 2;
        case 0x3E: m_regs.a = m_bus.read8(m_regs.pc++); return 2;
        case 0x36: m_bus.write8(m_regs.hl, m_bus.read8(m_regs.pc++)); return 3;

        case 0x01: m_regs.bc = m_bus.read16(m_regs.pc); m_regs.pc += 2; return 3;
        case 0x11: m_regs.de = m_bus.read16(m_regs.pc); m_regs.pc += 2; return 3;
        case 0x21: m_regs.hl = m_bus.read16(m_regs.pc); m_regs.pc += 2; return 3;
        case 0x31: m_regs.sp = m_bus.read16(m_regs.pc); m_regs.pc += 2; return 3;

        case 0x0A: m_regs.a = m_bus.read8(m_regs.bc); return 2;
        case 0x1A: m_regs.a = m_bus.read8(m_regs.de); return 2;
        case 0x2A: m_regs.a = m_bus.read8(m_regs.hl++); return 2;
        case 0x3A: m_regs.a = m_bus.read8(m_regs.hl--); return 2;
        case 0x7E: m_regs.a = m_bus.read8(m_regs.hl); return 2;
        case 0xFA: m_regs.a = m_bus.read8(m_bus.read16(m_regs.pc)); m_regs.pc += 2; return 4;

        case 0x02: m_bus.write8(m_regs.bc, m_regs.a); return 2;
        case 0x12: m_bus.write8(m_regs.de, m_regs.a); return 2;
        case 0x22: m_bus.write8(m_regs.hl++, m_regs.a); return 2;
        case 0x32: m_bus.write8(m_regs.hl--, m_regs.a); return 2;
        case 0x77: m_bus.write8(m_regs.hl, m_regs.a); return 2;
        case 0xEA: m_bus.write8(m_bus.read16(m_regs.pc), m_regs.a); m_regs.pc += 2; return 4;

        case 0xE0: m_bus.write8(static_cast<uint16_t>(0xFF00 | m_bus.read8(m_regs.pc++)), m_regs.a); return 3;
        case 0xF0: m_regs.a = m_bus.read8(static_cast<uint16_t>(0xFF00 | m_bus.read8(m_regs.pc++))); return 3;
        case 0xE2: m_bus.write8(static_cast<uint16_t>(0xFF00 | m_regs.c), m_regs.a); return 2;
        case 0xF2: m_regs.a = m_bus.read8(static_cast<uint16_t>(0xFF00 | m_regs.c)); return 2;

        case 0xF9: m_regs.sp = m_regs.hl; return 2;
        case 0xF8: { // LD HL, SP+e
            const int8_t e = static_cast<int8_t>(m_bus.read8(m_regs.pc++));
            setFlag(Flag_Z, false);
            setFlag(Flag_N, false);
            setFlag(Flag_H, ((m_regs.sp & 0xF) + (e & 0xF)) > 0xF);
            setFlag(Flag_C, ((m_regs.sp & 0xFF) + (e & 0xFF)) > 0xFF);
            m_regs.hl = static_cast<uint16_t>(m_regs.sp + e);
            return 3;
        }
        case 0xE8: { // ADD SP, e
            const int8_t e = static_cast<int8_t>(m_bus.read8(m_regs.pc++));
            setFlag(Flag_Z, false);
            setFlag(Flag_N, false);
            setFlag(Flag_H, ((m_regs.sp & 0xF) + (e & 0xF)) > 0xF);
            setFlag(Flag_C, ((m_regs.sp & 0xFF) + (e & 0xFF)) > 0xFF);
            m_regs.sp = static_cast<uint16_t>(m_regs.sp + e);
            return 4;
        }

        case 0x04: m_regs.b = aluInc(m_regs.b); return 1;
        case 0x0C: m_regs.c = aluInc(m_regs.c); return 1;
        case 0x14: m_regs.d = aluInc(m_regs.d); return 1;
        case 0x1C: m_regs.e = aluInc(m_regs.e); return 1;
        case 0x24: m_regs.h = aluInc(m_regs.h); return 1;
        case 0x2C: m_regs.l = aluInc(m_regs.l); return 1;
        case 0x3C: m_regs.a = aluInc(m_regs.a); return 1;
        case 0x34: m_bus.write8(m_regs.hl, aluInc(m_bus.read8(m_regs.hl))); return 3;
        case 0x05: m_regs.b = aluDec(m_regs.b); return 1;
        case 0x0D: m_regs.c = aluDec(m_regs.c); return 1;
        case 0x15: m_regs.d = aluDec(m_regs.d); return 1;
        case 0x1D: m_regs.e = aluDec(m_regs.e); return 1;
        case 0x25: m_regs.h = aluDec(m_regs.h); return 1;
        case 0x2D: m_regs.l = aluDec(m_regs.l); return 1;
        case 0x3D: m_regs.a = aluDec(m_regs.a); return 1;
        case 0x35: m_bus.write8(m_regs.hl, aluDec(m_bus.read8(m_regs.hl))); return 3;

        case 0x03: m_regs.bc++; return 2;
        case 0x13: m_regs.de++; return 2;
        case 0x23: m_regs.hl++; return 2;
        case 0x33: m_regs.sp++; return 2;
        case 0x0B: m_regs.bc--; return 2;
        case 0x1B: m_regs.de--; return 2;
        case 0x2B: m_regs.hl--; return 2;
        case 0x3B: m_regs.sp--; return 2;

        case 0x09: aluAddHl(m_regs.bc); return 2;
        case 0x19: aluAddHl(m_regs.de); return 2;
        case 0x29: aluAddHl(m_regs.hl); return 2;
        case 0x39: aluAddHl(m_regs.sp); return 2;

        case 0xC3: m_regs.pc = m_bus.read16(m_regs.pc); return 4;
        case 0xE9: m_regs.pc = m_regs.hl; return 1;

        case 0x18: { // JR e
            const int8_t e = static_cast<int8_t>(m_bus.read8(m_regs.pc++));
            m_regs.pc = static_cast<uint16_t>(m_regs.pc + e);
            return 3;
        }

        case 0xC9: m_regs.pc = pop16(); return 4; // RET
        case 0xD9: m_regs.pc = pop16(); m_ime = true; return 4; // RETI

        case 0xC7: push16(m_regs.pc); m_regs.pc = 0x00; return 4;
        case 0xCF: push16(m_regs.pc); m_regs.pc = 0x08; return 4;
        case 0xD7: push16(m_regs.pc); m_regs.pc = 0x10; return 4;
        case 0xDF: push16(m_regs.pc); m_regs.pc = 0x18; return 4;
        case 0xE7: push16(m_regs.pc); m_regs.pc = 0x20; return 4;
        case 0xEF: push16(m_regs.pc); m_regs.pc = 0x28; return 4;
        case 0xF7: push16(m_regs.pc); m_regs.pc = 0x30; return 4;
        case 0xFF: push16(m_regs.pc); m_regs.pc = 0x38; return 4;

        case 0xC1: m_regs.bc = pop16(); return 3;
        case 0xD1: m_regs.de = pop16(); return 3;
        case 0xE1: m_regs.hl = pop16(); return 3;
        case 0xF1: m_regs.af = static_cast<uint16_t>(pop16() & 0xFFF0); return 3;
        case 0xC5: push16(m_regs.bc); return 4;
        case 0xD5: push16(m_regs.de); return 4;
        case 0xE5: push16(m_regs.hl); return 4;
        case 0xF5: push16(m_regs.af); return 4;

        case 0xC6: aluAdd(m_bus.read8(m_regs.pc++), false); return 2;
        case 0xCE: aluAdd(m_bus.read8(m_regs.pc++), true); return 2;
        case 0xD6: aluSub(m_bus.read8(m_regs.pc++), false); return 2;
        case 0xDE: aluSub(m_bus.read8(m_regs.pc++), true); return 2;
        case 0xE6: aluAnd(m_bus.read8(m_regs.pc++)); return 2;
        case 0xEE: aluXor(m_bus.read8(m_regs.pc++)); return 2;
        case 0xF6: aluOr(m_bus.read8(m_regs.pc++)); return 2;
        case 0xFE: aluCp(m_bus.read8(m_regs.pc++)); return 2;

        case 0x27: aluDaa(); return 1;
        case 0x2F: m_regs.a = static_cast<uint8_t>(~m_regs.a); setFlag(Flag_N, true); setFlag(Flag_H, true); return 1;
        case 0x37: setFlag(Flag_N, false); setFlag(Flag_H, false); setFlag(Flag_C, true); return 1;
        case 0x07: { // RLCA
            const bool c = (m_regs.a & 0x80) != 0;
            m_regs.a = static_cast<uint8_t>((m_regs.a << 1) | (c ? 1 : 0));
            setFlag(Flag_Z, false); setFlag(Flag_N, false); setFlag(Flag_H, false); setFlag(Flag_C, c);
            return 1;
        }
        case 0x0F: { // RRCA
            const bool c = (m_regs.a & 0x01) != 0;
            m_regs.a = static_cast<uint8_t>((m_regs.a >> 1) | (c ? 0x80 : 0));
            setFlag(Flag_Z, false); setFlag(Flag_N, false); setFlag(Flag_H, false); setFlag(Flag_C, c);
            return 1;
        }
        case 0x17: { // RLA
            const bool c = (m_regs.a & 0x80) != 0;
            m_regs.a = static_cast<uint8_t>((m_regs.a << 1) | (getFlag(Flag_C) ? 1 : 0));
            setFlag(Flag_Z, false); setFlag(Flag_N, false); setFlag(Flag_H, false); setFlag(Flag_C, c);
            return 1;
        }
        case 0x1F: { // RRA
            const bool c = (m_regs.a & 0x01) != 0;
            m_regs.a = static_cast<uint8_t>((m_regs.a >> 1) | (getFlag(Flag_C) ? 0x80 : 0));
            setFlag(Flag_Z, false); setFlag(Flag_N, false); setFlag(Flag_H, false); setFlag(Flag_C, c);
            return 1;
        }
        case 0x3F: { // CCF
            const bool c = !getFlag(Flag_C);
            setFlag(Flag_N, false); setFlag(Flag_H, false); setFlag(Flag_C, c);
            return 1;
        }

        default: break;
    }

    // Conditional JP: C2/Ca/D2/DA.
    if (opcode == 0xC2 || opcode == 0xCA || opcode == 0xD2 || opcode == 0xDA) {
        const uint16_t addr = m_bus.read16(m_regs.pc);
        m_regs.pc += 2;
        bool take = false;
        if (opcode == 0xC2) { take = !getFlag(Flag_Z); }
        else if (opcode == 0xCA) { take = getFlag(Flag_Z); }
        else if (opcode == 0xD2) { take = !getFlag(Flag_C); }
        else { take = getFlag(Flag_C); }
        if (take) {
            m_regs.pc = addr;
            return 4;
        }
        return 3;
    }

    // Conditional JR: 20/28/30/38.
    if (opcode == 0x20 || opcode == 0x28 || opcode == 0x30 || opcode == 0x38) {
        const int8_t e = static_cast<int8_t>(m_bus.read8(m_regs.pc++));
        bool take = false;
        if (opcode == 0x20) { take = !getFlag(Flag_Z); }
        else if (opcode == 0x28) { take = getFlag(Flag_Z); }
        else if (opcode == 0x30) { take = !getFlag(Flag_C); }
        else { take = getFlag(Flag_C); }
        if (take) {
            m_regs.pc = static_cast<uint16_t>(m_regs.pc + e);
            return 3;
        }
        return 2;
    }

    // Conditional CALL: C4/CC/D4/DC; unconditional CD.
    if (opcode == 0xCD || opcode == 0xC4 || opcode == 0xCC || opcode == 0xD4 || opcode == 0xDC) {
        const uint16_t addr = m_bus.read16(m_regs.pc);
        m_regs.pc += 2;
        bool take = (opcode == 0xCD);
        if (opcode == 0xC4) { take = !getFlag(Flag_Z); }
        else if (opcode == 0xCC) { take = getFlag(Flag_Z); }
        else if (opcode == 0xD4) { take = !getFlag(Flag_C); }
        else if (opcode == 0xDC) { take = getFlag(Flag_C); }
        if (take) {
            push16(m_regs.pc);
            m_regs.pc = addr;
            return 6;
        }
        return 3;
    }

    // Conditional RET: C0/C8/D0/D8.
    if (opcode == 0xC0 || opcode == 0xC8 || opcode == 0xD0 || opcode == 0xD8) {
        bool take = false;
        if (opcode == 0xC0) { take = !getFlag(Flag_Z); }
        else if (opcode == 0xC8) { take = getFlag(Flag_Z); }
        else if (opcode == 0xD0) { take = !getFlag(Flag_C); }
        else { take = getFlag(Flag_C); }
        if (take) {
            m_regs.pc = pop16();
            return 5;
        }
        return 2;
    }

    // Generic LD r, r' block (0x40-0x7F, HALT excluded above).
    if (opcode >= 0x40 && opcode <= 0x7F) {
        const uint8_t dst = (opcode >> 3) & 0x07;
        const uint8_t src = opcode & 0x07;
        uint8_t value = 0;
        switch (src) {
            case 0: value = m_regs.b; break;
            case 1: value = m_regs.c; break;
            case 2: value = m_regs.d; break;
            case 3: value = m_regs.e; break;
            case 4: value = m_regs.h; break;
            case 5: value = m_regs.l; break;
            case 6: value = m_bus.read8(m_regs.hl); break;
            default: value = m_regs.a; break;
        }
        const bool hlAccess = (dst == 6) || (src == 6);
        switch (dst) {
            case 0: m_regs.b = value; break;
            case 1: m_regs.c = value; break;
            case 2: m_regs.d = value; break;
            case 3: m_regs.e = value; break;
            case 4: m_regs.h = value; break;
            case 5: m_regs.l = value; break;
            case 6: m_bus.write8(m_regs.hl, value); break;
            default: m_regs.a = value; break;
        }
        return hlAccess ? 2 : 1;
    }

    // ALU r block (0x80-0xBF):
    // group 0 ADD, 1 ADC, 2 SUB, 3 SBC, 4 AND, 5 XOR, 6 OR, 7 CP.
    if (opcode >= 0x80 && opcode <= 0xBF) {
        const uint8_t src = opcode & 0x07;
        const uint8_t group = (opcode >> 3) & 0x07;
        uint8_t value = 0;
        switch (src) {
            case 0: value = m_regs.b; break;
            case 1: value = m_regs.c; break;
            case 2: value = m_regs.d; break;
            case 3: value = m_regs.e; break;
            case 4: value = m_regs.h; break;
            case 5: value = m_regs.l; break;
            case 6: value = m_bus.read8(m_regs.hl); break;
            default: value = m_regs.a; break;
        }
        switch (group) {
            case 0: aluAdd(value, false); break;
            case 1: aluAdd(value, true); break;
            case 2: aluSub(value, false); break;
            case 3: aluSub(value, true); break;
            case 4: aluAnd(value); break;
            case 5: aluXor(value); break;
            case 6: aluOr(value); break;
            default: aluCp(value); break;
        }
        return (src == 6) ? 2 : 1;
    }

    return 1; // Unimplemented opcode fallback: safe NOP
}

} // namespace smile::systems::gameboy
