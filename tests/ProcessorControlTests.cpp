/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// ProcessorControlTests.cpp : addressing modes, branches, jumps, subroutines, traps and
// the arithmetic extensions of the KM1801VM2 (CPU, USER mode)
//
// Expected values come from the PDP-11 / KM1801VM2 instruction descriptions, not from the
// emulator output. Programs are placed at PROG, data at DATA; the stack is at STACK.

#include "stdafx.h"
#include <CppUnitTest.h>
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace
{

const uint16_t PROG = 01000;
const uint16_t DATA = 0200;
const uint16_t STACK = 0400;

// A board with the program at PROG, the registers R0..R5 zero and SP = STACK
void Prepare(TestBoard& tb, const uint16_t* program, size_t count)
{
    uint16_t regs[8] = { 0, 0, 0, 0, 0, 0, STACK, 0 };
    tb.SetRegs(regs);
    tb.LoadProgram(PROG, program, count);
}

// One MOV with the source addressed in the given mode
struct SourceModeCase
{
    const TCHAR* name;
    uint16_t program[2];
    uint16_t words;         // length of the instruction in words
    uint16_t r1;            // initial R1
    uint16_t r1Out;         // R1 after the instruction
    uint16_t expected;      // R0 after the instruction
};

}

TEST_CLASS(ProcessorControlTests)
{
public:
    // Addressing modes

    TEST_METHOD(Processor_AddressingModes_Source)
    {
        // Memory: 0200 = 0x1111, 0202 = 0x2222, 0210 = 0x3333, 0300 = 0x4444, 0302 = 0x5555
        static const SourceModeCase cases[] =
        {
            { _T("(R1)"),     { 0011100 },         1, 0200, 0200, 0x1111 },
            { _T("(R1)+"),    { 0012100 },         1, 0200, 0202, 0x1111 },
            { _T("@(R1)+"),   { 0013100 },         1, 0204, 0206, 0x4444 },  // 0204 holds 0300
            { _T("-(R1)"),    { 0014100 },         1, 0204, 0202, 0x2222 },
            { _T("@-(R1)"),   { 0015100 },         1, 0206, 0204, 0x4444 },
            { _T("10(R1)"),   { 0016100, 000010 }, 2, 0200, 0200, 0x3333 },
            { _T("-10(R1)"),  { 0016100, 0177770 }, 2, 0220, 0220, 0x3333 },  // 0220 - 10 = 0210
            { _T("@12(R1)"),  { 0017100, 000012 }, 2, 0200, 0200, 0x4444 },  // 0212 holds 0300
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        {
            const SourceModeCase& c = cases[i];
            TestBoard tb;
            Prepare(tb, c.program, c.words);
            tb.SetWord(0200, 0x1111);
            tb.SetWord(0202, 0x2222);
            tb.SetWord(0204, 0300);
            tb.SetWord(0210, 0x3333);
            tb.SetWord(0212, 0300);
            tb.SetWord(0300, 0x4444);
            tb.SetWord(0302, 0x5555);
            tb.CPU()->SetReg(1, c.r1);

            tb.Run();

            TCHAR msg[60];
            _stprintf_s(msg, _T("MOV %s,R0"), c.name);
            Eq(c.expected, tb.CPU()->GetReg(0), msg);
            Eq(c.r1Out, tb.CPU()->GetReg(1), msg);
            Eq(PROG + c.words * 2, tb.CPU()->GetPC(), msg);
        }
    }

    TEST_METHOD(Processor_AddressingModes_PC)
    {
        TestBoard tb;
        // MOV #1234,R0
        const uint16_t immediate[] = { 0012700, 01234 };
        Prepare(tb, immediate, 2);
        tb.Run();
        Eq(01234, tb.CPU()->GetReg(0), _T("MOV #n,R0"));
        Eq(PROG + 4, tb.CPU()->GetPC(), _T("PC after MOV #n,R0"));

        // MOV @#DATA,R0
        TestBoard tb2;
        const uint16_t absolute[] = { 0013700, DATA };
        Prepare(tb2, absolute, 2);
        tb2.SetWord(DATA, 0x5A5A);
        tb2.Run();
        Eq(0x5A5A, tb2.CPU()->GetReg(0), _T("MOV @#addr,R0"));
        Eq(PROG + 4, tb2.CPU()->GetPC(), _T("PC after MOV @#addr,R0"));

        // MOV rel,R0: the offset is counted from the PC after the offset word
        TestBoard tb3;
        const uint16_t relative[] = { 0016700, (uint16_t) (DATA - (PROG + 4)) };
        Prepare(tb3, relative, 2);
        tb3.SetWord(DATA, 0x6B6B);
        tb3.Run();
        Eq(0x6B6B, tb3.CPU()->GetReg(0), _T("MOV rel,R0"));
        Eq(PROG + 4, tb3.CPU()->GetPC(), _T("PC after MOV rel,R0"));

        // MOV @rel,R0
        TestBoard tb4;
        const uint16_t relativeDeferred[] = { 0017700, (uint16_t) (DATA - (PROG + 4)) };
        Prepare(tb4, relativeDeferred, 2);
        tb4.SetWord(DATA, 0300);
        tb4.SetWord(0300, 0x7C7C);
        tb4.Run();
        Eq(0x7C7C, tb4.CPU()->GetReg(0), _T("MOV @rel,R0"));
    }

    TEST_METHOD(Processor_AddressingModes_Destination)
    {
        // MOV R0,dst with R0 = 0xABCD
        struct DestinationCase
        {
            const TCHAR* name;
            uint16_t program[2];
            uint16_t words;
            uint16_t r1;
            uint16_t r1Out;
            uint16_t address;   // where the word has to appear
        };
        static const DestinationCase cases[] =
        {
            { _T("(R1)"),     { 0010011 },         1, 0220, 0220, 0220 },
            { _T("(R1)+"),    { 0010021 },         1, 0220, 0222, 0220 },
            { _T("@(R1)+"),   { 0010031 },         1, 0230, 0232, 0300 },  // 0230 holds 0300
            { _T("-(R1)"),    { 0010041 },         1, 0222, 0220, 0220 },
            { _T("@-(R1)"),   { 0010051 },         1, 0232, 0230, 0300 },
            { _T("10(R1)"),   { 0010061, 000010 }, 2, 0220, 0220, 0230 },
            { _T("@10(R1)"),  { 0010071, 000010 }, 2, 0220, 0220, 0300 },  // 0230 holds 0300
            { _T("@#addr"),   { 0010037, 0240 },   2, 0220, 0220, 0240 },
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        {
            const DestinationCase& c = cases[i];
            TestBoard tb;
            Prepare(tb, c.program, c.words);
            tb.SetWord(0230, 0300);
            tb.CPU()->SetReg(0, 0xABCD);
            tb.CPU()->SetReg(1, c.r1);

            tb.Run();

            TCHAR msg[60];
            _stprintf_s(msg, _T("MOV R0,%s"), c.name);
            Eq(0xABCD, tb.GetWord(c.address), msg);
            Eq(c.r1Out, tb.CPU()->GetReg(1), msg);
            Eq(PROG + c.words * 2, tb.CPU()->GetPC(), msg);
        }
    }

    TEST_METHOD(Processor_AddressingModes_ByteIncrements)
    {
        // The byte instructions step the register by 1, except SP and PC which step by 2
        struct StepCase
        {
            const TCHAR* name;
            uint16_t opcode;
            uint16_t reg;       // register number
            uint16_t before;
            uint16_t after;
        };
        static const StepCase cases[] =
        {
            { _T("MOVB (R1)+,R0"), 0112100, 1, 0200, 0201 },
            { _T("MOVB (R2)+,R0"), 0112200, 2, 0201, 0202 },
            { _T("MOVB (SP)+,R0"), 0112600, 6, 0200, 0202 },
            { _T("MOVB -(R1),R0"), 0114100, 1, 0201, 0200 },
            { _T("MOVB -(SP),R0"), 0114600, 6, 0202, 0200 },
            { _T("MOV (R1)+,R0"),  0012100, 1, 0200, 0202 },
            { _T("MOV -(R1),R0"),  0014100, 1, 0202, 0200 },
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        {
            const StepCase& c = cases[i];
            TestBoard tb;
            Prepare(tb, &c.opcode, 1);
            tb.CPU()->SetReg(c.reg, c.before);

            tb.Run();

            Eq(c.after, tb.CPU()->GetReg(c.reg), c.name);
        }
    }

    TEST_METHOD(Processor_MOVB_ReadsHighByteAtOddAddress)
    {
        TestBoard tb;
        const uint16_t program[] = { 0112100 };  // MOVB (R1)+,R0
        Prepare(tb, program, 1);
        tb.SetWord(0200, 0x7F12);
        tb.CPU()->SetReg(1, 0201);
        tb.Run();
        Eq(0x007F, tb.CPU()->GetReg(0), _T("high byte"));
        Eq(0202, tb.CPU()->GetReg(1), _T("R1"));
    }

    TEST_METHOD(Processor_MOVB_ToMemoryKeepsTheOtherByte)
    {
        {
            TestBoard tb;
            const uint16_t program[] = { 0110011 };  // MOVB R0,(R1)
            Prepare(tb, program, 1);
            tb.SetWord(0200, 0x1234);
            tb.CPU()->SetReg(0, 0x00AB);
            tb.CPU()->SetReg(1, 0200);
            tb.Run();
            Eq(0x12AB, tb.GetWord(0200), _T("even address: low byte"));
        }
        {
            TestBoard tb;
            const uint16_t program[] = { 0110011 };  // MOVB R0,(R1)
            Prepare(tb, program, 1);
            tb.SetWord(0200, 0x1234);
            tb.CPU()->SetReg(0, 0x00AB);
            tb.CPU()->SetReg(1, 0201);
            tb.Run();
            Eq(0xAB34, tb.GetWord(0200), _T("odd address: high byte"));
        }
    }

    TEST_METHOD(Processor_PushAndPop)
    {
        TestBoard tb;
        const uint16_t program[] = { 0010046, 0012601 };  // MOV R0,-(SP); MOV (SP)+,R1
        Prepare(tb, program, 2);
        tb.CPU()->SetReg(0, 0x1357);

        tb.Run();
        Eq(STACK - 2, tb.CPU()->GetSP(), _T("SP after push"));
        Eq(0x1357, tb.GetWord(STACK - 2), _T("pushed word"));

        tb.Run();
        Eq(STACK, tb.CPU()->GetSP(), _T("SP after pop"));
        Eq(0x1357, tb.CPU()->GetReg(1), _T("popped word"));
    }

    // Condition codes

    TEST_METHOD(Processor_ConditionCodeInstructions)
    {
        // 0240..0277: bit 4 = set, bits 3..0 select N, Z, V, C; 0240 and 0260 select nothing
        for (uint16_t opcode = 0240; opcode <= 0277; opcode++)
        {
            for (uint16_t flags = 0; flags <= TF_ALL; flags++)
            {
                TestBoard tb;
                Prepare(tb, &opcode, 1);
                tb.SetFlags(flags);

                tb.Run();

                uint16_t mask = opcode & 017;
                uint16_t expected = (opcode & 020) ? (flags | mask) : (flags & ~mask);
                TCHAR fin[8], fexp[8], msg[80];
                FlagsText(flags, fin);
                FlagsText(expected, fexp);
                _stprintf_s(msg, _T("opcode %06o flags %s expected %s"), (unsigned int) opcode, fin, fexp);
                Eq(expected, tb.GetFlags(), msg);
                Eq(PROG + 2, tb.CPU()->GetPC(), msg);
            }
        }
    }

    // Branches

    TEST_METHOD(Processor_ConditionalBranches)
    {
        struct BranchDef
        {
            const TCHAR* name;
            uint16_t opcode;
            bool (*condition)(bool n, bool z, bool v, bool c);
        };
        static const BranchDef branches[] =
        {
            { _T("BR"),   0000400, [](bool, bool, bool, bool) { return true; } },
            { _T("BNE"),  0001000, [](bool, bool z, bool, bool) { return !z; } },
            { _T("BEQ"),  0001400, [](bool, bool z, bool, bool) { return z; } },
            { _T("BGE"),  0002000, [](bool n, bool, bool v, bool) { return n == v; } },
            { _T("BLT"),  0002400, [](bool n, bool, bool v, bool) { return n != v; } },
            { _T("BGT"),  0003000, [](bool n, bool z, bool v, bool) { return !z && n == v; } },
            { _T("BLE"),  0003400, [](bool n, bool z, bool v, bool) { return z || n != v; } },
            { _T("BPL"),  0100000, [](bool n, bool, bool, bool) { return !n; } },
            { _T("BMI"),  0100400, [](bool n, bool, bool, bool) { return n; } },
            { _T("BHI"),  0101000, [](bool, bool z, bool, bool c) { return !c && !z; } },
            { _T("BLOS"), 0101400, [](bool, bool z, bool, bool c) { return c || z; } },
            { _T("BVC"),  0102000, [](bool, bool, bool v, bool) { return !v; } },
            { _T("BVS"),  0102400, [](bool, bool, bool v, bool) { return v; } },
            { _T("BCC"),  0103000, [](bool, bool, bool, bool c) { return !c; } },
            { _T("BCS"),  0103400, [](bool, bool, bool, bool c) { return c; } },
        };
        for (size_t b = 0; b < sizeof(branches) / sizeof(branches[0]); b++)
        {
            const BranchDef& def = branches[b];
            for (uint16_t flags = 0; flags <= TF_ALL; flags++)
            {
                TestBoard tb;
                uint16_t program = def.opcode | 3;  // offset +3 words
                Prepare(tb, &program, 1);
                tb.SetFlags(flags);

                tb.Run();

                bool taken = def.condition((flags & TF_N) != 0, (flags & TF_Z) != 0, (flags & TF_V) != 0, (flags & TF_C) != 0);
                TCHAR fin[8], msg[60];
                FlagsText(flags, fin);
                _stprintf_s(msg, _T("%s with flags %s, %s expected"), def.name, fin, taken ? _T("taken") : _T("not taken"));
                Eq(taken ? PROG + 2 + 6 : PROG + 2, tb.CPU()->GetPC(), msg);
                Eq(flags, tb.GetFlags(), msg);
            }
        }
    }

    TEST_METHOD(Processor_BranchBackwards)
    {
        TestBoard tb;
        const uint16_t program[] = { 0000775 };  // BR -3
        Prepare(tb, program, 1);
        tb.Run();
        Eq(PROG + 2 - 6, tb.CPU()->GetPC(), _T("BR -3"));
    }

    TEST_METHOD(Processor_BranchOffsetExtremes)
    {
        {
            TestBoard tb;
            const uint16_t program[] = { 0000577 };  // BR +127
            Prepare(tb, program, 1);
            tb.Run();
            Eq(PROG + 2 + 254, tb.CPU()->GetPC(), _T("BR +127"));
        }
        {
            TestBoard tb;
            const uint16_t program[] = { 0000600 };  // BR -128
            Prepare(tb, program, 1);
            tb.Run();
            Eq(PROG + 2 - 256, tb.CPU()->GetPC(), _T("BR -128"));
        }
    }

    TEST_METHOD(Processor_NOP)
    {
        TestBoard tb;
        const uint16_t program[] = { 0000240 };
        Prepare(tb, program, 1);
        tb.SetFlags(TF_N | TF_C);
        tb.Run();
        Eq(PROG + 2, tb.CPU()->GetPC(), _T("PC"));
        Eq(TF_N | TF_C, tb.GetFlags(), _T("flags"));
    }

    TEST_METHOD(Processor_SOB)
    {
        // R0 = 3: SOB R0,1 jumps to itself while R0 != 0
        TestBoard tb;
        const uint16_t program[] = { 0077001 };
        Prepare(tb, program, 1);
        tb.CPU()->SetReg(0, 3);
        tb.SetFlags(TF_N | TF_V);

        tb.Run();
        Eq(2, tb.CPU()->GetReg(0), _T("R0 after 1st"));
        Eq(PROG, tb.CPU()->GetPC(), _T("PC after 1st"));
        tb.Run();
        Eq(1, tb.CPU()->GetReg(0), _T("R0 after 2nd"));
        Eq(PROG, tb.CPU()->GetPC(), _T("PC after 2nd"));
        tb.Run();
        Eq(0, tb.CPU()->GetReg(0), _T("R0 after 3rd"));
        Eq(PROG + 2, tb.CPU()->GetPC(), _T("PC after 3rd: falls through"));
        Eq(TF_N | TF_V, tb.GetFlags(), _T("flags are not affected"));
    }

    TEST_METHOD(Processor_SOB_JumpsBackByNWords)
    {
        TestBoard tb;
        const uint16_t program[] = { 0077003 };  // SOB R0,3
        Prepare(tb, program, 1);
        tb.CPU()->SetReg(0, 2);
        tb.Run();
        Eq(1, tb.CPU()->GetReg(0), _T("R0"));
        Eq(PROG + 2 - 6, tb.CPU()->GetPC(), _T("PC"));
    }

    // Jumps and subroutines

    TEST_METHOD(Processor_JMP)
    {
        {
            TestBoard tb;
            const uint16_t program[] = { 0000111 };  // JMP (R1)
            Prepare(tb, program, 1);
            tb.CPU()->SetReg(1, 02000);
            tb.Run();
            Eq(02000, tb.CPU()->GetPC(), _T("JMP (R1)"));
        }
        {
            TestBoard tb;
            const uint16_t program[] = { 0000137, 03000 };  // JMP @#3000
            Prepare(tb, program, 2);
            tb.Run();
            Eq(03000, tb.CPU()->GetPC(), _T("JMP @#addr"));
        }
        {
            TestBoard tb;
            const uint16_t program[] = { 0000167, 000100 };  // JMP rel
            Prepare(tb, program, 2);
            tb.Run();
            Eq(PROG + 4 + 0100, tb.CPU()->GetPC(), _T("JMP rel"));
        }
    }

    TEST_METHOD(Processor_JSR)
    {
        TestBoard tb;
        const uint16_t program[] = { 0004511 };  // JSR R5,(R1)
        Prepare(tb, program, 1);
        tb.CPU()->SetReg(5, 0x5555);
        tb.CPU()->SetReg(1, 02000);

        tb.Run();

        Eq(02000, tb.CPU()->GetPC(), _T("PC = target"));
        Eq(PROG + 2, tb.CPU()->GetReg(5), _T("R5 = return address"));
        Eq(STACK - 2, tb.CPU()->GetSP(), _T("SP"));
        Eq(0x5555, tb.GetWord(STACK - 2), _T("old R5 is pushed"));
    }

    TEST_METHOD(Processor_CallAndReturn)
    {
        TestBoard tb;
        const uint16_t program[] = { 0004737, 03000 };  // CALL @#3000
        Prepare(tb, program, 2);
        const uint16_t sub[] = { 0000207 };             // RETURN
        tb.SetWord(03000, sub[0]);

        tb.Run();
        Eq(03000, tb.CPU()->GetPC(), _T("PC = target"));
        Eq(STACK - 2, tb.CPU()->GetSP(), _T("SP after CALL"));
        Eq(PROG + 4, tb.GetWord(STACK - 2), _T("return address is pushed"));

        tb.Run();
        Eq(PROG + 4, tb.CPU()->GetPC(), _T("PC after RETURN"));
        Eq(STACK, tb.CPU()->GetSP(), _T("SP after RETURN"));
    }

    TEST_METHOD(Processor_RTS)
    {
        TestBoard tb;
        const uint16_t program[] = { 0000205 };  // RTS R5
        Prepare(tb, program, 1);
        tb.CPU()->SetReg(5, 02000);
        tb.CPU()->SetSP(STACK - 2);
        tb.SetWord(STACK - 2, 0x4321);

        tb.Run();

        Eq(02000, tb.CPU()->GetPC(), _T("PC = R5"));
        Eq(0x4321, tb.CPU()->GetReg(5), _T("R5 is popped"));
        Eq(STACK, tb.CPU()->GetSP(), _T("SP"));
    }

    // Traps and interrupts

    TEST_METHOD(Processor_TrapInstructions)
    {
        struct TrapCase
        {
            const TCHAR* name;
            uint16_t opcode;
            uint16_t vector;
        };
        static const TrapCase cases[] =
        {
            { _T("BPT"),  0000003, 014 },
            { _T("IOT"),  0000004, 020 },
            { _T("EMT"),  0104001, 030 },
            { _T("TRAP"), 0104401, 034 },
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        {
            const TrapCase& c = cases[i];
            TestBoard tb;
            Prepare(tb, &c.opcode, 1);
            tb.SetWord(c.vector, 02000);        // new PC
            tb.SetWord(c.vector + 2, TF_Z);     // new PSW
            tb.SetFlags(TF_N | TF_C);

            tb.Run();

            Eq(02000, tb.CPU()->GetPC(), c.name);
            Eq(TF_Z, tb.GetFlags(), c.name);
            Eq(STACK - 4, tb.CPU()->GetSP(), c.name);
            Eq(PROG + 2, tb.GetWord(STACK - 4), c.name);        // the old PC is on the top
            Eq(TF_N | TF_C, tb.GetWord(STACK - 2), c.name);     // the old PSW is below
        }
    }

    TEST_METHOD(Processor_RTI)
    {
        TestBoard tb;
        const uint16_t program[] = { 0000002 };
        Prepare(tb, program, 1);
        tb.CPU()->SetSP(STACK - 4);
        tb.SetWord(STACK - 4, 02000);
        tb.SetWord(STACK - 2, TF_V | TF_C);

        tb.Run();

        Eq(02000, tb.CPU()->GetPC(), _T("PC"));
        Eq(TF_V | TF_C, tb.GetFlags(), _T("flags"));
        Eq(STACK, tb.CPU()->GetSP(), _T("SP"));
    }

    TEST_METHOD(Processor_TrapAndReturn)
    {
        TestBoard tb;
        const uint16_t program[] = { 0104001, 0005200 };  // EMT 1; INC R0
        Prepare(tb, program, 2);
        tb.SetWord(030, 02000);
        tb.SetWord(032, 0);
        tb.SetWord(02000, 0000002);  // RTI
        tb.SetFlags(TF_C);

        tb.Run(2);  // EMT, then RTI of the handler
        Eq(PROG + 2, tb.CPU()->GetPC(), _T("back after EMT"));
        Eq(TF_C, tb.GetFlags(), _T("flags are restored"));
        Eq(STACK, tb.CPU()->GetSP(), _T("SP"));
    }

    TEST_METHOD(Processor_ReservedInstructionTraps)
    {
        // Vector 010; the opcode 000007 does not exist in the KM1801VM2
        TestBoard tb;
        const uint16_t program[] = { 0000007 };
        Prepare(tb, program, 1);
        tb.SetWord(010, 02000);
        tb.SetWord(012, 0);

        tb.Run();
        Assert::IsTrue(tb.TakeInterrupt(), _T("interrupt is pending"));

        Eq(02000, tb.CPU()->GetPC(), _T("PC"));
        Eq(STACK - 4, tb.CPU()->GetSP(), _T("SP"));
        Eq(PROG + 2, tb.GetWord(STACK - 4), _T("return address"));
    }

    TEST_METHOD(Processor_JMPWithRegisterOperandTraps)
    {
        // JMP R1 has an invalid addressing mode; the trap goes to vector 004
        TestBoard tb;
        const uint16_t program[] = { 0000101 };
        Prepare(tb, program, 1);
        tb.SetWord(004, 02000);
        tb.SetWord(006, 0);

        tb.Run();
        Assert::IsTrue(tb.TakeInterrupt(), _T("interrupt is pending"));

        Eq(02000, tb.CPU()->GetPC(), _T("PC"));
        Eq(STACK - 4, tb.CPU()->GetSP(), _T("SP"));
    }

    // MUL, DIV, ASH, ASHC

    TEST_METHOD(Processor_MUL)
    {
        struct MulCase
        {
            uint16_t r2;
            uint16_t factor;
            uint16_t hi;
            uint16_t lo;
            uint16_t flags;
        };
        static const MulCase cases[] =
        {
            { 5,      3,      0,      15,     0 },
            { 0,      3,      0,      0,      TF_Z },
            { 0xFFFE, 3,      0xFFFF, 0xFFFA, TF_N },            // -2 * 3 = -6
            { 0x8000, 2,      0xFFFF, 0x0000, TF_N | TF_C },     // -65536 does not fit in 16 bits
            { 0x4000, 4,      0x0001, 0x0000, TF_C },            // 65536 does not fit in 16 bits
            { 0x7FFF, 1,      0x0000, 0x7FFF, 0 },
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        {
            const MulCase& c = cases[i];
            TestBoard tb;
            const uint16_t program[] = { 0070227, c.factor };  // MUL #n,R2
            Prepare(tb, program, 2);
            tb.CPU()->SetReg(2, c.r2);

            tb.Run();

            TCHAR msg[60];
            _stprintf_s(msg, _T("MUL #%o,R2 with R2=%o"), (unsigned int) c.factor, (unsigned int) c.r2);
            Eq(c.hi, tb.CPU()->GetReg(2), msg);
            Eq(c.lo, tb.CPU()->GetReg(3), msg);
            Eq(c.flags, tb.GetFlags(), msg);
        }
    }

    TEST_METHOD(Processor_DIV)
    {
        // The dividend is R2:R3
        TestBoard tb;
        const uint16_t program[] = { 0071227, 3 };  // DIV #3,R2
        Prepare(tb, program, 2);
        tb.CPU()->SetReg(2, 0);
        tb.CPU()->SetReg(3, 20);

        tb.Run();

        Eq(6, tb.CPU()->GetReg(2), _T("quotient"));
        Eq(2, tb.CPU()->GetReg(3), _T("remainder"));
        Eq(0, tb.GetFlags(), _T("flags"));
    }

    TEST_METHOD(Processor_DIV_Negative)
    {
        // -20 / 3 = -6, remainder -2
        TestBoard tb;
        const uint16_t program[] = { 0071227, 3 };
        Prepare(tb, program, 2);
        tb.CPU()->SetReg(2, 0xFFFF);
        tb.CPU()->SetReg(3, 0xFFEC);

        tb.Run();

        Eq(0xFFFA, tb.CPU()->GetReg(2), _T("quotient"));
        Eq(0xFFFE, tb.CPU()->GetReg(3), _T("remainder"));
        Eq(TF_N, tb.GetFlags(), _T("flags"));
    }

    TEST_METHOD(Processor_DIV_ByZero)
    {
        TestBoard tb;
        const uint16_t program[] = { 0071227, 0 };
        Prepare(tb, program, 2);
        tb.CPU()->SetReg(2, 0);
        tb.CPU()->SetReg(3, 20);

        tb.Run();

        Eq(0, tb.CPU()->GetReg(2), _T("R2 is unchanged"));
        Eq(20, tb.CPU()->GetReg(3), _T("R3 is unchanged"));
        Eq(TF_V | TF_C, tb.GetFlags(), _T("flags"));
    }

    TEST_METHOD(Processor_DIV_Overflow)
    {
        // 0x00010000 / 1 = 65536 does not fit in 16 bits: V is set, the registers stay
        TestBoard tb;
        const uint16_t program[] = { 0071227, 1 };
        Prepare(tb, program, 2);
        tb.CPU()->SetReg(2, 1);
        tb.CPU()->SetReg(3, 0);

        tb.Run();

        Eq(1, tb.CPU()->GetReg(2), _T("R2 is unchanged"));
        Eq(0, tb.CPU()->GetReg(3), _T("R3 is unchanged"));
        Eq(TF_V, tb.GetFlags() & TF_V, _T("V"));
    }

    TEST_METHOD(Processor_ASH)
    {
        // The shift count is the low 6 bits of the source, signed: positive = left
        struct AshCase
        {
            uint16_t count;
            uint16_t in;
            uint16_t out;
            uint16_t flags;
        };
        static const AshCase cases[] =
        {
            { 2,      0x0001, 0x0004, 0 },
            { 0177777, 0x0004, 0x0002, 0 },                 // -1: right by one
            { 1,      0x8000, 0x0000, TF_Z | TF_V | TF_C },  // the sign changes, the bit goes to C
            { 1,      0x4000, 0x8000, TF_N | TF_V },         // the sign changes
            { 0177777, 0x8000, 0xC000, TF_N },                // arithmetic shift keeps the sign
            { 0177777, 0x0001, 0x0000, TF_Z | TF_C },         // the shifted out bit goes to C
            { 0,      0x1234, 0x1234, 0 },
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        {
            const AshCase& c = cases[i];
            TestBoard tb;
            const uint16_t program[] = { 0072027, c.count };  // ASH #n,R0
            Prepare(tb, program, 2);
            tb.CPU()->SetReg(0, c.in);

            tb.Run();

            TCHAR msg[60];
            _stprintf_s(msg, _T("ASH #%o,R0 with R0=%o"), (unsigned int) c.count, (unsigned int) c.in);
            Eq(c.out, tb.CPU()->GetReg(0), msg);
            Eq(c.flags, tb.GetFlags(), msg);
        }
    }

    TEST_METHOD(Processor_ASHC)
    {
        // The register pair R0:R1 is shifted as one 32-bit value
        TestBoard tb;
        const uint16_t program[] = { 0073027, 4 };  // ASHC #4,R0
        Prepare(tb, program, 2);
        tb.CPU()->SetReg(0, 0x0000);
        tb.CPU()->SetReg(1, 0xF001);

        tb.Run();

        Eq(0x000F, tb.CPU()->GetReg(0), _T("high part"));
        Eq(0x0010, tb.CPU()->GetReg(1), _T("low part"));
        Eq(0, tb.GetFlags(), _T("flags"));
    }

    // Instructions with a memory operand: read-modify-write

    TEST_METHOD(Processor_ReadModifyWriteInMemory)
    {
        struct MemoryCase
        {
            const TCHAR* name;
            uint16_t opcode;    // the operand is (R1)
            uint16_t before;
            uint16_t after;
            uint16_t flags;
        };
        static const MemoryCase cases[] =
        {
            { _T("INC (R1)"),  0005211, 0x0005, 0x0006, 0 },
            { _T("DEC (R1)"),  0005311, 0x0000, 0xFFFF, TF_N },
            { _T("CLR (R1)"),  0005011, 0x1234, 0x0000, TF_Z },
            { _T("COM (R1)"),  0005111, 0x00FF, 0xFF00, TF_N | TF_C },
            { _T("NEG (R1)"),  0005411, 0x0001, 0xFFFF, TF_N | TF_C },
            { _T("TST (R1)"),  0005711, 0x8000, 0x8000, TF_N },
            { _T("ASL (R1)"),  0006311, 0x4000, 0x8000, TF_N | TF_V },
            { _T("ASR (R1)"),  0006211, 0x0001, 0x0000, TF_Z | TF_C | TF_V },
            { _T("SWAB (R1)"), 0000311, 0x1234, 0x3412, 0 },
            { _T("INCB (R1)"), 0105211, 0x12FF, 0x1200, TF_Z },    // only the low byte changes
            { _T("DECB (R1)"), 0105311, 0x1200, 0x12FF, TF_N },
            { _T("CLRB (R1)"), 0105011, 0x1234, 0x1200, TF_Z },
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        {
            const MemoryCase& c = cases[i];
            TestBoard tb;
            Prepare(tb, &c.opcode, 1);
            tb.SetWord(DATA, c.before);
            tb.CPU()->SetReg(1, DATA);

            tb.Run();

            TCHAR fexp[8], msg[60];
            FlagsText(c.flags, fexp);
            _stprintf_s(msg, _T("%s, expected flags %s"), c.name, fexp);
            Eq(c.after, tb.GetWord(DATA), msg);
            Eq(c.flags, tb.GetFlags(), msg);
            Eq(DATA, tb.CPU()->GetReg(1), msg);
        }
    }

    TEST_METHOD(Processor_ReadModifyWriteWithAutoModes)
    {
        {
            TestBoard tb;
            const uint16_t program[] = { 0005221 };  // INC (R1)+
            Prepare(tb, program, 1);
            tb.SetWord(DATA, 5);
            tb.CPU()->SetReg(1, DATA);
            tb.Run();
            Eq(6, tb.GetWord(DATA), _T("INC (R1)+: word"));
            Eq(DATA + 2, tb.CPU()->GetReg(1), _T("INC (R1)+: R1"));
        }
        {
            TestBoard tb;
            const uint16_t program[] = { 0005241 };  // INC -(R1)
            Prepare(tb, program, 1);
            tb.SetWord(DATA, 5);
            tb.CPU()->SetReg(1, DATA + 2);
            tb.Run();
            Eq(6, tb.GetWord(DATA), _T("INC -(R1): word"));
            Eq(DATA, tb.CPU()->GetReg(1), _T("INC -(R1): R1"));
        }
    }

    TEST_METHOD(Processor_ADD_MemoryToMemory)
    {
        TestBoard tb;
        const uint16_t program[] = { 0061112 };  // ADD (R1),(R2)
        Prepare(tb, program, 1);
        tb.SetWord(0200, 0x0100);
        tb.SetWord(0210, 0x0023);
        tb.CPU()->SetReg(1, 0200);
        tb.CPU()->SetReg(2, 0210);
        tb.Run();
        Eq(0x0123, tb.GetWord(0210), _T("destination"));
        Eq(0x0100, tb.GetWord(0200), _T("source is unchanged"));
        Eq(0, tb.GetFlags(), _T("flags"));
    }

    // MARK, MFPS, MTPS

    TEST_METHOD(Processor_MARK)
    {
        // MARK nn: SP = PC + 2 * nn; PC = R5; R5 = pop
        TestBoard tb;
        const uint16_t program[] = { 0006402 };  // MARK 2
        Prepare(tb, program, 1);
        tb.CPU()->SetReg(5, 02000);
        tb.SetWord(PROG + 2 + 4, 0x1234);

        tb.Run();

        Eq(02000, tb.CPU()->GetPC(), _T("PC = old R5"));
        Eq(0x1234, tb.CPU()->GetReg(5), _T("R5 is popped"));
        Eq(PROG + 2 + 4 + 2, tb.CPU()->GetSP(), _T("SP"));
    }

    TEST_METHOD(Processor_MFPS)
    {
        // R0 = PSW low byte, sign extended; N and Z are set from the byte, V is cleared, C is kept
        TestBoard tb;
        const uint16_t program[] = { 0106700 };  // MFPS R0
        Prepare(tb, program, 1);
        tb.SetFlags(TF_N | TF_C);  // PSW low byte = 011

        tb.Run();

        Eq(011, tb.CPU()->GetReg(0), _T("R0"));
        Eq(TF_C, tb.GetFlags(), _T("flags"));
    }

    TEST_METHOD(Processor_MTPS)
    {
        // The low byte of the PSW is loaded from the low byte of the operand
        TestBoard tb;
        const uint16_t program[] = { 0106400 };  // MTPS R0
        Prepare(tb, program, 1);
        tb.CPU()->SetReg(0, 0xFF05);

        tb.Run();

        Eq(TF_Z | TF_C, tb.GetFlags(), _T("flags = 05"));
    }
};
