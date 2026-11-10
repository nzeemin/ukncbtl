#include "stdafx.h"
#include <gtest/gtest.h>
#include "ScreenText.h"
#include <algorithm>
#include <array>
#include <vector>

class ScreenText : public testing::Test
{
protected:
    std::array<uint8_t, screen_text::FontSize> current{}, standard{};
    std::vector<uint32_t> pixels = std::vector<uint32_t>(screen_text::FrameSize);
};
TEST_F(ScreenText, BlankAndTiesChooseFirstCharacter)
{
    EXPECT_EQ(0x20, screen_text::Recognize(current, standard, pixels));
    current[11] = current[22] = 1;
    pixels[0] = 0xffffff;
    EXPECT_EQ(0x21, screen_text::Recognize(current, standard, pixels));
}
TEST_F(ScreenText, EveryCharacterFromEachFontIncludingLast)
{
    pixels[10 * 640 + 7] = 0xffffff;
    for (auto* font : {&current, &standard})
        for (size_t index = 0; index < 224; ++index)
        {
            font->fill(0);
            (*font)[index * 11 + 10] = 0x80;
            EXPECT_EQ(0x20 + index, screen_text::Recognize(current, standard, pixels)) << index;
        }
}
TEST_F(ScreenText, PixelBitOrientationAndStride)
{
    for (int x = 0; x < 8; ++x)
    {
        current.fill(0);
        std::fill(pixels.begin(), pixels.end(), 0);
        current[11 + 5] = static_cast<uint8_t>(1 << x);
        current[22 + 5] = static_cast<uint8_t>(1 << (7 - x));
        pixels[5 * 640 + x] = 0xffffff;
        EXPECT_EQ(0x21, screen_text::Recognize(current, standard, pixels));
    }
}
TEST_F(ScreenText, BrightnessThresholdIgnoresAlpha)
{
    current[11] = 1;
    for (auto color : {0x00808080u, 0xff808080u, 0xff000000u})
    {
        pixels[0] = color;
        EXPECT_EQ(0x20, screen_text::Recognize(current, standard, pixels));
    }
    for (auto color : {0x00818080u, 0xff818080u, 0x00ff8200u})
    {
        pixels[0] = color;
        EXPECT_EQ(0x21, screen_text::Recognize(current, standard, pixels));
    }
}
TEST_F(ScreenText, DarkPixelsPenalizeExtraFontBits)
{
    current[11] = 3;
    standard[22] = 1;
    pixels[0] = 0xffffff;
    EXPECT_EQ(0x22, screen_text::Recognize(current, standard, pixels));
}
TEST_F(ScreenText, EqualScoresAcrossFontsUseEarlierCharacter)
{
    current[22] = 1;
    standard[11] = 1;
    pixels[0] = 0xffffff;
    EXPECT_EQ(0x21, screen_text::Recognize(current, standard, pixels));
}
TEST_F(ScreenText, GlyphSpanMinimumAndShortInputs)
{
    EXPECT_FALSE(screen_text::Recognize({}, standard, pixels));
    EXPECT_FALSE(screen_text::Recognize(current, {}, pixels));
    EXPECT_FALSE(screen_text::Recognize(current, standard, std::span(pixels).first(640 * 10 + 7)));
    EXPECT_EQ(0x20, screen_text::Recognize(current, standard, std::span(pixels).first(640 * 10 + 8)));
    EXPECT_FALSE(screen_text::Recognize(std::span(current).first(current.size() - 1), standard, pixels));
}
TEST_F(ScreenText, InvalidSpansLeaveOutputUntouched)
{
    std::vector<uint8_t> output(screen_text::OutputSize, 0xcd);
    EXPECT_FALSE(screen_text::Convert(current, standard, pixels, std::span(output).first(output.size() - 1)));
    EXPECT_FALSE(screen_text::Convert(current, standard, std::span(pixels).first(pixels.size() - 1), output));
    EXPECT_FALSE(screen_text::Convert({}, standard, pixels, output));
    EXPECT_FALSE(screen_text::Convert(current, {}, pixels, output));
    EXPECT_TRUE(std::ranges::all_of(output, [](auto value) { return value == 0xcd; }));
}
TEST_F(ScreenText, FullScreenRowsGapsCrLfTerminatorAndGuards)
{
    current[('A' - 0x20) * 11] = 1;
    // The indicator gaps are not text rows.
    std::fill_n(pixels.begin() + 11 * 640, 640, 0xffffff);
    std::fill_n(pixels.begin() + 276 * 640, 640, 0xffffff);
    for (size_t row = 0; row < 26; ++row)
    {
        const size_t y = row == 0 ? 0 : row == 25 ? 277 : 1 + row * 11;
        for (size_t col = 0; col < 80; ++col)
            if ((row + col) % 3 == 0) pixels[y * 640 + col * 8] = 0xffffff;
    }
    std::vector<uint8_t> output(screen_text::OutputSize + 2, 0xcd);
    ASSERT_TRUE(screen_text::Convert(current, standard, pixels, std::span(output).subspan(1, screen_text::OutputSize)));
    EXPECT_EQ(0xcd, output.front());
    EXPECT_EQ(0xcd, output.back());
    EXPECT_EQ(0, output[screen_text::OutputSize]);
    for (size_t row = 0; row < 26; ++row)
    {
        for (size_t col = 0; col < 80; ++col)
            EXPECT_EQ((row + col) % 3 == 0 ? 'A' : ' ', output[1 + row * 82 + col]) << row << ':' << col;
        EXPECT_EQ('\r', output[1 + row * 82 + 80]);
        EXPECT_EQ('\n', output[1 + row * 82 + 81]);
    }
}
