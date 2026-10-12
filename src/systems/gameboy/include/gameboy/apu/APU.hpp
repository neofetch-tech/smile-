#pragma once

#include <array>
#include <cstdint>
#include "smile/core/AudioBuffer.hpp"
#include "gameboy/StateCodec.hpp"

namespace smile::systems::gameboy {

// DMG APU: two pulse channels (CH1 with sweep), wave channel, noise channel.
// Clocked in T-cycles; stereo float samples are pushed into the frontend ring buffer.
class APU {
public:
    static constexpr uint32_t kSampleRate = 44100;
    static constexpr uint32_t kClockHz = 4194304;

    explicit APU(smile::core::AudioRingBuffer& output) : m_output(output) {}

    void reset();
    void step(uint32_t tCycles);

    [[nodiscard]] uint8_t readRegister(uint16_t address) const;
    void writeRegister(uint16_t address, uint8_t value);

    [[nodiscard]] uint8_t readWaveRam(uint16_t address) const;
    void writeWaveRam(uint16_t address, uint8_t value);

    void saveState(StateWriter& out) const;
    bool loadState(StateReader& in);

private:
    struct PulseChannel {
        bool enabled{false};
        bool dac{false};
        uint8_t sweepPace{0};
        bool sweepDir{false};
        uint8_t sweepSlope{0};
        uint8_t duty{0};
        uint8_t length{0};
        bool lengthEnabled{false};
        uint8_t initVolume{0};
        bool envDir{false};
        uint8_t envPace{0};
        uint16_t frequency{0};
        // Runtime state
        uint8_t volume{0};
        uint8_t envTimer{0};
        uint8_t dutyPos{0};
        uint16_t freqTimer{0};
        uint8_t lengthTimer{0};
        bool sweepOn{false};
        uint8_t sweepTimer{0};
        uint16_t shadowFreq{0};
    };

    struct WaveChannel {
        bool enabled{false};
        bool dac{false};
        uint8_t volumeCode{0};
        uint8_t length{0};
        bool lengthEnabled{false};
        uint16_t frequency{0};
        uint16_t freqTimer{0};
        uint8_t position{0};
        uint8_t lengthTimer{0};
        std::array<uint8_t, 16> ram{};
    };

    struct NoiseChannel {
        bool enabled{false};
        bool dac{false};
        uint8_t length{0};
        bool lengthEnabled{false};
        uint8_t initVolume{0};
        bool envDir{false};
        uint8_t envPace{0};
        uint8_t clockShift{0};
        bool narrowMode{false};
        uint8_t divisor{0};
        // Runtime state
        uint8_t volume{0};
        uint8_t envTimer{0};
        uint16_t lfsr{0x7FFF};
        uint16_t timer{0};
        uint8_t lengthTimer{0};
    };

    void tickSequencer();
    void tickTimers();
    void pushSample();
    void triggerPulse(PulseChannel& ch, bool withSweep);
    void triggerWave();
    void triggerNoise();
    void clockLength(uint8_t& timer, bool enabled, bool& channelOn);
    static void clockEnvelope(uint8_t& volume, uint8_t& timer, uint8_t pace, bool dir);
    void clockSweep();

    [[nodiscard]] uint8_t pulseOutput(const PulseChannel& ch) const;
    [[nodiscard]] uint8_t waveOutput() const;
    [[nodiscard]] uint8_t noiseOutput() const;

    smile::core::AudioRingBuffer& m_output;

    PulseChannel m_ch1;
    PulseChannel m_ch2;
    WaveChannel m_ch3;
    NoiseChannel m_ch4;

    uint8_t m_volLeft{7};
    uint8_t m_volRight{7};
    uint8_t m_pan{0xF3};
    bool m_power{true};

    uint8_t m_seqStep{0};
    uint32_t m_seqCounter{0};
    uint32_t m_sampleFrac{0};

    // One-pole high-pass state per side: removes the DAC DC offset
    // so digital silence is true silence instead of a constant -1.0.
    float m_hpL{0.0f};
    float m_hpR{0.0f};
};

} // namespace smile::systems::gameboy
