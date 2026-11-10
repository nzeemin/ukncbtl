/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once
#include <cstdint>
#include <span>

namespace ram_changes
{
inline constexpr size_t PlaneSize = 65536;
// Flags are changed bits (XOR), not booleans. Each update replaces the baseline.
template<class ReadByte>
bool Update(std::span<uint8_t> previous, std::span<uint8_t> changed, ReadByte read)
{
    if (previous.size() < PlaneSize || changed.size() < PlaneSize) return false;
    for (size_t address=0; address<PlaneSize; ++address)
    {
        const uint8_t current = read(static_cast<uint16_t>(address));
        changed[address] = current ^ previous[address];
        previous[address] = current;
    }
    return true;
}
inline uint16_t Word(std::span<const uint8_t> changed,uint16_t address)
{
    if (changed.size() < PlaneSize) return 0;
    const size_t aligned = address & ~1u;
    return static_cast<uint16_t>(changed[aligned] | (changed[aligned+1] << 8));
}
inline uint16_t Interleaved(std::span<const uint8_t> low,std::span<const uint8_t> high,uint16_t address)
{
    if (low.size() < PlaneSize || high.size() < PlaneSize) return 0;
    return static_cast<uint16_t>(low[address/2] | (high[address/2] << 8));
}
}
