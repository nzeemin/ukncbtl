/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once
#include "CppUnitTest.h"
#include <cstdio>
#include <exception>

extern int testCount;
extern int failureCount;

template<class F> void RunLegacyTest(const char* name, F&& run)
{
    ++testCount;
    try { run(); }
    catch (const Microsoft::VisualStudio::CppUnitTestFramework::AssertionFailure& error)
    {
        ++failureCount;
        std::fprintf(stderr, "FAIL %s: %ls\n", name, error.message().c_str());
    }
    catch (const std::exception& error)
    {
        ++failureCount;
        std::fprintf(stderr, "FAIL %s: %s\n", name, error.what());
    }
    catch (...)
    {
        ++failureCount;
        std::fprintf(stderr, "FAIL %s: unexpected exception\n", name);
    }
}
