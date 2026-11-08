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
#include <bit>
#include <format>
#include <tuple>


namespace {
struct OperandText
{
    TcharString text;
    uint16_t extraWords;
};

// addr is the address of the operand word, if one is present.
OperandText FormatOperand(uint8_t reg, uint8_t mode, uint16_t addr, uint16_t word)
{
    const auto name = REGISTER_NAME[reg];
    if (reg != 7)
    {
        switch (mode)
        {
        case 0: return {name, 0};
        case 1: return {std::format(_T("({})"), name), 0};
        case 2: return {std::format(_T("({})+"), name), 0};
        case 3: return {std::format(_T("@({})+"), name), 0};
        case 4: return {std::format(_T("-({})"), name), 0};
        case 5: return {std::format(_T("@-({})"), name), 0};
        case 6: return {std::format(_T("{:06o}({})"), word, name), 1};
        case 7: return {std::format(_T("@{:06o}({})"), word, name), 1};
        }
    }
    else
    {
        switch (mode)
        {
        case 0: return {_T("PC"), 0};
        case 1: return {_T("(PC)"), 0};
        case 2: return {std::format(_T("#{:06o}"), word), 1};
        case 3: return {std::format(_T("@#{:06o}"), word), 1};
        case 4: return {_T("-(PC)"), 0};
        case 5: return {_T("@-(PC)"), 0};
        case 6: return {std::format(_T("{:06o}"), static_cast<uint16_t>(addr + word + 2)), 1};
        case 7: return {std::format(_T("@{:06o}"), static_cast<uint16_t>(addr + word + 2)), 1};
        }
    }
    return {}; // mode comes from a three-bit instruction field.
}
}

