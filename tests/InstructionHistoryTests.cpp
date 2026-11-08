/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include "InstructionHistory.h"
#include <algorithm>
#include <gtest/gtest.h>

namespace {
class InstructionHistoryTests : public testing::Test
{
protected:
    InstructionHistory history;

    void Record(bool cpu, uint16_t sequence)
    {
        history.RecordInstruction(cpu, sequence, {sequence, 01234, 05670}, 0417);
    }
};
}

TEST_F(InstructionHistoryTests, BothProcessorsStartEmpty)
{
    EXPECT_EQ(history.Length(true), 0);
    EXPECT_EQ(history.Length(false), 0);
}

TEST_F(InstructionHistoryTests, InstructionPreservesAllFields)
{
    const std::array<uint16_t, 3> words{012700, 0177777, 012345};
    history.RecordInstruction(true, 0177776, words, 0417);
    ASSERT_EQ(history.Length(true), 1);
    const auto& record = history.Get(true, 0);
    EXPECT_EQ(record.pc, 0177776);
    EXPECT_EQ(record.words, words);
    EXPECT_EQ(record.psw, 0417);
    EXPECT_FALSE(record.interrupt);
    EXPECT_EQ(history.Length(false), 0);
}

TEST_F(InstructionHistoryTests, InstructionCopiesSourceWords)
{
    std::array<uint16_t, 3> words{1, 2, 3};
    history.RecordInstruction(false, 01000, words, 0);
    words.fill(0);
    EXPECT_EQ(history.Get(false, 0).words, (std::array<uint16_t, 3>{1, 2, 3}));
}

TEST_F(InstructionHistoryTests, InterruptStoresOldPcAndNewMachineState)
{
    history.RecordInterrupt(false, 01234, 0170174, 05670, 0401);
    ASSERT_EQ(history.Length(false), 1);
    const auto& record = history.Get(false, 0);
    EXPECT_EQ(record.pc, 01234);
    EXPECT_EQ(record.words, (std::array<uint16_t, 3>{0170174, 05670, 0401}));
    EXPECT_EQ(record.psw, 0401);
    EXPECT_TRUE(record.interrupt);
    EXPECT_EQ(history.Length(true), 0);
}

TEST_F(InstructionHistoryTests, NewestRecordIsFirst)
{
    for (uint16_t sequence = 0; sequence < 10; ++sequence) Record(true, sequence);
    ASSERT_EQ(history.Length(true), 10);
    for (int index = 0; index < 10; ++index)
        EXPECT_EQ(history.Get(true, index).pc, 9 - index);
}

TEST_F(InstructionHistoryTests, ExactlyFullBufferRetainsEveryRecord)
{
    for (uint16_t sequence = 0; sequence < INSTR_HISTORY_SIZE; ++sequence) Record(false, sequence);
    ASSERT_EQ(history.Length(false), INSTR_HISTORY_SIZE);
    for (int index = 0; index < INSTR_HISTORY_SIZE; ++index)
        EXPECT_EQ(history.Get(false, index).pc, INSTR_HISTORY_SIZE - 1 - index);
}

TEST_F(InstructionHistoryTests, FirstOverflowDropsOnlyOldestRecord)
{
    for (uint16_t sequence = 0; sequence <= INSTR_HISTORY_SIZE; ++sequence) Record(true, sequence);
    ASSERT_EQ(history.Length(true), INSTR_HISTORY_SIZE);
    for (int index = 0; index < INSTR_HISTORY_SIZE; ++index)
        EXPECT_EQ(history.Get(true, index).pc, INSTR_HISTORY_SIZE - index);
}

TEST_F(InstructionHistoryTests, MultipleWrapsPreserveOrderAndSaturateLength)
{
    constexpr int count = INSTR_HISTORY_SIZE * 5 + 17;
    for (uint16_t sequence = 0; sequence < count; ++sequence)
    {
        Record(true, sequence);
        EXPECT_EQ(history.Length(true), (std::min)(static_cast<int>(sequence) + 1, INSTR_HISTORY_SIZE));
    }
    for (int index = 0; index < INSTR_HISTORY_SIZE; ++index)
    {
        const auto& record = history.Get(true, index);
        EXPECT_EQ(record.pc, count - 1 - index);
        EXPECT_EQ(record.words[0], record.pc);
        EXPECT_EQ(record.psw, 0417);
    }
}

