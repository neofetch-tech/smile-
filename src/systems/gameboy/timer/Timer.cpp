#include "gameboy/timer/Timer.hpp"

namespace smile::systems::gameboy {

void Timer::reset() {
    m_divCounter = 0;
    m_tima = 0;
    m_tma = 0;
    m_tac = 0;
    m_timaCounter = 0;
}

uint8_t Timer::readRegister(uint16_t address) const {
    switch (address) {
        case 0xFF04: return static_cast<uint8_t>(m_divCounter >> 8);
        case 0xFF05: return m_tima;
        case 0xFF06: return m_tma;
        case 0xFF07: return m_tac | 0xF8; // Unused upper bits read as 1
        default: return 0xFF;
    }
}

void Timer::writeRegister(uint16_t address, uint8_t value) {
    switch (address) {
        case 0xFF04: m_divCounter = 0; break; // Any write resets DIV
        case 0xFF05: m_tima = value; break;
        case 0xFF06: m_tma = value; break;
        case 0xFF07: m_tac = value & 0x07; break;
    }
}

void Timer::step(uint32_t tCycles, bool& outTimerInterrupt) {
    outTimerInterrupt = false;

    // DIV runs at 16384 Hz: increments every 256 T-cycles.
    m_divCounter += static_cast<uint16_t>(tCycles);

    // TIMA is clocked only when TAC bit 2 is set.
    if ((m_tac & 0x04) == 0) {
        return;
    }

    m_timaCounter += static_cast<int>(tCycles);

    // TAC clock select -> T-cycles per TIMA increment.
    int threshold = 1024;
    switch (m_tac & 0x03) {
        case 0x00: threshold = 1024; break; // 4096 Hz
        case 0x01: threshold = 16;   break; // 262144 Hz
        case 0x02: threshold = 64;   break; // 65536 Hz
        case 0x03: threshold = 256;  break; // 16384 Hz
    }

    while (m_timaCounter >= threshold) {
        m_timaCounter -= threshold;
        if (m_tima == 0xFF) {
            m_tima = m_tma; // Reload on overflow
            outTimerInterrupt = true;
        } else {
            m_tima++;
        }
    }
}

} // namespace smile::systems::gameboy
