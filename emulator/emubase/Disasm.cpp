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


// Формат отображения режимов адресации
const LPCTSTR ADDRESS_MODE_FORMAT[] =
{
    _T("%s"), _T("(%s)"), _T("(%s)+"), _T("@(%s)+"), _T("-(%s)"), _T("@-(%s)"), _T("%06o(%s)"), _T("@%06o(%s)")
};
// Формат отображения режимов адресации для регистра PC
const LPCTSTR ADDRESS_MODE_PC_FORMAT[] =
{
    _T("PC"), _T("(PC)"), _T("#%06o"), _T("@#%06o"), _T("-(PC)"), _T("@-(PC)"), _T("%06o"), _T("@%06o")
};

//   strSrc - at least 24 characters
uint16_t ConvertSrcToString(uint16_t instr, uint16_t addr, TCHAR* strSrc, uint16_t code)
{
    uint8_t reg = GetDigit(instr, 2);
    uint8_t param = GetDigit(instr, 3);

    LPCTSTR pszReg = REGISTER_NAME[reg];

    if (reg != 7)
    {
        LPCTSTR format = ADDRESS_MODE_FORMAT[param];

        if (param == 6 || param == 7)
        {
            uint16_t word = code;  //TODO: pMemory
            _sntprintf(strSrc, 24, format, word, pszReg);
            return 1;
        }
        else
            _sntprintf(strSrc, 24, format, pszReg);
    }
    else
    {
        LPCTSTR format = ADDRESS_MODE_PC_FORMAT[param];

        if (param == 2 || param == 3)
        {
            uint16_t word = code;  //TODO: pMemory
            _sntprintf(strSrc, 24, format, word);
            return 1;
        }
        else if (param == 6 || param == 7)
        {
            uint16_t word = code;  //TODO: pMemory
            _sntprintf(strSrc, 24, format, (uint16_t)(addr + word + 2));
            return 1;
        }
        else
            _sntprintf(strSrc, 24, format, pszReg);
    }

    return 0;
}

//   strDst - at least 24 characters
uint16_t ConvertDstToString (uint16_t instr, uint16_t addr, TCHAR* strDst, uint16_t code)
{
    uint8_t reg = GetDigit(instr, 0);
    uint8_t param = GetDigit(instr, 1);

    LPCTSTR pszReg = REGISTER_NAME[reg];

    if (reg != 7)
    {
        LPCTSTR format = ADDRESS_MODE_FORMAT[param];

        if (param == 6 || param == 7)
        {
            _sntprintf(strDst, 24, format, code, pszReg);
            return 1;
        }
        else
            _sntprintf(strDst, 24, format, pszReg);
    }
    else
    {
        LPCTSTR format = ADDRESS_MODE_PC_FORMAT[param];

        if (param == 2 || param == 3)
        {
            _sntprintf(strDst, 24, format, code);
            return 1;
        }
        else if (param == 6 || param == 7)
        {
            _sntprintf(strDst, 24, format, (uint16_t)(addr + code + 2));
            return 1;
        }
        else
            _sntprintf(strDst, 24, format, pszReg);
    }

    return 0;
}

