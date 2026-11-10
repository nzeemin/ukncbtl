/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include <gtest/gtest.h>
#include "emubase/Emubase.h"
#include "util/File.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

#ifdef _DEBUG
int AssertFailedLine(const char* file, int line)
{
    std::fprintf(stderr, "Assertion failed at %s:%d\n", file, line);
    std::abort();
}
#endif
void DebugLog(LPCTSTR) {}
const TCHAR* REGISTER_NAME[] = {_T("R0"), _T("R1"), _T("R2"), _T("R3"),
                               _T("R4"), _T("R5"), _T("SP"), _T("PC")};

namespace {
class CoreSmokeTests : public testing::Test
{
protected:
    void SetUp() override { CProcessor::Init(); }
    void TearDown() override { CProcessor::Done(); }
};

TEST_F(CoreSmokeTests, DisassemblesThreeWordInstruction)
{
    const std::array<uint16_t, 3> instruction{0012761, 1, 2};
    const auto decoded = DisassembleInstruction(instruction, 01000);
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded->length, 3);
    EXPECT_EQ(decoded->mnemonic, _T("MOV"));
    EXPECT_EQ(decoded->argument, _T("#000001, 000002(R1)"));
}

TEST_F(CoreSmokeTests, DisassemblerRejectsEmptyAndPadsTruncatedSpans)
{
    EXPECT_FALSE(DisassembleInstruction({}, 01000));
    const std::array<uint16_t, 1> instruction{0012761};
    const auto decoded = DisassembleInstruction(instruction, 01000);
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded->length, 3);
    EXPECT_EQ(decoded->argument, _T("#000000, 000000(R1)"));
}

TEST_F(CoreSmokeTests, JumpRequiresOperandAndRecognizesSelfTarget)
{
    const std::array<uint16_t, 1> incomplete{0000137};
    EXPECT_FALSE(Disasm_CheckForJump(incomplete, 01000));
    const std::array<uint16_t, 2> complete{0000137, 01000};
    EXPECT_EQ(Disasm_CheckForJump(complete, 01000), 0);
}

TEST_F(CoreSmokeTests, LastRamWordAlignsAndStoresLittleEndianBytes)
{
    CMotherboard board;
    board.Reset();
    board.SetRAMWord(0, 0xffff, 0x1234);
    EXPECT_EQ(board.GetRAMWord(0, 0xfffe), 0x1234);
    EXPECT_EQ(board.GetRAMByte(0, 0xfffe), 0x34);
    EXPECT_EQ(board.GetRAMByte(0, 0xffff), 0x12);
}

TEST_F(CoreSmokeTests, UnalignedSnapshotPreservesHeaderGuardsAndRoundTrips)
{
    CMotherboard board;
    board.Reset();
    board.SetRAMWord(0, 0xffff, 0x1234);
    board.GetCPU()->SetReg(0, 0x1234);
    board.GetPPU()->SetReg(1, 0xabcd);
    board.GetCPUMemoryController()->SetWord(0176640, false, 0123);
    std::vector<uint8_t> snapshot(UKNCIMAGE_SIZE + 2, 0xcc);
    board.SaveToImage(std::span<uint8_t, UKNCIMAGE_SIZE>{snapshot.data() + 1, UKNCIMAGE_SIZE});
    EXPECT_EQ(snapshot.front(), 0xcc);
    EXPECT_EQ(snapshot.back(), 0xcc);
    EXPECT_EQ(snapshot[1 + 160 + 2], 0x34);
    EXPECT_EQ(snapshot[1 + 160 + 3], 0x12);
    EXPECT_EQ(snapshot[1 + 224 + 4], 0xcd);
    EXPECT_EQ(snapshot[1 + 224 + 5], 0xab);
    EXPECT_EQ(snapshot[1 + 288], 0123);
    EXPECT_EQ(snapshot[1 + 289], 0);
    for (size_t i = 1; i <= 32; ++i) EXPECT_EQ(snapshot[i], 0xcc) << i;
    CMotherboard restored;
    restored.Reset();
    restored.LoadFromImage(std::span<const uint8_t, UKNCIMAGE_SIZE>{snapshot.data() + 1, UKNCIMAGE_SIZE});
    EXPECT_EQ(restored.GetCPU()->GetReg(0), 0x1234);
    EXPECT_EQ(restored.GetPPU()->GetReg(1), 0xabcd);
    EXPECT_EQ(restored.GetCPUMemoryController()->GetWord(0176640, false, false), 0123);
    EXPECT_EQ(restored.GetRAMWord(0, 0xfffe), 0x1234);
    std::vector<uint8_t> savedAgain(UKNCIMAGE_SIZE + 2, 0xcc);
    restored.SaveToImage(std::span<uint8_t, UKNCIMAGE_SIZE>{savedAgain.data() + 1, UKNCIMAGE_SIZE});
    EXPECT_EQ(savedAgain, snapshot);
}