std::optional<DisassembledInstruction> DisassembleInstruction(std::span<const uint16_t> memory, uint16_t addr)
{
    if (memory.empty()) return std::nullopt;
    std::array<uint16_t, 3> words{};
    std::ranges::copy(memory.first((std::min)(memory.size(), words.size())), words.begin());
    const uint16_t instr = words[0];
    const auto make = [](TcharString mnemonic, TcharString argument = {}, uint16_t length = 1) {
        return DisassembledInstruction{std::move(mnemonic), std::move(argument), length};
    };

    switch (instr)
    {
    case PI_HALT: return make(_T("HALT"));
    case PI_WAIT: return make(_T("WAIT"));
    case PI_RTI: return make(_T("RTI"));
    case PI_BPT: return make(_T("BPT"));
    case PI_IOT: return make(_T("IOT"));
    case PI_RESET: return make(_T("RESET"));
    case PI_RTT: return make(_T("RTT"));
    case PI_NOP: return make(_T("NOP"));
    case PI_CLC: return make(_T("CLC"));
    case PI_CLV: return make(_T("CLV"));
    case PI_CLVC: return make(_T("CLVC"));
    case PI_CLZ: return make(_T("CLZ"));
    case PI_CLZC: return make(_T("CLZC"));
    case PI_CLZV: return make(_T("CLZV"));
    case PI_CLZVC: return make(_T("CLZVC"));
    case PI_CLN: return make(_T("CLN"));
    case PI_CLNC: return make(_T("CLNC"));
    case PI_CLNV: return make(_T("CLNV"));
    case PI_CLNVC: return make(_T("CLNVC"));
    case PI_CLNZ: return make(_T("CLNZ"));
    case PI_CLNZC: return make(_T("CLNZC"));
    case PI_CLNZV: return make(_T("CLNZV"));
    case PI_CCC: return make(_T("CCC"));
    case PI_NOP260: return make(_T("NOP260"));
    case PI_SEC: return make(_T("SEC"));
    case PI_SEV: return make(_T("SEV"));
    case PI_SEVC: return make(_T("SEVC"));
    case PI_SEZ: return make(_T("SEZ"));
    case PI_SEZC: return make(_T("SEZC"));
    case PI_SEZV: return make(_T("SEZV"));
    case PI_SEZVC: return make(_T("SEZVC"));
    case PI_SEN: return make(_T("SEN"));
    case PI_SENC: return make(_T("SENC"));
    case PI_SENV: return make(_T("SENV"));
    case PI_SENVC: return make(_T("SENVC"));
    case PI_SENZ: return make(_T("SENZ"));
    case PI_SENZC: return make(_T("SENZC"));
    case PI_SENZV: return make(_T("SENZV"));
    case PI_SCC: return make(_T("SCC"));
    case PI_START: return make(_T("START"));
    case PI_STEP: return make(_T("STEP"));
    case PI_RSEL: return make(_T("RSEL"));
    case PI_MFUS: return make(_T("MFUS"));
    case PI_RCPC: return make(_T("RCPC"));
    case PI_RCPS: return make(_T("RCPS"));
    case PI_MTUS: return make(_T("MTUS"));
    case PI_WCPC: return make(_T("WCPC"));
    case PI_WCPS: return make(_T("WCPS"));
    }

    if ((instr & ~static_cast<uint16_t>(7)) == PI_RTS)
        return GetDigit(instr, 0) == 7 ? make(_T("RETURN")) : make(_T("RTS"), REGISTER_NAME[GetDigit(instr, 0)]);

    switch (instr & ~static_cast<uint16_t>(7))
    {
    case PI_FADD: return make(_T("FADD"), REGISTER_NAME[GetDigit(instr, 0)]);
    case PI_FSUB: return make(_T("FSUB"), REGISTER_NAME[GetDigit(instr, 0)]);
    case PI_FMUL: return make(_T("FMUL"), REGISTER_NAME[GetDigit(instr, 0)]);
    case PI_FDIV: return make(_T("FDIV"), REGISTER_NAME[GetDigit(instr, 0)]);
    }
    if ((instr & ~static_cast<uint16_t>(077)) == PI_MARK)
        return make(_T("MARK"), std::format(_T("{:o}"), instr & 077));

    const auto destination = FormatOperand(GetDigit(instr, 0), GetDigit(instr, 1), static_cast<uint16_t>(addr + 2), words[1]);
    const auto oneOperand = [&](TcharString mnemonic) {
        return make(std::move(mnemonic), destination.text, static_cast<uint16_t>(1 + destination.extraWords));
    };
    switch (instr & ~static_cast<uint16_t>(077))
    {
    case PI_JMP: return oneOperand(_T("JMP"));
    case PI_SWAB: return oneOperand(_T("SWAB"));
    case PI_SXT: return oneOperand(_T("SXT"));
    case PI_MTPS: return oneOperand(_T("MTPS"));
    case PI_MFPS: return oneOperand(_T("MFPS"));
    }
    const bool byte = (instr & 0100000) != 0;
    switch (instr & ~static_cast<uint16_t>(0100077))
    {
    case PI_CLR: return oneOperand(byte ? _T("CLRB") : _T("CLR"));
    case PI_COM: return oneOperand(byte ? _T("COMB") : _T("COM"));
    case PI_INC: return oneOperand(byte ? _T("INCB") : _T("INC"));
    case PI_DEC: return oneOperand(byte ? _T("DECB") : _T("DEC"));
    case PI_NEG: return oneOperand(byte ? _T("NEGB") : _T("NEG"));
    case PI_ADC: return oneOperand(byte ? _T("ADCB") : _T("ADC"));
    case PI_SBC: return oneOperand(byte ? _T("SBCB") : _T("SBC"));
    case PI_TST: return oneOperand(byte ? _T("TSTB") : _T("TST"));
    case PI_ROR: return oneOperand(byte ? _T("RORB") : _T("ROR"));
    case PI_ROL: return oneOperand(byte ? _T("ROLB") : _T("ROL"));
    case PI_ASR: return oneOperand(byte ? _T("ASRB") : _T("ASR"));
    case PI_ASL: return oneOperand(byte ? _T("ASLB") : _T("ASL"));
    }

    const auto branchTarget = static_cast<uint16_t>(addr + static_cast<int8_t>(instr & 0xff) * 2 + 2);
    const auto branch = [&](TcharString mnemonic) {
        return make(std::move(mnemonic), std::format(_T("{:06o}"), branchTarget));
    };
    switch (instr & ~static_cast<uint16_t>(0377))
    {
    case PI_BR: return branch(_T("BR"));
    case PI_BNE: return branch(_T("BNE"));
    case PI_BEQ: return branch(_T("BEQ"));
    case PI_BGE: return branch(_T("BGE"));
    case PI_BLT: return branch(_T("BLT"));
    case PI_BGT: return branch(_T("BGT"));
    case PI_BLE: return branch(_T("BLE"));
    case PI_BPL: return branch(_T("BPL"));
    case PI_BMI: return branch(_T("BMI"));
    case PI_BHI: return branch(_T("BHI"));
    case PI_BLOS: return branch(_T("BLOS"));
    case PI_BVC: return branch(_T("BVC"));
    case PI_BVS: return branch(_T("BVS"));
    case PI_BHIS: return branch(_T("BHIS"));
    case PI_BLO: return branch(_T("BLO"));
    case PI_EMT: return make(_T("EMT"), std::format(_T("{:03o}"), instr & 0xff));
    case PI_TRAP: return make(_T("TRAP"), std::format(_T("{:03o}"), instr & 0xff));
    }

    const auto reg = REGISTER_NAME[GetDigit(instr, 2)];
    const auto arithmetic = [&](TcharString mnemonic, bool registerFirst) {
        return make(std::move(mnemonic), registerFirst ? std::format(_T("{}, {}"), reg, destination.text)
                    : std::format(_T("{}, {}"), destination.text, reg), static_cast<uint16_t>(1 + destination.extraWords));
    };
    switch (instr & ~static_cast<uint16_t>(0777))
    {
    case PI_JSR:
        return GetDigit(instr, 2) == 7 ? oneOperand(_T("CALL")) : arithmetic(_T("JSR"), true);
    case PI_MUL: return arithmetic(_T("MUL"), false);
    case PI_DIV: return arithmetic(_T("DIV"), false);
    case PI_ASH: return arithmetic(_T("ASH"), false);
    case PI_ASHC: return arithmetic(_T("ASHC"), false);
    case PI_XOR: return arithmetic(_T("XOR"), true);
    case PI_SOB:
        return make(_T("SOB"), std::format(_T("{}, {:06o}"), reg,
            static_cast<uint16_t>(addr - (GetDigit(instr, 1) * 8 + GetDigit(instr, 0)) * 2 + 2)));
    }

    const auto source = FormatOperand(GetDigit(instr, 2), GetDigit(instr, 3), static_cast<uint16_t>(addr + 2), words[1]);
    const auto target = FormatOperand(GetDigit(instr, 0), GetDigit(instr, 1),
        static_cast<uint16_t>(addr + 2 + source.extraWords * 2), words[1 + source.extraWords]);
    const auto twoOperands = [&](TcharString mnemonic) {
        return make(std::move(mnemonic), std::format(_T("{}, {}"), source.text, target.text),
                    static_cast<uint16_t>(1 + source.extraWords + target.extraWords));
    };
    switch (instr & ~static_cast<uint16_t>(0107777))
    {
    case PI_MOV: return twoOperands(byte ? _T("MOVB") : _T("MOV"));
    case PI_CMP: return twoOperands(byte ? _T("CMPB") : _T("CMP"));
    case PI_BIT: return twoOperands(byte ? _T("BITB") : _T("BIT"));
    case PI_BIC: return twoOperands(byte ? _T("BICB") : _T("BIC"));
    case PI_BIS: return twoOperands(byte ? _T("BISB") : _T("BIS"));
    }
    switch (instr & ~static_cast<uint16_t>(0007777))
    {
    case PI_ADD: return twoOperands(_T("ADD"));
    case PI_SUB: return twoOperands(_T("SUB"));
    }
    return make(_T("unknown"), std::format(_T("{:06o}"), instr));
}

