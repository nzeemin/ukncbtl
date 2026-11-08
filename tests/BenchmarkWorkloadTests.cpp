/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include <gtest/gtest.h>
#include "../benchmarks/Workloads.h"

TEST(BenchmarkWorkloads, InstructionLoopExecutesAndStores)
{
    CMotherboard board;
    workloads::Prepare(board);
    for (auto* cpu : {board.GetCPU(), board.GetPPU()})
    {
        ASSERT_FALSE(cpu->IsStopped());
        for (int i = 0; i < 4; ++i) cpu->CommandExecution();
        EXPECT_EQ(cpu->GetPC(), workloads::ProgramAddress);
        EXPECT_EQ(cpu->GetReg(0), 1);
        EXPECT_EQ(cpu->GetReg(1), 1);
        EXPECT_EQ(cpu->GetMemoryController()->GetWord(workloads::DataAddress, false, false), 1);
    }
}

TEST(BenchmarkWorkloads, SystemFrameKeepsBothProcessorsRunning)
{
    CMotherboard board;
    workloads::Prepare(board);
    for (int i = 0; i < 10; ++i)
    {
        ASSERT_TRUE(board.SystemFrame());
        for (auto* cpu : {board.GetCPU(), board.GetPPU()})
        {
            EXPECT_FALSE(cpu->IsStopped());
            EXPECT_GT(cpu->GetReg(1), 0);
            EXPECT_GE(cpu->GetPC(), workloads::ProgramAddress);
            EXPECT_LT(cpu->GetPC(), workloads::ProgramAddress + 2 * workloads::Program.size());
        }
    }
}
