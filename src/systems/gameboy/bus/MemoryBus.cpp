#include "gameboy/bus/MemoryBus.hpp"
#include <algorithm>
#include <array>
#include <cstdio>

namespace smile::systems::gameboy {

MemoryBus::MemoryBus(smile::core::AudioRingBuffer& audio)
    : m_apu(audio) {
    m_wram.resize(8192, 0); // 8 KB WRAM (0xC000-0xDFFF)
    m_hram.resize(127, 0);  // 127 B HRAM (0xFF80-0xFFFE)
}

bool MemoryBus::loadRom(std::span<const uint8_t> romData) {
    return m_cartridge.loadRom(romData);
}

void MemoryBus::reset() {
    std::fill(m_wram.begin(), m_wram.end(), 0);
    std::fill(m_hram.begin(), m_hram.end(), 0);
    m_cartridge.reset();
    m_ppu.reset();
    m_timer.reset();
    m_apu.reset();
    m_if = 0xE0;
    m_ie = 0x00;
    m_serialData = 0xFF;
    m_serialCtrl = 0x7E;
    m_joypadSelect = 0xFF;
    m_joypadState = 0x00;
}

void MemoryBus::stepComponents(uint32_t tCycles) {
    bool vblankIrq = false;
    bool statIrq = false;
    m_ppu.step(tCycles, vblankIrq, statIrq);
    if (vblankIrq) {
        requestInterrupt(Interrupt::VBlank);
    }
    if (statIrq) {
        requestInterrupt(Interrupt::Stat);
    }

    bool timerIrq = false;
    m_timer.step(tCycles, timerIrq);
    if (timerIrq) {
        requestInterrupt(Interrupt::Timer);
    }

    m_apu.step(tCycles);
}

void MemoryBus::doDma(uint8_t page) {
    // Copy 160 bytes from (page << 8) into OAM via the PPU.
    const uint16_t src = static_cast<uint16_t>(page << 8);
    std::array<uint8_t, 160> tmp{};
    for (size_t i = 0; i < tmp.size(); ++i) {
        tmp[i] = read8(static_cast<uint16_t>(src + i));
    }
    m_ppu.dmaCopy(tmp);
}

uint8_t MemoryBus::read8(uint16_t address) {
    if (address < 0x8000) { // Cartridge ROM (MBC-banked)
        return m_cartridge.readRom(address);
    }
    if (address <= 0x9FFF) { // VRAM
        return m_ppu.readVRAM(address);
    }
    if (address >= 0xA000 && address <= 0xBFFF) { // Cartridge RAM
        return m_cartridge.readRam(address);
    }
    if (address >= 0xC000 && address <= 0xDFFF) { // WRAM
        return m_wram[address - 0xC000];
    }
    if (address >= 0xE000 && address <= 0xFDFF) { // Echo RAM
        return m_wram[address - 0xE000];
    }
    if (address >= 0xFE00 && address <= 0xFE9F) { // OAM
        return m_ppu.readOAM(address);
    }
    if (address == 0xFF01) { // Serial data (SB)
        return m_serialData;
    }
    if (address == 0xFF02) { // Serial control (SC)
        return m_serialCtrl;
    }
    if (address == 0xFF00) { // Joypad
        uint8_t result = static_cast<uint8_t>(m_joypadSelect | 0x0F);
        if ((m_joypadSelect & 0x10) == 0) { // Direction keys (low nibble, active low)
            result &= static_cast<uint8_t>(~((m_joypadState >> 4) & 0x0F));
        }
        if ((m_joypadSelect & 0x20) == 0) { // Action buttons
            result &= static_cast<uint8_t>(~(m_joypadState & 0x0F));
        }
        return result | 0xC0; // Upper bits read as 1
    }
    if (address >= 0xFF04 && address <= 0xFF07) { // Timer
        return m_timer.readRegister(address);
    }
    if (address == 0xFF0F) { // IF
        return m_if;
    }
    if (address >= 0xFF10 && address <= 0xFF26) { // APU
        return m_apu.readRegister(address);
    }
    if (address >= 0xFF30 && address <= 0xFF3F) { // Wave RAM
        return m_apu.readWaveRam(address);
    }
    if (address >= 0xFF40 && address <= 0xFF4B) { // PPU registers
        return m_ppu.readRegister(address);
    }
    if (address >= 0xFF80 && address <= 0xFFFE) { // HRAM
        return m_hram[address - 0xFF80];
    }
    if (address == 0xFFFF) { // IE
        return m_ie;
    }
    // Serial (0xFF01-0xFF02), unmapped I/O: open bus.
    return 0xFF;
}

void MemoryBus::write8(uint16_t address, uint8_t value) {
    if (address < 0x8000) { // Cartridge control (MBC registers)
        m_cartridge.writeControl(address, value);
        return;
    }
    if (address <= 0x9FFF && address >= 0x8000) { // VRAM
        m_ppu.writeVRAM(address, value);
        return;
    }
    if (address >= 0xA000 && address <= 0xBFFF) { // Cartridge RAM
        m_cartridge.writeRam(address, value);
        return;
    }
    if (address >= 0xC000 && address <= 0xDFFF) { // WRAM
        m_wram[address - 0xC000] = value;
        return;
    }
    if (address >= 0xE000 && address <= 0xFDFF) { // Echo RAM
        m_wram[address - 0xE000] = value;
        return;
    }
    if (address >= 0xFE00 && address <= 0xFE9F) { // OAM
        m_ppu.writeOAM(address, value);
        return;
    }
    if (address == 0xFF01) { // Serial data (SB)
        m_serialData = value;
        return;
    }
    if (address == 0xFF02) { // Serial control (SC)
        m_serialCtrl = value;
        if ((value & 0x80) != 0) {
            // Instant transfer: test ROMs (e.g. Blargg's) print results here.
            std::putchar(static_cast<char>(m_serialData));
            std::fflush(stdout);
            m_serialCtrl &= 0x7F; // Transfer completes immediately
        }
        return;
    }
    if (address == 0xFF00) { // Joypad select
        m_joypadSelect = static_cast<uint8_t>(value & 0x30);
        return;
    }
    if (address >= 0xFF04 && address <= 0xFF07) { // Timer
        m_timer.writeRegister(address, value);
        return;
    }
    if (address == 0xFF0F) { // IF
        m_if = static_cast<uint8_t>(value | 0xE0);
        return;
    }
    if (address >= 0xFF10 && address <= 0xFF26) { // APU
        m_apu.writeRegister(address, value);
        return;
    }
    if (address >= 0xFF30 && address <= 0xFF3F) { // Wave RAM
        m_apu.writeWaveRam(address, value);
        return;
    }
    // NOTE: $FF46 (OAM DMA) sits inside the PPU register range, so it must
    // be decoded first — otherwise the DMA trigger is swallowed and OAM
    // stays zero forever (a classic: background renders, sprites never do).
    if (address == 0xFF46) { // OAM DMA
        doDma(value);
        return;
    }
    if (address >= 0xFF40 && address <= 0xFF4B) { // PPU registers
        m_ppu.writeRegister(address, value);
        return;
    }
    if (address >= 0xFF80 && address <= 0xFFFE) { // HRAM
        m_hram[address - 0xFF80] = value;
        return;
    }
    if (address == 0xFFFF) { // IE
        m_ie = value;
        return;
    }
    // Serial and unmapped I/O: ignored.
}

} // namespace smile::systems::gameboy