std::optional<int> Disasm_CheckForJump(std::span<const uint16_t> memory, uint16_t address)
{
    if (memory.empty()) return std::nullopt;
    const uint16_t instr = memory.front();

    // BR, BNE, BEQ, BGE, BLT, BGT, BLE
    // BPL, BMI, BHI, BLOS, BVC, BVS, BHIS, BLO
    if ((instr & 0177400) >= 0000400 && (instr & 0177400) < 0004000 ||
        (instr & 0177400) >= 0100000 && (instr & 0177400) < 0104000)
    {
        return static_cast<int>(static_cast<int8_t>(instr & 0xff)) + 1;
    }

    // SOB
    if ((instr & ~static_cast<uint16_t>(0777)) == PI_SOB)
    {
        return -(GetDigit(instr, 1) * 8 + GetDigit(instr, 0)) + 1;
    }

    // JMP and JSR (with any link register, CALL is JSR PC) with a target that is known without the registers:
    // the relative X(PC) and the absolute @#addr addressing; the word after the instruction is the offset or the address
    if ((instr & ~static_cast<uint16_t>(077)) == PI_JMP || (instr & ~static_cast<uint16_t>(0777)) == PI_JSR)
    {
        if (memory.size() < 2) return std::nullopt;
        if ((instr & 077) == 067)  // X(PC): the target is the address after the offset word plus the offset
        {
            if ((memory[1] & 1) != 0)
                return std::nullopt;  // an odd offset gives an odd target
            return (static_cast<int16_t>(memory[1]) + 4) / 2;
        }
        if ((instr & 077) == 037)  // @#addr
        {
            uint16_t target = memory[1];
            if ((target & 1) != 0)
                return std::nullopt;  // an odd address is not a target of a jump
            return static_cast<int>(static_cast<int16_t>(static_cast<uint16_t>(target - address))) / 2;
        }
    }

    return std::nullopt;
}

// Register values as the operand being decoded sees them. The source operand changes its register
// (autoincrement, autodecrement) before the destination is decoded, so the hint of the destination is built
// from the registers as the source left them.
struct HintRegs
{
    std::array<uint16_t, 8> r{};
};

namespace {
std::array<uint16_t, 3> PadHintWords(std::span<const uint16_t> memory)
{
    std::array<uint16_t, 3> words{};
    std::ranges::copy(memory.first((std::min)(memory.size(), words.size())), words.begin());
    return words;
}

}

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
        regs.r[regnum] = static_cast<uint16_t>(regs.r[regnum] + step);
    else if (regmod == 4 || regmod == 5)
        regs.r[regnum] = static_cast<uint16_t>(regs.r[regnum] - step);
}

struct HintValue
{
    std::optional<uint16_t> value;
    TcharString text;
};

// I/O and inaccessible addresses have descriptive text but no readable value.
static HintValue Disasm_ReadHintValue(const CMemoryController& memoryController, bool haltMode,
        uint16_t address, bool byteword)
{
    auto [word, addrtype] = memoryController.GetWordView(static_cast<uint16_t>(address & ~1), haltMode, false);
    // The types: memory is below ADDRTYPE_IO (64), an I/O port is 64..127 (the device number is in the low bits),
    // ADDRTYPE_NONE (128) and ADDRTYPE_DENY (255) have no value. ADDRTYPE_DENY has the I/O bit set too,
    // so everything from ADDRTYPE_NONE up is tested before the range of the I/O ports.
    if (addrtype >= ADDRTYPE_NONE)
        return {std::nullopt, _T("N/A")};
    if (addrtype >= ADDRTYPE_IO)
        return {std::nullopt, _T("I/O")};
    if (byteword)
    {
        word = (address & 1) ? (word >> 8) : (word & 0xff);
        return {word, std::format(_T("{:03o}"), word)};
    }
    return {word, std::format(_T("{:06o}"), word)};
}

// "(label)=value", the value is read at the address
static TcharString Disasm_MemoryHint(const CMemoryController& memoryController, bool haltMode,
        std::basic_string_view<TCHAR> label, uint16_t address, bool byteword)
{
    const auto value = Disasm_ReadHintValue(memoryController, haltMode, address, byteword);
    return std::format(_T("{}={}"), label, value.text);
}

// "(label)=HHHHHH:LLLLLL = value": a 32-bit FIS value, two words at the address and at address + 2.
// An odd address cannot be read as a word pair (bus error); a pair that is not readable shows N/A or I/O.
static TcharString Disasm_PairHint(const CMemoryController& memoryController, bool haltMode,
        std::basic_string_view<TCHAR> label, uint16_t address)
{
    if (address & 1)
        return std::format(_T("{}=N/A"), label);
    const auto high = Disasm_ReadHintValue(memoryController, haltMode, address, false);
    const auto low = Disasm_ReadHintValue(memoryController, haltMode, static_cast<uint16_t>(address + 2), false);
    if (high.value && low.value)
        return std::format(_T("{}={}:{} = {}"), label, high.text, low.text,
            Disasm_FloatToString(*high.value, *low.value));
    return std::format(_T("{}={}"), label, high.value ? low.text : high.text);
}

// Is the instruction a jump or a return, so that its hint has the color of the jump hints
static bool Disasm_IsJumpInstruction(uint16_t instr)
{
    if ((instr & 0177400) >= 0000400 && (instr & 0177400) < 0004000 ||
        (instr & 0177400) >= 0100000 && (instr & 0177400) < 0104000)  // BR, BNE...BLE, BPL...BLO
        return true;
    if ((instr & ~static_cast<uint16_t>(0777)) == PI_SOB)
        return true;
    if ((instr & ~static_cast<uint16_t>(077)) == PI_JMP || (instr & ~static_cast<uint16_t>(0777)) == PI_JSR)  // JMP, JSR, CALL
        return true;
    if (instr >= 0000200 && instr <= 0000207)  // RTS, RETURN
        return true;
    if (instr == PI_RTI || instr == PI_RTT || instr == PI_BPT || instr == PI_IOT)
        return true;
    if (instr >= PI_EMT && instr <= 0104777)  // EMT, TRAP
        return true;
    return false;
}

