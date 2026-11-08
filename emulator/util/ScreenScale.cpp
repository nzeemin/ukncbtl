/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include "ScreenScale.h"
#include <algorithm>
#include <array>

namespace screen_scale {
namespace {
constexpr std::array<Mode, 8> modes{{
    {640, 288, _T("640 x 288 Standard")},
    {640, 432, _T("640 x 432 Upscaled to 1.5")},
    {640, 576, _T("640 x 576 Interlaced")},
    {640, 576, _T("640 x 576 Doubled")},
    {960, 576, _T("960 x 576 Interlaced")},
    {960, 720, _T("960 x 720, 4:3")},
    {1120, 864, _T("1120 x 864 Interlaced")},
    {1280, 864, _T("1280 x 864 Interlaced")},
}};

constexpr uint32_t AverageRgb(uint32_t a, uint32_t b)
{
    return ((a & 0xfefefeffu) + (b & 0xfefefeffu)) >> 1;
}

static void PrepareScreenCopy(std::span<const uint32_t> source, std::span<uint32_t> destination)
{
    for (int line = 0; line < UKNC_SCREEN_HEIGHT; line++)
    {
        const uint32_t* pSrc = source.data() + UKNC_SCREEN_WIDTH * (UKNC_SCREEN_HEIGHT - 1 - line);
        uint32_t* pDest = destination.data() + UKNC_SCREEN_WIDTH * line;
        ::memcpy(pDest, pSrc, UKNC_SCREEN_WIDTH * 4);
    }
}

// Upscale screen from height 288 to 432
static void PrepareScreenUpscale(std::span<const uint32_t> source, std::span<uint32_t> destination)
{
    int ukncline = 0;
    for (int line = 0; line < 432; line++)
    {
        uint32_t* pdest = destination.data() + line * UKNC_SCREEN_WIDTH;
        if (line % 3 == 1)
        {
            const uint32_t* psrc1 = source.data() + ((UKNC_SCREEN_HEIGHT - 1 - ukncline) + 1) * UKNC_SCREEN_WIDTH;
            const uint32_t* psrc2 = source.data() + ((UKNC_SCREEN_HEIGHT - 1 - ukncline) + 0) * UKNC_SCREEN_WIDTH;
            uint32_t* pdst1 = pdest;
            for (int i = 0; i < UKNC_SCREEN_WIDTH; i++)
            {
                *pdst1 = AverageRgb(*psrc1, *psrc2);
                psrc1++;  psrc2++;  pdst1++;
            }
        }
        else
        {
            const uint32_t* psrc = source.data() + (UKNC_SCREEN_HEIGHT - 1 - ukncline) * UKNC_SCREEN_WIDTH;
            memcpy(pdest, psrc, UKNC_SCREEN_WIDTH * 4);
            ukncline++;
        }
    }
}

// Upscale screen to twice height with "interlaced" effect
static void PrepareScreenUpscale2(std::span<const uint32_t> source, std::span<uint32_t> destination)
{
    for (int ukncline = 0; ukncline < UKNC_SCREEN_HEIGHT; ukncline++)
    {
        const uint32_t* psrc = source.data() + (UKNC_SCREEN_HEIGHT - 1 - ukncline) * UKNC_SCREEN_WIDTH;
        uint32_t* pdest = destination.data() + (ukncline * 2) * UKNC_SCREEN_WIDTH;
        memcpy(pdest, psrc, UKNC_SCREEN_WIDTH * 4);

        pdest += UKNC_SCREEN_WIDTH;
        memset(pdest, 0, UKNC_SCREEN_WIDTH * 4);
    }
}

// Upscale screen to twice height
static void PrepareScreenUpscale2d(std::span<const uint32_t> source, std::span<uint32_t> destination)
{
    for (int ukncline = 0; ukncline < UKNC_SCREEN_HEIGHT; ukncline++)
    {
        const uint32_t* psrc = source.data() + (UKNC_SCREEN_HEIGHT - 1 - ukncline) * UKNC_SCREEN_WIDTH;
        uint32_t* pdest = destination.data() + (ukncline * 2) * UKNC_SCREEN_WIDTH;
        memcpy(pdest, psrc, UKNC_SCREEN_WIDTH * 4);

        pdest += UKNC_SCREEN_WIDTH;
        memcpy(pdest, psrc, UKNC_SCREEN_WIDTH * 4);
    }
}

// Upscale screen width 640->960, height 288->720
static void PrepareScreenUpscale4(std::span<const uint32_t> source, std::span<uint32_t> destination)
{
    for (int ukncline = 0; ukncline < UKNC_SCREEN_HEIGHT; ukncline += 2)
    {
        const uint32_t* psrc1 = source.data() + (UKNC_SCREEN_HEIGHT - 1 - ukncline) * UKNC_SCREEN_WIDTH;
        const uint32_t* psrc2 = psrc1 - UKNC_SCREEN_WIDTH;
        uint32_t* pdest0 = destination.data() + ukncline / 2 * 5 * 960;
        uint32_t* pdest1 = pdest0 + 960;
        uint32_t* pdest2 = pdest1 + 960;
        uint32_t* pdest3 = pdest2 + 960;
        uint32_t* pdest4 = pdest3 + 960;
        for (int i = 0; i < UKNC_SCREEN_WIDTH / 2; i++)
        {
            uint32_t c1a = *(psrc1++);  uint32_t c1b = *(psrc1++);
            uint32_t c2a = *(psrc2++);  uint32_t c2b = *(psrc2++);
            uint32_t c1 = AverageRgb(c1a, c1b);
            uint32_t c2 = AverageRgb(c2a, c2b);
            uint32_t ca = AverageRgb(c1a, c2a);
            uint32_t cb = AverageRgb(c1b, c2b);
            uint32_t c  = AverageRgb(ca,  cb);
            (*pdest0++) = c1a;  (*pdest0++) = c1;  (*pdest0++) = c1b;
            (*pdest1++) = c1a;  (*pdest1++) = c1;  (*pdest1++) = c1b;
            (*pdest2++) = ca;   (*pdest2++) = c;   (*pdest2++) = cb;
            (*pdest3++) = c2a;  (*pdest3++) = c2;  (*pdest3++) = c2b;
            (*pdest4++) = c2a;  (*pdest4++) = c2;  (*pdest4++) = c2b;
        }
    }
}

// Upscale screen width 640->960, height 288->576 with "interlaced" effect
static void PrepareScreenUpscale3(std::span<const uint32_t> source, std::span<uint32_t> destination)
{
    for (int ukncline = 0; ukncline < UKNC_SCREEN_HEIGHT; ++ukncline)
    {
        const uint32_t* psrc = source.data() + (UKNC_SCREEN_HEIGHT - 1 - ukncline) * UKNC_SCREEN_WIDTH;
        uint32_t* pdest = destination.data() + (ukncline * 2) * 960;
        for (int i = 0; i < UKNC_SCREEN_WIDTH / 2; ++i)
        {
            const uint32_t c1 = *psrc++;
            const uint32_t c2 = *psrc++;
            *pdest++ = c1;
            *pdest++ = AverageRgb(c1, c2);
            *pdest++ = c2;
        }
        std::fill_n(pdest, 960, 0);
    }
}

// Upscale screen width 640->1120 (x1.75), height 288->864 (x3) with "interlaced" effect
static void PrepareScreenUpscale175(std::span<const uint32_t> source, std::span<uint32_t> destination)
{
    for (int ukncline = 0; ukncline < UKNC_SCREEN_HEIGHT; ukncline++)
    {
        const uint32_t* psrc = source.data() + (UKNC_SCREEN_HEIGHT - 1 - ukncline) * UKNC_SCREEN_WIDTH;
        uint32_t* pdest1 = destination.data() + ukncline * 3 * 1120;
        uint32_t* pdest2 = pdest1 + 1120;
        for (int i = 0; i < UKNC_SCREEN_WIDTH / 4; i++)
        {
            uint32_t c1 = *(psrc++);
            uint32_t c2 = *(psrc++);
            uint32_t c3 = *(psrc++);
            uint32_t c4 = *(psrc++);

            *(pdest1++) = *(pdest2++) = c1;
            *(pdest1++) = *(pdest2++) = AverageRgb(c1, c2);
            *(pdest1++) = *(pdest2++) = c2;
            *(pdest1++) = *(pdest2++) = AverageRgb(c2, c3);
            *(pdest1++) = *(pdest2++) = c3;
            *(pdest1++) = *(pdest2++) = AverageRgb(c3, c4);
            *(pdest1++) = *(pdest2++) = c4;
        }
        std::fill_n(pdest2, 1120, 0);
    }
}

// Upscale screen width 640->1280, height 288->864 with "interlaced" effect
static void PrepareScreenUpscale5(std::span<const uint32_t> source, std::span<uint32_t> destination)
{
    for (int ukncline = 0; ukncline < UKNC_SCREEN_HEIGHT; ukncline++)
    {
        const uint32_t* psrc = source.data() + (UKNC_SCREEN_HEIGHT - 1 - ukncline) * UKNC_SCREEN_WIDTH;
        uint32_t* pdest = destination.data() + ukncline * 3 * 1280;
        uint32_t* pdest2 = pdest + 1280;
        for (int i = 0; i < UKNC_SCREEN_WIDTH; ++i)
        {
            const uint32_t color = *psrc++;
            *pdest++ = color;
            *pdest++ = color;
            *pdest2++ = color;
            *pdest2++ = color;
        }
        std::fill_n(pdest2, 1280, 0);
    }
}


size_t ModeIndex(int mode)
{
    return mode >= 0 && static_cast<size_t>(mode) < modes.size() ? static_cast<size_t>(mode) : 1;
}
}

std::span<const Mode> Modes() { return modes; }
const Mode& GetMode(int mode) { return modes[ModeIndex(mode)]; }

bool Apply(int mode, std::span<const uint32_t> source, std::span<uint32_t> destination)
{
    const size_t index = ModeIndex(mode);
    const auto& selected = modes[index];
    if (source.size() < static_cast<size_t>(UKNC_SCREEN_WIDTH) * UKNC_SCREEN_HEIGHT ||
        destination.size() < static_cast<size_t>(selected.width) * selected.height)
        return false;

    switch (index)
    {
    case 0: PrepareScreenCopy(source, destination); break;
    case 1: PrepareScreenUpscale(source, destination); break;
    case 2: PrepareScreenUpscale2(source, destination); break;
    case 3: PrepareScreenUpscale2d(source, destination); break;
    case 4: PrepareScreenUpscale3(source, destination); break;
    case 5: PrepareScreenUpscale4(source, destination); break;
    case 6: PrepareScreenUpscale175(source, destination); break;
    case 7: PrepareScreenUpscale5(source, destination); break;
    }
    return true;
}
}
