/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once
#include <algorithm>
#include <cstdint>

namespace view_calc
{
inline int MemoryAddress(int x, int y, int lineHeight, int charWidth, int increment, int rows, uint16_t base)
{
    if (lineHeight <= 0 || charWidth <= 0 || increment <= 0 || rows <= 0) return -1;
    const auto line = std::max<int64_t>(0, int64_t{y} / lineHeight - 1);
    if (line >= rows) return -1;
    const auto position = std::clamp<int64_t>((int64_t{x} - 36 - 9LL * charWidth + charWidth / 2) / increment, 0, 7);
    return static_cast<uint16_t>(base + line * 16 + position * 2);
}
inline int DebugLine(int y, int lineHeight)
{
    if (lineHeight <= 0 || y < lineHeight / 2) return -1;
    return (y - lineHeight / 2) / lineHeight;
}
inline bool IsRegisterLine(int line) { return line >= 0 && line <= 11 && line != 9; }
inline int HistoryRows(int height, int lineHeight)
{
    return lineHeight <= 0 ? 1 : static_cast<int>(std::max<int64_t>(1, (int64_t{height} - 2) / lineHeight));
}
inline int HistoryMaximum(int length, int rows) { return std::max(0, length - std::max(1, rows)); }
inline int HistoryClamp(int64_t index, int maximum) { return static_cast<int>(std::clamp<int64_t>(index, 0, std::max(0, maximum))); }
// command uses the Win32 SB_* values supplied by the view.
int HistoryScroll(int current, int maximum, int rows, unsigned command, int track);
inline int HistoryWheel(int current, int maximum, int delta)
{
    return HistoryClamp(int64_t{current} + delta / 40, maximum);
}
}