struct JumpHintLines
{
    TcharString first;
    TcharString second;
    bool predict = true;
};

// Prepare the jump hint lines and the prediction for a conditional jump.
static JumpHintLines Disasm_JumpHintLines(std::span<const uint16_t, 3> memory,
        const CProcessor& processor, const CMemoryController& memoryController)
{
    JumpHintLines result;
    auto& buffer = result.first;
    auto& buffer2 = result.second;
    uint16_t instr = memory.front();
    uint16_t psw = processor.GetPSW();

    if (instr >= 0001000 && instr <= 0001777)  // BNE, BEQ
    {
        buffer = std::format(_T("Z={}"), (psw & PSW_Z) ? _T('1') : _T('0'));
        // BNE: IF (Z == 0)
        // BEQ: IF (Z == 1)
        bool value = ((psw & PSW_Z) != 0);
        result.predict = ((instr & 0400) == 0) ? !value : value;
        return result;
    }
    if (instr >= 0002000 && instr <= 0002777)  // BGE, BLT
    {
        buffer = std::format(_T("N={}, V={}"), (psw & PSW_N) ? _T('1') : _T('0'), (psw & PSW_V) ? _T('1') : _T('0'));
        // BGE: IF ((N xor V) == 0)
        // BLT: IF ((N xor V) == 1)
        bool value = (((psw & PSW_N) != 0) != ((psw & PSW_V) != 0));
        result.predict = ((instr & 0400) == 0) ? !value : value;
        return result;
    }
    if (instr >= 0003000 && instr <= 0003777)  // BGT, BLE
    {
        buffer = std::format(_T("N={}, V={}, Z={}"), (psw & PSW_N) ? _T('1') : _T('0'), (psw & PSW_V) ? _T('1') : _T('0'), (psw & PSW_Z) ? _T('1') : _T('0'));
        // BGT: IF (((N xor V) or Z) == 0)
        // BLE: IF (((N xor V) or Z) == 1)
        bool value = ((((psw & PSW_N) != 0) != ((psw & PSW_V) != 0)) || ((psw & PSW_Z) != 0));
        result.predict = ((instr & 0400) == 0) ? !value : value;
        return result;
    }
    if (instr >= 0100000 && instr <= 0100777)  // BPL, BMI
    {
        buffer = std::format(_T("N={}"), (psw & PSW_N) ? _T('1') : _T('0'));
        // BPL: IF (N == 0)
        // BMI: IF (N == 1)
        bool value = ((psw & PSW_N) != 0);
        result.predict = ((instr & 0400) == 0) ? !value : value;
        return result;
    }
    if (instr >= 0101000 && instr <= 0101777)  // BHI, BLOS
    {
        buffer = std::format(_T("C={}, Z={}"), (psw & PSW_C) ? _T('1') : _T('0'), (psw & PSW_Z) ? _T('1') : _T('0'));
        // BHI:  IF ((C or Z) == 0)
        // BLOS: IF ((C or Z) == 1)
        bool value = (((psw & PSW_C) != 0) || ((psw & PSW_Z) != 0));
        result.predict = ((instr & 0400) == 0) ? !value : value;
        return result;
    }
    if (instr >= 0102000 && instr <= 0102777)  // BVC, BVS
    {
        buffer = std::format(_T("V={}"), (psw & PSW_V) ? _T('1') : _T('0'));
        // BVC: IF (V == 0)
        // BVS: IF (V == 1)
        bool value = ((psw & PSW_V) != 0);
        result.predict = ((instr & 0400) == 0) ? !value : value;
        return result;
    }
    if (instr >= 0103000 && instr <= 0103777)  // BCC/BHIS, BCS/BLO
    {
        buffer = std::format(_T("C={}"), (psw & PSW_C) ? _T('1') : _T('0'));
        // BCC/BHIS: IF (C == 0)
        // BCS/BLO:  IF (C == 1)
        bool value = ((psw & PSW_C) != 0);
        result.predict = ((instr & 0400) == 0) ? !value : value;
        return result;
    }
    if (instr >= 0077000 && instr <= 0077777)  // SOB
    {
        int reg = (instr >> 6) & 7;
        uint16_t regvalue = processor.GetReg(reg);
        buffer = std::format(_T("{}={:06o}"), REGISTER_NAME[reg], regvalue);
        result.predict = regvalue != 1;
        return result;
    }

    // JSR has no jump hint: its link register and its destination are shown by the instruction hint
    if (instr >= 000200 && instr <= 000207)  // RTS / RETURN
    {
        const auto hintSp = Disasm_MemoryHint(memoryController, processor.IsHaltMode(), _T("(SP)"), processor.GetSP(), false);
        if (instr == 000207)  // RETURN
            buffer = hintSp;
        else  // RTS
        {
            int reg = instr & 7;
            buffer = std::format(_T("{}={:06o}, {}"), REGISTER_NAME[reg], processor.GetReg(reg), hintSp);
        }
        return result;
    }

    if (instr == 000002 || instr == 000006)  // RTI, RTT: the new PC and the new PSW from the stack
    {
        uint16_t sp = processor.GetSP();
        buffer = Disasm_MemoryHint(memoryController, processor.IsHaltMode(), _T("(SP)"), sp, false);
        buffer2 = Disasm_MemoryHint(memoryController, processor.IsHaltMode(), _T("(SP+2)"), static_cast<uint16_t>(sp + 2), false);
        return result;
    }
    if (instr == 000003 || instr == 000004 ||  // IOT, BPT
        (instr >= 0104000 && instr <= 0104777))  // TRAP, EMT
    {
        uint16_t intvec;
        if (instr == 000003) intvec = 000014;
        else if (instr == 000004) intvec = 000020;
        else if (instr < 0104400) intvec = 000030;
        else intvec = 000034;

        // The trap is a USER mode interrupt: the new PC and PSW are read from the vector in USER mode
        buffer = Disasm_MemoryHint(memoryController, false, std::format(_T("({:06o})"), intvec), intvec, false);
        buffer2 = Disasm_MemoryHint(memoryController, false, std::format(_T("({:06o})"), intvec + 2), static_cast<uint16_t>(intvec + 2), false);
        return result;
    }

    return result;  // All other jumps are non-conditional
}

