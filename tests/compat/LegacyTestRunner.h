/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once
#include "CppUnitTest.h"
// Throwing out of a legacy assertion preserves its original behavior: abort the
// entire case, including nested helpers, then let GoogleTest run the next case.
template<class F> void RunLegacyTest(F&& run)
{
    try { run(); }
    catch (const Microsoft::VisualStudio::CppUnitTestFramework::AssertionFailure& error)
    {
        ADD_FAILURE_AT(error.location().file_name(), static_cast<int>(error.location().line()))
            << error.what();
    }
}
