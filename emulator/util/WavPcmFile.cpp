/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

#include "stdafx.h"
#include "WavPcmFile.h"
#include "LittleEndian.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <share.h>

namespace {
constexpr std::array<uint8_t, 4> riff{'R', 'I', 'F', 'F'};
constexpr std::array<uint8_t, 4> wave{'W', 'A', 'V', 'E'};
constexpr std::array<uint8_t, 4> formatId{'f', 'm', 't', ' '};
constexpr std::array<uint8_t, 4> dataId{'d', 'a', 't', 'a'};
constexpr uint16_t pcmFormat = 1;
}

WavPcmFile::WavPcmFile(File file, int sampleFrequency, int bitsPerSample, int blockAlign,
                       uint32_t dataOffset, uint32_t dataSize, bool writing)
    : file_(std::move(file)), sampleFrequency_(sampleFrequency), bitsPerSample_(bitsPerSample),
      blockAlign_(blockAlign), dataOffset_(dataOffset), dataSize_(dataSize), writing_(writing)
{
}

WavPcmFile::~WavPcmFile()
{
    (void)Close();
}

std::expected<WavPcmFile, WavError> WavPcmFile::Create(LPCTSTR filename, int sampleRate)
{
    if (!filename || sampleRate <= 0) return std::unexpected(WavError::invalid_argument);
    File file(::_tfsopen(filename, _T("w+b"), _SH_DENYWR), &std::fclose);
    if (!file) return std::unexpected(WavError::open_failed);

    constexpr uint16_t channels = 1, bits = 8, alignment = 1;
    const uint32_t rate = static_cast<uint32_t>(sampleRate);
    std::array<uint8_t, 44> header{};
    std::ranges::copy(riff, header.begin());
    std::ranges::copy(wave, header.begin() + 8);
    std::ranges::copy(formatId, header.begin() + 12);
    const auto bytes = std::span{header};
    little_endian::write<uint32_t>(bytes.subspan<16, 4>(), 16);
    little_endian::write<uint16_t>(bytes.subspan<20, 2>(), pcmFormat);
    little_endian::write<uint16_t>(bytes.subspan<22, 2>(), channels);
    little_endian::write<uint32_t>(bytes.subspan<24, 4>(), rate);
    little_endian::write<uint32_t>(bytes.subspan<28, 4>(), rate * alignment);
    little_endian::write<uint16_t>(bytes.subspan<32, 2>(), alignment);
    little_endian::write<uint16_t>(bytes.subspan<34, 2>(), bits);
    std::ranges::copy(dataId, header.begin() + 36);
    if (std::fwrite(header.data(), 1, header.size(), file.get()) != header.size())
        return std::unexpected(WavError::io_error);

    return WavPcmFile(std::move(file), sampleRate, bits, alignment,
                      static_cast<uint32_t>(header.size()), 0, true);
}

std::expected<WavPcmFile, WavError> WavPcmFile::Open(LPCTSTR filename)
{
    if (!filename) return std::unexpected(WavError::invalid_argument);
    File file(::_tfsopen(filename, _T("rb"), _SH_DENYWR), &std::fclose);
    if (!file) return std::unexpected(WavError::open_failed);
    if (std::fseek(file.get(), 0, SEEK_END) != 0) return std::unexpected(WavError::io_error);
    const long fileLength = std::ftell(file.get());
    if (fileLength < 0 || std::fseek(file.get(), 0, SEEK_SET) != 0)
        return std::unexpected(WavError::io_error);
    if (fileLength < 12) return std::unexpected(WavError::invalid_format);

    std::array<uint8_t, 12> header{};
    if (std::fread(header.data(), 1, header.size(), file.get()) != header.size())
        return std::unexpected(WavError::io_error);
    if (!std::equal(riff.begin(), riff.end(), header.begin()) ||
        !std::equal(wave.begin(), wave.end(), header.begin() + 8))
        return std::unexpected(WavError::invalid_format);
    const uint32_t riffSize = little_endian::read<uint32_t>(std::span{header}.subspan<4, 4>());
    if (riffSize < 4 || riffSize > static_cast<uint32_t>(fileLength) - 8)
        return std::unexpected(WavError::invalid_format);

    const uint32_t statedSize = riffSize + 8;
    uint32_t offset = static_cast<uint32_t>(header.size());
    uint32_t sampleFrequency = 0, dataOffset = 0, dataSize = 0;
    uint16_t bitsPerSample = 0, blockAlign = 0;
    bool formatSpecified = false, dataSpecified = false;
    while (offset < statedSize)
    {
        std::array<uint8_t, 8> chunk{};
        if (statedSize - offset < chunk.size()) return std::unexpected(WavError::invalid_format);
        if (std::fread(chunk.data(), 1, chunk.size(), file.get()) != chunk.size())
            return std::unexpected(WavError::io_error);
        offset += static_cast<uint32_t>(chunk.size());
        const uint32_t chunkSize = little_endian::read<uint32_t>(std::span{chunk}.subspan<4, 4>());
        if (chunkSize > statedSize - offset) return std::unexpected(WavError::invalid_format);
        if (std::equal(formatId.begin(), formatId.end(), chunk.begin()))
        {
            std::array<uint8_t, 16> format{};
            if (formatSpecified || chunkSize < format.size()) return std::unexpected(WavError::invalid_format);
            if (std::fread(format.data(), 1, format.size(), file.get()) != format.size())
                return std::unexpected(WavError::io_error);
            const auto fields = std::span{format};
            const auto type = little_endian::read<uint16_t>(fields.subspan<0, 2>());
            const auto channels = little_endian::read<uint16_t>(fields.subspan<2, 2>());
            sampleFrequency = little_endian::read<uint32_t>(fields.subspan<4, 4>());
            const auto bytesPerSecond = little_endian::read<uint32_t>(fields.subspan<8, 4>());
            blockAlign = little_endian::read<uint16_t>(fields.subspan<12, 2>());
            bitsPerSample = little_endian::read<uint16_t>(fields.subspan<14, 2>());
            if (type != pcmFormat) return std::unexpected(WavError::unsupported_format);
            if ((bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 32) ||
                channels == 0 || blockAlign == 0 || blockAlign > 16 ||
                blockAlign != channels * (bitsPerSample / 8) || sampleFrequency == 0 ||
                sampleFrequency > INT_MAX || static_cast<uint64_t>(sampleFrequency) * blockAlign != bytesPerSecond)
                return std::unexpected(WavError::invalid_format);
            formatSpecified = true;
        }
        else if (std::equal(dataId.begin(), dataId.end(), chunk.begin()))
        {
            if (!formatSpecified || dataSpecified || chunkSize % blockAlign != 0)
                return std::unexpected(WavError::invalid_format);
            dataSpecified = true;
            dataOffset = offset;
            dataSize = chunkSize;
        }
        offset += chunkSize;
        // Older emulator versions omitted padding on an odd-sized final data chunk.
        if ((chunkSize & 1) && offset < statedSize) ++offset;
        if (std::fseek(file.get(), static_cast<long>(offset), SEEK_SET) != 0)
            return std::unexpected(WavError::io_error);
    }
    if (!formatSpecified || !dataSpecified) return std::unexpected(WavError::invalid_format);
    if (std::fseek(file.get(), static_cast<long>(dataOffset), SEEK_SET) != 0)
        return std::unexpected(WavError::io_error);
    return WavPcmFile(std::move(file), static_cast<int>(sampleFrequency), bitsPerSample,
                      blockAlign, dataOffset, dataSize, false);
}