DisasmJumpHint Disasm_GetJumpConditionHint(std::span<const uint16_t> memory,
    const CProcessor& processor, const CMemoryController& memoryController)
{
    if (memory.empty()) return {};
    const auto words = PadHintWords(memory);
    auto hint = Disasm_JumpHintLines(words, processor, memoryController);
    return {std::move(hint.first), hint.predict};
}

using OperandHints = std::array<TcharString, DISASM_HINT_LINES>;

// A deferred operand shows both the pointer and, when readable, its target.
static std::pair<TcharString, TcharString> Disasm_PointerHint(
        const CProcessor& processor, const CMemoryController& memoryController,
        std::basic_string_view<TCHAR> label, uint16_t pointerAddress, bool byteword)
{
    const auto pointer = Disasm_ReadHintValue(memoryController, processor.IsHaltMode(), pointerAddress, false);
    auto pointerHint = std::format(_T("{}={}"), label, pointer.text);
    TcharString operandHint;
    if (pointer.value)
        operandHint = Disasm_MemoryHint(memoryController, processor.IsHaltMode(),
            std::format(_T("({:06o})"), *pointer.value), *pointer.value, byteword);
    return {std::move(pointerHint), std::move(operandHint)};
}

static OperandHints Disasm_RegisterHint(const CProcessor& processor, const CMemoryController& memoryController,
        const HintRegs& regs, int regnum, int regmod, bool byteword, uint16_t indexval)
{
    OperandHints hints;
    const uint16_t regval = regs.r[regnum];
    const auto regname = REGISTER_NAME[regnum];
    hints[0] = std::format(_T("{}={:06o}"), regname, regval);

    switch (regmod)
    {
    case 1:
    case 2:
        hints[1] = Disasm_MemoryHint(memoryController, processor.IsHaltMode(),
            std::format(_T("({})"), regname), regval, byteword);
        break;
    case 3:
        std::tie(hints[1], hints[2]) = Disasm_PointerHint(processor, memoryController,
            std::format(_T("({})"), regname), regval, byteword);
        break;
    case 4:
        {
            const int step = Disasm_GetStep(regnum, regmod, byteword);
            hints[1] = Disasm_MemoryHint(memoryController, processor.IsHaltMode(),
                std::format(_T("({}-{})"), regname, step), static_cast<uint16_t>(regval - step), byteword);
        }
        break;
    case 5:
        std::tie(hints[1], hints[2]) = Disasm_PointerHint(processor, memoryController,
            std::format(_T("({}-2)"), regname), static_cast<uint16_t>(regval - 2), byteword);
        break;
    case 6:
        hints[1] = Disasm_MemoryHint(memoryController, processor.IsHaltMode(),
            std::format(_T("({}+{:06o})"), regname, indexval), static_cast<uint16_t>(regval + indexval), byteword);
        break;
    case 7:
        std::tie(hints[1], hints[2]) = Disasm_PointerHint(processor, memoryController,
            std::format(_T("({}+{:06o})"), regname, indexval), static_cast<uint16_t>(regval + indexval), byteword);
        break;
    }
    return hints;
}

static OperandHints Disasm_RegisterHintPC(const CProcessor& processor, const CMemoryController& memoryController,
        int regmod, bool byteword, uint16_t curaddr, uint16_t value)
{
    OperandHints hints;
    if (regmod == 3)  // @#addr: the value is at the address in the instruction
        hints[0] = Disasm_MemoryHint(memoryController, processor.IsHaltMode(),
            std::format(_T("({:06o})"), value), value, byteword);
    else if (regmod == 6)  // PC-relative, after the offset word
    {
        const auto address = static_cast<uint16_t>(curaddr + value);
        hints[0] = Disasm_MemoryHint(memoryController, processor.IsHaltMode(),
            std::format(_T("({:06o})"), address), address, byteword);
    }
    else if (regmod == 7)  // PC-relative pointer and operand
    {
        const auto address = static_cast<uint16_t>(curaddr + value);
        std::tie(hints[0], hints[1]) = Disasm_PointerHint(processor, memoryController,
            std::format(_T("({:06o})"), address), address, byteword);
    }
    return hints;
}

// Join source and destination pieces, aligning a lone destination beneath line 1.
static TcharString Disasm_JoinHints(std::basic_string_view<TCHAR> src,
        std::basic_string_view<TCHAR> dst, size_t dstColumn)
{
    if (!src.empty() && !dst.empty())
        return src == dst ? TcharString(src) : std::format(_T("{}, {}"), src, dst);
    if (!src.empty()) return TcharString(src);
    if (dst.empty()) return {};
    constexpr size_t alignmentLimit = 63;
    const size_t column = dstColumn + dst.size() < alignmentLimit ? dstColumn : 0;
    return TcharString(column, _T(' ')) + TcharString(dst);
}

