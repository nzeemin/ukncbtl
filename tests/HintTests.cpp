/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// HintTests.cpp : tests for the instruction hints of the disassembler view
// (Disasm_GetInstructionHint and Disasm_GetJumpConditionHint), see docs/hints-improvements.md
//
// Expected values come from what the processor does with the operands (GetWordAddr, GetByteAddr,
// ExecuteMARK, InterruptProcessing), not from the output of the hint functions. A case that the
// current code gets wrong is an ignored *_KnownBug test with the correct expected text.
// The instruction is placed at PROG, the program counter is PROG, the memory is RAM.

#include "stdafx.h"
#include <CppUnitTest.h>
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace
{

const uint16_t PROG = 01000;
const uint16_t STACK = 0400;

struct MemWord
{
    uint16_t addr;      // 0 ends the list
    uint16_t value;
};

#define NO_MEM { { 0, 0 } }

const int MAX_MEM = 4;

struct HintCase
{
    const TCHAR* name;
    uint16_t words[3];  // the instruction and the two words after it
    uint16_t regs[7];   // R0..R5, SP
    MemWord mem[MAX_MEM];
    uint16_t psw;       // PSW before the instruction, 0400 = HALT mode, otherwise the flags TF_*
    uint16_t cpc;       // CPC and CPSW, for the HALT mode commands
    uint16_t cpsw;
    int lines;          // the expected number of hint lines
    const TCHAR* line1;
    const TCHAR* line2;
};

struct JumpHintCase
{
    const TCHAR* name;
    uint16_t words[2];
    uint16_t regs[7];
    MemWord mem[MAX_MEM];
    uint16_t psw;
    const TCHAR* text;  // the expected hint, empty = no hint
    bool predict;       // the expected jump prediction
};

// Registers, memory, then the program counter and the PSW, the way the processor would have them
void Prepare(TestBoard& tb, const uint16_t* words, size_t wordCount, const uint16_t* regs, const MemWord* mem,
             uint16_t psw, uint16_t cpc, uint16_t cpsw)
{
    uint16_t allRegs[8] = { regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], 0 };
    tb.SetRegs(allRegs);
    tb.LoadProgram(PROG, words, wordCount);
    for (int i = 0; i < MAX_MEM && mem[i].addr != 0; i++)
        tb.SetWord(mem[i].addr, mem[i].value);
    tb.CPU()->SetPSW(psw);
    tb.CPU()->SetCPC(cpc);
    tb.CPU()->SetCPSW(cpsw);
}

void CheckHint(const HintCase& c)
{
    TestBoard tb;
    Prepare(tb, c.words, 3, c.regs, c.mem, c.psw, c.cpc, c.cpsw);

    TCHAR line1[64] = _T("not set");
    TCHAR line2[64] = _T("not set");
    int lines = Disasm_GetInstructionHint(c.words, tb.CPU(), tb.CPU()->GetMemoryControllerConst(), line1, line2);

    TCHAR msg[100];
    _stprintf_s(msg, _T("%s, opcode %06o: lines"), c.name, (unsigned int) c.words[0]);
    Assert::AreEqual(c.lines, lines, msg);
    _stprintf_s(msg, _T("%s, opcode %06o: line 1"), c.name, (unsigned int) c.words[0]);
    Assert::AreEqual(c.line1, line1, msg);
    _stprintf_s(msg, _T("%s, opcode %06o: line 2"), c.name, (unsigned int) c.words[0]);
    Assert::AreEqual(c.line2, line2, msg);
}

void CheckHints(const HintCase* cases, size_t count)
{
    for (size_t i = 0; i < count; i++)
        CheckHint(cases[i]);
}

