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
// (Disasm_GetHint and Disasm_GetJumpConditionHint), see docs/hints-improvements.md
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

const MemWord NO_MEM_ARRAY[MAX_MEM] = { { 0, 0 } };

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
    const TCHAR* line3;  // the third line; omitted = empty
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

// Compare the hint with the expected lines; a null expected line is the empty line
void CompareHint(const DisasmHint& hint, const TCHAR* name, uint16_t opcode, int lines,
                 const TCHAR* line1, const TCHAR* line2, const TCHAR* line3)
{
    const TCHAR* expected[3] = { line1, line2, line3 };
    TCHAR msg[100];
    _stprintf_s(msg, _T("%s, opcode %06o: lines"), name, (unsigned int) opcode);
    Assert::AreEqual(lines, hint.count, msg);
    for (int i = 0; i < 3; i++)
    {
        _stprintf_s(msg, _T("%s, opcode %06o: line %d"), name, (unsigned int) opcode, i + 1);
        Assert::AreEqual(expected[i] != nullptr ? expected[i] : _T(""), hint.lines[i], msg);
    }
}

void CheckHint(const HintCase& c)
{
    TestBoard tb;
    Prepare(tb, c.words, 3, c.regs, c.mem, c.psw, c.cpc, c.cpsw);

    DisasmHint hint;
    Disasm_GetHint(c.words, tb.CPU(), tb.CPU()->GetMemoryControllerConst(), &hint);
    CompareHint(hint, c.name, c.words[0], c.lines, c.line1, c.line2, c.line3);
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

// The cases of Disasm_GetHint: up to three lines
struct HintCase3
{
    const TCHAR* name;
    uint16_t words[3];
    uint16_t regs[7];
    MemWord mem[MAX_MEM];
    uint16_t psw;
    int lines;
    const TCHAR* line1;
    const TCHAR* line2;
    const TCHAR* line3;
};

void CheckHint3(const HintCase3& c)
{
    TestBoard tb;
    Prepare(tb, c.words, 3, c.regs, c.mem, c.psw, 0, 0);

    DisasmHint hint;
    Disasm_GetHint(c.words, tb.CPU(), tb.CPU()->GetMemoryControllerConst(), &hint);
    CompareHint(hint, c.name, c.words[0], c.lines, c.line1, c.line2, c.line3);
}
void CheckHints3(const HintCase3* cases, size_t count)
{
    for (size_t i = 0; i < count; i++)
        CheckHint3(cases[i]);
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
                3, _T("R1=000200"), _T("(R1)=000300"), _T("(000300)=000000") },  // the pointer is at 0200, the operand at 0300 is 0
            { _T("CLR @-(R1)"),   { 0005051 }, { 0, 0202, 0, 0, 0, 0, STACK }, { { 0200, 0300 } }, 0, 0, 0,
                3, _T("R1=000202"), _T("(R1-2)=000300"), _T("(000300)=000000") },
            { _T("CLR 4(R1)"),    { 0005061, 4 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0204, 0123456 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("(R1+000004)=123456") },
            { _T("CLR @4(R1)"),   { 0005071, 4 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0204, 0300 } }, 0, 0, 0,
                3, _T("R1=000200"), _T("(R1+000004)=000300"), _T("(000300)=000000") },
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
                3, _T("R1=000200"), _T("(R1)=000300"), _T("(000300)=000") },  // the pointer is a word even for a byte operation; the operand is a byte
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

    TEST_METHOD(Hint_AddressEdges)
    {
        // The index is added in 16 bits; SP has the same word hints as the other registers; PC-relative
        // operands count the extra words of the operands before them (the PC is read after the index word)
        static const HintCase cases[] =
        {
            { _T("CLR 4(R1), 16-bit wrap"),  { 0005061, 4 }, { 0, 0177776, 0, 0, 0, 0, STACK }, { { 02, 0123456 } }, 0, 0, 0,
                2, _T("R1=177776"), _T("(R1+000004)=123456") },   // 0177776 + 4 = 0200002 = 000002
            { _T("CLR @4(R1), 16-bit wrap"), { 0005071, 4 }, { 0, 0177776, 0, 0, 0, 0, STACK }, { { 02, 0300 } }, 0, 0, 0,
                3, _T("R1=177776"), _T("(R1+000004)=000300"), _T("(000300)=000000") },
            { _T("CLR -(SP)"),   { 0005046 }, { 0, 0, 0, 0, 0, 0, 0402 }, { { 0400, 0123456 } }, 0, 0, 0,
                2, _T("SP=000402"), _T("(SP-2)=123456") },
            { _T("CLR (SP)+"),   { 0005026 }, { 0, 0, 0, 0, 0, 0, 0400 }, { { 0400, 0123456 } }, 0, 0, 0,
                2, _T("SP=000400"), _T("(SP)=123456") },
            { _T("CLRB (SP)+"),  { 0105026 }, { 0, 0, 0, 0, 0, 0, 0400 }, { { 0400, 0123456 } }, 0, 0, 0,
                2, _T("SP=000400"), _T("(SP)=056") },   // the autoincrement of a byte operation is 2, the operand is at SP
            // The source has an extra word, so the PC-relative destination is read after two extra words:
            // the PC is 01006 and 01006 + 0177172 = 0200 (16 bits)
            { _T("MOV #1234,200(PC)"),    { 0012767, 01234, 0177172 }, { 0, 0, 0, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                1, _T("(000200)=123456"), _T("") },
            { _T("MOV 4(R1),200(PC)"),    { 0016167, 4, 0177172 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0123456 }, { 0204, 0111 } }, 0, 0, 0,
                2, _T("R1=000200, (000200)=123456"), _T("(R1+000004)=000111") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_RegisterAndOperand)
    {
        // MUL, DIV, ASH, ASHC, XOR: the register in bits 8..6 and the operand; the hint follows the notation:
        // "operand, register" for MUL, ASH (and DIV, ASHC), "register, operand" for XOR
        static const HintCase cases[] =
        {
            { _T("MUL (R2),R1"),  { 0070112 }, { 0, 3, 0200, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                2, _T("R2=000200, R1=000003"), _T("(R2)=123456") },
            { _T("XOR R1,(R2)"),  { 0074112 }, { 0, 7, 0200, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                2, _T("R1=000007, R2=000200"), _T("           (R2)=123456") },  // aligned under R2
            { _T("ASH R2,R1"),    { 0072102 }, { 0, 3, 2, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0,
                1, _T("R2=000002, R1=000003"), _T("") },
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
                1, _T("R1=000200"), _T("") },
            // JMP with the relative and the absolute addressing is shown by the jump arrow, not by a hint
            { _T("JMP 10(PC)"), { 0000167, 010 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0, 0, _T(""), _T("") },
            { _T("JMP @#200"),  { 0000137, 0200 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0, 0, _T(""), _T("") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_CompareAndLogicInstructions)
    {
        // CMP, BIT, BIC, BIS: source and destination like MOV. The word at 0200 is 0123456 (low byte 056,
        // high byte 0247), the word at 0300 is 0111111 (low byte 0111, high byte 0222).
        // When only the destination has the second line, it is aligned under the destination hint.
        static const HintCase cases[] =
        {
            { _T("CMP (R1),(R2)"),  { 0021112 }, { 0, 0200, 0300, 0, 0, 0, STACK }, { { 0200, 0111 }, { 0300, 0222 } }, 0, 0, 0,
                2, _T("R1=000200, R2=000300"), _T("(R1)=000111, (R2)=000222") },
            { _T("BIT (R1),R2"),    { 0031102 }, { 0, 0200, 7, 0, 0, 0, STACK }, { { 0200, 0111 } }, 0, 0, 0,
                2, _T("R1=000200, R2=000007"), _T("(R1)=000111") },
            { _T("BIC R1,(R2)"),    { 0040112 }, { 0, 7, 0200, 0, 0, 0, STACK }, { { 0200, 0111 } }, 0, 0, 0,
                2, _T("R1=000007, R2=000200"), _T("           (R2)=000111") },
            { _T("BIS (R1)+,R2"),   { 0052102 }, { 0, 0200, 7, 0, 0, 0, STACK }, { { 0200, 0111 } }, 0, 0, 0,
                2, _T("R1=000200, R2=000007"), _T("(R1)=000111") },
            { _T("CMPB (R1),(R2)"), { 0121112 }, { 0, 0200, 0300, 0, 0, 0, STACK }, { { 0200, 0123456 }, { 0300, 0111111 } }, 0, 0, 0,
                2, _T("R1=000200, R2=000300"), _T("(R1)=056, (R2)=111") },
            { _T("BITB (R1),R2"),   { 0131102 }, { 0, 0200, 7, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                2, _T("R1=000200, R2=000007"), _T("(R1)=056") },
            { _T("BICB R1,(R2)"),   { 0140112 }, { 0, 7, 0301, 0, 0, 0, STACK }, { { 0300, 0111111 } }, 0, 0, 0,
                2, _T("R1=000007, R2=000301"), _T("           (R2)=222") },  // the odd address: the high byte
            { _T("BISB (R1)+,R2"),  { 0152102 }, { 0, 0201, 7, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                2, _T("R1=000201, R2=000007"), _T("(R1)=247") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_StatusWordAndSignExtension)
    {
        // MTPS and MFPS are byte operations (the bit 15 of the opcode is set), SXT is a word operation
        static const HintCase cases[] =
        {
            // MTPS and MFPS show the low byte of the PSW, SXT shows N
            { _T("MTPS R1"),    { 0106401 }, { 0, 0123, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0,
                1, _T("R1=000123, PS=000"), _T("") },
            { _T("MTPS (R1)"),  { 0106411 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 0, 0, 0,
                2, _T("R1=000200, PS=000"), _T("(R1)=056") },
            { _T("MFPS (R1)+"), { 0106721 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 013, 0, 0,
                2, _T("R1=000200, PS=013"), _T("(R1)=056") },
            { _T("SXT R1"),     { 0006701 }, { 0, 0123, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0,
                1, _T("R1=000123, N=0"), _T("") },
            { _T("SXT (R1)"),   { 0006711 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0123456 } }, 010, 0, 0,
                2, _T("R1=000200, N=1"), _T("(R1)=123456") },
            { _T("SXT 2(R1)"),  { 0006761, 2 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0202, 0111 } }, 0, 0, 0,
                2, _T("R1=000200, N=0"), _T("(R1+000002)=000111") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_DivAndAshc)
    {
        // DIV and ASHC use the register pair Rn:Rn|1; the hint shows the operand first, then the pair
        // (for an odd register Rn|1 is the register itself). The registers already shown are not repeated.
        // MUL reads only Rn: the operand, then the register.
        static const HintCase cases[] =
        {
            { _T("DIV (R2),R0"), { 0071012 }, { 1, 2, 0200, 0, 0, 0, STACK }, { { 0200, 0111 } }, 0, 0, 0,
                2, _T("R2=000200, R0=000001, R1=000002"), _T("(R2)=000111") },
            { _T("ASHC R2,R0"),  { 0073002 }, { 1, 2, 3, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0,
                1, _T("R2=000003, R0=000001, R1=000002"), _T("") },
            { _T("DIV (R2),R3"), { 0071312 }, { 0, 0, 0200, 5, 0, 0, STACK }, { { 0200, 0111 } }, 0, 0, 0,
                2, _T("R2=000200, R3=000005"), _T("(R2)=000111") },
            { _T("ASHC R1,R0"),  { 0073001 }, { 1, 2, 3, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0,
                1, _T("R1=000002, R0=000001"), _T("") },
            { _T("MUL R2,R0"),   { 0070002 }, { 1, 2, 3, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0,
                1, _T("R2=000003, R0=000001"), _T("") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_OldInterfaceWrappers)
    {
        // Disasm_GetInstructionHint (two lines, a regular instruction) and Disasm_GetJumpConditionHint (the first
        // line of the jump hint) are wrappers for the callers of the old interface
        {
            TestBoard tb;
            const uint16_t words[3] = { 0012122, 0, 0 };  // MOV (R1)+,(R2)+
            const uint16_t regs[7] = { 0, 0200, 0300, 0, 0, 0, STACK };
            const MemWord mem[MAX_MEM] = { { 0200, 0111 }, { 0300, 0222 } };
            Prepare(tb, words, 3, regs, mem, 0, 0, 0);
            TCHAR line1[64] = _T("x"), line2[64] = _T("x");
            int lines = Disasm_GetInstructionHint(words, tb.CPU(), tb.CPU()->GetMemoryControllerConst(), line1, line2);
            Assert::AreEqual(2, lines);
            Assert::AreEqual(_T("R1=000200, R2=000300"), line1);
            Assert::AreEqual(_T("(R1)=000111, (R2)=000222"), line2);
        }
        {
            TestBoard tb;
            const uint16_t words[3] = { 0001002, 0, 0 };  // BNE
            const uint16_t regs[7] = { 0, 0, 0, 0, 0, 0, STACK };
            Prepare(tb, words, 3, regs, NO_MEM_ARRAY, 0, 0, 0);
            TCHAR text[64] = _T("x");
            bool predict = Disasm_GetJumpConditionHint(words, tb.CPU(), tb.CPU()->GetMemoryControllerConst(), text);
            Assert::IsTrue(predict);  // Z=0: BNE jumps
            Assert::AreEqual(_T("Z=0"), text);
        }
    }

    TEST_METHOD(Hint_Fis)
    {
        // FADD, FSUB, FMUL, FDIV: the register points at two 32-bit values, (Rn) and (Rn+4), shown as word pairs HIGH:LOW
        static const HintCase cases[] =
        {
            { _T("FADD R1"), { 0075001 }, { 0, 0200, 0, 0, 0, 0, STACK },
                { { 0200, 040200 }, { 0202, 0 }, { 0204, 040000 }, { 0206, 01 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("(R1)=040200:000000, (R1+4)=040000:000001") },
            { _T("FSUB R1"), { 0075011 }, { 0, 0200, 0, 0, 0, 0, STACK },
                { { 0200, 040200 }, { 0202, 0 }, { 0204, 040000 }, { 0206, 01 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("(R1)=040200:000000, (R1+4)=040000:000001") },
            { _T("FMUL R1"), { 0075021 }, { 0, 0200, 0, 0, 0, 0, STACK },
                { { 0200, 040200 }, { 0202, 0 }, { 0204, 040000 }, { 0206, 01 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("(R1)=040200:000000, (R1+4)=040000:000001") },
            { _T("FDIV R1"), { 0075031 }, { 0, 0200, 0, 0, 0, 0, STACK },
                { { 0200, 040200 }, { 0202, 0 }, { 0204, 040000 }, { 0206, 01 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("(R1)=040200:000000, (R1+4)=040000:000001") },
            { _T("FADD SP"), { 0075006 }, { 0, 0, 0, 0, 0, 0, 0200 },
                { { 0200, 040200 }, { 0202, 0 }, { 0204, 040000 }, { 0206, 0 } }, 0, 0, 0,
                2, _T("SP=000200"), _T("(SP)=040200:000000, (SP+4)=040000:000000") },
            // an odd register cannot address a word
            { _T("FADD R1, odd"), { 0075001 }, { 0, 0201, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0,
                2, _T("R1=000201"), _T("(R1)=N/A, (R1+4)=N/A") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_CallAndJmpDestinations)
    {
        // CALL is JSR PC,dst. The jump hint does not cover it (its range stops at JSR SP), so the instruction hint
        // shows the destination, except for the relative and the absolute addressing, which have the arrow.
        static const HintCase cases[] =
        {
            { _T("CALL (R1)"),     { 0004711 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0111 } }, 0, 0, 0,
                1, _T("R1=000200"), _T("") },
            { _T("JMP (R1)+"),     { 0000121 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0111 } }, 0, 0, 0,
                1, _T("R1=000200"), _T("") },
            { _T("JMP -(R1)"),     { 0000141 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0176, 0111 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("R1-2=000176") },
            { _T("JMP 4(R1)"),     { 0000161, 4 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0204, 0111 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("R1+4=000204") },
            { _T("CALL @(R1)+"),   { 0004731 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0300 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("(R1)=000300") },
            { _T("JMP @(R1)+"),    { 0000131 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0300 } }, 0, 0, 0,
                2, _T("R1=000200"), _T("(R1)=000300") },
            // the pointer is the jump target; the word at the target is the next instruction, no third line
            { _T("JMP @2(R0)"),    { 0000170, 2 }, { 0200, 0, 0, 0, 0, 0, STACK }, { { 0202, 0300 }, { 0300, 0222 } }, 0, 0, 0,
                3, _T("R0=000200"), _T("R0+2=000202"), _T("(000202)=000300") },
            { _T("JMP @-(R0)"),    { 0000150 }, { 0200, 0, 0, 0, 0, 0, STACK }, { { 0176, 0300 } }, 0, 0, 0,
                3, _T("R0=000200"), _T("R0-2=000176"), _T("(000176)=000300") },
            { _T("CALL @#200"),    { 0004737, 0200 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0, 0, _T(""), _T("") },
            { _T("CALL 10(PC)"),   { 0004767, 010 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, 0, 0, _T(""), _T("") },
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
            // the PC destination: the carry alone, or after the destination hint if it has one
            { _T("ADC PC"),       { 0005507 }, { 0, 010, 0, 0, 0, 0, STACK }, NO_MEM, TF_C, 0, 0,
                1, _T("C=1"), _T("") },
            { _T("ADC @#300"),    { 0005537, 0300 }, { 0, 010, 0, 0, 0, 0, STACK }, { { 0300, 0123456 } }, 0, 0, 0,
                1, _T("(000300)=123456, C=0"), _T("") },
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
        // The cases use the HALT mode, where the commands work
        static const HintCase cases[] =
        {
            // MFUS reads (R5), MTUS overwrites (R5-2), both in the USER mode map; RAM here, the values are put there
            { _T("MFUS"), { 0000021 }, { 045, 0, 0, 0, 0, 0124, STACK }, { { 0124, 07777 } }, 0400, 0, 0,
                2, _T("R5=000124, R0=000045"), _T("(R5)=007777") },
            { _T("MTUS"), { 0000031 }, { 045, 0, 0, 0, 0, 0124, STACK }, { { 0122, 05252 } }, 0400, 0, 0,
                2, _T("R0=000045, R5=000124"), _T("(R5-2)=005252") },
            { _T("RCPC"), { 0000022 }, { 045, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0400, 01234, 0,
                1, _T("PC'=001234, R0=000045"), _T("") },
            { _T("RCPS"), { 0000024 }, { 045, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0400, 0, 0340,
                1, _T("PS'=000340, R0=000045"), _T("") },
            { _T("RUN"),  { 0000010 }, { 045, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0400, 01234, 0340,
                1, _T("PC'=001234, PS'=000340"), _T("") },
            { _T("STEP"), { 0000014 }, { 045, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0400, 01234, 0340,
                1, _T("PC'=001234, PS'=000340"), _T("") },
            { _T("WCPC"), { 0000032 }, { 045, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0400, 01234, 0340,
                1, _T("R0=000045, PC'=001234"), _T("") },
            { _T("WCPS"), { 0000034 }, { 045, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0400, 01234, 0340,
                1, _T("R0=000045, PS'=000340"), _T("") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_HaltModeCommandsInUserModeShowTheMode)
    {
        // In USER mode every command 000010..000037 traps as a reserved instruction: the hint is the mode only
        for (uint16_t instr = 0000010; instr <= 0000037; instr++)
        {
            const HintCase cases[] =
            {
                { _T("HALT-mode command"), { instr }, { 045, 0, 0, 0, 0, 0123, STACK }, NO_MEM, 0, 01234, 0340,
                    1, _T("USER mode"), _T("") },
            };
            CheckHints(cases, 1);
        }
    }

    TEST_METHOD(Hint_Jsr)
    {
        // JSR shows the link register, and the destination except the relative and the absolute addressing
        // (they have the arrow); CALL (JSR PC) has no link register to show
        static const HintCase3 cases[] =
        {
            { _T("JSR R5,(R1)"),    { 0004511 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0111 } }, 0,
                1, _T("R5=000010, R1=000200"), _T(""), _T("") },
            { _T("JSR R5,@(R1)+"),  { 0004531 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0300, 0222 } }, 0,
                2, _T("R5=000010, R1=000200"), _T("           (R1)=000300"), _T("") },
            { _T("JSR R5,@4(R1)"),  { 0004571, 4 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0204, 0300 } }, 0,
                3, _T("R5=000010, R1=000200"), _T("           R1+4=000204"), _T("           (000204)=000300") },
            { _T("JSR R3,@#200"),   { 0004337, 0200 }, { 0, 0, 0, 3, 0, 0, STACK }, NO_MEM, 0,
                1, _T("R3=000003"), _T(""), _T("") },
            { _T("JSR R5,10(PC)"),  { 0004567, 010 }, { 0, 0, 0, 0, 0, 010, STACK }, NO_MEM, 0,
                1, _T("R5=000010"), _T(""), _T("") },
            { _T("JSR SP,(R1)"),    { 0004611 }, { 0, 0200, 0, 0, 0, 0, 0400 }, { { 0200, 0111 } }, 0,
                1, _T("SP=000400, R1=000200"), _T(""), _T("") },
            { _T("CALL (R1)"),      { 0004711 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0111 } }, 0,
                1, _T("R1=000200"), _T(""), _T("") },
            { _T("CALL @#200"),     { 0004737, 0200 }, { 0, 0, 0, 0, 0, 0, STACK }, NO_MEM, 0, 0, _T(""), _T(""), _T("") },
        };
        CheckHints3(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_JumpDestinationsAllModes)
    {
        // JMP, CALL (JSR PC) and JSR R5: every addressing mode of the destination, on R1 and on PC.
        // The word at the target is the next instruction and is never shown; an address calculated from a register
        // is "expression=address", a deferred mode adds the pointer; the relative and the absolute modes have the arrow.
        // R1 = 0200, R5 = 010; 0200 = 0300 (a pointer), 0176 = 0301, 0204 = 0302, 01014 = 0300 (a PC-relative pointer)
        static const HintCase3 cases[] =
        {
            { _T("JMP R1"), { 0101 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                1, _T("R1=000200"), _T(""), _T("") },
            { _T("JMP (R1)"), { 0111 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                1, _T("R1=000200"), _T(""), _T("") },
            { _T("JMP (R1)+"), { 0121 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                1, _T("R1=000200"), _T(""), _T("") },
            { _T("JMP @(R1)+"), { 0131 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                2, _T("R1=000200"), _T("(R1)=000300"), _T("") },
            { _T("JMP -(R1)"), { 0141 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                2, _T("R1=000200"), _T("R1-2=000176"), _T("") },
            { _T("JMP @-(R1)"), { 0151 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                3, _T("R1=000200"), _T("R1-2=000176"), _T("(000176)=000301") },
            { _T("JMP 4(R1)"), { 0161, 04 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                2, _T("R1=000200"), _T("R1+4=000204"), _T("") },
            { _T("JMP @4(R1)"), { 0171, 04 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                3, _T("R1=000200"), _T("R1+4=000204"), _T("(000204)=000302") },
            { _T("JMP #300"), { 0127, 0300 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                0, _T(""), _T(""), _T("") },
            { _T("JMP @#200"), { 0137, 0200 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                0, _T(""), _T(""), _T("") },
            { _T("JMP (PC)"), { 0117 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                0, _T(""), _T(""), _T("") },
            { _T("JMP -(PC)"), { 0147 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                0, _T(""), _T(""), _T("") },
            { _T("JMP @-(PC)"), { 0157 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                0, _T(""), _T(""), _T("") },
            { _T("JMP 10(PC)"), { 0167, 010 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                0, _T(""), _T(""), _T("") },
            { _T("JMP @10(PC)"), { 0177, 010 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                1, _T("(001014)=000300"), _T(""), _T("") },
            { _T("CALL R1"), { 04701 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                1, _T("R1=000200"), _T(""), _T("") },
            { _T("CALL (R1)"), { 04711 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                1, _T("R1=000200"), _T(""), _T("") },
            { _T("CALL (R1)+"), { 04721 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                1, _T("R1=000200"), _T(""), _T("") },
            { _T("CALL @(R1)+"), { 04731 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                2, _T("R1=000200"), _T("(R1)=000300"), _T("") },
            { _T("CALL -(R1)"), { 04741 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                2, _T("R1=000200"), _T("R1-2=000176"), _T("") },
            { _T("CALL @-(R1)"), { 04751 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                3, _T("R1=000200"), _T("R1-2=000176"), _T("(000176)=000301") },
            { _T("CALL 4(R1)"), { 04761, 04 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                2, _T("R1=000200"), _T("R1+4=000204"), _T("") },
            { _T("CALL @4(R1)"), { 04771, 04 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                3, _T("R1=000200"), _T("R1+4=000204"), _T("(000204)=000302") },
            { _T("CALL #300"), { 04727, 0300 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                0, _T(""), _T(""), _T("") },
            { _T("CALL @#200"), { 04737, 0200 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                0, _T(""), _T(""), _T("") },
            { _T("CALL (PC)"), { 04717 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                0, _T(""), _T(""), _T("") },
            { _T("CALL -(PC)"), { 04747 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                0, _T(""), _T(""), _T("") },
            { _T("CALL @-(PC)"), { 04757 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                0, _T(""), _T(""), _T("") },
            { _T("CALL 10(PC)"), { 04767, 010 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                0, _T(""), _T(""), _T("") },
            { _T("CALL @10(PC)"), { 04777, 010 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                1, _T("(001014)=000300"), _T(""), _T("") },
            { _T("JSR R5,R1"), { 04501 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                1, _T("R5=000010, R1=000200"), _T(""), _T("") },
            { _T("JSR R5,(R1)"), { 04511 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                1, _T("R5=000010, R1=000200"), _T(""), _T("") },
            { _T("JSR R5,(R1)+"), { 04521 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                1, _T("R5=000010, R1=000200"), _T(""), _T("") },
            { _T("JSR R5,@(R1)+"), { 04531 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                2, _T("R5=000010, R1=000200"), _T("           (R1)=000300"), _T("") },
            { _T("JSR R5,-(R1)"), { 04541 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                2, _T("R5=000010, R1=000200"), _T("           R1-2=000176"), _T("") },
            { _T("JSR R5,@-(R1)"), { 04551 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                3, _T("R5=000010, R1=000200"), _T("           R1-2=000176"), _T("           (000176)=000301") },
            { _T("JSR R5,4(R1)"), { 04561, 04 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                2, _T("R5=000010, R1=000200"), _T("           R1+4=000204"), _T("") },
            { _T("JSR R5,@4(R1)"), { 04571, 04 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                3, _T("R5=000010, R1=000200"), _T("           R1+4=000204"), _T("           (000204)=000302") },
            { _T("JSR R5,#300"), { 04527, 0300 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                1, _T("R5=000010"), _T(""), _T("") },
            { _T("JSR R5,@#200"), { 04537, 0200 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                1, _T("R5=000010"), _T(""), _T("") },
            { _T("JSR R5,(PC)"), { 04517 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                1, _T("R5=000010"), _T(""), _T("") },
            { _T("JSR R5,-(PC)"), { 04547 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                1, _T("R5=000010"), _T(""), _T("") },
            { _T("JSR R5,@-(PC)"), { 04557 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                1, _T("R5=000010"), _T(""), _T("") },
            { _T("JSR R5,10(PC)"), { 04567, 010 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                1, _T("R5=000010"), _T(""), _T("") },
            { _T("JSR R5,@10(PC)"), { 04577, 010 }, { 0, 0200, 0, 0, 0, 010, STACK }, { { 0200, 0300 }, { 0176, 0301 }, { 0204, 0302 }, { 01014, 0300 } }, 0,
                1, _T("R5=000010, (001014)=000300"), _T(""), _T("") },
        };
        CheckHints3(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_ReturnsAndTrapsHaveTheNewPsw)
    {
        // RTI and RTT pop the PC and the PSW; a trap takes the new PC and PSW from its vector, in USER mode
        static const HintCase3 cases[] =
        {
            { _T("RTI"),     { 0000002 }, { 0, 0, 0, 0, 0, 0, STACK }, { { STACK, 01234 }, { STACK + 2, 0340 } }, 0,
                2, _T("(SP)=001234"), _T("(SP+2)=000340"), _T("") },
            { _T("RTT"),     { 0000006 }, { 0, 0, 0, 0, 0, 0, STACK }, { { STACK, 01234 }, { STACK + 2, 0340 } }, 0,
                2, _T("(SP)=001234"), _T("(SP+2)=000340"), _T("") },
            { _T("BPT"),     { 0000003 }, { 0, 0, 0, 0, 0, 0, STACK }, { { 014, 0300 }, { 016, 0340 } }, 0,
                2, _T("(000014)=000300"), _T("(000016)=000340"), _T("") },
            { _T("IOT"),     { 0000004 }, { 0, 0, 0, 0, 0, 0, STACK }, { { 020, 0301 }, { 022, 0341 } }, 0,
                2, _T("(000020)=000301"), _T("(000022)=000341"), _T("") },
            { _T("EMT 5"),   { 0104005 }, { 0, 0, 0, 0, 0, 0, STACK }, { { 030, 0302 }, { 032, 0342 } }, 0,
                2, _T("(000030)=000302"), _T("(000032)=000342"), _T("") },
            { _T("TRAP 5"),  { 0104405 }, { 0, 0, 0, 0, 0, 0, STACK }, { { 034, 0303 }, { 036, 0343 } }, 0,
                2, _T("(000034)=000303"), _T("(000036)=000343"), _T("") },
            // RETURN and RTS have one line
            { _T("RETURN"),  { 0000207 }, { 0, 0, 0, 0, 0, 0, STACK }, { { STACK, 01234 } }, 0,
                1, _T("(SP)=001234"), _T(""), _T("") },
        };
        CheckHints3(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_GetHint_JumpInstructionsHaveTheJumpFlag)
    {
        // isJump is set for every jump and return, also when the hint is empty or comes from the instruction hint
        static const uint16_t jumps[] =
        {
            0000402, 0001002, 0003402, 0100002, 0103402,   // BR, BNE, BLE, BPL, BLO
            0077102,                                        // SOB
            0000111, 0000137, 0000167,                      // JMP (R1), JMP @#, JMP X(PC)
            0004511, 0004711, 0004737,                      // JSR R5,(R1), CALL (R1), CALL @#
            0000207, 0000205, 0000002, 0000006,             // RETURN, RTS R5, RTI, RTT
            0000003, 0000004, 0104005, 0104405,             // BPT, IOT, EMT, TRAP
        };
        static const uint16_t others[] =
        {
            0010102, 0005001, 0006403, 0000240, 0000001, 0000000, 0000005, 0070112,  // MOV, CLR, MARK, NOP, WAIT, HALT, RESET, MUL
        };
        for (int kind = 0; kind < 2; kind++)
        {
            const uint16_t* list = kind == 0 ? jumps : others;
            size_t count = kind == 0 ? sizeof(jumps) / sizeof(jumps[0]) : sizeof(others) / sizeof(others[0]);
            for (size_t i = 0; i < count; i++)
            {
                TestBoard tb;
                const uint16_t words[3] = { list[i], 0, 0 };
                const uint16_t regs[7] = { 0, 0200, 0, 0, 0, 010, STACK };
                Prepare(tb, words, 3, regs, NO_MEM_ARRAY, 0, 0, 0);
                DisasmHint hint;
                Disasm_GetHint(words, tb.CPU(), tb.CPU()->GetMemoryControllerConst(), &hint);
                TCHAR msg[40];
                _stprintf_s(msg, _T("opcode %06o"), (unsigned int) list[i]);
                Assert::AreEqual(kind == 0, hint.isJump, msg);
            }
        }
    }
    // The third line: the operand of a deferred addressing mode

    TEST_METHOD(Hint_ThirdLine_DeferredModes)
    {
        // Line 2 is the pointer, line 3 is the operand at the pointer. The word at 0300 is 0123456
        // (low byte 056, high byte 0247), the word at 0300 for the byte case is 0111111 (high byte 0222).
        static const HintCase3 cases[] =
        {
            { _T("CLR @(R1)+"),   { 0005031 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0300 }, { 0300, 0123456 } }, 0,
                3, _T("R1=000200"), _T("(R1)=000300"), _T("(000300)=123456") },
            { _T("CLR @-(R1)"),   { 0005051 }, { 0, 0202, 0, 0, 0, 0, STACK }, { { 0200, 0300 }, { 0300, 0123456 } }, 0,
                3, _T("R1=000202"), _T("(R1-2)=000300"), _T("(000300)=123456") },
            { _T("CLR @4(R1)"),   { 0005071, 4 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0204, 0300 }, { 0300, 0123456 } }, 0,
                3, _T("R1=000200"), _T("(R1+000004)=000300"), _T("(000300)=123456") },
            { _T("CLRB @(R1)+"),  { 0105031 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0301 }, { 0300, 0111111 } }, 0,
                3, _T("R1=000200"), _T("(R1)=000301"), _T("(000301)=222") },  // the pointer is odd: the high byte
            { _T("MOV @(R1)+,R2"), { 0013102 }, { 0, 0200, 7, 0, 0, 0, STACK }, { { 0200, 0300 }, { 0300, 0123456 } }, 0,
                3, _T("R1=000200, R2=000007"), _T("(R1)=000300"), _T("(000300)=123456") },
            { _T("ADC @(R1)+"),   { 0005531 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0300 }, { 0300, 0123456 } }, 0,
                3, _T("R1=000200, C=0"), _T("(R1)=000300"), _T("(000300)=123456") },
        };
        CheckHints3(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_ThirdLine_PcDeferred)
    {
        // @X(PC): the pointer is at the PC-relative address (01004 + 0177174 = 0200), it goes to line 1,
        // and the operand to line 2
        static const HintCase3 cases[] =
        {
            { _T("MOV @X(PC),R1"), { 0017701, 0177174 }, { 0, 5, 0, 0, 0, 0, STACK }, { { 0200, 0300 }, { 0300, 0123456 } }, 0,
                2, _T("(000200)=000300, R1=000005"), _T("(000300)=123456"), _T("") },
        };
        CheckHints3(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_ThirdLine_SourceAndDestination)
    {
        static const HintCase3 cases[] =
        {
            // Both operands are deferred: every line joins two pieces
            { _T("MOV @(R1)+,@(R2)+"), { 0013132 }, { 0, 0200, 0210, 0, 0, 0, STACK },
                { { 0200, 0300 }, { 0210, 0310 }, { 0300, 0111 }, { 0310, 0222 } }, 0,
                3, _T("R1=000200, R2=000210"), _T("(R1)=000300, (R2)=000310"), _T("(000300)=000111, (000310)=000222") },
            // Only the destination is deferred: the pieces of the lines 2 and 3 are aligned under the destination piece
            { _T("MOV R1,@(R2)+"), { 0010132 }, { 0, 7, 0210, 0, 0, 0, STACK }, { { 0210, 0310 }, { 0310, 0222 } }, 0,
                3, _T("R1=000007, R2=000210"), _T("           (R2)=000310"), _T("           (000310)=000222") },
        };
        CheckHints3(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_ThirdLine_IoAndUnknownPointers)
    {
        // "I/O" is not a value: when the pointer is an I/O port there is no third line;
        // when the operand is in the I/O range, the third line shows it as "I/O"
        static const HintCase3 cases[] =
        {
            { _T("CLR @(R1)+, the pointer is I/O"), { 0005031 }, { 0, 0177560, 0, 0, 0, 0, STACK }, NO_MEM, 0,
                2, _T("R1=177560"), _T("(R1)=I/O"), _T("") },
            { _T("CLR @(R1)+, the operand is I/O"), { 0005031 }, { 0, 0200, 0, 0, 0, 0, STACK }, { { 0200, 0177560 } }, 0,
                3, _T("R1=000200"), _T("(R1)=177560"), _T("(177560)=I/O") },
        };
        CheckHints3(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Hint_GetHint_JumpFlagAndPrediction)
    {
        // Disasm_GetHint gives the jump hint if the instruction has one, the instruction hint otherwise
        {
            TestBoard tb;
            const uint16_t words[3] = { 0001002, 0, 0 };  // BNE
            const uint16_t regs[7] = { 0, 0, 0, 0, 0, 0, STACK };
            Prepare(tb, words, 3, regs, NO_MEM_ARRAY, TF_Z, 0, 0);
            DisasmHint hint;
            Disasm_GetHint(words, tb.CPU(), tb.CPU()->GetMemoryControllerConst(), &hint);
            Assert::AreEqual(1, hint.count, _T("BNE: lines"));
            Assert::IsTrue(hint.isJump, _T("BNE: isJump"));
            Assert::IsFalse(hint.jumpPredict, _T("BNE with Z set: the jump is not taken"));
            Assert::AreEqual(_T("Z=1"), hint.lines[0], _T("BNE: text"));
        }
        {
            TestBoard tb;
            const uint16_t words[3] = { 0010102, 0, 0 };  // MOV R1,R2
            const uint16_t regs[7] = { 0, 1, 2, 0, 0, 0, STACK };
            Prepare(tb, words, 3, regs, NO_MEM_ARRAY, 0, 0, 0);
            DisasmHint hint;
            Disasm_GetHint(words, tb.CPU(), tb.CPU()->GetMemoryControllerConst(), &hint);
            Assert::AreEqual(1, hint.count, _T("MOV: lines"));
            Assert::IsFalse(hint.isJump, _T("MOV: isJump"));
            Assert::AreEqual(_T("R1=000001, R2=000002"), hint.lines[0], _T("MOV: text"));
            Assert::AreEqual(_T(""), hint.lines[1], _T("MOV: the unused line is empty"));
        }
        {
            TestBoard tb;
            const uint16_t words[3] = { 0000240, 0, 0 };  // NOP
            const uint16_t regs[7] = { 0, 0, 0, 0, 0, 0, STACK };
            Prepare(tb, words, 3, regs, NO_MEM_ARRAY, 0, 0, 0);
            DisasmHint hint;
            Disasm_GetHint(words, tb.CPU(), tb.CPU()->GetMemoryControllerConst(), &hint);
            Assert::AreEqual(0, hint.count, _T("NOP: no hint"));
        }
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
            // JSR and CALL have no jump hint: the instruction hint shows the link register and the destination (Hint_Jsr)
            { _T("JSR R5,(R1)"), { 0004511 }, { 0, 0200, 0, 0, 0, 010, STACK }, NO_MEM, 0, _T(""), true },
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

    // The former known bugs of the hints, fixed in Phase 3 of docs/hints-improvements.md (1.2, 1.3, 1.4, 1.5, 1.7),
    // and the one still open (1.6 the name of SP). The expected text is what the processor does.

    // 1.1 (fixed in Phase 5): MARK nn: SP = PC + 2*nn, where PC is the address after MARK; PC = R5; R5 = (SP)+.
    // The hint must show the new PC (R5) and the word that becomes R5, at PROG + 2 + 2 * 3 = 01010.
    TEST_METHOD(Hint_MarkShowsTheWordThatBecomesR5)
    {
        static const HintCase cases[] =
        {
            { _T("MARK 3"), { 0006403 }, { 0, 0, 0, 0, 0, 0300, STACK }, { { 01010, 05252 } }, 0, 0, 0,
                2, _T("R5=000300"), _T("(001010)=005252") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    // 1.2: the destination is evaluated after the autoincrement / autodecrement of the source
    TEST_METHOD(Hint_DestinationSeesTheAutoincrementOfTheSource)
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
    TEST_METHOD(Hint_PcRegisterWithoutAnExtraWord)
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
    TEST_METHOD(Hint_ByteDecrementOfSpSteps2)
    {
        static const HintCase cases[] =
        {
            { _T("CLRB -(SP)"), { 0105046 }, { 0, 0, 0, 0, 0, 0, 0402 }, { { 0400, 0123456 } }, 0, 0, 0,
                2, _T("SP=000402"), _T("(SP-2)=056") },
        };
        CheckHints(cases, sizeof(cases) / sizeof(cases[0]));
    }

    // 1.7: SUB has the bit 15 set, but it is a word operation: the operand is a word
    TEST_METHOD(Hint_SubIsAWordOperation)
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
    TEST_METHOD(Hint_IoPortIsNotShownAsAValue)
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

    TEST_METHOD(Hint_Jump_StackPointerIsNamedSp)
    {
        // The stack pointer is named SP, as in the operand hints and in the disassembler (JSR SP is in Hint_Jsr)
        static const JumpHintCase cases[] =
        {
            { _T("RTS SP"),      { 0000206 }, { 0, 0, 0, 0, 0, 0, 0400 }, { { 0400, 01234 } }, 0, _T("SP=000400, (SP)=001234"), true },
            { _T("SOB SP,"),     { 0077602 }, { 0, 0, 0, 0, 0, 0, 0400 }, NO_MEM, 0, _T("SP=000400"), true },
        };
        CheckJumpHints(cases, sizeof(cases) / sizeof(cases[0]));
    }
};
