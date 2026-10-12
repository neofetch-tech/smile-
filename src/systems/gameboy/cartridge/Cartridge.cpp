#include "gameboy/cartridge/Cartridge.hpp"
#include <algorithm>

namespace smile::systems::gameboy {

namespace {
// Header offsets in the ROM image.
constexpr uint16_t kTypeAddr = 0x0147;
constexpr uint16_t kRamSizeAddr = 0x0149;
constexpr size_t kBankSize = 16384;
constexpr size_t kRamBankSize = 8192;
} // namespace

bool Cartridge::loadRom(std::span<const uint8_t> romData) {
    if (romData.size() < 0x014A) {
        return false;
    }
    const uint8_t typeByte = romData[kTypeAddr];
    Type type = Type::None;
    bool battery = false;
    if (typeByte == 0x00) {
        type = Type::NoMBC;
    } else if (typeByte >= 0x01 && typeByte <= 0x03) {
        type = Type::MBC1;
        battery = (typeByte == 0x03);
    } else if (typeByte >= 0x19 && typeByte <= 0x1E) {
        type = Type::MBC5;
        battery = (typeByte == 0x1B || typeByte == 0x1E);
    } else {
        return false; // MBC2/MBC3/MBC6/MMM01 and others are not emulated yet
    }

    m_rom.assign(romData.begin(), romData.end());
    m_type = type;
    m_hasBattery = battery;
    m_dirty = false;

    // External RAM size from header (applies to MBC1/MBC5 cartridges).
    size_t ramBanks = 0;
    if (type != Type::NoMBC) {
        switch (romData[kRamSizeAddr]) {
            case 0x01: ramBanks = 1; break; // 2 KB (partial bank)
            case 0x02: ramBanks = 1; break; // 8 KB
            case 0x03: ramBanks = 4; break; // 32 KB
            case 0x04: ramBanks = 16; break; // 128 KB
            case 0x05: ramBanks = 8; break; // 64 KB
            default: break;
        }
    }
    m_hasRam = ramBanks > 0;
    m_ram.assign(ramBanks * kRamBankSize, 0);

    reset();
    return true;
}

void Cartridge::reset() {
    m_ramEnabled = false;
    m_romLow5 = 1;
    m_romBank = 1;
    m_ramBank = 0;
    m_mode = 0;
}

size_t Cartridge::bankCount() const noexcept {
    return m_rom.size() / kBankSize;
}

uint16_t Cartridge::effectiveRomBank(bool upperArea) const noexcept {
    const size_t banks = bankCount();
    if (banks == 0) {
        return 0;
    }
    uint16_t bank = 0;
    if (m_type == Type::MBC1) {
        if (!upperArea && m_mode == 0) {
            return 0; // Fixed bank 0 in ROM banking mode
        }
        bank = static_cast<uint16_t>((m_ramBank << 5) | m_romLow5);
        if (!upperArea) {
            bank &= 0x60; // Lower area exposes only the high bits in RAM banking mode
        }
    } else if (m_type == Type::MBC5) {
        bank = upperArea ? m_romBank : 0;
    }
    // Bank 0 of the upper area can never be 0 on real hardware; it maps to 1.
    if (upperArea && (bank & 0x1F) == 0 && m_type == Type::MBC1) {
        bank |= 1;
    }
    return static_cast<uint16_t>(bank % banks);
}

uint8_t Cartridge::readRom(uint16_t address) const {
    if (address < m_rom.size() && m_type == Type::NoMBC) {
        return m_rom[address];
    }
    if (m_type == Type::NoMBC) {
        return 0xFF;
    }
    const size_t banks = bankCount();
    if (banks == 0) {
        return 0xFF;
    }
    if (address < 0x4000) {
        const size_t offset = static_cast<size_t>(effectiveRomBank(false)) * kBankSize + address;
        return (offset < m_rom.size()) ? m_rom[offset] : 0xFF;
    }
    const size_t offset =
        static_cast<size_t>(effectiveRomBank(true)) * kBankSize + (address - 0x4000);
    return (offset < m_rom.size()) ? m_rom[offset] : 0xFF;
}

void Cartridge::writeControl(uint16_t address, uint8_t value) {
    if (m_type == Type::MBC1) {
        if (address < 0x2000) {
            m_ramEnabled = (value & 0x0F) == 0x0A;
        } else if (address < 0x4000) {
            m_romLow5 = static_cast<uint8_t>(value & 0x1F);
            if (m_romLow5 == 0) {
                m_romLow5 = 1;
            }
        } else if (address < 0x6000) {
            m_ramBank = static_cast<uint8_t>(value & 0x03);
        } else if (address < 0x8000) {
            m_mode = static_cast<uint8_t>(value & 0x01);
        }
    } else if (m_type == Type::MBC5) {
        if (address < 0x2000) {
            m_ramEnabled = (value & 0x0F) == 0x0A;
        } else if (address < 0x3000) {
            m_romBank = static_cast<uint16_t>((m_romBank & 0x100) | value);
        } else if (address < 0x4000) {
            m_romBank = static_cast<uint16_t>((m_romBank & 0xFF) |
                                              ((value & 0x01) << 8));
        } else if (address < 0x6000) {
            m_ramBank = static_cast<uint8_t>(value & 0x0F);
        }
    }
}

uint8_t Cartridge::readRam(uint16_t address) const {
    if (!m_hasRam || !m_ramEnabled || address < 0xA000 || address > 0xBFFF) {
        return 0xFF;
    }
    size_t bank = 0;
    if (m_type == Type::MBC1) {
        bank = (m_mode == 1) ? (m_ramBank & 0x03) : 0;
    } else if (m_type == Type::MBC5) {
        bank = m_ramBank & 0x0F;
    }
    const size_t offset = bank * kRamBankSize + (address - 0xA000);
    return (offset < m_ram.size()) ? m_ram[offset] : 0xFF;
}

void Cartridge::writeRam(uint16_t address, uint8_t value) {
    if (!m_hasRam || !m_ramEnabled || address < 0xA000 || address > 0xBFFF) {
        return;
    }
    size_t bank = 0;
    if (m_type == Type::MBC1) {
        bank = (m_mode == 1) ? (m_ramBank & 0x03) : 0;
    } else if (m_type == Type::MBC5) {
        bank = m_ramBank & 0x0F;
    }
    const size_t offset = bank * kRamBankSize + (address - 0xA000);
    if (offset < m_ram.size()) {
        m_ram[offset] = value;
        m_dirty = true;
    }
}

void Cartridge::saveState(StateWriter& out) const {
    out.putU8(static_cast<uint8_t>(m_type));
    out.putU8(m_ramEnabled ? 1 : 0);
    out.putU8(m_romLow5);
    out.putU16(m_romBank);
    out.putU8(m_ramBank);
    out.putU8(m_mode);
    out.putU32(static_cast<uint32_t>(m_ram.size()));
    out.putBytes(m_ram);
}

bool Cartridge::loadState(StateReader& in) {
    uint8_t type = 0;
    uint8_t ramEnabled = 0;
    uint8_t romLow5 = 0;
    uint16_t romBank = 0;
    uint8_t ramBank = 0;
    uint8_t mode = 0;
    uint32_t ramSize = 0;

    if (!in.readU8(type) || !in.readU8(ramEnabled) || !in.readU8(romLow5) ||
        !in.readU16(romBank) || !in.readU8(ramBank) || !in.readU8(mode) ||
        !in.readU32(ramSize)) {
        return false;
    }
    if (type != static_cast<uint8_t>(m_type) || ramSize != m_ram.size()) {
        return false; // State belongs to another cartridge image
    }
    if (!in.readBytes(m_ram)) {
        return false;
    }

    m_ramEnabled = ramEnabled != 0;
    m_romLow5 = (romLow5 & 0x1F) == 0 ? 1 : static_cast<uint8_t>(romLow5 & 0x1F);
    m_romBank = romBank & 0x1FF;
    m_ramBank = ramBank & 0x0F;
    m_mode = mode & 0x01;
    return true;
}

bool Cartridge::importRam(std::span<const uint8_t> data) {
    // Battery file must match the cartridge RAM exactly; anything else is a
    // foreign save and is rejected instead of corrupting memory.
    if (!m_hasBattery || data.size() != m_ram.size() || m_ram.empty()) {
        return false;
    }
    std::copy(data.begin(), data.end(), m_ram.begin());
    m_dirty = false;
    return true;
}

} // namespace smile::systems::gameboy
