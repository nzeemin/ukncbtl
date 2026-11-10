/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include "emubase/ImageSerialization.h"
#include <gtest/gtest.h>
#include <array>
#include <limits>

namespace {
template<class T> class LittleEndianTests : public testing::Test {};
using IntegerWidths = testing::Types<uint8_t, uint16_t, uint32_t, uint64_t>;
TYPED_TEST_SUITE(LittleEndianTests, IntegerWidths);

TYPED_TEST(LittleEndianTests, ReadsKnownBytesAtUnalignedAddress)
{
    alignas(8) std::array<uint8_t, sizeof(TypeParam) + 2> bytes{};
    bytes.fill(0xcc);
    uint64_t expected = 0;
    for (size_t i = 0; i < sizeof(TypeParam); ++i)
    {
        bytes[i + 1] = static_cast<uint8_t>(0x81 + i);
        expected |= uint64_t{bytes[i + 1]} << (8 * i);
    }
    const auto before = bytes;
    EXPECT_EQ(little_endian::read<TypeParam>(std::span{bytes}.template subspan<1, sizeof(TypeParam)>()),
              static_cast<TypeParam>(expected));
    EXPECT_EQ(bytes, before);
}

TYPED_TEST(LittleEndianTests, WritesKnownBytesWithoutTouchingGuards)
{
    alignas(8) std::array<uint8_t, sizeof(TypeParam) + 2> bytes{};
    bytes.fill(0xcc);
    constexpr auto value = static_cast<TypeParam>(0x8877665544332211ull);
    little_endian::write<TypeParam>(std::span{bytes}.template subspan<1, sizeof(TypeParam)>(), value);
    for (size_t i = 0; i < sizeof(TypeParam); ++i)
        EXPECT_EQ(bytes[i + 1], static_cast<uint8_t>(uint64_t{value} >> (8 * i)));
    EXPECT_EQ(bytes.front(), 0xcc);
    EXPECT_EQ(bytes.back(), 0xcc);
}

TYPED_TEST(LittleEndianTests, RoundTripsZeroMaximumAndEachBit)
{
    std::array<uint8_t, sizeof(TypeParam)> bytes{};
    for (const TypeParam value : {TypeParam{0}, (std::numeric_limits<TypeParam>::max)()})
    {
        little_endian::write<TypeParam>(bytes, value);
        EXPECT_EQ(little_endian::read<TypeParam>(bytes), value);
    }
    for (size_t bit = 0; bit < sizeof(TypeParam) * 8; ++bit)
    {
        const auto value = static_cast<TypeParam>(uint64_t{1} << bit);
        little_endian::write<TypeParam>(bytes, value);
        EXPECT_EQ(little_endian::read<TypeParam>(bytes), value);
    }
}

enum class Operation { Byte, Word, Dword, Copy, Skip };
size_t Width(Operation operation)
{
    switch (operation)
    {
    case Operation::Byte: return 1;
    case Operation::Word: return 2;
    case Operation::Dword: return 4;
    case Operation::Copy: return 3;
    default: return 3;
    }
}
void Write(image::Writer& writer, Operation operation)
{
    const uint8_t data[]{1, 2, 3};
    switch (operation)
    {
    case Operation::Byte: writer.byte(1); break;
    case Operation::Word: writer.word(0x0201); break;
    case Operation::Dword: writer.dword(0x04030201); break;
    case Operation::Copy: writer.copy(data); break;
    case Operation::Skip: writer.skip(3); break;
    }
}
void Read(image::Reader& reader, Operation operation, uint8_t (&output)[3])
{
    switch (operation)
    {
    case Operation::Byte: EXPECT_EQ(reader.byte(), 1); break;
    case Operation::Word: EXPECT_EQ(reader.word(), 0x0201); break;
    case Operation::Dword: EXPECT_EQ(reader.dword(), 0x04030201u); break;
    case Operation::Copy: reader.copy(output); break;
    case Operation::Skip: reader.skip(3); break;
    }
}

class SerializationOperations : public testing::TestWithParam<Operation> {};

TEST_P(SerializationOperations, WriterAcceptsExactCapacityAndPreservesGuards)
{
    std::array<uint8_t, 6> bytes{};
    bytes.fill(0xcc);
    const auto width = Width(GetParam());
    image::Writer writer(std::span{bytes}.subspan(1, width));
    EXPECT_NO_THROW(Write(writer, GetParam()));
    for (size_t i = 0; i < width; ++i)
        EXPECT_EQ(bytes[i + 1], GetParam() == Operation::Skip ? 0xcc : i + 1);
    EXPECT_EQ(bytes.front(), 0xcc);
    EXPECT_EQ(bytes[width + 1], 0xcc);
    EXPECT_THROW(writer.byte(0), std::out_of_range);
}

TEST_P(SerializationOperations, ReaderAcceptsExactCapacity)
{
    const std::array<uint8_t, 6> bytes{0xcc, 1, 2, 3, 4, 0xcc};
    image::Reader reader(std::span{bytes}.subspan(1, Width(GetParam())));
    uint8_t output[]{0xcc, 0xcc, 0xcc};
    EXPECT_NO_THROW(Read(reader, GetParam(), output));
    if (GetParam() == Operation::Copy)
        for (int i = 0; i < 3; ++i) EXPECT_EQ(output[i], i + 1);
    EXPECT_THROW(reader.byte(), std::out_of_range);
}

TEST_P(SerializationOperations, ShortWriterDoesNotMutateBufferOrAdvanceCursor)
{
    for (size_t size = 0; size < Width(GetParam()); ++size)
    {
        SCOPED_TRACE(size);
        std::array<uint8_t, 6> bytes{};
        bytes.fill(0xcc);
        const auto before = bytes;
        image::Writer writer(std::span{bytes}.subspan(1, size));
        EXPECT_THROW(Write(writer, GetParam()), std::out_of_range);
        EXPECT_EQ(bytes, before);
        for (size_t i = 0; i < size; ++i) writer.byte(static_cast<uint8_t>(i));
        for (size_t i = 0; i < size; ++i) EXPECT_EQ(bytes[i + 1], i);
        EXPECT_THROW(writer.byte(0), std::out_of_range);
    }
}

TEST_P(SerializationOperations, ShortReaderDoesNotMutateOutputOrAdvanceCursor)
{
    for (size_t size = 0; size < Width(GetParam()); ++size)
    {
        SCOPED_TRACE(size);
        const std::array<uint8_t, 4> bytes{1, 2, 3, 4};
        image::Reader reader(std::span{bytes}.first(size));
        uint8_t output[]{0xcc, 0xcc, 0xcc};
        EXPECT_THROW(Read(reader, GetParam(), output), std::out_of_range);
        for (const auto value : output) EXPECT_EQ(value, 0xcc);
        for (size_t i = 0; i < size; ++i) EXPECT_EQ(reader.byte(), i + 1);
        EXPECT_THROW(reader.byte(), std::out_of_range);
    }
}

INSTANTIATE_TEST_SUITE_P(AllOperations, SerializationOperations,
    testing::Values(Operation::Byte, Operation::Word, Operation::Dword, Operation::Copy, Operation::Skip),
    [](const testing::TestParamInfo<Operation>& info) {
        switch (info.param) {
        case Operation::Byte: return "Byte";
        case Operation::Word: return "Word";
        case Operation::Dword: return "Dword";
        case Operation::Copy: return "Copy";
        default: return "Skip";
        }
    });
}