static OperandHints Disasm_InstructionHint(std::span<const uint16_t, 3> memory, const CProcessor& processor,
        const CMemoryController& memoryController, int srcreg, int srcmod, int dstreg, int dstmod, bool byteword)
{
    OperandHints source, destination;
    HintRegs regs;
    for (int i = 0; i < 8; ++i)
        regs.r[i] = processor.GetReg(i);
    size_t nextWord = 1;
    uint16_t curaddr = processor.GetPC() + 2;

    if (srcreg >= 0)
    {
        uint16_t extraWord = 0;
        if (Disasm_HasExtraWord(srcreg, srcmod))
        {
            extraWord = memory[nextWord++];
            curaddr += 2;
        }
        if (srcreg == 7)
            source = Disasm_RegisterHintPC(processor, memoryController, srcmod, byteword, curaddr, extraWord);
        else
        {
            source = Disasm_RegisterHint(processor, memoryController, regs, srcreg, srcmod, byteword, extraWord);
            Disasm_ApplySideEffect(regs, srcreg, srcmod, byteword);
        }
    }
    if (dstreg >= 0)
    {
        uint16_t extraWord = 0;
        if (Disasm_HasExtraWord(dstreg, dstmod))
        {
            extraWord = memory[nextWord++];
            curaddr += 2;
        }
        if (dstreg == 7)
            destination = Disasm_RegisterHintPC(processor, memoryController, dstmod, byteword, curaddr, extraWord);
        else
            destination = Disasm_RegisterHint(processor, memoryController, regs, dstreg, dstmod, byteword, extraWord);
    }

    const size_t dstColumn = !source[0].empty() && !destination[0].empty() && source[0] != destination[0]
        ? source[0].size() + 2 : 0;
    return {Disasm_JoinHints(source[0], destination[0], 0),
            Disasm_JoinHints(source[1], destination[1], dstColumn),
            Disasm_JoinHints(source[2], destination[2], dstColumn)};
}
// Prepare the lines of the instruction hint for a regular instruction.
static DisasmHint Disasm_InstructionHintLines(std::span<const uint16_t, 3> memory, const CProcessor& processor,
        const CMemoryController& memoryController)
{
    DisasmHint hint;
    auto& buffer = hint.lines[0];
    auto& buffer2 = hint.lines[1];
    auto& buffer3 = hint.lines[2];
    uint16_t instr = memory.front();

    // Source and Destination
    if ((instr & ~static_cast<uint16_t>(0107777)) == PI_MOV || (instr & ~static_cast<uint16_t>(0107777)) == PI_CMP ||
        (instr & ~static_cast<uint16_t>(0107777)) == PI_BIT || (instr & ~static_cast<uint16_t>(0107777)) == PI_BIC || (instr & ~static_cast<uint16_t>(0107777)) == PI_BIS ||
        (instr & ~static_cast<uint16_t>(0007777)) == PI_ADD || (instr & ~static_cast<uint16_t>(0007777)) == PI_SUB)
    {
        int srcreg = (instr >> 6) & 7;
        int srcmod = (instr >> 9) & 7;
        int dstreg = instr & 7;
        int dstmod = (instr >> 3) & 7;
        // The bit 15 is the byte flag, except for SUB (0160000), which is a word operation
        bool okByte = (instr & 0100000) != 0 && (instr & ~static_cast<uint16_t>(0007777)) != PI_SUB;
        hint.lines = Disasm_InstructionHint(memory, processor, memoryController, srcreg, srcmod, dstreg, dstmod, okByte);
    }

    // Register and Destination
    if ((instr & ~static_cast<uint16_t>(0777)) == PI_MUL || (instr & ~static_cast<uint16_t>(0777)) == PI_DIV ||
        (instr & ~static_cast<uint16_t>(0777)) == PI_ASH || (instr & ~static_cast<uint16_t>(0777)) == PI_ASHC ||
        (instr & ~static_cast<uint16_t>(0777)) == PI_XOR)
    {
        int srcreg = (instr >> 6) & 7;
        int dstreg = instr & 7;
        int dstmod = (instr >> 3) & 7;
        if ((instr & ~static_cast<uint16_t>(0777)) != PI_XOR)
        {
            // MUL, DIV, ASH, ASHC are written "operand, register": line 1 is the operand piece, then the register.
            // DIV and ASHC use the register pair Rn:Rn|1, so they show both; for an odd Rn the pair is Rn itself.
            // A register that the operand piece already shows is not repeated.
            bool okPair = (instr & ~static_cast<uint16_t>(0777)) == PI_DIV || (instr & ~static_cast<uint16_t>(0777)) == PI_ASHC;
            hint.lines = Disasm_InstructionHint(memory, processor, memoryController, -1, -1, dstreg, dstmod, false);
            for (int reg = srcreg; reg <= (okPair ? (srcreg | 1) : srcreg); reg++)
            {
                if (reg == dstreg && dstmod == 0)
                    continue;
                buffer += std::format(_T("{}{}={:06o}"),
                    buffer.empty() ? _T("") : _T(", "), REGISTER_NAME[reg], processor.GetReg(reg));
            }
        }
        else  // XOR is written "register, operand"
            hint.lines = Disasm_InstructionHint(memory, processor, memoryController, srcreg, 0, dstreg, dstmod, false);
    }

    // Destination only
    if ((instr & ~static_cast<uint16_t>(0100077)) == PI_CLR || (instr & ~static_cast<uint16_t>(0100077)) == PI_COM ||
        (instr & ~static_cast<uint16_t>(0100077)) == PI_INC || (instr & ~static_cast<uint16_t>(0100077)) == PI_DEC || (instr & ~static_cast<uint16_t>(0100077)) == PI_NEG ||
        (instr & ~static_cast<uint16_t>(0100077)) == PI_TST ||
        (instr & ~static_cast<uint16_t>(0100077)) == PI_ASR || (instr & ~static_cast<uint16_t>(0100077)) == PI_ASL ||
        (instr & ~static_cast<uint16_t>(077)) == PI_SWAB || (instr & ~static_cast<uint16_t>(077)) == PI_SXT ||
        (instr & ~static_cast<uint16_t>(077)) == PI_MTPS || (instr & ~static_cast<uint16_t>(077)) == PI_MFPS)
    {
        int dstreg = instr & 7;
        int dstmod = (instr >> 3) & 7;
        // The bit 15 is the byte flag: CLRB...ASLB, MTPS and MFPS are byte operations, SWAB and SXT are not
        hint.lines = Disasm_InstructionHint(memory, processor, memoryController, -1, -1, dstreg, dstmod, (instr & 0100000) != 0);

        // MFPS, MTPS: also the low byte of the PSW; SXT: also the N flag, the value of the result is -N
        TcharString extra;
        if ((instr & ~static_cast<uint16_t>(077)) == PI_MFPS || (instr & ~static_cast<uint16_t>(077)) == PI_MTPS)
            extra = std::format(_T("PS={:03o}"), processor.GetPSW() & 0377);
        else if ((instr & ~static_cast<uint16_t>(077)) == PI_SXT)
            extra = std::format(_T("N={}"), (processor.GetPSW() & PSW_N) ? _T('1') : _T('0'));
        if (!extra.empty())
            buffer += (buffer.empty() ? _T("") : _T(", ")) + extra;
    }

    // ADC, SBC, ROR, ROL: destination only, and also show C flag
    if ((instr & ~static_cast<uint16_t>(0100077)) == PI_ADC || (instr & ~static_cast<uint16_t>(0100077)) == PI_SBC ||
        (instr & ~static_cast<uint16_t>(0100077)) == PI_ROR || (instr & ~static_cast<uint16_t>(0100077)) == PI_ROL)
    {
        int dstreg = instr & 7;
        int dstmod = (instr >> 3) & 7;
        hint.lines = Disasm_InstructionHint(memory, processor, memoryController, -1, -1, dstreg, dstmod, (instr & 0100000) != 0);
        uint16_t psw = processor.GetPSW();
        buffer += std::format(_T("{}C={}"), buffer.empty() ? _T("") : _T(", "), (psw & PSW_C) ? _T('1') : _T('0'));
    }

    // CLC..CCC, SEC..SCC -- show flags
    if (instr >= 0000241 && instr <= 0000257 || instr >= 0000261 && instr <= 0000277)
    {
        uint16_t psw = processor.GetPSW();
        buffer = std::format(_T("C={}, V={}, Z={}, N={}"),
            (psw & PSW_C) ? _T('1') : _T('0'), (psw & PSW_V) ? _T('1') : _T('0'),
            (psw & PSW_Z) ? _T('1') : _T('0'), (psw & PSW_N) ? _T('1') : _T('0'));
    }

    // JSR, JMP: the destination, except the relative and the absolute addressing, which have the arrow;
    // JSR also shows its link register (CALL, JSR PC, has none to show)
    bool okJsr = (instr & ~static_cast<uint16_t>(0777)) == PI_JSR;
    if (okJsr || (instr & ~static_cast<uint16_t>(077)) == PI_JMP)
    {
        bool okTrivial = (instr & 077) == 067 || (instr & 077) == 037;
        int linkreg = okJsr ? ((instr >> 6) & 7) : -1;
        if (linkreg == 7)
            linkreg = -1;
        int dstreg = okTrivial ? -1 : (instr & 7);
        int dstmod = okTrivial ? -1 : ((instr >> 3) & 7);
        if (linkreg >= 0 || dstreg >= 0)
            hint.lines = Disasm_InstructionHint(memory, processor, memoryController,
                    linkreg, (linkreg >= 0) ? 0 : -1, dstreg, dstmod, false);
        // The word at the jump target is the next instruction, not data, so it is never shown. What is shown is how
        // the target is calculated, in the form "expression=address": R1-2=XXXXXX, R1+N=XXXXXX. For (Rn) and (Rn)+
        // the target is Rn, which line 1 shows already. For a deferred mode the address is the one of the pointer, and
        // the next line shows the pointer, which is the target: (XXXXXX)=XXXXXX; @(Rn)+ has it as (Rn)=XXXXXX.
        // The lines are aligned under the destination piece of line 1 (after the JSR link register).
        buffer3.clear();
        if (dstreg == 7)
            buffer2.clear();
        else if (dstreg >= 0 && (dstmod == 1 || dstmod == 2))
            buffer2.clear();
        else if (dstreg >= 0 && dstmod >= 4)
        {
            size_t column = 0;
            while (column < buffer2.size() && buffer2[column] == _T(' '))
                column++;
            TcharString expr;
            uint16_t address = processor.GetReg(dstreg);
            if (dstmod == 4 || dstmod == 5)
            {
                address -= 2;
                expr = std::format(_T("{}-2"), REGISTER_NAME[dstreg]);
            }
            else
            {
                address += memory[1];  // the index word follows the instruction
                expr = std::format(_T("{}+{:o}"), REGISTER_NAME[dstreg], memory[1]);
            }
            const TcharString indent(column, _T(' '));
            buffer2 = indent + std::format(_T("{}={:06o}"), expr, address);
            if (dstmod == 5 || dstmod == 7)  // the pointer is the target
                buffer3 = indent + Disasm_MemoryHint(memoryController, processor.IsHaltMode(),
                    std::format(_T("({:06o})"), address), address, false);
        }
    }

    // FADD, FSUB, FMUL, FDIV: the register points at the arguments, the first one at (Rn), the second at (Rn+4);
    // the roles are not claimed, the values are shown by position. Line 1: the register; lines 2 and 3: the two values.
    if ((instr & ~static_cast<uint16_t>(7)) == PI_FADD || (instr & ~static_cast<uint16_t>(7)) == PI_FSUB ||
        (instr & ~static_cast<uint16_t>(7)) == PI_FMUL || (instr & ~static_cast<uint16_t>(7)) == PI_FDIV)
    {
        int reg = instr & 7;
        uint16_t regval = processor.GetReg(reg);
        buffer = std::format(_T("{}={:06o}"), REGISTER_NAME[reg], regval);
        buffer2 = Disasm_PairHint(memoryController, processor.IsHaltMode(), std::format(_T("({})"), REGISTER_NAME[reg]), regval);
        buffer3 = Disasm_PairHint(memoryController, processor.IsHaltMode(), std::format(_T("({}+4)"), REGISTER_NAME[reg]), static_cast<uint16_t>(regval + 4));
    }

    // HALT mode commands (000010..000037): in USER mode they trap as a reserved instruction, so the hint is only the mode
    if (instr >= 0000010 && instr <= 0000037 && !processor.IsHaltMode())
    {
        buffer = _T("USER mode");
    }
    else if (instr >= 0000010 && instr <= 0000017)  // RUN / START, STEP: PC and PSW are taken from the copies
    {
        buffer = std::format(_T("PC'={:06o}, PS'={:06o}"), processor.GetCPC(), processor.GetCPSW());
    }
    else if (instr == 0000032 || instr == 0000033)  // WCPC: CPC <- R0
    {
        buffer = std::format(_T("R0={:06o}, PC'={:06o}"), processor.GetReg(0), processor.GetCPC());
    }
    else if (instr >= 0000034 && instr <= 0000037)  // WCPS: CPSW <- R0
    {
        buffer = std::format(_T("R0={:06o}, PS'={:06o}"), processor.GetReg(0), processor.GetCPSW());
    }
    else if (instr == PI_MFUS)
    {
        buffer = std::format(_T("R5={:06o}, R0={:06o}"), processor.GetReg(5), processor.GetReg(0));
        // the word that becomes R0, read from the USER mode memory map
        buffer2 = Disasm_MemoryHint(memoryController, false, _T("(R5)"), processor.GetReg(5), false);
    }
    else if (instr == PI_MTUS)
    {
        buffer = std::format(_T("R0={:06o}, R5={:06o}"), processor.GetReg(0), processor.GetReg(5));
        // the word that will be overwritten, in the USER mode memory map
        buffer2 = Disasm_MemoryHint(memoryController, false, _T("(R5-2)"), static_cast<uint16_t>(processor.GetReg(5) - 2), false);
    }
    else if (instr == 0000022 || instr == 0000023)  // RCPC / MFPC
    {
        buffer = std::format(_T("PC'={:06o}, R0={:06o}"), processor.GetCPC(), processor.GetReg(0));
    }
    else if (instr >= 0000024 && instr <= 0000027)  // RCPS / MFPS
    {
        buffer = std::format(_T("PS'={:06o}, R0={:06o}"), processor.GetCPSW(), processor.GetReg(0));
    }
    else if (instr == PI_RSEL)
    {
        buffer = std::format(_T("SEL={:06o}, R0={:06o}"), memoryController.GetSelRegister(), processor.GetReg(0));
    }

    if ((instr & ~static_cast<uint16_t>(077)) == PI_MARK)
    {
        // MARK nn: SP = PC + 2*nn, PC = R5, R5 = (SP)+; here PC is the address of MARK, the instruction is one word long
        uint16_t spnew = static_cast<uint16_t>(processor.GetPC() + 2 + 2 * (instr & 077));
        buffer = std::format(_T("R5={:06o}"), processor.GetReg(5));
        buffer2 = Disasm_MemoryHint(memoryController, processor.IsHaltMode(), std::format(_T("({:06o})"), spnew), spnew, false);
    }

    if (!buffer.empty()) hint.count = 1;
    if (!buffer2.empty()) hint.count = 2;
    if (!buffer3.empty()) hint.count = 3;
    return hint;
}

