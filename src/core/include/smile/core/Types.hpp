#pragma once

#include <cstdint>
#include <concepts>
#include <bit>

namespace smile::core {

// Fixed-width integer aliases for readability in hardware-level code
using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using s8  = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

// Dedicated type for tracking cycle counts
using Cycles = std::uint64_t;

// Bit manipulation helpers optimized for constexpr / zero overhead
template <std::integral T>
[[nodiscard]] constexpr bool testBit(T value, unsigned int bit) noexcept {
    return (value & (T{1} << bit)) != 0;
}

template <std::integral T>
constexpr void setBit(T& value, unsigned int bit) noexcept {
    value |= static_cast<T>(T{1} << bit);
}

template <std::integral T>
constexpr void clearBit(T& value, unsigned int bit) noexcept {
    value &= static_cast<T>(~(T{1} << bit));
}

template <std::integral T>
constexpr void setBitVal(T& value, unsigned int bit, bool set) noexcept {
    if (set) {
        setBit(value, bit);
    } else {
        clearBit(value, bit);
    }
}

template <std::integral T>
[[nodiscard]] constexpr T bitSlice(T value, unsigned int startBit, unsigned int length) noexcept {
    const T mask = static_cast<T>((T{1} << length) - 1);
    return static_cast<T>((value >> startBit) & mask);
}

} // namespace smile::core
