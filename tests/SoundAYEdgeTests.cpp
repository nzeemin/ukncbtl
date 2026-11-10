/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include <gtest/gtest.h>
#include "emubase/Emubase.h"
#include <algorithm>
#include <array>
#include <vector>

namespace {
std::vector<uint8_t> Render(CSoundAY& ay,size_t size) {
    std::vector<uint8_t> result(size); ay.Callback(result); return result;
}
void Configure(CSoundAY& ay,int mixer=0,int shape=10) {
    for(int channel=0;channel<3;++channel) { ay.SetReg(channel*2,17+channel*12); ay.SetReg(channel*2+1,0); ay.SetReg(8+channel,16); }
    ay.SetReg(6,13); ay.SetReg(7,mixer); ay.SetReg(11,7); ay.SetReg(12,0); ay.SetReg(13,shape);
}
std::vector<uint8_t> Split(CSoundAY& ay,size_t size) {
    std::vector<uint8_t> result(size); size_t offset=0;
    for(size_t chunk=1;offset<size;chunk=chunk%23+1) {
        const auto count=std::min(chunk,size-offset); ay.Callback(std::span(result).subspan(offset,count)); offset+=count;
    }
    return result;
}
class Probe : public CSoundAY {
public:
    void NotReady() { ready=0; }
    auto Registers() const { return Regs; }
};
}
class AyChunks : public testing::TestWithParam<int> {};
TEST_P(AyChunks, OutputAndLaterEnabledPhaseDoNotDependOnChunkSizes) {
    CSoundAY whole,split; Configure(whole,GetParam()); Configure(split,GetParam());
    EXPECT_EQ(Render(whole,4099),Split(split,4099));
    whole.SetReg(7,0); split.SetReg(7,0);
    EXPECT_EQ(Render(whole,1024),Render(split,1024));
}
INSTANTIATE_TEST_SUITE_P(MixerMasks,AyChunks,testing::Range(0,64));
class AyShapes : public testing::TestWithParam<int> {};
TEST_P(AyShapes, RewritingShapeRestartsEnvelopeEvenAfterHold) {
    CSoundAY ay,fresh; Configure(ay,63,GetParam()); Configure(fresh,63,GetParam());
    Render(ay,10000); ay.SetReg(13,GetParam());
    EXPECT_EQ(Render(fresh,2048),Render(ay,2048));
}
INSTANTIATE_TEST_SUITE_P(AllShapes,AyShapes,testing::Range(0,16));
TEST(AyEdges, EmptySpanDoesNotAdvanceState) {
    CSoundAY ay,reference; Configure(ay); Configure(reference);
    EXPECT_EQ(Render(ay,17),Render(reference,17)); ay.Callback({});
    EXPECT_EQ(Render(ay,4096),Render(reference,4096));
}
TEST(AyEdges, SpanBoundariesPreserveGuardsAndWriteEverySample) {
    for(size_t size : {size_t{0},size_t{1},size_t{2},size_t{3},size_t{441},size_t{65537}}) {
        SCOPED_TRACE(size); CSoundAY ay; std::vector<uint8_t> guarded(size+2,0xa5);
        ay.Callback(std::span(guarded).subspan(1,size)); EXPECT_EQ(0xa5,guarded.front()); EXPECT_EQ(0xa5,guarded.back());
        EXPECT_TRUE(std::all_of(guarded.begin()+1,guarded.end()-1,[](auto value){return value==0;}));
    }
}
TEST(AyEdges, NotReadyFillsOnlyRequestedSpanWithSilence) {
    Probe ay; Configure(ay); ay.NotReady(); std::array<uint8_t,9> data; data.fill(0xa5);
    ay.Callback(std::span(data).subspan(1,7)); EXPECT_EQ(0xa5,data.front()); EXPECT_EQ(0xa5,data.back());
    EXPECT_TRUE(std::all_of(data.begin()+1,data.end()-1,[](auto value){return value==0;}));
}
TEST(AyEdges, RegisterMasksIgnoreUnusedBits) {
    constexpr std::array<int,16> masks{255,15,255,15,255,15,31,255,31,31,31,255,255,15,255,255};
    for(int reg=0;reg<16;++reg) for(int value=0;value<256;++value) {
        SCOPED_TRACE(reg);
        SCOPED_TRACE(value);
        Probe raw,masked; Configure(raw); Configure(masked);
        raw.SetReg(reg,value); masked.SetReg(reg,value&masks[reg]);
        EXPECT_EQ(raw.Registers(),masked.Registers()); EXPECT_EQ(Render(raw,128),Render(masked,128));
    }
}
TEST(AyEdges, ZeroAndOneToneAndNoisePeriodsAreEquivalent) {
    for(int reg : {0,2,4,6}) {
        CSoundAY zero,one; Configure(zero); Configure(one); zero.SetReg(reg,0); one.SetReg(reg,1);
        EXPECT_EQ(Render(zero,2048),Render(one,2048));
    }
}
TEST(AyEdges, ResetRestoresNoiseEnvelopeAndToneSequence) {
    CSoundAY used,fresh; Configure(used); Render(used,12345); used.Reset();
    EXPECT_EQ(Render(used,100),Render(fresh,100)); Configure(used); Configure(fresh);
    EXPECT_EQ(Render(used,8192),Render(fresh,8192));
}
TEST(AyEdges, MutedChannelPhaseDoesNotDependOnChunkSizes) {
    for(int channel=0;channel<3;++channel) {
        CSoundAY whole,split; Configure(whole); Configure(split);
        whole.SetReg(8+channel,0); split.SetReg(8+channel,0);
        EXPECT_EQ(Render(whole,11),Split(split,11));
        whole.SetReg(8+channel,15); split.SetReg(8+channel,15);
        EXPECT_EQ(Render(whole,2048),Render(split,2048));
    }
}
TEST(AyEdges, MidRenderRegisterChangesPreserveChunkContinuity) {
    CSoundAY whole,split; Configure(whole); Configure(split);
    constexpr std::array<std::pair<int,int>,12> changes{{{0,1},{2,255},{4,3},{6,0},{7,63},{8,0},{9,15},{11,0},{12,2},{13,14},{7,0},{8,16}}};
    for(auto [reg,value]:changes) {
        EXPECT_EQ(Render(whole,37),Split(split,37)); whole.SetReg(reg,value); split.SetReg(reg,value);
    }
    EXPECT_EQ(Render(whole,2048),Split(split,2048));
}
TEST(AyEdges, DisabledToneAndNoiseGiveConstantOutputForAllPeriods) {
    for(int period : {0,1,17,4095}) {
        CSoundAY ay; ay.SetReg(7,63);
        for(int channel=0;channel<3;++channel) { ay.SetReg(channel*2,period&255); ay.SetReg(channel*2+1,period>>8); ay.SetReg(8+channel,15); }
        auto output=Render(ay,8192); EXPECT_TRUE(std::all_of(output.begin(),output.end(),[](auto v){return v==255;}));
    }
}
TEST(AyEdges, PortRegistersDoNotAffectAudio) {
    CSoundAY ay,reference; Configure(ay); Configure(reference);
    for(int value=0;value<256;++value) {
        ay.SetReg(14,value); ay.SetReg(15,255-value); EXPECT_EQ(Render(ay,17),Render(reference,17));
    }
}
