/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

#pragma once
#include <cstdint>
#include <cstdio>
#include <expected>
#include <memory>
#include <tchar.h>

enum class WavError
{
    invalid_argument,
    open_failed,
    invalid_format,
    unsupported_format,
    io_error,
    end_of_data,
    invalid_position,
};

// A PCM WAV stream. Recordings are mono, 8-bit PCM. Read samples are returned
// as unsigned 32-bit values with silence at 0x80000000, regardless of source depth.
class WavPcmFile
{
public:
    [[nodiscard]] static std::expected<WavPcmFile, WavError> Create(LPCTSTR filename, int sampleRate);
    [[nodiscard]] static std::expected<WavPcmFile, WavError> Open(LPCTSTR filename);

    WavPcmFile(const WavPcmFile&) = delete;
    WavPcmFile& operator=(const WavPcmFile&) = delete;
    WavPcmFile(WavPcmFile&&) noexcept = default;
    WavPcmFile& operator=(WavPcmFile&&) = delete;
    ~WavPcmFile();

    [[nodiscard]] int Frequency() const { return sampleFrequency_; }
    [[nodiscard]] uint32_t Length() const { return dataSize_ / blockAlign_; }
    [[nodiscard]] uint32_t Position() const { return position_; }
    [[nodiscard]] std::expected<void, WavError> SetPosition(uint32_t position);
    [[nodiscard]] std::expected<uint32_t, WavError> ReadOne();
    [[nodiscard]] std::expected<void, WavError> WriteOne(uint32_t value);
    [[nodiscard]] std::expected<void, WavError> Close();

private:
    using File = std::unique_ptr<FILE, decltype(&std::fclose)>;
    WavPcmFile(File file, int sampleFrequency, int bitsPerSample, int blockAlign,
               uint32_t dataOffset, uint32_t dataSize, bool writing);

    File file_{nullptr, &std::fclose};
    int sampleFrequency_ = 0;
    int bitsPerSample_ = 0;
    int blockAlign_ = 0;
    uint32_t dataOffset_ = 0;
    uint32_t dataSize_ = 0;
    uint32_t position_ = 0;
    bool writing_ = false;
};
