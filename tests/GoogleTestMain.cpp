/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include <gtest/gtest.h>

void TestModuleInitialize();
void TestModuleCleanup();

namespace {
class CoreEnvironment final : public testing::Environment
{
public:
    void SetUp() override { TestModuleInitialize(); }
    void TearDown() override { TestModuleCleanup(); }
};
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new CoreEnvironment);
    return RUN_ALL_TESTS();
}
