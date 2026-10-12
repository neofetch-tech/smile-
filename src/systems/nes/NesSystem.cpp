#include "nes/NesSystem.hpp"

#include <algorithm>

namespace smile::systems::nes {

namespace {
// NTSC frame budget in CPU cycles.
constexpr int kCyclesPerFrame = 29780;
} // namespace

NesSystem::NesSystem()
    : m_bus(), m_cpu(m_bus) {}

std::expected<void, core::EmulationError> NesSystem::loadRom(std::span<const uint8_t> romData) {
    if (romData.empty()) {
        return std::unexpected(core::EmulationError::InvalidRomFormat);
    }
    switch (m_bus.loadCartridge(romData)) {
        case NesCartridge::LoadResult::Ok:
            break;
        case NesCartridge::LoadResult::BadMapper:
            return std::unexpected(core::EmulationError::UnsupportedMapper);
        case NesCartridge::LoadResult::BadFormat:
        default:
            return std::unexpected(core::EmulationError::InvalidRomFormat);
    }
    m_hasRom = true;
    reset();
    return {};
}

void NesSystem::reset() {
    m_bus.reset();
    m_cpu.reset();
}

void NesSystem::stepFrame() {
    if (!m_hasRom) {
        return;
    }
    // NTSC frame: 29780 CPU cycles, 3 PPU dots per CPU cycle.
    int executed = 0;
    while (executed < kCyclesPerFrame) {
        const uint16_t cycles = m_cpu.step();
        bool nmi = false;
        m_bus.ppu().stepDots(static_cast<uint32_t>(cycles) * 3, nmi);
        if (nmi) {
            m_cpu.requestNmi();
        }
        executed += cycles;
    }
}

std::span<const uint32_t> NesSystem::getFrameBuffer() const noexcept {
    return m_bus.ppu().frame();
}

core::Resolution NesSystem::getResolution() const noexcept {
    return {256, 240};
}

size_t NesSystem::readAudioSamples(std::span<float> /*outBuffer*/) {
    return 0; // APU lands after the PPU
}

void NesSystem::setControllerState(uint8_t port, uint32_t buttonMask) {
    // Frontend mask (shared GB layout: A,B,Select,Start,Right,Left,Up,Down)
    // repacked into NES order: A,B,Select,Start,Up,Down,Left,Right.
    const uint8_t mask = static_cast<uint8_t>(buttonMask & 0xFF);
    const uint8_t nes =
        static_cast<uint8_t>(((mask >> 0) & 1) | (((mask >> 1) & 1) << 1) |
                             (((mask >> 2) & 1) << 2) | (((mask >> 3) & 1) << 3) |
                             (((mask >> 6) & 1) << 4) | (((mask >> 7) & 1) << 5) |
                             (((mask >> 5) & 1) << 6) | (((mask >> 4) & 1) << 7));
    if (port == 0) {
        m_bus.setButtons(nes, 0);
    }
}

core::SystemInfo NesSystem::getSystemInfo() const noexcept {
    return {
        .name = "NES / Dendy (NTSC)",
        .nativeResolution = {256, 240},
        .targetFps = 60.10,
        .audioSampleRate = 44100
    };
}

std::vector<uint8_t> NesSystem::serializeState() const {
    return {}; // Save states arrive with the PPU core
}

bool NesSystem::deserializeState(std::span<const uint8_t> /*data*/) {
    return false;
}

} // namespace smile::systems::nes
