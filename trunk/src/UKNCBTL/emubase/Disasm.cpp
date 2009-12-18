// Disasm.cpp
// See defines in header file Emubase.h

#include "StdAfx.h"
#include "Disasm.h"
#include "Defines.h"

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
            _snwprintf_s(strSrc, 24,24, format, word, pszReg);
            return TRUE;
        }
        else
            _snwprintf_s(strSrc, 24,24, format, pszReg);
    }
    else {
        LPCTSTR format = ADDRESS_MODE_PC_FORMAT[param];

        if (param == 2 || param == 3) {
            WORD word = code;  //TODO: pMemory
            _snwprintf_s(strSrc, 24,24, format, word);
            return TRUE;
        }
        else if (param == 6 || param == 7) {
            WORD word = code;  //TODO: pMemory
            _snwprintf_s(strSrc, 24,24, format, (WORD)(addr + word + 2));
            return TRUE;
        }
        else
            _snwprintf_s(strSrc, 24,24, format, pszReg);
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
            _snwprintf_s(strDst, 24,24, format, code, pszReg);
            return TRUE;
        }
        else
            _snwprintf_s(strDst, 24,24, format, pszReg);
    }
    else {
        LPCTSTR format = ADDRESS_MODE_PC_FORMAT[param];

        if (param == 2 || param == 3) {
            _snwprintf_s(strDst, 24,24, format, code);
            return TRUE;
        }
        else if (param == 6 || param == 7) {
            _snwprintf_s(strDst, 24,24, format, (WORD)(addr + code + 2));
            return TRUE;
        }
        else
            _snwprintf_s(strDst, 24,24, format, pszReg);
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
    LPCTSTR strReg;
    TCHAR strSrc[24];
    TCHAR strDst[24];
    BOOL okByte;

    // No fields
    switch (instr) {
        case PI_HALT:   wcscpy_s(strInstr, 8, _T("HALT"));   return 1;
        case PI_WAIT:   wcscpy_s(strInstr, 8, _T("WAIT"));   return 1;
        case PI_RTI:    wcscpy_s(strInstr, 8, _T("RTI"));    return 1;
        case PI_BPT:    wcscpy_s(strInstr, 8, _T("BPT"));    return 1;
        case PI_IOT:    wcscpy_s(strInstr, 8, _T("IOT"));    return 1;
        case PI_RESET:  wcscpy_s(strInstr, 8, _T("RESET"));  return 1;
        case PI_RTT:    wcscpy_s(strInstr, 8, _T("RTT"));    return 1;
        case PI_NOP:    wcscpy_s(strInstr, 8, _T("NOP"));    return 1;
        case PI_CLC:    wcscpy_s(strInstr, 8, _T("CLC"));    return 1;
        case PI_CLV:    wcscpy_s(strInstr, 8, _T("CLV"));    return 1;
        case PI_CLVC:   wcscpy_s(strInstr, 8, _T("CLVC"));   return 1;
        case PI_CLZ:    wcscpy_s(strInstr, 8, _T("CLZ"));    return 1;
        case PI_CLZC:   wcscpy_s(strInstr, 8, _T("CLZC"));   return 1;
        case PI_CLZV:   wcscpy_s(strInstr, 8, _T("CLZV"));   return 1;
        case PI_CLZVC:  wcscpy_s(strInstr, 8, _T("CLZVC"));  return 1;
        case PI_CLN:    wcscpy_s(strInstr, 8, _T("CLN"));    return 1;
        case PI_CLNC:   wcscpy_s(strInstr, 8, _T("CLNC"));   return 1;
        case PI_CLNV:   wcscpy_s(strInstr, 8, _T("CLNV"));   return 1;
        case PI_CLNVC:  wcscpy_s(strInstr, 8, _T("CLNVC"));  return 1;
        case PI_CLNZ:   wcscpy_s(strInstr, 8, _T("CLNZ"));   return 1;
        case PI_CLNZC:  wcscpy_s(strInstr, 8, _T("CLNZC"));  return 1;
        case PI_CLNZV:  wcscpy_s(strInstr, 8, _T("CLNZV"));  return 1;
        case PI_CCC:    wcscpy_s(strInstr, 8, _T("CCC"));    return 1;
        case PI_NOP260: wcscpy_s(strInstr, 8, _T("NOP260")); return 1;
        case PI_SEC:    wcscpy_s(strInstr, 8, _T("SEC"));    return 1;
        case PI_SEV:    wcscpy_s(strInstr, 8, _T("SEV"));    return 1;
        case PI_SEVC:   wcscpy_s(strInstr, 8, _T("SEVC"));   return 1;
        case PI_SEZ:    wcscpy_s(strInstr, 8, _T("SEZ"));    return 1;
        case PI_SEZC:   wcscpy_s(strInstr, 8, _T("SEZC"));   return 1;
        case PI_SEZV:   wcscpy_s(strInstr, 8, _T("SEZV"));   return 1;
        case PI_SEZVC:  wcscpy_s(strInstr, 8, _T("SEZVC"));  return 1;
        case PI_SEN:    wcscpy_s(strInstr, 8, _T("SEN"));    return 1;
        case PI_SENC:   wcscpy_s(strInstr, 8, _T("SENC"));   return 1;
        case PI_SENV:   wcscpy_s(strInstr, 8, _T("SENV"));   return 1;
        case PI_SENVC:  wcscpy_s(strInstr, 8, _T("SENVC"));  return 1;
        case PI_SENZ:   wcscpy_s(strInstr, 8, _T("SENZ"));   return 1;
        case PI_SENZC:  wcscpy_s(strInstr, 8, _T("SENZC"));  return 1;
        case PI_SENZV:  wcscpy_s(strInstr, 8, _T("SENZV"));  return 1;
        case PI_SCC:    wcscpy_s(strInstr, 8, _T("SCC"));    return 1;

        // Спецкоманды режима HALT ВМ2
        case PI_GO:     wcscpy_s(strInstr, 8, _T("GO"));     return 1;
        case PI_STEP:   wcscpy_s(strInstr, 8, _T("STEP"));   return 1;
        case PI_RSEL:   wcscpy_s(strInstr, 8, _T("RSEL"));   return 1;
        case PI_MFUS:   wcscpy_s(strInstr, 8, _T("MFUS"));   return 1;
        case PI_RCPC:   wcscpy_s(strInstr, 8, _T("RCPC"));   return 1;
        case PI_RCPS:   wcscpy_s(strInstr, 8, _T("RCPS"));   return 1;
        case PI_MTUS:   wcscpy_s(strInstr, 8, _T("MTUS"));   return 1;
        case PI_WCPC:   wcscpy_s(strInstr, 8, _T("WCPC"));   return 1;
        case PI_WCPS:   wcscpy_s(strInstr, 8, _T("WCPS"));   return 1;
    }

    // One field
    if ((instr & ~(WORD)7) == PI_RTS) {
        wcscpy_s(strInstr, 8, _T("RTS"));
        wcscpy_s(strArg, 32, REGISTER_NAME[GetDigit(instr, 0)]);
        return 1;
    }

    // Two fields
    length += ConvertDstToString(instr, addr + 2, strDst, pMemory[1]);

    switch (instr & ~(WORD)077) {
        case PI_JMP:    wcscpy_s(strInstr, 8, _T("JMP"));   wcscpy_s(strArg, 32, strDst);  return length;
        case PI_SWAB:   wcscpy_s(strInstr, 8, _T("SWAB"));  wcscpy_s(strArg, 32, strDst);  return length;
        case PI_MARK:   wcscpy_s(strInstr, 8, _T("MARK"));  wcscpy_s(strArg, 32, strDst);  return length;
        case PI_SXT:    wcscpy_s(strInstr, 8, _T("SXT"));   wcscpy_s(strArg, 32, strDst);  return length;
        case PI_MTPS:   wcscpy_s(strInstr, 8, _T("MTPS"));  wcscpy_s(strArg, 32, strDst);  return length;
        case PI_MFPS:   wcscpy_s(strInstr, 8, _T("MFPS"));  wcscpy_s(strArg, 32, strDst);  return length;
    }
    
    okByte = (instr & 0100000);

    switch (instr & ~(WORD)0100077) {
        case PI_CLR:  wcscpy_s(strInstr, 8, okByte ? _T("CLRB") : _T("CLR"));  wcscpy_s(strArg, 32, strDst);  return length;
        case PI_COM:  wcscpy_s(strInstr, 8, okByte ? _T("COMB") : _T("COM"));  wcscpy_s(strArg, 32, strDst);  return length;
        case PI_INC:  wcscpy_s(strInstr, 8, okByte ? _T("INCB") : _T("INC"));  wcscpy_s(strArg, 32, strDst);  return length;
        case PI_DEC:  wcscpy_s(strInstr, 8, okByte ? _T("DECB") : _T("DEC"));  wcscpy_s(strArg, 32, strDst);  return length;
        case PI_NEG:  wcscpy_s(strInstr, 8, okByte ? _T("NEGB") : _T("NEG"));  wcscpy_s(strArg, 32, strDst);  return length;
        case PI_ADC:  wcscpy_s(strInstr, 8, okByte ? _T("ADCB") : _T("ADC"));  wcscpy_s(strArg, 32, strDst);  return length;
        case PI_SBC:  wcscpy_s(strInstr, 8, okByte ? _T("SBCB") : _T("SBC"));  wcscpy_s(strArg, 32, strDst);  return length;
        case PI_TST:  wcscpy_s(strInstr, 8, okByte ? _T("TSTB") : _T("TST"));  wcscpy_s(strArg, 32, strDst);  return length;
        case PI_ROR:  wcscpy_s(strInstr, 8, okByte ? _T("RORB") : _T("ROR"));  wcscpy_s(strArg, 32, strDst);  return length;
        case PI_ROL:  wcscpy_s(strInstr, 8, okByte ? _T("ROLB") : _T("ROL"));  wcscpy_s(strArg, 32, strDst);  return length;
        case PI_ASR:  wcscpy_s(strInstr, 8, okByte ? _T("ASRB") : _T("ASR"));  wcscpy_s(strArg, 32, strDst);  return length;
        case PI_ASL:  wcscpy_s(strInstr, 8, okByte ? _T("ASLB") : _T("ASL"));  wcscpy_s(strArg, 32, strDst);  return length;
    }

    length = 1;
    _snwprintf_s(strDst, 24,24, _T("%06o"), addr + ((short)(char)LOBYTE (instr) * 2) + 2);

    // Branchs & interrupts
    switch (instr & ~(WORD)0377) {
        case PI_BR:   wcscpy_s(strInstr, 8, _T("BR"));   wcscpy_s(strArg, 32, strDst);  return 1;
        case PI_BNE:  wcscpy_s(strInstr, 8, _T("BNE"));  wcscpy_s(strArg, 32, strDst);  return 1;
        case PI_BEQ:  wcscpy_s(strInstr, 8, _T("BEQ"));  wcscpy_s(strArg, 32, strDst);  return 1;
        case PI_BGE:  wcscpy_s(strInstr, 8, _T("BGE"));  wcscpy_s(strArg, 32, strDst);  return 1;
        case PI_BLT:  wcscpy_s(strInstr, 8, _T("BLT"));  wcscpy_s(strArg, 32, strDst);  return 1;
        case PI_BGT:  wcscpy_s(strInstr, 8, _T("BGT"));  wcscpy_s(strArg, 32, strDst);  return 1;
        case PI_BLE:  wcscpy_s(strInstr, 8, _T("BLE"));  wcscpy_s(strArg, 32, strDst);  return 1;
        case PI_BPL:  wcscpy_s(strInstr, 8, _T("BPL"));  wcscpy_s(strArg, 32, strDst);  return 1;
        case PI_BMI:  wcscpy_s(strInstr, 8, _T("BMI"));  wcscpy_s(strArg, 32, strDst);  return 1;
        case PI_BHI:  wcscpy_s(strInstr, 8, _T("BHI"));  wcscpy_s(strArg, 32, strDst);  return 1;
        case PI_BLOS:  wcscpy_s(strInstr, 8, _T("BLOS"));  wcscpy_s(strArg, 32, strDst);  return 1;
        case PI_BVC:  wcscpy_s(strInstr, 8, _T("BVC"));  wcscpy_s(strArg, 32, strDst);  return 1;
        case PI_BVS:  wcscpy_s(strInstr, 8, _T("BVS"));  wcscpy_s(strArg, 32, strDst);  return 1;
        case PI_BHIS:  wcscpy_s(strInstr, 8, _T("BHIS"));  wcscpy_s(strArg, 32, strDst);  return 1;
        case PI_BLO:  wcscpy_s(strInstr, 8, _T("BLO"));  wcscpy_s(strArg, 32, strDst);  return 1;
    }

    _snwprintf_s(strDst, 24,24, _T("%06o"), LOBYTE (instr));

    switch (instr & ~(WORD)0377) {
        case PI_EMT:   wcscpy_s(strInstr, 8, _T("EMT"));   wcscpy_s(strArg, 32, strDst);  return 1;
        case PI_TRAP:  wcscpy_s(strInstr, 8, _T("TRAP"));  wcscpy_s(strArg, 32, strDst);  return 1;
    }

    // Three fields
    switch(instr & ~(WORD)0777) {
    case PI_JSR:
        wcscpy_s(strInstr, 8, _T("JSR"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
        _snwprintf_s(strArg, 32,32, _T("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return length;
	case PI_MUL:
        wcscpy_s(strInstr, 8, _T("MUL"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
        _snwprintf_s(strArg, 32,32, _T("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return length;
	case PI_DIV:
        wcscpy_s(strInstr, 8, _T("DIV"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
        _snwprintf_s(strArg, 32,32, _T("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return length;
	case PI_ASH:
        wcscpy_s(strInstr, 8, _T("ASH"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
        _snwprintf_s(strArg, 32,32, _T("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return length;
	case PI_ASHC:
        wcscpy_s(strInstr, 8, _T("ASHC"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
        _snwprintf_s(strArg, 32,32, _T("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return length;
    case PI_XOR:
        wcscpy_s(strInstr, 8, _T("XOR"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
        _snwprintf_s(strArg, 32,32, _T("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return length;
    case PI_SOB:
        wcscpy_s(strInstr, 8, _T("SOB"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        _snwprintf_s(strDst, 24,24, _T("%06o"), addr - (GetDigit(instr, 1) * 8 + GetDigit(instr, 0)) * 2 + 2);
        _snwprintf_s(strArg, 32,32, _T("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return 1;
    }

    // Four fields

    okByte = (instr & 0100000);

    length += ConvertSrcToString(instr, addr + 2, strSrc, pMemory[1]);
    length += ConvertDstToString(instr, addr + 2 + (length - 1) * 2, strDst, pMemory[length]);

    switch(instr & ~(WORD)0107777) {
    case PI_MOV:
        wcscpy_s(strInstr, 8, okByte ? _T("MOVB") : _T("MOV"));
        _snwprintf_s(strArg, 32,32, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_CMP:
        wcscpy_s(strInstr, 8, okByte ? _T("CMPB") : _T("CMP"));
        _snwprintf_s(strArg, 32,32, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_BIT:
        wcscpy_s(strInstr, 8, okByte ? _T("BITB") : _T("BIT"));
        _snwprintf_s(strArg, 32,32, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_BIC:
        wcscpy_s(strInstr, 8, okByte ? _T("BICB") : _T("BIC"));
        _snwprintf_s(strArg, 32,32, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_BIS:
        wcscpy_s(strInstr, 8, okByte ? _T("BISB") : _T("BIS"));
        _snwprintf_s(strArg, 32,32, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    }

    switch (instr & ~(WORD)0007777) {
    case PI_ADD:
		wcscpy_s(strInstr, 8, _T("ADD"));
        _snwprintf_s(strArg, 32,32, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_SUB:
		wcscpy_s(strInstr, 8, _T("SUB"));
        _snwprintf_s(strArg, 32,32, _T("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    }

    wcscpy_s(strInstr, 8, _T("unknown"));
    _snwprintf_s(strArg, 32,32, _T("%06o"), instr);
    return 1;
}
