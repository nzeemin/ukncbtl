#include "stdafx.h"
#include <gtest/gtest.h>
#include "ViewCalculations.h"

TEST(MemoryHitTest, AllColumnsRowsAndAddressWrap)
{
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 8; ++column)
            EXPECT_EQ(static_cast<uint16_t>(0xfff0 + row * 16 + column * 2),
                view_calc::MemoryAddress(104 + column * 56, (row + 1) * 16, 16, 8, 56, 4, 0xfff0));
}
TEST(MemoryHitTest, HeaderAndMarginsClampToNearestWord)
{
    EXPECT_EQ(0, view_calc::MemoryAddress(-100, -100, 16, 8, 56, 4, 0));
    EXPECT_EQ(14, view_calc::MemoryAddress(INT_MAX, 0, 16, 8, 56, 4, 0));
    EXPECT_EQ(0, view_calc::MemoryAddress(159, 31, 16, 8, 56, 4, 0));
    EXPECT_EQ(2, view_calc::MemoryAddress(160, 31, 16, 8, 56, 4, 0));
    EXPECT_EQ(48, view_calc::MemoryAddress(104, 79, 16, 8, 56, 4, 0));
    EXPECT_EQ(-1, view_calc::MemoryAddress(104, 80, 16, 8, 56, 4, 0));
}
TEST(MemoryHitTest, InvalidMetricsAndEmptyPages)
{
    EXPECT_EQ(-1, view_calc::MemoryAddress(0, 0, 0, 8, 56, 4, 0));
    EXPECT_EQ(-1, view_calc::MemoryAddress(0, 0, 16, 0, 56, 4, 0));
    EXPECT_EQ(-1, view_calc::MemoryAddress(0, 0, 16, 8, 0, 4, 0));
    EXPECT_EQ(-1, view_calc::MemoryAddress(0, 0, 16, 8, 56, 0, 0));
    EXPECT_EQ(-1, view_calc::MemoryAddress(0, 0, 16, 8, 56, -1, 0));
}
TEST(DebugHitTest, TopMarginAndUninitializedMetricsAreInvalid)
{
    EXPECT_EQ(-1, view_calc::DebugLine(100, 0));
    EXPECT_EQ(-1, view_calc::DebugLine(-1, 16));
    EXPECT_EQ(-1, view_calc::DebugLine(7, 16));
    EXPECT_EQ(0, view_calc::DebugLine(8, 16));
    EXPECT_EQ(0, view_calc::DebugLine(23, 16));
    EXPECT_EQ(1, view_calc::DebugLine(24, 16));
}
TEST(DebugHitTest, RegisterSavedStateAndBlankRows)
{
    for (int line = -1; line < 14; ++line)
        EXPECT_EQ(line >= 0 && line < 12 && line != 9, view_calc::IsRegisterLine(line));
}
TEST(HistoryView, RowCountAndMaximum)
{
    EXPECT_EQ(1, view_calc::HistoryRows(0, 16));
    EXPECT_EQ(1, view_calc::HistoryRows(100, 0));
    EXPECT_EQ(1, view_calc::HistoryRows(INT_MIN, 16));
    EXPECT_EQ(2, view_calc::HistoryRows(34, 16));
    EXPECT_EQ(0, view_calc::HistoryMaximum(0, 10));
    EXPECT_EQ(0, view_calc::HistoryMaximum(10, 10));
    EXPECT_EQ(246, view_calc::HistoryMaximum(256, 10));
}
TEST(HistoryView, LinesPagesTopAndBottom)
{
    EXPECT_EQ(11, view_calc::HistoryScroll(10, 100, 8, SB_LINEUP, 0));
    EXPECT_EQ(9, view_calc::HistoryScroll(10, 100, 8, SB_LINEDOWN, 0));
    EXPECT_EQ(18, view_calc::HistoryScroll(10, 100, 8, SB_PAGEUP, 0));
    EXPECT_EQ(2, view_calc::HistoryScroll(10, 100, 8, SB_PAGEDOWN, 0));
    EXPECT_EQ(100, view_calc::HistoryScroll(10, 100, 8, SB_TOP, 0));
    EXPECT_EQ(0, view_calc::HistoryScroll(10, 100, 8, SB_BOTTOM, 0));
    EXPECT_EQ(10, view_calc::HistoryScroll(10, 100, 8, SB_ENDSCROLL, 0));
}
TEST(HistoryView, ClampsBothEndsAndEmptyHistory)
{
    for (auto command : {SB_LINEUP, SB_LINEDOWN, SB_PAGEUP, SB_PAGEDOWN, SB_TOP, SB_BOTTOM})
        EXPECT_EQ(0, view_calc::HistoryScroll(0, 0, 8, command, 0));
    EXPECT_EQ(0, view_calc::HistoryScroll(3, 100, 8, SB_PAGEDOWN, 0));
    EXPECT_EQ(100, view_calc::HistoryScroll(98, 100, 8, SB_PAGEUP, 0));
    EXPECT_EQ(100, view_calc::HistoryClamp(INT_MAX, 100));
    EXPECT_EQ(0, view_calc::HistoryClamp(INT_MIN, 100));
}
TEST(HistoryView, ThumbDirectionAndFullWidthPosition)
{
    for (auto command : {SB_THUMBTRACK, SB_THUMBPOSITION})
    {
        EXPECT_EQ(100, view_calc::HistoryScroll(50, 100, 8, command, 0));
        EXPECT_EQ(0, view_calc::HistoryScroll(50, 100, 8, command, 100));
        EXPECT_EQ(30, view_calc::HistoryScroll(50, 100, 8, command, 70));
        EXPECT_EQ(0, view_calc::HistoryScroll(50, 100, 8, command, 70000));
        EXPECT_EQ(100, view_calc::HistoryScroll(50, 100, 8, command, -1));
    }
}
TEST(HistoryView, WheelDirectionPartialNotchesAndClamping)
{
    EXPECT_EQ(53, view_calc::HistoryWheel(50, 100, 120));
    EXPECT_EQ(47, view_calc::HistoryWheel(50, 100, -120));
    EXPECT_EQ(50, view_calc::HistoryWheel(50, 100, 39));
    EXPECT_EQ(50, view_calc::HistoryWheel(50, 100, -39));
    EXPECT_EQ(51, view_calc::HistoryWheel(50, 100, 40));
    EXPECT_EQ(100, view_calc::HistoryWheel(99, 100, 120));
    EXPECT_EQ(0, view_calc::HistoryWheel(1, 100, -120));
}
