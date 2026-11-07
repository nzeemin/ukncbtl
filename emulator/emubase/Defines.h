/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

/// \file Defines.h  PDP11-like processor defines

#pragma once


//////////////////////////////////////////////////////////////////////

constexpr uint8_t GetDigit(uint16_t word, int pos)
{
    return static_cast<uint8_t>((word >> (pos * 3)) & 7);
}

// Constants for "pos" argument
inline constexpr auto SRC = 2;
inline constexpr auto DST = 0;


//////////////////////////////////////////////////////////////////////

// Interrupts
inline constexpr auto NO_INTERRUPT = 0000;
inline constexpr auto INTERRUPT_4 = 0004;
inline constexpr auto INTERRUPT_10 = 0010;
inline constexpr auto INTERRUPT_14 = 0014;
inline constexpr auto INTERRUPT_20 = 0020;
inline constexpr auto INTERRUPT_24 = 0024;
inline constexpr auto INTERRUPT_30 = 0030;
inline constexpr auto INTERRUPT_34 = 0034;
inline constexpr auto INTERRUPT_60 = 0060;
inline constexpr auto INTERRUPT_100 = 0100;
inline constexpr auto INTERRUPT_274 = 0274;

// Process Status Word (PSW) bits
inline constexpr auto PSW_C = 1;      // Carry
inline constexpr auto PSW_V = 2;      // Arithmetic overflow
inline constexpr auto PSW_Z = 4;      // Zero result
inline constexpr auto PSW_N = 8;      // Negative result
inline constexpr auto PSW_T = 16;     // Trap/Debug
inline constexpr auto PSW_P = 0200;   // Priority
inline constexpr auto PSW_HALT = 0400;   // Halt

// Commands -- no operands
inline constexpr auto PI_HALT = 0000000;
inline constexpr auto PI_WAIT = 0000001;
inline constexpr auto PI_RTI = 0000002;
inline constexpr auto PI_BPT = 0000003;
inline constexpr auto PI_IOT = 0000004;
inline constexpr auto PI_RESET = 0000005;
inline constexpr auto PI_RTT = 0000006;
inline constexpr auto PI_MFPT = 0000007;
inline constexpr auto PI_HALT10 = 0000010;
inline constexpr auto PI_HALT11 = 0000011;
inline constexpr auto PI_HALT12 = 0000012;
inline constexpr auto PI_HALT13 = 0000013;
inline constexpr auto PI_HALT14 = 0000014;
inline constexpr auto PI_HALT15 = 0000015;
inline constexpr auto PI_HALT16 = 0000016;
inline constexpr auto PI_HALT17 = 0000017;
inline constexpr auto PI_NOP = 0000240;
inline constexpr auto PI_CLC = 0000241;
inline constexpr auto PI_CLV = 0000242;
inline constexpr auto PI_CLVC = 0000243;
inline constexpr auto PI_CLZ = 0000244;
inline constexpr auto PI_CLZC = 0000245;
inline constexpr auto PI_CLZV = 0000246;
inline constexpr auto PI_CLZVC = 0000247;
inline constexpr auto PI_CLN = 0000250;
inline constexpr auto PI_CLNC = 0000251;
inline constexpr auto PI_CLNV = 0000252;
inline constexpr auto PI_CLNVC = 0000253;
inline constexpr auto PI_CLNZ = 0000254;
inline constexpr auto PI_CLNZC = 0000255;
inline constexpr auto PI_CLNZV = 0000256;
inline constexpr auto PI_CCC = 0000257;
inline constexpr auto PI_NOP260 = 0000260;
inline constexpr auto PI_SEC = 0000261;
inline constexpr auto PI_SEV = 0000262;
inline constexpr auto PI_SEVC = 0000263;
inline constexpr auto PI_SEZ = 0000264;
inline constexpr auto PI_SEZC = 0000265;
inline constexpr auto PI_SEZV = 0000266;
inline constexpr auto PI_SEZVC = 0000267;
inline constexpr auto PI_SEN = 0000270;
inline constexpr auto PI_SENC = 0000271;
inline constexpr auto PI_SENV = 0000272;
inline constexpr auto PI_SENVC = 0000273;
inline constexpr auto PI_SENZ = 0000274;
inline constexpr auto PI_SENZC = 0000275;
inline constexpr auto PI_SENZV = 0000276;
inline constexpr auto PI_SCC = 0000277;
inline constexpr auto PI_MED = 0076600;

