#pragma once

#include <cstdint>
#include <type_traits>

namespace smile::core {

enum class Button : std::uint32_t {
    None    = 0,
    Right   = 1 << 0,
    Left    = 1 << 1,
    Up      = 1 << 2,
    Down    = 1 << 3,
    A       = 1 << 4,
    B       = 1 << 5,
    Select  = 1 << 6,
    Start   = 1 << 7,

    // Extra buttons for next-generation systems (NES / Mega Drive / SNES)
    C       = 1 << 8,
    X       = 1 << 9,
    Y       = 1 << 10,
    Z       = 1 << 11,
    L       = 1 << 12,
    R       = 1 << 13,
    Mode    = 1 << 14,
};

[[nodiscard]] constexpr Button operator|(Button lhs, Button rhs) noexcept {
    using Underlying = std::underlying_type_t<Button>;
    return static_cast<Button>(static_cast<Underlying>(lhs) | static_cast<Underlying>(rhs));
}

[[nodiscard]] constexpr Button operator&(Button lhs, Button rhs) noexcept {
    using Underlying = std::underlying_type_t<Button>;
    return static_cast<Button>(static_cast<Underlying>(lhs) & static_cast<Underlying>(rhs));
}

[[nodiscard]] constexpr Button operator^(Button lhs, Button rhs) noexcept {
    using Underlying = std::underlying_type_t<Button>;
    return static_cast<Button>(static_cast<Underlying>(lhs) ^ static_cast<Underlying>(rhs));
}

[[nodiscard]] constexpr Button operator~(Button btn) noexcept {
    using Underlying = std::underlying_type_t<Button>;
    return static_cast<Button>(~static_cast<Underlying>(btn));
}

constexpr Button& operator|=(Button& lhs, Button rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

constexpr Button& operator&=(Button& lhs, Button rhs) noexcept {
    lhs = lhs & rhs;
    return lhs;
}

} // namespace smile::core
