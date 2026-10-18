/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// ProcessorTests.cpp : tests for the KM1801VM2 instruction execution (CPU, USER mode)
//
// Expected values come from the PDP-11 / KM1801VM2 instruction descriptions, not from the
// emulator output. Cases marked "characterization" only pin the current behavior.
// Programs are placed at PROG, data at DATA; the stack is at STACK.

#include "stdafx.h"
#include <CppUnitTest.h>
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace
{

const uint16_t PROG = 01000;
const uint16_t DATA = 0200;
const uint16_t STACK = 0400;

// Two-operand instruction on registers: opcode uses R0 as the source and R1 as the destination
struct BinaryCase
{
    uint16_t opcode;
    uint16_t src;       // R0
    uint16_t dst;       // R1
    uint16_t flagsIn;
    uint16_t dstOut;    // R1 after the instruction
    uint16_t flagsOut;
};

void CheckBinaryCases(const BinaryCase* cases, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        const BinaryCase& c = cases[i];
        TestBoard tb;
        uint16_t regs[8] = { c.src, c.dst, 0, 0, 0, 0, STACK, 0 };
        tb.SetRegs(regs);
        tb.SetFlags(c.flagsIn);
        tb.LoadProgram(PROG, &c.opcode, 1);

        tb.Run();

        TCHAR fin[8], fexp[8], msg[100];
        FlagsText(c.flagsIn, fin);
        FlagsText(c.flagsOut, fexp);
        _stprintf_s(msg, _T("opcode %06o src %06o dst %06o flags %s, expected flags %s"), (unsigned int) c.opcode, (unsigned int) c.src, (unsigned int) c.dst, fin, fexp);
        Eq(c.dstOut, tb.CPU()->GetReg(1), msg);
        Eq(c.src, tb.CPU()->GetReg(0), msg);
        Eq(c.flagsOut, tb.GetFlags(), msg);
        Eq(PROG + 2, tb.CPU()->GetPC(), msg);
    }
}

// Single-operand instruction on R0
struct UnaryCase
{
    uint16_t opcode;
    uint16_t in;        // R0
    uint16_t flagsIn;
    uint16_t out;       // R0 after the instruction
    uint16_t flagsOut;
};

void CheckUnaryCases(const UnaryCase* cases, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        const UnaryCase& c = cases[i];
        TestBoard tb;
        uint16_t regs[8] = { c.in, 0, 0, 0, 0, 0, STACK, 0 };
        tb.SetRegs(regs);
        tb.SetFlags(c.flagsIn);
        tb.LoadProgram(PROG, &c.opcode, 1);

        tb.Run();

        TCHAR fin[8], fexp[8], msg[100];
        FlagsText(c.flagsIn, fin);
        FlagsText(c.flagsOut, fexp);
        _stprintf_s(msg, _T("opcode %06o in %06o flags %s, expected flags %s"), (unsigned int) c.opcode, (unsigned int) c.in, fin, fexp);
        Eq(c.out, tb.CPU()->GetReg(0), msg);
        Eq(c.flagsOut, tb.GetFlags(), msg);
        Eq(PROG + 2, tb.CPU()->GetPC(), msg);
    }
}

}

