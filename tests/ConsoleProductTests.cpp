/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "ConsoleCommands.h"
#include <gtest/gtest.h>

TEST(ConsoleProductTests, TraceCommandsAreUnavailable)
{
    for (const auto text : {_T("t"), _T("tc"), _T("t123")})
        EXPECT_FALSE(console_command::Parse(text));
}

TEST(ConsoleProductTests, OrdinaryCommandsRemainAvailable)
{
    const auto result = console_command::Parse(_T("r7=177777"));
    ASSERT_TRUE(result);
    EXPECT_EQ(result->action, console_command::Action::SetRegisterValue);
    EXPECT_EQ(result->params.paramReg1, 7);
    EXPECT_EQ(result->params.paramOct1, 0177777);
}
