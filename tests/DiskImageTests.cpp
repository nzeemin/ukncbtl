#include "stdafx.h"
#include <gtest/gtest.h>
#include "DiskImage.h"
#include <algorithm>
#include <vector>
namespace {
const uint8_t DiskInitBlockAt0000[] =
{
    0xA0, 0x00, 0x05, 0x00, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x10, 0x43, 0x10, 0x9C, 0x00, 0x01,
    0x37, 0x08, 0x24, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x3F, 0x42, 0x4F, 0x4F, 0x54, 0x2D,
    0x55, 0x2D, 0x4E, 0x6F, 0x20, 0x62, 0x6F, 0x6F, 0x74, 0x20, 0x6F, 0x6E, 0x20, 0x76, 0x6F, 0x6C,
    0x75, 0x6D, 0x65, 0x0D, 0x0A, 0x0A, 0x80, 0x00, 0xDF, 0x8B, 0x74, 0xFF, 0xFD, 0x80, 0x1F, 0x94,
    0x76, 0xFF, 0xFA, 0x80, 0xFF, 0x01
};
const uint8_t DiskInitBlockAt03D2[] =
{
    0x01, 0x00, 0x06, 0x00, 0x53, 0x8E, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20
};
const uint8_t DiskInitBlockAt0C00For400K[] =
{
    0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x02, 0xD5, 0x19, 0xC0, 0x62,
    0xEA, 0x27, 0x12, 0x03, 0x00, 0x00, 0x4D, 0x2D, 0x00, 0x08
};
const uint8_t DiskInitBlockAt0C00For800K[] =
{
    0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x02, 0xD5, 0x00, 0x39, 0x67,
    0xF4, 0x26, 0x32, 0x06, 0x00, 0x00, 0xFB, 0x33, 0x00, 0x08
};

}