TEST_CLASS(ProcessorTests)
{
public:
    // Harness checks

    TEST_METHOD(Processor_HarnessMemory)
    {
        TestBoard tb;
        tb.SetWord(0200, 0x1234);
        tb.SetByte(0203, 0xAB);
        Eq(0xAB00, tb.GetWord(0202), _T("odd byte is the high byte"));
        Eq(0x1234, tb.GetWord(0200), _T("word"));
        Eq(0x34, tb.GetByte(0200), _T("low byte"));
        Eq(0x12, tb.GetByte(0201), _T("high byte"));
        Eq(0xAB, tb.GetByte(0203), _T("byte at the odd address"));
    }

    TEST_METHOD(Processor_HarnessRunsProgram)
    {
        TestBoard tb;
        const uint16_t program[] = { 0005200 };  // INC R0
        tb.LoadProgram(PROG, program, 1);
        tb.Run();
        Eq(1, tb.CPU()->GetReg(0), _T("R0"));
        Eq(PROG + 2, tb.CPU()->GetPC(), _T("PC"));
    }

    // Two-operand instructions

    TEST_METHOD(Processor_MOV)
    {
        static const BinaryCase cases[] =
        {
            { 0010001, 0x1234, 0xFFFF, 0,           0x1234, 0 },
            { 0010001, 0x0000, 0x1234, 0,           0x0000, TF_Z },
            { 0010001, 0x8000, 0x0000, 0,           0x8000, TF_N },
            { 0010001, 0x1234, 0x0000, TF_C,        0x1234, TF_C },          // C is not affected
            { 0010001, 0x1234, 0x0000, TF_V,        0x1234, 0 },             // V is cleared
            { 0010001, 0x0000, 0x0000, TF_N | TF_V, 0x0000, TF_Z },
        };
        CheckBinaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_ADD)
    {
        static const BinaryCase cases[] =
        {
            { 0060001, 0000001, 0000001, 0, 0000002, 0 },
            { 0060001, 0000001, 0077777, 0, 0100000, TF_N | TF_V },
            { 0060001, 0000001, 0177777, 0, 0000000, TF_Z | TF_C },
            { 0060001, 0100000, 0100000, 0, 0000000, TF_Z | TF_V | TF_C },
            { 0060001, 0000000, 0000000, 0, 0000000, TF_Z },
            { 0060001, 0177777, 0177777, 0, 0177776, TF_N | TF_C },
            { 0060001, 0000002, 0000003, TF_C | TF_V | TF_N | TF_Z, 0000005, 0 },  // all flags are recalculated
        };
        CheckBinaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_SUB)
    {
        // dst = dst - src; C is set on borrow
        static const BinaryCase cases[] =
        {
            { 0160001, 0000003, 0000005, 0, 0000002, 0 },
            { 0160001, 0000005, 0000003, 0, 0177776, TF_N | TF_C },
            { 0160001, 0000001, 0100000, 0, 0077777, TF_V },
            { 0160001, 0177777, 0077777, 0, 0100000, TF_N | TF_V | TF_C },
            { 0160001, 0000007, 0000007, 0, 0000000, TF_Z },
            { 0160001, 0100000, 0000000, 0, 0100000, TF_N | TF_V | TF_C },
        };
        CheckBinaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_CMP)
    {
        // flags of src - dst, nothing is stored
        static const BinaryCase cases[] =
        {
            { 0020001, 0000005, 0000003, 0, 0000003, 0 },
            { 0020001, 0000003, 0000005, 0, 0000005, TF_N | TF_C },
            { 0020001, 0000007, 0000007, 0, 0000007, TF_Z },
            { 0020001, 0100000, 0000001, 0, 0000001, TF_V },
            { 0020001, 0077777, 0177777, 0, 0177777, TF_N | TF_V | TF_C },
        };
        CheckBinaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_BIT)
    {
        static const BinaryCase cases[] =
        {
            { 0030001, 0170360, 0007417, 0,    0007417, TF_Z },
            { 0030001, 0100000, 0100001, 0,    0100001, TF_N },
            { 0030001, 0000001, 0000003, TF_C, 0000003, TF_C },  // C is not affected
            { 0030001, 0000001, 0000003, TF_V, 0000003, 0 },     // V is cleared
        };
        CheckBinaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_BIC)
    {
        static const BinaryCase cases[] =
        {
            { 0040001, 0xFF00, 0x1234, 0,    0x0034, 0 },
            { 0040001, 0xFFFF, 0x1234, 0,    0x0000, TF_Z },
            { 0040001, 0x0001, 0x8001, TF_C, 0x8000, TF_N | TF_C },
            { 0040001, 0x0000, 0x1234, TF_V, 0x1234, 0 },
        };
        CheckBinaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_BIS)
    {
        static const BinaryCase cases[] =
        {
            { 0050001, 0x00FF, 0x1200, 0,    0x12FF, 0 },
            { 0050001, 0x0000, 0x0000, 0,    0x0000, TF_Z },
            { 0050001, 0x8000, 0x0001, TF_C, 0x8001, TF_N | TF_C },
            { 0050001, 0x0001, 0x0000, TF_V, 0x0001, 0 },
        };
        CheckBinaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_XOR)
    {
        // XOR R0,R1: R1 = R1 ^ R0
        static const BinaryCase cases[] =
        {
            { 0074001, 0x00FF, 0x0FF0, 0,    0x0F0F, 0 },
            { 0074001, 0x1234, 0x1234, 0,    0x0000, TF_Z },
            { 0074001, 0x8000, 0x0001, TF_C, 0x8001, TF_N | TF_C },
            { 0074001, 0x0001, 0x0000, TF_V, 0x0001, 0 },
        };
        CheckBinaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    // Single-operand instructions

    TEST_METHOD(Processor_CLR)
    {
        static const UnaryCase cases[] =
        {
            { 0005000, 0x1234, 0,      0, TF_Z },
            { 0005000, 0xFFFF, TF_ALL, 0, TF_Z },
        };
        CheckUnaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_COM)
    {
        static const UnaryCase cases[] =
        {
            { 0005100, 0x0000, 0,    0xFFFF, TF_N | TF_C },
            { 0005100, 0xFFFF, 0,    0x0000, TF_Z | TF_C },
            { 0005100, 0x00FF, TF_V, 0xFF00, TF_N | TF_C },
        };
        CheckUnaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_INC)
    {
        // C is not affected
        static const UnaryCase cases[] =
        {
            { 0005200, 0x0000, 0,    0x0001, 0 },
            { 0005200, 0x0000, TF_C, 0x0001, TF_C },
            { 0005200, 0x7FFF, 0,    0x8000, TF_N | TF_V },
            { 0005200, 0xFFFF, 0,    0x0000, TF_Z },
            { 0005200, 0xFFFF, TF_C, 0x0000, TF_Z | TF_C },
        };
        CheckUnaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_DEC)
    {
        // C is not affected
        static const UnaryCase cases[] =
        {
            { 0005300, 0x0001, 0,    0x0000, TF_Z },
            { 0005300, 0x0000, 0,    0xFFFF, TF_N },
            { 0005300, 0x0000, TF_C, 0xFFFF, TF_N | TF_C },
            { 0005300, 0x8000, 0,    0x7FFF, TF_V },
            { 0005300, 0x0005, 0,    0x0004, 0 },
        };
        CheckUnaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_NEG)
    {
        static const UnaryCase cases[] =
        {
            { 0005400, 0x0001, 0, 0xFFFF, TF_N | TF_C },
            { 0005400, 0x0000, 0, 0x0000, TF_Z },
            { 0005400, 0x8000, 0, 0x8000, TF_N | TF_V | TF_C },
            { 0005400, 0x0005, 0, 0xFFFB, TF_N | TF_C },
            { 0005400, 0xFFFF, 0, 0x0001, TF_C },
        };
        CheckUnaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_ADC)
    {
        static const UnaryCase cases[] =
        {
            { 0005500, 0x0000, TF_C, 0x0001, 0 },
            { 0005500, 0x7FFF, TF_C, 0x8000, TF_N | TF_V },
            { 0005500, 0xFFFF, TF_C, 0x0000, TF_Z | TF_C },
            { 0005500, 0xFFFF, 0,    0xFFFF, TF_N },
            { 0005500, 0x0005, 0,    0x0005, 0 },
        };
        CheckUnaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_SBC)
    {
        static const UnaryCase cases[] =
        {
            { 0005600, 0x0001, TF_C, 0x0000, TF_Z },
            { 0005600, 0x0000, TF_C, 0xFFFF, TF_N | TF_C },
            { 0005600, 0x8000, TF_C, 0x7FFF, TF_V },
            { 0005600, 0x0005, 0,    0x0005, 0 },
            { 0005600, 0x0000, 0,    0x0000, TF_Z },
        };
        CheckUnaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_TST)
    {
        static const UnaryCase cases[] =
        {
            { 0005700, 0x0000, 0,             0x0000, TF_Z },
            { 0005700, 0x8000, 0,             0x8000, TF_N },
            { 0005700, 0x1234, TF_V | TF_C,   0x1234, 0 },  // V and C are cleared
        };
        CheckUnaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_ROR)
    {
        // V = N xor C after the shift
        static const UnaryCase cases[] =
        {
            { 0006000, 0x0001, 0,    0x0000, TF_Z | TF_C | TF_V },
            { 0006000, 0x0000, TF_C, 0x8000, TF_N | TF_V },
            { 0006000, 0x0002, 0,    0x0001, 0 },
            { 0006000, 0x0003, TF_C, 0x8001, TF_N | TF_C },
        };
        CheckUnaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_ROL)
    {
        static const UnaryCase cases[] =
        {
            { 0006100, 0x8000, 0,    0x0000, TF_Z | TF_C | TF_V },
            { 0006100, 0x0000, TF_C, 0x0001, 0 },
            { 0006100, 0x4000, 0,    0x8000, TF_N | TF_V },
            { 0006100, 0x8000, TF_C, 0x0001, TF_C | TF_V },
        };
        CheckUnaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_ASR)
    {
        // the sign bit is kept
        static const UnaryCase cases[] =
        {
            { 0006200, 0x0001, 0, 0x0000, TF_Z | TF_C | TF_V },
            { 0006200, 0x8000, 0, 0xC000, TF_N | TF_V },
            { 0006200, 0x8001, 0, 0xC000, TF_N | TF_C },
            { 0006200, 0x0004, 0, 0x0002, 0 },
        };
        CheckUnaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_ASL)
    {
        static const UnaryCase cases[] =
        {
            { 0006300, 0x8000, 0, 0x0000, TF_Z | TF_C | TF_V },
            { 0006300, 0x4000, 0, 0x8000, TF_N | TF_V },
            { 0006300, 0x0001, 0, 0x0002, 0 },
            { 0006300, 0xC000, 0, 0x8000, TF_N | TF_C },
        };
        CheckUnaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_SWAB)
    {
        // N and Z are calculated from the low byte of the result
        static const UnaryCase cases[] =
        {
            { 0000300, 0x1234, 0,            0x3412, 0 },
            { 0000300, 0x0080, 0,            0x8000, TF_Z },
            { 0000300, 0x8000, 0,            0x0080, TF_N },
            { 0000300, 0x1234, TF_V | TF_C,  0x3412, 0 },
        };
        CheckUnaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_SXT)
    {
        // R0 = N ? 177777 : 0; Z = !N; V = 0; C is not affected
        static const UnaryCase cases[] =
        {
            { 0006700, 0x1234, TF_N,         0xFFFF, TF_N },
            { 0006700, 0x1234, 0,            0x0000, TF_Z },
            { 0006700, 0x1234, TF_C,         0x0000, TF_Z | TF_C },
            { 0006700, 0x1234, TF_N | TF_V,  0xFFFF, TF_N },
        };
        CheckUnaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    // Byte instructions on registers: only the low byte is affected

    TEST_METHOD(Processor_ByteInstructionsOnRegisters)
    {
        static const UnaryCase cases[] =
        {
            { 0105000, 0x1234, 0, 0x1200, TF_Z },                    // CLRB
            { 0105100, 0x1200, 0, 0x12FF, TF_N | TF_C },            // COMB
            { 0105200, 0x12FF, 0, 0x1200, TF_Z },                    // INCB
            { 0105200, 0x127F, 0, 0x1280, TF_N | TF_V },
            { 0105300, 0x1280, 0, 0x127F, TF_V },                    // DECB
            { 0105300, 0x1200, 0, 0x12FF, TF_N },
            { 0105400, 0x1201, 0, 0x12FF, TF_N | TF_C },            // NEGB
            { 0105700, 0x1280, 0, 0x1280, TF_N },                    // TSTB
            { 0105700, 0x1200, 0, 0x1200, TF_Z },
            { 0106300, 0x1280, 0, 0x1200, TF_Z | TF_C | TF_V },      // ASLB
            { 0106200, 0x1281, 0, 0x12C0, TF_N | TF_C },            // ASRB
            { 0106000, 0x1201, 0, 0x1200, TF_Z | TF_C | TF_V },      // RORB
            { 0106100, 0x1280, 0, 0x1200, TF_Z | TF_C | TF_V },      // ROLB
        };
        CheckUnaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_MOVB_ToRegisterExtendsSign)
    {
        static const BinaryCase cases[] =
        {
            { 0110001, 0x0080, 0x0000, 0, 0xFF80, TF_N },
            { 0110001, 0x1234, 0xFFFF, 0, 0x0034, 0 },
            { 0110001, 0x1200, 0xFFFF, 0, 0x0000, TF_Z },
        };
        CheckBinaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Processor_CMPB_BITB_BICB_BISB)
    {
        static const BinaryCase cases[] =
        {
            { 0120001, 0x1205, 0x9903, 0, 0x9903, 0 },                    // CMPB 5 - 3
            { 0120001, 0x1203, 0x9905, 0, 0x9905, TF_N | TF_C },
            { 0120001, 0x1280, 0x9901, 0, 0x9901, TF_V },                 // 0200 - 1 overflows
            { 0130001, 0x1280, 0x9980, 0, 0x9980, TF_N },                 // BITB
            { 0130001, 0x1201, 0x9902, 0, 0x9902, TF_Z },
            { 0140001, 0x120F, 0x99FF, 0, 0x99F0, TF_N },                 // BICB: the high byte stays
            { 0150001, 0x120F, 0x99F0, 0, 0x99FF, TF_N },                 // BISB
        };
        CheckBinaryCases(cases, sizeof(cases) / sizeof(cases[0]));
    }
};