TEST_F(InstructionHistoryTests, CpuWrapDoesNotChangePpuRecords)
{
    Record(false, 01000);
    history.RecordInterrupt(false, 02000, 0100, 03000, 0400);
    for (uint16_t sequence = 0; sequence < INSTR_HISTORY_SIZE * 2; ++sequence) Record(true, sequence);
    ASSERT_EQ(history.Length(false), 2);
    EXPECT_EQ(history.Get(false, 0).pc, 02000);
    EXPECT_TRUE(history.Get(false, 0).interrupt);
    EXPECT_EQ(history.Get(false, 1).pc, 01000);
    Record(false, 04000);
    EXPECT_EQ(history.Get(true, 0).pc, INSTR_HISTORY_SIZE * 2 - 1);
}

TEST_F(InstructionHistoryTests, MixedRecordsShareChronologicalOrder)
{
    Record(true, 01000);
    history.RecordInterrupt(true, 01002, 0004, 02000, 0400);
    Record(true, 02000);
    ASSERT_EQ(history.Length(true), 3);
    EXPECT_EQ(history.Get(true, 0).pc, 02000);
    EXPECT_FALSE(history.Get(true, 0).interrupt);
    EXPECT_EQ(history.Get(true, 1).pc, 01002);
    EXPECT_TRUE(history.Get(true, 1).interrupt);
    EXPECT_EQ(history.Get(true, 2).pc, 01000);
}

TEST_F(InstructionHistoryTests, OverwritingInterruptWithInstructionReplacesEveryField)
{
    history.RecordInterrupt(true, 0177777, 0177777, 0177777, 0177777);
    for (int i = 0; i < INSTR_HISTORY_SIZE; ++i) Record(true, 0);
    const auto& record = history.Get(true, 0);
    EXPECT_FALSE(record.interrupt);
    EXPECT_EQ(record.pc, 0);
    EXPECT_EQ(record.words, (std::array<uint16_t, 3>{0, 01234, 05670}));
    EXPECT_EQ(record.psw, 0417);
    history.RecordInterrupt(true, 1, 2, 3, 4);
    EXPECT_TRUE(history.Get(true, 0).interrupt);
    EXPECT_EQ(history.Get(true, 0).words, (std::array<uint16_t, 3>{2, 3, 4}));
}

TEST_F(InstructionHistoryTests, ClearResetsBothWrappedRingsAndAllowsReuse)
{
    for (uint16_t sequence = 0; sequence < INSTR_HISTORY_SIZE + 3; ++sequence)
    {
        Record(true, sequence);
        Record(false, sequence);
    }
    history.Clear();
    history.Clear();
    EXPECT_EQ(history.Length(true), 0);
    EXPECT_EQ(history.Length(false), 0);
    Record(true, 01234);
    history.RecordInterrupt(false, 05670, 0004, 01000, 0400);
    ASSERT_EQ(history.Length(true), 1);
    ASSERT_EQ(history.Length(false), 1);
    EXPECT_EQ(history.Get(true, 0).pc, 01234);
    EXPECT_FALSE(history.Get(true, 0).interrupt);
    EXPECT_EQ(history.Get(false, 0).pc, 05670);
    EXPECT_TRUE(history.Get(false, 0).interrupt);
}

TEST(InstructionHistoryNames, RecognizesVectorsWithEverySelHighByte)
{
    const std::array<std::pair<uint16_t, const TCHAR*>, 11> names{{
        {0000, _T("START")}, {0004, _T("BUS")}, {0010, _T("RSVD")},
        {0014, _T("BPT/TBIT")}, {0020, _T("IOT")}, {0024, _T("ACLO")},
        {0030, _T("EMT")}, {0034, _T("TRAP")}, {0100, _T("EVNT")},
        {0170, _T("HALT")}, {0174, _T("HALT BUS")}}};
    for (const auto& [vector, name] : names)
        for (unsigned high = 0; high < 256; ++high)
            EXPECT_STREQ(InstructionHistory::InterruptName(static_cast<uint16_t>((high << 8) | vector)), name)
                << "vector " << vector << ", SEL " << high;
}

TEST(InstructionHistoryNames, OtherLowBytesUseVirqIncludingOddVectors)
{
    constexpr std::array<uint16_t, 11> known{0000, 0004, 0010, 0014, 0020, 0024, 0030, 0034, 0100, 0170, 0174};
    for (uint16_t low = 0; low < 256; ++low)
        if (!std::ranges::contains(known, low))
            for (unsigned high : {0u, 1u, 255u})
                EXPECT_STREQ(InstructionHistory::InterruptName(static_cast<uint16_t>((high << 8) | low)), _T("VIRQ"));
}
