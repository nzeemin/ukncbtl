#include "stdafx.h"
#include <gtest/gtest.h>
#include "Subtitles.h"

TEST(Subtitles, RejectsEmptyAndBigEndian)
{
    EXPECT_EQ(subtitles::ParseError::Empty, subtitles::Parse(L"").error());
    EXPECT_EQ(subtitles::ParseError::BigEndian, subtitles::Parse(L"\xfffe").error());
}
TEST(Subtitles, AcceptsOptionalBomAndBomOnly)
{
    for (auto text : {L"000010 ;hello", L"\xfeff" L"000010 ;hello"})
    {
        auto parsed = subtitles::Parse(text);
        ASSERT_TRUE(parsed);
        ASSERT_EQ(1, parsed->size());
        EXPECT_EQ(8, parsed->front().address);
        EXPECT_EQ(L";hello", parsed->front().comment);
    }
    ASSERT_TRUE(subtitles::Parse(L"\xfeff"));
    EXPECT_TRUE(subtitles::Parse(L"\xfeff")->empty());
}
TEST(Subtitles, AllLineEndingsAndFinalDirective)
{
    for (auto newline : {L"\r", L"\n", L"\r\n"})
    {
        auto parsed = subtitles::Parse(std::wstring(L"10 .WORD 1") + newline + L"12 .BYTE 2");
        ASSERT_TRUE(parsed);
        ASSERT_EQ(2, parsed->size());
        EXPECT_EQ(SUBTYPE_DATA, (*parsed)[0].type);
        EXPECT_EQ(SUBTYPE_DATA, (*parsed)[1].type);
        EXPECT_TRUE((*parsed)[1].comment.empty());
    }
}
TEST(Subtitles, DelimitersDirectiveAndInstructionComments)
{
    auto parsed = subtitles::Parse(L"10 \t$:: .WORD 1 ;data\n12: MOV R0,R1 ;instruction;rest");
    ASSERT_TRUE(parsed);
    ASSERT_EQ(2, parsed->size());
    EXPECT_EQ(SUBTYPE_DATA | SUBTYPE_COMMENT, (*parsed)[0].type);
    EXPECT_EQ(L";data", (*parsed)[0].comment);
    EXPECT_EQ(SUBTYPE_COMMENT, (*parsed)[1].type);
    EXPECT_EQ(L";instruction;rest", (*parsed)[1].comment);
}
TEST(Subtitles, LastBlockCommentAttachesAcrossBlankLines)
{
    auto parsed = subtitles::Parse(L";first\n;second\r\n\r\n10 MOV ;inline");
    ASSERT_TRUE(parsed);
    ASSERT_EQ(2, parsed->size());
    EXPECT_EQ(SUBTYPE_BLOCKCOMMENT, (*parsed)[0].type);
    EXPECT_EQ(L";second", (*parsed)[0].comment);
    EXPECT_EQ(8, (*parsed)[0].address);
    EXPECT_EQ(SUBTYPE_COMMENT, (*parsed)[1].type);
}
TEST(Subtitles, NonCommentLineCancelsBlockAndIndentedLinesAreIgnored)
{
    auto parsed = subtitles::Parse(L";block\nheading\n10 MOV\n 12 ;ignored\n;trailing");
    ASSERT_TRUE(parsed);
    EXPECT_TRUE(parsed->empty());
}
TEST(Subtitles, BlockAtFinalBareAddress)
{
    auto parsed = subtitles::Parse(L";block\n177777");
    ASSERT_TRUE(parsed);
    ASSERT_EQ(1, parsed->size());
    EXPECT_EQ(65535, parsed->front().address);
}
class InvalidSubtitleAddress : public testing::TestWithParam<const wchar_t*> {};
TEST_P(InvalidSubtitleAddress, RejectsWithoutPartialResult)
{
    auto parsed = subtitles::Parse(std::wstring(L"10 ;valid\n") + GetParam());
    ASSERT_FALSE(parsed);
    EXPECT_EQ(subtitles::ParseError::InvalidAddress, parsed.error());
}
INSTANTIATE_TEST_SUITE_P(Subtitles, InvalidSubtitleAddress,
    testing::Values(L"8 ;invalid", L"9", L"200000 .WORD", L"99999999999999999", L"177778 ;invalid"));
TEST(Subtitles, BoundariesLeadingZerosAndIgnoredInstruction)
{
    auto parsed = subtitles::Parse(L"000000000000 ;zero\n177777 ;max\n42 MOV R0,R1");
    ASSERT_TRUE(parsed);
    ASSERT_EQ(2, parsed->size());
    EXPECT_EQ(0, parsed->front().address);
    EXPECT_EQ(65535, parsed->back().address);
}
TEST(Subtitles, OwnsUnicodeCommentsAndCopiesSafely)
{
    std::wstring source = L"10 ;\u041f\u0440\u0438\u0432\u0435\u0442";
    auto parsed = subtitles::Parse(source);
    ASSERT_TRUE(parsed);
    auto copy = *parsed;
    source.assign(1000, L'x');
    parsed->clear();
    ASSERT_EQ(1, copy.size());
    EXPECT_EQ(L";\u041f\u0440\u0438\u0432\u0435\u0442", copy[0].comment);
}
TEST(Subtitles, BoundedViewAndEmbeddedNull)
{
    const wchar_t text[] = {L'1', L'0', L' ', L';', L'x', L'9'};
    auto parsed = subtitles::Parse(std::wstring_view(text, 5));
    ASSERT_TRUE(parsed);
    EXPECT_EQ(L";x", parsed->front().comment);
    constexpr wchar_t nulText[] = L"10 ;x\0\n9 invalid";
    parsed = subtitles::Parse(std::wstring_view(nulText, std::size(nulText) - 1));
    ASSERT_TRUE(parsed);
    EXPECT_EQ(1, parsed->size());
}
TEST(Subtitles, FindEmptyMissingAndPastLast)
{
    EXPECT_EQ(nullptr, subtitles::Find({}, 0, 7));
    const auto parsed = subtitles::Parse(L"10 ;one\n12 .WORD");
    ASSERT_TRUE(parsed);
    EXPECT_EQ(nullptr, subtitles::Find(*parsed, 0, 7));
    EXPECT_EQ(nullptr, subtitles::Find(*parsed, 9, 7));
    EXPECT_EQ(nullptr, subtitles::Find(*parsed, 65535, 7));
}
TEST(Subtitles, FindUnsortedDuplicateAddressesAndMasks)
{
    const auto parsed = subtitles::Parse(L"100 ;high\n;block\n10 .WORD ;data\n10 ;duplicate");
    ASSERT_TRUE(parsed);
    ASSERT_EQ(4, parsed->size());
    EXPECT_EQ(&(*parsed)[1], subtitles::Find(*parsed, 8, SUBTYPE_BLOCKCOMMENT));
    EXPECT_EQ(&(*parsed)[2], subtitles::Find(*parsed, 8, SUBTYPE_DATA));
    EXPECT_EQ(&(*parsed)[2], subtitles::Find(*parsed, 8, SUBTYPE_COMMENT));
    EXPECT_EQ(nullptr, subtitles::Find(*parsed, 8, 0));
    EXPECT_EQ(&(*parsed)[0], subtitles::Find(*parsed, 64, 7));
}
