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

TEST(GoogleTestAdapter, SuccessfulAssertionsContinueNormally)
{
    int value=7;
    EXPECT_NO_THROW(RunLegacyTest([&] {
        Assert::IsTrue(true); Assert::IsFalse(false); Assert::IsNotNull(&value);
        Assert::AreEqual(7,value); Assert::AreEqual(std::wstring(L"same"),std::wstring(L"same"));
    }));
}
TEST(GoogleTestAdapter, BooleanAndPointerFailuresReportTheirMessages)
{
    EXPECT_NONFATAL_FAILURE(RunLegacyTest([] { Assert::IsTrue(false,L"true failure"); }),"true failure");
    EXPECT_NONFATAL_FAILURE(RunLegacyTest([] { Assert::IsFalse(true,L"false failure"); }),"false failure");
    EXPECT_NONFATAL_FAILURE(RunLegacyTest([] { Assert::IsNotNull(static_cast<int*>(nullptr),L"null failure"); }),"null failure");
}
TEST(GoogleTestAdapter, ExplicitFailureHasDefaultOrSuppliedMessage)
{
    EXPECT_NONFATAL_FAILURE(RunLegacyTest([] { Assert::Fail(); }),"assertion failed");
    EXPECT_NONFATAL_FAILURE(RunLegacyTest([] { Assert::Fail(L"explicit failure"); }),"explicit failure");
}
TEST(GoogleTestAdapter, NullWideStringsCompareSafely)
{
    const wchar_t* missing=nullptr;
    EXPECT_NO_THROW(Assert::AreEqual(missing,missing));
    EXPECT_NONFATAL_FAILURE(RunLegacyTest([&] { Assert::AreEqual(missing,L"present"); }),"present");
    EXPECT_NONFATAL_FAILURE(RunLegacyTest([&] { Assert::AreEqual(L"present",missing); }),"present");
}
TEST(GoogleTestAdapter, OriginalAssertionLocationAndValuesReachGoogleTest)
{
    testing::TestPartResultArray results;
    int expectedLine=0;
    {
        testing::ScopedFakeTestPartResultReporter reporter(testing::ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD,&results);
        RunLegacyTest([&] {
            expectedLine=__LINE__+1;
            Assert::AreEqual(11,22,L"location marker");
        });
    }
    ASSERT_EQ(1,results.size()); const auto& result=results.GetTestPartResult(0);
    EXPECT_STREQ(__FILE__,result.file_name()); EXPECT_EQ(expectedLine,result.line_number());
    EXPECT_EQ(testing::TestPartResult::kNonFatalFailure,result.type());
    const std::string message=result.message();
    EXPECT_NE(std::string::npos,message.find("Expected: 11")); EXPECT_NE(std::string::npos,message.find("Actual: 22"));
    EXPECT_NE(std::string::npos,message.find("location marker"));
}
TEST(GoogleTestAdapter, NestedFailureUnwindsResourcesAndStopsCaller)
{
    bool destroyed=false,continued=false;
    struct Guard { bool& destroyed; ~Guard() { destroyed=true; } };
    EXPECT_NONFATAL_FAILURE(RunLegacyTest([&] {
        Guard guard{destroyed}; auto helper=[] { Assert::Fail(L"nested"); }; helper(); continued=true;
    }),"nested");
    EXPECT_TRUE(destroyed); EXPECT_FALSE(continued);
}
TEST(GoogleTestAdapter, UnexpectedExceptionsAreNotSwallowed)
{
    EXPECT_THROW(RunLegacyTest([] { throw std::runtime_error("unexpected"); }),std::runtime_error);
    EXPECT_THROW(RunLegacyTest([] { throw 42; }),int);
}
TEST(GoogleTestAdapter, FailureDoesNotPreventNextLegacyCase)
{
    EXPECT_NONFATAL_FAILURE(RunLegacyTest([] { Assert::Fail(L"first"); }),"first");
    bool ran=false; RunLegacyTest([&] { ran=true; }); EXPECT_TRUE(ran);
}
