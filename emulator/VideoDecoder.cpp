/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include "VideoDecoder.h"
#include "emubase/Board.h"
#include "util/Profiler.h"
#include <array>

namespace video {
DisplayTag DisplayListCursor::Next(const CMotherboard& board)
{
    DisplayTag tag{address_, kind_};
    if (kind_ != TagKind::Short)
    {
        tag.tag1 = board.GetRAMWord(0, address_);
        address_ += 2;
        tag.tag2 = board.GetRAMWord(0, address_);
        address_ += 2;
    }
    tag.pixels = board.GetRAMWord(0, address_);
    address_ += 2;
    tag.link = board.GetRAMWord(0, address_);
    address_ = tag.NextAddress();
    kind_ = tag.NextKind();
    return tag;
}

bool RenderRgb(const CMotherboard& board, std::span<uint32_t> pixels, std::span<const uint32_t> colors)
{
    UKNC_PROFILE_SCOPE("Video decode");
    if (pixels.size() < Width * Height || colors.size() < 128) return false;

    DisplayListCursor tags;
    uint32_t palette = 0;
    std::array<uint32_t, 8> currentPalette{};
    int scale = 1, brightness = 0;
    int cursorColor = 0, cursorPosition = 128, cursorBit = 0;
    bool cursorOn = false, graphicalCursor = false;
    for (int line = 0; line < TotalLines; ++line)
    {
        const auto tag = tags.Next(board);
        if (tag.kind != TagKind::Short)
        {
            if (tag.kind == TagKind::Palette)
                palette = tag.tag1 | (static_cast<uint32_t>(tag.tag2) << 16);
            else
            {
                const int shift = (tag.tag2 >> 4) & 3;
                scale = tag.Scale();
                brightness = (7 - (tag.tag2 & 7)) << 4;
                cursorColor = tag.tag1 & 15;
                graphicalCursor = (tag.tag1 & 16) != 0;
                cursorPosition = ((tag.tag1 >> 8) >> shift) & 0x7f;
                cursorBit = (tag.tag1 >> 5) & 7;
            }
            for (int color = 0; color < 8; ++color)
                currentPalette[color] = colors[brightness | ((palette >> (color * 4)) & 15)];
        }
        if (tag.ToggleCursor()) cursorOn = !cursorOn;
        if (line < HiddenLines) continue;

        uint16_t address = tag.pixels;
        auto row = pixels.subspan(static_cast<size_t>(line - HiddenLines) * Width, Width);
        int x = 0;
        for (int position = 0; x < Width; ++position, ++address)
        {
            const auto plane0 = board.GetRAMByte(0, address);
            const auto plane1 = board.GetRAMByte(1, address);
            const auto plane2 = board.GetRAMByte(2, address);
            for (int bit = 0; bit < 8; ++bit)
            {
                const int index = ((plane0 >> bit) & 1) | (((plane1 >> bit) & 1) << 1) | (((plane2 >> bit) & 1) << 2);
                const auto color = cursorOn && position == cursorPosition && (!graphicalCursor || bit == cursorBit)
                    ? colors[cursorColor] : currentPalette[index];
                for (int repeat = 0; repeat < scale; ++repeat) row[x++] = color;
            }
        }
    }
    return true;
}

bool RenderText(const CMotherboard& board, std::span<uint32_t> pixels, std::span<const uint32_t> colors)
{
    if (pixels.size() < Width * Height || colors.size() < 8) return false;
    DisplayListCursor tags;
    int scale = 1;
    for (int line = 0; line < TotalLines; ++line)
    {
        const auto tag = tags.Next(board);
        if (tag.kind == TagKind::Parameters) scale = tag.Scale();
        if (line < HiddenLines) continue;
        uint16_t address = tag.pixels;
        auto row = pixels.subspan(static_cast<size_t>(line - HiddenLines) * Width, Width);
        for (int x = 0; x < Width / scale; x += 8, ++address)
        {
            const auto plane0 = board.GetRAMByte(0, address);
            const auto plane1 = board.GetRAMByte(1, address);
            const auto plane2 = board.GetRAMByte(2, address);
            for (int bit = 0; bit < 8; ++bit)
            {
                const int index = ((plane0 >> bit) & 1) | (((plane1 >> bit) & 1) << 1) | (((plane2 >> bit) & 1) << 2);
                row[x + bit] = colors[index];
            }
        }
    }
    return true;
}
}
