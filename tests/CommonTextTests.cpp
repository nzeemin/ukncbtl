/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include "util/Octal.h"
#include <gtest/gtest.h>
#include <array>
#include <format>
#include <string>

namespace {
struct NumberCase { uint16_t value; const TCHAR* octal; const TCHAR* hex; const TCHAR* binary; };
class NumberFormatTests : public testing::TestWithParam<NumberCase> {};
TEST_P(NumberFormatTests, ExactDigitsTerminationAndGuardBytes)
{
    const auto& expected = GetParam();
    std::array<TCHAR, 9> octal;
    std::array<TCHAR, 7> hex;
    std::array<TCHAR, 19> binary;
    octal.fill(_T('!')); hex.fill(_T('!')); binary.fill(_T('!'));
    PrintOctalValue(octal.data() + 1, expected.value);
    PrintHexValue(hex.data() + 1, expected.value);
    PrintBinaryValue(binary.data() + 1, expected.value);
    EXPECT_STREQ(octal.data() + 1, expected.octal);
    EXPECT_STREQ(hex.data() + 1, expected.hex);
    EXPECT_STREQ(binary.data() + 1, expected.binary);
    EXPECT_EQ(octal[7], 0); EXPECT_EQ(hex[5], 0); EXPECT_EQ(binary[17], 0);
    EXPECT_EQ(octal.front(), _T('!')); EXPECT_EQ(octal.back(), _T('!'));
    EXPECT_EQ(hex.front(), _T('!')); EXPECT_EQ(hex.back(), _T('!'));
    EXPECT_EQ(binary.front(), _T('!')); EXPECT_EQ(binary.back(), _T('!'));
}
INSTANTIATE_TEST_SUITE_P(Boundaries, NumberFormatTests, testing::Values(
    NumberCase{0, _T("000000"), _T("0000"), _T("0000000000000000")},
    NumberCase{1, _T("000001"), _T("0001"), _T("0000000000000001")},
    NumberCase{7, _T("000007"), _T("0007"), _T("0000000000000111")},
    NumberCase{8, _T("000010"), _T("0008"), _T("0000000000001000")},
    NumberCase{0xabcd, _T("125715"), _T("abcd"), _T("1010101111001101")},
    NumberCase{0x7fff, _T("077777"), _T("7fff"), _T("0111111111111111")},
    NumberCase{0x8000, _T("100000"), _T("8000"), _T("1000000000000000")},
    NumberCase{0xffff, _T("177777"), _T("ffff"), _T("1111111111111111")}
));

TEST(NumberFormatTests, EveryWordMatchesStandardFormattingAndOctalRoundTrips)
{
    std::array<TCHAR, 7> octal{};
    std::array<TCHAR, 5> hex{};
    std::array<TCHAR, 17> binary{};
    for (uint32_t value = 0; value <= 65535; ++value)
    {
        const auto word = static_cast<uint16_t>(value);
        PrintOctalValue(octal.data(), word);
        PrintHexValue(hex.data(), word);
        PrintBinaryValue(binary.data(), word);
        ASSERT_EQ(std::wstring(octal.data()), std::format(L"{:06o}", value)) << value;
        ASSERT_EQ(std::wstring(hex.data()), std::format(L"{:04x}", value)) << value;
        ASSERT_EQ(std::wstring(binary.data()), std::format(L"{:016b}", value)) << value;
        ASSERT_EQ(ParseOctalValue(octal.data()), word) << value;
    }
}

struct OctalCase { const TCHAR* text; OctalError error; };
class InvalidOctalTests : public testing::TestWithParam<OctalCase> {};
TEST_P(InvalidOctalTests, ReturnsSpecificError)
{
    const auto& input = GetParam();
    EXPECT_EQ(ParseOctalValue(input.text), std::unexpected(input.error));
}
INSTANTIATE_TEST_SUITE_P(Inputs, InvalidOctalTests, testing::Values(
    OctalCase{_T(""), OctalError::empty},
    OctalCase{_T("8"), OctalError::invalid_digit},
    OctalCase{_T("19"), OctalError::invalid_digit},
    OctalCase{_T("-1"), OctalError::invalid_digit},
    OctalCase{_T("+1"), OctalError::invalid_digit},
    OctalCase{_T(" 1"), OctalError::invalid_digit},
    OctalCase{_T("1 "), OctalError::invalid_digit},
    OctalCase{_T("1\t"), OctalError::invalid_digit},
    OctalCase{_T("1\n"), OctalError::invalid_digit},
    OctalCase{_T("0o7"), OctalError::invalid_digit},
    OctalCase{_T("0x7"), OctalError::invalid_digit},
    OctalCase{_T("7a"), OctalError::invalid_digit},
    OctalCase{_T("\uff17"), OctalError::invalid_digit},
    OctalCase{_T("200000"), OctalError::out_of_range},
    OctalCase{_T("777777"), OctalError::out_of_range},
    OctalCase{_T("1000000"), OctalError::out_of_range},
    OctalCase{_T("777777777777777777777777777777777"), OctalError::out_of_range}
));

TEST(OctalParsingTests, LeadingZerosDoNotCauseFalseOverflow)
{
    const std::wstring zeros(4096, L'0');
    EXPECT_EQ(ParseOctalValue(zeros), 0);
    EXPECT_EQ(ParseOctalValue(zeros + L"177777"), 65535);
    EXPECT_EQ(ParseOctalValue(zeros + L"200000"), std::unexpected(OctalError::out_of_range));
}

TEST(OctalParsingTests, UsesViewLengthAndRejectsEmbeddedNull)
{
    const std::array<TCHAR, 3> digits{_T('1'), _T('2'), _T('3')};
    EXPECT_EQ(ParseOctalValue(std::wstring_view(digits.data(), digits.size())), 0123);
    EXPECT_EQ(ParseOctalValue(std::wstring_view(L"123junk", 3)), 0123);
    EXPECT_EQ(ParseOctalValue(std::wstring_view(L"12\0", 3)), std::unexpected(OctalError::invalid_digit));
}

TEST(Koi8rTests, AsciiAndControlBytesArePreserved)
{
    for (uint16_t byte = 0; byte < 128; ++byte)
        EXPECT_EQ(Translate_KOI8R(static_cast<uint8_t>(byte)), static_cast<TCHAR>(byte));
}

TEST(Koi8rTests, CyrillicAlphabetMapsInBothCases)
{
    // Alphabetical order is deliberately different from the KOI8 byte order.
    constexpr std::array<uint8_t, 32> lowerBytes{
        0xc1,0xc2,0xd7,0xc7,0xc4,0xc5,0xd6,0xda,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,0xd0,
        0xd2,0xd3,0xd4,0xd5,0xc6,0xc8,0xc3,0xde,0xdb,0xdd,0xdf,0xd9,0xd8,0xdc,0xc0,0xd1};
    for (size_t i = 0; i < lowerBytes.size(); ++i)
    {
        EXPECT_EQ(Translate_KOI8R(lowerBytes[i]), static_cast<TCHAR>(0x0430 + i));
        EXPECT_EQ(Translate_KOI8R(static_cast<uint8_t>(lowerBytes[i] + 32)), static_cast<TCHAR>(0x0410 + i));
    }
}

TEST(Koi8rTests, UkncGraphicsAndSymbolsRetainMachineSpecificMapping)
{
    const std::array<std::pair<uint8_t, TCHAR>, 18> symbols{{
        {0x80, 0x2191}, {0x81, 0x2193}, {0x82, 0x2190}, {0x83, 0x2192},
        {0x84, 0x25b2}, {0x8f, 0x2556}, {0x90, 0x00d7}, {0x91, 0x00f7},
        {0x92, 0x2265}, {0x93, 0x2264}, {0x99, 0x00b0}, {0x9a, 0x2260},
        {0x9b, 0x2591}, {0x9c, 0x2591}, {0x9f, 0x2588}, {0xa0, 0x2562},
        {0xb5, 0x2500}, {0xbf, 0x2557}}};
    for (const auto& [byte, expected] : symbols) EXPECT_EQ(Translate_KOI8R(byte), expected) << int(byte);
}

struct PathCase { const TCHAR* input; const TCHAR* filename; };
class FileNameTests : public testing::TestWithParam<PathCase> {};
TEST_P(FileNameTests, ReturnsExpectedSuffixWithinOriginalString)
{
    const auto& input = GetParam();
    const auto result = GetFileNameFromFilePath(input.input);
    EXPECT_STREQ(result, input.filename);
    const auto expectedOffset = _tcslen(input.input) - _tcslen(input.filename);
    EXPECT_EQ(result, input.input + expectedOffset);
}
INSTANTIATE_TEST_SUITE_P(Paths, FileNameTests, testing::Values(
    PathCase{_T(""), _T("")},
    PathCase{_T("disk.dsk"), _T("disk.dsk")},
    PathCase{_T("C:\\images\\disk.dsk"), _T("disk.dsk")},
    PathCase{_T("C:\\images\\"), _T("")},
    PathCase{_T("\\"), _T("")},
    PathCase{_T("\\\\server\\share\\disk.dsk"), _T("disk.dsk")},
    PathCase{_T("folder\\\\file"), _T("file")},
    PathCase{_T("folder\\..\\file"), _T("file")},
    PathCase{_T("C:disk.dsk"), _T("C:disk.dsk")},
    PathCase{_T("C:/images/disk.dsk"), _T("C:/images/disk.dsk")},
    PathCase{_T("C:\\images\\\u0423\u041a\u041d\u0426.dsk"), _T("\u0423\u041a\u041d\u0426.dsk")}
));
}
