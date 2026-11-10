#include "stdafx.h"
#include <gtest/gtest.h>
#include "SpriteDecoder.h"
#include <vector>

class SpriteModes : public testing::TestWithParam<int> {};
TEST_P(SpriteModes, EveryWidthFullBitmapMemoryOrderWrappingAndGuards)
{
    const int mode = GetParam();
    constexpr uint32_t palette[5][4] = {
        {0,0xffffff,0,0xffffff}, {0,0x00ff00,0xff0000,0xffff00},
        {0xff,0x00ffff,0xff00ff,0xffffff}, {0,0xff0000,0x00ff00,0xffff00},
        {0xff,0xff00ff,0x00ffff,0xffffff}};
    std::array<uint8_t, 65536> memory{};
    for (size_t i = 0; i < memory.size(); ++i) memory[i] = static_cast<uint8_t>(i * 37 + i / 256);
    for (int width = 1; width <= 32; ++width)
    {
        SCOPED_TRACE(width);
        std::vector<uint32_t> pixels(sprite::Width * sprite::Height + 2, 0x12345678);
        int reads = 0;
        bool aligned = true, ordered = true;
        ASSERT_TRUE(sprite::Decode(std::span(pixels).subspan(1, sprite::Width * sprite::Height),
            65533, width, mode, [&](uint16_t address) {
                aligned &= (address & 1) == 0;
                ordered &= address == (static_cast<uint16_t>(65533 + reads) & ~1);
                ++reads;
                return static_cast<uint16_t>(memory[address] | (memory[address + 1] << 8));
            }));
        EXPECT_TRUE(aligned);
        EXPECT_TRUE(ordered);
        EXPECT_EQ(sprite::PageBytes(width, mode), reads);
        EXPECT_EQ(0x12345678, pixels.front());
        EXPECT_EQ(0x12345678, pixels.back());
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 262; ++x)
            {
                const int column = x / (width * 8 + 2);
                const int within = x % (width * 8 + 2);
                uint32_t expected = 0x7f7f7f7f;
                if (within < width * 8 && (column + 1) * width * 8 + column * 2 <= 262)
                {
                    const auto address = static_cast<uint16_t>(65533 +
                        (column * 256 * width + y * width + within / 8) * (mode == 0 ? 1 : 2));
                    const int low = (memory[address] >> (within % 8)) & 1;
                    const int high = mode == 0 ? 0 : (memory[static_cast<uint16_t>(address + 1)] >> (within % 8)) & 1;
                    expected = palette[mode][low + high * 2];
                }
                ASSERT_EQ(expected, pixels[1 + (255 - y) * 262 + x]) << x << ',' << y;
            }
    }
}
INSTANTIATE_TEST_SUITE_P(All, SpriteModes, testing::Range(0, 5));
TEST(SpriteDecoder, InvalidParametersDoNotReadOrWrite)
{
    std::vector<uint32_t> pixels(sprite::Width * sprite::Height, 42);
    int reads = 0;
    auto read = [&](uint16_t) { ++reads; return uint16_t{}; };
    for (int width : {-1, 0, 33, INT_MAX}) EXPECT_FALSE(sprite::Decode(pixels, 0, width, 0, read));
    for (int mode : {-1, 5, INT_MAX}) EXPECT_FALSE(sprite::Decode(pixels, 0, 1, mode, read));
    EXPECT_FALSE(sprite::Decode(std::span(pixels).first(pixels.size() - 1), 0, 1, 0, read));
    EXPECT_EQ(0, reads);
    EXPECT_TRUE(std::ranges::all_of(pixels, [](auto value) { return value == 42; }));
}
TEST(SpriteDecoder, ExactFitColumnsCountTowardPage)
{
    // Width 8: four 64-pixel columns and three two-pixel separators fill 262 pixels.
    EXPECT_EQ(4 * 256 * 8, sprite::PageBytes(8, 0));
    EXPECT_EQ(4 * 256 * 8 * 2, sprite::PageBytes(8, 1));
    EXPECT_EQ(256 * 32, sprite::PageBytes(32, 0));
    EXPECT_EQ(0, sprite::PageBytes(0, 0));
}
TEST(SpriteDecoder, EvenAndOddStartingBytes)
{
    std::vector<uint32_t> pixels(sprite::Width * sprite::Height);
    for (uint16_t address : {0, 1, 65534, 65535})
    {
        ASSERT_TRUE(sprite::Decode(pixels, address, 32, 0, [](uint16_t) { return uint16_t{0x8001}; }));
        for (int bit = 0; bit < 8; ++bit)
            EXPECT_EQ(bit == ((address & 1) ? 7 : 0) ? 0xffffffu : 0u, pixels[255 * 262 + bit]);
    }
}
