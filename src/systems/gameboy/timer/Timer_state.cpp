#include "gameboy/timer/Timer.hpp"

namespace smile::systems::gameboy {

void Timer::saveState(StateWriter& out) const {
    out.putU16(m_divCounter);
    out.putU8(m_tima);
    out.putU8(m_tma);
    out.putU8(m_tac);
    out.putU32(static_cast<uint32_t>(m_timaCounter));
}

bool Timer::loadState(StateReader& in) {
    uint16_t div = 0;
    uint8_t tima = 0;
    uint8_t tma = 0;
    uint8_t tac = 0;
    uint32_t counter = 0;

    if (!in.readU16(div) || !in.readU8(tima) || !in.readU8(tma) ||
        !in.readU8(tac) || !in.readU32(counter)) {
        return false;
    }

    m_divCounter = div;
    m_tima = tima;
    m_tma = tma;
    m_tac = static_cast<uint8_t>(tac & 0x07);
    m_timaCounter = static_cast<int>(counter);
    return true;
}

} // namespace smile::systems::gameboy
