#include "stdafx.h"
#include <gtest/gtest.h>
#include "TapeTransport.h"
#include <array>
#include <vector>

namespace {
struct Stream
{
    std::vector<uint32_t> data;
    unsigned position = 0, calls = 0, failAt = UINT_MAX;
    unsigned Position() const { return position; }
    unsigned Length() const { return static_cast<unsigned>(data.size()); }
    std::expected<uint32_t, WavError> ReadOne()
    {
        ++calls;
        if (position == failAt) return std::unexpected(WavError::io_error);
        if (position == data.size()) return std::unexpected(WavError::end_of_data);
        return data[position++];
    }
    std::expected<void, WavError> WriteOne(uint32_t value)
    {
        ++calls;
        if (position == failAt) return std::unexpected(WavError::io_error);
        data.push_back(value); ++position; return {};
    }
};
}
TEST(TapeState, EmptyCannotPlayAndStopIsIdempotent)
{
    tape::State state;
    EXPECT_FALSE(state.Inserted()); EXPECT_FALSE(state.Recording()); EXPECT_FALSE(state.Playing());
    EXPECT_FALSE(state.Play()); EXPECT_FALSE(state.Stop());
}
TEST(TapeState, PlaybackAndRecordingTransitions)
{
    for (bool recording : {false, true})
    {
        tape::State state;
        state.Insert(recording);
        EXPECT_TRUE(state.Inserted()); EXPECT_EQ(recording, state.Recording());
        EXPECT_TRUE(state.Play()); EXPECT_FALSE(state.Play()); EXPECT_TRUE(state.Playing());
        EXPECT_TRUE(state.Stop()); EXPECT_FALSE(state.Stop()); EXPECT_FALSE(state.Playing());
        EXPECT_TRUE(state.Play());
        state.Eject();
        EXPECT_FALSE(state.Inserted()); EXPECT_FALSE(state.Playing()); EXPECT_FALSE(state.Recording());
        EXPECT_FALSE(state.Play());
    }
}
TEST(TapeState, ReinsertResetsModeAndPlaying)
{
    tape::State state;
    state.Insert(true); state.Play(); state.Insert(false);
    EXPECT_FALSE(state.Recording()); EXPECT_FALSE(state.Playing()); EXPECT_TRUE(state.Play());
}
TEST(TapeBatch, ZeroCountLeavesStreamAndGraphUntouched)
{
    Stream stream;
    std::array<uint8_t, 4> graph{1,2,3,4};
    EXPECT_FALSE(tape::Read(stream, 0, graph).stop);
    EXPECT_FALSE(tape::Write(stream, 0, 0, graph).stop);
    EXPECT_EQ(0, stream.calls);
    EXPECT_EQ((std::array<uint8_t,4>{1,2,3,4}), graph);
}
TEST(TapeBatch, ThresholdAndLastSampleDetermineInputBit)
{
    for (uint32_t sample : {0u, 0x7fffffffu, 0x80000000u, 0xffffffffu})
    {
        Stream stream{{0xffffffffu, sample}};
        const auto result = tape::Read(stream, 2, {});
        EXPECT_EQ(sample >= 0x80000000u, result.bit);
        EXPECT_EQ(2, result.processed); EXPECT_TRUE(result.stop); EXPECT_FALSE(result.error);
    }
}
TEST(TapeBatch, PartialBatchRetainsHistoryAndUpdatesOnlySuccessfulSamples)
{
    Stream stream{{0x10000000,0x20000000}};
    std::array<uint8_t,4> graph{1,2,3,4};
    const auto result = tape::Read(stream, 100, graph);
    EXPECT_EQ(2, result.processed); EXPECT_TRUE(result.stop); EXPECT_FALSE(result.error);
    EXPECT_EQ((std::array<uint8_t,4>{3,4,16,32}), graph);
}
TEST(TapeBatch, OversizedAndConsecutiveBatchesKeepLatestSamples)
{
    Stream stream;
    for (unsigned i = 1; i <= 20; ++i) stream.data.push_back(i << 24);
    std::array<uint8_t,4> graph{};
    EXPECT_FALSE(tape::Read(stream, 7, graph).stop);
    EXPECT_EQ((std::array<uint8_t,4>{4,5,6,7}), graph);
    tape::Read(stream, 2, graph);
    EXPECT_EQ((std::array<uint8_t,4>{6,7,8,9}), graph);
    tape::Read(stream, 4, graph);
    EXPECT_EQ((std::array<uint8_t,4>{10,11,12,13}), graph);
}
TEST(TapeBatch, EmptyTapeDoesNotAlterGraph)
{
    Stream stream;
    std::array<uint8_t,2> graph{12,34};
    const auto result = tape::Read(stream, 100, graph);
    EXPECT_TRUE(result.stop); EXPECT_FALSE(result.error); EXPECT_FALSE(result.bit);
    EXPECT_EQ((std::array<uint8_t,2>{12,34}), graph);
}
TEST(TapeBatch, ReadErrorPreservesLastBitAndStopsImmediately)
{
    Stream stream{{0x80000000, 0}};
    stream.failAt = 1;
    std::array<uint8_t,2> graph{1,2};
    const auto result = tape::Read(stream, 10, graph);
    EXPECT_TRUE(result.stop); EXPECT_TRUE(result.bit); EXPECT_EQ(WavError::io_error, result.error);
    EXPECT_EQ(2, stream.calls); EXPECT_EQ(1, result.processed);
    EXPECT_EQ((std::array<uint8_t,2>{2,128}), graph);
}
TEST(TapeBatch, WriteFailureShowsOnlyWrittenSamples)
{
    Stream stream; stream.failAt = 2;
    std::array<uint8_t,4> graph{1,2,3,4};
    const auto result = tape::Write(stream, 0xff123456, 20, graph);
    EXPECT_TRUE(result.stop); EXPECT_EQ(WavError::io_error, result.error);
    EXPECT_EQ(2, result.processed); EXPECT_EQ(3, stream.calls);
    EXPECT_EQ((std::vector<uint32_t>{0xff123456,0xff123456}), stream.data);
    EXPECT_EQ((std::array<uint8_t,4>{3,4,255,255}), graph);
}
TEST(TapeBatch, WriteLargeBatchAndEmptyGraph)
{
    Stream stream;
    std::array<uint8_t,4> graph{};
    const auto result = tape::Write(stream, 0x80123456, 11, graph);
    EXPECT_EQ(11, result.processed); EXPECT_FALSE(result.stop); EXPECT_FALSE(result.error);
    EXPECT_EQ((std::array<uint8_t,4>{128,128,128,128}), graph);
    EXPECT_EQ(5, tape::Write(stream, 0, 5, {}).processed);
}
TEST(TapePosition, TimeConversionUsesWideArithmetic)
{
    EXPECT_EQ(0, tape::Centiseconds(123, 0));
    EXPECT_EQ(0, tape::Centiseconds(123, -1));
    EXPECT_EQ(123, tape::Centiseconds(54464, 44100));
    EXPECT_EQ(429496729500ULL, tape::Centiseconds(UINT_MAX, 1));
}
TEST(TapePosition, UpdateThresholdStopAndRewind)
{
    EXPECT_FALSE(tape::UpdatePosition(100, 0, 600, true));
    EXPECT_TRUE(tape::UpdatePosition(101, 0, 600, true));
    EXPECT_TRUE(tape::UpdatePosition(0, 0, 600, false));
    EXPECT_TRUE(tape::UpdatePosition(0, 100, 600, true));
}
