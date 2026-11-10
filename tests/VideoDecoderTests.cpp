/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include "VideoDecoder.h"
#include "emubase/Emubase.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <vector>

#ifdef _DEBUG
int AssertFailedLine(const char*, int) { std::abort(); }
#endif
void DebugLog(LPCTSTR) {}
const TCHAR* REGISTER_NAME[] = {_T("R0"), _T("R1"), _T("R2"), _T("R3"),
                               _T("R4"), _T("R5"), _T("SP"), _T("PC")};

namespace {
constexpr uint32_t Guard = 0xdeadbeef;
constexpr size_t FrameSize = video::Width * video::Height;

class VideoDecoderTests : public testing::Test
{
protected:
    std::unique_ptr<CMotherboard> board;
    std::array<video::TagKind, video::TotalLines> kinds{};
    std::array<uint32_t, 128> colors{};
    std::vector<uint32_t> storage;
    std::span<uint32_t> pixels;

    static uint16_t TagAddress(int line)
    { return line == 0 ? 0000270 : static_cast<uint16_t>(0x1000 + line * 8); }
    static uint16_t BitsAddress(int line)
    { return static_cast<uint16_t>(0x8000 + line * 80); }
    static uint16_t Flags(video::TagKind kind)
    { return kind == video::TagKind::Short ? 0 : kind == video::TagKind::Parameters ? 2 : 6; }
    uint16_t LinkAddress(int line) const
    { return TagAddress(line) + (kinds[line] == video::TagKind::Short ? 2 : 6); }
    void Tag(int line, video::TagKind kind, uint16_t first = 0, uint16_t second = 0)
    {
        kinds[line] = kind;
        uint16_t address = TagAddress(line);
        if (kind != video::TagKind::Short)
        {
            board->SetRAMWord(0, address, first);
            board->SetRAMWord(0, address + 2, second);
            address += 4;
        }
        board->SetRAMWord(0, address, BitsAddress(line));
        const int next = line + 1 < video::TotalLines ? line + 1 : line;
        board->SetRAMWord(0, address + 2, TagAddress(next) | Flags(kinds[next]));
        if (line > 0)
        {
            const auto previous = LinkAddress(line - 1);
            const auto toggle = board->GetRAMWord(0, previous) & 1;
            board->SetRAMWord(0, previous, TagAddress(line) | Flags(kind) | toggle);
        }
    }
    void Toggle(int line)
    {
        const auto address = LinkAddress(line);
        board->SetRAMWord(0, address, board->GetRAMWord(0, address) ^ 1);
    }
    void Parameters(int shift, int brightness = 0, uint16_t cursor = 0)
    { Tag(1, video::TagKind::Parameters, cursor, static_cast<uint16_t>((shift << 4) | (7 - brightness))); }
    void SetBits(int line, uint8_t p0, uint8_t p1, uint8_t p2)
    {
        for (int offset = 0; offset < 80; ++offset)
        {
            const auto address = static_cast<uint16_t>(BitsAddress(line) + offset);
            board->SetRAMByte(0, address, p0);
            board->SetRAMByte(1, address, p1);
            board->SetRAMByte(2, address, p2);
        }
    }
    void SetUp() override
    {
        CProcessor::Init();
        board = std::make_unique<CMotherboard>();
        board->Reset();
        storage.assign(FrameSize + 3, Guard);
        pixels = std::span{storage}.subspan(1, FrameSize + 1);
        for (size_t i = 0; i < colors.size(); ++i) colors[i] = 0x10000000 + static_cast<uint32_t>(i);
        kinds.fill(video::TagKind::Short);
        for (int line = 0; line < video::TotalLines; ++line)
        {
            Tag(line, video::TagKind::Short);
            SetBits(line, 0xaa, 0xcc, 0xf0); // Low bit first gives indices 0..7.
        }
        Parameters(0);
        Tag(2, video::TagKind::Palette, 0x3210, 0x7654);
    }
    void TearDown() override { board.reset(); CProcessor::Done(); }
    void ExpectGuards()
    {
        EXPECT_EQ(storage.front(), Guard);
        EXPECT_EQ(storage.back(), Guard);
        EXPECT_EQ(pixels[FrameSize], Guard);
    }
};

TEST_F(VideoDecoderTests, CursorDecodesShortParameterAndPaletteTags)
{
    video::DisplayListCursor cursor;
    const auto first = cursor.Next(*board);
    EXPECT_EQ(first.address, 0000270);
    EXPECT_EQ(first.kind, video::TagKind::Short);
    EXPECT_EQ(first.tag1, 0);
    EXPECT_EQ(first.tag2, 0);
    EXPECT_EQ(first.pixels, BitsAddress(0));
    EXPECT_EQ(first.NextAddress(), TagAddress(1));
    EXPECT_EQ(first.NextKind(), video::TagKind::Parameters);
    const auto parameters = cursor.Next(*board);
    EXPECT_EQ(parameters.kind, video::TagKind::Parameters);
    EXPECT_EQ(parameters.tag2, 7);
    EXPECT_EQ(parameters.Scale(), 1);
    EXPECT_EQ(parameters.NextKind(), video::TagKind::Palette);
    const auto palette = cursor.Next(*board);
    EXPECT_EQ(palette.tag1, 0x3210);
    EXPECT_EQ(palette.tag2, 0x7654);
    EXPECT_EQ(palette.kind, video::TagKind::Palette);
    EXPECT_EQ(palette.NextKind(), video::TagKind::Short);
    EXPECT_EQ(cursor.Next(*board).kind, video::TagKind::Short);
}

TEST_F(VideoDecoderTests, LinkFlagsDoNotLeakIntoAddresses)
{
    for (uint16_t flags = 0; flags < 8; ++flags)
    {
        const video::DisplayTag tag{0, video::TagKind::Short, 0, 0, 0, static_cast<uint16_t>(0xfff8 | flags)};
        EXPECT_EQ(tag.NextAddress(), (flags & 2) ? 0xfff8 : (0xfff8 | (flags & 4)));
        EXPECT_EQ(tag.ToggleCursor(), (flags & 1) != 0);
        EXPECT_EQ(tag.NextKind(), !(flags & 2) ? video::TagKind::Short :
                  (flags & 4) ? video::TagKind::Palette : video::TagKind::Parameters);
    }
}

TEST_F(VideoDecoderTests, CursorFollowsEndOfRamBackToZero)
{
    board->SetRAMWord(0, 0000272, 0xfffe); // Four-word palette tag at fff8.
    board->SetRAMWord(0, 0xfff8, 0x1234);
    board->SetRAMWord(0, 0xfffa, 0x5678);
    board->SetRAMWord(0, 0xfffc, 0xabcd);
    board->SetRAMWord(0, 0xfffe, 0); // Next two-word tag at zero.
    board->SetRAMWord(0, 0, 0xbeef);
    board->SetRAMWord(0, 2, 0000270);
    video::DisplayListCursor cursor;
    cursor.Next(*board);
    const auto end = cursor.Next(*board);
    EXPECT_EQ(end.address, 0xfff8);
    EXPECT_EQ(end.tag1, 0x1234);
    EXPECT_EQ(end.tag2, 0x5678);
    EXPECT_EQ(end.pixels, 0xabcd);
    EXPECT_EQ(cursor.Next(*board).pixels, 0xbeef);
}

class ScaledVideoTests : public VideoDecoderTests, public testing::WithParamInterface<int> {};
TEST_P(ScaledVideoTests, RgbExpandsAllPlanesAcrossEntireFrame)
{
    Parameters(GetParam());
    ASSERT_TRUE(video::RenderRgb(*board, pixels, colors));
    const int scale = 1 << GetParam();
    for (int row = 0; row < video::Height; ++row)
        for (int x = 0; x < video::Width; ++x)
            ASSERT_EQ(pixels[row * video::Width + x], colors[(x / scale) % 8]) << row << "," << x;
    ExpectGuards();
}

TEST_P(ScaledVideoTests, TextDoesNotExpandPixelsAndPreservesRowTails)
{
    Parameters(GetParam(), 5, 15);
    Tag(2, video::TagKind::Palette, 0xffff, 0xffff);
    Toggle(video::HiddenLines);
    ASSERT_TRUE(video::RenderText(*board, pixels, std::span{colors}.first<8>()));
    const int written = video::Width >> GetParam();
    for (int row = 0; row < video::Height; ++row)
        for (int x = 0; x < video::Width; ++x)
            ASSERT_EQ(pixels[row * video::Width + x], x < written ? colors[x % 8] : Guard) << row << "," << x;
    ExpectGuards();
}

TEST_P(ScaledVideoTests, SymbolicCursorPositionTracksHorizontalScale)
{
    const int scale = 1 << GetParam();
    Parameters(GetParam(), 6, static_cast<uint16_t>(((3 * scale) << 8) | 15));
    Toggle(video::HiddenLines);
    ASSERT_TRUE(video::RenderRgb(*board, pixels, colors));
    for (int x = 0; x < video::Width; ++x)
        EXPECT_EQ(pixels[x], x >= 3 * 8 * scale && x < 4 * 8 * scale ? colors[15] : colors[96 + (x / scale) % 8]);
}

TEST_P(ScaledVideoTests, GraphicalCursorReplacesOnlySelectedBit)
{
    const int scale = 1 << GetParam();
    Parameters(GetParam(), 0, static_cast<uint16_t>(((2 * scale) << 8) | (5 << 5) | 16 | 14));
    Toggle(video::HiddenLines);
    ASSERT_TRUE(video::RenderRgb(*board, pixels, colors));
    for (int x = 0; x < video::Width; ++x)
        EXPECT_EQ(pixels[x], x >= 21 * scale && x < 22 * scale ? colors[14] : colors[(x / scale) % 8]);
}
INSTANTIATE_TEST_SUITE_P(AllScales, ScaledVideoTests, testing::Values(0, 1, 2, 3));

class BrightnessVideoTests : public VideoDecoderTests, public testing::WithParamInterface<int> {};
TEST_P(BrightnessVideoTests, MapsEveryPaletteNibbleThroughBrightnessBank)
{
    Parameters(0, GetParam());
    Tag(2, video::TagKind::Palette, 0xba98, 0xfedc);
    ASSERT_TRUE(video::RenderRgb(*board, pixels, colors));
    for (int x = 0; x < video::Width; ++x)
        EXPECT_EQ(pixels[x], colors[GetParam() * 16 + 8 + x % 8]);
}
INSTANTIATE_TEST_SUITE_P(AllBanks, BrightnessVideoTests, testing::Range(0, 8));

TEST_F(VideoDecoderTests, PaletteAndScaleChangesPersistAcrossShortTags)
{
    Tag(20, video::TagKind::Palette, 0xcdef, 0x89ab);
    Tag(22, video::TagKind::Parameters, 0, 027); // Scale 2, same brightness bank.
    ASSERT_TRUE(video::RenderRgb(*board, pixels, colors));
    for (int x = 0; x < video::Width; ++x)
    {
        EXPECT_EQ(pixels[x], colors[x % 8]);
        EXPECT_EQ(pixels[video::Width + x], colors[15 - x % 8]);
        EXPECT_EQ(pixels[2 * video::Width + x], colors[15 - x % 8]);
        EXPECT_EQ(pixels[3 * video::Width + x], colors[15 - (x / 2) % 8]);
    }
}

TEST_F(VideoDecoderTests, HiddenLinesUpdateStateButDoNotProduceOutput)
{
    for (int line = 0; line < video::HiddenLines; ++line) SetBits(line, 0xff, 0xff, 0xff);
    SetBits(video::HiddenLines, 0xff, 0, 0);
    SetBits(video::TotalLines - 1, 0, 0xff, 0);
    ASSERT_TRUE(video::RenderRgb(*board, pixels, colors));
    EXPECT_EQ(pixels.front(), colors[1]);
    EXPECT_EQ(pixels[FrameSize - 1], colors[2]);
    EXPECT_EQ(pixels[video::Width], colors[0]);
    ExpectGuards();
}

TEST_F(VideoDecoderTests, CursorTogglesBeforeDrawingAndPersistsUntilNextToggle)
{
    Parameters(0, 0, 15);
    Toggle(video::HiddenLines - 1); // Enabled during blanking.
    Toggle(video::HiddenLines + 1); // Disabled on second visible row.
    ASSERT_TRUE(video::RenderRgb(*board, pixels, colors));
    for (int x = 0; x < 8; ++x)
    {
        EXPECT_EQ(pixels[x], colors[15]);
        EXPECT_EQ(pixels[video::Width + x], colors[x]);
        EXPECT_EQ(pixels[2 * video::Width + x], colors[x]);
    }
}

TEST_F(VideoDecoderTests, PixelAddressWrapsAcrossAllThreePlanes)
{
    board->SetRAMWord(0, TagAddress(video::HiddenLines), 0xffff);
    for (int plane = 0; plane < 3; ++plane)
    {
        board->SetRAMByte(plane, 0xffff, plane == 0 ? 0xff : 0);
        board->SetRAMByte(plane, 0, plane == 1 ? 0xff : 0);
        board->SetRAMByte(plane, 1, plane == 2 ? 0xff : 0);
    }
    ASSERT_TRUE(video::RenderRgb(*board, pixels, colors));
    EXPECT_EQ(pixels[0], colors[1]);
    EXPECT_EQ(pixels[8], colors[2]);
    EXPECT_EQ(pixels[16], colors[4]);
    ASSERT_TRUE(video::RenderText(*board, pixels, colors));
    EXPECT_EQ(pixels[0], colors[1]);
    EXPECT_EQ(pixels[8], colors[2]);
    EXPECT_EQ(pixels[16], colors[4]);
}

TEST_F(VideoDecoderTests, ShortOnlyCyclicListHasBoundedFrameAndInitialBlackPalette)
{
    board->SetRAMWord(0, 0000272, 0000270);
    ASSERT_TRUE(video::RenderRgb(*board, pixels, colors));
    EXPECT_TRUE(std::ranges::all_of(pixels.first(FrameSize), [](auto pixel) { return pixel == 0; }));
    ASSERT_TRUE(video::RenderText(*board, pixels, colors));
    EXPECT_EQ(pixels[0], colors[0]);
    EXPECT_EQ(pixels[FrameSize - 1], colors[7]);
    ExpectGuards();
}

TEST_F(VideoDecoderTests, RejectedSpansLeaveAllOutputUntouched)
{
    for (size_t size : {size_t{0}, size_t{1}, FrameSize - 1})
    {
        EXPECT_FALSE(video::RenderRgb(*board, pixels.first(size), colors));
        EXPECT_FALSE(video::RenderText(*board, pixels.first(size), colors));
    }
    EXPECT_FALSE(video::RenderRgb(*board, pixels, std::span{colors}.first<127>()));
    EXPECT_FALSE(video::RenderRgb(*board, pixels, {}));
    EXPECT_FALSE(video::RenderText(*board, pixels, std::span{colors}.first<7>()));
    EXPECT_FALSE(video::RenderText(*board, pixels, {}));
    EXPECT_TRUE(std::ranges::all_of(storage, [](auto pixel) { return pixel == Guard; }));
}

TEST_F(VideoDecoderTests, ExactSizeBuffersWorkAndRenderingDoesNotModifyRam)
{
    std::vector<uint8_t> before(UKNCIMAGE_SIZE, 0), after(UKNCIMAGE_SIZE, 0);
    board->SaveToImage(std::span<uint8_t, UKNCIMAGE_SIZE>{before});
    ASSERT_TRUE(video::RenderRgb(*board, pixels.first(FrameSize), colors));
    ASSERT_TRUE(video::RenderText(*board, pixels.first(FrameSize), std::span{colors}.first<8>()));
    board->SaveToImage(std::span<uint8_t, UKNCIMAGE_SIZE>{after});
    EXPECT_EQ(after, before);
    ExpectGuards();
}
}
