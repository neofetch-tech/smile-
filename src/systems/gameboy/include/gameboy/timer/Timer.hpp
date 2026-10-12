#pragma once

#include <cstdint>
#include "gameboy/StateCodec.hpp"

namespace smile::systems::gameboy {

// Game Boy timer: DIV (0xFF04), TIMA (0xFF05), TMA (0xFF06), TAC (0xFF07).
// All timing is driven in T-cycles (4.194304 MHz master clock).
class Timer {
public:
    Timer() = default;
    ~Timer() = default;

    void reset();
    void step(uint32_t tCycles, bool& outTimerInterrupt);

    [[nodiscard]] uint8_t readRegister(uint16_t address) const;
    void writeRegister(uint16_t address, uint8_t value);

    void saveState(StateWriter& out) const;
    bool loadState(StateReader& in);

private:
    uint16_t m_divCounter{0}; // Internal 16-bit counter, upper byte is DIV
    uint8_t m_tima{0};
    uint8_t m_tma{0};
    uint8_t m_tac{0};

    int m_timaCounter{0};
};

} // namespace smile::systems::gameboy
