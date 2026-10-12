#include "nes/bus/NesBus.hpp"
#include <array>

namespace smile::systems::nes {

NesBus::NesBus() {
    m_ram.fill(0);
    m_ppu.attachCartridge(&m_cart);
}

void NesBus::reset() {
    m_ram.fill(0);
    m_ppu.reset();
    m_shift[0] = m_shift[1] = 0;
    m_shiftCount[0] = m_shiftCount[1] = 0;
    m_strobe = false;
    m_dmaStall = 0;
}

NesCartridge::LoadResult NesBus::loadCartridge(std::span<const uint8_t> romData) {
    return m_cart.load(romData);
}

void NesBus::setButtons(uint8_t pad1, uint8_t pad2) {
    m_buttons[0] = pad1;
    m_buttons[1] = pad2;
}

void NesBus::doOamDma(uint8_t page) {
    // Copy $xx00-$xxFF into OAM, then stall the CPU. The extra cycle lands
    // on odd PPU dots (real hardware parity rule).
    std::array<uint8_t, 256> buffer{};
    const uint16_t base = static_cast<uint16_t>(page << 8);
    for (size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] = read8(static_cast<uint16_t>(base + i));
    }
    m_ppu.oamDma(buffer);
    m_dmaStall = static_cast<uint16_t>(513 + (m_ppu.dot() & 1));
}

uint8_t NesBus::read8(uint16_t address) {
    if (address < 0x2000) { // 2 KB RAM, mirrored 4x
        return m_ram[address & 0x07FF];
    }
    if (address < 0x4000) { // PPU registers, mirrored every 8 bytes
        return m_ppu.readRegister(address);
    }
    if (address == 0x4015) { // APU status stub
        return 0x00;
    }
    if (address == 0x4016 || address == 0x4017) { // Joypads
        const int pad = address - 0x4016;
        if (m_strobe) {
            return m_buttons[pad] & 1; // Strobe held: A button
        }
        if (m_shiftCount[pad] >= 8) {
            return 1; // Shift exhausted: open bus reads 1
        }
        const uint8_t bit = m_shift[pad] & 1;
        m_shift[pad] >>= 1;
        m_shiftCount[pad]++;
        return bit;
    }
    if (address < 0x4020) { // APU / I/O stubs
        return 0xFF;
    }
    if (address < 0x8000) { // Cartridge SRAM/PRG-RAM zone (no battery yet)
        return 0xFF;
    }
    return m_cart.prgRead(address); // $8000-FFFF: banked PRG
}

void NesBus::write8(uint16_t address, uint8_t value) {
    if (address < 0x2000) { // RAM mirror
        m_ram[address & 0x07FF] = value;
        return;
    }
    if (address < 0x4000) { // PPU registers
        m_ppu.writeRegister(address, value);
        return;
    }
    if (address == 0x4014) { // OAM DMA
        doOamDma(value);
        return;
    }
    if (address == 0x4016) { // Joypad strobe
        m_strobe = (value & 1) != 0;
        if (m_strobe) {
            m_shift[0] = m_buttons[0];
            m_shift[1] = m_buttons[1];
            m_shiftCount[0] = 0;
            m_shiftCount[1] = 0;
        }
        return;
    }
    if (address >= 0x8000) { // Mapper registers (UNROM bank switch)
        m_cart.prgWrite(address, value);
        return;
    }
    // APU registers and the rest: ignored until their cores land.
}

} // namespace smile::systems::nes
