/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// WavPcmFile.cpp

#include "stdafx.h"
#include "WavPcmFile.h"
#include "LittleEndian.h"
#include <cstdio>
#include <array>
#include <climits>
#include <memory>
#include <share.h>


//////////////////////////////////////////////////////////////////////

// WAV PCM format description: https://ccrma.stanford.edu/courses/422/projects/WaveFormat/

static const char magic1[4] = { 'R', 'I', 'F', 'F' };
static const char magic2[4] = { 'W', 'A', 'V', 'E' };
static const char format_tag_id[4] = { 'f', 'm', 't', ' ' };
static const char data_tag_id[4] = { 'd', 'a', 't', 'a' };

const int WAV_FORMAT_PCM = 1;

struct WAVPCMFILE
{
    FILE* fpFile;
    int nChannels;
    int nBitsPerSample;
    int nSampleFrequency;
    int nBlockAlign;
    uint32_t dwDataOffset;
    uint32_t dwDataSize;
    uint32_t dwCurrentPosition;
    bool okWriting;
};

int WavPcmFile_GetFrequency(HWAVPCMFILE wavpcmfile)
{
    if (wavpcmfile == INVALID_HANDLE_VALUE)
        return 0;

    WAVPCMFILE* pWavPcm = reinterpret_cast<WAVPCMFILE*>(wavpcmfile);

    return pWavPcm->nSampleFrequency;
}

uint32_t WavPcmFile_GetLength(HWAVPCMFILE wavpcmfile)
{
    if (wavpcmfile == INVALID_HANDLE_VALUE)
        return 0;

    WAVPCMFILE* pWavPcm = reinterpret_cast<WAVPCMFILE*>(wavpcmfile);

    return pWavPcm->dwDataSize / pWavPcm->nBlockAlign;
}

uint32_t WavPcmFile_GetPosition(HWAVPCMFILE wavpcmfile)
{
    if (wavpcmfile == INVALID_HANDLE_VALUE)
        return 0;

    WAVPCMFILE* pWavPcm = reinterpret_cast<WAVPCMFILE*>(wavpcmfile);

    return pWavPcm->dwCurrentPosition;
}

void WavPcmFile_SetPosition(HWAVPCMFILE wavpcmfile, uint32_t position)
{
    if (wavpcmfile == INVALID_HANDLE_VALUE)
        return;

    WAVPCMFILE* pWavPcm = reinterpret_cast<WAVPCMFILE*>(wavpcmfile);

    uint32_t offsetInData = position * pWavPcm->nBlockAlign;
    ::fseek(pWavPcm->fpFile, pWavPcm->dwDataOffset + offsetInData, SEEK_SET);

    pWavPcm->dwCurrentPosition = position;
}

