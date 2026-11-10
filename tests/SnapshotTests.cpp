/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include "Snapshot.h"
#include "InstructionHistory.h"
#include "emubase/Emubase.h"
#include "util/File.h"
#include "util/LittleEndian.h"
#include <gtest/gtest.h>
#include <array>
#include <cstdlib>
#include <memory>
#include <vector>

#ifdef _DEBUG
int AssertFailedLine(const char*, int) { std::abort(); }
#endif
void DebugLog(LPCTSTR) {}
const TCHAR* REGISTER_NAME[] = {_T("R0"), _T("R1"), _T("R2"), _T("R3"),
                               _T("R4"), _T("R5"), _T("SP"), _T("PC")};

namespace {
class SnapshotTests : public testing::Test
{
protected:
    std::array<TCHAR, MAX_PATH> path{};
    std::unique_ptr<CMotherboard> board;
    InstructionHistory history;
    uint32_t uptime = 123;
    int stops = 0;

    void SetUp() override
    {
        CProcessor::Init();
        board = std::make_unique<CMotherboard>();
        board->Reset();
        std::array<TCHAR, MAX_PATH> directory{};
        ASSERT_NE(GetTempPath(MAX_PATH, directory.data()), 0u);
        ASSERT_NE(GetTempFileName(directory.data(), _T("snp"), 0, path.data()), 0u);
        board->GetCPU()->SetReg(0, 012345);
        board->GetPPU()->SetReg(1, 065432);
        for (int plane = 0; plane < 3; ++plane)
            for (uint32_t address = 0; address < 65536; ++address)
                board->SetRAMByte(plane, static_cast<uint16_t>(address),
                    static_cast<uint8_t>(address * 17 + plane));
        history.RecordInstruction(true, 01000, {1, 2, 3}, 0400);
        history.RecordInterrupt(false, 02000, 0004, 03000, 0417);
    }

    void TearDown() override
    {
        board.reset();
        CProcessor::Done();
        if (path[0]) DeleteFile(path.data());
    }

    std::vector<uint8_t> State()
    {
        std::vector<uint8_t> bytes(UKNCIMAGE_SIZE, 0);
        board->SaveToImage(std::span<uint8_t, UKNCIMAGE_SIZE>{bytes});
        return bytes;
    }

    std::vector<uint8_t> ReadFileBytes()
    {
        UniqueFile file(_tfopen(path.data(), _T("rb")));
        if (!file) { ADD_FAILURE() << "Cannot read fixture"; return {}; }
        std::vector<uint8_t> bytes(UKNCIMAGE_SIZE);
        if (std::fread(bytes.data(), 1, bytes.size(), file.get()) != bytes.size())
            ADD_FAILURE() << "Incomplete fixture";
        return bytes;
    }

    void WriteFileBytes(std::span<const uint8_t> bytes)
    {
        UniqueFile file(_tfopen(path.data(), _T("wb")));
        ASSERT_TRUE(file);
        if (!bytes.empty()) ASSERT_EQ(std::fwrite(bytes.data(), 1, bytes.size(), file.get()), bytes.size());
        ASSERT_EQ(std::fclose(file.release()), 0);
    }

    bool Load(const TCHAR* filename)
    {
        return snapshot::Load(filename, *board, uptime, history, [&] { ++stops; });
    }