DisasmHint Disasm_GetInstructionHint(std::span<const uint16_t> memory,
    const CProcessor& processor, const CMemoryController& memoryController)
{
    if (memory.empty()) return {};
    const auto words = PadHintWords(memory);
    return Disasm_InstructionHintLines(words, processor, memoryController);
}

DisasmHint Disasm_GetHint(std::span<const uint16_t> memory,
    const CProcessor& processor, const CMemoryController& memoryController)
{
    if (memory.empty()) return {};
    const auto words = PadHintWords(memory);
    auto jump = Disasm_JumpHintLines(words, processor, memoryController);
    DisasmHint hint;
    if (!jump.first.empty())
    {
        hint.lines[0] = std::move(jump.first);
        hint.lines[1] = std::move(jump.second);
        hint.count = hint.lines[1].empty() ? 1 : 2;
    }
    else
        hint = Disasm_InstructionHintLines(words, processor, memoryController);
    hint.isJump = Disasm_IsJumpInstruction(words[0]);
    hint.jumpPredict = jump.predict;
    return hint;
}

// Round the absolute value to the 24-bit mantissa of the FIS number
static double Disasm_RoundMantissa24(double value)
{
    int e;
    double m = frexp(fabs(value), &e);
    return ldexp(floor(m * 16777216.0 + 0.5), e - 24);
}