HWAVPCMFILE WavPcmFile_Create(LPCTSTR filename, int sampleRate)
{
    if (sampleRate <= 0)
        return static_cast<HWAVPCMFILE>(INVALID_HANDLE_VALUE);
    const int bitsPerSample = 8;
    const int channels = 1;
    const int blockAlign = channels * bitsPerSample / 8;

    std::unique_ptr<FILE, decltype(&::fclose)> file(::_tfsopen(filename, _T("w+b"), _SH_DENYWR), &::fclose);
    if (!file)
        return static_cast<HWAVPCMFILE>(INVALID_HANDLE_VALUE);  // Failed to create file

    // Prepare and write file header
    uint8_t consolidated_header[12 + 8 + 16 + 8];
    ::memset(consolidated_header, 0, sizeof(consolidated_header));

    memcpy(&consolidated_header[0], magic1, 4);  // RIFF
    memcpy(&consolidated_header[8], magic2, 4);  // WAVE

    memcpy(&consolidated_header[12], format_tag_id, 4);  // fmt
    const uint32_t formatSize = 16;
    const uint16_t format = WAV_FORMAT_PCM, channelCount = channels, align = blockAlign, bits = bitsPerSample;
    const uint32_t rate = static_cast<uint32_t>(sampleRate), byteRate = rate * blockAlign;
    auto header = std::span{consolidated_header};
    little_endian::write<uint32_t>(header.subspan<16, 4>(), formatSize);
    little_endian::write<uint16_t>(header.subspan<20, 2>(), format);
    little_endian::write<uint16_t>(header.subspan<22, 2>(), channelCount);
    little_endian::write<uint32_t>(header.subspan<24, 4>(), rate);
    little_endian::write<uint32_t>(header.subspan<28, 4>(), byteRate);
    little_endian::write<uint16_t>(header.subspan<32, 2>(), align);
    little_endian::write<uint16_t>(header.subspan<34, 2>(), bits);

    memcpy(&consolidated_header[36], data_tag_id, 4);  // data

    // Write consolidated header
    size_t bytesWritten = ::fwrite(consolidated_header, 1, sizeof(consolidated_header), file.get());
    if (bytesWritten != sizeof(consolidated_header))
    {
        return static_cast<HWAVPCMFILE>(INVALID_HANDLE_VALUE);  // Failed to write consolidated header
    }

    auto pWavPcm = std::make_unique<WAVPCMFILE>();
    pWavPcm->fpFile = file.get();
    pWavPcm->nChannels = channels;
    pWavPcm->nSampleFrequency = sampleRate;
    pWavPcm->nBitsPerSample = bitsPerSample;
    pWavPcm->nBlockAlign = blockAlign;
    pWavPcm->dwDataOffset = sizeof(consolidated_header);
    pWavPcm->dwDataSize = 0;
    pWavPcm->okWriting = true;

    WavPcmFile_SetPosition(reinterpret_cast<HWAVPCMFILE>(pWavPcm.get()), 0);
    file.release();
    return reinterpret_cast<HWAVPCMFILE>(pWavPcm.release());
}

