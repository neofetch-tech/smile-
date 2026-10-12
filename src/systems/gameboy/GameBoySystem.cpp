#include "gameboy/GameBoySystem.hpp"
#include "gameboy/StateCodec.hpp"

namespace smile::systems::gameboy {

namespace {
// Save-state envelope: magic + format version, then CPU, then bus.
constexpr uint32_t kStateVersion = 1;
}

GameBoySystem::GameBoySystem()
    : m_bus(m_audio), m_cpu(m_bus) {}

std::expected<void, core::EmulationError> GameBoySystem::loadRom(std::span<const uint8_t> romData) {
    if (romData.empty()) {
        return std::unexpected(core::EmulationError::InvalidRomFormat);
    }
    if (!m_bus.loadRom(romData)) {
        return std::unexpected(core::EmulationError::UnsupportedMapper);
    }
    reset();
    return {};
}

void GameBoySystem::reset() {
    m_bus.reset();
    m_cpu.reset();
    m_audio.clear();
}

void GameBoySystem::stepFrame() {
    // One frame = 70224 T-states (154 scanlines x 456 dots).
    constexpr int kCyclesPerFrame = 70224;
    int accumulated = 0;

    while (accumulated < kCyclesPerFrame) {
        const uint8_t mCycles = m_cpu.step();
        const uint32_t tCycles = static_cast<uint32_t>(mCycles) * 4;
        m_bus.stepComponents(tCycles);
        accumulated += static_cast<int>(tCycles);
    }
}

std::span<const uint32_t> GameBoySystem::getFrameBuffer() const noexcept {
    return m_bus.getPPU().getFrameBuffer();
}

core::Resolution GameBoySystem::getResolution() const noexcept {
    return {160, 144};
}

size_t GameBoySystem::readAudioSamples(std::span<float> outBuffer) {
    // Drains interleaved stereo frames produced by the APU.
    return m_audio.read(outBuffer);
}

void GameBoySystem::setControllerState(uint8_t /*port*/, uint32_t buttonMask) {
    // Lower 8 bits: bit0 A, bit1 B, bit2 Select, bit3 Start,
    //               bit4 Right, bit5 Left, bit6 Up, bit7 Down (1 = pressed).
    m_bus.setJoypadState(static_cast<uint8_t>(buttonMask & 0xFF));
    if (buttonMask & 0xFF) {
        m_bus.requestInterrupt(Interrupt::Joypad);
    }
}

core::SystemInfo GameBoySystem::getSystemInfo() const noexcept {
    return {
        .name = "Game Boy (DMG)",
        .nativeResolution = {160, 144},
        .targetFps = 59.73,
        .audioSampleRate = 44100
    };
}

std::vector<uint8_t> GameBoySystem::serializeState() const {
    StateWriter out;
    out.putU8('S');
    out.putU8('M');
    out.putU8('L');
    out.putU8('E');
    out.putU32(kStateVersion);
    m_cpu.saveState(out);
    m_bus.saveState(out);
    return std::move(out).finish();
}

bool GameBoySystem::deserializeState(std::span<const uint8_t> data) {
    StateReader in(data);

    uint8_t magic[4] = {0, 0, 0, 0};
    for (auto& byte : magic) {
        if (!in.readU8(byte)) {
            return false;
        }
    }
    if (magic[0] != 'S' || magic[1] != 'M' || magic[2] != 'L' || magic[3] != 'E') {
        return false;
    }

    uint32_t version = 0;
    if (!in.readU32(version) || version != kStateVersion) {
        return false;
    }
    // NOTE: ROM bytes are not stored; the state only matches the loaded cartridge.
    if (!m_cpu.loadState(in) || !m_bus.loadState(in)) {
        return false;
    }
    return in.exhausted();
}

std::vector<uint8_t> GameBoySystem::exportBattery() const {
    const Cartridge& cart = m_bus.cartridge();
    if (cart.hasBattery() && cart.ramSize() > 0) {
        return cart.exportRam();
    }
    return {};
}

bool GameBoySystem::importBattery(std::span<const uint8_t> data) {
    return m_bus.cartridge().importRam(data);
}

bool GameBoySystem::takeBatteryDirty() {
    return m_bus.cartridge().takeDirty();
}

} // namespace smile::systems::gameboy