TcharString Disasm_FloatToString(uint16_t high, uint16_t low)
{
    int exp = (high >> 7) & 0xFF;
    double res;
    if (exp == 0)  // zero exponent means real zero (0.0)
        res = 0.0;
    else if (exp >= 3)
    {
        // DEC and IEEE single share the layout; DEC exponent = IEEE exponent + 2
        // (bias 128 and mantissa 0.1m vs bias 127 and 1.m), so subtract 1 from the high 7 bits of exponent
        uint32_t bits = (static_cast<uint32_t>(high) << 16) | low;
        bits = (bits & 0x80FFFFFF) | (((((bits >> 24) & 0x7F) - 1) & 0x7F) << 24);
        res = std::bit_cast<float>(bits);
    }
    else  // the result would be denormal in IEEE single
    {
        uint32_t mant = ((static_cast<uint32_t>(high) << 16 | low) & 0x7FFFFF) | 0x800000;
        res = ldexp(static_cast<double>(mant) / 16777216.0, exp - 128);
        if ((high & 0x8000) != 0) res = -res;
    }
    // Use the shortest representation that gives back the same 24-bit mantissa (at most 9 digits);
    // "%g" with a short precision switches to the exponent form too early, so keep the integer digits
    int digits = 1;
    if (res != 0.0)
    {
        int intdigits = static_cast<int>(floor(log10(fabs(res)))) + 1;
        if (intdigits > 1 && intdigits <= 9) digits = intdigits;
    }
    for (; digits < 9; digits++)
    {
        const auto text = std::format(_T("{:.{}g}"), res, digits);
        if (Disasm_RoundMantissa24(_tcstod(text.c_str(), nullptr)) == Disasm_RoundMantissa24(res))
            return text;
    }
    return std::format(_T("{:.9g}"), res);
}
