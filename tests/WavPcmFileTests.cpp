#include "stdafx.h"
#include <gtest/gtest.h>
#include "util/WavPcmFile.h"
#include <array>
#include <algorithm>
#include <tuple>
#include <cstdio>
#include <vector>

#ifdef _DEBUG
int AssertFailedLine(const char* file, int line)
{
    std::fprintf(stderr, "Assertion failed at %s:%d\n", file, line);
    std::abort();
}
#endif

namespace {
void Put32(std::vector<uint8_t>& bytes, size_t pos, uint32_t value)
{
    for (int i = 0; i < 4; ++i) bytes[pos + i] = static_cast<uint8_t>(value >> (8 * i));
}
std::vector<uint8_t> Wave()
{
    const uint8_t raw[] = {
        'R','I','F','F', 38,0,0,0, 'W','A','V','E',
        'f','m','t',' ', 16,0,0,0, 1,0, 1,0,
        0,0,0,0, 0,0,0,0, 1,0, 8,0,
        'd','a','t','a', 2,0,0,0, 0x80,0x81
    };
    std::vector<uint8_t> result(raw, raw + sizeof(raw));
    Put32(result, 24, 96000);
    Put32(result, 28, 96000);
    return result;
}
bool Write(const TCHAR* path, const std::vector<uint8_t>& bytes)
{
    FILE* file = _tfopen(path, _T("wb"));
    if (!file) return false;
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    return std::fclose(file) == 0 && ok;
}
}

class WavFile : public testing::Test
{
protected:
    TCHAR path[MAX_PATH]{};
    void SetUp() override
    {
        TCHAR directory[MAX_PATH]{};
        ASSERT_NE(0, GetTempPath(MAX_PATH, directory));
        ASSERT_NE(0, GetTempFileName(directory, L"wav", 0, path));
    }
    void TearDown() override { DeleteFile(path); }
    void Store(const std::vector<uint8_t>& bytes) { ASSERT_TRUE(Write(path, bytes)); }
};
TEST_F(WavFile, HighRateEightBitDataStopsAtChunkAndRewinds)
{
    Store(Wave());
    auto wav = WavPcmFile::Open(path); ASSERT_TRUE(wav);
    EXPECT_EQ(96000, wav->Frequency()); EXPECT_EQ(2, wav->Length()); EXPECT_EQ(0, wav->Position());
    EXPECT_EQ(0x80000000u, wav->ReadOne()); EXPECT_EQ(0x81000000u, wav->ReadOne());
    EXPECT_EQ(std::unexpected(WavError::end_of_data), wav->ReadOne());
    ASSERT_TRUE(wav->SetPosition(0)); EXPECT_EQ(0x80000000u, wav->ReadOne());
    EXPECT_EQ(std::unexpected(WavError::invalid_position), wav->SetPosition(UINT_MAX));
    EXPECT_EQ(1, wav->Position());
    ASSERT_TRUE(wav->SetPosition(2)); EXPECT_EQ(std::unexpected(WavError::end_of_data), wav->ReadOne());
}
TEST_F(WavFile, UnknownOddChunksAndTrailingChunksAreSkipped)
{
    auto bytes = Wave();
    const std::vector<uint8_t> junk{'J','U','N','K',1,0,0,0,42,0};
    bytes.insert(bytes.begin() + 36, junk.begin(), junk.end());
    bytes.insert(bytes.end(), junk.begin(), junk.end());
    Put32(bytes, 4, static_cast<uint32_t>(bytes.size() - 8)); Store(bytes);
    auto wav = WavPcmFile::Open(path); ASSERT_TRUE(wav);
    EXPECT_EQ(2, wav->Length()); EXPECT_EQ(0x80000000u, wav->ReadOne()); EXPECT_EQ(0x81000000u, wav->ReadOne());
    EXPECT_EQ(std::unexpected(WavError::end_of_data), wav->ReadOne());
}
TEST_F(WavFile, ExtendedFormatAndBytesOutsideRiffAreAccepted)
{
    auto bytes = Wave(); bytes.insert(bytes.begin() + 36, {0,0});
    Put32(bytes,16,18); Put32(bytes,4,static_cast<uint32_t>(bytes.size()-8));
    bytes.insert(bytes.end(), {1,2,3}); Store(bytes);
    auto wav = WavPcmFile::Open(path); ASSERT_TRUE(wav); EXPECT_EQ(2, wav->Length());
}
class PcmFormats : public WavFile, public testing::WithParamInterface<int> {};
TEST_P(PcmFormats, SignedBoundariesLittleEndianAndFirstChannelOnly)
{
    const int bits = GetParam(), size = bits / 8;
    for (int channels : {1,2,4})
    {
        auto bytes = Wave(); bytes.resize(44);
        bytes[22] = static_cast<uint8_t>(channels); bytes[32] = static_cast<uint8_t>(channels * size);
        bytes[34] = static_cast<uint8_t>(bits); Put32(bytes,28,96000 * channels * size);
        std::vector<uint32_t> samples = bits == 8 ? std::vector<uint32_t>{0,127,128,255} :
            bits == 16 ? std::vector<uint32_t>{0x8000,0xffff,0,0x7fff,0x1234} :
                         std::vector<uint32_t>{0x80000000,0xffffffff,0,0x7fffffff,0x56781234};
        for (auto sample : samples)
        {
            for (int i = 0; i < size; ++i) bytes.push_back(static_cast<uint8_t>(sample >> (8*i)));
            for (int i = size; i < size * channels; ++i) bytes.push_back(0x55);
        }
        Put32(bytes,40,static_cast<uint32_t>(bytes.size()-44));
        if (bytes.size() % 2) bytes.push_back(0);
        Put32(bytes,4,static_cast<uint32_t>(bytes.size()-8)); Store(bytes);
        auto wav = WavPcmFile::Open(path); ASSERT_TRUE(wav);
        EXPECT_EQ(samples.size(), wav->Length());
        for (auto sample : samples)
            EXPECT_EQ(bits == 8 ? sample << 24 : bits == 16 ? (sample << 16) ^ 0x80000000u : sample ^ 0x80000000u, wav->ReadOne());
        ASSERT_TRUE(wav->SetPosition(1));
        EXPECT_EQ(bits == 8 ? samples[1] << 24 : bits == 16 ? (samples[1] << 16) ^ 0x80000000u : samples[1] ^ 0x80000000u, wav->ReadOne());
    }
}
INSTANTIATE_TEST_SUITE_P(All, PcmFormats, testing::Values(8,16,32));
class BadWaveField : public WavFile, public testing::WithParamInterface<std::tuple<size_t,uint32_t>> {};
TEST_P(BadWaveField, RejectsMalformedHeader)
{
    auto bytes = Wave(); const auto [offset,value] = GetParam(); Put32(bytes,offset,value); Store(bytes);
    auto wav = WavPcmFile::Open(path); ASSERT_FALSE(wav); EXPECT_EQ(WavError::invalid_format,wav.error());
}
const std::vector<std::tuple<size_t,uint32_t>> badWaveFields{
    {0,0}, {8,0}, {4,3}, {4,UINT_MAX}, {4,39},
    {16,15}, {16,UINT_MAX}, {24,0}, {24,0x80000000}, {28,1},
    {32,0}, {32,0x00080011}, {32,0x00180003}, {32,0x00080002}, {40,UINT_MAX}};