class DiskImages : public testing::TestWithParam<int> {};
TEST_P(DiskImages, ExactInitializationBytesOffsetsZeroFillAndGuards)
{
    const int tracks = GetParam();
    const size_t size = tracks == 40 ? 409600 : 819200;
    EXPECT_EQ(size, disk_image::Size(tracks));
    std::vector<uint8_t> bytes(size + 2, 0xcd), expected(size, 0);
    std::copy(std::begin(DiskInitBlockAt0000),std::end(DiskInitBlockAt0000),expected.begin());
    std::copy(std::begin(DiskInitBlockAt03D2),std::end(DiskInitBlockAt03D2),expected.begin()+0x3d2);
    const auto* block = tracks == 40 ? DiskInitBlockAt0C00For400K : DiskInitBlockAt0C00For800K;
    std::copy_n(block,26,expected.begin()+0xc00);
    ASSERT_TRUE(disk_image::Build(tracks,std::span(bytes).subspan(1,size)));
    EXPECT_EQ(0xcd,bytes.front()); EXPECT_EQ(0xcd,bytes.back());
    EXPECT_TRUE(std::equal(expected.begin(),expected.end(),bytes.begin()+1));
}
INSTANTIATE_TEST_SUITE_P(BothSizes, DiskImages, testing::Values(40,80));
TEST(DiskImage, InvalidTracksAndShortSpansRemainUntouched)
{
    std::vector<uint8_t> bytes(819200,0xcd);
    for (int tracks : {INT_MIN,-1,0,1,39,41,79,81,INT_MAX})
    {
        EXPECT_EQ(0,disk_image::Size(tracks));
        EXPECT_FALSE(disk_image::Build(tracks,bytes));
    }
    for (int tracks : {40,80})
        EXPECT_FALSE(disk_image::Build(tracks,std::span(bytes).first(disk_image::Size(tracks)-1)));
    EXPECT_FALSE(disk_image::Build(40,{}));
    EXPECT_TRUE(std::ranges::all_of(bytes,[](auto byte) { return byte == 0xcd; }));
}
TEST(DiskImage, OversizedOutputPreservesTail)
{
    std::vector<uint8_t> bytes(819200,0xcd);
    ASSERT_TRUE(disk_image::Build(40,bytes));
    EXPECT_TRUE(std::all_of(bytes.begin()+409600,bytes.end(),[](auto byte) { return byte == 0xcd; }));
}
class DiskImageFile : public testing::Test
{
protected:
    std::wstring path;
    void SetUp() override
    {
        wchar_t directory[MAX_PATH]{}, temporary[MAX_PATH]{};
        ASSERT_NE(0,GetTempPathW(MAX_PATH,directory));
        ASSERT_NE(0,GetTempFileNameW(directory,L"dsk",0,temporary));
        path = temporary;
    }
    void TearDown() override { DeleteFileW(path.c_str()); }
    std::vector<uint8_t> Read()
    {
        FILE* file = _wfopen(path.c_str(),L"rb");
        if (!file) { ADD_FAILURE(); return {}; }
        std::fseek(file,0,SEEK_END); const auto size = std::ftell(file); std::rewind(file);
        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        EXPECT_EQ(bytes.size(),std::fread(bytes.data(),1,bytes.size(),file));
        EXPECT_EQ(0,std::fclose(file)); return bytes;
    }
};
TEST_F(DiskImageFile, ActualFilesMatchEntireImageAndOverwriteTruncates)
{
    for (int tracks : {80,40,80})
    {
        ASSERT_TRUE(disk_image::Create(path.c_str(),tracks));
        std::vector<uint8_t> expected(disk_image::Size(tracks));
        ASSERT_TRUE(disk_image::Build(tracks,expected));
        EXPECT_EQ(expected,Read());
    }
}
TEST_F(DiskImageFile, UnicodePath)
{
    const auto unicode = path + L"-\u65e5\u672c\u8a9e.dsk";
    ASSERT_TRUE(MoveFileW(path.c_str(),unicode.c_str())); path = unicode;
    ASSERT_TRUE(disk_image::Create(path.c_str(),40));
    EXPECT_EQ(409600,Read().size());
}
TEST_F(DiskImageFile, InvalidArgumentsDoNotTruncateExistingFile)
{
    ASSERT_TRUE(disk_image::Create(path.c_str(),40)); const auto before = Read();
    for (int tracks : {0,41,INT_MAX})
    {
        auto result = disk_image::Create(path.c_str(),tracks); ASSERT_FALSE(result);
        EXPECT_EQ(disk_image::ErrorKind::InvalidArgument,result.error().kind);
    }
    EXPECT_FALSE(disk_image::Create(nullptr,40)); EXPECT_FALSE(disk_image::Create(L"",40));
    EXPECT_EQ(before,Read());
}
TEST_F(DiskImageFile, LockedFileReportsOpenFailureAndPreservesContents)
{
    ASSERT_TRUE(disk_image::Create(path.c_str(),40)); const auto before = Read();
    const auto handle = CreateFileW(path.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
    ASSERT_NE(INVALID_HANDLE_VALUE,handle);
    const auto result = disk_image::Create(path.c_str(),80);
    EXPECT_TRUE(CloseHandle(handle));
    ASSERT_FALSE(result); EXPECT_EQ(disk_image::ErrorKind::Open,result.error().kind);
    EXPECT_EQ(ERROR_SHARING_VIOLATION,result.error().code); EXPECT_EQ(before,Read());
}
TEST_F(DiskImageFile, MissingParentReportsOpenFailure)
{
    auto result = disk_image::Create((path + L"\\disk.dsk").c_str(),40);
    ASSERT_FALSE(result); EXPECT_EQ(disk_image::ErrorKind::Open,result.error().kind);
}
namespace {
struct FakeDisk
{
    static FakeDisk* active;
    int opens = 0, writes = 0, closes = 0;
    bool openFail = false, writeFail = false, shortWrite = false, closeFail = false;
    DWORD error = 0;
    std::vector<uint8_t> data;
    static HANDLE WINAPI Open(LPCWSTR, DWORD access, DWORD sharing, LPSECURITY_ATTRIBUTES, DWORD disposition, DWORD, HANDLE)
    {
        ++active->opens; EXPECT_EQ(GENERIC_WRITE,access); EXPECT_EQ(FILE_SHARE_READ,sharing); EXPECT_EQ(CREATE_ALWAYS,disposition);
        active->error = ERROR_ACCESS_DENIED;
        return active->openFail ? INVALID_HANDLE_VALUE : reinterpret_cast<HANDLE>(1);
    }
    static BOOL WINAPI Write(HANDLE handle, LPCVOID bytes, DWORD count, LPDWORD written, LPOVERLAPPED overlap)
    {
        ++active->writes; EXPECT_EQ(reinterpret_cast<HANDLE>(1),handle); EXPECT_EQ(nullptr,overlap);
        const auto* begin = static_cast<const uint8_t*>(bytes); active->data.assign(begin,begin+count);
        *written = active->shortWrite ? count-1 : count; active->error = ERROR_DISK_FULL;
        return !active->writeFail;
    }
    static BOOL WINAPI Close(HANDLE handle)
    { ++active->closes; EXPECT_EQ(reinterpret_cast<HANDLE>(1),handle); active->error = ERROR_INVALID_HANDLE; return !active->closeFail; }
    static DWORD WINAPI LastError() { return active->error; }
    static disk_image::Backend Backend() { return {Open,Write,Close,LastError}; }
};
FakeDisk* FakeDisk::active = nullptr;
}
class DiskImageErrors : public testing::Test
{
protected:
    FakeDisk fake;
    void SetUp() override { FakeDisk::active = &fake; }
    void TearDown() override { FakeDisk::active = nullptr; }
};
TEST_F(DiskImageErrors, OpenFailureDoesNotWriteOrClose)
{
    fake.openFail = true; auto result = disk_image::Create(L"ignored",40,FakeDisk::Backend());
    ASSERT_FALSE(result); EXPECT_EQ(disk_image::ErrorKind::Open,result.error().kind);
    EXPECT_EQ(ERROR_ACCESS_DENIED,result.error().code); EXPECT_EQ(0,fake.writes); EXPECT_EQ(0,fake.closes);
}
TEST_F(DiskImageErrors, WriteFailureClosesAndPreservesOriginalError)
{
    fake.writeFail = true; fake.closeFail = true;
    auto result = disk_image::Create(L"ignored",40,FakeDisk::Backend());
    ASSERT_FALSE(result); EXPECT_EQ(disk_image::ErrorKind::Write,result.error().kind);
    EXPECT_EQ(ERROR_DISK_FULL,result.error().code); EXPECT_EQ(1,fake.closes);
}
TEST_F(DiskImageErrors, ShortWriteIsFailureEvenWhenApiSucceeds)
{
    fake.shortWrite = true; auto result = disk_image::Create(L"ignored",80,FakeDisk::Backend());
    ASSERT_FALSE(result); EXPECT_EQ(disk_image::ErrorKind::Write,result.error().kind);
    EXPECT_EQ(ERROR_WRITE_FAULT,result.error().code); EXPECT_EQ(1,fake.closes);
}
TEST_F(DiskImageErrors, CloseFailureIsReported)
{
    fake.closeFail = true; auto result = disk_image::Create(L"ignored",40,FakeDisk::Backend());
    ASSERT_FALSE(result); EXPECT_EQ(disk_image::ErrorKind::Close,result.error().kind);
    EXPECT_EQ(ERROR_INVALID_HANDLE,result.error().code); EXPECT_EQ(1,fake.writes); EXPECT_EQ(1,fake.closes);
}
TEST_F(DiskImageErrors, SuccessfulWriteContainsCompleteZeroFilledImage)
{
    ASSERT_TRUE(disk_image::Create(L"ignored",80,FakeDisk::Backend()));
    std::vector<uint8_t> expected(819200); ASSERT_TRUE(disk_image::Build(80,expected));
    EXPECT_EQ(expected,fake.data); EXPECT_EQ(1,fake.opens); EXPECT_EQ(1,fake.writes); EXPECT_EQ(1,fake.closes);
}
TEST_F(DiskImageErrors, IncompleteBackendRejectedBeforeOpening)
{
    for (int field = 0; field < 4; ++field)
    {
        auto backend = FakeDisk::Backend();
        switch (field) {
        case 0: backend.open = nullptr; break; case 1: backend.write = nullptr; break;
        case 2: backend.close = nullptr; break; case 3: backend.lastError = nullptr; break;
        }
        auto result = disk_image::Create(L"ignored",40,backend);
        ASSERT_FALSE(result); EXPECT_EQ(disk_image::ErrorKind::InvalidArgument,result.error().kind);
    }
    EXPECT_EQ(0,fake.opens);
}
