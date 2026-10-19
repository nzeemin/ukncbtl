/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

/// \file Disasm.cpp
/// \brief Disassembler for KM1801VM2 processor
/// \details See defines in header file Emubase.h

#include "stdafx.h"
#include "Defines.h"
#include "Emubase.h"


// Display formats for the addressing modes
const LPCTSTR ADDRESS_MODE_FORMAT[] =
{
    _T("%s"), _T("(%s)"), _T("(%s)+"), _T("@(%s)+"), _T("-(%s)"), _T("@-(%s)"), _T("%06o(%s)"), _T("@%06o(%s)")
};
// Display formats for the addressing modes of the PC register
const LPCTSTR ADDRESS_MODE_PC_FORMAT[] =
{
    _T("PC"), _T("(PC)"), _T("#%06o"), _T("@#%06o"), _T("-(PC)"), _T("@-(PC)"), _T("%06o"), _T("@%06o")
};

//   pCode  - pointer to the extra word of the operand; read only if the addressing mode has one
//   strSrc - at least 24 characters
uint16_t ConvertSrcToString(uint16_t instr, uint16_t addr, TCHAR* strSrc, const uint16_t* pCode)
{
    const size_t strSrcSize = 24;

    uint8_t reg = GetDigit(instr, 2);
    uint8_t param = GetDigit(instr, 3);

    LPCTSTR pszReg = REGISTER_NAME[reg];

    if (reg != 7)
    {
        LPCTSTR format = ADDRESS_MODE_FORMAT[param];

        if (param == 6 || param == 7)
        {
            uint16_t word = *pCode;
            _sntprintf(strSrc, strSrcSize - 1, format, word, pszReg);
            return 1;
        }
        else
            _sntprintf(strSrc, strSrcSize - 1, format, pszReg);
    }
    else
    {
        LPCTSTR format = ADDRESS_MODE_PC_FORMAT[param];

        if (param == 2 || param == 3)
        {
            uint16_t word = *pCode;
            _sntprintf(strSrc, strSrcSize - 1, format, word);
            return 1;
        }
        else if (param == 6 || param == 7)
        {
            uint16_t word = *pCode;
            _sntprintf(strSrc, strSrcSize - 1, format, (uint16_t)(addr + word + 2));
            return 1;
        }
        else
            _sntprintf(strSrc, strSrcSize - 1, format, pszReg);
    }

    return 0;
}

//   strDst - at least 24 characters
uint16_t ConvertDstToString (uint16_t instr, uint16_t addr, TCHAR* strDst, const uint16_t* pCode)
{
    const size_t strDstSize = 24;

    uint8_t reg = GetDigit(instr, 0);
    uint8_t param = GetDigit(instr, 1);

    LPCTSTR pszReg = REGISTER_NAME[reg];

    if (reg != 7)
    {
        LPCTSTR format = ADDRESS_MODE_FORMAT[param];

        if (param == 6 || param == 7)
        {
            _sntprintf(strDst, strDstSize - 1, format, *pCode, pszReg);
            return 1;
        }
        else
            _sntprintf(strDst, strDstSize - 1, format, pszReg);
    }
    else
    {
        LPCTSTR format = ADDRESS_MODE_PC_FORMAT[param];

        if (param == 2 || param == 3)
        {
            _sntprintf(strDst, strDstSize - 1, format, *pCode);
            return 1;
        }
        else if (param == 6 || param == 7)
        {
            _sntprintf(strDst, strDstSize - 1, format, (uint16_t)(addr + *pCode + 2));
            return 1;
        }
        else
            _sntprintf(strDst, strDstSize - 1, format, pszReg);
    }

    return 0;
}

