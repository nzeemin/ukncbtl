/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>
#include <tchar.h>
#include "emubase/Defines.h"

struct ConsoleCommandParams
{
    const TCHAR* commandText = nullptr;
    int paramReg1 = -1;
    uint16_t paramOct1 = 0, paramOct2 = 0;
};

namespace console_command {
enum class Action {
    ShowHelp,
    ClearConsoleLog,
    SwitchCpuPpu,
    SetRegisterValue,
    PrintRegister,
    PrintAllRegisters,
    SetRegisterPSW,
    PrintRegisterPSW,
    StepInto,
    StepOver,
    PrintDisassembleAtAddress,
    PrintDisassembleAtPC,
    SaveMemoryFragment,
    SaveMemoryDump,
    SaveDisplayListDump,
    SetMemoryAtAddress,
    PrintMemoryDumpAtAddress,
    PrintMemoryDumpAtRegister,
    PrintMemoryDumpAtPC,
    RunToAddress,
    Run,
    SetBreakpointAtAddress,
    PrintAllBreakpoints,
    RemoveBreakpointAtAddress,
    RemoveAllBreakpoints,
    CalculateFloatNumber,
    TraceLogWithMask,
    TraceLogOnOff,
    ClearTraceLog
};
struct Specification { std::basic_string_view<TCHAR> pattern; Action action; };
inline constexpr Specification Specifications[]{
    {_T("h"), Action::ShowHelp},
    {_T("c"), Action::ClearConsoleLog},
    {_T("p"), Action::SwitchCpuPpu},
    {_T("r%d=%ho"), Action::SetRegisterValue},
    {_T("r%d %ho"), Action::SetRegisterValue},
    {_T("r%d"), Action::PrintRegister},
    {_T("r"), Action::PrintAllRegisters},
    {_T("rps=%ho"), Action::SetRegisterPSW},
    {_T("rps %ho"), Action::SetRegisterPSW},
    {_T("rps"), Action::PrintRegisterPSW},
    {_T("s"), Action::StepInto},
    {_T("so"), Action::StepOver},
    {_T("d%ho"), Action::PrintDisassembleAtAddress},
    {_T("D%ho"), Action::PrintDisassembleAtAddress},
    {_T("d"), Action::PrintDisassembleAtPC},
    {_T("D"), Action::PrintDisassembleAtPC},
    {_T("u%ho %ho"), Action::SaveMemoryFragment},
    {_T("u"), Action::SaveMemoryDump},
    {_T("udl"), Action::SaveDisplayListDump},
    {_T("m%ho %ho"), Action::SetMemoryAtAddress},
    {_T("m%ho=%ho"), Action::SetMemoryAtAddress},
    {_T("m%ho"), Action::PrintMemoryDumpAtAddress},
    {_T("mr%d"), Action::PrintMemoryDumpAtRegister},
    {_T("m"), Action::PrintMemoryDumpAtPC},
    {_T("g%ho"), Action::RunToAddress},
    {_T("g"), Action::Run},
    {_T("b%ho"), Action::SetBreakpointAtAddress},
    {_T("b"), Action::PrintAllBreakpoints},
    {_T("bc%ho"), Action::RemoveBreakpointAtAddress},
    {_T("bc"), Action::RemoveAllBreakpoints},
    {_T("fc%ho %ho"), Action::CalculateFloatNumber},
#if !defined(PRODUCT)
    {_T("t%ho"), Action::TraceLogWithMask},
    {_T("t"), Action::TraceLogOnOff},
    {_T("tc"), Action::ClearTraceLog},
#endif
};
enum class Error { UnknownCommand, InvalidRegister, InvalidValue };
struct Parsed { Action action; ConsoleCommandParams params; };

inline bool Space(TCHAR ch) { return ch == _T(' ') || ch == _T('\t'); }

// Match the small command-pattern language, consuming the entire input.
inline bool Match(std::basic_string_view<TCHAR> pattern, std::basic_string_view<TCHAR> text,
                  ConsoleCommandParams& params, std::optional<Error>& error)
{
    size_t p = 0, t = 0;
    int octalIndex = 0;
    while (p < pattern.size())
    {
        if (Space(pattern[p]))
        {
            if (t == text.size() || !Space(text[t])) return false;
            while (t < text.size() && Space(text[t])) ++t;
            ++p;
        }
        else if (pattern[p] != _T('%'))
        {
            if (t == text.size() || text[t++] != pattern[p++]) return false;
        }
        else
        {
            const bool reg = pattern.substr(p).starts_with(_T("%d"));
            p += reg ? 2 : 3; // %d or %ho; only these occur in Specifications.
            while (t < text.size() && Space(text[t])) ++t;
            bool invalid = false;
            if (t < text.size() && (text[t] == _T('-') || text[t] == _T('+')))
            { invalid = true; ++t; }
            const size_t start = t;
            uint32_t value = 0;
            while (t < text.size() && text[t] >= _T('0') && text[t] <= _T('9'))
            {
                const auto digit = static_cast<uint32_t>(text[t++] - _T('0'));
                const uint32_t base = reg ? 10 : 8;
                if (digit >= base || value > (65535 - digit) / base) invalid = true;
                if (!invalid) value = value * base + digit;
            }
            if (t == start) return false;
            if (reg)
            {
                if (invalid || value > 7) error = Error::InvalidRegister;
                params.paramReg1 = static_cast<int>(value);
            }
            else
            {
                if (invalid && !error) error = Error::InvalidValue;
                (octalIndex++ == 0 ? params.paramOct1 : params.paramOct2) = static_cast<uint16_t>(value);
            }
        }
    }
    return t == text.size();
}

inline std::expected<Parsed, Error> Parse(const TCHAR* command)
{
    for (const auto& spec : Specifications)
    {
        ConsoleCommandParams params{command};
        std::optional<Error> error;
        if (!Match(spec.pattern, command, params, error)) continue;
        if (error) return std::unexpected(*error);
        return Parsed{spec.action, params};
    }
    return std::unexpected(Error::UnknownCommand);
}

// No target means execute a single instruction instead of running to a breakpoint.
inline std::optional<uint16_t> StepOverTarget(uint16_t opcode, uint16_t pc, int words)
{
    if ((opcode & ~uint16_t{077}) == PI_JMP || (opcode & ~uint16_t{0377}) == PI_BR ||
        words < 1 || words > 3) return std::nullopt;
    return static_cast<uint16_t>(pc + words * 2);
}
}
