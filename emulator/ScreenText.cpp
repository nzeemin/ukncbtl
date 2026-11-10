/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include "ScreenText.h"
namespace screen_text {
std::optional<uint8_t> Recognize(std::span<const uint8_t> fontcur, std::span<const uint8_t> fontstd, std::span<const uint32_t> pixels)
{
    if (fontcur.size() < FontSize || fontstd.size() < FontSize || pixels.size() < 640 * 10 + 8) return std::nullopt;
    const auto* pBits = pixels.data();
    int16_t bestmatch = -32767;
    uint8_t bestchar = 0;
    for (uint8_t charidx = 0; charidx < 16 * 14; charidx++)
    {
        int16_t matchcur = 0;
        int16_t matchstd = 0;
        const uint32_t * pb = pBits;
        for (int16_t y = 0; y < 11; y++)
        {
            uint8_t fontcurdata = fontcur[charidx * 11 + y];
            uint8_t fontstddata = fontstd[charidx * 11 + y];
            for (int x = 0; x < 8; x++)
            {
                uint32_t color = pb[x];
                int sum = (color & 0xff) + ((color >> 8) & 0xff) + ((color >> 16) & 0xff);
                uint8_t fontcurbit = (fontcurdata >> x) & 1;
                uint8_t fontstdbit = (fontstddata >> x) & 1;
                if (sum > 384)
                {
                    matchcur += fontcurbit;  matchstd += fontstdbit;
                }
                else
                {
                    matchcur -= fontcurbit;  matchstd -= fontstdbit;
                }
            }
            pb += 640;
        }
        if (matchcur > bestmatch)
        {
            bestmatch = matchcur;
            bestchar = charidx;
        }
        if (matchstd > bestmatch)
        {
            bestmatch = matchstd;
            bestchar = charidx;
        }
    }

    return 0x20 + bestchar;
}

bool Convert(std::span<const uint8_t> current, std::span<const uint8_t> standard,
    std::span<const uint32_t> pixels, std::span<uint8_t> output)
{
    if (current.size() < FontSize || standard.size() < FontSize ||
        pixels.size() < FrameSize || output.size() < OutputSize) return false;
    size_t index = 0;
    for (size_t row = 0; row < 26; ++row)
    {
        const size_t y = row * 11 + (row > 0 ? 1 : 0) + (row == 25 ? 1 : 0);
        for (size_t x = 0; x < 640; x += 8)
            output[index++] = *Recognize(current, standard, pixels.subspan(y * 640 + x));
        output[index++] = '\r';
        output[index++] = '\n';
    }
    output[index] = 0;
    return true;
}
}
