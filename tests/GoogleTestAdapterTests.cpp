/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include "LegacyTestRunner.h"
#include <gtest/gtest-spi.h>

using Microsoft::VisualStudio::CppUnitTestFramework::Assert;

TEST(GoogleTestAdapter, ReportsMismatchAndAbortsLegacyCase)
{
    bool continued = false;
    EXPECT_NONFATAL_FAILURE(RunLegacyTest([&] {
        Assert::AreEqual(1, 2, L"deliberate mismatch");
        continued = true;
    }), "Actual: 2");
    EXPECT_FALSE(continued);
}

TEST(GoogleTestAdapter, ComparesWideStringContents)
{
    const wchar_t first[] = L"same";
    const wchar_t second[] = L"same";
    RunLegacyTest([&] { Assert::AreEqual(first, second); });
    EXPECT_NONFATAL_FAILURE(RunLegacyTest([] {
        Assert::AreEqual(L"first", L"second");
    }), "second");
}
