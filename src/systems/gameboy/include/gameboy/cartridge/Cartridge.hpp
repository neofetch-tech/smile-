#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include "gameboy/StateCodec.hpp"

namespace smile::systems::gameboy {

// Cartridge (MBC) controller: ROM banking + external RAM.
// Supported: NoMBC (0x00), MBC1 (0x01-0x03), MBC5 (0x19-0x1E, rumble ignored).
class Cartridge {
public:
    enum class Type : uint8_t { None, NoMBC, MBC1, MBC5 };

    Cartridge() = default;

    // Returns false when the cartridge type byte (0x0147) is unsupported.
    bool loadRom(std::span<const uint8_t> romData);
    void reset(); // Clears banking state; the ROM image stays loaded.

    [[nodiscard]] Type type() const noexcept { return m_type; }
    [[nodiscard]] bool hasRom() const noexcept { return !m_rom.empty(); }
    [[nodiscard]] bool hasBattery() const noexcept { return m_hasBattery; }
    [[nodiscard]] size_t ramSize() const noexcept { return m_ram.size(); }

    [[nodiscard]] std::vector<uint8_t> exportRam() const { return m_ram; }
    bool importRam(std::span<const uint8_t> data);

    // True once after RAM changed (battery-backed only); clears the flag.
    bool takeDirty() noexcept {
        const bool dirty = m_dirty && m_hasBattery;
        m_dirty = false;
        return dirty;
    }

    [[nodiscard]] uint8_t readRom(uint16_t address) const;
    void writeControl(uint16_t address, uint8_t value);

    [[nodiscard]] uint8_t readRam(uint16_t address) const;
    void writeRam(uint16_t address, uint8_t value);

    void saveState(StateWriter& out) const;
    bool loadState(StateReader& in);

private:
    [[nodiscard]] size_t bankCount() const noexcept;
    [[nodiscard]] uint16_t effectiveRomBank(bool upperArea) const noexcept;

    Type m_type{Type::None};
    std::vector<uint8_t> m_rom;
    std::vector<uint8_t> m_ram;

    bool m_hasRam{false};
    bool m_hasBattery{false};
    bool m_dirty{false};
    bool m_ramEnabled{false};
    uint8_t m_romLow5{1};  // MBC1 low 5 bits of ROM bank
    uint16_t m_romBank{1}; // MBC5 full 9-bit ROM bank
    uint8_t m_ramBank{0};  // MBC1 high bits / RAM bank, MBC5 RAM bank
    uint8_t m_mode{0};     // MBC1 banking mode
};

} // namespace smile::systems::gameboy
