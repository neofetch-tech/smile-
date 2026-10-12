#pragma once

#include "core/ISystem.hpp"
#include "gameboy/bus/MemoryBus.hpp"
#include "gameboy/cpu/CPU.hpp"
#include "smile/core/AudioBuffer.hpp"
#include <vector>
#include <span>

namespace smile::systems::gameboy {

// DMG Game Boy system: owns the bus (PPU + Timer + memory map) and the CPU.
// The framebuffer lives in the PPU; this class only forwards the core interface.
class GameBoySystem final : public core::ISystem {
public:
    GameBoySystem();
    ~GameBoySystem() override = default;

    [[nodiscard]] std::expected<void, core::EmulationError> loadRom(std::span<const uint8_t> romData) override;
    void reset() override;

    void stepFrame() override;

    [[nodiscard]] std::span<const uint32_t> getFrameBuffer() const noexcept override;
    [[nodiscard]] core::Resolution getResolution() const noexcept override;

    size_t readAudioSamples(std::span<float> outBuffer) override;

    void setControllerState(uint8_t port, uint32_t buttonMask) override;

    [[nodiscard]] core::SystemInfo getSystemInfo() const noexcept override;

    [[nodiscard]] std::vector<uint8_t> serializeState() const override;
    bool deserializeState(std::span<const uint8_t> data) override;

    [[nodiscard]] std::vector<uint8_t> exportBattery() const override;
    bool importBattery(std::span<const uint8_t> data) override;
    bool takeBatteryDirty() override;

private:
    smile::core::AudioRingBuffer m_audio; // APU output, drained by the frontend
    MemoryBus m_bus;
    CPU m_cpu;
};

} // namespace smile::systems::gameboy
