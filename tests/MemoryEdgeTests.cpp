/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include <gtest/gtest.h>
#include "emubase/Emubase.h"
#include <memory>

void DebugLog(LPCTSTR) {}
const TCHAR* REGISTER_NAME[] = {L"R0",L"R1",L"R2",L"R3",L"R4",L"R5",L"SP",L"PC"};
#ifdef _DEBUG
int AssertFailedLine(const char*,int) { std::abort(); }
#endif
class MemoryEdges : public testing::Test
{
protected:
    std::unique_ptr<CMotherboard> board;
    CFirstMemoryController* cpu;
    CSecondMemoryController* ppu;
    void SetUp() override
    {
        CProcessor::Init(); board=std::make_unique<CMotherboard>(); board->Reset();
        cpu=static_cast<CFirstMemoryController*>(board->GetCPUMemoryController());
        ppu=static_cast<CSecondMemoryController*>(board->GetPPUMemoryController());
        board->GetPPU()->InterruptProcessing(); ppu->SetWord(0177716,false,0100000); board->GetCPU()->InterruptProcessing();
        for (auto* processor : {board->GetCPU(),board->GetPPU()}) { processor->SetPSW(0); processor->SetPC(01000); processor->SetSP(0400); }
    }
    void TearDown() override { board.reset(); CProcessor::Done(); }
    uint16_t C(uint16_t address) { return cpu->GetWord(address,false,false); }
    uint16_t P(uint16_t address) { return ppu->GetWord(address,false,false); }
    static int Signed7(unsigned value) { return value&64 ? int(value)-128 : int(value); }
};
TEST_F(MemoryEdges, CpuMapAllAddressesUserHaltAndExecution)
{
    for (unsigned address=0;address<65536;++address)
        for (bool halt : {false,true})
            for (bool execute : {false,true})
            {
                uint16_t offset=0;
                const int type=cpu->TranslateAddress(static_cast<uint16_t>(address),halt,execute,&offset,true);
                ASSERT_EQ(halt || address<0160000 ? ADDRTYPE_RAM12 : ADDRTYPE_IO,type) << address;
                ASSERT_EQ(address,offset);
            }
}
TEST_F(MemoryEdges, PpuWindowPriorityAndBoundaryMapping)
{
    for (unsigned control=0;control<256;++control)
    {
        ppu->SetWord(0177054,false,static_cast<uint16_t>(control));
        for (uint16_t address : {0,077777,0100000,0117777,0120000,0137777,0140000,0157777,0160000,0176777,0177000,0177777})
            for (bool execute : {false,true})
            {
                uint16_t offset=0;
                const int window=address>>13;
                const int expected=address>=0177000 ? (execute?ADDRTYPE_DENY:ADDRTYPE_IO) :
                    window<4 || (control & (1u<<window)) ? ADDRTYPE_RAM0 :
                    window>4 || (control&1) ? ADDRTYPE_ROM : (control&14) ? ADDRTYPE_DENY : ADDRTYPE_NONE;
                ASSERT_EQ(expected,ppu->TranslateAddress(address,false,execute,&offset,true)) << control << ':' << address;
                if (expected==ADDRTYPE_RAM0) EXPECT_EQ(address,offset);
                if (expected==ADDRTYPE_ROM) EXPECT_EQ(address-0100000,offset);
            }
    }
}
TEST_F(MemoryEdges, DebuggerAccessDoesNotTriggerCpuAddressWatchInterrupt)
{
    cpu->SetWord(0176646,false,02000); cpu->SetWord(0176644,false,0x101|0100);
    cpu->GetWordView(02000,false,false); EXPECT_EQ(0,board->GetCPU()->GetVIRQ(6));
    cpu->GetWord(02000,false,false); EXPECT_EQ(0100,board->GetCPU()->GetVIRQ(6));
    cpu->SetWord(0176644,false,0); EXPECT_EQ(0,board->GetCPU()->GetVIRQ(6));
}
TEST_F(MemoryEdges, AddressWatchRequiresExactAddressAndConfiguredMode)
{
    cpu->SetWord(0176646,false,02000); cpu->SetWord(0176644,false,0x103|0100);
    cpu->GetByte(02000,false); cpu->GetWord(02002,true,false); EXPECT_EQ(0,board->GetCPU()->GetVIRQ(6));
    cpu->SetByte(02000,true,1); EXPECT_EQ(0100,board->GetCPU()->GetVIRQ(6));
    cpu->SetWord(0176644,false,0x103|0120); EXPECT_EQ(0120,board->GetCPU()->GetVIRQ(6));
}
TEST_F(MemoryEdges, DebuggerUnknownPortAndDeniedExecutionHaveNoBusSideEffects)
{
    EXPECT_EQ(ADDRTYPE_IO,cpu->GetWordView(0177000,false,false).addressType);
    EXPECT_EQ(ADDRTYPE_DENY,ppu->GetWordView(0177700,false,true).addressType);
    EXPECT_FALSE(board->GetCPU()->InterruptProcessing()); EXPECT_FALSE(board->GetPPU()->InterruptProcessing());
    ppu->GetWord(0177700,false,true); EXPECT_TRUE(board->GetPPU()->InterruptProcessing());
}
TEST_F(MemoryEdges, CpuPlaneByteWritesPreserveOtherPlaneAtLastAddress)
{
    cpu->SetWord(0176640,false,65535); cpu->SetWord(0176642,false,0x1234);
    cpu->SetByte(0176642,false,0xab); EXPECT_EQ(0x12ab,C(0176642));
    cpu->SetByte(0176643,false,0xcd); EXPECT_EQ(0xcdab,C(0176642));
    EXPECT_EQ(0xab,board->GetRAMByte(1,65535)); EXPECT_EQ(0xcd,board->GetRAMByte(2,65535));
    EXPECT_EQ(0xab,cpu->GetByte(0176642,false)); EXPECT_EQ(0xcd,cpu->GetByte(0176643,false));
}
TEST_F(MemoryEdges, PpuPlaneByteWritesPreserveOtherPlane)
{
    ppu->SetWord(0177010,false,65535); ppu->SetWord(0177014,false,0x1234);
    ppu->SetByte(0177014,false,0xab); EXPECT_EQ(0x12ab,P(0177014));
    ppu->SetByte(0177015,false,0xcd); EXPECT_EQ(0xcdab,P(0177014));
    EXPECT_EQ(0xab,board->GetRAMByte(1,65535)); EXPECT_EQ(0xcd,board->GetRAMByte(2,65535));
    EXPECT_EQ(0xab,ppu->GetByte(0177014,false)); EXPECT_EQ(0xcd,ppu->GetByte(0177015,false));
}
TEST_F(MemoryEdges, PlaneAddressByteWritesUseBusLaneValue)
{
    for (auto pair : {std::pair<CMemoryController*,uint16_t>{cpu,0176640},{ppu,0177010}})
    {
        pair.first->SetWord(pair.second,false,0xffff);
        pair.first->SetByte(pair.second,false,0x12); EXPECT_EQ(0x12,pair.first->GetWord(pair.second,false,false));
        pair.first->SetByte(pair.second+1,false,0x34); EXPECT_EQ(0x3400,pair.first->GetWord(pair.second,false,false));
    }
}
class ReceiverEdges : public MemoryEdges,public testing::WithParamInterface<bool> {};
TEST_P(ReceiverEdges, OverflowKeepsFirstByteAndDataReadClearsReadyAndOverflow)
{
    const bool network=GetParam(); const uint16_t status=network?0176560:0176570;
    auto input=[&](uint8_t byte){return network?cpu->NetworkInput(byte):cpu->SerialInput(byte);};
    cpu->SetWord(status,false,0100); EXPECT_TRUE(input(0x12)); EXPECT_FALSE(input(0x34));
    EXPECT_EQ(010300,C(status)&010300);
    EXPECT_EQ(0,cpu->GetWordView(status+2,false,false).value); EXPECT_EQ(010300,C(status)&010300);
    EXPECT_EQ(0x12,C(status+2)&255); EXPECT_EQ(0100,C(status)&010300);
    EXPECT_TRUE(input(0x56)); EXPECT_EQ(0x56,cpu->GetByte(status+2,false)); EXPECT_EQ(0100,C(status)&010300);
}
TEST_P(ReceiverEdges, OddByteDataReadAcknowledgesReceiver)
{
    const bool network=GetParam(); const uint16_t status=network?0176560:0176570;
    cpu->SetWord(status,false,0100);
    if (network) cpu->NetworkInput(0xab); else cpu->SerialInput(0xab);
    cpu->GetByte(status+3,false); EXPECT_EQ(0,C(status)&0200);
    cpu->SetByte(status+1,false,0xff); EXPECT_EQ(0,C(status)&0100);
}
INSTANTIATE_TEST_SUITE_P(SerialAndNetwork,ReceiverEdges,testing::Bool());
TEST_F(MemoryEdges, TransmitterRegisterMasksAndDataWriteClearReady)
{
    for (uint16_t status : {0176564,0176574})
    {
        cpu->SetWord(status,false,0xffff); EXPECT_EQ(0305,C(status)&0377);
        cpu->SetByte(status+2,false,0xab); EXPECT_EQ(0105,C(status)&0377);
        cpu->SetByte(status+1,false,0xff); EXPECT_EQ(0,C(status)&0105);
    }
}
TEST_F(MemoryEdges, KeyboardDebuggerReadDoesNotAcknowledgeButByteReadDoes)
{
    ppu->SetWord(0177700,false,0100); ppu->KeyboardEvent(0x2a,true);
    EXPECT_EQ(0300,board->GetPPU()->GetVIRQ(3)); EXPECT_EQ(0,ppu->GetWordView(0177702,false,false).value);
    EXPECT_EQ(0200,P(0177700)&0200); EXPECT_EQ(0300,board->GetPPU()->GetVIRQ(3));
    EXPECT_EQ(0x2a,ppu->GetByte(0177702,false)); EXPECT_EQ(0,P(0177700)&0200); EXPECT_EQ(0,board->GetPPU()->GetVIRQ(3));
}
TEST_F(MemoryEdges, MouseAccumulatesClampsAndDrainsInSignedChunks)
{
    board->SetMouse(true); board->MouseMove(300,-300,true,true);
    for (int expected : {63,63,63,63,4,0})
    {
        const auto word=P(0177400); EXPECT_EQ(expected,Signed7((word>>1)&127)); EXPECT_EQ(expected,Signed7((word>>9)&127));
        EXPECT_EQ(0x0101,word&0x0101);
    }
    board->MouseMove(-100,100,false,false);
    for (int expected : {-63,-37,0})
    {
        const auto word=P(0177400); EXPECT_EQ(expected,Signed7((word>>1)&127)); EXPECT_EQ(expected,Signed7((word>>9)&127)); EXPECT_EQ(0,word&0x0101);
    }
}
TEST_F(MemoryEdges, MouseDebuggerReadDoesNotConsumeMovement)
{
    board->SetMouse(true); board->MouseMove(10,20,false,true);
    EXPECT_EQ(0,ppu->GetWordView(0177400,false,false).value);
    const auto word=P(0177400); EXPECT_EQ(10,Signed7((word>>1)&127)); EXPECT_EQ(-20,Signed7((word>>9)&127));
    EXPECT_EQ(1,P(0177400));
}
TEST_F(MemoryEdges, MouseAccumulationDoesNotWrapBeforeClamping)
{
    board->SetMouse(true); board->MouseMove(256,-256,false,false); board->MouseMove(32767,-32768,false,false);
    const auto word=P(0177400); EXPECT_EQ(63,Signed7((word>>1)&127)); EXPECT_EQ(63,Signed7((word>>9)&127));
    for (int i=0;i<5;++i) P(0177400);
    board->MouseMove(-256,256,false,false); board->MouseMove(-32768,32767,false,false);
    const auto negative=P(0177400); EXPECT_EQ(-63,Signed7((negative>>1)&127)); EXPECT_EQ(-63,Signed7((negative>>9)&127));
}
TEST_F(MemoryEdges, TapeInputRequiresEnableAndSignalsOnlyTransitions)
{
    ppu->SetWord(0177716,false,0100000); EXPECT_FALSE(ppu->TapeInput(false)); EXPECT_EQ(0,P(0177716)&1);
    ppu->SetWord(0177716,false,0100004); EXPECT_TRUE(ppu->TapeInput(false)); EXPECT_EQ(1,P(0177716)&1);
    EXPECT_EQ(0310,board->GetPPU()->GetVIRQ(3)); board->GetPPU()->InterruptVIRQ(3,0);
    EXPECT_FALSE(ppu->TapeInput(false)); EXPECT_EQ(0,board->GetPPU()->GetVIRQ(3));
    EXPECT_TRUE(ppu->TapeInput(true)); EXPECT_EQ(0,P(0177716)&1); EXPECT_EQ(0310,board->GetPPU()->GetVIRQ(3));
}
TEST_F(MemoryEdges, TapeOutputAndSystemWritesPreserveInputBit)
{
    ppu->SetWord(0177716,false,0100004); ASSERT_TRUE(ppu->TapeInput(false));
    ppu->SetWord(0177716,false,0100002); EXPECT_TRUE(ppu->TapeOutput()); EXPECT_EQ(1,P(0177716)&1);
    ppu->SetWord(0177716,false,0100000); EXPECT_FALSE(ppu->TapeOutput()); EXPECT_EQ(1,P(0177716)&1);
}
TEST_F(MemoryEdges, BackgroundRegistersPackAllThreePlanesByPixel)
{
    board->SetRAMByte(0,0200,0x55); board->SetRAMByte(1,0200,0x33); board->SetRAMByte(2,0200,0x0f);
    ppu->SetWord(0177010,false,0200); P(0177024);
    EXPECT_EQ(0x4567,P(0177020)); EXPECT_EQ(0x0123,P(0177022));
}
