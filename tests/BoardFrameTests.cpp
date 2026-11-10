/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include <gtest/gtest.h>
#include "emubase/Emubase.h"
#include <algorithm>
#include <array>
#include <memory>
#include <vector>

void DebugLog(LPCTSTR) {}
const TCHAR* REGISTER_NAME[] = {L"R0",L"R1",L"R2",L"R3",L"R4",L"R5",L"SP",L"PC"};
#ifdef _DEBUG
int AssertFailedLine(const char*,int) { std::abort(); }
#endif
namespace {
struct Capture
{
    CMotherboard* board=nullptr;
    uint64_t samples=0;
    unsigned tapeCalls=0, maxBatch=0, serialPolls=0, networkPolls=0;
    bool input=false, readBit=false, stopRead=false, stopWrite=false;
    uint8_t inputByte=0x5a;
    std::vector<uint32_t> tapeValues;
    std::vector<std::pair<uint16_t,uint16_t>> sound;
    std::vector<uint8_t> serial, network, parallel, terminal;
};
Capture* capture;
bool CALLBACK TapeRead(unsigned samples)
{
    capture->samples+=samples; ++capture->tapeCalls; capture->maxBatch=std::max(capture->maxBatch,samples);
    if (capture->stopRead) capture->board->SetTapeReadCallback(nullptr,0);
    return capture->readBit;
}
void CALLBACK TapeWrite(int value,unsigned samples)
{
    capture->samples+=samples; ++capture->tapeCalls; capture->maxBatch=std::max(capture->maxBatch,samples);
    capture->tapeValues.push_back(static_cast<uint32_t>(value));
    if (capture->stopWrite) capture->board->SetTapeWriteCallback(nullptr,0);
}
void CALLBACK Sound(unsigned short left,unsigned short right) { capture->sound.emplace_back(left,right); }
bool CALLBACK SerialIn(uint8_t* byte) { ++capture->serialPolls; *byte=capture->inputByte; return capture->input; }
bool CALLBACK NetworkIn(uint8_t* byte) { ++capture->networkPolls; *byte=capture->inputByte; return capture->input; }
bool CALLBACK SerialOut(uint8_t byte) { capture->serial.push_back(byte); return true; }
bool CALLBACK NetworkOut(uint8_t byte) { capture->network.push_back(byte); return true; }
bool CALLBACK Parallel(uint8_t byte) { capture->parallel.push_back(byte); return true; }
void CALLBACK Terminal(uint8_t byte) { capture->terminal.push_back(byte); }
}
class BoardFrames : public testing::Test
{
protected:
    std::unique_ptr<CMotherboard> board;
    Capture data;
    CMemoryController* cpuMem;
    CMemoryController* ppuMem;
    void SetUp() override
    {
        CProcessor::Init(); board=std::make_unique<CMotherboard>(); board->Reset();
        // Reset presses the reserved keyboard row; isolate synthetic key events.
        for (int column=0;column<8;++column)
            board->KeyboardEvent(static_cast<uint8_t>((column<<4)|3),false);
        board->GetCPU()->SetDCLOPin(true); board->GetPPU()->SetDCLOPin(true);
        cpuMem=board->GetCPUMemoryController(); ppuMem=board->GetPPUMemoryController();
        data.board=board.get(); capture=&data;
    }
    void TearDown() override { board.reset(); CProcessor::Done(); capture=nullptr; }
    void Start(CProcessor* cpu,uint16_t opcode=1)
    {
        cpu->SetACLOPin(true); cpu->SetDCLOPin(false); cpu->SetACLOPin(false); cpu->InterruptProcessing();
        cpu->SetPSW(0200); cpu->SetPC(01000); cpu->SetSP(0400); cpu->ClearInternalTick();
        cpu->GetMemoryController()->SetWord(01000,false,opcode);
    }
    uint16_t P(uint16_t address) { return ppuMem->GetWord(address,false,false); }
    uint16_t C(uint16_t address) { return cpuMem->GetWord(address,false,false); }
};
TEST_F(BoardFrames, SoundCadenceSilenceAndCallbackRemoval)
{
    board->SetSoundGenCallback(Sound); ASSERT_TRUE(board->SystemFrame());
    ASSERT_EQ(SAMPLERATE/FRAMERATE,data.sound.size());
    for (auto [left,right]:data.sound) { EXPECT_EQ(0,left); EXPECT_EQ(left,right); }
    ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(2*SAMPLERATE/FRAMERATE,data.sound.size());
    board->SetSoundGenCallback(nullptr); ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(2*SAMPLERATE/FRAMERATE,data.sound.size());
}
TEST_F(BoardFrames, ConstantSoundCountsOneRisingEdgeNotEverySample)
{
    board->SetSoundGenCallback(Sound); board->SetSound(0200); ASSERT_TRUE(board->SystemFrame());
    for (auto [left,right]:data.sound) { EXPECT_EQ(0x7fff,left); EXPECT_EQ(left,right); }
    EXPECT_EQ(1,board->GetSoundChanges()); ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(0,board->GetSoundChanges());
    board->SetSound(0); ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(0,board->GetSoundChanges());
    board->SetSound(0200); ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(1,board->GetSoundChanges());
}
TEST_F(BoardFrames, CovoxInvertsPrinterByteAndMixesBothChannels)
{
    board->SetSoundGenCallback(Sound); board->SetSoundCovox(true);
    ppuMem->SetWord(0177100,false,0x80); ASSERT_TRUE(board->SystemFrame());
    for (auto [left,right]:data.sound) { EXPECT_EQ(0x3f80,left); EXPECT_EQ(left,right); }
    data.sound.clear(); board->SetSound(0200); ASSERT_TRUE(board->SystemFrame());
    for (auto [left,right]:data.sound) { EXPECT_EQ(0x407f,left); EXPECT_EQ(left,right); }
}
class TapeRates : public BoardFrames,public testing::WithParamInterface<int> {};
TEST_P(TapeRates, WriteSamplingPreservesFractionAcrossFramesAndBatchesHighRates)
{
    const int rate=GetParam(); board->SetTapeWriteCallback(TapeWrite,rate);
    for (int frame=1;frame<=FRAMERATE;++frame)
    {
        ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(static_cast<uint64_t>(rate)*frame/FRAMERATE,data.samples);
    }
    EXPECT_EQ(static_cast<uint64_t>(rate),data.samples);
    EXPECT_EQ(static_cast<unsigned>((static_cast<uint64_t>(rate)+500000-1)/500000),data.maxBatch);
}
INSTANTIATE_TEST_SUITE_P(BoundaryRates,TapeRates,testing::Values(1,44101,500000,1000000,INT_MAX));
TEST_F(BoardFrames, TapeWriteOutputLevelsAndSwitchToRead)
{
    board->SetTapeWriteCallback(TapeWrite,1000000); ppuMem->SetWord(0177716,false,2);
    ASSERT_TRUE(board->SystemFrame()); ASSERT_FALSE(data.tapeValues.empty());
    EXPECT_TRUE(std::ranges::all_of(data.tapeValues,[](auto v){return v==0xffffffff;}));
    data.tapeValues.clear(); ppuMem->SetWord(0177716,false,0); ASSERT_TRUE(board->SystemFrame());
    EXPECT_TRUE(std::ranges::all_of(data.tapeValues,[](auto v){return v==0;}));
    data.tapeValues.clear(); board->SetTapeReadCallback(TapeRead,1000); data.samples=0;
    ASSERT_TRUE(board->SystemFrame()); EXPECT_TRUE(data.tapeValues.empty()); EXPECT_EQ(20,data.samples);
}
TEST_F(BoardFrames, TapeCallbacksCanRemoveThemselvesDuringFrame)
{
    data.stopRead=true; board->SetTapeReadCallback(TapeRead,500000); ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(1,data.tapeCalls);
    data.tapeCalls=0; data.stopWrite=true; board->SetTapeWriteCallback(TapeWrite,500000);
    ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(1,data.tapeCalls); EXPECT_EQ(1,data.tapeValues.size());
}
TEST_F(BoardFrames, InvalidRatesDisableCallbacksAndReplacementResetsPhase)
{
    for (int rate : {0,-1})
    {
        board->SetTapeReadCallback(TapeRead,rate); ASSERT_TRUE(board->SystemFrame());
        board->SetTapeWriteCallback(TapeWrite,rate); ASSERT_TRUE(board->SystemFrame());
    }
    EXPECT_EQ(0,data.tapeCalls);
    board->SetTapeReadCallback(TapeRead,1); for(int i=0;i<49;++i) ASSERT_TRUE(board->SystemFrame());
    EXPECT_EQ(0,data.samples); board->SetTapeWriteCallback(TapeWrite,1);
    ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(0,data.samples);
    for(int i=0;i<49;++i) ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(1,data.samples);
}
TEST_F(BoardFrames, TapeReadTransitionSetsExternalTimerEvent)
{
    ppuMem->SetWord(0177716,false,4); data.readBit=false; board->SetTapeReadCallback(TapeRead,1000);
    ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(1,P(0177716)&1); EXPECT_NE(0,board->GetTimerState()&040);
}
TEST_F(BoardFrames, SerialAndNetworkInputPollAtTheirFrameCadences)
{
    board->SetSerialCallbacks(SerialIn,SerialOut); board->SetNetworkCallbacks(NetworkIn,NetworkOut);
    ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(25,data.serialPolls); EXPECT_EQ(157,data.networkPolls);
    EXPECT_TRUE(data.serial.empty()); EXPECT_TRUE(data.network.empty());
    board->SetSerialCallbacks(nullptr,SerialOut); board->SetNetworkCallbacks(NetworkIn,nullptr);
    ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(25,data.serialPolls); EXPECT_EQ(157,data.networkPolls);
}
TEST_F(BoardFrames, InputCallbacksDeliverBytesAndLatchReceiverInterrupts)
{
    Start(board->GetCPU()); data.input=true;
    cpuMem->SetWord(0176570,false,0100); cpuMem->SetWord(0176560,false,0100);
    board->SetSerialCallbacks(SerialIn,SerialOut); board->SetNetworkCallbacks(NetworkIn,NetworkOut);
    ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(0370,board->GetCPU()->GetVIRQ(7)); EXPECT_EQ(0360,board->GetCPU()->GetVIRQ(9));
    EXPECT_EQ(0x5a,C(0176572)&255); EXPECT_EQ(0x5a,C(0176562)&255);
}
TEST_F(BoardFrames, OutputCallbacksDeliverOnceAndRestoreReadyAndInterrupts)
{
    Start(board->GetCPU()); cpuMem->SetWord(0176574,false,0100); cpuMem->SetWord(0176564,false,0100);
    cpuMem->SetWord(0176576,false,0xab); cpuMem->SetWord(0176566,false,0xcd);
    board->SetSerialCallbacks(SerialIn,SerialOut); board->SetNetworkCallbacks(NetworkIn,NetworkOut);
    ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ((std::vector<uint8_t>{0xab}),data.serial); EXPECT_EQ((std::vector<uint8_t>{0xcd}),data.network);
    EXPECT_NE(0,C(0176574)&0200); EXPECT_NE(0,C(0176564)&0200);
    EXPECT_EQ(0374,board->GetCPU()->GetVIRQ(8)); EXPECT_EQ(0364,board->GetCPU()->GetVIRQ(10));
    ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(1,data.serial.size()); EXPECT_EQ(1,data.network.size());
}
TEST_F(BoardFrames, LoopbackBypassesHostCallbacksAndReturnsTransmittedData)
{
    board->SetSerialCallbacks(SerialIn,SerialOut); board->SetNetworkCallbacks(NetworkIn,NetworkOut);
    cpuMem->SetWord(0176574,false,4); cpuMem->SetWord(0176564,false,4);
    cpuMem->SetWord(0176576,false,0x12); cpuMem->SetWord(0176566,false,0x34);
    ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(0,data.serialPolls); EXPECT_EQ(0,data.networkPolls);
    EXPECT_TRUE(data.serial.empty()); EXPECT_TRUE(data.network.empty()); EXPECT_EQ(0x12,C(0176572)&255); EXPECT_EQ(0x34,C(0176562)&255);
}
TEST_F(BoardFrames, ParallelStrobeHandshakeAndCovoxExclusion)
{
    board->SetParallelOutCallback(Parallel); EXPECT_NE(0,P(0177101)&2);
    ASSERT_TRUE(board->SystemFrame()); EXPECT_TRUE(data.parallel.empty()); EXPECT_EQ(0,P(0177101)&0x80);
    ppuMem->SetWord(0177100,false,0x42); ppuMem->SetWord(0177102,false,0);
    ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ((std::vector<uint8_t>{0x42}),data.parallel); EXPECT_NE(0,P(0177101)&0x80);
    ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(1,data.parallel.size());
    board->SetSoundCovox(true); ppuMem->SetWord(0177102,false,0x80); ASSERT_TRUE(board->SystemFrame());
    EXPECT_NE(0,P(0177101)&0x80); EXPECT_EQ(1,data.parallel.size());
    board->SetParallelOutCallback(nullptr); EXPECT_EQ(0,P(0177101)&2);
}
TEST_F(BoardFrames, TerminalCallbackObservesOnlyCpuChannelZero)
{
    board->SetTerminalCallback(Terminal); board->ChanWriteByCPU(0,0x12); board->ChanWriteByCPU(1,0x34); board->ChanWriteByCPU(2,0x56);
    board->ChanWriteByPPU(0,0x78); EXPECT_EQ((std::vector<uint8_t>{0x12}),data.terminal);
    board->SetTerminalCallback(nullptr); board->ChanWriteByCPU(0,0x90); EXPECT_EQ(1,data.terminal.size());
}
TEST_F(BoardFrames, KeyboardScanWaitsForAcknowledgeThenReportsRowRelease)
{
    board->KeyboardEvent(0x2a,true); ASSERT_TRUE(board->SystemFrame()); EXPECT_NE(0,P(0177700)&0200);
    const auto scanned=board->GetScannedKey(); ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(scanned,board->GetScannedKey());
    EXPECT_EQ(0x2a,P(0177702)); board->KeyboardEvent(0x2a,false);
    ASSERT_TRUE(board->SystemFrame()); EXPECT_NE(0,P(0177700)&0200); EXPECT_EQ(0x8a,P(0177702));
}
TEST_F(BoardFrames, HeldKeyboardRowDoesNotRepeatAndReleasesOnlyWhenEmpty)
{
    board->KeyboardEvent(0x2a,true); board->KeyboardEvent(0x3a,true); ASSERT_TRUE(board->SystemFrame());
    EXPECT_EQ(0x2a,P(0177702)); ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(0,P(0177700)&0200);
    board->KeyboardEvent(0x2a,false); ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(0,P(0177700)&0200);
    board->KeyboardEvent(0x3a,false); ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(0x8a,P(0177702));
}
TEST_F(BoardFrames, KeyboardScanRaisesEnabledInterrupt)
{
    Start(board->GetPPU()); ppuMem->SetWord(0177700,false,0100); board->KeyboardEvent(0x01,true);
    ASSERT_TRUE(board->SystemFrame()); EXPECT_EQ(0300,board->GetPPU()->GetVIRQ(3));
    EXPECT_EQ(1,P(0177702)); EXPECT_EQ(0,board->GetPPU()->GetVIRQ(3));
}
TEST_F(BoardFrames, CpuBreakpointStopsBeforePpuOrCallbacksRun)
{
    Start(board->GetCPU(),0005200); Start(board->GetPPU(),0005200);
    const std::array<uint16_t,1> bp{01002}; board->SetCPUBreakpoints(bp);
    board->SetSoundGenCallback(Sound); board->SetTapeReadCallback(TapeRead,500000);
    ASSERT_FALSE(board->SystemFrame()); EXPECT_EQ(1,board->GetCPU()->GetReg(0)); EXPECT_EQ(0,board->GetPPU()->GetReg(0));
    EXPECT_TRUE(data.sound.empty()); EXPECT_EQ(0,data.tapeCalls);
}
TEST_F(BoardFrames, PpuBreakpointStopsAfterBothFirstInstructions)
{
    Start(board->GetCPU(),0005200); Start(board->GetPPU(),0005200);
    const std::array<uint16_t,1> bp{01002}; board->SetPPUBreakpoints(bp);
    ASSERT_FALSE(board->SystemFrame()); EXPECT_EQ(1,board->GetCPU()->GetReg(0)); EXPECT_EQ(1,board->GetPPU()->GetReg(0));
}
TEST_F(BoardFrames, NonmatchingBreakpointsPreserveFrameExecution)
{
    Start(board->GetCPU(),0005200); Start(board->GetPPU(),0005200);
    cpuMem->SetWord(01002,false,0000776); ppuMem->SetWord(01002,false,0000776); // BR back to INC
    std::vector<uint8_t> initial(UKNCIMAGE_SIZE),fast(UKNCIMAGE_SIZE),checked(UKNCIMAGE_SIZE);
    board->SaveToImage(std::span<uint8_t,UKNCIMAGE_SIZE>(initial.data(),initial.size())); ASSERT_TRUE(board->SystemFrame());
    board->SaveToImage(std::span<uint8_t,UKNCIMAGE_SIZE>(fast.data(),fast.size()));
    board->LoadFromImage(std::span<const uint8_t,UKNCIMAGE_SIZE>(initial.data(),initial.size()));
    const std::array<uint16_t,1> bp{04000}; board->SetCPUBreakpoints(bp); board->SetPPUBreakpoints(bp);
    ASSERT_TRUE(board->SystemFrame()); board->SaveToImage(std::span<uint8_t,UKNCIMAGE_SIZE>(checked.data(),checked.size()));
    EXPECT_EQ(fast,checked); EXPECT_GT(board->GetCPU()->GetReg(0),0); EXPECT_GT(board->GetPPU()->GetReg(0),0);
}
