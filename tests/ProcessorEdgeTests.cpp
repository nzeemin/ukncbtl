/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include <gtest/gtest.h>
#include "emubase/Emubase.h"
#include <memory>
#include <optional>
#include <vector>

void DebugLog(LPCTSTR) {}
const TCHAR* REGISTER_NAME[] = {L"R0",L"R1",L"R2",L"R3",L"R4",L"R5",L"SP",L"PC"};
#ifdef _DEBUG
int AssertFailedLine(const char*, int) { std::abort(); }
#endif

namespace {
// Use the real memory-access path with controlled mapping and bus faults.
// HALT and USER have separate RAM planes so accidental mode changes are visible.
class Memory : public CMemoryController
{
public:
    uint16_t sel = 0x1200;
    std::optional<uint16_t> denied;
    int resets = 0, dclo = 0;
    void DCLO_Signal() override { ++dclo; }
    void ResetDevices() override { ++resets; }
    uint16_t GetSelRegister() const override { return sel; }
    uint16_t GetPortView(uint16_t) const override { return 0; }
    void SaveToImage(std::span<uint8_t,64>) override {}
    void LoadFromImage(std::span<const uint8_t,64>) override {}
protected:
    int TranslateAddress(uint16_t address, bool halt, bool, uint16_t* offset, bool) const override
    { *offset = address; return denied == address ? ADDRTYPE_DENY : halt ? ADDRTYPE_RAM0 : ADDRTYPE_RAM1; }
    uint16_t GetPortWord(uint16_t) override { return 0; }
    void SetPortWord(uint16_t,uint16_t) override {}
    uint8_t GetPortByte(uint16_t) override { return 0; }
    void SetPortByte(uint16_t,uint8_t) override {}
};
struct Event { const CProcessor* cpu; uint16_t address, code, pc, psw, r0; };
std::vector<Event> instructions, interrupts;
void CALLBACK Instruction(const CProcessor* cpu,uint16_t address,uint16_t opcode)
{ instructions.push_back({cpu,address,opcode,cpu->GetPC(),cpu->GetPSW(),cpu->GetReg(0)}); }
void CALLBACK Interrupt(const CProcessor* cpu,uint16_t address,uint16_t vector)
{ interrupts.push_back({cpu,address,vector,cpu->GetPC(),cpu->GetPSW(),cpu->GetReg(0)}); }
}
class ProcessorEdges : public testing::Test
{
protected:
    static constexpr uint16_t Program = 01000, Stack = 010000, Handler = 04000;
    std::unique_ptr<CMotherboard> board;
    std::unique_ptr<CProcessor> cpu;
    Memory memory;
    void Put(uint16_t address,uint16_t value,bool halt=false) { board->SetRAMWord(halt ? 0 : 1,address,value); }
    uint16_t Get(uint16_t address,bool halt=false) { return board->GetRAMWord(halt ? 0 : 1,address); }
    void Vector(uint16_t vector,bool halt=false,uint16_t psw=0)
    { Put(vector,Handler,halt); Put(vector+2,psw,halt); }
    void SetUp() override
    {
        CProcessor::Init(); board = std::make_unique<CMotherboard>(); cpu = std::make_unique<CProcessor>(L"TEST");
        memory.Attach(board.get(),cpu.get()); cpu->AttachMemoryController(&memory);
        Vector(memory.sel,true,0600);
        cpu->SetDCLOPin(false); cpu->SetACLOPin(false); ASSERT_TRUE(cpu->InterruptProcessing());
        cpu->SetPSW(0); cpu->SetPC(Program); cpu->SetSP(Stack); cpu->ClearInternalTick();
        instructions.clear(); interrupts.clear();
        cpu->SetInstructionCallback(Instruction); cpu->SetInterruptCallback(Interrupt);
    }
    void TearDown() override { cpu.reset(); board.reset(); CProcessor::Done(); }
    void Run(uint16_t opcode,bool halt=false)
    { Put(Program,opcode,halt); cpu->SetPC(Program); cpu->CommandExecution(); }
    void Halt() { cpu->SetPSW(0600); cpu->SetCPC(01234); cpu->SetCPSW(011); }
};

