// Disasm.cpp
// See defines in header file Emubase.h

#include "StdAfx.h"
#include "Disasm.h"
#include "Defines.h"

//NOTE: I know, we use unsafe string copy functions
#pragma warning( disable: 4996 )

// Формат отображения режимов адресации
const LPCTSTR ADDRESS_MODE_FORMAT[] = {
        _T("%s"), _T("(%s)"), _T("(%s)+"), _T("@(%s)+"), _T("-(%s)"), _T("@-(%s)"), _T("%06o(%s)"), _T("@%06o(%s)") };
// Формат отображения режимов адресации для регистра PC
const LPCTSTR ADDRESS_MODE_PC_FORMAT[] = {
        _T("PC"), _T("(PC)"), _T("#%06o"), _T("@#%06o"), _T("-(PC)"), _T("@-(PC)"), _T("%06o"), _T("@%06o") };

//   strSrc - at least 24 characters
BOOL ConvertSrcToString(WORD instr, WORD addr, TCHAR* strSrc, WORD code) {
    int reg = GetDigit(instr, 2);
    int param = GetDigit(instr, 3);

    LPCTSTR pszReg = REGISTER_NAME[reg];

    if (reg != 7) {
        LPCTSTR format = ADDRESS_MODE_FORMAT[param];

        if (param == 6 || param == 7) {
            WORD word = code;  //TODO: pMemory
            _sntprintf(strSrc, 24, format, word, pszReg);
            return TRUE;
        }
        else
            _sntprintf(strSrc, 24, format, pszReg);
    }
    else {
        LPCTSTR format = ADDRESS_MODE_PC_FORMAT[param];

        if (param == 2 || param == 3) {
            WORD word = code;  //TODO: pMemory
            _sntprintf(strSrc, 24, format, word);
            return TRUE;
        }
        else if (param == 6 || param == 7) {
            WORD word = code;  //TODO: pMemory
            _sntprintf(strSrc, 24, format, (WORD)(addr + word + 2));
            return TRUE;
        }
        else
            _sntprintf(strSrc, 24, format, pszReg);
    }

    return FALSE;
}

//   strDst - at least 24 characters
BOOL ConvertDstToString (WORD instr, WORD addr, TCHAR* strDst, WORD code) {
    int reg = GetDigit(instr, 0);
    int param = GetDigit(instr, 1);

    LPCTSTR pszReg = REGISTER_NAME[reg];

    if (reg != 7) {
        LPCTSTR format = ADDRESS_MODE_FORMAT[param];

        if (param == 6 || param == 7) {
            _sntprintf(strDst, 24, format, code, pszReg);
            return TRUE;
        }
        else
            _sntprintf(strDst, 24, format, pszReg);
    }
    else {
        LPCTSTR format = ADDRESS_MODE_PC_FORMAT[param];

        if (param == 2 || param == 3) {
            _sntprintf(strDst, 24, format, code);
            return TRUE;
        }
        else if (param == 6 || param == 7) {
            _sntprintf(strDst, 24, format, (WORD)(addr + code + 2));
            return TRUE;
        }
        else
            _sntprintf(strDst, 24, format, pszReg);
    }

    return FALSE;
}

// Disassemble one instruction
//   pMemory - memory image (we read only words of the instruction)
//   sInstr  - instruction mnemonics buffer - at least 8 characters
//   sArg    - instruction arguments buffer - at least 32 characters
//   Return value: number of words in the instruction
int DisassembleInstruction(WORD* pMemory, WORD addr, TCHAR* strInstr, TCHAR* strArg)
{
    *strInstr = 0;
    *strArg = 0;

    WORD instr = *pMemory;

    int length = 1;
    LPCTSTR strReg = NULL;
    TCHAR strSrc[24];
    TCHAR strDst[24];
    BOOL okByte;

    // No fields
    switch (instr) {
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
        case PI_GO:     _tcscpy(strInstr, _T("GO"));     return 1;
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
    if ((instr & ~(WORD)7) == PI_RTS) {
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

    switch (instr & ~(WORD)077) {
        case PI_JMP:    _tcscpy(strInstr, _T("JMP"));   _tcscpy(strArg, strDst);  return length;
        case PI_SWAB:   _tcscpy(strInstr, _T("SWAB"));  _tcscpy(strArg, strDst);  return length;
        case PI_MARK:   _tcscpy(strInstr, _T("MARK"));  _tcscpy(strArg, strDst);  return length;
        case PI_SXT:    _tcscpy(strInstr, _T("SXT"));   _tcscpy(strArg, strDst);  return length;
        case PI_MTPS:   _tcscpy(strInstr, _T("MTPS"));  _tcscpy(strArg, strDst);  return length;
        case PI_MFPS:   _tcscpy(strInstr, _T("MFPS"));  _tcscpy(strArg, strDst);  return length;
    }
    
    okByte = (instr & 0100000);

    switch (instr & ~(WORD)0100077) {
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
    _sntprintf(strDst, 24, _T("%06o"), addr + ((short)(char)LOBYTE (instr) * 2) + 2);

    // Branchs & interrupts
    switch (instr & ~(WORD)0377) {
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

    _sntprintf(strDst, 24, _T("%06o"), LOBYTE (instr));

    switch (instr & ~(WORD)0377) {
        case PI_EMT:   _tcscpy(strInstr, _T("EMT"));   _tcscpy(strArg, strDst);  return 1;
        case PI_TRAP:  _tcscpy(strInstr, _T("TRAP"));  _tcscpy(strArg, strDst);  return 1;
    }

    // Three fields
    switch(instr & ~(WORD)0777) {
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
        _sntprintf(strArg, 32, _T("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return length;
	case PI_DIV:
        _tcscpy(strInstr, _T("DIV"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
        _sntprintf(strArg, 32, _T("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return length;
	case PI_ASH:
        _tcscpy(strInstr, _T("ASH"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
        _sntprintf(strArg, 32, _T("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return length;
	case PI_ASHC:
        _tcscpy(strInstr, _T("ASHC"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
        _sntprintf(strArg, 32, _T("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
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

    okByte = (instr & 0100000);

    length += ConvertSrcToString(instr, addr + 2, strSrc, pMemory[1]);
    length += ConvertDstToString(instr, addr + 2 + (length - 1) * 2, strDst, pMemory[length]);

    switch(instr & ~(WORD)0107777) {
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

    switch (instr & ~(WORD)0007777) {
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