TEST(SerializationTests, MixedFieldsHaveExpectedLayoutAndRoundTrip)
{
    std::array<uint8_t, 13> bytes{};
    bytes.fill(0xcc);
    image::Writer writer(std::span{bytes}.subspan(1, 11));
    const uint16_t source[]{0x1234};
    writer.byte(0xab);
    writer.word(0x1234);
    writer.skip(2);
    writer.dword(0x89abcdef);
    writer.copy(source);
    EXPECT_EQ(bytes[0], 0xcc);
    EXPECT_EQ(bytes[1], 0xab);
    EXPECT_EQ(bytes[2], 0x34);
    EXPECT_EQ(bytes[3], 0x12);
    EXPECT_EQ(bytes[4], 0xcc);
    EXPECT_EQ(bytes[5], 0xcc);
    EXPECT_EQ(bytes[6], 0xef);
    EXPECT_EQ(bytes[9], 0x89);
    EXPECT_EQ(bytes[12], 0xcc);
    image::Reader reader(std::span{bytes}.subspan(1, 11));
    EXPECT_EQ(reader.byte(), 0xab);
    EXPECT_EQ(reader.word(), 0x1234);
    reader.skip(2);
    EXPECT_EQ(reader.dword(), 0x89abcdefu);
    uint16_t target[]{0};
    reader.copy(target);
    EXPECT_EQ(target[0], source[0]);
    EXPECT_THROW(reader.byte(), std::out_of_range);
}

TEST(SerializationTests, ZeroSkipWorksOnEmptySpansAndHugeSkipDoesNotAdvance)
{
    image::Writer emptyWriter({});
    image::Reader emptyReader({});
    EXPECT_NO_THROW(emptyWriter.skip(0));
    EXPECT_NO_THROW(emptyReader.skip(0));
    std::array<uint8_t, 1> bytes{0xab};
    image::Writer writer(bytes);
    image::Reader reader(bytes);
    EXPECT_THROW(writer.skip((std::numeric_limits<size_t>::max)()), std::out_of_range);
    EXPECT_THROW(reader.skip((std::numeric_limits<size_t>::max)()), std::out_of_range);
    EXPECT_EQ(reader.byte(), 0xab);
    writer.byte(0xcd);
    EXPECT_EQ(bytes[0], 0xcd);
}