class PrivilegedOpcodes : public ProcessorEdges, public testing::WithParamInterface<int> {};
TEST_P(PrivilegedOpcodes, UserModeTrapsWithoutExecutingPrivilegedOperation)
{
    Vector(010); cpu->SetReg(0,012345); cpu->SetReg(5,02000);
    Run(static_cast<uint16_t>(GetParam()));
    EXPECT_EQ(012345,cpu->GetReg(0)); EXPECT_EQ(02000,cpu->GetReg(5));
    ASSERT_TRUE(cpu->InterruptProcessing());
    EXPECT_EQ(Handler,cpu->GetPC()); ASSERT_EQ(1,interrupts.size()); EXPECT_EQ(010,interrupts[0].code);
    EXPECT_EQ(Program+2,Get(Stack-4));
}
INSTANTIATE_TEST_SUITE_P(AllHaltAliases, PrivilegedOpcodes, testing::Range(010,040));
TEST_F(ProcessorEdges, ReadCopiesAndSelPreserveFlags)
{
    for (uint16_t opcode : {020,022,023,024,025,026,027})
    {
        Halt(); Run(opcode,true);
        EXPECT_EQ(opcode == 020 ? memory.sel : opcode <= 023 ? 01234 : 011,cpu->GetReg(0));
        EXPECT_EQ(0600,cpu->GetPSW()); EXPECT_EQ(01234,cpu->GetCPC()); EXPECT_EQ(011,cpu->GetCPSW());
    }
}
TEST_F(ProcessorEdges, WriteCopiesDoNotChangeLiveRegistersOrMaskSavedPsw)
{
    for (uint16_t opcode : {032,033,034,035,036,037})
    {
        Halt(); cpu->SetReg(0,0xffff); Run(opcode,true);
        EXPECT_EQ(opcode <= 033 ? 0xffff : 01234,cpu->GetCPC());
        EXPECT_EQ(opcode >= 034 ? 0xffff : 011,cpu->GetCPSW());
        EXPECT_EQ(0600,cpu->GetPSW()); EXPECT_EQ(Program+2,cpu->GetPC());
    }
}
TEST_F(ProcessorEdges, RunAliasesRestoreSavedPcAndMaskedPsw)
{
    for (uint16_t opcode : {010,011,012,013})
    {
        Halt(); cpu->SetCPSW(0xfe09); Run(opcode,true);
        EXPECT_EQ(01234,cpu->GetPC()); EXPECT_EQ(011,cpu->GetPSW());
    }
}
TEST_F(ProcessorEdges, StepAliasesDeferPendingInterruptForOneInstruction)
{
    Vector(0100);
    for (uint16_t opcode : {014,015,016,017})
    {
        Halt(); cpu->SetCPSW(0); Put(01234,0005200); cpu->SetReg(0,0); cpu->TickEVNT();
        Run(opcode,true); EXPECT_FALSE(cpu->InterruptProcessing());
        cpu->CommandExecution(); EXPECT_EQ(1,cpu->GetReg(0));
        ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(Handler,cpu->GetPC());
    }
}
TEST_F(ProcessorEdges, MfusReadsUserSpaceAndIncrementsR5)
{
    Halt(); Put(02000,0x1234); Put(02000,0xabcd,true); cpu->SetReg(5,02000);
    Run(021,true); EXPECT_EQ(0x1234,cpu->GetReg(0)); EXPECT_EQ(02002,cpu->GetReg(5)); EXPECT_TRUE(cpu->IsHaltMode());
}
TEST_F(ProcessorEdges, MtusWritesUserSpaceAndWrapsPredecrement)
{
    Halt(); cpu->SetReg(5,0); cpu->SetReg(0,0xabcd); Put(0xfffe,0x1234,true);
    Run(031,true); EXPECT_EQ(0xfffe,cpu->GetReg(5)); EXPECT_EQ(0xabcd,Get(0xfffe));
    EXPECT_EQ(0x1234,Get(0xfffe,true)); EXPECT_TRUE(cpu->IsHaltMode());
}
TEST_F(ProcessorEdges, MfusFaultPreservesR0AndRestoresHaltMode)
{
    Halt(); cpu->SetReg(0,0xbeef); cpu->SetReg(5,02000); memory.denied=02000;
    Run(021,true); EXPECT_EQ(0xbeef,cpu->GetReg(0)); EXPECT_EQ(02002,cpu->GetReg(5)); EXPECT_TRUE(cpu->IsHaltMode());
    memory.denied.reset(); Vector(memory.sel|4,true,0600); ASSERT_TRUE(cpu->InterruptProcessing());
    EXPECT_EQ(memory.sel|4,interrupts.back().code);
}
TEST_F(ProcessorEdges, NormalizeInstructionHandlesAlreadyNegativeAndZeroInputs)
{
    Halt(); cpu->SetReg(2,0200); Run(030,true);
    EXPECT_EQ(0,cpu->GetReg(0)); EXPECT_EQ(0604,cpu->GetPSW());
    Halt(); cpu->SetReg(1,0); cpu->SetReg(2,0); cpu->SetReg(3,0); Run(030,true);
    EXPECT_EQ(0200,cpu->GetReg(0)); EXPECT_EQ(0,cpu->GetReg(2)); EXPECT_EQ(0600,cpu->GetPSW());
}
TEST_F(ProcessorEdges, NormalizeInstructionShiftsAndSignExtends)
{
    Halt(); cpu->SetReg(1,1); cpu->SetReg(2,0100); cpu->SetReg(3,2); Run(030,true);
    EXPECT_EQ(1,cpu->GetReg(0)); EXPECT_EQ(2,cpu->GetReg(1)); EXPECT_EQ(0xff80,cpu->GetReg(2)); EXPECT_EQ(4,cpu->GetReg(3));
}
TEST_F(ProcessorEdges, HaltInstructionUsesSelVectorWithoutTouchingStack)
{
    Vector(memory.sel|0170,true,0600); cpu->SetPSW(011); Run(0);
    EXPECT_EQ(Handler,cpu->GetPC()); EXPECT_EQ(Stack,cpu->GetSP());
    EXPECT_EQ(Program+2,cpu->GetCPC()); EXPECT_EQ(011,cpu->GetCPSW());
    ASSERT_EQ(1,interrupts.size()); EXPECT_EQ(memory.sel|0170,interrupts[0].code);
}
TEST_F(ProcessorEdges, FisUsesHaltVectorOrReservedTrapDependingOnSel)
{
    Vector(memory.sel|010,true,0600); Run(075000);
    ASSERT_EQ(1,interrupts.size()); EXPECT_EQ(memory.sel|010,interrupts.back().code);
    memory.sel |= 0200; cpu->SetPSW(0); Vector(010); Run(075037);
    ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(010,interrupts.back().code);
}
TEST_F(ProcessorEdges, DcloStopsExecutionClearsRequestsAndResetsDevices)
{
    cpu->InterruptVIRQ(1,0100); cpu->TickEVNT(); cpu->MemoryError(); cpu->SetDCLOPin(true);
    EXPECT_TRUE(cpu->IsStopped()); EXPECT_EQ(1,memory.dclo); EXPECT_EQ(1,memory.resets);
    EXPECT_EQ(0,cpu->GetVIRQ(1)); EXPECT_EQ(0,cpu->GetInternalTick());
    Put(Program,0005200); cpu->Execute(); EXPECT_EQ(Program,cpu->GetPC()); EXPECT_TRUE(instructions.empty());
    cpu->InterruptVIRQ(1,0100); cpu->TickEVNT(); EXPECT_EQ(0,cpu->GetVIRQ(1));
}
TEST_F(ProcessorEdges, StartupRequiresAcloFallingEdgeWithDcloReleased)
{
    cpu->SetDCLOPin(true); cpu->SetACLOPin(true); cpu->SetACLOPin(false);
    EXPECT_TRUE(cpu->IsStopped()); cpu->SetDCLOPin(false); cpu->SetACLOPin(false); EXPECT_TRUE(cpu->IsStopped());
    cpu->SetACLOPin(true); cpu->SetACLOPin(false); EXPECT_FALSE(cpu->IsStopped());
    ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(memory.sel,interrupts.back().code);
}
TEST_F(ProcessorEdges, AcloRisingEdgeIsLatchedAndNotRepeatedBySteadyHigh)
{
    Vector(024); cpu->SetACLOPin(true); ASSERT_TRUE(cpu->InterruptProcessing());
    EXPECT_EQ(024,interrupts.back().code); cpu->SetACLOPin(true); EXPECT_FALSE(cpu->InterruptProcessing());
    cpu->SetACLOPin(false); cpu->SetACLOPin(true); EXPECT_TRUE(cpu->InterruptProcessing());
}
TEST_F(ProcessorEdges, HaltPinIsLevelSensitiveOnlyOutsideHaltMode)
{
    Vector(memory.sel|0170,true,0600); cpu->SetHALTPin(true); ASSERT_TRUE(cpu->InterruptProcessing());
    EXPECT_FALSE(cpu->InterruptProcessing()); cpu->SetPSW(0); EXPECT_TRUE(cpu->InterruptProcessing());
    cpu->SetHALTPin(false); cpu->SetPSW(0); EXPECT_FALSE(cpu->InterruptProcessing());
}
TEST_F(ProcessorEdges, InterruptPriorityTraceAcloHaltEvntVirq)
{
    for (uint16_t vector : {014,024,0100,0120,0124}) Vector(vector);
    Vector(memory.sel|0170,true,0600);
    cpu->InterruptVIRQ(3,0124); cpu->InterruptVIRQ(1,0120); cpu->TickEVNT(); cpu->SetHALTPin(true); cpu->SetACLOPin(true); cpu->SetPSW(020);
    ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(014,interrupts.back().code);
    ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(024,interrupts.back().code);
    ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(memory.sel|0170,interrupts.back().code);
    cpu->SetHALTPin(false); cpu->SetPSW(0);
    ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(0100,interrupts.back().code);
    ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(0120,interrupts.back().code);
    EXPECT_EQ(0,cpu->GetVIRQ(1)); EXPECT_EQ(0124,cpu->GetVIRQ(3));
    ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(0124,interrupts.back().code); EXPECT_FALSE(cpu->InterruptProcessing());
}
TEST_F(ProcessorEdges, MaskedEventAndVirqRemainPendingUntilUnmasked)
{
    Vector(0100); Vector(0120); cpu->SetPSW(0200); cpu->TickEVNT(); cpu->InterruptVIRQ(15,0120);
    EXPECT_FALSE(cpu->InterruptProcessing()); EXPECT_EQ(0120,cpu->GetVIRQ(15));
    cpu->SetPSW(0); ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(0100,interrupts.back().code);
    ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(0120,interrupts.back().code);
}
TEST_F(ProcessorEdges, AcloIsMaskedOnlyByBothHighStatusBits)
{
    Vector(024); Halt(); cpu->SetACLOPin(true); EXPECT_FALSE(cpu->InterruptProcessing());
    cpu->SetPSW(0200); ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(024,interrupts.back().code);
}
TEST_F(ProcessorEdges, FetchFaultSkipsInstructionCallbackAndTakesBusVector)
{
    memory.denied=Program; cpu->SetReg(0,7); cpu->CommandExecution();
    EXPECT_TRUE(instructions.empty()); EXPECT_EQ(7,cpu->GetReg(0)); EXPECT_EQ(Program+2,cpu->GetPC());
    Vector(4); ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(4,interrupts.back().code);
}
TEST_F(ProcessorEdges, SourceFaultDoesNotWriteDestination)
{
    memory.denied=02000; cpu->SetReg(0,0xabcd); cpu->SetReg(1,02000); Run(0012100);
    EXPECT_EQ(0xabcd,cpu->GetReg(0)); EXPECT_EQ(02002,cpu->GetReg(1));
    Vector(4); ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(4,interrupts.back().code);
}
TEST_F(ProcessorEdges, FailedInterruptStackWriteSuppressesCallbackAndKeepsVirq)
{
    Vector(0100); cpu->InterruptVIRQ(1,0100); memory.denied=Stack-2;
    ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_TRUE(interrupts.empty()); EXPECT_EQ(0100,cpu->GetVIRQ(1));
    EXPECT_EQ(Program,cpu->GetPC()); EXPECT_EQ(Stack-4,cpu->GetSP());
}
TEST_F(ProcessorEdges, FailedVectorReadSuppressesCallback)
{
    cpu->InterruptVIRQ(1,0100); memory.denied=0100;
    ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_TRUE(interrupts.empty()); EXPECT_EQ(Program,cpu->GetPC());
}
TEST_F(ProcessorEdges, ConsecutiveBusFaultsUseDoubleFaultHaltVector)
{
    Vector(4); Vector(memory.sel|0174,true,0600);
    cpu->MemoryError(); ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(4,interrupts.back().code);
    cpu->MemoryError(); ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(memory.sel|0174,interrupts.back().code);
}
TEST_F(ProcessorEdges, InstructionCallbackSeesFetchedOpcodeBeforeItsEffects)
{
    cpu->SetReg(0,7); Run(0005200);
    ASSERT_EQ(1,instructions.size()); EXPECT_EQ(cpu.get(),instructions[0].cpu); EXPECT_EQ(Program,instructions[0].address);
    EXPECT_EQ(0005200,instructions[0].code); EXPECT_EQ(Program+2,instructions[0].pc); EXPECT_EQ(7,instructions[0].r0);
    EXPECT_EQ(8,cpu->GetReg(0)); cpu->SetInstructionCallback(nullptr); Run(0005200); EXPECT_EQ(1,instructions.size());
}
TEST_F(ProcessorEdges, InterruptCallbackSeesLoadedStateAndStack)
{
    Vector(0100,false,011); cpu->SetPSW(1); cpu->TickEVNT(); ASSERT_TRUE(cpu->InterruptProcessing());
    ASSERT_EQ(1,interrupts.size()); EXPECT_EQ(cpu.get(),interrupts[0].cpu); EXPECT_EQ(Program,interrupts[0].address);
    EXPECT_EQ(Handler,interrupts[0].pc); EXPECT_EQ(011,interrupts[0].psw);
    EXPECT_EQ(Program,Get(Stack-4)); EXPECT_EQ(1,Get(Stack-2));
    cpu->SetInterruptCallback(nullptr); cpu->TickEVNT(); ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(1,interrupts.size());
}
TEST_F(ProcessorEdges, ExecuteConsumesTimingBeforeNextInstruction)
{
    Run(0005200); const int ticks = cpu->GetInternalTick(); ASSERT_GT(ticks,0);
    Put(Program+2,0005200); for (int i=0;i<ticks;++i) cpu->Execute();
    EXPECT_EQ(1,cpu->GetReg(0)); EXPECT_EQ(1,instructions.size());
    cpu->Execute(); EXPECT_EQ(2,cpu->GetReg(0)); EXPECT_EQ(2,instructions.size());
}
TEST_F(ProcessorEdges, WaitDoesNotInvokeInstructionCallbackUntilInterrupted)
{
    Run(1); for (int i=0;i<10;++i) cpu->CommandExecution(); EXPECT_EQ(1,instructions.size());
    Vector(0100); Put(Handler,0005200); cpu->TickEVNT(); ASSERT_TRUE(cpu->InterruptProcessing());
    cpu->CommandExecution(); EXPECT_EQ(2,instructions.size()); EXPECT_EQ(1,cpu->GetReg(0));
}

