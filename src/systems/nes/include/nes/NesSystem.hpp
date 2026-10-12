#pragma once

#include "core/ISystem.hpp"
#include "nes/bus/NesBus.hpp"
#include "nes/cpu/NesCPU.hpp"
#include <span>
#include <vector>

namespace smile::systems::nes {

// NES / Dendy core (NTSC): CPU-only bring-up for now. Parses iNES, maps NROM,
// runs the 6502 at 29780 cycles/frame. PPU rendering lands next.
class NesSystem final : public core::ISystem {
public:
    NesSystem();
    ~NesSystem() override = default;

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

private:
    NesBus m_bus;
    NesCPU m_cpu;
    bool m_hasRom{false};
};

} // namespace smile::systems::nes