HWAVPCMFILE WavPcmFile_Open(LPCTSTR filename)
{
    std::unique_ptr<FILE, decltype(&::fclose)> file(::_tfsopen(filename, _T("rb"), _SH_DENYWR), &::fclose);
    FILE* fpFileOpen = file.get();
    if (fpFileOpen == nullptr)
        return static_cast<HWAVPCMFILE>(INVALID_HANDLE_VALUE);  // Failed to open file

    uint32_t offset = 0;
    if (::fseek(fpFileOpen, 0, SEEK_END) != 0)
        return static_cast<HWAVPCMFILE>(INVALID_HANDLE_VALUE);
    long fileLength = ::ftell(fpFileOpen);
    if (fileLength < 12 || ::fseek(fpFileOpen, 0, SEEK_SET) != 0)
        return static_cast<HWAVPCMFILE>(INVALID_HANDLE_VALUE);
    uint32_t fileSize = static_cast<uint32_t>(fileLength);

    uint8_t fileHeader[12];
    size_t bytesRead = ::fread(fileHeader, 1, sizeof(fileHeader), fpFileOpen);
    if (bytesRead != sizeof(fileHeader) ||
        memcmp(&fileHeader[0], magic1, 4) != 0 ||
        memcmp(&fileHeader[8], magic2, 4) != 0)
    {
        return static_cast<HWAVPCMFILE>(INVALID_HANDLE_VALUE);  // Failed to read file header OR invalid 'RIFF' tag OR invalid 'WAVE' tag
    }
    offset += bytesRead;

    uint32_t riffSize = little_endian::read<uint32_t>(std::span{fileHeader}.subspan<4, 4>());
    if (riffSize < 4 || riffSize > fileSize - 8)
        return static_cast<HWAVPCMFILE>(INVALID_HANDLE_VALUE);
    uint32_t statedSize = riffSize + 8;

    uint8_t tagHeader[8];
    std::array<uint8_t, 16> formatTag{};
    bool formatSpecified = false;
    bool dataSpecified = false;
    uint16_t formatType = 1, channels = 1, bitsPerSample = 1, blockAlign = 0;
    uint32_t sampleFrequency = 22050, bytesPerSecond, dataOffset = 0, dataSize = 0;
    while (offset < statedSize)
    {
        if (statedSize - offset < sizeof(tagHeader))
            return static_cast<HWAVPCMFILE>(INVALID_HANDLE_VALUE);
        bytesRead = ::fread(tagHeader, 1, sizeof(tagHeader), fpFileOpen);
        if (bytesRead != sizeof(tagHeader))
        {
            return static_cast<HWAVPCMFILE>(INVALID_HANDLE_VALUE);  // Failed to read tag header
        }
        offset += bytesRead;

        uint32_t tagSize = little_endian::read<uint32_t>(std::span{tagHeader}.subspan<4, 4>());
        if (tagSize > statedSize - offset)
            return static_cast<HWAVPCMFILE>(INVALID_HANDLE_VALUE);
        if (!memcmp(tagHeader, format_tag_id, 4))
        {
            if (formatSpecified || tagSize < formatTag.size())
            {
                return static_cast<HWAVPCMFILE>(INVALID_HANDLE_VALUE);  // Wrong tag header
            }
            formatSpecified = true;

            bytesRead = ::fread(formatTag.data(), 1, formatTag.size(), fpFileOpen);
            if (bytesRead != formatTag.size())
            {
                return static_cast<HWAVPCMFILE>(INVALID_HANDLE_VALUE);  // Failed to read format tag
            }

            const auto formatBytes = std::span{formatTag};
            formatType = little_endian::read<uint16_t>(formatBytes.subspan<0, 2>());
            channels = little_endian::read<uint16_t>(formatBytes.subspan<2, 2>());
            sampleFrequency = little_endian::read<uint32_t>(formatBytes.subspan<4, 4>());
            bytesPerSecond = little_endian::read<uint32_t>(formatBytes.subspan<8, 4>());
            blockAlign = little_endian::read<uint16_t>(formatBytes.subspan<12, 2>());
            bitsPerSample = little_endian::read<uint16_t>(formatBytes.subspan<14, 2>());

            if (formatType != WAV_FORMAT_PCM)
            {
                return static_cast<HWAVPCMFILE>(INVALID_HANDLE_VALUE);  // Unsupported format
            }
            if ((bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 32) ||
                channels == 0 || blockAlign == 0 || blockAlign > 16 ||
                blockAlign != channels * (bitsPerSample / 8) ||
                sampleFrequency == 0 || sampleFrequency > INT_MAX ||
                static_cast<uint64_t>(sampleFrequency) * blockAlign != bytesPerSecond)
            {
                return static_cast<HWAVPCMFILE>(INVALID_HANDLE_VALUE);  // Wrong format tag
            }
        }
        else if (!memcmp(tagHeader, data_tag_id, 4))
        {
            if (!formatSpecified || dataSpecified || tagSize % blockAlign != 0)
            {
                return static_cast<HWAVPCMFILE>(INVALID_HANDLE_VALUE);  // Wrong tag
            }

            dataSpecified = true;
            dataOffset = offset;
            dataSize = tagSize;
        }
        else  // Ignore all other tags
        {
        }

        offset += tagSize;
        // Older versions of the emulator wrote odd-sized final data chunks without padding.
        if ((tagSize & 1) && offset < statedSize)
            ++offset;
        if (::fseek(fpFileOpen, offset, SEEK_SET) != 0)
            return static_cast<HWAVPCMFILE>(INVALID_HANDLE_VALUE);
    }

    if (!formatSpecified || !dataSpecified)
        return static_cast<HWAVPCMFILE>(INVALID_HANDLE_VALUE);

    auto pWavPcm = std::make_unique<WAVPCMFILE>();
    pWavPcm->fpFile = fpFileOpen;
    pWavPcm->nChannels = channels;
    pWavPcm->nSampleFrequency = sampleFrequency;
    pWavPcm->nBitsPerSample = bitsPerSample;
    pWavPcm->nBlockAlign = blockAlign;
    pWavPcm->dwDataOffset = dataOffset;
    pWavPcm->dwDataSize = dataSize;
    pWavPcm->okWriting = false;

    file.release();

    WavPcmFile_SetPosition(reinterpret_cast<HWAVPCMFILE>(pWavPcm.get()), 0);

    return reinterpret_cast<HWAVPCMFILE>(pWavPcm.release());
}

