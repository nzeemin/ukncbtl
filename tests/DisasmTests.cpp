/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// DisasmTests.cpp : tests for the KM1801VM2 disassembler
//
// Expected values come from the PDP-11 / KM1801VM2 instruction encoding, not from the
// emulator output. Cases marked "characterization" only pin the current behavior.

#include "stdafx.h"
#include <CppUnitTest.h>
#include "emubase\Emubase.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace
{

struct DisasmCase
{
    uint16_t addr;          // address of the instruction
    uint16_t words[3];      // instruction words, zero padded
    const TCHAR* instr;     // expected mnemonic
    const TCHAR* arg;       // expected arguments
    unsigned int length;    // expected length in words
};

void CheckCases(const DisasmCase* cases, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        const DisasmCase& c = cases[i];

        TCHAR sInstr[16];
        TCHAR sArg[40];
        TCHAR msg[80];
        _stprintf_s(msg, _T("opcode %06o at %06o"), (unsigned int) c.words[0], (unsigned int) c.addr);

        unsigned int length = DisassembleInstruction(c.words, c.addr, sInstr, sArg);

        Assert::AreEqual(c.instr, sInstr, msg);
        Assert::AreEqual(c.arg, sArg, msg);
        Assert::AreEqual(c.length, length, msg);
    }
}

struct JumpCase
{
    uint16_t words[2];
    int delta;
};

void CheckJumpCases(const JumpCase* cases, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        TCHAR msg[40];
        _stprintf_s(msg, _T("opcode %06o"), (unsigned int) cases[i].words[0]);
        int delta = 12345;
        Assert::IsTrue(Disasm_CheckForJump(cases[i].words, &delta), msg);
        Assert::AreEqual(cases[i].delta, delta, msg);
    }
}

}

