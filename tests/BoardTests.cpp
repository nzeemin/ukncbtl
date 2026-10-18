/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// BoardTests.cpp : smoke tests for CMotherboard

#include "stdafx.h"
#include <CppUnitTest.h>
#include "emubase/Emubase.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

TEST_CLASS(BoardSmokeTests)
{
public:
    TEST_METHOD(Board_CreateAndReset)
    {
        CMotherboard board;
        board.Reset();

        Assert::IsNotNull(board.GetCPU());
        Assert::IsNotNull(board.GetPPU());
        Assert::IsNotNull(board.GetCPUMemoryController());
        Assert::IsNotNull(board.GetPPUMemoryController());
    }

    TEST_METHOD(Board_RamWordRoundTrip)
    {
        CMotherboard board;
        board.Reset();

        for (int plan = 0; plan < 3; plan++)
        {
            board.SetRAMWord(plan, 01000, 0123456);
            Assert::AreEqual(0123456u, (unsigned int) board.GetRAMWord(plan, 01000));
        }
    }
};
