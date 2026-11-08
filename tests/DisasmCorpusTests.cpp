/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include <gtest/gtest.h>
#include "emubase/Emubase.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#ifdef _DEBUG
BOOL AssertFailedLine(LPCSTR file, int line)
{
    std::fprintf(stderr, "Assertion failed at %s:%d\n", file, line);
    std::abort();
}
#endif
void DebugLog(LPCTSTR) {}
const TCHAR* REGISTER_NAME[] = { _T("R0"), _T("R1"), _T("R2"), _T("R3"),
                                 _T("R4"), _T("R5"), _T("SP"), _T("PC") };

static uint64_t DisassemblyHash()
{
    uint64_t hash = 14695981039346656037ull;
    const auto add = [&hash](uint16_t value) {
        hash = (hash ^ (value & 0xff)) * 1099511628211ull;
        hash = (hash ^ (value >> 8)) * 1099511628211ull;
    };
    for (const auto& fixture : std::array<std::array<uint16_t, 3>, 2>{{
            {01000, 012345, 065432}, {0177776, 0177777, 0100000}}})
    {
        for (uint32_t opcode = 0; opcode <= 0xffff; ++opcode)
        {
            const std::array<uint16_t, 3> words{static_cast<uint16_t>(opcode), fixture[1], fixture[2]};
            const auto decoded = DisassembleInstruction(words, fixture[0]);
            if (!decoded) std::abort();
            add(decoded->length);
            for (const auto ch : decoded->mnemonic) add(static_cast<uint16_t>(ch));
            add(0);
            for (const auto ch : decoded->argument) add(static_cast<uint16_t>(ch));
            add(0);
        }
    }
    return hash;
}

static uint64_t HintHash()
{
    uint64_t hash = 14695981039346656037ull;
    const auto add = [&hash](uint16_t value) {
        hash = (hash ^ (value & 0xff)) * 1099511628211ull;
        hash = (hash ^ (value >> 8)) * 1099511628211ull;
    };
    const auto addText = [&add](const TcharString& value) {
        for (const auto ch : value) add(static_cast<uint16_t>(ch));
        add(0);
    };
    CMotherboard board;
    board.Reset();
    CProcessor& cpu = *board.GetCPU();
    auto& memory = *board.GetCPUMemoryController();
    for (int i = 0; i < 6; ++i) cpu.SetReg(i, static_cast<uint16_t>(0400 + i * 0100));
    cpu.SetSP(02000);
    cpu.SetPC(01000);
    cpu.SetCPC(01234);
    cpu.SetCPSW(07654);
    for (uint16_t address = 0400; address < 03000; address += 2)
        board.SetRAMWord(0, address, static_cast<uint16_t>(address ^ 012345));

    for (const auto psw : std::array<uint16_t, 2>{0, 0417})
    {
        cpu.SetPSW(psw);
        for (uint32_t opcode = 0; opcode <= 0xffff; ++opcode)
        {
            const std::array<uint16_t, 3> words{static_cast<uint16_t>(opcode), 01234, 05670};
            const auto hint = Disasm_GetHint(words, cpu, memory);
            add(static_cast<uint16_t>(hint.count)); add(hint.isJump); add(hint.jumpPredict);
            for (const auto& line : hint.lines) addText(line);
            const auto instruction = Disasm_GetInstructionHint(words, cpu, memory);
            add(static_cast<uint16_t>(instruction.count));
            for (const auto& line : instruction.lines) addText(line);
            const auto jump = Disasm_GetJumpConditionHint(words, cpu, memory);
            add(jump.predict); addText(jump.text);
        }
    }
    return hash;
}
static int RunRegressionChecks()
{
    CProcessor::Init();
    const auto hash = DisassemblyHash();
    const auto hintHash = HintHash();
    CProcessor::Done();
    // Captured from the pre-refactor decoder for every opcode and both fixtures.
    constexpr uint64_t expected = 0x1d71c2c39c45930cull;
    if (hash != expected) {
        std::fprintf(stderr, "disassembly corpus mismatch: %016llx\n", static_cast<unsigned long long>(hash));
        return 1;
    }
    constexpr uint64_t expectedHints = 0xe2a07d058b2a589dull; // Captured before refactoring the hint builders.
    if (hintHash != expectedHints) {
        std::fprintf(stderr, "hint corpus mismatch: %016llx\n", static_cast<unsigned long long>(hintHash));
        return 1;
    }
    return 0;
}

TEST(DisasmCorpusTests, RegressionChecks)
{
    EXPECT_EQ(RunRegressionChecks(), 0);
}
