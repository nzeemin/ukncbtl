/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// SoundGen.cpp

#include "stdafx.h"
#include "util/Profiler.h"
#include "emubase/Emubase.h"
#include "SoundGen.h"
#include "SoundBackend.h"
#include "mmsystem.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <array>

//////////////////////////////////////////////////////////////////////


static std::array<WAVEHDR, BLOCK_COUNT> waveBlocks{};
static std::array<std::array<char, BLOCK_SIZE>, BLOCK_COUNT> waveSamples{};
static std::atomic<int> waveFreeBlockCount{0};
static int              waveCurrentBlock;

static bool m_SoundGenInitialized = false;
static sound::Backend backend;

bool sound::SetBackend(const Backend& replacement)
{
    if (m_SoundGenInitialized || !replacement.open || !replacement.close ||
        !replacement.prepare || !replacement.unprepare || !replacement.write ||
        !replacement.volume || !replacement.rate || !replacement.wait) return false;
    backend = replacement;
    return true;
}

HWAVEOUT hWaveOut;

WAVEFORMATEX wfx;
static std::array<char, BUFSIZE> buffer{};

int bufcurpos;


//////////////////////////////////////////////////////////////////////


static void CALLBACK WaveCallback(HWAVEOUT /*hwo*/, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR /*dwParam1*/, DWORD_PTR /*dwParam2*/)
{
    auto* freeBlockCounter = reinterpret_cast<std::atomic<int>*>(dwInstance);
    if (uMsg != WOM_DONE)
        return;

    freeBlockCounter->fetch_add(1);
}

void SoundGen_Initialize(WORD volume)
{
    if (m_SoundGenInitialized)
        return;

    waveBlocks = {};
    for (int i = 0; i < BLOCK_COUNT; i++)
    {
        waveBlocks[i].dwBufferLength = BLOCK_SIZE;
        waveBlocks[i].lpData = waveSamples[i].data();
    }

    waveFreeBlockCount = BLOCK_COUNT;
    waveCurrentBlock   = 0;

    wfx.nSamplesPerSec  = SAMPLERATE;
    wfx.wBitsPerSample  = 16;
    wfx.nChannels       = 2;
    wfx.cbSize          = 0;
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nBlockAlign     = (wfx.wBitsPerSample * wfx.nChannels) >> 3;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

    MMRESULT result = backend.open(
            &hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)WaveCallback, (DWORD_PTR)&waveFreeBlockCount, CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR)
    {
        return;
    }

    backend.volume(hWaveOut, ((DWORD)volume << 16) | ((DWORD)volume));

    bufcurpos = 0;

    m_SoundGenInitialized = true;
}

void SoundGen_Finalize()
{
    if (!m_SoundGenInitialized)
        return;

    while (waveFreeBlockCount < BLOCK_COUNT)
        backend.wait(1);

    for (int i = 0; i < BLOCK_COUNT; i++)
    {
        if (waveBlocks[i].dwFlags & WHDR_PREPARED)
            if (backend.unprepare(hWaveOut, &waveBlocks[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
                return; // Keep the device and headers alive so cleanup can be retried.
    }

    if (backend.close(hWaveOut) != MMSYSERR_NOERROR) return;

    m_SoundGenInitialized = false;
}

void SoundGen_SetVolume(WORD volume)
{
    if (!m_SoundGenInitialized)
        return;

    backend.volume(hWaveOut, ((DWORD)volume << 16) | ((DWORD)volume));
}

void SoundGen_SetSpeed(WORD speedpercent)
{
    if (!m_SoundGenInitialized)
        return;

    DWORD dwRate = 0x00010000;
    if (speedpercent > 0 && speedpercent < 1000)
        dwRate = (((DWORD)speedpercent / 100) << 16) | ((speedpercent % 100) * 0x00010000 / 100);

    backend.rate(hWaveOut, dwRate);
}

void CALLBACK SoundGen_FeedDAC(unsigned short L, unsigned short R)
{
    if (!m_SoundGenInitialized)
        return;

    const uint32_t word = (static_cast<uint32_t>(R) << 16) | L;
    const auto bytes = std::bit_cast<std::array<char, sizeof(word)>>(word);
    std::ranges::copy(bytes, buffer.begin() + bufcurpos);
    bufcurpos += 4;

    if (bufcurpos >= BUFSIZE)
    {
        UKNC_PROFILE_SCOPE("Audio submit");
        WAVEHDR* current = &waveBlocks[waveCurrentBlock];

        if ((current->dwFlags & WHDR_PREPARED) &&
            backend.unprepare(hWaveOut, current, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
        {
            bufcurpos = 0;
            return; // Do not overwrite a buffer still owned by the backend.
        }

        std::ranges::copy(buffer, current->lpData);
        current->dwBufferLength = BLOCK_SIZE;

        if (backend.prepare(hWaveOut, current, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
        {
            bufcurpos = 0;
            return;
        }
        waveFreeBlockCount.fetch_sub(1);
        if (backend.write(hWaveOut, current, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
        {
            waveFreeBlockCount.fetch_add(1);
            bufcurpos = 0;
            return;
        }

        {
            UKNC_PROFILE_SCOPE("Audio buffer wait");
            while (!waveFreeBlockCount)
                backend.wait(1);
        }

        waveCurrentBlock++;
        if (waveCurrentBlock >= BLOCK_COUNT)
            waveCurrentBlock = 0;

        bufcurpos = 0;
    }
}


//////////////////////////////////////////////////////////////////////