// Commands -- single operand
inline constexpr auto PI_RTS = 0000200;

// Commands -- two operands
inline constexpr auto PI_JMP = 0000100;
inline constexpr auto PI_SWAB = 0000300;
inline constexpr auto PI_CLR = 0005000;
inline constexpr auto PI_COM = 0005100;
inline constexpr auto PI_INC = 0005200;
inline constexpr auto PI_DEC = 0005300;
inline constexpr auto PI_NEG = 0005400;
inline constexpr auto PI_ADC = 0005500;
inline constexpr auto PI_SBC = 0005600;
inline constexpr auto PI_TST = 0005700;
inline constexpr auto PI_ROR = 0006000;
inline constexpr auto PI_ROL = 0006100;
inline constexpr auto PI_ASR = 0006200;
inline constexpr auto PI_ASL = 0006300;
inline constexpr auto PI_MARK = 0006400;
inline constexpr auto PI_SXT = 0006700;
inline constexpr auto PI_MTPS = 0106400;
inline constexpr auto PI_MFPS = 0106700;

// Commands -- branchs
inline constexpr auto PI_BR = 0000400;
inline constexpr auto PI_BNE = 0001000;
inline constexpr auto PI_BEQ = 0001400;
inline constexpr auto PI_BGE = 0002000;
inline constexpr auto PI_BLT = 0002400;
inline constexpr auto PI_BGT = 0003000;
inline constexpr auto PI_BLE = 0003400;
inline constexpr auto PI_BPL = 0100000;
inline constexpr auto PI_BMI = 0100400;
inline constexpr auto PI_BHI = 0101000;
inline constexpr auto PI_BLOS = 0101400;
inline constexpr auto PI_BVC = 0102000;
inline constexpr auto PI_BVS = 0102400;
inline constexpr auto PI_BHIS = 0103000;
inline constexpr auto PI_BLO = 0103400;

inline constexpr auto PI_EMT = 0104000;
inline constexpr auto PI_TRAP = 0104400;

// Commands -- three operands
inline constexpr auto PI_JSR = 0004000;
inline constexpr auto PI_MUL = 0070000;
inline constexpr auto PI_DIV = 0071000;
inline constexpr auto PI_ASH = 0072000;
inline constexpr auto PI_ASHC = 0073000;
inline constexpr auto PI_XOR = 0074000;
inline constexpr auto PI_SOB = 0077000;

// Commands -- four operands
inline constexpr auto PI_MOV = 0010000;
inline constexpr auto PI_CMP = 0020000;
inline constexpr auto PI_BIT = 0030000;
inline constexpr auto PI_BIC = 0040000;
inline constexpr auto PI_BIS = 0050000;

inline constexpr auto PI_ADD = 0060000;
inline constexpr auto PI_SUB = 0160000;

// Commands -- VM2 specifics
inline constexpr auto PI_FADD = 0075000;
inline constexpr auto PI_FSUB = 0075010;
inline constexpr auto PI_FMUL = 0075020;
inline constexpr auto PI_FDIV = 0075030;

// Commands -- special commands, HALT mode only
inline constexpr auto PI_START = 0000012;  // Return to USER mode;  PC := CPC; PSW := CPS
inline constexpr auto PI_STEP = 0000016;
inline constexpr auto PI_RSEL = 0000020;  // R0 := SEL  - Read SEL register
inline constexpr auto PI_MFUS = 0000021;  // R0 := (R5)+
inline constexpr auto PI_RCPC = 0000022;  // R0 := CPC
inline constexpr auto PI_RCPS = 0000024;  // R0 := CPS
inline constexpr auto PI_MTUS = 0000031;  // -(R5) := R0
inline constexpr auto PI_WCPC = 0000032;  // CPC := R0
inline constexpr auto PI_WCPS = 0000034;  // CPS := R0

