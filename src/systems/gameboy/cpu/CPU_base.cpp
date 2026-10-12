#include "gameboy/cpu/CPU.hpp"
#include "gameboy/bus/MemoryBus.hpp"

namespace smile::systems::gameboy {

CPU::CPU(MemoryBus& bus) : m_bus(bus) {}

void CPU::reset() {
    m_regs = Registers{};
    m_regs.pc = 0x0100;
    m_regs.sp = 0xFFFE;
    m_regs.af = 0x01B0; // Post-boot ROM state (DMG)
    m_regs.bc = 0x0013;
    m_regs.de = 0x00D8;
    m_regs.hl = 0x014D;
    m_ime = false;
    m_halted = false;
    m_eiDelay = 0;
}

void CPU::setFlag(Flags flag, bool set) {
    if (set) {
        m_regs.f |= static_cast<uint8_t>(flag);
    } else {
        m_regs.f &= ~static_cast<uint8_t>(flag);
    }
    m_regs.f &= 0xF0; // Low nibble of F is always zero
}

bool CPU::getFlag(Flags flag) const {
    return (m_regs.f & static_cast<uint8_t>(flag)) != 0;
}

void CPU::push16(uint16_t value) {
    m_regs.sp -= 2;
    m_bus.write8(m_regs.sp, static_cast<uint8_t>(value & 0xFF));
    m_bus.write8(static_cast<uint16_t>(m_regs.sp + 1), static_cast<uint8_t>(value >> 8));
}

uint16_t CPU::pop16() {
    const uint16_t lo = m_bus.read8(m_regs.sp);
    const uint16_t hi = m_bus.read8(static_cast<uint16_t>(m_regs.sp + 1));
    m_regs.sp += 2;
    return static_cast<uint16_t>((hi << 8) | lo);
}

uint8_t CPU::serviceInterrupts() {
    const uint8_t pending = static_cast<uint8_t>(m_bus.getIE() & m_bus.getIF() & 0x1F);
    if (pending == 0) {
        return 0;
    }
    // A pending interrupt wakes the CPU from HALT even when IME == 0.
    m_halted = false;
    if (!m_ime) {
        return 0;
    }
    // Priority: VBlank(0) -> STAT(1) -> Timer(2) -> Serial(3) -> Joypad(4).
    for (uint8_t bit = 0; bit < 5; ++bit) {
        if (pending & (1 << bit)) {
            m_ime = false;
            m_bus.setIF(static_cast<uint8_t>(m_bus.getIF() & ~(1 << bit)));
            push16(m_regs.pc);
            m_regs.pc = static_cast<uint16_t>(0x40 + bit * 8);
            return 5; // Service costs 5 M-cycles
        }
    }
    return 0;
}

void CPU::aluAdd(uint8_t value, bool withCarry) {
    const uint8_t carry = (withCarry && getFlag(Flag_C)) ? 1 : 0;
    const int result = m_regs.a + value + carry;
    setFlag(Flag_Z, (result & 0xFF) == 0);
    setFlag(Flag_N, false);
    setFlag(Flag_H, ((m_regs.a & 0xF) + (value & 0xF) + carry) > 0xF);
    setFlag(Flag_C, result > 0xFF);
    m_regs.a = static_cast<uint8_t>(result);
}

void CPU::aluSub(uint8_t value, bool withCarry) {
    const uint8_t carry = (withCarry && getFlag(Flag_C)) ? 1 : 0;
    const int result = m_regs.a - value - carry;
    setFlag(Flag_Z, (result & 0xFF) == 0);
    setFlag(Flag_N, true);
    setFlag(Flag_H, (m_regs.a & 0xF) < ((value & 0xF) + carry));
    setFlag(Flag_C, result < 0);
    m_regs.a = static_cast<uint8_t>(result);
}

void CPU::aluAnd(uint8_t value) {
    m_regs.a &= value;
    setFlag(Flag_Z, m_regs.a == 0);
    setFlag(Flag_N, false);
    setFlag(Flag_H, true);
    setFlag(Flag_C, false);
}

void CPU::aluXor(uint8_t value) {
    m_regs.a ^= value;
    setFlag(Flag_Z, m_regs.a == 0);
    setFlag(Flag_N, false);
    setFlag(Flag_H, false);
    setFlag(Flag_C, false);
}

void CPU::aluOr(uint8_t value) {
    m_regs.a |= value;
    setFlag(Flag_Z, m_regs.a == 0);
    setFlag(Flag_N, false);
    setFlag(Flag_H, false);
    setFlag(Flag_C, false);
}

void CPU::aluCp(uint8_t value) {
    const int result = m_regs.a - value;
    setFlag(Flag_Z, (result & 0xFF) == 0);
    setFlag(Flag_N, true);
    setFlag(Flag_H, (m_regs.a & 0xF) < (value & 0xF));
    setFlag(Flag_C, result < 0);
}

uint8_t CPU::aluInc(uint8_t value) {
    const uint8_t result = static_cast<uint8_t>(value + 1);
    setFlag(Flag_Z, result == 0);
    setFlag(Flag_N, false);
    setFlag(Flag_H, (value & 0xF) == 0xF);
    return result;
}

uint8_t CPU::aluDec(uint8_t value) {
    const uint8_t result = static_cast<uint8_t>(value - 1);
    setFlag(Flag_Z, result == 0);
    setFlag(Flag_N, true);
    setFlag(Flag_H, (value & 0xF) == 0x00);
    return result;
}

void CPU::aluAddHl(uint16_t value) {
    const int result = m_regs.hl + value;
    setFlag(Flag_N, false);
    setFlag(Flag_H, ((m_regs.hl & 0xFFF) + (value & 0xFFF)) > 0xFFF);
    setFlag(Flag_C, result > 0xFFFF);
    m_regs.hl = static_cast<uint16_t>(result);
}

void CPU::aluDaa() {
    // Decimal adjust A for BCD arithmetic.
    uint8_t adjust = 0;
    bool carry = getFlag(Flag_C);
    if (!getFlag(Flag_N)) {
        if (getFlag(Flag_H) || (m_regs.a & 0xF) > 9) {
            adjust |= 0x06;
        }
        if (getFlag(Flag_C) || m_regs.a > 0x99) {
            adjust |= 0x60;
            carry = true;
        }
        m_regs.a = static_cast<uint8_t>(m_regs.a + adjust);
    } else {
        if (getFlag(Flag_H)) {
            adjust |= 0x06;
        }
        if (getFlag(Flag_C)) {
            adjust |= 0x60;
        }
        m_regs.a = static_cast<uint8_t>(m_regs.a - adjust);
    }
    setFlag(Flag_Z, m_regs.a == 0);
    setFlag(Flag_H, false);
    setFlag(Flag_C, carry);
}

} // namespace smile::systems::gameboy
