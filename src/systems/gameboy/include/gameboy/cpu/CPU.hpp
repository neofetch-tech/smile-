#pragma once

#include <cstdint>
#include "gameboy/StateCodec.hpp"

namespace smile::systems::gameboy {

class MemoryBus; // Forward declaration to avoid a hard CPU<->Bus dependency.

// Sharp LR35902 CPU: fetch/decode/execute returns consumed M-cycles.
// T-cycles = M-cycles * 4. Interrupt dispatch (VBlank/STAT/Timer/Serial/Joypad)
// follows the standard IME + IE/IF handshake with 5 M-cycle service cost.
class CPU {
public:
    explicit CPU(MemoryBus& bus);
    ~CPU() = default;

    void reset();

    // Execute one instruction (or one HALT idle tick). Returns M-cycles.
    uint8_t step();

    struct Registers {
        union {
            struct { uint8_t f, a; };
            uint16_t af;
        };
        union {
            struct { uint8_t c, b; };
            uint16_t bc;
        };
        union {
            struct { uint8_t e, d; };
            uint16_t de;
        };
        union {
            struct { uint8_t l, h; };
            uint16_t hl;
        };
        uint16_t sp{0xFFFE};
        uint16_t pc{0x0100}; // Entry point after boot ROM
    };

    [[nodiscard]] const Registers& getRegisters() const noexcept { return m_regs; }
    [[nodiscard]] bool isHalted() const noexcept { return m_halted; }

    // Save-state support (fixed layout, little-endian).
    void saveState(StateWriter& out) const;
    bool loadState(StateReader& in);

private:
    enum Flags : uint8_t {
        Flag_Z = 1 << 7,
        Flag_N = 1 << 6,
        Flag_H = 1 << 5,
        Flag_C = 1 << 4
    };

    void setFlag(Flags flag, bool set);
    [[nodiscard]] bool getFlag(Flags flag) const;

    // Returns M-cycles spent servicing (5) or 0 when no interrupt fires.
    uint8_t serviceInterrupts();

    void push16(uint16_t value);
    [[nodiscard]] uint16_t pop16();

    // ALU helpers (set flags per LR35902 semantics).
    void aluAdd(uint8_t value, bool withCarry);
    void aluSub(uint8_t value, bool withCarry);
    void aluAnd(uint8_t value);
    void aluXor(uint8_t value);
    void aluOr(uint8_t value);
    void aluCp(uint8_t value);
    uint8_t aluInc(uint8_t value);
    uint8_t aluDec(uint8_t value);
    void aluAddHl(uint16_t value);
    void aluDaa();

    uint8_t executeCb(uint8_t cbOpcode);

    MemoryBus& m_bus;
    Registers m_regs{};

    bool m_ime{false};      // Interrupt master enable
    bool m_halted{false};   // HALT state
    uint8_t m_eiDelay{0};   // EI takes effect after the following instruction
};

} // namespace smile::systems::gameboy