void CheckJumpHint(const JumpHintCase& c)
{
    TestBoard tb;
    Prepare(tb, c.words, 2, c.regs, c.mem, c.psw, 0, 0);

    TCHAR text[64] = _T("not set");
    bool predict = Disasm_GetJumpConditionHint(c.words, tb.CPU(), tb.CPU()->GetMemoryControllerConst(), text);

    TCHAR msg[100];
    _stprintf_s(msg, _T("%s, opcode %06o: text"), c.name, (unsigned int) c.words[0]);
    Assert::AreEqual(c.text, text, msg);
    _stprintf_s(msg, _T("%s, opcode %06o: prediction"), c.name, (unsigned int) c.words[0]);
    Assert::AreEqual(c.predict, predict, msg);
}

void CheckJumpHints(const JumpHintCase* cases, size_t count)
{
    for (size_t i = 0; i < count; i++)
        CheckJumpHint(cases[i]);
}

}

TEST_CLASS(HintTests)
{
public:
    // Registers and the memory modes of the operands

    TEST_METHOD(Hint_NoHint)
    {
        static const HintCase cases[] =
        {
            { _T("NOP"),   { 0000240 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0, 0, _T(""), _T("") },
            { _T("WAIT"),  { 0000001 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0, 0, _T(""), _T("") },
            { _T("RESET"), { 0000005 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0, 0, _T(""), _T("") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_RegisterAndMemoryModes)
    {
        // The word at 0200 is 0123456, so its low byte is 056 and its high byte is 0247
        static const HintCase cases[] =
        {
            { _T("MOV R1,R2"),    { 0010102 }, { 0, 0123, 045, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0,
                1, _T("R1=000123, R2=000045"), _T("") },
            { _T("MOV R1,R1"),    { 0010101 }, { 0, 0123, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0,
                1, _T("R1=000123"), _T("") },  // equal hints are shown once
            { _T("CLR (R1)"),     { 0005011 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("(R1)=123456") },
            { _T("CLR (R1)+"),    { 0005021 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("(R1)=123456") },
            { _T("CLR -(R1)"),    { 0005041 }, { 0, 0202, 0, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                2, _T("R1=000202"), _T("(R1-2)=123456") },
            { _T("CLR @(R1)+"),   { 0005031 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0300 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("(R1)=000300") },  // the word at the pointer: Part 2 adds a third line
            { _T("CLR @-(R1)"),   { 0005051 }, { 0, 0202, 0, 0, 0, 0, STACK }, { { 0200, 0300 } }, 0, 0, 0,
                2, _T("R1=000202"), _T("(R1-2)=000300") },
            { _T("CLR 4(R1)"),    { 0005061, 4 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0204, 0123456 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("(R1+000004)=123456") },
            { _T("CLR @4(R1)"),   { 0005071, 4 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0204, 0300 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("(R1+000004)=000300") },
            { _T("CLR 177774(R1)"), { 0005061, 0177774 }, { 0, 0210, 0, 0, 0, 0, STACK }, { { 0204, 0123456 } }, 0, 0, 0,
                2, _T("R1=000210"), _T("(R1+177774)=123456") },  // the index is negative: 0210 - 4 = 0204
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_ByteOperations)
    {
        static const HintCase cases[] =
        {
            { _T("CLRB (R1)"),     { 0105011 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("(R1)=056") },   // even address: the low byte
            { _T("CLRB (R1) odd"), { 0105011 }, { 0, 0201, 0, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                2, _T("R1=000201"), _T("(R1)=247") },   // odd address: the high byte
            { _T("CLRB -(R1)"),    { 0105041 }, { 0, 0201, 0, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                2, _T("R1=000201"), _T("(R1-1)=056") },   // R1 is decremented by 1: the byte at 0200
            { _T("CLRB -(R1) even"), { 0105041 }, { 0, 0202, 0, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                2, _T("R1=000202"), _T("(R1-1)=247") },   // the byte at 0201: the high byte of 0200
            { _T("CLRB 1(R1)"),    { 0105061, 1 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("(R1+000001)=247") },
            { _T("CLRB @(R1)+"),   { 0105031 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0300 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("(R1)=000300") },  // the pointer is a word even for a byte operation
            { _T("MOVB (R1)+,R2"), { 0112102 }, { 0, 0200, 5, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                2, _T("R1=000200, R2=000005"), _T("(R1)=056") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_TwoOperands)
    {
        static const HintCase cases[] =
        {
            // Both operands have an index word: the second word belongs to the source, the third to the destination
            { _T("MOV 4(R1),6(R2)"), { 0016162, 4, 6 }, { 0, 0200, 0300, 0, 0, 0, STACK },
                { { 0204, 0111 }, { 0306, 0222 } }, 0, 0, 0,
                2, _T("R1=000200, R2=000300"), _T("(R1+000004)=000111, (R2+000006)=000222") },
            { _T("ADD (R1),R2"),     { 0061102 }, { 0, 0200, 7, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                2, _T("R1=000200, R2=000007"), _T("(R1)=123456") },
            { _T("MOV R1,(R2)"),     { 0010112 }, { 0, 0123, 0200, 0, 0, 0, STACK }, { { 0200, 0111 } }, 0, 0, 0,
                2, _T("R1=000123, R2=000200"), _T("           (R2)=000111") },  // line 2 is aligned under the destination hint
            { _T("MOV (R1),R2"),     { 0011102 }, { 0, 0200, 7, 0, 0, 0, STACK }, { { 0200, 0111 } }, 0, 0, 0,
                2, _T("R1=000200, R2=000007"), _T("(R1)=000111") },
            { _T("MOV (R1),(R2)"),   { 0011112 }, { 0, 0200, 0300, 0, 0, 0, STACK }, { { 0200, 0111 }, { 0300, 0222 } }, 0, 0, 0,
                2, _T("R1=000200, R2=000300"), _T("(R1)=000111, (R2)=000222") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_PcModes)
    {
        // PC-relative: the address is computed from the PC after the index word, 01004 for the source
        static const HintCase cases[] =
        {
            { _T("MOV #1234,R1"),     { 0012701, 01234 }, { 0, 5, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0,
                1, _T("R1=000005"), _T("") },   // the immediate value is in the instruction text, no hint
            { _T("MOV @#200,R1"),     { 0013701, 0200 }, { 0, 0, 0, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                1, _T("(000200)=123456, R1=000000"), _T("") },
            { _T("MOV 177174(PC),R1"), { 0016701, 0177174 }, { 0, 0, 0, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                1, _T("(000200)=123456, R1=000000"), _T("") },  // 01004 + 0177174 = 0200 (16 bits)
            { _T("CLR @#200"),        { 0005037, 0200 }, { 0, 0, 0, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                1, _T("(000200)=123456"), _T("") },
            { _T("CLRB @#201"),       { 0105037, 0201 }, { 0, 0, 0, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                1, _T("(000201)=247"), _T("") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_RegisterAndOperand)
    {
        // MUL, DIV, ASH, ASHC, XOR: the register in bits 8..6 and the operand
        static const HintCase cases[] =
        {
            { _T("MUL (R2),R1"),  { 0070112 }, { 0, 3, 0200, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                2, _T("R1=000003, R2=000200"), _T("           (R2)=123456") },  // aligned under R2
            { _T("XOR R1,(R2)"),  { 0074112 }, { 0, 7, 0200, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                2, _T("R1=000007, R2=000200"), _T("           (R2)=123456") },  // aligned under R2
            { _T("ASH R2,R1"),    { 0072102 }, { 0, 3, 2, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0,
                1, _T("R1=000003, R2=000002"), _T("") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_SingleOperandInstructions)
    {
        static const HintCase cases[] =
        {
            { _T("SWAB R1"),  { 0000301 }, { 0, 0123, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0, 1, _T("R1=000123"), _T("") },
            { _T("TST (R1)"), { 0005711 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0111 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("(R1)=000111") },
            { _T("ASL (R1)"), { 0006311 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0111 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("(R1)=000111") },
            { _T("JMP (R1)"), { 0000111 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0111 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("(R1)=000111") },
            // JMP with the relative and the absolute addressing is shown by the jump arrow, not by a hint
            { _T("JMP 10(PC)"), { 0000167, 010 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0, 0, _T(""), _T("") },
            { _T("JMP @#200"),  { 0000137, 0200 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0, 0, _T(""), _T("") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_CarryInstructions)
    {
        // ADC, SBC, ROR, ROL: the destination hint and the carry flag
        static const HintCase cases[] =
        {
            { _T("ADC R1, C=1"),  { 0005501 }, { 0, 010, 0, 0, 0, 0, STACK }, NO_MEM, TF_C, 0, 0,
                1, _T("R1=000010, C=1"), _T("") },
            { _T("ADC R1, C=0"),  { 0005501 }, { 0, 010, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0,
                1, _T("R1=000010, C=0"), _T("") },
            { _T("ADC (R1)"),     { 0005511 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0123456 } }, TF_C, 0, 0,
                2, _T("R1=000200, C=1"), _T("(R1)=123456") },
            { _T("SBC R1"),       { 0005601 }, { 0, 010, 0, 0, 0, 0, STACK }, NO_MEM, TF_C, 0, 0,
                1, _T("R1=000010, C=1"), _T("") },
            { _T("ROR R1"),       { 0006001 }, { 0, 010, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0,
                1, _T("R1=000010, C=0"), _T("") },
            { _T("ROL R1"),       { 0006101 }, { 0, 010, 0, 0, 0, 0, STACK }, NO_MEM, TF_C, 0, 0,
                1, _T("R1=000010, C=1"), _T("") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_ConditionCodeInstructions)
    {
        static const HintCase cases[] =
        {
            { _T("CLC"),  { 0000241 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_C | TF_N, 0, 0,
                1, _T("C=1, V=0, Z=0, N=1"), _T("") },
            { _T("SEC"),  { 0000261 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_V | TF_Z, 0, 0,
                1, _T("C=0, V=1, Z=1, N=0"), _T("") },
            { _T("CCC"),  { 0000257 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_ALL, 0, 0,
                1, _T("C=1, V=1, Z=1, N=1"), _T("") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_HaltModeCommands)
    {
        // The hints do not check the mode; the cases use the HALT mode, where the commands work
        static const HintCase cases[] =
        {
            { _T("MFUS"), { 0000021 }, { 045, 0, 0, 0, 0, 0123, STACK }, NO_MEM, 0400, 0, 0,
                1, _T("R5=000123, R0=000045"), _T("") },
            { _T("MTUS"), { 0000031 }, { 045, 0, 0, 0, 0, 0123, STACK }, NO_MEM, 0400, 0, 0,
                1, _T("R0=000045, R5=000123"), _T("") },
            { _T("RCPC"), { 0000022 }, { 045, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0400, 01234, 0,
                1, _T("PC'=001234, R0=000045"), _T("") },
            { _T("RCPS"), { 0000024 }, { 045, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0400, 0, 0340,
                1, _T("PS'=000340, R0=000045"), _T("") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    // The jump hints

    TEST_METHOD(Hint_Jump_NoHint)
    {
        static const JumpHintCase cases[] =
        {
            { _T("MOV R1,R2"), { 0010102 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0, _T(""), true },
            { _T("BR"),        { 0000402 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0, _T(""), true },   // always jumps
            { _T("JMP (R1)"),  { 0000111 }, { 0, 0200, 0, 0, 0, 0, STACK }, NO_MEM, 0, _T(""), true },
        };
        CheckJumpHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_Jump_ConditionalBranches)
    {
        // The text shows the flags the condition uses, the prediction follows the condition
        static const JumpHintCase cases[] =
        {
            { _T("BNE, Z=0"),  { 0001002 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0,    _T("Z=0"), true },
            { _T("BNE, Z=1"),  { 0001002 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_Z, _T("Z=1"), false },
            { _T("BEQ, Z=0"),  { 0001402 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0,    _T("Z=0"), false },
            { _T("BEQ, Z=1"),  { 0001402 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_Z, _T("Z=1"), true },

            { _T("BGE, N=0 V=0"), { 0002002 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0,           _T("N=0, V=0"), true },
            { _T("BGE, N=1 V=0"), { 0002002 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_N,        _T("N=1, V=0"), false },
            { _T("BGE, N=1 V=1"), { 0002002 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_N | TF_V, _T("N=1, V=1"), true },
            { _T("BLT, N=0 V=0"), { 0002402 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0,           _T("N=0, V=0"), false },
            { _T("BLT, N=0 V=1"), { 0002402 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_V,        _T("N=0, V=1"), true },
            { _T("BLT, N=1 V=1"), { 0002402 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_N | TF_V, _T("N=1, V=1"), false },

            { _T("BGT, all 0"),   { 0003002 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0,           _T("N=0, V=0, Z=0"), true },
            { _T("BGT, Z=1"),     { 0003002 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_Z,        _T("N=0, V=0, Z=1"), false },
            { _T("BGT, N=1"),     { 0003002 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_N,        _T("N=1, V=0, Z=0"), false },
            { _T("BLE, all 0"),   { 0003402 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0,           _T("N=0, V=0, Z=0"), false },
            { _T("BLE, Z=1"),     { 0003402 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_Z,        _T("N=0, V=0, Z=1"), true },
            { _T("BLE, N=1 V=1, Z=0"), { 0003402 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_N | TF_V, _T("N=1, V=1, Z=0"), false },

            { _T("BPL, N=0"),  { 0100002 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0,    _T("N=0"), true },
            { _T("BPL, N=1"),  { 0100002 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_N, _T("N=1"), false },
            { _T("BMI, N=0"),  { 0100402 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0,    _T("N=0"), false },
            { _T("BMI, N=1"),  { 0100402 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_N, _T("N=1"), true },

            { _T("BHI, C=0 Z=0"), { 0101002 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0,    _T("C=0, Z=0"), true },
            { _T("BHI, C=1 Z=0"), { 0101002 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_C, _T("C=1, Z=0"), false },
            { _T("BHI, C=0 Z=1"), { 0101002 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_Z, _T("C=0, Z=1"), false },
            { _T("BLOS, C=0 Z=0"), { 0101402 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0,    _T("C=0, Z=0"), false },
            { _T("BLOS, C=1 Z=0"), { 0101402 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_C, _T("C=1, Z=0"), true },

            { _T("BVC, V=0"),  { 0102002 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0,    _T("V=0"), true },
            { _T("BVC, V=1"),  { 0102002 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_V, _T("V=1"), false },
            { _T("BVS, V=0"),  { 0102402 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0,    _T("V=0"), false },
            { _T("BVS, V=1"),  { 0102402 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_V, _T("V=1"), true },

            { _T("BCC, C=0"),  { 0103002 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0,    _T("C=0"), true },
            { _T("BCC, C=1"),  { 0103002 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_C, _T("C=1"), false },
            { _T("BCS, C=0"),  { 0103402 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0,    _T("C=0"), false },
            { _T("BCS, C=1"),  { 0103402 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, TF_C, _T("C=1"), true },
        };
        CheckJumpHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_Jump_Sob)
    {
        // SOB decrements the register and jumps unless the result is zero
        static const JumpHintCase cases[] =
        {
            { _T("SOB R1, R1=5"), { 0077102 }, { 0, 5, 0, 0, 0, 0, STACK }, NO_MEM, 0, _T("R1=000005"), true },
            { _T("SOB R1, R1=1"), { 0077102 }, { 0, 1, 0, 0, 0, 0, STACK }, NO_MEM, 0, _T("R1=000001"), false },
            { _T("SOB R1, R1=0"), { 0077102 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0, _T("R1=000000"), true },  // 0 - 1 = 177777
            { _T("SOB R5, R5=2"), { 0077502 }, { 0, 0, 0, 0, 0, 2, STACK }, NO_MEM, 0, _T("R5=000002"), true },
        };
        CheckJumpHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_Jump_Subroutines)
    {
        static const JumpHintCase cases[] =
        {
            // JSR with a register other than PC shows the link register; CALL has no jump hint
            { _T("JSR R5,(R1)"), { 0004511 }, { 0, 0200, 0, 0, 0, 010, STACK }, NO_MEM, 0, _T("R5=000010"), true },
            { _T("JSR R3,@#200"), { 0004337, 0200 }, { 0, 0, 0, 3, 0, 0, STACK }, NO_MEM, 0, _T("R3=000003"), true },
            { _T("CALL @#200"), { 0004737, 0200 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0, _T(""), true },
            // RETURN is RTS PC: the new PC is the word at (SP); RTS Rn takes it from Rn, and then pops Rn
            { _T("RETURN"), { 0000207 }, { 0, 0, 0, 0, 0, 0, STACK }, { { STACK, 01234 } }, 0, _T("(SP)=001234"), true },
            { _T("RTS R5"), { 0000205 }, { 0, 0, 0, 0, 0, 02000, STACK }, { { STACK, 01234 } }, 0,
                _T("R5=002000, (SP)=001234"), true },
            { _T("RTI"),    { 0000002 }, { 0, 0, 0, 0, 0, 0, STACK }, { { STACK, 01234 } }, 0, _T("(SP)=001234"), true },
            { _T("RTT"),    { 0000006 }, { 0, 0, 0, 0, 0, 0, STACK }, { { STACK, 01234 } }, 0, _T("(SP)=001234"), true },
        };
        CheckJumpHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_Jump_Traps)
    {
        // The hint shows the new PC taken from the vector: BPT 014, IOT 020, EMT 030, TRAP 034
        static const JumpHintCase cases[] =
        {
            { _T("BPT"),    { 0000003 },  { 0, 0, 0, 0, 0, 0, STACK }, { { 014, 0300 } }, 0, _T("(000014)=000300"), true },
            { _T("IOT"),    { 0000004 },  { 0, 0, 0, 0, 0, 0, STACK }, { { 020, 0301 } }, 0, _T("(000020)=000301"), true },
            { _T("EMT 5"),  { 0104005 },  { 0, 0, 0, 0, 0, 0, STACK }, { { 030, 0302 } }, 0, _T("(000030)=000302"), true },
            { _T("TRAP 5"), { 0104405 },  { 0, 0, 0, 0, 0, 0, STACK }, { { 034, 0303 } }, 0, _T("(000034)=000303"), true },
        };
        CheckJumpHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    // Known bugs of the hints, see docs/hints-improvements.md. The expected text is what the processor does.

    // 1.1: MARK nn: SP = PC + 2*nn, where PC is the address after MARK; PC = R5; R5 = (SP)+.
    // The hint must show the new PC (R5) and the word that becomes R5, at PROG + 2 + 2 * 3 = 01010.
    BEGIN_TEST_METHOD_ATTRIBUTE(Hint_MarkShowsTheWordThatBecomesR5_KnownBug)
        TEST_IGNORE()
    END_TEST_METHOD_ATTRIBUTE()
    TEST_METHOD(Hint_MarkShowsTheWordThatBecomesR5_KnownBug)
    {
        static const HintCase cases[] =
        {
            { _T("MARK 3"), { 0006403 }, { 0, 0, 0, 0, 0, 0300, STACK }, { { 01010, 05252 } }, 0, 0, 0,
                2, _T("R5=000300"), _T("(001010)=005252") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    // 1.2: the destination is evaluated after the autoincrement / autodecrement of the source
    BEGIN_TEST_METHOD_ATTRIBUTE(Hint_DestinationSeesTheAutoincrementOfTheSource_KnownBug)
        TEST_IGNORE()
    END_TEST_METHOD_ATTRIBUTE()
    TEST_METHOD(Hint_DestinationSeesTheAutoincrementOfTheSource_KnownBug)
    {
        static const HintCase cases[] =
        {
            // The source is at 0200, R0 becomes 0202, the destination is at 0202
            { _T("MOV (R0)+,(R0)+"), { 0012020 }, { 0200, 0, 0, 0, 0, 0, STACK }, { { 0200, 0111 }, { 0202, 0222 } }, 0, 0, 0,
                2, _T("R0=000200, R0=000202"), _T("(R0)=000111, (R0)=000222") },
            // R1 becomes 0202, the destination is at 0202 + 2 = 0204
            { _T("MOV (R1)+,2(R1)"), { 0012161, 2 }, { 0, 0200, 0, 0, 0, 0, STACK },
                { { 0200, 0111 }, { 0202, 0222 }, { 0204, 0333 } }, 0, 0, 0,
                2, _T("R1=000200, R1=000202"), _T("(R1)=000111, (R1+000002)=000333") },
            // R2 becomes 0204 after the source, the destination is at 0204 - 2 = 0202
            { _T("MOV -(R2),-(R2)"), { 0014242 }, { 0, 0, 0206, 0, 0, 0, STACK }, { { 0202, 0222 }, { 0204, 0111 } }, 0, 0, 0,
                2, _T("R2=000206, R2=000204"), _T("(R2-2)=000111, (R2-2)=000222") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    // 1.3: PC as a register has an extra word only in the modes 2, 3, 6 and 7. MOV PC,2(R1) has one
    // extra word (the index of the destination), and the hint takes it for the word of the source.
    BEGIN_TEST_METHOD_ATTRIBUTE(Hint_PcRegisterWithoutAnExtraWord_KnownBug)
        TEST_IGNORE()
    END_TEST_METHOD_ATTRIBUTE()
    TEST_METHOD(Hint_PcRegisterWithoutAnExtraWord_KnownBug)
    {
        static const HintCase cases[] =
        {
            // The third word (05252) is not a part of the instruction
            { _T("MOV PC,2(R1)"), { 0010761, 2, 05252 }, { 0, 0200, 0, 0, 0, 0, STACK },
                { { 0200, 0111 }, { 0202, 0777 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("(R1+000002)=000777") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    // 1.4: a byte operation with -(SP) steps by 2, so the operand is the low byte of the word at SP-2
    BEGIN_TEST_METHOD_ATTRIBUTE(Hint_ByteDecrementOfSpSteps2_KnownBug)
        TEST_IGNORE()
    END_TEST_METHOD_ATTRIBUTE()
    TEST_METHOD(Hint_ByteDecrementOfSpSteps2_KnownBug)
    {
        static const HintCase cases[] =
        {
            { _T("CLRB -(SP)"), { 0105046 }, { 0, 0, 0, 0, 0, 0, 0402 }, { { 0400, 0123456 } }, 0, 0, 0,
                2, _T("SP=000402"), _T("(SP-2)=056") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    // 1.7: SUB has the bit 15 set, but it is a word operation: the operand is a word
    BEGIN_TEST_METHOD_ATTRIBUTE(Hint_SubIsAWordOperation_KnownBug)
        TEST_IGNORE()
    END_TEST_METHOD_ATTRIBUTE()
    TEST_METHOD(Hint_SubIsAWordOperation_KnownBug)
    {
        static const HintCase cases[] =
        {
            { _T("SUB (R1),R0"), { 0161100 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                2, _T("R1=000200, R0=000000"), _T("(R1)=123456") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    // 1.5: an I/O port is not memory: reading it from the view gives 0, and the hint must not show it as a value.
    // The CPU in USER mode sees 0160000-0177777 as I/O.
    BEGIN_TEST_METHOD_ATTRIBUTE(Hint_IoPortIsNotShownAsAValue_KnownBug)
        TEST_IGNORE()
    END_TEST_METHOD_ATTRIBUTE()
    TEST_METHOD(Hint_IoPortIsNotShownAsAValue_KnownBug)
    {
        static const HintCase cases[] =
        {
            { _T("MOV @#177560,R0"), { 0013700, 0177560 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0,
                1, _T("(177560)=I/O, R0=000000"), _T("") },
            { _T("MOV (R1),R2"),     { 0011102 }, { 0, 0177560, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0,
                2, _T("R1=177560, R2=000000"), _T("(R1)=I/O") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }
};