    void ExpectRejected(const TCHAR* filename)
    {
        const auto before = State();
        const auto oldUptime = uptime;
        EXPECT_FALSE(Load(filename));
        EXPECT_EQ(stops, 0);
        EXPECT_EQ(uptime, oldUptime);
        EXPECT_EQ(State(), before);
        ASSERT_EQ(history.Length(true), 1);
        ASSERT_EQ(history.Length(false), 1);
        EXPECT_EQ(history.Get(true, 0).pc, 01000);
        EXPECT_EQ(history.Get(true, 0).words, (std::array<uint16_t, 3>{1, 2, 3}));
        EXPECT_EQ(history.Get(false, 0).words, (std::array<uint16_t, 3>{0004, 03000, 0417}));
    }
};

TEST_F(SnapshotTests, SavesExpectedHeaderAndExactFileSize)
{
    const auto before = State();
    ASSERT_TRUE(snapshot::Save(path.data(), *board, 0x78563412));
    const auto bytes = ReadFileBytes();
    ASSERT_EQ(bytes.size(), UKNCIMAGE_SIZE);
    const std::array<uint8_t, 12> prefix{'U','K','N','C','B','T','L','!',1,0,1,0};
    EXPECT_TRUE(std::equal(prefix.begin(), prefix.end(), bytes.begin()));
    EXPECT_EQ(little_endian::read<uint32_t>(std::span{bytes}.subspan<12, 4>()), UKNCIMAGE_SIZE);
    EXPECT_EQ(bytes[16], 0x12);
    EXPECT_EQ(bytes[19], 0x78);
    for (size_t i = 20; i < 32; ++i) EXPECT_EQ(bytes[i], 0);
    UniqueFile file(_tfopen(path.data(), _T("rb")));
    ASSERT_TRUE(file);
    ASSERT_EQ(std::fseek(file.get(), 0, SEEK_END), 0);
    EXPECT_EQ(std::ftell(file.get()), UKNCIMAGE_SIZE);
    EXPECT_EQ(State(), before);
    EXPECT_EQ(history.Length(true), 1);
}

TEST_F(SnapshotTests, ValidLoadRestoresBoardAndUptimeAndClearsBothHistories)
{
    const auto saved = State();
    ASSERT_TRUE(snapshot::Save(path.data(), *board, UINT32_MAX));
    board->GetCPU()->SetReg(0, 0);
    board->GetPPU()->SetReg(1, 0);
    board->SetRAMWord(0, 0, 0);
    board->SetRAMWord(1, 65534, 0);
    board->SetRAMWord(2, 32768, 0);
    EXPECT_TRUE(Load(path.data()));
    EXPECT_EQ(stops, 1);
    EXPECT_EQ(uptime, UINT32_MAX);
    EXPECT_EQ(State(), saved);
    EXPECT_EQ(history.Length(true), 0);
    EXPECT_EQ(history.Length(false), 0);
}

TEST_F(SnapshotTests, StopRunsOnceBeforeStateIsRestored)
{
    ASSERT_TRUE(snapshot::Save(path.data(), *board, 0));
    board->GetCPU()->SetReg(0, 07654);
    ASSERT_TRUE(snapshot::Load(path.data(), *board, uptime, history, [&] {
        ++stops;
        EXPECT_EQ(board->GetCPU()->GetReg(0), 07654);
        EXPECT_EQ(uptime, 123u);
        EXPECT_EQ(history.Length(true), 1);
    }));
    EXPECT_EQ(stops, 1);
    EXPECT_EQ(board->GetCPU()->GetReg(0), 012345);
    EXPECT_EQ(uptime, 0u);
}

TEST_F(SnapshotTests, SaveLoadSaveProducesIdenticalFile)
{
    ASSERT_TRUE(snapshot::Save(path.data(), *board, uptime));
    const auto original = ReadFileBytes();
    ASSERT_TRUE(Load(path.data()));
    ASSERT_TRUE(snapshot::Save(path.data(), *board, uptime));
    EXPECT_EQ(ReadFileBytes(), original);
}

TEST_F(SnapshotTests, NullAndMissingPathsAreRejectedWithoutStateChanges)
{
    ExpectRejected(nullptr);
    const auto missing = std::basic_string<TCHAR>(path.data()) + _T(".missing");
    ExpectRejected(missing.c_str());
    EXPECT_FALSE(snapshot::Save(nullptr, *board, uptime));
}

TEST_F(SnapshotTests, ExclusiveFileLockRejectsReadsAndWrites)
{
    ASSERT_TRUE(snapshot::Save(path.data(), *board, uptime));
    const auto original = ReadFileBytes();
    const auto locked = CreateFile(path.data(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    ASSERT_NE(locked, INVALID_HANDLE_VALUE);
    // Release even if an assertion in a helper fails.
    struct HandleCloser { void operator()(void* handle) const { CloseHandle(handle); } };
    {
        std::unique_ptr<void, HandleCloser> lock(locked);
        ExpectRejected(path.data());
        EXPECT_FALSE(snapshot::Save(path.data(), *board, uptime));
    }
    EXPECT_EQ(ReadFileBytes(), original);
}

TEST_F(SnapshotTests, TrailingDataRemainsAcceptedForCompatibility)
{
    ASSERT_TRUE(snapshot::Save(path.data(), *board, uptime));
    auto bytes = ReadFileBytes();
    bytes.insert(bytes.end(), {0xde, 0xad, 0xbe, 0xef});
    WriteFileBytes(bytes);
    EXPECT_TRUE(Load(path.data()));
    EXPECT_EQ(stops, 1);
}

TEST_F(SnapshotTests, ReservedHeaderBytesAreIgnored)
{
    ASSERT_TRUE(snapshot::Save(path.data(), *board, uptime));
    auto bytes = ReadFileBytes();
    std::fill(bytes.begin() + 20, bytes.begin() + 32, 0xff);
    WriteFileBytes(bytes);
    EXPECT_TRUE(Load(path.data()));
}

class TruncatedSnapshots : public SnapshotTests, public testing::WithParamInterface<size_t> {};
TEST_P(TruncatedSnapshots, RejectsWithoutChangingState)
{
    ASSERT_TRUE(snapshot::Save(path.data(), *board, uptime));
    auto bytes = ReadFileBytes();
    bytes.resize(GetParam());
    WriteFileBytes(bytes);
    ExpectRejected(path.data());
}
INSTANTIATE_TEST_SUITE_P(FileLengths, TruncatedSnapshots,
    testing::Values(size_t{0}, size_t{1}, size_t{19}, size_t{20}, size_t{31}, size_t{32},
                    size_t{511}, size_t{512}, size_t{UKNCIMAGE_SIZE - 1}));

class InvalidSnapshotHeaders : public SnapshotTests, public testing::WithParamInterface<size_t> {};
TEST_P(InvalidSnapshotHeaders, RejectsWithoutChangingState)
{
    ASSERT_TRUE(snapshot::Save(path.data(), *board, uptime));
    const auto original = ReadFileBytes();
    for (const uint32_t bad : {0u, UINT32_MAX, 2u})
    {
        auto bytes = original;
        little_endian::write<uint32_t>(std::span<uint8_t, 4>{bytes.data() + GetParam(), 4}, bad);
        WriteFileBytes(bytes);
        ExpectRejected(path.data());
    }
}
INSTANTIATE_TEST_SUITE_P(HeaderOffsets, InvalidSnapshotHeaders,
    testing::Values(size_t{0}, size_t{4}, size_t{8}, size_t{12}));
}
