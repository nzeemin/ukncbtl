#include "stdafx.h"
#include <array>
#include <gtest/gtest.h>

namespace {
std::wstring lastMessage;
}
void AlertWarning(LPCTSTR message) { lastMessage = message; }
void DebugPrint(LPCTSTR message) { lastMessage = message; }
void DebugLog(LPCTSTR message) { lastMessage = message; }

TEST(TextFormatTests, PreservesDebuggerNumberFormats)
{
    std::array<wchar_t, 64> buffer{};
    FormatBuffer(buffer, L"{:06o} {:04x} {:08X} {:<7} {}", 012345, 0xab, 0x1234, L"MOV", -2);
    EXPECT_STREQ(buffer.data(), L"012345 00ab 00001234 MOV     -2");
}

TEST(TextFormatTests, TruncatesAndTerminatesWithinTheSuppliedSpan)
{
    std::array<wchar_t, 8> buffer{};
    buffer.fill(L'!');
    FormatBuffer(std::span{buffer}.subspan<1, 4>(), L"{}", L"abcdef");
    EXPECT_EQ(buffer[0], L'!');
    EXPECT_STREQ(buffer.data() + 1, L"abc");
    EXPECT_EQ(buffer[5], L'!');
}

TEST(TextFormatTests, HandlesEmptyAndTerminatorOnlyDestinations)
{
    FormatBuffer({}, L"{}", 42);
    std::array<wchar_t, 1> buffer{L'!'};
    FormatBuffer(buffer, L"{}", 42);
    EXPECT_EQ(buffer.front(), L'\0');
}

TEST(TextFormatTests, LogAndWarningFormattingDoesNotTruncateLongText)
{
    const std::wstring text(2048, L'x');
    DebugLogFormat(L"{} {:06o}", text, 0777);
    EXPECT_EQ(lastMessage, text + L" 000777");
    DebugPrintFormat(L"{}", text);
    EXPECT_EQ(lastMessage, text);
    AlertWarningFormat(L"{} (0x{:08x})", text, 5);
    EXPECT_EQ(lastMessage, text + L" (0x00000005)");
}
