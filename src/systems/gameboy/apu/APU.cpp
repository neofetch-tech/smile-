#include "gameboy/apu/APU.hpp"

#include <bit>

namespace smile::systems::gameboy {

namespace {
// Pulse duty patterns: 12.5%, 25%, 50%, 75%.
constexpr uint8_t kDutyPatterns[4][8] = {
    {0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 1, 0},
};
constexpr uint32_t kSeqPeriod = 8192; // 512 Hz frame sequencer in T-cycles
} // namespace

void APU::reset() {
    m_ch1 = PulseChannel{};
    m_ch2 = PulseChannel{};
    m_ch3 = WaveChannel{};
    m_ch4 = NoiseChannel{};
    m_volLeft = 7;
    m_volRight = 7;
    m_pan = 0xF3; // Post-boot panning
    m_power = true;
    m_seqStep = 0;
    m_seqCounter = 0;
    m_sampleFrac = 0;
    m_hpL = 0.0f;
    m_hpR = 0.0f;
}

void APU::step(uint32_t tCycles) {
    if (!m_power) {
        return;
    }
    // 512 Hz frame sequencer.
    m_seqCounter += tCycles;
    while (m_seqCounter >= kSeqPeriod) {
        m_seqCounter -= kSeqPeriod;
        tickSequencer();
    }
    // Per-cycle frequency timers.
    for (uint32_t i = 0; i < tCycles; ++i) {
        tickTimers();
    }
    // Exact sample-rate conversion: 4194304 Hz -> 44100 Hz.
    m_sampleFrac += tCycles * kSampleRate;
    while (m_sampleFrac >= kClockHz) {
        m_sampleFrac -= kClockHz;
        pushSample();
    }
}

void APU::tickTimers() {
    if (m_ch1.enabled) {
        if (--m_ch1.freqTimer == 0) {
            m_ch1.freqTimer = static_cast<uint16_t>((2048 - m_ch1.frequency) * 4);
            m_ch1.dutyPos = (m_ch1.dutyPos + 1) & 7;
        }
    }
    if (m_ch2.enabled) {
        if (--m_ch2.freqTimer == 0) {
            m_ch2.freqTimer = static_cast<uint16_t>((2048 - m_ch2.frequency) * 4);
            m_ch2.dutyPos = (m_ch2.dutyPos + 1) & 7;
        }
    }
    if (m_ch3.enabled) {
        if (--m_ch3.freqTimer == 0) {
            m_ch3.freqTimer = static_cast<uint16_t>((2048 - m_ch3.frequency) * 2);
            m_ch3.position = (m_ch3.position + 1) & 31;
        }
    }
    if (m_ch4.enabled) {
        if (--m_ch4.timer == 0) {
            const uint16_t base = (m_ch4.divisor == 0) ? 8 : (m_ch4.divisor * 16);
            const uint32_t period = static_cast<uint32_t>(base) << m_ch4.clockShift;
            // Clamp: extreme shifts would overflow 16 bits and wrap the timer
            // to a tiny period (hypersonic screech instead of rumble).
            m_ch4.timer = (period > 0xFFFF) ? 0xFFFF : static_cast<uint16_t>(period);
            const uint16_t xorBit = (m_ch4.lfsr ^ (m_ch4.lfsr >> 1)) & 1;
            m_ch4.lfsr = (m_ch4.lfsr >> 1) | (xorBit << 14);
            if (m_ch4.narrowMode) {
                m_ch4.lfsr = (m_ch4.lfsr & ~0x40) | (xorBit << 6);
            }
        }
    }
}

void APU::clockLength(uint8_t& timer, bool enabled, bool& channelOn) {
    if (enabled && timer > 0) {
        if (--timer == 0) {
            channelOn = false;
        }
    }
}

void APU::clockEnvelope(uint8_t& volume, uint8_t& timer, uint8_t pace, bool dir) {
    if (pace == 0) {
        return;
    }
    if (--timer == 0) {
        timer = pace;
        if (dir && volume < 15) {
            volume++;
        } else if (!dir && volume > 0) {
            volume--;
        }
    }
}

void APU::clockSweep() {
    PulseChannel& ch = m_ch1;
    if (!ch.sweepOn || ch.sweepPace == 0) {
        return;
    }
    if (--ch.sweepTimer == 0) {
        ch.sweepTimer = ch.sweepPace;
        const uint16_t delta = ch.sweepSlope == 0 ? 0 : (ch.shadowFreq >> ch.sweepSlope);
        const uint16_t next = ch.sweepDir ? (ch.shadowFreq - delta) : (ch.shadowFreq + delta);
        if (next > 2047) {
            ch.enabled = false;
        } else if (ch.sweepSlope > 0) {
            ch.shadowFreq = next;
            ch.frequency = next;
        }
    }
}

void APU::tickSequencer() {
    m_seqStep = (m_seqStep + 1) & 7;
    if ((m_seqStep & 1) == 0) { // 256 Hz: length counters
        clockLength(m_ch1.lengthTimer, m_ch1.lengthEnabled, m_ch1.enabled);
        clockLength(m_ch2.lengthTimer, m_ch2.lengthEnabled, m_ch2.enabled);
        clockLength(m_ch3.lengthTimer, m_ch3.lengthEnabled, m_ch3.enabled);
        clockLength(m_ch4.lengthTimer, m_ch4.lengthEnabled, m_ch4.enabled);
    }
    if (m_seqStep == 7) { // 64 Hz: volume envelopes
        clockEnvelope(m_ch1.volume, m_ch1.envTimer, m_ch1.envPace, m_ch1.envDir);
        clockEnvelope(m_ch2.volume, m_ch2.envTimer, m_ch2.envPace, m_ch2.envDir);
        clockEnvelope(m_ch4.volume, m_ch4.envTimer, m_ch4.envPace, m_ch4.envDir);
    }
    if (m_seqStep == 2 || m_seqStep == 6) { // 128 Hz: frequency sweep
        clockSweep();
    }
}

uint8_t APU::pulseOutput(const PulseChannel& ch) const {
    if (!ch.enabled || !ch.dac) {
        return 0;
    }
    return kDutyPatterns[ch.duty & 3][ch.dutyPos] ? ch.volume : 0;
}

uint8_t APU::waveOutput() const {
    if (!m_ch3.enabled || !m_ch3.dac || m_ch3.volumeCode == 0) {
        return 0;
    }
    const uint8_t byte = m_ch3.ram[m_ch3.position / 2];
    uint8_t sample = (m_ch3.position % 2 == 0) ? (byte >> 4) : (byte & 0x0F);
    if (m_ch3.volumeCode == 2) {
        sample >>= 1;
    } else if (m_ch3.volumeCode == 3) {
        sample >>= 2;
    }
    return sample;
}

uint8_t APU::noiseOutput() const {
    if (!m_ch4.enabled || !m_ch4.dac) {
        return 0;
    }
    return ((~m_ch4.lfsr & 1) != 0) ? m_ch4.volume : 0;
}

void APU::pushSample() {
    const uint8_t o1 = pulseOutput(m_ch1);
    const uint8_t o2 = pulseOutput(m_ch2);
    const uint8_t o3 = waveOutput();
    const uint8_t o4 = noiseOutput();

    // NR51 panning: low nibble = right (SO1), high nibble = left (SO2).
    float sumLeft = 0.0f;
    float sumRight = 0.0f;
    if (m_pan & 0x10) { sumLeft += o1; }
    if (m_pan & 0x20) { sumLeft += o2; }
    if (m_pan & 0x40) { sumLeft += o3; }
    if (m_pan & 0x80) { sumLeft += o4; }
    if (m_pan & 0x01) { sumRight += o1; }
    if (m_pan & 0x02) { sumRight += o2; }
    if (m_pan & 0x04) { sumRight += o3; }
    if (m_pan & 0x08) { sumRight += o4; }

    const float scaleLeft = ((m_volLeft & 7) + 1) / 8.0f;
    const float scaleRight = ((m_volRight & 7) + 1) / 8.0f;
    // Raw mix is unipolar (0..60). The high-pass strips DC so that
    // silence outputs 0.0 instead of a constant -1.0 (audible hum/clicks).
    const float rawLeft = (sumLeft / 60.0f) * 2.0f - 1.0f;
    const float rawRight = (sumRight / 60.0f) * 2.0f - 1.0f;
    m_hpL += 0.002f * (rawLeft - m_hpL);
    m_hpR += 0.002f * (rawRight - m_hpR);
    const float left = (rawLeft - m_hpL) * scaleLeft * 0.6f;
    const float right = (rawRight - m_hpR) * scaleRight * 0.6f;
    m_output.write(left);
    m_output.write(right);
}

void APU::triggerPulse(PulseChannel& ch, bool withSweep) {
    ch.enabled = ch.dac;
    if (ch.lengthTimer == 0) {
        ch.lengthTimer = 64 - (ch.length & 63);
    }
    ch.freqTimer = static_cast<uint16_t>((2048 - ch.frequency) * 4);
    ch.envTimer = ch.envPace;
    ch.volume = ch.initVolume;
    if (withSweep) {
        ch.shadowFreq = ch.frequency;
        ch.sweepTimer = ch.sweepPace;
        ch.sweepOn = ch.sweepPace > 0;
        if (ch.sweepSlope > 0) { // Overflow check runs once at trigger
            const uint16_t delta = ch.shadowFreq >> ch.sweepSlope;
            const uint16_t next = ch.sweepDir ? (ch.shadowFreq - delta) : (ch.shadowFreq + delta);
            if (next > 2047) {
                ch.enabled = false;
            }
        }
    }
}

void APU::triggerWave() {
    m_ch3.enabled = m_ch3.dac;
    if (m_ch3.lengthTimer == 0) {
        m_ch3.lengthTimer = static_cast<uint8_t>(256 - m_ch3.length);
    }
    m_ch3.freqTimer = static_cast<uint16_t>((2048 - m_ch3.frequency) * 2);
    m_ch3.position = 0;
}

void APU::triggerNoise() {
    m_ch4.enabled = m_ch4.dac;
    if (m_ch4.lengthTimer == 0) {
        m_ch4.lengthTimer = 64 - (m_ch4.length & 63);
    }
    m_ch4.lfsr = 0x7FFF;
    m_ch4.envTimer = m_ch4.envPace;
    m_ch4.volume = m_ch4.initVolume;
    const uint16_t base = (m_ch4.divisor == 0) ? 8 : (m_ch4.divisor * 16);
    const uint32_t period = static_cast<uint32_t>(base) << m_ch4.clockShift;
    m_ch4.timer = (period > 0xFFFF) ? 0xFFFF : static_cast<uint16_t>(period);
}

uint8_t APU::readRegister(uint16_t address) const {
    switch (address) {
        case 0xFF10: return static_cast<uint8_t>(0x80 | (m_ch1.sweepPace << 4) |
                                                (m_ch1.sweepDir ? 0x08 : 0) | m_ch1.sweepSlope);
        case 0xFF11: return static_cast<uint8_t>((m_ch1.duty << 6) | 0x3F);
        case 0xFF12: return static_cast<uint8_t>((m_ch1.initVolume << 4) |
                                                (m_ch1.envDir ? 0x08 : 0) | m_ch1.envPace);
        case 0xFF13: return 0xFF;
        case 0xFF14: return static_cast<uint8_t>(0xBF | (m_ch1.lengthEnabled ? 0x40 : 0));
        case 0xFF16: return static_cast<uint8_t>((m_ch2.duty << 6) | 0x3F);
        case 0xFF17: return static_cast<uint8_t>((m_ch2.initVolume << 4) |
                                                (m_ch2.envDir ? 0x08 : 0) | m_ch2.envPace);
        case 0xFF18: return 0xFF;
        case 0xFF19: return static_cast<uint8_t>(0xBF | (m_ch2.lengthEnabled ? 0x40 : 0));
        case 0xFF1A: return static_cast<uint8_t>(0x7F | (m_ch3.dac ? 0x80 : 0));
        case 0xFF1B: return 0xFF;
        case 0xFF1C: return static_cast<uint8_t>(0x9F | (m_ch3.volumeCode << 5));
        case 0xFF1D: return 0xFF;
        case 0xFF1E: return static_cast<uint8_t>(0xBF | (m_ch3.lengthEnabled ? 0x40 : 0));
        case 0xFF20: return 0xFF;
        case 0xFF21: return static_cast<uint8_t>((m_ch4.initVolume << 4) |
                                                (m_ch4.envDir ? 0x08 : 0) | m_ch4.envPace);
        case 0xFF22: return static_cast<uint8_t>((m_ch4.clockShift << 4) |
                                                (m_ch4.narrowMode ? 0x08 : 0) | m_ch4.divisor);
        case 0xFF23: return static_cast<uint8_t>(0xBF | (m_ch4.lengthEnabled ? 0x40 : 0));
        case 0xFF24: return static_cast<uint8_t>((m_volLeft << 4) | m_volRight);
        case 0xFF25: return m_pan;
        case 0xFF26:
            return static_cast<uint8_t>(0x70 | (m_power ? 0x80 : 0) |
                                        (m_ch4.enabled ? 0x08 : 0) | (m_ch3.enabled ? 0x04 : 0) |
                                        (m_ch2.enabled ? 0x02 : 0) | (m_ch1.enabled ? 0x01 : 0));
        default: return 0xFF;
    }
}

void APU::writeRegister(uint16_t address, uint8_t value) {
    switch (address) {
        case 0xFF10:
            m_ch1.sweepPace = (value >> 4) & 7;
            m_ch1.sweepDir = (value & 0x08) != 0;
            m_ch1.sweepSlope = value & 7;
            break;
        case 0xFF11:
            m_ch1.duty = (value >> 6) & 3;
            m_ch1.length = value & 63;
            m_ch1.lengthTimer = 64 - m_ch1.length;
            break;
        case 0xFF12:
            m_ch1.initVolume = (value >> 4) & 15;
            m_ch1.envDir = (value & 0x08) != 0;
            m_ch1.envPace = value & 7;
            m_ch1.dac = (value & 0xF8) != 0;
            if (!m_ch1.dac) { m_ch1.enabled = false; }
            break;
        case 0xFF13:
            m_ch1.frequency = (m_ch1.frequency & 0x700) | value;
            break;
        case 0xFF14:
            m_ch1.frequency = (m_ch1.frequency & 0xFF) | ((value & 7) << 8);
            m_ch1.lengthEnabled = (value & 0x40) != 0;
            if (value & 0x80) { triggerPulse(m_ch1, true); }
            break;
        case 0xFF16:
            m_ch2.duty = (value >> 6) & 3;
            m_ch2.length = value & 63;
            m_ch2.lengthTimer = 64 - m_ch2.length;
            break;
        case 0xFF17:
            m_ch2.initVolume = (value >> 4) & 15;
            m_ch2.envDir = (value & 0x08) != 0;
            m_ch2.envPace = value & 7;
            m_ch2.dac = (value & 0xF8) != 0;
            if (!m_ch2.dac) { m_ch2.enabled = false; }
            break;
        case 0xFF18:
            m_ch2.frequency = (m_ch2.frequency & 0x700) | value;
            break;
        case 0xFF19:
            m_ch2.frequency = (m_ch2.frequency & 0xFF) | ((value & 7) << 8);
            m_ch2.lengthEnabled = (value & 0x40) != 0;
            if (value & 0x80) { triggerPulse(m_ch2, false); }
            break;
        case 0xFF1A:
            m_ch3.dac = (value & 0x80) != 0;
            if (!m_ch3.dac) { m_ch3.enabled = false; }
            break;
        case 0xFF1B:
            m_ch3.length = value;
            m_ch3.lengthTimer = static_cast<uint8_t>(256 - value);
            break;
        case 0xFF1C:
            m_ch3.volumeCode = (value >> 5) & 3;
            break;
        case 0xFF1D:
            m_ch3.frequency = (m_ch3.frequency & 0x700) | value;
            break;
        case 0xFF1E:
            m_ch3.frequency = (m_ch3.frequency & 0xFF) | ((value & 7) << 8);
            m_ch3.lengthEnabled = (value & 0x40) != 0;
            if (value & 0x80) { triggerWave(); }
            break;
        case 0xFF20:
            m_ch4.length = value & 63;
            m_ch4.lengthTimer = 64 - m_ch4.length;
            break;
        case 0xFF21:
            m_ch4.initVolume = (value >> 4) & 15;
            m_ch4.envDir = (value & 0x08) != 0;
            m_ch4.envPace = value & 7;
            m_ch4.dac = (value & 0xF8) != 0;
            if (!m_ch4.dac) { m_ch4.enabled = false; }
            break;
        case 0xFF22:
            m_ch4.clockShift = (value >> 4) & 15;
            m_ch4.narrowMode = (value & 0x08) != 0;
            m_ch4.divisor = value & 7;
            break;
        case 0xFF23:
            m_ch4.lengthEnabled = (value & 0x40) != 0;
            if (value & 0x80) { triggerNoise(); }
            break;
        case 0xFF24:
            m_volLeft = (value >> 4) & 7;
            m_volRight = value & 7;
            break;
        case 0xFF25:
            m_pan = value;
            break;
        case 0xFF26: {
            const bool power = (value & 0x80) != 0;
            if (m_power && !power) { // Powering off silences everything
                m_ch1.enabled = m_ch2.enabled = m_ch3.enabled = m_ch4.enabled = false;
                m_seqStep = 0;
                m_seqCounter = 0;
            }
            m_power = power;
            break;
        }
        default: break;
    }
}

uint8_t APU::readWaveRam(uint16_t address) const {
    if (address >= 0xFF30 && address <= 0xFF3F) {
        return m_ch3.ram[address - 0xFF30];
    }
    return 0xFF;
}

void APU::writeWaveRam(uint16_t address, uint8_t value) {
    if (address >= 0xFF30 && address <= 0xFF3F) {
        m_ch3.ram[address - 0xFF30] = value;
    }
}

void APU::saveState(StateWriter& out) const {
    const auto savePulse = [&out](const PulseChannel& ch) {
        out.putU8(ch.enabled ? 1 : 0);
        out.putU8(ch.dac ? 1 : 0);
        out.putU8(ch.sweepPace);
        out.putU8(ch.sweepDir ? 1 : 0);
        out.putU8(ch.sweepSlope);
        out.putU8(ch.duty);
        out.putU8(ch.length);
        out.putU8(ch.lengthEnabled ? 1 : 0);
        out.putU8(ch.initVolume);
        out.putU8(ch.envDir ? 1 : 0);
        out.putU8(ch.envPace);
        out.putU16(ch.frequency);
        out.putU8(ch.volume);
        out.putU8(ch.envTimer);
        out.putU8(ch.dutyPos);
        out.putU16(ch.freqTimer);
        out.putU8(ch.lengthTimer);
        out.putU8(ch.sweepOn ? 1 : 0);
        out.putU8(ch.sweepTimer);
        out.putU16(ch.shadowFreq);
    };
    savePulse(m_ch1);
    savePulse(m_ch2);

    out.putU8(m_ch3.enabled ? 1 : 0);
    out.putU8(m_ch3.dac ? 1 : 0);
    out.putU8(m_ch3.volumeCode);
    out.putU8(m_ch3.length);
    out.putU8(m_ch3.lengthEnabled ? 1 : 0);
    out.putU16(m_ch3.frequency);
    out.putU16(m_ch3.freqTimer);
    out.putU8(m_ch3.position);
    out.putU8(m_ch3.lengthTimer);
    out.putBytes(m_ch3.ram);

    out.putU8(m_ch4.enabled ? 1 : 0);
    out.putU8(m_ch4.dac ? 1 : 0);
    out.putU8(m_ch4.length);
    out.putU8(m_ch4.lengthEnabled ? 1 : 0);
    out.putU8(m_ch4.initVolume);
    out.putU8(m_ch4.envDir ? 1 : 0);
    out.putU8(m_ch4.envPace);
    out.putU8(m_ch4.clockShift);
    out.putU8(m_ch4.narrowMode ? 1 : 0);
    out.putU8(m_ch4.divisor);
    out.putU8(m_ch4.volume);
    out.putU8(m_ch4.envTimer);
    out.putU16(m_ch4.lfsr);
    out.putU16(m_ch4.timer);
    out.putU8(m_ch4.lengthTimer);

    out.putU8(m_volLeft);
    out.putU8(m_volRight);
    out.putU8(m_pan);
    out.putU8(m_power ? 1 : 0);
    out.putU8(m_seqStep);
    out.putU32(m_seqCounter);
    out.putU32(m_sampleFrac);
    out.putU32(std::bit_cast<uint32_t>(m_hpL));
    out.putU32(std::bit_cast<uint32_t>(m_hpR));
}

bool APU::loadState(StateReader& in) {
    const auto loadPulse = [&in](PulseChannel& ch) {
        uint8_t enabled = 0, dac = 0, pace = 0, dir = 0, slope = 0, duty = 0;
        uint8_t len = 0, lenOn = 0, initVol = 0, envDir = 0, envPace = 0;
        uint16_t freq = 0;
        uint8_t vol = 0, envT = 0, pos = 0, lenT = 0, swOn = 0, swT = 0;
        uint16_t freqT = 0, shadow = 0;
        if (!in.readU8(enabled) || !in.readU8(dac) || !in.readU8(pace) ||
            !in.readU8(dir) || !in.readU8(slope) || !in.readU8(duty) ||
            !in.readU8(len) || !in.readU8(lenOn) || !in.readU8(initVol) ||
            !in.readU8(envDir) || !in.readU8(envPace) || !in.readU16(freq) ||
            !in.readU8(vol) || !in.readU8(envT) || !in.readU8(pos) ||
            !in.readU16(freqT) || !in.readU8(lenT) || !in.readU8(swOn) ||
            !in.readU8(swT) || !in.readU16(shadow)) {
            return false;
        }
        ch.enabled = enabled != 0;
        ch.dac = dac != 0;
        ch.sweepPace = pace & 7;
        ch.sweepDir = dir != 0;
        ch.sweepSlope = slope & 7;
        ch.duty = duty & 3;
        ch.length = len & 63;
        ch.lengthEnabled = lenOn != 0;
        ch.initVolume = initVol & 15;
        ch.envDir = envDir != 0;
        ch.envPace = envPace & 7;
        ch.frequency = freq & 2047;
        ch.volume = vol & 15;
        ch.envTimer = envT;
        ch.dutyPos = pos & 7;
        ch.freqTimer = freqT;
        ch.lengthTimer = lenT;
        ch.sweepOn = swOn != 0;
        ch.sweepTimer = swT;
        ch.shadowFreq = shadow & 2047;
        return true;
    };
    if (!loadPulse(m_ch1) || !loadPulse(m_ch2)) {
        return false;
    }

    uint8_t waveOn = 0, waveDac = 0, volCode = 0, waveLen = 0, waveLenOn = 0;
    uint16_t waveFreq = 0, waveFreqT = 0;
    uint8_t wavePos = 0, waveLenT = 0;
    if (!in.readU8(waveOn) || !in.readU8(waveDac) || !in.readU8(volCode) ||
        !in.readU8(waveLen) || !in.readU8(waveLenOn) || !in.readU16(waveFreq) ||
        !in.readU16(waveFreqT) || !in.readU8(wavePos) || !in.readU8(waveLenT) ||
        !in.readBytes(m_ch3.ram)) {
        return false;
    }
    m_ch3.enabled = waveOn != 0;
    m_ch3.dac = waveDac != 0;
    m_ch3.volumeCode = volCode & 3;
    m_ch3.length = waveLen;
    m_ch3.lengthEnabled = waveLenOn != 0;
    m_ch3.frequency = waveFreq & 2047;
    m_ch3.freqTimer = waveFreqT;
    m_ch3.position = wavePos & 31;
    m_ch3.lengthTimer = waveLenT;

    uint8_t nOn = 0, nDac = 0, nLen = 0, nLenOn = 0, nInit = 0, nDir = 0, nPace = 0;
    uint8_t nShift = 0, nNarrow = 0, nDiv = 0, nVol = 0, nEnvT = 0, nLenT = 0;
    uint16_t nLfsr = 0, nTimer = 0;
    if (!in.readU8(nOn) || !in.readU8(nDac) || !in.readU8(nLen) ||
        !in.readU8(nLenOn) || !in.readU8(nInit) || !in.readU8(nDir) ||
        !in.readU8(nPace) || !in.readU8(nShift) || !in.readU8(nNarrow) ||
        !in.readU8(nDiv) || !in.readU8(nVol) || !in.readU8(nEnvT) ||
        !in.readU16(nLfsr) || !in.readU16(nTimer) || !in.readU8(nLenT)) {
        return false;
    }
    m_ch4.enabled = nOn != 0;
    m_ch4.dac = nDac != 0;
    m_ch4.length = nLen & 63;
    m_ch4.lengthEnabled = nLenOn != 0;
    m_ch4.initVolume = nInit & 15;
    m_ch4.envDir = nDir != 0;
    m_ch4.envPace = nPace & 7;
    m_ch4.clockShift = nShift & 15;
    m_ch4.narrowMode = nNarrow != 0;
    m_ch4.divisor = nDiv & 7;
    m_ch4.volume = nVol & 15;
    m_ch4.envTimer = nEnvT;
    m_ch4.lfsr = nLfsr;
    m_ch4.timer = nTimer;
    m_ch4.lengthTimer = nLenT;

    uint8_t volL = 0, volR = 0, pan = 0, power = 0, seq = 0;
    uint32_t seqC = 0, frac = 0, hpL = 0, hpR = 0;
    if (!in.readU8(volL) || !in.readU8(volR) || !in.readU8(pan) ||
        !in.readU8(power) || !in.readU8(seq) || !in.readU32(seqC) ||
        !in.readU32(frac) || !in.readU32(hpL) || !in.readU32(hpR)) {
        return false;
    }
    m_volLeft = volL & 7;
    m_volRight = volR & 7;
    m_pan = pan;
    m_power = power != 0;
    m_seqStep = seq & 7;
    m_seqCounter = seqC;
    m_sampleFrac = frac;
    m_hpL = std::bit_cast<float>(hpL);
    m_hpR = std::bit_cast<float>(hpR);
    return true;
}

} // namespace smile::systems::gameboy
