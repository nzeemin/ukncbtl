/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include <gtest/gtest.h>
#include "util/ScreenScale.h"
#include <algorithm>
#include <array>
#include <vector>
namespace {
constexpr uint32_t sentinel=0xdeadbeef;
constexpr std::array<std::pair<int,int>,8> sizes{{{640,288},{640,432},{640,576},{640,576},{960,576},{960,720},{1120,864},{1280,864}}};
uint32_t Average(uint32_t a,uint32_t b) { return ((a&0xfefefeffu)+(b&0xfefefeffu))>>1; }
std::vector<uint32_t> Source() {
    std::vector<uint32_t> source(640*288);
    for(int y=0;y<288;++y) for(int x=0;x<640;++x) source[y*640+x]=0x010000u|(static_cast<uint32_t>(y)<<8)|(x&255);
    return source;
}
}
TEST(ScreenScaleTests, ModeCatalogAndDefaultDimensions)
{
    const auto modes=screen_scale::Modes(); ASSERT_EQ(sizes.size(),modes.size());
    for(size_t i=0;i<modes.size();++i) { EXPECT_EQ(sizes[i].first,modes[i].width); EXPECT_EQ(sizes[i].second,modes[i].height); }
    EXPECT_EQ(432,screen_scale::GetMode(-1).height);
}
TEST(ScreenScaleTests, ShortBuffersDoNotWriteDestination)
{
    const auto source=Source(); std::array<uint32_t,1> shortBuffer{sentinel}; std::vector<uint32_t> full(source.size(),sentinel);
    EXPECT_FALSE(screen_scale::Apply(0,shortBuffer,shortBuffer)); EXPECT_FALSE(screen_scale::Apply(0,source,shortBuffer));
    EXPECT_FALSE(screen_scale::Apply(0,shortBuffer,full)); EXPECT_EQ(sentinel,shortBuffer[0]);
    EXPECT_TRUE(std::ranges::all_of(full,[](auto pixel){return pixel==sentinel;}));
}
class ScreenScaleModes : public testing::TestWithParam<int> {};
TEST_P(ScreenScaleModes, OrientationEdgesInterpolationAndScanlines)
{
    const int mode=GetParam(); const auto source=Source(); const auto info=screen_scale::GetMode(mode);
    const size_t count=static_cast<size_t>(info.width)*info.height; std::vector<uint32_t> guarded(count+2,sentinel);
    auto pixels=std::span(guarded).subspan(1,count); ASSERT_TRUE(screen_scale::Apply(mode,source,pixels));
    EXPECT_EQ(sentinel,guarded.front()); EXPECT_EQ(sentinel,guarded.back()); EXPECT_EQ(source[287*640],pixels.front());
    if(mode==0) for(int y=0;y<288;++y) EXPECT_EQ(source[(287-y)*640+639],pixels[y*640+639]);
    else if(mode==1) EXPECT_EQ(Average(source[287*640],source[286*640]),pixels[640]);
    else if(mode==2 || mode==4 || mode==6 || mode==7) {
        const int factor=mode==6 || mode==7?3:2;
        for(int y=0;y<288;++y) {
            const size_t row=static_cast<size_t>(y)*factor*info.width;
            auto black=pixels.subspan(row+(factor-1)*info.width,info.width);
            EXPECT_TRUE(std::ranges::all_of(black,[](auto pixel){return pixel==0;}));
            EXPECT_EQ(source[(287-y)*640+639],pixels[row+info.width-1]);
        }
    }
    else if(mode==3) EXPECT_TRUE(std::equal(pixels.begin(),pixels.begin()+640,pixels.begin()+640));
    else if(mode==5) { EXPECT_EQ(pixels[0],pixels[960]); EXPECT_EQ(Average(source[287*640],source[286*640]),pixels[1920]); }
}
INSTANTIATE_TEST_SUITE_P(AllModes,ScreenScaleModes,testing::Range(0,8));