std::expected<void, WavError> WavPcmFile::SetPosition(uint32_t position)
{
    if (!file_) return std::unexpected(WavError::io_error);
    if (position > Length()) return std::unexpected(WavError::invalid_position);
    const auto offset = static_cast<uint64_t>(dataOffset_) + static_cast<uint64_t>(position) * blockAlign_;
    if (offset > LONG_MAX) return std::unexpected(WavError::invalid_position);
    if (std::fseek(file_.get(), static_cast<long>(offset), SEEK_SET) != 0)
        return std::unexpected(WavError::io_error);
    position_ = position;
    return {};
}

std::expected<uint32_t, WavError> WavPcmFile::ReadOne()
{
    if (!file_ || writing_) return std::unexpected(WavError::io_error);
    if (position_ >= Length()) return std::unexpected(WavError::end_of_data);
    std::array<uint8_t, 16> data{};
    if (std::fread(data.data(), 1, blockAlign_, file_.get()) != static_cast<size_t>(blockAlign_))
        return std::unexpected(WavError::io_error);
    ++position_;
    switch (bitsPerSample_)
    {
    case 8: return static_cast<uint32_t>(data[0]) << 24;
    case 16: return (static_cast<uint32_t>(little_endian::read<uint16_t>(std::span{data}.first<2>())) << 16) ^ 0x80000000u;
    case 32: return little_endian::read<uint32_t>(std::span{data}.first<4>()) ^ 0x80000000u;
    default: return std::unexpected(WavError::invalid_format);
    }
}

std::expected<void, WavError> WavPcmFile::WriteOne(uint32_t value)
{
    if (!file_ || !writing_) return std::unexpected(WavError::io_error);
    const uint64_t nextSize = (static_cast<uint64_t>(position_) + 1) * blockAlign_;
    if (static_cast<uint64_t>(dataOffset_) + nextSize + 1 > LONG_MAX)
        return std::unexpected(WavError::invalid_position);
    const auto sample = static_cast<uint8_t>(value >> 24);
    if (std::fwrite(&sample, 1, 1, file_.get()) != 1) return std::unexpected(WavError::io_error);
    ++position_;
    dataSize_ = (std::max)(dataSize_, static_cast<uint32_t>(nextSize));
    return {};
}

std::expected<void, WavError> WavPcmFile::Close()
{
    if (!file_) return {};
    bool ok = true;
    if (writing_)
    {
        if (dataSize_ & 1)
        {
            const uint8_t padding = 0;
            ok = std::fseek(file_.get(), static_cast<long>(dataOffset_ + dataSize_), SEEK_SET) == 0 &&
                 std::fwrite(&padding, 1, 1, file_.get()) == 1;
        }
        std::array<uint8_t, 4> sizeBytes{};
        little_endian::write<uint32_t>(sizeBytes, 36 + dataSize_ + (dataSize_ & 1));
        ok = ok && std::fseek(file_.get(), 4, SEEK_SET) == 0 &&
             std::fwrite(sizeBytes.data(), 1, sizeBytes.size(), file_.get()) == sizeBytes.size();
        little_endian::write<uint32_t>(sizeBytes, dataSize_);
        ok = ok && std::fseek(file_.get(), 40, SEEK_SET) == 0 &&
             std::fwrite(sizeBytes.data(), 1, sizeBytes.size(), file_.get()) == sizeBytes.size();
    }
    if (std::fclose(file_.release()) != 0) ok = false;
    return ok ? std::expected<void, WavError>{} : std::unexpected(WavError::io_error);
}
