// Disasm.h
//

#pragma once


//////////////////////////////////////////////////////////////////////

// Disassemble one instruction of KM1801VM2 processor
//   pMemory - memory image (we read only words of the instruction)
//   sInstr  - instruction mnemonics buffer - at least 8 characters
//   sArg    - instruction arguments buffer - at least 32 characters
//   Return value: number of words in the instruction
int DisassembleInstruction(WORD* pMemory, WORD addr, TCHAR* sInstr, TCHAR* sArg);


//////////////////////////////////////////////////////////////////////