// Disassemble one instruction
//   pMemory - memory image (we read only words of the instruction)
//   sInstr  - instruction mnemonics buffer - at least 8 characters
//   sArg    - instruction arguments buffer - at least 32 characters
//   Return value: number of words in the instruction
uint16_t DisassembleInstruction(uint16_t* pMemory, uint16_t addr, TCHAR* strInstr, TCHAR* strArg)
{
    *strInstr = 0;
    *strArg = 0;

    uint16_t instr = *pMemory;

    uint16_t length = 1;
    LPCTSTR strReg = nullptr;
    TCHAR strSrc[24];
    TCHAR strDst[24];
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

        // Спецкоманды режима HALT ВМ2
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
        }
        else
        {
            _tcscpy(strInstr, _T("RTS"));
            _tcscpy(strArg, REGISTER_NAME[GetDigit(instr, 0)]);
        }
        return 1;
    }

    // Two fields
    length += ConvertDstToString(instr, addr + 2, strDst, pMemory[1]);

    switch (instr & ~(uint16_t)077)
    {
    case PI_JMP:    _tcscpy(strInstr, _T("JMP"));   _tcscpy(strArg, strDst);  return length;
    case PI_SWAB:   _tcscpy(strInstr, _T("SWAB"));  _tcscpy(strArg, strDst);  return length;
    case PI_MARK:   _tcscpy(strInstr, _T("MARK"));  _tcscpy(strArg, strDst);  return length;
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
    _sntprintf(strDst, 24, _T("%06o"), addr + ((short)(char)(uint8_t)(instr & 0xff) * 2) + 2);

    // Branchs & interrupts
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

    _sntprintf(strDst, 24, _T("%06o"), (uint8_t)(instr & 0xff));

    switch (instr & ~(uint16_t)0377)
    {
    case PI_EMT:   _tcscpy(strInstr, _T("EMT"));   _tcscpy(strArg, strDst);  return 1;
    case PI_TRAP:  _tcscpy(strInstr, _T("TRAP"));  _tcscpy(strArg, strDst);  return 1;
    }

    // Three fields
    switch (instr & ~(uint16_t)0777)
    {
    case PI_JSR:
        if (GetDigit(instr, 2) == 7)
        {
            _tcscpy(strInstr, _T("CALL"));
            length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
            _sntprintf(strArg, 32, _T("%s"), strDst);  // strArg = strDst;
        }
        else
        {
            _tcscpy(strInstr, _T("JSR"));
            strReg = REGISTER_NAME[GetDigit(instr, 2)];
            length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
            _sntprintf(strArg, 32, _T("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        }
        return length;
    case PI_MUL:
        _tcscpy(strInstr, _T("MUL"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
        _sntprintf(strArg, 32, _T("%s, %s"), strDst, strReg);  // strArg = strDst + ", " + strReg;
        return length;
    case PI_DIV:
        _tcscpy(strInstr, _T("DIV"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
        _sntprintf(strArg, 32, _T("%s, %s"), strDst, strReg);  // strArg = strDst + ", " + strReg;
        return length;
    case PI_ASH:
        _tcscpy(strInstr, _T("ASH"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
        _sntprintf(strArg, 32, _T("%s, %s"), strDst, strReg);  // strArg = strDst + ", " + strReg;
        return length;
    case PI_ASHC:
        _tcscpy(strInstr, _T("ASHC"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
        _sntprintf(strArg, 32, _T("%s, %s"), strDst, strReg);  // strArg = strDst + ", " + strReg;
        return length;
    case PI_XOR:
        _tcscpy(strInstr, _T("XOR"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
        _sntprintf(strArg, 32, _T("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return length;
    case PI_SOB:
        _tcscpy(strInstr, _T("SOB"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        _sntprintf(strDst, 24, _T("%06o"), addr - (GetDigit(instr, 1) * 8 + GetDigit(instr, 0)) * 2 + 2);
        _sntprintf(strArg, 32, _T("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return 1;
    }

    // Four fields

    okByte = (instr & 0100000) != 0;

    length += ConvertSrcToString(instr, addr + 2, strSrc, pMemory[1]);
    length += ConvertDstToString(instr, (uint16_t)(addr + 2 + (length - 1) * 2), strDst, pMemory[length]);

    switch (instr & ~(uint16_t)0107777)
    {
    case PI_MOV:
        _tcscpy(strInstr, okByte ? _T("MOVB") : _T("MOV"));
        _sntprintf(strArg, 32, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_CMP:
        _tcscpy(strInstr, okByte ? _T("CMPB") : _T("CMP"));
        _sntprintf(strArg, 32, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_BIT:
        _tcscpy(strInstr, okByte ? _T("BITB") : _T("BIT"));
        _sntprintf(strArg, 32, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_BIC:
        _tcscpy(strInstr, okByte ? _T("BICB") : _T("BIC"));
        _sntprintf(strArg, 32, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_BIS:
        _tcscpy(strInstr, okByte ? _T("BISB") : _T("BIS"));
        _sntprintf(strArg, 32, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    }

    switch (instr & ~(uint16_t)0007777)
    {
    case PI_ADD:
        _tcscpy(strInstr, _T("ADD"));
        _sntprintf(strArg, 32, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_SUB:
        _tcscpy(strInstr, _T("SUB"));
        _sntprintf(strArg, 32, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    }

    _tcscpy(strInstr, _T("unknown"));
    _sntprintf(strArg, 32, _T("%06o"), instr);
    return 1;
}