class MoveAddressing : public ProcessorEdges, public testing::WithParamInterface<bool>
{
protected:
    uint16_t Operand(int mode,int reg,uint16_t address,uint16_t pointer,int step,uint16_t& extension)
    {
        Put(pointer,address);
        switch (mode)
        {
        case 1: case 2: cpu->SetReg(reg,address); break;
        case 3: cpu->SetReg(reg,pointer); break;
        case 4: cpu->SetReg(reg,address+step); break;
        case 5: cpu->SetReg(reg,pointer+2); break;
        case 6: cpu->SetReg(reg,address-020); Put(extension,020); extension+=2; break;
        case 7: cpu->SetReg(reg,pointer-020); Put(extension,020); extension+=2; break;
        }
        return static_cast<uint16_t>(mode*8+reg);
    }
};
TEST_P(MoveAddressing, AllSourceDestinationModesAndIncomingConditionCodes)
{
    const bool byte = GetParam();
    for (int source=0;source<8;++source)
        for (int destination=0;destination<8;++destination)
            for (unsigned value : {0u,1u,0x80u,0xffu,0x8000u,0xffffu})
                for (uint16_t flags=0;flags<16;++flags)
                {
                    SCOPED_TRACE(testing::Message() << "byte=" << byte << " source=" << source << " dest=" << destination << " value=" << value << " flags=" << flags);
                    cpu->SetPSW(flags); cpu->SetReg(1,static_cast<uint16_t>(value)); cpu->SetReg(2,0xabcd);
                    Put(02000,static_cast<uint16_t>(value)); Put(03000,0xabcd);
                    uint16_t extension = Program+2;
                    const auto src = Operand(source,1,02000,02400,byte?1:2,extension);
                    const auto dst = Operand(destination,2,03000,03400,byte?1:2,extension);
                    const auto before1 = cpu->GetReg(1), before2 = cpu->GetReg(2);
                    Run(static_cast<uint16_t>((byte ? 0110000 : 0010000) | (src << 6) | dst));
                    const unsigned mask = byte ? 255 : 65535;
                    const unsigned result = value & mask;
                    const unsigned expected = byte && destination == 0 ? (result >= 128 ? result | 0xff00 : result) :
                        byte ? 0xab00 | result : result;
                    EXPECT_EQ(expected,destination == 0 ? cpu->GetReg(2) : Get(03000));
                    EXPECT_EQ((flags & 1) | (result == 0 ? 4 : 0) | (result & (byte ? 128 : 32768) ? 8 : 0),cpu->GetPSW());
                    EXPECT_EQ(extension,cpu->GetPC());
                    const auto after = [byte](int mode,uint16_t before) {
                        return static_cast<uint16_t>(before + (mode==2 ? (byte?1:2) : mode==3 ? 2 : mode==4 ? -(byte?1:2) : mode==5 ? -2 : 0));
                    };
                    EXPECT_EQ(after(source,before1),cpu->GetReg(1));
                    if (destination != 0) EXPECT_EQ(after(destination,before2),cpu->GetReg(2));
                }
}
INSTANTIATE_TEST_SUITE_P(WordAndByte, MoveAddressing, testing::Bool());

