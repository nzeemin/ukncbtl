/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace sprite
{
inline constexpr int Width = 262, Height = 256;
inline constexpr std::array<std::array<uint32_t, 3>, 5> Palettes{{
    {0, 0xffffff, 0}, {0, 0x00ff00, 0xff0000}, {0xff, 0x00ffff, 0xff00ff},
    {0, 0xff0000, 0x00ff00}, {0xff, 0xff00ff, 0x00ffff}}};
inline int PageBytes(int width, int mode)
{
    if (width < 1 || width > 32 || mode < 0 || mode > 4) return 0;
    return (Width + 2) / (width * 8 + 2) * Height * width * (mode == 0 ? 1 : 2);
}
// readWord receives an aligned CPU address. Pixels are bottom-up DIB order.
// Invalid parameters leave the output untouched and perform no memory reads.
template<class ReadWord>
bool Decode(std::span<uint32_t> pixels, uint16_t address, int width, int mode, ReadWord readWord)
{
    if (pixels.size() < Width * Height || PageBytes(width, mode) == 0) return false;
    std::fill_n(pixels.begin(), Width * Height, 0x7f7f7f7fu);
    const auto readByte = [&]() {
        const auto word = readWord(static_cast<uint16_t>(address & ~1));
        const auto value = static_cast<uint8_t>(word >> ((address & 1) * 8));
        ++address;
        return value;
    };
    for (int x = 0; x + width * 8 <= Width; x += width * 8 + 2)
        for (int y = 0; y < Height; ++y)
            for (int w = 0; w < width; ++w)
            {
                const auto first = readByte();
                const auto second = mode == 0 ? 0 : readByte();
                for (int bit = 0; bit < 8; ++bit)
                    pixels[(Height - 1 - y) * Width + x + w * 8 + bit] = Palettes[mode][0] |
                        ((first & (1 << bit)) ? Palettes[mode][1] : 0) |
                        ((second & (1 << bit)) ? Palettes[mode][2] : 0);
            }
    return true;
}
}
