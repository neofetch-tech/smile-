#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <filesystem>
#include <expected>
#include <vector>

namespace smile::core {

enum class EmulationError {
    RomNotFound,
    InvalidRomFormat,
    UnsupportedMapper,
    CorruptedState
};

struct Resolution {
    uint32_t width{0};
    uint32_t height{0};
};

struct SystemInfo {
    std::string_view name;
    Resolution nativeResolution;
    double targetFps;
    uint32_t audioSampleRate;
};

class ISystem {
public:
    virtual ~ISystem() = default;

    // Lifecycle
    [[nodiscard]] virtual std::expected<void, EmulationError> loadRom(std::span<const uint8_t> romData) = 0;
    virtual void reset() = 0;

    // Emulation: advance by one frame
    virtual void stepFrame() = 0;

    // Video: linear pixel buffer (ARGB8888)
    [[nodiscard]] virtual std::span<const uint32_t> getFrameBuffer() const noexcept = 0;
    [[nodiscard]] virtual Resolution getResolution() const noexcept = 0;

    // Audio: drain generated stereo samples (L/R interleaved)
    virtual size_t readAudioSamples(std::span<float> outBuffer) = 0;

    // Input: button state bitmask per port
    virtual void setControllerState(uint8_t port, uint32_t buttonMask) = 0;

    // Metadata
    [[nodiscard]] virtual SystemInfo getSystemInfo() const noexcept = 0;

    // Save states
    [[nodiscard]] virtual std::vector<uint8_t> serializeState() const = 0;
    virtual bool deserializeState(std::span<const uint8_t> data) = 0;

    // Battery-backed cartridge RAM (game saves). Default: unsupported.
    [[nodiscard]] virtual std::vector<uint8_t> exportBattery() const { return {}; }
    virtual bool importBattery(std::span<const uint8_t> /*data*/) { return false; }
    // True once after battery RAM changed; clears the flag. Drives autosave.
    virtual bool takeBatteryDirty() { return false; }
};

} // namespace smile::core
