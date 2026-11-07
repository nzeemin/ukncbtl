/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <span>

namespace little_endian {
static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big);
template <std::unsigned_integral T>
    requires (!std::same_as<T, bool>)
[[nodiscard]] T read(std::span<const uint8_t, sizeof(T)> bytes)
{
    std::array<uint8_t, sizeof(T)> raw{};
    std::ranges::copy(bytes, raw.begin());
    auto value = std::bit_cast<T>(raw);
    if constexpr (std::endian::native == std::endian::big)
        value = std::byteswap(value);
    return value;
}
template <std::unsigned_integral T>
    requires (!std::same_as<T, bool>)
void write(std::span<uint8_t, sizeof(T)> bytes, T value)
{
    if constexpr (std::endian::native == std::endian::big)
        value = std::byteswap(value);
    const auto raw = std::bit_cast<std::array<uint8_t, sizeof(T)>>(value);
    std::ranges::copy(raw, bytes.begin());
}
}
