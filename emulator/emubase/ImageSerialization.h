/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once

#include "util/LittleEndian.h"
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <type_traits>

// Snapshot fields are little-endian, independent of pointer alignment.
namespace image {
class Writer {
public:
    explicit Writer(std::span<uint8_t> bytes) : bytes_(bytes) {}
    void word(uint16_t value)
    {
        ensure(2);
        little_endian::write<uint16_t>(bytes_.first<2>(), value);
        bytes_ = bytes_.subspan(2);
    }
    void dword(uint32_t value)
    {
        ensure(4);
        little_endian::write<uint32_t>(bytes_.first<4>(), value);
        bytes_ = bytes_.subspan(4);
    }
    void byte(uint8_t value)
    {
        ensure(1);
        bytes_.front() = value;
        bytes_ = bytes_.subspan(1);
    }
    template<class T, size_t N>
    void copy(const T (&src)[N])
    {
        static_assert(std::is_trivially_copyable_v<T>);
        const auto source = std::as_bytes(std::span{src});
        ensure(source.size());
        std::memcpy(bytes_.data(), source.data(), source.size());
        bytes_ = bytes_.subspan(source.size());
    }
    void skip(size_t count) { ensure(count); bytes_ = bytes_.subspan(count); }
private:
    void ensure(size_t count) const { if (count > bytes_.size()) throw std::out_of_range("snapshot write"); }
    std::span<uint8_t> bytes_;
};
class Reader {
public:
    explicit Reader(std::span<const uint8_t> bytes) : bytes_(bytes) {}
    uint16_t word()
    {
        ensure(2);
        const auto value = little_endian::read<uint16_t>(bytes_.first<2>());
        bytes_ = bytes_.subspan(2);
        return value;
    }
    uint32_t dword()
    {
        ensure(4);
        const auto value = little_endian::read<uint32_t>(bytes_.first<4>());
        bytes_ = bytes_.subspan(4);
        return value;
    }
    uint8_t byte()
    {
        ensure(1);
        const auto value = bytes_.front();
        bytes_ = bytes_.subspan(1);
        return value;
    }
    template<class T, size_t N>
    void copy(T (&dest)[N])
    {
        static_assert(std::is_trivially_copyable_v<T>);
        auto target = std::as_writable_bytes(std::span{dest});
        ensure(target.size());
        std::memcpy(target.data(), bytes_.data(), target.size());
        bytes_ = bytes_.subspan(target.size());
    }
    void skip(size_t count) { ensure(count); bytes_ = bytes_.subspan(count); }
private:
    void ensure(size_t count) const { if (count > bytes_.size()) throw std::out_of_range("snapshot read"); }
    std::span<const uint8_t> bytes_;
};
}
