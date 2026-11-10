/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include <gtest/gtest.h>
#include "util/ScreenScale.h"
#include <array>
#include <vector>
#include <algorithm>
extern int g_SourceWidth,g_SourceHeight;
void CALLBACK PrepareScreenCopy(const void*,void*);
void CALLBACK PrepareScreenUpscale(const void*,void*);
void CALLBACK PrepareScreenUpscale2(const void*,void*);
void CALLBACK PrepareScreenUpscale2d(const void*,void*);
void CALLBACK PrepareScreenUpscale3(const void*,void*);
void CALLBACK PrepareScreenUpscale4(const void*,void*);
void CALLBACK PrepareScreenUpscale175(const void*,void*);
void CALLBACK PrepareScreenUpscale5(const void*,void*);
namespace {
uint32_t Mix(uint32_t a,uint32_t b) {
    const auto blue=((a&255)+(b&255))/2;
    const auto green=(((a>>8)&255)/2+((b>>8)&255)/2)<<8;
    const auto red=(((a>>16)&255)/2+((b>>16)&255)/2)<<16;
    return red|green|blue;
}
std::vector<uint32_t> Source() {
    std::vector<uint32_t> pixels(640*288);
    for(int y=0;y<288;++y) for(int x=0;x<640;++x)
        pixels[y*640+x]=((x*37+y*13)&255)|(((x*11+y*29)&255)<<8)|(((x*53+y*7)&255)<<16);
    return pixels;
}
uint32_t Expected(int mode,int x,int y,const std::vector<uint32_t>& source) {
    const auto pixel=[&](int sx,int sy){return source[(287-sy)*640+sx];};
    if(mode==0) return pixel(x,y);
    if(mode==1) return y%3==1?Mix(pixel(x,y/3*2),pixel(x,y/3*2+1)):pixel(x,y/3*2+(y%3==2));
    if(mode==2 || mode==3) return mode==2 && y%2 ? 0 : pixel(x,y/2);
    if(mode==7) return y%3==2 ? 0 : pixel(x/2,y/3);
    if(mode==6) {
        if(y%3==2) return 0;
        const int sx=x/7*4+x%7/2; return x%7%2 ? Mix(pixel(sx,y/3),pixel(sx+1,y/3)) : pixel(sx,y/3);
    }
    if(mode==4 && y%2) return 0;
    const int sx=x/3*2; const int sy=mode==4?y/2:y/5*2;
    const auto vertical=[&](int xx) {
        if(mode==4 || y%5<2) return pixel(xx,sy);
        if(y%5==2) return Mix(pixel(xx,sy),pixel(xx,sy+1));
        return pixel(xx,sy+1);
    };
    return x%3==1?Mix(vertical(sx),vertical(sx+1)):vertical(sx+(x%3==2));
}
}
class ScalePixels : public testing::TestWithParam<int> {};
TEST_P(ScalePixels, EveryPixelMatchesScalarReferenceAndPreservesBuffers) {
    auto source=Source(); const auto original=source; const int mode=GetParam(); const auto info=screen_scale::GetMode(mode);
    const size_t count=info.width*info.height; std::vector<uint32_t> dest(count+3,0xdeadbeef);
    ASSERT_TRUE(screen_scale::Apply(mode,source,std::span(dest).subspan(1,count+1)));
    EXPECT_EQ(source,original); EXPECT_EQ(0xdeadbeef,dest[0]); EXPECT_EQ(0xdeadbeef,dest[count+1]); EXPECT_EQ(0xdeadbeef,dest[count+2]);
    for(int y=0;y<info.height;++y) for(int x=0;x<info.width;++x)
        ASSERT_EQ(Expected(mode,x,y,source),dest[1+y*info.width+x]) << "x="<<x<<" y="<<y;
}
TEST_P(ScalePixels, ShortInputsAreRejectedBeforeWriting) {
    auto source=Source(); const auto info=screen_scale::GetMode(GetParam()); std::vector<uint32_t> dest(info.width*info.height,0xdeadbeef);
    EXPECT_FALSE(screen_scale::Apply(GetParam(),std::span(source).first(source.size()-1),dest));
    EXPECT_FALSE(screen_scale::Apply(GetParam(),source,std::span(dest).first(dest.size()-1)));
    EXPECT_TRUE(std::ranges::all_of(dest,[](auto value){return value==0xdeadbeef;}));
}
TEST_P(ScalePixels, RendererDuplicateMatchesCompleteSharedOutput) {
    const std::array callbacks{PrepareScreenCopy,PrepareScreenUpscale,PrepareScreenUpscale2,PrepareScreenUpscale2d,PrepareScreenUpscale3,PrepareScreenUpscale4,PrepareScreenUpscale175,PrepareScreenUpscale5};
    g_SourceWidth=640; g_SourceHeight=288; const auto source=Source(); const auto info=screen_scale::GetMode(GetParam());
    std::vector<uint32_t> expected(info.width*info.height),actual(expected.size()+2,0xdeadbeef);
    ASSERT_TRUE(screen_scale::Apply(GetParam(),source,expected)); callbacks[GetParam()](source.data(),actual.data()+1);
    EXPECT_EQ(0xdeadbeef,actual.front()); EXPECT_EQ(0xdeadbeef,actual.back());
    EXPECT_TRUE(std::equal(expected.begin(),expected.end(),actual.begin()+1));
}
INSTANTIATE_TEST_SUITE_P(AllModes,ScalePixels,testing::Range(0,8));
TEST(ScaleEdges, InvalidModesUseDefaultDimensionsAndPixels) {
    const auto source=Source(); const auto info=screen_scale::GetMode(1); std::vector<uint32_t> expected(info.width*info.height),actual(expected.size());
    ASSERT_TRUE(screen_scale::Apply(1,source,expected));
    for(int mode : {-1,8,INT_MIN,INT_MAX}) {
        EXPECT_EQ(info.width,screen_scale::GetMode(mode).width); EXPECT_EQ(info.height,screen_scale::GetMode(mode).height);
        ASSERT_TRUE(screen_scale::Apply(mode,source,actual)); EXPECT_EQ(expected,actual);
    }
}
TEST(ScaleEdges, AllChannelPairsPreserveLegacyRoundingWithoutCrossChannelCarry) {
    std::vector<uint32_t> source(640*288),output(640*432);
    for(int shift : {0,8,16}) {
        for(int a=0;a<256;++a) for(int b=0;b<256;++b) {
            const int n=a*256+b,x=n%640,y=n/640;
            source[(287-y*2)*640+x]=static_cast<uint32_t>(a)<<shift;
            source[(286-y*2)*640+x]=static_cast<uint32_t>(b)<<shift;
        }
        ASSERT_TRUE(screen_scale::Apply(1,source,output));
        for(int a=0;a<256;++a) for(int b=0;b<256;++b) {
            const int n=a*256+b,x=n%640,y=n/640;
            EXPECT_EQ(static_cast<uint32_t>(shift==0?(a+b)/2:a/2+b/2)<<shift,output[(y*3+1)*640+x]);
        }
        std::fill(source.begin(),source.end(),0);
    }
}