void WavPcmFile_Close(HWAVPCMFILE wavpcmfile)
{
    if (wavpcmfile == INVALID_HANDLE_VALUE)
        return;

    WAVPCMFILE* pWavPcm = reinterpret_cast<WAVPCMFILE*>(wavpcmfile);

    if (pWavPcm->okWriting)
    {
        if (pWavPcm->dwDataSize & 1)
        {
            ::fseek(pWavPcm->fpFile, 0, SEEK_END);
            const uint8_t padding = 0;
            ::fwrite(&padding, 1, 1, pWavPcm->fpFile);
        }
        // Write data chunk size
        ::fseek(pWavPcm->fpFile, 4, SEEK_SET);
        uint32_t chunkSize = 36 + pWavPcm->dwDataSize + (pWavPcm->dwDataSize & 1);
        std::array<uint8_t, 4> sizeBytes{};
        little_endian::write<uint32_t>(sizeBytes, chunkSize);
        size_t bytesWritten = ::fwrite(sizeBytes.data(), 1, sizeBytes.size(), pWavPcm->fpFile);
        // Write data subchunk size
        ::fseek(pWavPcm->fpFile, 40, SEEK_SET);
        little_endian::write<uint32_t>(sizeBytes, pWavPcm->dwDataSize);
        bytesWritten = ::fwrite(sizeBytes.data(), 1, sizeBytes.size(), pWavPcm->fpFile);
    }

    ::fclose(pWavPcm->fpFile);
    pWavPcm->fpFile = nullptr;
    delete pWavPcm;
}

bool WavPcmFile_WriteOne(HWAVPCMFILE wavpcmfile, unsigned int value)
{
    if (wavpcmfile == INVALID_HANDLE_VALUE)
        return false;

    WAVPCMFILE* pWavPcm = reinterpret_cast<WAVPCMFILE*>(wavpcmfile);
    if (!pWavPcm->okWriting)
        return false;
    ASSERT(pWavPcm->nBitsPerSample == 8);
    ASSERT(pWavPcm->nChannels == 1);

    uint8_t data = (uint8_t)((value >> 24) & 0xff);

    size_t bytesWritten = ::fwrite(&data, 1, 1, pWavPcm->fpFile);
    if (bytesWritten != 1)
        return false;

    pWavPcm->dwCurrentPosition++;
    pWavPcm->dwDataSize += pWavPcm->nBlockAlign;

    return true;
}

unsigned int WavPcmFile_ReadOne(HWAVPCMFILE wavpcmfile)
{
    if (wavpcmfile == INVALID_HANDLE_VALUE)
        return 0;

    WAVPCMFILE* pWavPcm = reinterpret_cast<WAVPCMFILE*>(wavpcmfile);
    if (pWavPcm->okWriting)
        return 0;

    // Read one sample
    uint32_t bytesToRead = pWavPcm->nBlockAlign;
    std::array<uint8_t, 16> data{};
    if (bytesToRead == 0 || bytesToRead > data.size())
        return 0;
    size_t bytesRead = ::fread(data.data(), 1, bytesToRead, pWavPcm->fpFile);
    if (bytesRead != bytesToRead)
        return 0;

    pWavPcm->dwCurrentPosition++;

    // Decode first channel
    unsigned int value = 0;
    switch (pWavPcm->nBitsPerSample)
    {
    case 8:
        value = data[0];
        value = value << 24;
        break;
    case 16:
        value = little_endian::read<uint16_t>(std::span{data}.first<2>());
        value = value << 16;
        break;
    case 32:
        value = little_endian::read<uint32_t>(std::span{data}.first<4>());
        break;
    }

    return value;
}


//////////////////////////////////////////////////////////////////////