INSTANTIATE_TEST_SUITE_P(All, BadWaveField, testing::ValuesIn(badWaveFields));
TEST_F(WavFile, EveryTruncatedPrefixIsRejected)
{
    const auto complete = Wave();
    for (size_t size = 0; size < complete.size(); ++size)
    {
        Store(std::vector<uint8_t>(complete.begin(),complete.begin()+size));
        EXPECT_FALSE(WavPcmFile::Open(path)) << size;
    }
}
TEST_F(WavFile, UnsupportedFormatHasDistinctError)
{
    auto bytes = Wave(); bytes[20] = 3; Store(bytes);
    auto wav = WavPcmFile::Open(path); ASSERT_FALSE(wav); EXPECT_EQ(WavError::unsupported_format,wav.error());
}
TEST_F(WavFile, ZeroChannelsAndInconsistentFrameSizeRejected)
{
    auto bytes = Wave(); bytes[22] = 0; Store(bytes); EXPECT_FALSE(WavPcmFile::Open(path));
    bytes = Wave(); bytes[22] = 3; Store(bytes); EXPECT_FALSE(WavPcmFile::Open(path));
    bytes = Wave(); bytes[22] = 2; bytes[32] = 2; Put32(bytes,28,192000);
    Put32(bytes,40,1); Store(bytes); EXPECT_FALSE(WavPcmFile::Open(path));
}
TEST_F(WavFile, MissingDuplicateAndOutOfOrderChunksRejected)
{
    for (int variant = 0; variant < 5; ++variant)
    {
        auto bytes = Wave();
        if (variant == 0) bytes[12] = 'x';
        if (variant == 1) bytes[36] = 'x';
        if (variant == 2) { auto copy = bytes; bytes.insert(bytes.end(),copy.begin()+12,copy.begin()+36); }
        if (variant == 3) { auto copy = bytes; bytes.insert(bytes.end(),copy.begin()+36,copy.end()); }
        if (variant == 4) std::rotate(bytes.begin()+12,bytes.begin()+36,bytes.end());
        Put32(bytes,4,static_cast<uint32_t>(bytes.size()-8)); Store(bytes);
        EXPECT_FALSE(WavPcmFile::Open(path)) << variant;
    }
}
TEST_F(WavFile, IncompleteChunkHeaderAndChunkBeyondRiffRejected)
{
    auto bytes = Wave(); bytes.push_back(0); Put32(bytes,4,39); Store(bytes);
    EXPECT_FALSE(WavPcmFile::Open(path));
    bytes = Wave(); Put32(bytes,40,3); Store(bytes); EXPECT_FALSE(WavPcmFile::Open(path));
}
TEST_F(WavFile, EmptyAndLegacyUnpaddedOddDataAccepted)
{
    for (int size : {0,1})
    {
        auto bytes = Wave(); bytes.resize(44+size); Put32(bytes,40,size); Put32(bytes,4,36+size); Store(bytes);
        auto wav = WavPcmFile::Open(path); ASSERT_TRUE(wav); EXPECT_EQ(size,wav->Length());
    }
}
TEST_F(WavFile, InvalidArgumentsAndOpenFailure)
{
    EXPECT_EQ(WavError::invalid_argument,WavPcmFile::Open(nullptr).error());
    EXPECT_EQ(WavError::invalid_argument,WavPcmFile::Create(nullptr,44100).error());
    EXPECT_EQ(WavError::invalid_argument,WavPcmFile::Create(path,0).error());
    EXPECT_EQ(WavError::invalid_argument,WavPcmFile::Create(path,-1).error());
    ASSERT_TRUE(DeleteFile(path)); EXPECT_EQ(WavError::open_failed,WavPcmFile::Open(path).error());
}
TEST_F(WavFile, RecordingOverwriteAppendAndCloseRoundTrip)
{
    auto wav = WavPcmFile::Create(path,44100); ASSERT_TRUE(wav);
    ASSERT_TRUE(wav->WriteOne(0x80123456)); ASSERT_TRUE(wav->WriteOne(0x81000000));
    ASSERT_TRUE(wav->SetPosition(0)); ASSERT_TRUE(wav->WriteOne(0x82000000)); EXPECT_EQ(2,wav->Length());
    ASSERT_TRUE(wav->SetPosition(2)); ASSERT_TRUE(wav->WriteOne(0xff000000));
    EXPECT_EQ(3,wav->Length()); EXPECT_EQ(3,wav->Position()); ASSERT_TRUE(wav->Close());
    auto read = WavPcmFile::Open(path); ASSERT_TRUE(read);
    EXPECT_EQ(44100,read->Frequency()); EXPECT_EQ(3,read->Length());
    EXPECT_EQ(0x82000000u,read->ReadOne()); EXPECT_EQ(0x81000000u,read->ReadOne()); EXPECT_EQ(0xff000000u,read->ReadOne());
}
TEST_F(WavFile, DestructorFinalizesAndMoveTransfersOwnership)
{
    {
        auto original = WavPcmFile::Create(path,22050); ASSERT_TRUE(original);
        ASSERT_TRUE(original->WriteOne(0x80000000));
        auto moved = std::move(*original);
        ASSERT_TRUE(original->Close());
        EXPECT_EQ(std::unexpected(WavError::io_error),original->WriteOne(0));
        ASSERT_TRUE(moved.WriteOne(0xff000000));
    }
    auto read = WavPcmFile::Open(path); ASSERT_TRUE(read); EXPECT_EQ(2,read->Length());
}
TEST_F(WavFile, ModesClosedOperationsAndRepeatedClose)
{
    auto writer = WavPcmFile::Create(path,44100); ASSERT_TRUE(writer);
    EXPECT_EQ(std::unexpected(WavError::io_error),writer->ReadOne());
    EXPECT_EQ(std::unexpected(WavError::invalid_position),writer->SetPosition(1));
    ASSERT_TRUE(writer->Close()); ASSERT_TRUE(writer->Close());
    EXPECT_EQ(std::unexpected(WavError::io_error),writer->WriteOne(0));
    EXPECT_EQ(std::unexpected(WavError::io_error),writer->SetPosition(0));
    auto reader = WavPcmFile::Open(path); ASSERT_TRUE(reader);
    EXPECT_EQ(std::unexpected(WavError::io_error),reader->WriteOne(0));
    EXPECT_EQ(std::unexpected(WavError::end_of_data),reader->ReadOne());
    ASSERT_TRUE(reader->Close()); EXPECT_EQ(std::unexpected(WavError::io_error),reader->ReadOne());
}
TEST_F(WavFile, FinalHeaderAndOddPaddingMatchPhysicalSize)
{
    for (int count : {0,1,2,3})
    {
        auto wav = WavPcmFile::Create(path,44100); ASSERT_TRUE(wav);
        for (int i = 0; i < count; ++i) ASSERT_TRUE(wav->WriteOne(0xab123456));
        ASSERT_TRUE(wav->Close());
        FILE* file = _tfopen(path,L"rb"); ASSERT_NE(nullptr,file);
        std::vector<uint8_t> bytes(48); const auto size = std::fread(bytes.data(),1,bytes.size(),file); std::fclose(file);
        EXPECT_EQ(44+count+(count&1),size); EXPECT_EQ(36+count+(count&1),bytes[4]); EXPECT_EQ(count,bytes[40]);
        for (int i = 0; i < count; ++i) EXPECT_EQ(0xab,bytes[44+i]);
        if (count&1) EXPECT_EQ(0,bytes[44+count]);
    }
}