TEST_CLASS(DisasmTests)
{
public:
    TEST_METHOD(Disasm_NoOperandInstructions)
    {
        static const DisasmCase cases[] =
        {
            { 01000, { 0000000 }, _T("HALT"),  _T(""), 1 },
            { 01000, { 0000001 }, _T("WAIT"),  _T(""), 1 },
            { 01000, { 0000002 }, _T("RTI"),   _T(""), 1 },
            { 01000, { 0000003 }, _T("BPT"),   _T(""), 1 },
            { 01000, { 0000004 }, _T("IOT"),   _T(""), 1 },
            { 01000, { 0000005 }, _T("RESET"), _T(""), 1 },
            { 01000, { 0000006 }, _T("RTT"),   _T(""), 1 },
            { 01000, { 0000240 }, _T("NOP"),   _T(""), 1 },
            { 01000, { 0000241 }, _T("CLC"),   _T(""), 1 },
            { 01000, { 0000242 }, _T("CLV"),   _T(""), 1 },
            { 01000, { 0000243 }, _T("CLVC"),  _T(""), 1 },
            { 01000, { 0000244 }, _T("CLZ"),   _T(""), 1 },
            { 01000, { 0000250 }, _T("CLN"),   _T(""), 1 },
            { 01000, { 0000257 }, _T("CCC"),   _T(""), 1 },
            { 01000, { 0000261 }, _T("SEC"),   _T(""), 1 },
            { 01000, { 0000262 }, _T("SEV"),   _T(""), 1 },
            { 01000, { 0000264 }, _T("SEZ"),   _T(""), 1 },
            { 01000, { 0000270 }, _T("SEN"),   _T(""), 1 },
            { 01000, { 0000277 }, _T("SCC"),   _T(""), 1 },
        };
        CheckCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_AllConditionCodeInstructions)
    {
        // CLx / SEx: bit 4 = set/clear, bits 3..0 = N, Z, V, C; the mnemonic lists the flags as NZVC
        for (int set = 0; set < 2; set++)
        {
            for (int flags = 1; flags < 16; flags++)
            {
                uint16_t memory[2] = { (uint16_t) (0000240 | (set ? 020 : 0) | flags), 0 };

                TCHAR expected[8];
                _tcscpy_s(expected, set ? _T("SE") : _T("CL"));
                if (flags & 010) _tcscat_s(expected, _T("N"));
                if (flags & 004) _tcscat_s(expected, _T("Z"));
                if (flags & 002) _tcscat_s(expected, _T("V"));
                if (flags & 001) _tcscat_s(expected, _T("C"));
                if (flags == 017) _tcscpy_s(expected, set ? _T("SCC") : _T("CCC"));  // all four flags

                TCHAR msg[40];
                _stprintf_s(msg, _T("opcode %06o"), (unsigned int) memory[0]);
                TCHAR sInstr[16], sArg[40];
                DisassembleInstruction(memory, 01000, sInstr, sArg);
                Assert::AreEqual((const TCHAR*) expected, (const TCHAR*) sInstr, msg);
            }
        }
    }

    TEST_METHOD(Disasm_HaltModeInstructions)
    {
        // Special instructions of the KM1801VM2 HALT mode
        static const DisasmCase cases[] =
        {
            { 01000, { 0000012 }, _T("START"), _T(""), 1 },
            { 01000, { 0000016 }, _T("STEP"),  _T(""), 1 },
            { 01000, { 0000020 }, _T("RSEL"),  _T(""), 1 },
            { 01000, { 0000021 }, _T("MFUS"),  _T(""), 1 },
            { 01000, { 0000022 }, _T("RCPC"),  _T(""), 1 },
            { 01000, { 0000024 }, _T("RCPS"),  _T(""), 1 },
            { 01000, { 0000031 }, _T("MTUS"),  _T(""), 1 },
            { 01000, { 0000032 }, _T("WCPC"),  _T(""), 1 },
            { 01000, { 0000034 }, _T("WCPS"),  _T(""), 1 },
        };
        CheckCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_RtsAndFis)
    {
        static const DisasmCase cases[] =
        {
            { 01000, { 0000200 }, _T("RTS"),    _T("R0"), 1 },
            { 01000, { 0000205 }, _T("RTS"),    _T("R5"), 1 },
            { 01000, { 0000206 }, _T("RTS"),    _T("SP"), 1 },
            { 01000, { 0000207 }, _T("RETURN"), _T(""),   1 },  // RTS PC
            { 01000, { 0075000 }, _T("FADD"),   _T("R0"), 1 },
            { 01000, { 0075015 }, _T("FSUB"),   _T("R5"), 1 },
            { 01000, { 0075022 }, _T("FMUL"),   _T("R2"), 1 },
            { 01000, { 0075033 }, _T("FDIV"),   _T("R3"), 1 },
        };
        CheckCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_AddressingModes)
    {
        // Every mode of the destination operand, using CLR
        static const DisasmCase cases[] =
        {
            { 01000, { 0005000 },           _T("CLR"), _T("R0"),          1 },
            { 01000, { 0005011 },           _T("CLR"), _T("(R1)"),        1 },
            { 01000, { 0005022 },           _T("CLR"), _T("(R2)+"),       1 },
            { 01000, { 0005033 },           _T("CLR"), _T("@(R3)+"),      1 },
            { 01000, { 0005044 },           _T("CLR"), _T("-(R4)"),       1 },
            { 01000, { 0005055 },           _T("CLR"), _T("@-(R5)"),      1 },
            { 01000, { 0005061, 0001000 },  _T("CLR"), _T("001000(R1)"),  2 },
            { 01000, { 0005072, 0001000 },  _T("CLR"), _T("@001000(R2)"), 2 },
            { 01000, { 0005026 },           _T("CLR"), _T("(SP)+"),       1 },
            { 01000, { 0005046 },           _T("CLR"), _T("-(SP)"),       1 },
        };
        CheckCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_AddressingModesOfPC)
    {
        static const DisasmCase cases[] =
        {
            { 01000, { 0005007 },          _T("CLR"), _T("PC"),       1 },
            { 01000, { 0005017 },          _T("CLR"), _T("(PC)"),     1 },
            { 01000, { 0005027, 0000123 }, _T("CLR"), _T("#000123"),  2 },  // immediate
            { 01000, { 0005037, 0177564 }, _T("CLR"), _T("@#177564"), 2 },  // absolute
            { 01000, { 0005047 },          _T("CLR"), _T("-(PC)"),    1 },
            { 01000, { 0005057 },          _T("CLR"), _T("@-(PC)"),   1 },
            // relative: PC after fetching the offset word is 001004, plus offset 10
            { 01000, { 0005067, 0000010 }, _T("CLR"), _T("001014"),   2 },
            { 01000, { 0005077, 0000010 }, _T("CLR"), _T("@001014"),  2 },
            // negative offset
            { 01000, { 0005067, 0177770 }, _T("CLR"), _T("000774"),   2 },
        };
        CheckCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_SingleOperandInstructions)
    {
        static const DisasmCase cases[] =
        {
            { 01000, { 0000111 }, _T("JMP"),  _T("(R1)"), 1 },
            { 01000, { 0000302 }, _T("SWAB"), _T("R2"),   1 },
            { 01000, { 0005000 }, _T("CLR"),  _T("R0"),   1 },
            { 01000, { 0005100 }, _T("COM"),  _T("R0"),   1 },
            { 01000, { 0005200 }, _T("INC"),  _T("R0"),   1 },
            { 01000, { 0005300 }, _T("DEC"),  _T("R0"),   1 },
            { 01000, { 0005400 }, _T("NEG"),  _T("R0"),   1 },
            { 01000, { 0005500 }, _T("ADC"),  _T("R0"),   1 },
            { 01000, { 0005600 }, _T("SBC"),  _T("R0"),   1 },
            { 01000, { 0005700 }, _T("TST"),  _T("R0"),   1 },
            { 01000, { 0006000 }, _T("ROR"),  _T("R0"),   1 },
            { 01000, { 0006100 }, _T("ROL"),  _T("R0"),   1 },
            { 01000, { 0006200 }, _T("ASR"),  _T("R0"),   1 },
            { 01000, { 0006300 }, _T("ASL"),  _T("R0"),   1 },
            { 01000, { 0006700 }, _T("SXT"),  _T("R0"),   1 },
            { 01000, { 0106400 }, _T("MTPS"), _T("R0"),   1 },
            { 01000, { 0106700 }, _T("MFPS"), _T("R0"),   1 },
            // byte variants
            { 01000, { 0105000 }, _T("CLRB"), _T("R0"),   1 },
            { 01000, { 0105100 }, _T("COMB"), _T("R0"),   1 },
            { 01000, { 0105200 }, _T("INCB"), _T("R0"),   1 },
            { 01000, { 0105300 }, _T("DECB"), _T("R0"),   1 },
            { 01000, { 0105400 }, _T("NEGB"), _T("R0"),   1 },
            { 01000, { 0105500 }, _T("ADCB"), _T("R0"),   1 },
            { 01000, { 0105600 }, _T("SBCB"), _T("R0"),   1 },
            { 01000, { 0105700 }, _T("TSTB"), _T("R0"),   1 },
            { 01000, { 0106000 }, _T("RORB"), _T("R0"),   1 },
            { 01000, { 0106100 }, _T("ROLB"), _T("R0"),   1 },
            { 01000, { 0106200 }, _T("ASRB"), _T("R0"),   1 },
            { 01000, { 0106300 }, _T("ASLB"), _T("R0"),   1 },
        };
        CheckCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_TwoOperandInstructions)
    {
        static const DisasmCase cases[] =
        {
            { 01000, { 0010001 }, _T("MOV"),  _T("R0, R1"), 1 },
            { 01000, { 0020001 }, _T("CMP"),  _T("R0, R1"), 1 },
            { 01000, { 0030001 }, _T("BIT"),  _T("R0, R1"), 1 },
            { 01000, { 0040001 }, _T("BIC"),  _T("R0, R1"), 1 },
            { 01000, { 0050001 }, _T("BIS"),  _T("R0, R1"), 1 },
            { 01000, { 0060001 }, _T("ADD"),  _T("R0, R1"), 1 },
            { 01000, { 0160001 }, _T("SUB"),  _T("R0, R1"), 1 },
            { 01000, { 0110001 }, _T("MOVB"), _T("R0, R1"), 1 },
            { 01000, { 0120001 }, _T("CMPB"), _T("R0, R1"), 1 },
            { 01000, { 0130001 }, _T("BITB"), _T("R0, R1"), 1 },
            { 01000, { 0140001 }, _T("BICB"), _T("R0, R1"), 1 },
            { 01000, { 0150001 }, _T("BISB"), _T("R0, R1"), 1 },
            { 01000, { 0012243 }, _T("MOV"),  _T("(R2)+, -(R3)"), 1 },
        };
        CheckCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_TwoOperandInstructionsWithExtraWords)
    {
        static const DisasmCase cases[] =
        {
            // source immediate
            { 01000, { 0012700, 0000123 },          _T("MOV"), _T("#000123, R0"),             2 },
            // destination absolute
            { 01000, { 0010037, 0177564 },          _T("MOV"), _T("R0, @#177564"),            2 },
            // both operands take a word
            { 01000, { 0016162, 0000010, 0000020 }, _T("MOV"), _T("000010(R1), 000020(R2)"),  3 },
            { 01000, { 0012761, 0000001, 0000002 }, _T("MOV"), _T("#000001, 000002(R1)"),     3 },
            // both operands relative: PC is 001004 for the source and 001006 for the destination
            { 01000, { 0016767, 0000010, 0000020 }, _T("MOV"), _T("001014, 001026"),          3 },
            { 01000, { 0066767, 0000010, 0000020 }, _T("ADD"), _T("001014, 001026"),          3 },
        };
        CheckCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_PushAndPop)
    {
        static const DisasmCase cases[] =
        {
            { 01000, { 0010046 }, _T("PUSH"), _T("R0"),        1 },  // MOV R0,-(SP)
            { 01000, { 0012600 }, _T("POP"),  _T("R0"),        1 },  // MOV (SP)+,R0
            { 01000, { 0110046 }, _T("MOVB"), _T("R0, -(SP)"), 1 },  // byte move is not PUSH
            { 01000, { 0112600 }, _T("MOVB"), _T("(SP)+, R0"), 1 },  // byte move is not POP
        };
        CheckCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_Branches)
    {
        // Offset +1 word from the address 001000: target = 001000 + 2 + 2 = 001004
        static const DisasmCase cases[] =
        {
            { 01000, { 0000401 }, _T("BR"),   _T("001004"), 1 },
            { 01000, { 0001001 }, _T("BNE"),  _T("001004"), 1 },
            { 01000, { 0001401 }, _T("BEQ"),  _T("001004"), 1 },
            { 01000, { 0002001 }, _T("BGE"),  _T("001004"), 1 },
            { 01000, { 0002401 }, _T("BLT"),  _T("001004"), 1 },
            { 01000, { 0003001 }, _T("BGT"),  _T("001004"), 1 },
            { 01000, { 0003401 }, _T("BLE"),  _T("001004"), 1 },
            { 01000, { 0100001 }, _T("BPL"),  _T("001004"), 1 },
            { 01000, { 0100401 }, _T("BMI"),  _T("001004"), 1 },
            { 01000, { 0101001 }, _T("BHI"),  _T("001004"), 1 },
            { 01000, { 0101401 }, _T("BLOS"), _T("001004"), 1 },
            { 01000, { 0102001 }, _T("BVC"),  _T("001004"), 1 },
            { 01000, { 0102401 }, _T("BVS"),  _T("001004"), 1 },
            { 01000, { 0103001 }, _T("BHIS"), _T("001004"), 1 },  // same opcode as BCC
            { 01000, { 0103401 }, _T("BLO"),  _T("001004"), 1 },  // same opcode as BCS
        };
        CheckCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_BranchOffsets)
    {
        static const DisasmCase cases[] =
        {
            { 01000, { 0000400 }, _T("BR"), _T("001002"), 1 },  // offset 0: next instruction
            { 01000, { 0000777 }, _T("BR"), _T("001000"), 1 },  // offset -1: itself
            { 01000, { 0000776 }, _T("BR"), _T("000776"), 1 },  // offset -2
            { 01000, { 0000577 }, _T("BR"), _T("001400"), 1 },  // offset +127, the maximum
            { 01000, { 0000600 }, _T("BR"), _T("000402"), 1 },  // offset -128, the minimum
            { 00100, { 0000403 }, _T("BR"), _T("000110"), 1 },  // other base address
        };
        CheckCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_EmtAndTrap)
    {
        static const DisasmCase cases[] =
        {
            { 01000, { 0104000 }, _T("EMT"),  _T("000"), 1 },
            { 01000, { 0104012 }, _T("EMT"),  _T("012"), 1 },
            { 01000, { 0104377 }, _T("EMT"),  _T("377"), 1 },
            { 01000, { 0104400 }, _T("TRAP"), _T("000"), 1 },
            { 01000, { 0104412 }, _T("TRAP"), _T("012"), 1 },
            { 01000, { 0104777 }, _T("TRAP"), _T("377"), 1 },
        };
        CheckCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_JsrAndCall)
    {
        static const DisasmCase cases[] =
        {
            { 01000, { 0004310 },          _T("JSR"),  _T("R3, (R0)"),     1 },
            { 01000, { 0004537, 0100000 }, _T("JSR"),  _T("R5, @#100000"), 2 },
            { 01000, { 0004711 },          _T("CALL"), _T("(R1)"),         1 },  // JSR PC,(R1)
            { 01000, { 0004737, 0123456 }, _T("CALL"), _T("@#123456"),     2 },
            { 01000, { 0004767, 0000010 }, _T("CALL"), _T("001014"),       2 },
        };
        CheckCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_ArithmeticExtensions)
    {
        // The register is in bits 8..6, the operand in bits 5..0; the text is "operand, register"
        static const DisasmCase cases[] =
        {
            { 01000, { 0070102 },          _T("MUL"),  _T("R2, R1"),      1 },
            { 01000, { 0071102 },          _T("DIV"),  _T("R2, R1"),      1 },
            { 01000, { 0072102 },          _T("ASH"),  _T("R2, R1"),      1 },
            { 01000, { 0073102 },          _T("ASHC"), _T("R2, R1"),      1 },
            { 01000, { 0070127, 0000005 }, _T("MUL"),  _T("#000005, R1"), 2 },
            // XOR is the other way around: "register, operand"
            { 01000, { 0074102 },          _T("XOR"),  _T("R1, R2"),      1 },
        };
        CheckCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_Sob)
    {
        // SOB jumps back by nn words: target = address + 2 - 2 * nn
        static const DisasmCase cases[] =
        {
            { 01000, { 0077103 }, _T("SOB"), _T("R1, 000774"), 1 },
            { 01000, { 0077501 }, _T("SOB"), _T("R5, 001000"), 1 },
            { 01000, { 0077177 }, _T("SOB"), _T("R1, 000604"), 1 },  // maximum nn = 077
        };
        CheckCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_SobTargetBelowZeroWrapsTo16Bits)
    {
        // The address is 16-bit: 000000 + 2 - 2 * 3 = -4 = 177774
        static const DisasmCase cases[] =
        {
            { 00000, { 0077103 }, _T("SOB"), _T("R1, 177774"), 1 },
            { 00002, { 0077177 }, _T("SOB"), _T("R1, 177606"), 1 },  // 2 + 2 - 126 = -122
        };
        CheckCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_Mark)
    {
        // MARK nn: the 6-bit constant nn, not an operand; the instruction is one word long
        static const DisasmCase cases[] =
        {
            { 01000, { 0006400 }, _T("MARK"), _T("0"),  1 },
            { 01000, { 0006405 }, _T("MARK"), _T("5"),  1 },
            { 01000, { 0006412 }, _T("MARK"), _T("12"), 1 },
            { 01000, { 0006477 }, _T("MARK"), _T("77"), 1 },
            // low bits that look like an operand with an extra word: still one word
            { 01000, { 0006427, 0123456 }, _T("MARK"), _T("27"), 1 },
            { 01000, { 0006467, 0123456 }, _T("MARK"), _T("67"), 1 },
        };
        CheckCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_UnknownInstructions)
    {
        // Characterization: the opcodes below have no mnemonic in the disassembler
        static const DisasmCase cases[] =
        {
            { 01000, { 0000007 }, _T("unknown"), _T("000007"), 1 },
            { 01000, { 0170000 }, _T("unknown"), _T("170000"), 1 },
        };
        CheckCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_CheckForJumpBranches)
    {
        // Delta is the distance in words from the instruction to the jump target
        static const JumpCase cases[] =
        {
            { { 0000401 },  2 },    // BR +1
            { { 0000400 },  1 },    // BR 0
            { { 0000777 },  0 },    // BR -1: itself
            { { 0000600 }, -127 },  // BR -128
            { { 0000577 },  128 },  // BR +127
            { { 0001003 },  4 },    // BNE
            { { 0003777 },  0 },    // BLE -1
            { { 0100001 },  2 },    // BPL
            { { 0103401 },  2 },    // BLO
        };
        CheckJumpCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_CheckForJumpSobJmpCall)
    {
        static const JumpCase cases[] =
        {
            { { 0077103 },          -2 },  // SOB nn=3
            { { 0077100 },           1 },  // SOB nn=0
            { { 0000167, 0000010 },  6 },  // JMP 001014: (8 + 4) / 2
            { { 0004767, 0177770 }, -2 },  // CALL back by 8 bytes
        };
        CheckJumpCases(cases, sizeof(cases) / sizeof(cases[0]));
    }

    TEST_METHOD(Disasm_CheckForJumpNotAJump)
    {
        static const uint16_t cases[] =
        {
            0000000,  // HALT
            0004000,  // JSR R0,... is not tracked
            0004737,  // CALL @#addr: the target is not relative
            0000111,  // JMP (R1)
            0010001,  // MOV
            0104000,  // EMT
            0104400,  // TRAP
            0005000,  // CLR
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        {
            TCHAR msg[40];
            _stprintf_s(msg, _T("opcode %06o"), (unsigned int) cases[i]);
            uint16_t memory[2] = { cases[i], 0 };
            int delta = 12345;
            Assert::IsFalse(Disasm_CheckForJump(memory, &delta), msg);
        }
    }
};