// Disassemble one instruction
//   pMemory - memory image (we read only words of the instruction)
//   strInstr - instruction mnemonics buffer, at least 8 characters
//   strArg   - instruction arguments buffer, at least 32 characters
//   Return value: number of words in the instruction
uint16_t DisassembleInstruction(const uint16_t* pMemory, uint16_t addr, TCHAR* strInstr, TCHAR* strArg)
{
    //const size_t strInstrSize = 8;
    const size_t strArgSize = 32;

    *strInstr = 0;
    *strArg = 0;

    uint16_t instr = *pMemory;
    uint16_t length = 1;
    LPCTSTR strReg = nullptr;

    const size_t strSrcSize = 24;
    TCHAR strSrc[strSrcSize];
    const size_t strDstSize = 24;
    TCHAR strDst[strDstSize];

    bool okByte;

    // No fields
    switch (instr)
    {
    case PI_HALT:   _tcscpy(strInstr, _T("HALT"));   return 1;
    case PI_WAIT:   _tcscpy(strInstr, _T("WAIT"));   return 1;
    case PI_RTI:    _tcscpy(strInstr, _T("RTI"));    return 1;
    case PI_BPT:    _tcscpy(strInstr, _T("BPT"));    return 1;
    case PI_IOT:    _tcscpy(strInstr, _T("IOT"));    return 1;
    case PI_RESET:  _tcscpy(strInstr, _T("RESET"));  return 1;
    case PI_RTT:    _tcscpy(strInstr, _T("RTT"));    return 1;
    case PI_NOP:    _tcscpy(strInstr, _T("NOP"));    return 1;
    case PI_CLC:    _tcscpy(strInstr, _T("CLC"));    return 1;
    case PI_CLV:    _tcscpy(strInstr, _T("CLV"));    return 1;
    case PI_CLVC:   _tcscpy(strInstr, _T("CLVC"));   return 1;
    case PI_CLZ:    _tcscpy(strInstr, _T("CLZ"));    return 1;
    case PI_CLZC:   _tcscpy(strInstr, _T("CLZC"));   return 1;
    case PI_CLZV:   _tcscpy(strInstr, _T("CLZV"));   return 1;
    case PI_CLZVC:  _tcscpy(strInstr, _T("CLZVC"));  return 1;
    case PI_CLN:    _tcscpy(strInstr, _T("CLN"));    return 1;
    case PI_CLNC:   _tcscpy(strInstr, _T("CLNC"));   return 1;
    case PI_CLNV:   _tcscpy(strInstr, _T("CLNV"));   return 1;
    case PI_CLNVC:  _tcscpy(strInstr, _T("CLNVC"));  return 1;
    case PI_CLNZ:   _tcscpy(strInstr, _T("CLNZ"));   return 1;
    case PI_CLNZC:  _tcscpy(strInstr, _T("CLNZC"));  return 1;
    case PI_CLNZV:  _tcscpy(strInstr, _T("CLNZV"));  return 1;
    case PI_CCC:    _tcscpy(strInstr, _T("CCC"));    return 1;
    case PI_NOP260: _tcscpy(strInstr, _T("NOP260")); return 1;
    case PI_SEC:    _tcscpy(strInstr, _T("SEC"));    return 1;
    case PI_SEV:    _tcscpy(strInstr, _T("SEV"));    return 1;
    case PI_SEVC:   _tcscpy(strInstr, _T("SEVC"));   return 1;
    case PI_SEZ:    _tcscpy(strInstr, _T("SEZ"));    return 1;
    case PI_SEZC:   _tcscpy(strInstr, _T("SEZC"));   return 1;
    case PI_SEZV:   _tcscpy(strInstr, _T("SEZV"));   return 1;
    case PI_SEZVC:  _tcscpy(strInstr, _T("SEZVC"));  return 1;
    case PI_SEN:    _tcscpy(strInstr, _T("SEN"));    return 1;
    case PI_SENC:   _tcscpy(strInstr, _T("SENC"));   return 1;
    case PI_SENV:   _tcscpy(strInstr, _T("SENV"));   return 1;
    case PI_SENVC:  _tcscpy(strInstr, _T("SENVC"));  return 1;
    case PI_SENZ:   _tcscpy(strInstr, _T("SENZ"));   return 1;
    case PI_SENZC:  _tcscpy(strInstr, _T("SENZC"));  return 1;
    case PI_SENZV:  _tcscpy(strInstr, _T("SENZV"));  return 1;
    case PI_SCC:    _tcscpy(strInstr, _T("SCC"));    return 1;

        // Special commands of the VM2 HALT mode
    case PI_START:  _tcscpy(strInstr, _T("START"));  return 1;
    case PI_STEP:   _tcscpy(strInstr, _T("STEP"));   return 1;
    case PI_RSEL:   _tcscpy(strInstr, _T("RSEL"));   return 1;
    case PI_MFUS:   _tcscpy(strInstr, _T("MFUS"));   return 1;
    case PI_RCPC:   _tcscpy(strInstr, _T("RCPC"));   return 1;
    case PI_RCPS:   _tcscpy(strInstr, _T("RCPS"));   return 1;
    case PI_MTUS:   _tcscpy(strInstr, _T("MTUS"));   return 1;
    case PI_WCPC:   _tcscpy(strInstr, _T("WCPC"));   return 1;
    case PI_WCPS:   _tcscpy(strInstr, _T("WCPS"));   return 1;
    }

    // One field
    if ((instr & ~(uint16_t)7) == PI_RTS)
    {
        if (GetDigit(instr, 0) == 7)
        {
            _tcscpy(strInstr, _T("RETURN"));
            return 1;
        }

        _tcscpy(strInstr, _T("RTS"));
        _tcscpy(strArg, REGISTER_NAME[GetDigit(instr, 0)]);
        return 1;
    }

    // FIS
    switch (instr & ~(uint16_t)7)
    {
    case PI_FADD:  _tcscpy(strInstr, _T("FADD"));  _tcscpy(strArg, REGISTER_NAME[GetDigit(instr, 0)]);  return 1;
    case PI_FSUB:  _tcscpy(strInstr, _T("FSUB"));  _tcscpy(strArg, REGISTER_NAME[GetDigit(instr, 0)]);  return 1;
    case PI_FMUL:  _tcscpy(strInstr, _T("FMUL"));  _tcscpy(strArg, REGISTER_NAME[GetDigit(instr, 0)]);  return 1;
    case PI_FDIV:  _tcscpy(strInstr, _T("FDIV"));  _tcscpy(strArg, REGISTER_NAME[GetDigit(instr, 0)]);  return 1;
    }

    // MARK has a 6-bit constant instead of an operand, the instruction is one word long
    if ((instr & ~(uint16_t)077) == PI_MARK)
    {
        _tcscpy(strInstr, _T("MARK"));
        _sntprintf(strArg, strArgSize - 1, _T("%o"), (unsigned int)(instr & 077));
        return 1;
    }

    // Two fields
    length += ConvertDstToString(instr, addr + 2, strDst, pMemory + 1);

    switch (instr & ~(uint16_t)077)
    {
    case PI_JMP:    _tcscpy(strInstr, _T("JMP"));   _tcscpy(strArg, strDst);  return length;
    case PI_SWAB:   _tcscpy(strInstr, _T("SWAB"));  _tcscpy(strArg, strDst);  return length;
    case PI_SXT:    _tcscpy(strInstr, _T("SXT"));   _tcscpy(strArg, strDst);  return length;
    case PI_MTPS:   _tcscpy(strInstr, _T("MTPS"));  _tcscpy(strArg, strDst);  return length;
    case PI_MFPS:   _tcscpy(strInstr, _T("MFPS"));  _tcscpy(strArg, strDst);  return length;
    }

    okByte = (instr & 0100000) != 0;

    switch (instr & ~(uint16_t)0100077)
    {
    case PI_CLR:  _tcscpy(strInstr, okByte ? _T("CLRB") : _T("CLR"));  _tcscpy(strArg, strDst);  return length;
    case PI_COM:  _tcscpy(strInstr, okByte ? _T("COMB") : _T("COM"));  _tcscpy(strArg, strDst);  return length;
    case PI_INC:  _tcscpy(strInstr, okByte ? _T("INCB") : _T("INC"));  _tcscpy(strArg, strDst);  return length;
    case PI_DEC:  _tcscpy(strInstr, okByte ? _T("DECB") : _T("DEC"));  _tcscpy(strArg, strDst);  return length;
    case PI_NEG:  _tcscpy(strInstr, okByte ? _T("NEGB") : _T("NEG"));  _tcscpy(strArg, strDst);  return length;
    case PI_ADC:  _tcscpy(strInstr, okByte ? _T("ADCB") : _T("ADC"));  _tcscpy(strArg, strDst);  return length;
    case PI_SBC:  _tcscpy(strInstr, okByte ? _T("SBCB") : _T("SBC"));  _tcscpy(strArg, strDst);  return length;
    case PI_TST:  _tcscpy(strInstr, okByte ? _T("TSTB") : _T("TST"));  _tcscpy(strArg, strDst);  return length;
    case PI_ROR:  _tcscpy(strInstr, okByte ? _T("RORB") : _T("ROR"));  _tcscpy(strArg, strDst);  return length;
    case PI_ROL:  _tcscpy(strInstr, okByte ? _T("ROLB") : _T("ROL"));  _tcscpy(strArg, strDst);  return length;
    case PI_ASR:  _tcscpy(strInstr, okByte ? _T("ASRB") : _T("ASR"));  _tcscpy(strArg, strDst);  return length;
    case PI_ASL:  _tcscpy(strInstr, okByte ? _T("ASLB") : _T("ASL"));  _tcscpy(strArg, strDst);  return length;
    }

    length = 1;
    _sntprintf(strDst, strDstSize - 1, _T("%06ho"), (uint16_t)(addr + ((short)(char)(uint8_t)(instr & 0xff) * 2) + 2));

    // Branches & interrupts
    switch (instr & ~(uint16_t)0377)
    {
    case PI_BR:   _tcscpy(strInstr, _T("BR"));   _tcscpy(strArg, strDst);  return 1;
    case PI_BNE:  _tcscpy(strInstr, _T("BNE"));  _tcscpy(strArg, strDst);  return 1;
    case PI_BEQ:  _tcscpy(strInstr, _T("BEQ"));  _tcscpy(strArg, strDst);  return 1;
    case PI_BGE:  _tcscpy(strInstr, _T("BGE"));  _tcscpy(strArg, strDst);  return 1;
    case PI_BLT:  _tcscpy(strInstr, _T("BLT"));  _tcscpy(strArg, strDst);  return 1;
    case PI_BGT:  _tcscpy(strInstr, _T("BGT"));  _tcscpy(strArg, strDst);  return 1;
    case PI_BLE:  _tcscpy(strInstr, _T("BLE"));  _tcscpy(strArg, strDst);  return 1;
    case PI_BPL:  _tcscpy(strInstr, _T("BPL"));  _tcscpy(strArg, strDst);  return 1;
    case PI_BMI:  _tcscpy(strInstr, _T("BMI"));  _tcscpy(strArg, strDst);  return 1;
    case PI_BHI:  _tcscpy(strInstr, _T("BHI"));  _tcscpy(strArg, strDst);  return 1;
    case PI_BLOS:  _tcscpy(strInstr, _T("BLOS"));  _tcscpy(strArg, strDst);  return 1;
    case PI_BVC:  _tcscpy(strInstr, _T("BVC"));  _tcscpy(strArg, strDst);  return 1;
    case PI_BVS:  _tcscpy(strInstr, _T("BVS"));  _tcscpy(strArg, strDst);  return 1;
    case PI_BHIS:  _tcscpy(strInstr, _T("BHIS"));  _tcscpy(strArg, strDst);  return 1;
    case PI_BLO:  _tcscpy(strInstr, _T("BLO"));  _tcscpy(strArg, strDst);  return 1;
    }

    _sntprintf(strDst, strDstSize - 1, _T("%06o"), (uint8_t)(instr & 0xff));

    switch (instr & ~(uint16_t)0377)
    {
    case PI_EMT:   _tcscpy(strInstr, _T("EMT"));   _tcscpy(strArg, strDst + 3);  return 1;
    case PI_TRAP:  _tcscpy(strInstr, _T("TRAP"));  _tcscpy(strArg, strDst + 3);  return 1;
    }

    // Three fields
    switch (instr & ~(uint16_t)0777)
    {
    case PI_JSR:
        if (GetDigit(instr, 2) == 7)
        {
            _tcscpy(strInstr, _T("CALL"));
            length += ConvertDstToString (instr, addr + 2, strDst, pMemory + 1);
            _sntprintf(strArg, strArgSize - 1, _T("%s"), strDst);  // strArg = strDst;
        }
        else
        {
            _tcscpy(strInstr, _T("JSR"));
            strReg = REGISTER_NAME[GetDigit(instr, 2)];
            length += ConvertDstToString (instr, addr + 2, strDst, pMemory + 1);
            _sntprintf(strArg, strArgSize - 1, _T("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        }
        return length;
    case PI_MUL:
        _tcscpy(strInstr, _T("MUL"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory + 1);
        _sntprintf(strArg, strArgSize - 1, _T("%s, %s"), strDst, strReg);  // strArg = strDst + ", " + strReg;
        return length;
    case PI_DIV:
        _tcscpy(strInstr, _T("DIV"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory + 1);
        _sntprintf(strArg, strArgSize - 1, _T("%s, %s"), strDst, strReg);  // strArg = strDst + ", " + strReg;
        return length;
    case PI_ASH:
        _tcscpy(strInstr, _T("ASH"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory + 1);
        _sntprintf(strArg, strArgSize - 1, _T("%s, %s"), strDst, strReg);  // strArg = strDst + ", " + strReg;
        return length;
    case PI_ASHC:
        _tcscpy(strInstr, _T("ASHC"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory + 1);
        _sntprintf(strArg, strArgSize - 1, _T("%s, %s"), strDst, strReg);  // strArg = strDst + ", " + strReg;
        return length;
    case PI_XOR:
        _tcscpy(strInstr, _T("XOR"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory + 1);
        _sntprintf(strArg, strArgSize - 1, _T("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return length;
    case PI_SOB:
        _tcscpy(strInstr, _T("SOB"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        _sntprintf(strDst, strDstSize - 1, _T("%06o"), (uint16_t)(addr - (GetDigit(instr, 1) * 8 + GetDigit(instr, 0)) * 2 + 2));
        _sntprintf(strArg, strArgSize - 1, _T("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return 1;
    }

    // Four fields

    okByte = (instr & 0100000) != 0;

    length += ConvertSrcToString(instr, addr + 2, strSrc, pMemory + 1);
    length += ConvertDstToString(instr, (uint16_t)(addr + 2 + (length - 1) * 2), strDst, pMemory + length);

    switch (instr & ~(uint16_t)0107777)
    {
    case PI_MOV:
        if (!okByte && GetDigit(instr, 0) == 6 && GetDigit(instr, 1) == 4)
        {
            _tcscpy(strInstr, _T("PUSH"));
            _sntprintf(strArg, strArgSize - 1, _T("%s"), strSrc);  // strArg = strSrc;
            return length;
        }
        if (!okByte && GetDigit(instr, 2) == 6 && GetDigit(instr, 3) == 2)
        {
            _tcscpy(strInstr, _T("POP"));
            _sntprintf(strArg, strArgSize - 1, _T("%s"), strDst);  // strArg = strDst;
            return length;
        }
        _tcscpy(strInstr, okByte ? _T("MOVB") : _T("MOV"));
        _sntprintf(strArg, strArgSize - 1, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_CMP:
        _tcscpy(strInstr, okByte ? _T("CMPB") : _T("CMP"));
        _sntprintf(strArg, strArgSize - 1, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_BIT:
        _tcscpy(strInstr, okByte ? _T("BITB") : _T("BIT"));
        _sntprintf(strArg, strArgSize - 1, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_BIC:
        _tcscpy(strInstr, okByte ? _T("BICB") : _T("BIC"));
        _sntprintf(strArg, strArgSize - 1, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_BIS:
        _tcscpy(strInstr, okByte ? _T("BISB") : _T("BIS"));
        _sntprintf(strArg, strArgSize - 1, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    }

    switch (instr & ~(uint16_t)0007777)
    {
    case PI_ADD:
        _tcscpy(strInstr, _T("ADD"));
        _sntprintf(strArg, strArgSize - 1, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_SUB:
        _tcscpy(strInstr, _T("SUB"));
        _sntprintf(strArg, strArgSize - 1, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    }

    _tcscpy(strInstr, _T("unknown"));
    _sntprintf(strArg, strArgSize - 1, _T("%06o"), instr);
    return 1;
}

bool Disasm_CheckForJump(const uint16_t* memory, uint16_t address, int* pDelta)
{
    uint16_t instr = *memory;

    // BR, BNE, BEQ, BGE, BLT, BGT, BLE
    // BPL, BMI, BHI, BLOS, BVC, BVS, BHIS, BLO
    if ((instr & 0177400) >= 0000400 && (instr & 0177400) < 0004000 ||
        (instr & 0177400) >= 0100000 && (instr & 0177400) < 0104000)
    {
        *pDelta = ((int)(int8_t)(instr & 0xff)) + 1;
        return true;
    }

    // SOB
    if ((instr & ~(uint16_t)0777) == PI_SOB)
    {
        *pDelta = -(GetDigit(instr, 1) * 8 + GetDigit(instr, 0)) + 1;
        return true;
    }

    // JMP and JSR (with any link register, CALL is JSR PC) with a target that is known without the registers:
    // the relative X(PC) and the absolute @#addr addressing; the word after the instruction is the offset or the address
    if ((instr & ~(uint16_t)077) == PI_JMP || (instr & ~(uint16_t)0777) == PI_JSR)
    {
        if ((instr & 077) == 067)  // X(PC): the target is the address after the offset word plus the offset
        {
            if ((memory[1] & 1) != 0)
                return false;  // the instruction address is even, so an odd offset gives an odd target, not a jump
            *pDelta = ((short)(memory[1]) + 4) / 2;
            return true;
        }
        if ((instr & 077) == 037)  // @#addr
        {
            uint16_t target = memory[1];
            if ((target & 1) != 0)
                return false;  // an odd address is not a target of a jump
            *pDelta = (int)(int16_t)(uint16_t)(target - address) / 2;
            return true;
        }
    }

    return false;
}

// Prepare "Jump Hint" string, and also calculate condition for conditional jump
// Returns: jump prediction flag: true = will jump, false = will not jump
bool Disasm_GetJumpConditionHint(
    const uint16_t* memory, const CProcessor * pProc, const CMemoryController * pMemCtl, LPTSTR buffer)
{
    const size_t buffersize = 32;
    *buffer = 0;
    uint16_t instr = *memory;
    uint16_t psw = pProc->GetPSW();

    if (instr >= 0001000 && instr <= 0001777)  // BNE, BEQ
    {
        _sntprintf(buffer, buffersize - 1, _T("Z=%c"), (psw & PSW_Z) ? '1' : '0');
        // BNE: IF (Z == 0)
        // BEQ: IF (Z == 1)
        bool value = ((psw & PSW_Z) != 0);
        return ((instr & 0400) == 0) ? !value : value;
    }
    if (instr >= 0002000 && instr <= 0002777)  // BGE, BLT
    {
        _sntprintf(buffer, buffersize - 1, _T("N=%c, V=%c"), (psw & PSW_N) ? '1' : '0', (psw & PSW_V) ? '1' : '0');
        // BGE: IF ((N xor V) == 0)
        // BLT: IF ((N xor V) == 1)
        bool value = (((psw & PSW_N) != 0) != ((psw & PSW_V) != 0));
        return ((instr & 0400) == 0) ? !value : value;
    }
    if (instr >= 0003000 && instr <= 0003777)  // BGT, BLE
    {
        _sntprintf(buffer, buffersize - 1, _T("N=%c, V=%c, Z=%c"), (psw & PSW_N) ? '1' : '0', (psw & PSW_V) ? '1' : '0', (psw & PSW_Z) ? '1' : '0');
        // BGT: IF (((N xor V) or Z) == 0)
        // BLE: IF (((N xor V) or Z) == 1)
        bool value = ((((psw & PSW_N) != 0) != ((psw & PSW_V) != 0)) || ((psw & PSW_Z) != 0));
        return ((instr & 0400) == 0) ? !value : value;
    }
    if (instr >= 0100000 && instr <= 0100777)  // BPL, BMI
    {
        _sntprintf(buffer, buffersize - 1, _T("N=%c"), (psw & PSW_N) ? '1' : '0');
        // BPL: IF (N == 0)
        // BMI: IF (N == 1)
        bool value = ((psw & PSW_N) != 0);
        return ((instr & 0400) == 0) ? !value : value;
    }
    if (instr >= 0101000 && instr <= 0101777)  // BHI, BLOS
    {
        _sntprintf(buffer, buffersize - 1, _T("C=%c, Z=%c"), (psw & PSW_C) ? '1' : '0', (psw & PSW_Z) ? '1' : '0');
        // BHI:  IF ((C or Z) == 0)
        // BLOS: IF ((C or Z) == 1)
        bool value = (((psw & PSW_C) != 0) || ((psw & PSW_Z) != 0));
        return ((instr & 0400) == 0) ? !value : value;
    }
    if (instr >= 0102000 && instr <= 0102777)  // BVC, BVS
    {
        _sntprintf(buffer, buffersize - 1, _T("V=%c"), (psw & PSW_V) ? '1' : '0');
        // BVC: IF (V == 0)
        // BVS: IF (V == 1)
        bool value = ((psw & PSW_V) != 0);
        return ((instr & 0400) == 0) ? !value : value;
    }
    if (instr >= 0103000 && instr <= 0103777)  // BCC/BHIS, BCS/BLO
    {
        _sntprintf(buffer, buffersize - 1, _T("C=%c"), (psw & PSW_C) ? '1' : '0');
        // BCC/BHIS: IF (C == 0)
        // BCS/BLO:  IF (C == 1)
        bool value = ((psw & PSW_C) != 0);
        return ((instr & 0400) == 0) ? !value : value;
    }
    if (instr >= 0077000 && instr <= 0077777)  // SOB
    {
        int reg = (instr >> 6) & 7;
        uint16_t regvalue = pProc->GetReg(reg);
        _sntprintf(buffer, buffersize - 1, _T("R%d=%06o"), reg, regvalue);  // "RN=XXXXXX"
        return (regvalue != 1);
    }

    if (instr >= 004000 && instr <= 004677)  // JSR (except CALL)
    {
        int reg = (instr >> 6) & 7;
        uint16_t regvalue = pProc->GetReg(reg);
        _sntprintf(buffer, buffersize - 1, _T("R%d=%06o"), reg, regvalue);  // "RN=XXXXXX"
        return true;
    }
    if (instr >= 000200 && instr <= 000207)  // RTS / RETURN
    {
        uint16_t spvalue = pProc->GetSP();
        int addrtype;
        uint16_t value = pMemCtl->GetWordView(spvalue, pProc->IsHaltMode(), false, &addrtype);
        if (instr == 000207)  // RETURN
            _sntprintf(buffer, buffersize - 1, _T("(SP)=%06o"), value);  // "(SP)=XXXXXX"
        else  // RTS
        {
            int reg = instr & 7;
            uint16_t regvalue = pProc->GetReg(reg);
            _sntprintf(buffer, buffersize - 1, _T("R%d=%06o, (SP)=%06o"), reg, regvalue, value);  // "RN=XXXXXX, (SP)=XXXXXX"
        }
        return true;
    }

    if (instr == 000002 || instr == 000006)  // RTI, RTT
    {
        uint16_t spvalue = pProc->GetSP();
        int addrtype;
        uint16_t value = pMemCtl->GetWordView(spvalue, pProc->IsHaltMode(), false, &addrtype);
        _sntprintf(buffer, buffersize - 1, _T("(SP)=%06o"), value);  // "(SP)=XXXXXX"
        return true;
    }
    if (instr == 000003 || instr == 000004 ||  // IOT, BPT
        (instr >= 0104000 && instr <= 0104777))  // TRAP, EMT
    {
        uint16_t intvec;
        if (instr == 000003) intvec = 000014;
        else if (instr == 000004) intvec = 000020;
        else if (instr < 0104400) intvec = 000030;
        else intvec = 000034;

        int addrtype;
        uint16_t value = pMemCtl->GetWordView(intvec, pProc->IsHaltMode(), false, &addrtype);
        _sntprintf(buffer, buffersize - 1, _T("(%06o)=%06o"), intvec, value);  // "(VVVVVV)=XXXXXX"
        return true;
    }

    return true;  // All other jumps are non-conditional
}

// Register values as the operand being decoded sees them. The source operand changes its register
// (autoincrement, autodecrement) before the destination is decoded, so the hint of the destination is built
// from the registers as the source left them.
struct HintRegs
{
    uint16_t r[8];
};

// Does the operand with the register and the addressing mode have an extra word after the instruction
static bool Disasm_HasExtraWord(int regnum, int regmod)
{
    if (regnum == 7)
        return regmod == 2 || regmod == 3 || regmod == 6 || regmod == 7;  // #n, @#addr, addr, @addr
    return regmod == 6 || regmod == 7;  // the index
}

// Step of the autoincrement / autodecrement: a byte operation steps by 1, but by 2 for SP and PC.
// The deferred modes 3 and 5 use a word pointer, step 2.
static int Disasm_GetStep(int regnum, int regmod, bool byteword)
{
    return (byteword && regnum < 6 && (regmod == 2 || regmod == 4)) ? 1 : 2;
}

// The change of the register made by the operand
static void Disasm_ApplySideEffect(HintRegs& regs, int regnum, int regmod, bool byteword)
{
    int step = Disasm_GetStep(regnum, regmod, byteword);
    if (regmod == 2 || regmod == 3)
        regs.r[regnum] = (uint16_t)(regs.r[regnum] + step);
    else if (regmod == 4 || regmod == 5)
        regs.r[regnum] = (uint16_t)(regs.r[regnum] - step);
}

// Read the value at the address for a hint and print it: the number (6 digits for a word, 3 for a byte),
// "I/O" for an I/O port (reading it from the view gives nothing real), "N/A" for an inaccessible address.
// strValue has at least 8 characters. Returns true if the value is real; then *pValue is the word or the byte.
static bool Disasm_ReadHintValue(const CProcessor * pProc, const CMemoryController * pMemCtl,
        uint16_t address, bool byteword, uint16_t* pValue, TCHAR* strValue)
{
    int addrtype = 0;
    uint16_t word = pMemCtl->GetWordView((uint16_t)(address & ~1), pProc->IsHaltMode(), false, &addrtype);
    *pValue = 0;
    // The types: memory is below ADDRTYPE_IO (64), an I/O port is 64..127 (the device number is in the low bits),
    // ADDRTYPE_NONE (128) and ADDRTYPE_DENY (255) have no value. ADDRTYPE_DENY has the I/O bit set too,
    // so everything from ADDRTYPE_NONE up is tested before the range of the I/O ports.
    if (addrtype >= ADDRTYPE_NONE)
    {
        _tcscpy(strValue, _T("N/A"));
        return false;
    }
    if (addrtype >= ADDRTYPE_IO)
    {
        _tcscpy(strValue, _T("I/O"));
        return false;
    }
    if (byteword)
    {
        word = (address & 1) ? (word >> 8) : (word & 0xff);
        _sntprintf(strValue, 8, _T("%03o"), (unsigned int)word);
    }
    else
        _sntprintf(strValue, 8, _T("%06o"), (unsigned int)word);
    *pValue = word;
    return true;
}

// "(label)=value", the value is read at the address
static bool Disasm_MemoryHint(LPTSTR hint, size_t hintsize, const CProcessor * pProc, const CMemoryController * pMemCtl,
        LPCTSTR label, uint16_t address, bool byteword)
{
    uint16_t value;
    TCHAR strValue[8];
    bool okValue = Disasm_ReadHintValue(pProc, pMemCtl, address, byteword, &value, strValue);
    _sntprintf(hint, hintsize - 1, _T("%s=%s"), label, strValue);
    return okValue;
}

// A deferred operand: the word at pointerAddress is the pointer, the operand is at the pointer.
// pointerHint gets "(label)=pointer"; operandHint gets "(pointer)=operand", but only if the pointer is real:
// when the pointer is "N/A" or "I/O", nothing that depends on it is shown.
static void Disasm_PointerHint(LPTSTR pointerHint, LPTSTR operandHint, size_t hintsize,
        const CProcessor * pProc, const CMemoryController * pMemCtl,
        LPCTSTR label, uint16_t pointerAddress, bool byteword)
{
    uint16_t pointer;
    TCHAR strValue[8];
    bool okPointer = Disasm_ReadHintValue(pProc, pMemCtl, pointerAddress, false, &pointer, strValue);  // the pointer is a word
    _sntprintf(pointerHint, hintsize - 1, _T("%s=%s"), label, strValue);
    if (okPointer)
    {
        TCHAR operandLabel[16];
        _sntprintf(operandLabel, 15, _T("(%06o)"), pointer);  // "(NNNNNN)=XXXXXX" or "(NNNNNN)=XXX"
        Disasm_MemoryHint(operandHint, hintsize, pProc, pMemCtl, operandLabel, pointer, byteword);
    }
}

void Disasm_RegisterHint(const CProcessor * pProc, const CMemoryController * pMemCtl, const HintRegs& regs,
        LPTSTR hint1, LPTSTR hint2, LPTSTR hint3,
        int regnum, int regmod, bool byteword, uint16_t indexval)
{
    const size_t hintsize = DISASM_HINT_PIECE_SIZE;
    uint16_t regval = regs.r[regnum];
    LPCTSTR regname = REGISTER_NAME[regnum];
    TCHAR label[16];

    _sntprintf(hint1, hintsize - 1, _T("%s=%06o"), regname, regval);  // "RN=XXXXXX"
    switch (regmod)
    {
    case 1:
    case 2:
        _sntprintf(label, 15, _T("(%s)"), regname);  // "(RN)=XXXXXX" or "(RN)=XXX"
        Disasm_MemoryHint(hint2, hintsize, pProc, pMemCtl, label, regval, byteword);
        break;
    case 3:
        _sntprintf(label, 15, _T("(%s)"), regname);  // line 2: the pointer, line 3: the operand
        Disasm_PointerHint(hint2, hint3, hintsize, pProc, pMemCtl, label, regval, byteword);
        break;
    case 4:
        {
            int step = Disasm_GetStep(regnum, regmod, byteword);
            _sntprintf(label, 15, _T("(%s-%d)"), regname, step);  // "(RN-2)=XXXXXX", "(RN-1)=XXX"
            Disasm_MemoryHint(hint2, hintsize, pProc, pMemCtl, label, (uint16_t)(regval - step), byteword);
        }
        break;
    case 5:
        _sntprintf(label, 15, _T("(%s-2)"), regname);  // line 2: the pointer, line 3: the operand
        Disasm_PointerHint(hint2, hint3, hintsize, pProc, pMemCtl, label, (uint16_t)(regval - 2), byteword);
        break;
    case 6:
        _sntprintf(label, 15, _T("(%s+%06o)"), regname, indexval);  // "(RN+NNNNNN)=XXXXXX" or "(RN+NNNNNN)=XXX"
        Disasm_MemoryHint(hint2, hintsize, pProc, pMemCtl, label, (uint16_t)(regval + indexval), byteword);
        break;
    case 7:
        _sntprintf(label, 15, _T("(%s+%06o)"), regname, indexval);  // line 2: the pointer, line 3: the operand
        Disasm_PointerHint(hint2, hint3, hintsize, pProc, pMemCtl, label, (uint16_t)(regval + indexval), byteword);
        break;
    }
}

void Disasm_RegisterHintPC(const CProcessor * pProc, const CMemoryController * pMemCtl,
        LPTSTR hint1, LPTSTR hint2,
        int regmod, bool byteword, uint16_t curaddr, uint16_t value)
{
    const size_t hintsize = DISASM_HINT_PIECE_SIZE;
    TCHAR label[16];

    //TODO: else if (regmod == 2)
    if (regmod == 3)  // @#addr: the value is at the address in the instruction
    {
        _sntprintf(label, 15, _T("(%06o)"), value);  // "(NNNNNN)=XXXXXX" or "(NNNNNN)=XXX"
        Disasm_MemoryHint(hint1, hintsize, pProc, pMemCtl, label, value, byteword);
    }
    else if (regmod == 6)  // addr: PC-relative, the PC is after the offset word
    {
        uint16_t addr2 = (uint16_t)(curaddr + value);
        _sntprintf(label, 15, _T("(%06o)"), addr2);  // "(NNNNNN)=XXXXXX" or "(NNNNNN)=XXX"
        Disasm_MemoryHint(hint1, hintsize, pProc, pMemCtl, label, addr2, byteword);
    }
    else if (regmod == 7)  // @addr: PC-relative pointer; line 1: the pointer, line 2: the operand
    {
        uint16_t addr2 = (uint16_t)(curaddr + value);
        _sntprintf(label, 15, _T("(%06o)"), addr2);
        Disasm_PointerHint(hint1, hint2, hintsize, pProc, pMemCtl, label, addr2, byteword);
    }
}

// Join the hint pieces of the source and of the destination operand into one line:
// "source, destination"; a piece that is alone is the line; equal pieces are shown once.
// dstColumn: when only the destination piece is present and the destination has a piece on line 1 too, the piece is
// aligned to the column of that piece (the length of the source piece of line 1 plus ", "); 0 = no alignment.
static void Disasm_JoinHints(LPTSTR line, const TCHAR* src, const TCHAR* dst, size_t dstColumn)
{
    if (*src != 0 && *dst != 0)
    {
        if (_tcscmp(src, dst) == 0)
            _tcscpy_s(line, DISASM_HINT_SIZE, src);
        else
            _sntprintf(line, DISASM_HINT_SIZE - 1, _T("%s, %s"), src, dst);
    }
    else if (*src != 0)
        _tcscpy_s(line, DISASM_HINT_SIZE, src);
    else if (*dst != 0)
    {
        size_t column = (dstColumn < DISASM_HINT_SIZE - 1 - _tcslen(dst)) ? dstColumn : 0;
        for (size_t i = 0; i < column; i++) line[i] = _T(' ');
        _tcscpy_s(line + column, DISASM_HINT_SIZE - column, dst);
    }
}

void Disasm_InstructionHint(const uint16_t* memory, const CProcessor * pProc, const CMemoryController * pMemCtl,
        LPTSTR buffer, LPTSTR buffer2, LPTSTR buffer3,
        int srcreg, int srcmod, int dstreg, int dstmod, bool byteword)
{
    const size_t hintsize = DISASM_HINT_PIECE_SIZE;
    TCHAR srchint1[hintsize] = { 0 }, dsthint1[hintsize] = { 0 };
    TCHAR srchint2[hintsize] = { 0 }, dsthint2[hintsize] = { 0 };
    TCHAR srchint3[hintsize] = { 0 }, dsthint3[hintsize] = { 0 };
    HintRegs regs;
    for (int i = 0; i < 8; i++)
        regs.r[i] = pProc->GetReg(i);
    const uint16_t* curmemory = memory + 1;
    uint16_t curaddr = pProc->GetPC() + 2;

    if (srcreg >= 0)
    {
        uint16_t extraWord = 0;
        if (Disasm_HasExtraWord(srcreg, srcmod))
        {
            extraWord = *(curmemory++);
            curaddr += 2;
        }
        if (srcreg == 7)
            Disasm_RegisterHintPC(pProc, pMemCtl, srchint1, srchint2, srcmod, byteword, curaddr, extraWord);
        else
        {
            Disasm_RegisterHint(pProc, pMemCtl, regs, srchint1, srchint2, srchint3, srcreg, srcmod, byteword, extraWord);
            // The destination is decoded after the autoincrement / autodecrement of the source
            Disasm_ApplySideEffect(regs, srcreg, srcmod, byteword);
        }
    }
    if (dstreg >= 0)
    {
        uint16_t extraWord = 0;
        if (Disasm_HasExtraWord(dstreg, dstmod))
        {
            extraWord = *(curmemory++);
            curaddr += 2;
        }
        if (dstreg == 7)
            Disasm_RegisterHintPC(pProc, pMemCtl, dsthint1, dsthint2, dstmod, byteword, curaddr, extraWord);
        else
            Disasm_RegisterHint(pProc, pMemCtl, regs, dsthint1, dsthint2, dsthint3, dstreg, dstmod, byteword, extraWord);
    }

    // The 1st line; equal pieces are shown once, and then the destination has no piece on the line
    Disasm_JoinHints(buffer, srchint1, dsthint1, 0);
    size_t dstColumn = 0;
    if (*srchint1 != 0 && *dsthint1 != 0 && _tcscmp(srchint1, dsthint1) != 0)
        dstColumn = _tcslen(srchint1) + 2;
    // The 2nd and the 3rd lines; a lone destination piece is aligned under the destination piece of the 1st line
    Disasm_JoinHints(buffer2, srchint2, dsthint2, dstColumn);
    Disasm_JoinHints(buffer3, srchint3, dsthint3, dstColumn);
}

// Prepare the lines of the "Instruction Hint" for a regular instruction (not a branch/jump one)
// Returns: number of hint lines, 0..3; 0 = no hints
static int Disasm_InstructionHintLines(const uint16_t* memory, const CProcessor * pProc,
        const CMemoryController * pMemCtl, DisasmHint* pHint)
{
    LPTSTR buffer = pHint->lines[0];
    LPTSTR buffer2 = pHint->lines[1];
    LPTSTR buffer3 = pHint->lines[2];
    const size_t buffersize = DISASM_HINT_SIZE;
    *buffer = 0;  *buffer2 = 0;  *buffer3 = 0;
    uint16_t instr = *memory;

    // Source and Destination
    if ((instr & ~(uint16_t)0107777) == PI_MOV || (instr & ~(uint16_t)0107777) == PI_CMP ||
        (instr & ~(uint16_t)0107777) == PI_BIT || (instr & ~(uint16_t)0107777) == PI_BIC || (instr & ~(uint16_t)0107777) == PI_BIS ||
        (instr & ~(uint16_t)0007777) == PI_ADD || (instr & ~(uint16_t)0007777) == PI_SUB)
    {
        int srcreg = (instr >> 6) & 7;
        int srcmod = (instr >> 9) & 7;
        int dstreg = instr & 7;
        int dstmod = (instr >> 3) & 7;
        // The bit 15 is the byte flag, except for SUB (0160000), which is a word operation
        bool okByte = (instr & 0100000) != 0 && (instr & ~(uint16_t)0007777) != PI_SUB;
        Disasm_InstructionHint(memory, pProc, pMemCtl, buffer, buffer2, buffer3, srcreg, srcmod, dstreg, dstmod, okByte);
    }

    // Register and Destination
    if ((instr & ~(uint16_t)0777) == PI_MUL || (instr & ~(uint16_t)0777) == PI_DIV ||
        (instr & ~(uint16_t)0777) == PI_ASH || (instr & ~(uint16_t)0777) == PI_ASHC ||
        (instr & ~(uint16_t)0777) == PI_XOR)
    {
        int srcreg = (instr >> 6) & 7;
        int dstreg = instr & 7;
        int dstmod = (instr >> 3) & 7;
        Disasm_InstructionHint(memory, pProc, pMemCtl, buffer, buffer2, buffer3, srcreg, 0, dstreg, dstmod, false);
    }

    // Destination only
    if ((instr & ~(uint16_t)0100077) == PI_CLR || (instr & ~(uint16_t)0100077) == PI_COM ||
        (instr & ~(uint16_t)0100077) == PI_INC || (instr & ~(uint16_t)0100077) == PI_DEC || (instr & ~(uint16_t)0100077) == PI_NEG ||
        (instr & ~(uint16_t)0100077) == PI_TST ||
        (instr & ~(uint16_t)0100077) == PI_ASR || (instr & ~(uint16_t)0100077) == PI_ASL ||
        (instr & ~(uint16_t)077) == PI_SWAB || (instr & ~(uint16_t)077) == PI_SXT ||
        (instr & ~(uint16_t)077) == PI_MTPS || (instr & ~(uint16_t)077) == PI_MFPS)
    {
        int dstreg = instr & 7;
        int dstmod = (instr >> 3) & 7;
        // The bit 15 is the byte flag: CLRB...ASLB, MTPS and MFPS are byte operations, SWAB and SXT are not
        Disasm_InstructionHint(memory, pProc, pMemCtl, buffer, buffer2, buffer3, -1, -1, dstreg, dstmod, (instr & 0100000) != 0);
    }

    // ADC, SBC, ROR, ROL: destination only, and also show C flag
    if ((instr & ~(uint16_t)0100077) == PI_ADC || (instr & ~(uint16_t)0100077) == PI_SBC ||
        (instr & ~(uint16_t)0100077) == PI_ROR || (instr & ~(uint16_t)0100077) == PI_ROL)
    {
        int dstreg = instr & 7;
        int dstmod = (instr >> 3) & 7;
        if (dstreg != 7)
        {
            Disasm_InstructionHint(memory, pProc, pMemCtl, buffer, buffer2, buffer3, -1, -1, dstreg, dstmod, (instr & 0100000) != 0);
            uint16_t psw = pProc->GetPSW();
            size_t length = _tcslen(buffer);
            _sntprintf(buffer + length, buffersize - 1 - length, _T(", C=%c"), (psw & PSW_C) ? '1' : '0');  // "..., C=X"
        }
    }

    // CLC..CCC, SEC..SCC -- show flags
    if (instr >= 0000241 && instr <= 0000257 || instr >= 0000261 && instr <= 0000277)
    {
        uint16_t psw = pProc->GetPSW();
        _sntprintf(buffer, buffersize - 1, _T("C=%c, V=%c, Z=%c, N=%c"),
                (psw & PSW_C) ? '1' : '0', (psw & PSW_V) ? '1' : '0', (psw & PSW_Z) ? '1' : '0', (psw & PSW_N) ? '1' : '0');
    }

    // JSR, JMP -- show non-trivial cases only
    if ((instr & ~(uint16_t)0777) == PI_JSR && (instr & 077) != 067 && (instr & 077) != 037 ||
        (instr & ~(uint16_t)077) == PI_JMP && (instr & 077) != 067 && (instr & 077) != 037)
    {
        int dstreg = instr & 7;
        int dstmod = (instr >> 3) & 7;
        Disasm_InstructionHint(memory, pProc, pMemCtl, buffer, buffer2, buffer3, -1, -1, dstreg, dstmod, false);
    }

    // HALT mode commands
    if (instr == PI_MFUS)
    {
        _sntprintf(buffer, buffersize - 1, _T("R5=%06o, R0=%06o"), pProc->GetReg(5), pProc->GetReg(0));  // "R5=XXXXXX, R0=XXXXXX"
    }
    else if (instr == PI_MTUS)
    {
        _sntprintf(buffer, buffersize - 1, _T("R0=%06o, R5=%06o"), pProc->GetReg(0), pProc->GetReg(5));  // "R0=XXXXXX, R5=XXXXXX"
    }
    else if (instr == 0000022 || instr == 0000023)  // RCPC / MFPC
    {
        _sntprintf(buffer, buffersize - 1, _T("PC'=%06o, R0=%06o"), pProc->GetCPC(), pProc->GetReg(0));  // "PC'=XXXXXX, R0=XXXXXX"
    }
    else if (instr >= 0000024 && instr <= 0000027)  // RCPS / MFPS
    {
        _sntprintf(buffer, buffersize - 1, _T("PS'=%06o, R0=%06o"), pProc->GetCPSW(), pProc->GetReg(0));  // "PS'=XXXXXX, R0=XXXXXX"
    }
    else if (instr == PI_RSEL)
    {
        _sntprintf(buffer, buffersize - 1, _T("SEL=%06o, R0=%06o"), pMemCtl->GetSelRegister(), pProc->GetReg(0));  // "SEL=XXXXXX, R0=XXXXXX"
    }

    if ((instr & ~(uint16_t)077) == PI_MARK)
    {
        uint16_t regval = pProc->GetReg(6);
        _sntprintf(buffer, buffersize - 1, _T("SP=%06o, R5=%06o"), regval, pProc->GetReg(5));  // "SP=XXXXXX, R5=XXXXXX"
        int addrtype = 0;
        uint16_t srcval2 = pMemCtl->GetWordView(regval, pProc->IsHaltMode(), false, &addrtype);
        _sntprintf(buffer2, buffersize - 1, _T("(SP)=%06o"), srcval2);  // "(SP)=XXXXXX"
    }

    int result = 0;
    if (*buffer != 0)
        result = 1;
    if (*buffer2 != 0)
        result = 2;
    if (*buffer3 != 0)
        result = 3;
    return result;
}

// Prepare "Instruction Hint" for a regular instruction (not a branch/jump one)
// buffer, buffer2 - buffers for 1st and 2nd lines of the instruction hint, min size 42
// Returns: number of hint lines, at most 2; 0 = no hints
// Kept for the callers that have two lines; Disasm_GetHint() gives up to three lines
int Disasm_GetInstructionHint(const uint16_t* memory, const CProcessor * pProc,
        const CMemoryController * pMemCtl,
        LPTSTR buffer, LPTSTR buffer2)
{
    DisasmHint hint;
    int count = Disasm_InstructionHintLines(memory, pProc, pMemCtl, &hint);
    _tcsncpy_s(buffer, 42, hint.lines[0], _TRUNCATE);
    _tcsncpy_s(buffer2, 42, hint.lines[1], _TRUNCATE);
    return (count > 2) ? 2 : count;
}

void Disasm_GetHint(const uint16_t* memory, const CProcessor * pProc, const CMemoryController * pMemCtl, DisasmHint* pHint)
{
    for (int i = 0; i < DISASM_HINT_LINES; i++)
        pHint->lines[i][0] = 0;
    pHint->count = 0;
    pHint->isJump = false;

    // The jump hint (also gives the prediction); if the instruction has none, the instruction hint
    pHint->jumpPredict = Disasm_GetJumpConditionHint(memory, pProc, pMemCtl, pHint->lines[0]);
    if (pHint->lines[0][0] != 0)
    {
        pHint->isJump = true;
        pHint->count = 1;
        return;
    }
    pHint->count = Disasm_InstructionHintLines(memory, pProc, pMemCtl, pHint);
}
