/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include <gtest/gtest.h>
#include "emubase/Emubase.h"
#include <array>
#include <format>
#include <memory>
#include <vector>
void DebugLog(LPCTSTR) {}
const TCHAR* REGISTER_NAME[] = {L"R0",L"R1",L"R2",L"R3",L"R4",L"R5",L"SP",L"PC"};
#ifdef _DEBUG
int AssertFailedLine(const char*,int) { std::abort(); }
#endif
namespace {
void Same(const DisassembledInstruction& a,const DisassembledInstruction& b) {
    EXPECT_EQ(a.mnemonic,b.mnemonic); EXPECT_EQ(a.argument,b.argument); EXPECT_EQ(a.length,b.length);
}
void Same(const DisasmHint& a,const DisasmHint& b) {
    EXPECT_EQ(a.lines,b.lines); EXPECT_EQ(a.count,b.count); EXPECT_EQ(a.isJump,b.isJump); EXPECT_EQ(a.jumpPredict,b.jumpPredict);
}
constexpr std::array<uint16_t,15> branches{0000400,0001000,0001400,0002000,0002400,0003000,0003400,0100000,0100400,0101000,0101400,0102000,0102400,0103000,0103400};
constexpr std::array<const wchar_t*,15> names{L"BR",L"BNE",L"BEQ",L"BGE",L"BLT",L"BGT",L"BLE",L"BPL",L"BMI",L"BHI",L"BLOS",L"BVC",L"BVS",L"BHIS",L"BLO"};
}
class DisasmEdges : public testing::Test {
protected:
    std::unique_ptr<CMotherboard> board;
    void SetUp() override { CProcessor::Init(); board=std::make_unique<CMotherboard>(); board->Reset(); board->GetCPU()->SetPC(0177776); }
    void TearDown() override { board.reset(); CProcessor::Done(); }
};
TEST_F(DisasmEdges, EmptyInputHasNoInstructionArrowOrHint) {
    EXPECT_FALSE(DisassembleInstruction({},0)); EXPECT_FALSE(Disasm_CheckForJump({},0));
    const auto& cpu=*board->GetCPU(); const auto& mem=*board->GetCPUMemoryController();
    Same({},Disasm_GetHint({},cpu,mem)); Same({},Disasm_GetInstructionHint({},cpu,mem));
    const auto jump=Disasm_GetJumpConditionHint({},cpu,mem); EXPECT_FALSE(jump.predict); EXPECT_TRUE(jump.text.empty());
}
class ShortInputs : public DisasmEdges, public testing::WithParamInterface<int> {};
TEST_P(ShortInputs, EveryOpcodeUsesZeroForMissingWordsAndIgnoresTrailingWords) {
    const auto& cpu=*board->GetCPU(); const auto& mem=*board->GetCPUMemoryController();
    for(unsigned opcode=0;opcode<65536;++opcode) {
        SCOPED_TRACE(opcode);
        const std::array<uint16_t,5> storage{static_cast<uint16_t>(opcode),012345,065432,0177777,0177777};
        const auto input=std::span(storage).first(GetParam());
        std::array<uint16_t,3> padded{storage[0],GetParam()>1?storage[1]:uint16_t{0},GetParam()>2?storage[2]:uint16_t{0}};
        const auto decoded=DisassembleInstruction(input,0177776), reference=DisassembleInstruction(padded,0177776);
        ASSERT_TRUE(decoded); ASSERT_TRUE(reference); Same(*decoded,*reference); EXPECT_GE(decoded->length,1); EXPECT_LE(decoded->length,3);
        Same(Disasm_GetHint(input,cpu,mem),Disasm_GetHint(padded,cpu,mem));
        Same(Disasm_GetInstructionHint(input,cpu,mem),Disasm_GetInstructionHint(padded,cpu,mem));
        auto a=Disasm_GetJumpConditionHint(input,cpu,mem), b=Disasm_GetJumpConditionHint(padded,cpu,mem);
        EXPECT_EQ(a.text,b.text); EXPECT_EQ(a.predict,b.predict);
    }
}
INSTANTIATE_TEST_SUITE_P(SpanLengths,ShortInputs,testing::Values(1,2,5));
class BranchSemantics : public DisasmEdges,public testing::WithParamInterface<int> {};
TEST_P(BranchSemantics, AllOffsetsWrapAndAllConditionCodesPredictCorrectly) {
    const int index=GetParam();
    for(uint16_t address : {uint16_t{0},uint16_t{2},uint16_t{01000},uint16_t{0177776}}) for(int offset=-128;offset<128;++offset) {
        const std::array<uint16_t,1> words{static_cast<uint16_t>(branches[index]|(offset&255))};
        const auto result=DisassembleInstruction(words,address); ASSERT_TRUE(result);
        EXPECT_EQ(names[index],result->mnemonic); EXPECT_EQ(1,result->length);
        EXPECT_EQ(std::format(L"{:06o}",static_cast<uint16_t>(address+2+2*offset)),result->argument);
        EXPECT_EQ(offset+1,Disasm_CheckForJump(words,address));
    }
    for(int flags=0;flags<16;++flags) {
        const bool n=flags&8,z=flags&4,v=flags&2,c=flags&1;
        const std::array<bool,15> truth{true,!z,z,n==v,n!=v,!z&&n==v,z||n!=v,!n,n,!c&&!z,c||z,!v,v,!c,c};
        board->GetCPU()->SetPSW(flags); const std::array<uint16_t,1> words{branches[index]};
        const auto hint=Disasm_GetHint(words,*board->GetCPU(),*board->GetCPUMemoryController());
        EXPECT_TRUE(hint.isJump); EXPECT_EQ(truth[index],hint.jumpPredict);
        EXPECT_EQ(truth[index],Disasm_GetJumpConditionHint(words,*board->GetCPU(),*board->GetCPUMemoryController()).predict);
    }
}
INSTANTIATE_TEST_SUITE_P(Conditions,BranchSemantics,testing::Range(0,15));
class OperandPairs : public testing::TestWithParam<int> {};
TEST_P(OperandPairs, AllPcModesConsumeExtensionsInOrderAcrossAddressWrap) {
    constexpr std::array<uint16_t,12> opcodes{0010000,0110000,0020000,0120000,0030000,0130000,0040000,0140000,0050000,0150000,0060000,0160000};
    constexpr std::array<const wchar_t*,12> mnemonics{L"MOV",L"MOVB",L"CMP",L"CMPB",L"BIT",L"BITB",L"BIC",L"BICB",L"BIS",L"BISB",L"ADD",L"SUB"};
    // At FFFE, the first extension is at 0000; a second extension is at 0002.
    constexpr std::array<const wchar_t*,8> source{L"PC",L"(PC)",L"#000004",L"@#000004",L"-(PC)",L"@-(PC)",L"000006",L"@000006"};
    constexpr std::array<const wchar_t*,8> second{L"PC",L"(PC)",L"#177770",L"@#177770",L"-(PC)",L"@-(PC)",L"177774",L"@177774"};
    constexpr std::array<int,8> extensions{0,0,1,1,0,0,1,1};
    for(int src=0;src<8;++src) for(int dst=0;dst<8;++dst) {
        const std::array<uint16_t,3> words{static_cast<uint16_t>(opcodes[GetParam()]|((src*8+7)<<6)|(dst*8+7)),4,0177770};
        const auto result=DisassembleInstruction(words,0177776); ASSERT_TRUE(result);
        EXPECT_EQ(mnemonics[GetParam()],result->mnemonic); EXPECT_EQ(1+extensions[src]+extensions[dst],result->length);
        EXPECT_EQ(std::format(L"{}, {}",source[src],extensions[src]?second[dst]:source[dst]),result->argument);
    }
}
INSTANTIATE_TEST_SUITE_P(Instructions,OperandPairs,testing::Range(0,12));
TEST(DisasmSpanEdges, MissingJumpExtensionHasNoArrowAndOddTargetsAreRejected) {
    for(uint16_t opcode : {uint16_t{0000167},uint16_t{0000137},uint16_t{0004767},uint16_t{0004737}}) {
        const std::array<uint16_t,2> words{opcode,1};
        EXPECT_FALSE(Disasm_CheckForJump(std::span(words).first(1),0)); EXPECT_FALSE(Disasm_CheckForJump(words,0));
    }
}
TEST(DisasmSpanEdges, SobEveryRegisterAndOffsetWraps) {
    for(int reg=0;reg<8;++reg) for(int offset=0;offset<64;++offset) {
        const std::array<uint16_t,1> words{static_cast<uint16_t>(0077000|(reg<<6)|offset)};
        const auto result=DisassembleInstruction(words,0); ASSERT_TRUE(result); EXPECT_EQ(L"SOB",result->mnemonic);
        EXPECT_EQ(std::format(L"{}, {:06o}",REGISTER_NAME[reg],static_cast<uint16_t>(2-offset*2)),result->argument);
        EXPECT_EQ(1-offset,Disasm_CheckForJump(words,0));
    }
}
TEST_F(DisasmEdges, HintQueriesPreserveMachineStateForEveryOpcode) {
    std::vector<uint8_t> before(UKNCIMAGE_SIZE),after(UKNCIMAGE_SIZE);
    board->SaveToImage(std::span<uint8_t,UKNCIMAGE_SIZE>(before.data(),before.size()));
    for(unsigned opcode=0;opcode<65536;++opcode) {
        const std::array<uint16_t,3> words{static_cast<uint16_t>(opcode),0177702,0177130};
        const auto hint=Disasm_GetHint(words,*board->GetCPU(),*board->GetCPUMemoryController());
        ASSERT_GE(hint.count,0); ASSERT_LE(hint.count,DISASM_HINT_LINES);
    }
    board->SaveToImage(std::span<uint8_t,UKNCIMAGE_SIZE>(after.data(),after.size())); EXPECT_EQ(before,after);
}