TEST_F(CoreSmokeTests, FailedHardDriveAttachLeavesSlotEmpty)
{
    CMotherboard board;
    board.Reset();
    EXPECT_FALSE(board.AttachHardImage(1, _T("?:\\invalid-ukncbtl-image")));
    EXPECT_FALSE(board.IsHardImageAttached(1));
}

TEST_F(CoreSmokeTests, BusDeviceListsAreCompleteAndNonNull)
{
    CMotherboard board;
    board.Reset();
    const auto cpu = board.GetCPUBusDevices();
    const auto ppu = board.GetPPUBusDevices();
    EXPECT_EQ(cpu.size(), 5u);
    EXPECT_EQ(ppu.size(), 7u);
    for (const auto* device : cpu) EXPECT_NE(device, nullptr);
    for (const auto* device : ppu) EXPECT_NE(device, nullptr);
}

TEST_F(CoreSmokeTests, DebugTicksCanRunAfterReset)
{
    CMotherboard board;
    board.Reset();
    EXPECT_NO_FATAL_FAILURE(board.DebugTicks());
}

class CoreDiskSmokeTests : public CoreSmokeTests
{
protected:
    std::array<TCHAR, MAX_PATH> path{};
    void SetUp() override
    {
        CoreSmokeTests::SetUp();
        std::array<TCHAR, MAX_PATH> directory{};
        ASSERT_NE(GetTempPath(MAX_PATH, directory.data()), 0u);
        ASSERT_NE(GetTempFileName(directory.data(), _T("hdd"), 0, path.data()), 0u);
        std::array<uint8_t, 4 * 2 * 8 * 512> image{};
        image[0] = 8;
        image[1] = 2;
        UniqueFile file(_tfopen(path.data(), _T("wb")));
        ASSERT_TRUE(file);
        ASSERT_EQ(std::fwrite(image.data(), 1, image.size(), file.get()), image.size());
        ASSERT_EQ(std::fclose(file.release()), 0);
    }
    void TearDown() override
    {
        if (path[0])
        {
            SetFileAttributes(path.data(), FILE_ATTRIBUTE_NORMAL);
            DeleteFile(path.data());
        }
        CoreSmokeTests::TearDown();
    }
};

TEST_F(CoreDiskSmokeTests, HardDriveReadsSectorAndRejectsSectorZero)
{
    CHardDrive drive;
    ASSERT_TRUE(drive.AttachImage(path.data()));
    drive.Reset();
    drive.WritePort(0x1f2, 1);
    drive.WritePort(0x1f3, 1);
    drive.WritePort(0x1f7, 0x20);
    for (int i = 0; i < 5000 && (drive.ReadPort(0x1f7) & 0x80); ++i) drive.Periodic();
    ASSERT_EQ(drive.ReadPort(0x1f7) & 0x09, 0x08);
    EXPECT_EQ(drive.ReadPort(0x1f0), static_cast<uint16_t>(~0x0208u));
    for (int i = 1; i < 256; ++i) drive.ReadPort(0x1f0);
    drive.WritePort(0x1f2, 1);
    drive.WritePort(0x1f3, 0);
    drive.WritePort(0x1f7, 0x20);
    for (int i = 0; i < 5000 && (drive.ReadPort(0x1f7) & 0x80); ++i) drive.Periodic();
    const auto status = drive.ReadPort(0x1f7);
    EXPECT_NE(status & 0x01, 0);
    EXPECT_EQ(status & 0x08, 0);
}

TEST_F(CoreDiskSmokeTests, ReadOnlyFloppyCanDetachAfterWriteAttempt)
{
    ASSERT_TRUE(SetFileAttributes(path.data(), FILE_ATTRIBUTE_READONLY));
    CFloppyController floppy;
    ASSERT_TRUE(floppy.AttachImage(0, path.data()));
    ASSERT_TRUE(floppy.IsReadOnly(0));
    floppy.SetCommand(FLOPPY_CMD_ENGINESTART | 02000 | 3);
    for (int i = 0; i < 47; ++i) floppy.Periodic();
    floppy.WriteData(0x1234);
    floppy.Periodic();
    floppy.DetachImage(0);
    EXPECT_FALSE(floppy.IsAttached(0));
    EXPECT_NE(floppy.GetState() & FLOPPY_STATUS_WRITEPROTECT, 0);
}
}
