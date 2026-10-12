#pragma once

#include <cstdint>

namespace smile::systems::nes {

class NesBus; // Concrete bus; the CPU talks to it directly (no virtuals inside).

// MOS 6502 (2A03, BCD disabled): fetch/decode/execute returns CPU cycles.
// Official 56 instructions; illegal opcodes fall back to a safe 2-cycle NOP
// so bring-up never hard-crashes (Contra uses official opcodes only).
class NesCPU {
public:
    explicit NesCPU(NesBus& bus);

    void reset();
    uint16_t step(); // Returns consumed CPU cycles (DMA stall can exceed 255)
    void requestNmi();

    struct Registers {
        uint8_t a{0};
        uint8_t x{0};
        uint8_t y{0};
        uint8_t sp{0xFD};
        uint8_t status{0x24};
        uint16_t pc{0};
    };

    [[nodiscard]] const Registers& regs() const noexcept { return m_regs; }

    static constexpr uint8_t F_C = 0x01;
    static constexpr uint8_t F_Z = 0x02;
    static constexpr uint8_t F_I = 0x04;
    static constexpr uint8_t F_D = 0x08;
    static constexpr uint8_t F_B = 0x10;
    static constexpr uint8_t F_U = 0x20;
    static constexpr uint8_t F_V = 0x40;
    static constexpr uint8_t F_N = 0x80;

private:
    enum class Mode { Imp, Acc, Imm, Zp, ZpX, ZpY, Abs, AbsX, AbsY, Ind, IndX, IndY };

    [[nodiscard]] uint16_t fetch16();
    [[nodiscard]] uint16_t fetchAddr(Mode mode, bool& crossed);
    [[nodiscard]] uint8_t readOp(Mode mode, bool& crossed);

    void setFlag(uint8_t mask, bool on);
    void setNZ(uint8_t value);
    uint8_t branch(bool condition);
    void push(uint8_t value);
    [[nodiscard]] uint8_t pull();

    void adc(uint8_t value);
    void sbc(uint8_t value);
    void cmpReg(uint8_t reg, uint8_t value);
    uint8_t asl(uint8_t value);
    uint8_t lsr(uint8_t value);
    uint8_t rol(uint8_t value);
    uint8_t ror(uint8_t value);
    void bitOp(uint8_t value);
    void serviceNmi();

    NesBus& m_bus;
    Registers m_regs{};
    bool m_nmi{false}; // Edge-triggered NMI line
};

} // namespace smile::systems::nes
