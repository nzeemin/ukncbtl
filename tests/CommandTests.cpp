/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include "ConsoleCommands.h"
#include "CommandHistory.h"
#include "CommandLineOptions.h"
#include <gtest/gtest.h>
#include <array>
#include <shellapi.h>

namespace {
using console_command::Action;
using console_command::Error;
struct CommandCase
{
    const TCHAR* text;
    Action action;
    int reg = -1;
    uint16_t first = 0, second = 0;
};
class ConsoleCommandTests : public testing::TestWithParam<CommandCase> {};
TEST_P(ConsoleCommandTests, ParsesActionAndEveryOperand)
{
    const auto& expected = GetParam();
    const auto result = console_command::Parse(expected.text);
    ASSERT_TRUE(result) << testing::PrintToString(expected.text);
    EXPECT_EQ(result->action, expected.action);
    EXPECT_EQ(result->params.paramReg1, expected.reg);
    EXPECT_EQ(result->params.paramOct1, expected.first);
    EXPECT_EQ(result->params.paramOct2, expected.second);
    EXPECT_STREQ(result->params.commandText, expected.text);
}
INSTANTIATE_TEST_SUITE_P(AllForms, ConsoleCommandTests, testing::Values(
    CommandCase{_T("h"), Action::ShowHelp},
    CommandCase{_T("c"), Action::ClearConsoleLog},
    CommandCase{_T("p"), Action::SwitchCpuPpu},
    CommandCase{_T("r0=177777"), Action::SetRegisterValue, 0, 0177777},
    CommandCase{_T("r7 123"), Action::SetRegisterValue, 7, 0123},
    CommandCase{_T("r3"), Action::PrintRegister, 3},
    CommandCase{_T("r"), Action::PrintAllRegisters},
    CommandCase{_T("rps=417"), Action::SetRegisterPSW, -1, 0417},
    CommandCase{_T("rps 0"), Action::SetRegisterPSW, -1, 0},
    CommandCase{_T("rps"), Action::PrintRegisterPSW},
    CommandCase{_T("s"), Action::StepInto},
    CommandCase{_T("so"), Action::StepOver},
    CommandCase{_T("d1000"), Action::PrintDisassembleAtAddress, -1, 01000},
    CommandCase{_T("D1000"), Action::PrintDisassembleAtAddress, -1, 01000},
    CommandCase{_T("d"), Action::PrintDisassembleAtPC},
    CommandCase{_T("D"), Action::PrintDisassembleAtPC},
    CommandCase{_T("u1000 2000"), Action::SaveMemoryFragment, -1, 01000, 02000},
    CommandCase{_T("u"), Action::SaveMemoryDump},
    CommandCase{_T("udl"), Action::SaveDisplayListDump},
    CommandCase{_T("m1000 12345"), Action::SetMemoryAtAddress, -1, 01000, 012345},
    CommandCase{_T("m1000=12345"), Action::SetMemoryAtAddress, -1, 01000, 012345},
    CommandCase{_T("m1000"), Action::PrintMemoryDumpAtAddress, -1, 01000},
    CommandCase{_T("mr7"), Action::PrintMemoryDumpAtRegister, 7},
    CommandCase{_T("m"), Action::PrintMemoryDumpAtPC},
    CommandCase{_T("g177776"), Action::RunToAddress, -1, 0177776},
    CommandCase{_T("g"), Action::Run},
    CommandCase{_T("b0"), Action::SetBreakpointAtAddress, -1, 0},
    CommandCase{_T("b"), Action::PrintAllBreakpoints},
    CommandCase{_T("bc123"), Action::RemoveBreakpointAtAddress, -1, 0123},
    CommandCase{_T("bc"), Action::RemoveAllBreakpoints},
    CommandCase{_T("fc123 456"), Action::CalculateFloatNumber, -1, 0123, 0456},
    CommandCase{_T("t123"), Action::TraceLogWithMask, -1, 0123},
    CommandCase{_T("t"), Action::TraceLogOnOff},
    CommandCase{_T("tc"), Action::ClearTraceLog},
    CommandCase{_T("r 7\t177777"), Action::SetRegisterValue, 7, 0177777},
    CommandCase{_T("m 000100\t000200"), Action::SetMemoryAtAddress, -1, 0100, 0200}
));

class RejectedConsoleCommands : public testing::TestWithParam<const TCHAR*> {};
TEST_P(RejectedConsoleCommands, NeverReturnsAnExecutableAction)
{ EXPECT_FALSE(console_command::Parse(GetParam())); }
INSTANTIATE_TEST_SUITE_P(Malformed, RejectedConsoleCommands, testing::Values(
    _T(""), _T("unknown"), _T("g100junk"), _T("r0=1junk"), _T("r0="), _T("r0 1 2"),
    _T("m100="), _T("m100 200junk"), _T("u100"), _T("fc1"), _T("fc1 2 3"),
    _T("rps="), _T("b123junk"), _T("bc123junk"), _T("sojunk"), _T("rpsjunk"),
    _T("g8"), _T("m9"), _T("g200000"), _T("r0=200000"), _T("m0 200000"),
    _T("g-1"), _T("g+1"), _T("r8"), _T("r-1"), _T("mr8"),
    _T("r999999999999999999999999999999999999999999999"),
    _T("g777777777777777777777777777777777777777777777")
));

TEST(ConsoleParsing, DistinguishesRegisterAndValueErrors)
{
    EXPECT_EQ(console_command::Parse(_T("r8=1")).error(), Error::InvalidRegister);
    EXPECT_EQ(console_command::Parse(_T("mr-1")).error(), Error::InvalidRegister);
    EXPECT_EQ(console_command::Parse(_T("r0=8")).error(), Error::InvalidValue);
    EXPECT_EQ(console_command::Parse(_T("g200000")).error(), Error::InvalidValue);
    EXPECT_EQ(console_command::Parse(_T("what")).error(), Error::UnknownCommand);
}

TEST(ConsoleParsing, TraceCommandsFollowProductBuildConfiguration)
{
#if defined(PRODUCT)
    for (const auto text : {_T("t"), _T("tc"), _T("t123")}) EXPECT_FALSE(console_command::Parse(text));
#else
    for (const auto text : {_T("t"), _T("tc"), _T("t123")}) EXPECT_TRUE(console_command::Parse(text));
#endif
}

TEST(StepOverTests, JumpsAndUnconditionalBranchesAlwaysUseSingleStep)
{
    for (uint16_t operand = 0; operand <= 077; ++operand)
        EXPECT_FALSE(console_command::StepOverTarget(PI_JMP | operand, 01000, 2));
    for (uint16_t displacement = 0; displacement <= 0377; ++displacement)
        EXPECT_FALSE(console_command::StepOverTarget(PI_BR | displacement, 01000, 1));
}

TEST(StepOverTests, OtherInstructionsUseLengthAndWrapAtAddressBoundary)
{
    for (const uint16_t opcode : {004767, 0010000, 0001000}) // JSR, MOV, BNE
        for (int length = 1; length <= 3; ++length)
        {
            EXPECT_EQ(console_command::StepOverTarget(opcode, 01000, length), 01000 + 2 * length);
            EXPECT_EQ(console_command::StepOverTarget(opcode, 0177776, length), 2 * (length - 1));
        }
    EXPECT_FALSE(console_command::StepOverTarget(0010000, 01000, 0));
    EXPECT_FALSE(console_command::StepOverTarget(0010000, 01000, 4));
}

TEST(CommandHistoryTests, EmptyHistoryDoesNotReplaceTheEditedLineOnDown)
{
    CommandHistory history;
    history.Up();
    EXPECT_FALSE(history.Down());
    EXPECT_STREQ(history.Current(), _T(""));
    history.Add(_T(""));
    EXPECT_EQ(history.Size(), 0u);
}

TEST(CommandHistoryTests, NavigatesNewestFirstAndClampsAtBothEnds)
{
    CommandHistory history;
    history.Add(_T("r")); history.Add(_T("s"));
    history.Up(); EXPECT_STREQ(history.Current(), _T("s"));
    history.Up(); EXPECT_STREQ(history.Current(), _T("r"));
    history.Up(); EXPECT_STREQ(history.Current(), _T("r"));
    EXPECT_TRUE(history.Down()); EXPECT_STREQ(history.Current(), _T("s"));
    EXPECT_TRUE(history.Down()); EXPECT_STREQ(history.Current(), _T(""));
    EXPECT_FALSE(history.Down());
}

TEST(CommandHistoryTests, DuplicateMovesToNewestWithoutIncreasingCount)
{
    CommandHistory history;
    for (auto text : {_T("r"), _T("s"), _T("g"), _T("r")}) history.Add(text);
    EXPECT_EQ(history.Size(), 3u);
    history.Up(); EXPECT_STREQ(history.Current(), _T("r"));
    history.Up(); EXPECT_STREQ(history.Current(), _T("g"));
    history.Up(); EXPECT_STREQ(history.Current(), _T("s"));
}

TEST(CommandHistoryTests, CapacityDropsOldestAndCopiesInput)
{
    CommandHistory history;
    for (size_t i = 0; i < CommandHistory::Capacity + 5; ++i)
    {
        auto text = std::to_wstring(i);
        history.Add(text.c_str());
        text.clear();
    }
    EXPECT_EQ(history.Size(), CommandHistory::Capacity);
    for (size_t i = CommandHistory::Capacity + 5; i > 5; --i)
    {
        history.Up();
        EXPECT_EQ(std::wstring(history.Current()), std::to_wstring(i - 1));
    }
    history.Up(); EXPECT_STREQ(history.Current(), _T("5"));
}

TEST(CommandHistoryTests, EditingAndAddingRestartNavigationAtNewest)
{
    CommandHistory history;
    history.Add(_T("r")); history.Add(_T("s"));
    history.Up(); history.Up(); history.ResetNavigation();
    EXPECT_FALSE(history.Down());
    history.Up(); EXPECT_STREQ(history.Current(), _T("s"));
    history.Add(_T("g"));
    EXPECT_STREQ(history.Current(), _T(""));
    history.Up(); EXPECT_STREQ(history.Current(), _T("g"));
    history.Add(_T(""));
    EXPECT_STREQ(history.Current(), _T(""));
    EXPECT_EQ(history.Size(), 3u);
}

CommandLineOptions Options(std::initializer_list<std::basic_string_view<TCHAR>> arguments)
{ return ParseCommandLineOptions(std::span{arguments.begin(), arguments.size()}); }

TEST(CommandLineTests, EmptyInputLeavesPersistentOptionsUnspecified)
{
    const auto options = Options({});
    EXPECT_FALSE(options.help); EXPECT_EQ(options.boot, 0);
    EXPECT_FALSE(options.autostart); EXPECT_FALSE(options.debug);
    EXPECT_FALSE(options.sound); EXPECT_FALSE(options.fullscreen);
    for (const auto& path : options.disks) EXPECT_FALSE(path);
    for (const auto& path : options.cartridges) EXPECT_FALSE(path);
    for (const auto& path : options.hardDrives) EXPECT_FALSE(path);
}

struct OptionAlias { const TCHAR* text; std::optional<bool> CommandLineOptions::* member; bool value; };
class OptionAliases : public testing::TestWithParam<OptionAlias> {};
TEST_P(OptionAliases, SelectsExpectedSetting)
{
    const auto& alias = GetParam();
    const auto options = Options({alias.text});
    EXPECT_EQ(options.*alias.member, alias.value);
}
INSTANTIATE_TEST_SUITE_P(AllAliases, OptionAliases, testing::Values(
    OptionAlias{_T("/autostart"), &CommandLineOptions::autostart, true},
    OptionAlias{_T("/autostarton"), &CommandLineOptions::autostart, true},
    OptionAlias{_T("/autostartoff"), &CommandLineOptions::autostart, false},
    OptionAlias{_T("/noautostart"), &CommandLineOptions::autostart, false},
    OptionAlias{_T("/debug"), &CommandLineOptions::debug, true},
    OptionAlias{_T("/debugon"), &CommandLineOptions::debug, true},
    OptionAlias{_T("/debugger"), &CommandLineOptions::debug, true},
    OptionAlias{_T("/debugoff"), &CommandLineOptions::debug, false},
    OptionAlias{_T("/nodebug"), &CommandLineOptions::debug, false},
    OptionAlias{_T("/sound"), &CommandLineOptions::sound, true},
    OptionAlias{_T("/soundon"), &CommandLineOptions::sound, true},
    OptionAlias{_T("/soundoff"), &CommandLineOptions::sound, false},
    OptionAlias{_T("/nosound"), &CommandLineOptions::sound, false},
    OptionAlias{_T("/fullscreen"), &CommandLineOptions::fullscreen, true},
    OptionAlias{_T("/fullscreenon"), &CommandLineOptions::fullscreen, true},
    OptionAlias{_T("/windowed"), &CommandLineOptions::fullscreen, false},
    OptionAlias{_T("/fullscreenoff"), &CommandLineOptions::fullscreen, false}
));

TEST(CommandLineTests, HelpAndBootForms)
{
    EXPECT_TRUE(Options({_T("/h")}).help);
    EXPECT_TRUE(Options({_T("/help")}).help);
    EXPECT_EQ(Options({_T("/boot")}).boot, 1);
    for (int i = 1; i <= 7; ++i)
    {
        const auto text = std::wstring(L"/boot") + std::to_wstring(i);
        EXPECT_EQ(Options({text}).boot, i);
    }
    for (auto text : {_T("/boot0"), _T("/boot8"), _T("/boot12"), _T("/boot1junk"), _T("/bootjunk")})
        EXPECT_EQ(Options({text}).boot, 0);
}

TEST(CommandLineTests, LastValidOptionWinsAndInvalidOptionsDoNotEraseIt)
{
    const auto options = Options({_T("/sound"), _T("/nosound"), _T("/debugoff"), _T("/debug"),
        _T("/autostart"), _T("/noautostart"), _T("/windowed"), _T("/fullscreen"),
        _T("/boot7"), _T("/boot8"), _T("/disk0:first"), _T("/disk0:last"), _T("/disk0:")});
    EXPECT_EQ(options.sound, false); EXPECT_EQ(options.debug, true);
    EXPECT_EQ(options.autostart, false); EXPECT_EQ(options.fullscreen, true);
    EXPECT_EQ(options.boot, 7); EXPECT_EQ(options.disks[0], _T("last"));
}

TEST(CommandLineTests, DeviceSlotsAreIndependentAndPathsAreOwned)
{
    const auto options = Options({_T("/disk0:a"), _T("/disk1:b"), _T("/disk2:c"), _T("/disk3:d"),
        _T("/cart1:one"), _T("/cart2:two"), _T("/hard1:C:\\images\\one.hdi"), _T("/hard2:two")});
    for (int i = 0; i < 4; ++i)
        EXPECT_EQ(options.disks[i], std::wstring(1, static_cast<wchar_t>(L'a' + i)));
    EXPECT_EQ(options.cartridges[0], _T("one")); EXPECT_EQ(options.cartridges[1], _T("two"));
    EXPECT_EQ(options.hardDrives[0], _T("C:\\images\\one.hdi")); EXPECT_EQ(options.hardDrives[1], _T("two"));
    auto source = std::wstring(L"/disk0:owned");
    const auto owned = Options({source});
    source.clear(); EXPECT_EQ(owned.disks[0], _T("owned"));
}

TEST(CommandLineTests, InvalidSlotsAndUnknownOptionsHaveNoEffect)
{
    const auto options = Options({_T("/disk4:x"), _T("/disk-1:x"), _T("/disk0"), _T("/disk0:"),
        _T("/disk0=x"), _T("/cart0:x"), _T("/cart3:x"), _T("/hard0:x"), _T("/hard3:x"),
        _T("/unknown"), _T("/Sound"), _T("/debugjunk"), _T("/huh")});
    for (const auto& path : options.disks) EXPECT_FALSE(path);
    for (const auto& path : options.cartridges) EXPECT_FALSE(path);
    for (const auto& path : options.hardDrives) EXPECT_FALSE(path);
    EXPECT_FALSE(options.sound); EXPECT_FALSE(options.debug); EXPECT_FALSE(options.help);
}

TEST(CommandLineTests, WindowsTokenizationPreservesQuotedUnicodePaths)
{
    int count = 0;
    auto args = CommandLineToArgvW(L"ukncbtl.exe \"/disk0:C:\\disk images\\\u0423\u041a\u041d\u0426.dsk\" /soundoff", &count);
    ASSERT_NE(args, nullptr);
    std::vector<std::wstring_view> views;
    for (int i = 1; i < count; ++i) views.emplace_back(args[i]);
    const auto options = ParseCommandLineOptions(views);
    LocalFree(args);
    EXPECT_EQ(options.disks[0], L"C:\\disk images\\\u0423\u041a\u041d\u0426.dsk");
    EXPECT_EQ(options.sound, false);
}
}
