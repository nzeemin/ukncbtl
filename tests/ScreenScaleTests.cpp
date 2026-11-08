/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include <gtest/gtest.h>
#include "util/ScreenScale.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <span>
#include <vector>

namespace {
constexpr uint32_t sentinel = 0xdeadbeef;

bool Check(bool condition, const char* message, int mode)
{
    if (!condition) std::fprintf(stderr, "mode %d: %s\n", mode, message);
    return condition;
}

uint32_t Average(uint32_t a, uint32_t b)
{
    return ((a & 0xfefefeffu) + (b & 0xfefefeffu)) >> 1;
}
}

static int RunRegressionChecks()
{
    bool ok = true;
    const auto modes = screen_scale::Modes();
    const std::array<std::pair<int, int>, 8> expectedSizes{{
        {640, 288}, {640, 432}, {640, 576}, {640, 576},
        {960, 576}, {960, 720}, {1120, 864}, {1280, 864}}};
    if (modes.size() != expectedSizes.size()) return 1;
    ok &= Check(screen_scale::GetMode(-1).height == 432, "invalid mode uses default", -1);
    std::vector<uint32_t> source(UKNC_SCREEN_WIDTH * UKNC_SCREEN_HEIGHT);
    for (int y = 0; y < UKNC_SCREEN_HEIGHT; ++y)
        for (int x = 0; x < UKNC_SCREEN_WIDTH; ++x)
            source[y * UKNC_SCREEN_WIDTH + x] = 0x010000u | (static_cast<uint32_t>(y) << 8) | (x & 0xff);
    std::array<uint32_t, 1> shortBuffer{sentinel};
    ok &= Check(!screen_scale::Apply(0, shortBuffer, shortBuffer), "rejects undersized buffers", 0);
    ok &= Check(!screen_scale::Apply(0, source, shortBuffer), "rejects an undersized destination", 0);
    std::vector<uint32_t> fullDestination(source.size(), sentinel);
    ok &= Check(!screen_scale::Apply(0, shortBuffer, fullDestination), "rejects an undersized source", 0);
    ok &= Check(shortBuffer[0] == sentinel, "invalid input leaves destination unchanged", 0);

    for (size_t index = 0; index < modes.size(); ++index)
    {
        const int mode = static_cast<int>(index);
        const auto& info = modes[index];
        ok &= Check(info.width == expectedSizes[index].first && info.height == expectedSizes[index].second,
            "dimensions", mode);
        const size_t count = static_cast<size_t>(info.width) * info.height;
        std::vector<uint32_t> guarded(count + 2, sentinel);
        auto pixels = std::span{guarded}.subspan(1, count);
        ok &= Check(screen_scale::Apply(mode, source, pixels), "scaling succeeds", mode);
        ok &= Check(guarded.front() == sentinel && guarded.back() == sentinel, "does not write outside destination", mode);
        ok &= Check(pixels.front() == source[(UKNC_SCREEN_HEIGHT - 1) * UKNC_SCREEN_WIDTH], "first pixel", mode);

        if (mode == 0)
        {
            for (int y = 0; y < UKNC_SCREEN_HEIGHT; ++y)
                ok &= Check(pixels[static_cast<size_t>(y) * 640 + 639] == source[(287 - y) * 640 + 639], "right edge", mode);
        }
        else if (mode == 1)
        {
            ok &= Check(pixels[640] == Average(source[287 * 640], source[286 * 640]), "interpolated row", mode);
        }
        else if (mode == 2 || mode == 4 || mode == 6 || mode == 7)
        {
            const int rowFactor = mode == 6 || mode == 7 ? 3 : 2;
            const int blackRow = rowFactor - 1;
            for (int y = 0; y < UKNC_SCREEN_HEIGHT; ++y)
            {
                const size_t row = static_cast<size_t>(y) * rowFactor * info.width;
                const auto black = pixels.subspan(row + static_cast<size_t>(blackRow) * info.width, info.width);
                ok &= Check(std::ranges::all_of(black, [](uint32_t pixel) { return pixel == 0; }), "black interlace row", mode);
                const uint32_t lastSource = source[(287 - y) * 640 + 639];
                ok &= Check(pixels[row + info.width - 1] == lastSource, "rightmost image pixel", mode);
            }
        }
        else if (mode == 3)
        {
            ok &= Check(std::equal(pixels.begin(), pixels.begin() + 640, pixels.begin() + 640), "doubled row", mode);
        }
        else if (mode == 5)
        {
            ok &= Check(pixels[960] == pixels[0], "first row repeated", mode);
            ok &= Check(pixels[2 * 960] == Average(source[287 * 640], source[286 * 640]), "vertical interpolation", mode);
        }
    }
    return ok ? 0 : 1;
}

TEST(ScreenScaleTests, RegressionChecks)
{
    EXPECT_EQ(RunRegressionChecks(), 0);
}
