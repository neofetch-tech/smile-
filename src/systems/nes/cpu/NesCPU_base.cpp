#include "nes/cpu/NesCPU.hpp"
#include "nes/bus/NesBus.hpp"

namespace smile::systems::nes {

NesCPU::NesCPU(NesBus& bus) : m_bus(bus) {}

void NesCPU::reset() {
    m_regs.a = 0;
    m_regs.x = 0;
    m_regs.y = 0;
    m_regs.sp = 0xFD;
    m_regs.status = F_U | F_I;
    m_regs.pc = static_cast<uint16_t>(m_bus.read8(0xFFFC) |
                                      (m_bus.read8(0xFFFD) << 8));
    m_nmi = false;
}

void NesCPU::requestNmi() {
    m_nmi = true;
}

void NesCPU::setFlag(uint8_t mask, bool on) {
    if (on) {
        m_regs.status |= mask;
    } else {
        m_regs.status &= ~mask;
    }
    m_regs.status |= F_U; // Unused bit always reads 1
}

void NesCPU::setNZ(uint8_t value) {
    setFlag(F_Z, value == 0);
    setFlag(F_N, (value & 0x80) != 0);
}

void NesCPU::push(uint8_t value) {
    m_bus.write8(static_cast<uint16_t>(0x100 | m_regs.sp), value);
    m_regs.sp--;
}

uint8_t NesCPU::pull() {
    m_regs.sp++;
    return m_bus.read8(static_cast<uint16_t>(0x100 | m_regs.sp));
}

uint16_t NesCPU::fetch16() {
    const uint8_t lo = m_bus.read8(m_regs.pc++);
    const uint8_t hi = m_bus.read8(m_regs.pc++);
    return static_cast<uint16_t>(lo | (hi << 8));
}

uint16_t NesCPU::fetchAddr(Mode mode, bool& crossed) {
    crossed = false;
    switch (mode) {
        case Mode::Zp:
            return m_bus.read8(m_regs.pc++);
        case Mode::ZpX:
            return static_cast<uint16_t>((m_bus.read8(m_regs.pc++) + m_regs.x) & 0xFF);
        case Mode::ZpY:
            return static_cast<uint16_t>((m_bus.read8(m_regs.pc++) + m_regs.y) & 0xFF);
        case Mode::Abs:
            return fetch16();
        case Mode::AbsX: {
            const uint16_t base = fetch16();
            const uint16_t addr = static_cast<uint16_t>(base + m_regs.x);
            crossed = (base & 0xFF00) != (addr & 0xFF00);
            return addr;
        }
        case Mode::AbsY: {
            const uint16_t base = fetch16();
            const uint16_t addr = static_cast<uint16_t>(base + m_regs.y);
            crossed = (base & 0xFF00) != (addr & 0xFF00);
            return addr;
        }
        case Mode::Ind: { // JMP only, with the page-wrap hardware bug
            const uint16_t ptr = fetch16();
            const uint8_t lo = m_bus.read8(ptr);
            const uint8_t hi = m_bus.read8(
                static_cast<uint16_t>((ptr & 0xFF00) | ((ptr + 1) & 0xFF)));
            return static_cast<uint16_t>(lo | (hi << 8));
        }
        case Mode::IndX: {
            const uint8_t zp = static_cast<uint8_t>(m_bus.read8(m_regs.pc++) + m_regs.x);
            const uint8_t lo = m_bus.read8(zp);
            const uint8_t hi = m_bus.read8(static_cast<uint8_t>(zp + 1));
            return static_cast<uint16_t>(lo | (hi << 8));
        }
        case Mode::IndY: {
            const uint8_t zp = m_bus.read8(m_regs.pc++);
            const uint16_t base = static_cast<uint16_t>(
                m_bus.read8(zp) | (m_bus.read8(static_cast<uint8_t>(zp + 1)) << 8));
            const uint16_t addr = static_cast<uint16_t>(base + m_regs.y);
            crossed = (base & 0xFF00) != (addr & 0xFF00);
            return addr;
        }
        default:
            return 0;
    }
}

uint8_t NesCPU::readOp(Mode mode, bool& crossed) {
    if (mode == Mode::Imm) {
        return m_bus.read8(m_regs.pc++);
    }
    return m_bus.read8(fetchAddr(mode, crossed));
}

uint8_t NesCPU::branch(bool condition) {
    const int8_t offset = static_cast<int8_t>(m_bus.read8(m_regs.pc++));
    if (!condition) {
        return 2;
    }
    const uint16_t old = m_regs.pc;
    m_regs.pc = static_cast<uint16_t>(m_regs.pc + offset);
    // +1 when taken, +1 more on page cross.
    return ((old & 0xFF00) != (m_regs.pc & 0xFF00)) ? 4 : 3;
}

void NesCPU::adc(uint8_t value) {
    // NOTE: the 2A03 has no BCD mode; binary arithmetic always.
    const uint16_t sum = static_cast<uint16_t>(m_regs.a + value +
                                               ((m_regs.status & F_C) ? 1 : 0));
    setFlag(F_C, sum > 0xFF);
    setFlag(F_V, ((~(m_regs.a ^ value) & (m_regs.a ^ sum)) & 0x80) != 0);
    m_regs.a = static_cast<uint8_t>(sum);
    setNZ(m_regs.a);
}

void NesCPU::sbc(uint8_t value) {
    adc(static_cast<uint8_t>(value ^ 0xFF));
}

void NesCPU::cmpReg(uint8_t reg, uint8_t value) {
    const uint16_t diff = static_cast<uint16_t>(reg - value);
    setFlag(F_C, reg >= value);
    setNZ(static_cast<uint8_t>(diff));
}

uint8_t NesCPU::asl(uint8_t value) {
    setFlag(F_C, (value & 0x80) != 0);
    return static_cast<uint8_t>(value << 1);
}

uint8_t NesCPU::lsr(uint8_t value) {
    setFlag(F_C, (value & 0x01) != 0);
    return static_cast<uint8_t>(value >> 1);
}

uint8_t NesCPU::rol(uint8_t value) {
    const bool carry = (value & 0x80) != 0;
    const uint8_t result =
        static_cast<uint8_t>((value << 1) | ((m_regs.status & F_C) ? 1 : 0));
    setFlag(F_C, carry);
    return result;
}

uint8_t NesCPU::ror(uint8_t value) {
    const bool carry = (value & 0x01) != 0;
    const uint8_t result =
        static_cast<uint8_t>((value >> 1) | ((m_regs.status & F_C) ? 0x80 : 0));
    setFlag(F_C, carry);
    return result;
}

void NesCPU::bitOp(uint8_t value) {
    setFlag(F_Z, (m_regs.a & value) == 0);
    setFlag(F_N, (value & 0x80) != 0);
    setFlag(F_V, (value & 0x40) != 0);
}

void NesCPU::serviceNmi() {
    push(static_cast<uint8_t>(m_regs.pc >> 8));
    push(static_cast<uint8_t>(m_regs.pc & 0xFF));
    push(static_cast<uint8_t>((m_regs.status & ~F_B) | F_U));
    setFlag(F_I, true);
    m_regs.pc = static_cast<uint16_t>(m_bus.read8(0xFFFA) | (m_bus.read8(0xFFFB) << 8));
}

} // namespace smile::systems::nes
