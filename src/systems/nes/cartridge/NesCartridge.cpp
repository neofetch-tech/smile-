#include "nes/cartridge/NesCartridge.hpp"
#include <algorithm>

namespace smile::systems::nes {

NesCartridge::LoadResult NesCartridge::load(std::span<const uint8_t> romData) {
    if (romData.size() < 16 || romData[0] != 'N' || romData[1] != 'E' ||
        romData[2] != 'S' || romData[3] != 0x1A) {
        return LoadResult::BadFormat;
    }
    const uint8_t prgUnits = romData[4];
    const uint8_t chrUnits = romData[5];
    const uint8_t flags6 = romData[6];
    const uint8_t flags7 = romData[7];
    const int mapper = ((flags6 >> 4) & 0x0F) | (flags7 & 0xF0);
    if (prgUnits == 0 || (mapper != 0 && mapper != 2)) {
        return (mapper == 0 || mapper == 2) ? LoadResult::BadFormat
                                            : LoadResult::BadMapper;
    }

    size_t offset = 16;
    if (flags6 & 0x04) {
        offset += 512; // Skip trainer
    }
    const size_t prgSize = static_cast<size_t>(prgUnits) * 16384;
    const size_t chrSize = static_cast<size_t>(chrUnits) * 8192;
    if (romData.size() < offset + prgSize + chrSize) {
        return LoadResult::BadFormat;
    }

    m_mapper = mapper;
    m_mirror = (flags6 & 0x01) ? NesMirror::Vertical : NesMirror::Horizontal;
    m_prg.assign(romData.data() + offset, romData.data() + offset + prgSize);
    if (chrSize > 0) {
        m_chr.assign(romData.data() + offset + prgSize,
                     romData.data() + offset + prgSize + chrSize);
        m_chrRam = false;
    } else {
        m_chr.assign(8192, 0); // CHR-RAM for homebrew and RAM boards
        m_chrRam = true;
    }
    m_prgBank = 0;
    return LoadResult::Ok;
}

uint8_t NesCartridge::prgRead(uint16_t address) const {
    if (m_prg.empty() || address < 0x8000) {
        return 0xFF;
    }
    if (m_mapper == 0) {
        // NROM: 32 KB straight, 16 KB mirrored across $8000/$C000.
        return m_prg[(address - 0x8000) % m_prg.size()];
    }
    // UNROM: switchable 16 KB at $8000, last bank fixed at $C000.
    const size_t banks = m_prg.size() / 16384;
    if (banks == 0) {
        return 0xFF;
    }
    size_t bank = 0;
    size_t offset = 0;
    if (address < 0xC000) {
        bank = m_prgBank % banks;
        offset = address - 0x8000;
    } else {
        bank = banks - 1;
        offset = address - 0xC000;
    }
    const size_t index = bank * 16384 + offset;
    return (index < m_prg.size()) ? m_prg[index] : 0xFF;
}

void NesCartridge::prgWrite(uint16_t address, uint8_t value) {
    // UNROM bank register: any write to $8000-FFFF selects the low 4 bits.
    // (Bus conflicts with ROM contents are ignored: no commercial UNROM
    // title relies on them.)
    if (m_mapper == 2 && address >= 0x8000) {
        m_prgBank = static_cast<uint8_t>(value & 0x0F);
    }
}

uint8_t NesCartridge::chrRead(uint16_t address) const {
    if (address >= m_chr.size()) {
        return 0xFF;
    }
    return m_chr[address];
}

void NesCartridge::chrWrite(uint16_t address, uint8_t value) {
    if (m_chrRam && address < m_chr.size()) {
        m_chr[address] = value;
    }
}

} // namespace smile::systems::nes
