#include "stdafx.h"
#include <gtest/gtest.h>
#include "RamChanges.h"
#include <algorithm>
#include <vector>

TEST(RamChanges, EveryByteIncludingLastIsComparedAndBaselineUpdated)
{
    std::vector<uint8_t> old(65538,0xcd), changed(65538,0xcd);
    unsigned calls=0;
    ASSERT_TRUE(ram_changes::Update(std::span(old).subspan(1,65536),std::span(changed).subspan(1,65536),
        [&](uint16_t address) { ++calls; return static_cast<uint8_t>(address ^ (address>>8)); }));
    EXPECT_EQ(65536,calls);
    for (unsigned i=0;i<65536;++i)
    {
        const uint8_t expected = static_cast<uint8_t>(i ^ (i>>8));
        ASSERT_EQ(expected,old[i+1]) << i; ASSERT_EQ(expected ^ 0xcd,changed[i+1]) << i;
    }
    EXPECT_EQ(0xcd,old.front()); EXPECT_EQ(0xcd,old.back()); EXPECT_EQ(0xcd,changed.front()); EXPECT_EQ(0xcd,changed.back());
}
TEST(RamChanges, UnchangedUpdateClearsFlagsAndRevertedBytesCountAsChanges)
{
    std::vector<uint8_t> old(65536), changed(65536);
    auto ones = [](uint16_t) { return uint8_t{255}; };
    ASSERT_TRUE(ram_changes::Update(old,changed,ones));
    ASSERT_TRUE(ram_changes::Update(old,changed,ones));
    EXPECT_TRUE(std::ranges::all_of(changed,[](auto value) { return value==0; }));
    ASSERT_TRUE(ram_changes::Update(old,changed,[](uint16_t address) { return uint8_t(address==65535 ? 0 : 255); }));
    EXPECT_EQ(255,changed.back()); EXPECT_EQ(0,old.back()); EXPECT_EQ(0,changed[65534]);
}
TEST(RamChanges, ShortBuffersDoNotReadOrModify)
{
    std::vector<uint8_t> old(65536,1), changed(65536,2); int calls=0;
    auto read = [&](uint16_t) { ++calls; return uint8_t{3}; };
    EXPECT_FALSE(ram_changes::Update(std::span(old).first(65535),changed,read));
    EXPECT_FALSE(ram_changes::Update(old,std::span(changed).first(65535),read));
    EXPECT_EQ(0,calls); EXPECT_TRUE(std::ranges::all_of(old,[](auto v){return v==1;}));
    EXPECT_TRUE(std::ranges::all_of(changed,[](auto v){return v==2;}));
}
TEST(RamChanges, WordReadsAlignEveryAddressAndStayInsidePlane)
{
    std::vector<uint8_t> flags(65536);
    for (unsigned i=0;i<65536;++i) flags[i]=static_cast<uint8_t>(i+i/256);
    for (unsigned i=0;i<65536;++i)
        ASSERT_EQ(flags[i&~1] | (flags[(i&~1)+1]<<8),ram_changes::Word(flags,static_cast<uint16_t>(i))) << i;
    EXPECT_EQ(0,ram_changes::Word({},65535));
}
TEST(RamChanges, InterleavedPlanesCoverHaltMemoryAndOddAddresses)
{
    std::vector<uint8_t> low(65536,0x12),high(65536,0x34);
    low[32767]=0xab; high[32767]=0xcd;
    for (uint16_t address : {0,1,0170000,0177774}) EXPECT_EQ(0x3412,ram_changes::Interleaved(low,high,address));
    EXPECT_EQ(0xcdab,ram_changes::Interleaved(low,high,65534)); EXPECT_EQ(0xcdab,ram_changes::Interleaved(low,high,65535));
    EXPECT_EQ(0,ram_changes::Interleaved({},high,0)); EXPECT_EQ(0,ram_changes::Interleaved(low,{},0));
}
TEST(RamChanges, IndependentBaselinesDoNotShareChanges)
{
    std::vector<uint8_t> oldA(65536),oldB(65536),a(65536),b(65536);
    ram_changes::Update(oldA,a,[](uint16_t){return uint8_t{1};});
    ram_changes::Update(oldB,b,[](uint16_t){return uint8_t{2};});
    EXPECT_EQ(0x0101,ram_changes::Word(a,0)); EXPECT_EQ(0x0202,ram_changes::Word(b,0));
}
