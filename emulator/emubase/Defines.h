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

inline uint8_t GetDigit(uint16_t word, int pos)
{
    //return (word >>= pos * 3) % 8;
    return (uint8_t)((word >>= ((pos << 1) + pos)) & 7);
}

// Constants for "pos" argument
#define SRC             2
#define DST             0


//////////////////////////////////////////////////////////////////////

// Interrupts
#define NO_INTERRUPT    0000
#define INTERRUPT_4     0004
#define INTERRUPT_10    0010
#define INTERRUPT_14    0014
#define INTERRUPT_20    0020
#define INTERRUPT_24    0024
#define INTERRUPT_30    0030
#define INTERRUPT_34    0034
#define INTERRUPT_60    0060
#define INTERRUPT_100   0100
#define INTERRUPT_274   0274

// Process Status Word (PSW) bits
#define PSW_C           1      // Carry
#define PSW_V           2      // Arithmetic overflow
#define PSW_Z           4      // Zero result
#define PSW_N           8      // Negative result
#define PSW_T           16     // Trap/Debug
#define PSW_P           0200   // Priority
#define PSW_HALT        0400   // Halt

// Commands -- no operands
#define PI_HALT         0000000
#define PI_WAIT         0000001
#define PI_RTI          0000002
#define PI_BPT          0000003
#define PI_IOT          0000004
#define PI_RESET        0000005
#define PI_RTT          0000006
#define PI_MFPT         0000007
#define PI_HALT10       0000010
#define PI_HALT11       0000011
#define PI_HALT12       0000012
#define PI_HALT13       0000013
#define PI_HALT14       0000014
#define PI_HALT15       0000015
#define PI_HALT16       0000016
#define PI_HALT17       0000017
#define PI_NOP          0000240
#define PI_CLC          0000241
#define PI_CLV          0000242
#define PI_CLVC         0000243
#define PI_CLZ          0000244
#define PI_CLZC         0000245
#define PI_CLZV         0000246
#define PI_CLZVC        0000247
#define PI_CLN          0000250
#define PI_CLNC         0000251
#define PI_CLNV         0000252
#define PI_CLNVC        0000253
#define PI_CLNZ         0000254
#define PI_CLNZC        0000255
#define PI_CLNZV        0000256
#define PI_CCC          0000257
#define PI_NOP260       0000260
#define PI_SEC          0000261
#define PI_SEV          0000262
#define PI_SEVC         0000263
#define PI_SEZ          0000264
#define PI_SEZC         0000265
#define PI_SEZV         0000266
#define PI_SEZVC        0000267
#define PI_SEN          0000270
#define PI_SENC         0000271
#define PI_SENV         0000272
#define PI_SENVC        0000273
#define PI_SENZ         0000274
#define PI_SENZC        0000275
#define PI_SENZV        0000276
#define PI_SCC          0000277
#define PI_MED          0076600

// Commands -- single operand
#define PI_RTS          0000200

// Commands -- two operands
#define PI_JMP          0000100
#define PI_SWAB         0000300
#define PI_CLR          0005000
#define PI_COM          0005100
#define PI_INC          0005200
#define PI_DEC          0005300
#define PI_NEG          0005400
#define PI_ADC          0005500
#define PI_SBC          0005600
#define PI_TST          0005700
#define PI_ROR          0006000
#define PI_ROL          0006100
#define PI_ASR          0006200
#define PI_ASL          0006300
#define PI_MARK         0006400
#define PI_SXT          0006700
#define PI_MTPS         0106400
#define PI_MFPS         0106700

// Commands -- branchs
#define PI_BR           0000400
#define PI_BNE          0001000
#define PI_BEQ          0001400
#define PI_BGE          0002000
#define PI_BLT          0002400
#define PI_BGT          0003000
#define PI_BLE          0003400
#define PI_BPL          0100000
#define PI_BMI          0100400
#define PI_BHI          0101000
#define PI_BLOS         0101400
#define PI_BVC          0102000
#define PI_BVS          0102400
#define PI_BHIS         0103000
#define PI_BLO          0103400

#define PI_EMT          0104000
#define PI_TRAP         0104400

// Commands -- three operands
#define PI_JSR          0004000
#define PI_MUL          0070000
#define PI_DIV          0071000
#define PI_ASH          0072000
#define PI_ASHC         0073000
#define PI_XOR          0074000
#define PI_SOB          0077000

// Commands -- four operands
#define PI_MOV          0010000
#define PI_CMP          0020000
#define PI_BIT          0030000
#define PI_BIC          0040000
#define PI_BIS          0050000

#define PI_ADD          0060000
#define PI_SUB          0160000

// Commands -- VM2 specifics
#define PI_MUL          0070000
#define PI_DIV          0071000
#define PI_ASH          0072000
#define PI_ASHC         0073000
#define PI_FADD         0075000
#define PI_FSUB         0075010
#define PI_FMUL         0075020
#define PI_FDIV         0075030

// Commands -- special commands, HALT mode only
#define PI_START        0000012  // Return to USER mode;  PC := CPC; PSW := CPS
#define PI_STEP         0000016
#define PI_RSEL         0000020  // R0 := SEL  - Read SEL register
#define PI_MFUS         0000021  // R0 := (R5)+
#define PI_RCPC         0000022  // R0 := CPC
#define PI_RCPS         0000024  // R0 := CPS
#define PI_MTUS         0000031  // -(R5) := R0
#define PI_WCPC         0000032  // CPC := R0
#define PI_WCPS         0000034  // CPS := R0