TEST_F(ProcessorEdges, RegisterByteSettersPreserveHighBytesAndSavedPcRules)
{
    for (uint16_t psw : {0,0400,0600})
        for (int reg=0;reg<8;++reg)
        {
            cpu->SetPSW(psw); cpu->SetReg(reg,0xabcd); cpu->SetCPC(01234); cpu->SetLReg(reg,0x56);
            EXPECT_EQ(0xab56,cpu->GetReg(reg));
            EXPECT_EQ(reg==7 && psw!=0600 ? 0xab56 : 01234,cpu->GetCPC());
        }
}
TEST_F(ProcessorEdges, PswMasksUnsupportedBitsAndFreezesCopyInHaltPriorityMode)
{
    for (unsigned value=0;value<=65535;++value)
    {
        cpu->SetCPSW(012345); cpu->SetPSW(static_cast<uint16_t>(value));
        EXPECT_EQ(value & 0777,cpu->GetPSW());
        EXPECT_EQ((value & 0600)==0600 ? 012345 : value & 0777,cpu->GetCPSW());
    }
}
TEST_F(ProcessorEdges, ConditionSettersPreserveOtherBitsAndSavedState)
{
    for (uint16_t mode : {0,0400,0600})
        for (uint16_t flags=0;flags<16;++flags)
        {
            cpu->SetPSW(mode|flags); cpu->SetCPSW(012345);
            cpu->SetC(!(flags&1)); cpu->SetV(!(flags&2)); cpu->SetZ(!(flags&4)); cpu->SetN(!(flags&8));
            EXPECT_EQ(mode|(flags^15),cpu->GetPSW());
            EXPECT_EQ(mode==0600 ? 012345 : mode|(flags^15),cpu->GetCPSW());
        }
}
TEST_F(ProcessorEdges, DestinationFaultLeavesMemoryUnchangedAndRequestsBusTrap)
{
    cpu->SetReg(0,0xabcd); cpu->SetReg(1,02000); Put(02000,0x1234); memory.denied=02000;
    Run(0010011); EXPECT_EQ(0x1234,Get(02000));
    Vector(4); ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(4,interrupts.back().code);
}
TEST_F(ProcessorEdges, ExecutedInstructionClearsDoubleBusFaultHistory)
{
    Vector(4); cpu->MemoryError(); ASSERT_TRUE(cpu->InterruptProcessing());
    Run(0000240); cpu->MemoryError(); ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(4,interrupts.back().code);
}
TEST_F(ProcessorEdges, BusFaultPrecedesTraceAndExternalInterrupts)
{
    Vector(4); cpu->SetPSW(020); cpu->TickEVNT(); cpu->InterruptVIRQ(1,0100); cpu->MemoryError();
    ASSERT_TRUE(cpu->InterruptProcessing()); EXPECT_EQ(4,interrupts.back().code); EXPECT_EQ(0100,cpu->GetVIRQ(1));
}
TEST_F(ProcessorEdges, MotherboardInstallsAndRemovesCallbacksForBothProcessors)
{
    board->Reset(); board->SetInstructionCallback(Instruction);
    for (auto* processor : {board->GetCPU(),board->GetPPU()})
    {
        processor->SetPSW(0); processor->SetPC(Program);
        processor->GetMemoryController()->SetWord(Program,false,0005200);
        processor->CommandExecution();
        ASSERT_FALSE(instructions.empty()); EXPECT_EQ(processor,instructions.back().cpu);
        EXPECT_EQ(Program,instructions.back().address); EXPECT_EQ(0005200,instructions.back().code);
    }
    ASSERT_EQ(2,instructions.size()); board->SetInstructionCallback(nullptr);
    for (auto* processor : {board->GetCPU(),board->GetPPU()}) { processor->SetPC(Program); processor->CommandExecution(); }
    EXPECT_EQ(2,instructions.size());
}
