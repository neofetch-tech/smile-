#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace smile::systems::nes {

enum class NesMirror : uint8_t { Horizontal, Vertical };

// NES cartridge: NROM (mapper 0) and UNROM (mapper 2, Contra's board).
// CHR is ROM or RAM (RAM when the iNES image carries zero CHR units).
class NesCartridge {
public:
    enum class LoadResult : uint8_t { Ok, BadFormat, BadMapper };

    NesCartridge() = default;

    LoadResult load(std::span<const uint8_t> romData);

    [[nodiscard]] int mapper() const noexcept { return m_mapper; }
    [[nodiscard]] NesMirror mirror() const noexcept { return m_mirror; }
    [[nodiscard]] bool hasRom() const noexcept { return !m_prg.empty(); }

    [[nodiscard]] uint8_t prgRead(uint16_t address) const;
    void prgWrite(uint16_t address, uint8_t value); // UNROM bank register

    [[nodiscard]] uint8_t chrRead(uint16_t address) const;
    void chrWrite(uint16_t address, uint8_t value); // CHR-RAM only

private:
    int m_mapper{-1};
    NesMirror m_mirror{NesMirror::Horizontal};
    std::vector<uint8_t> m_prg;
    std::vector<uint8_t> m_chr;
    bool m_chrRam{false};
    uint8_t m_prgBank{0}; // UNROM: 16 KB bank at $8000 (low 4 bits)
};

} // namespace smile::systems::nes
