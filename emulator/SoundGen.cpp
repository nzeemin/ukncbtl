﻿/*  This file is part of UKNCBTL.
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
#include "emubase/Emubase.h"
#include "SoundGen.h"
#include "mmsystem.h"

//////////////////////////////////////////////////////////////////////


static void CALLBACK waveOutProc(HWAVEOUT, UINT, DWORD, DWORD, DWORD);

static CRITICAL_SECTION waveCriticalSection;
static WAVEHDR*         waveBlocks;
static volatile int     waveFreeBlockCount;
static int              waveCurrentBlock;

static bool m_SoundGenInitialized = false;

HWAVEOUT hWaveOut;

WAVEFORMATEX wfx;
char buffer[BUFSIZE];

int bufcurpos;


//////////////////////////////////////////////////////////////////////


static void CALLBACK WaveCallback(HWAVEOUT /*hwo*/, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR /*dwParam1*/, DWORD_PTR /*dwParam2*/)
{
    int* freeBlockCounter = (int*)dwInstance;
    if (uMsg != WOM_DONE)
        return;

    EnterCriticalSection(&waveCriticalSection);
    (*freeBlockCounter)++;

    LeaveCriticalSection(&waveCriticalSection);
}

void SoundGen_Initialize(WORD volume)
{
    if (m_SoundGenInitialized)
        return;

    unsigned char* mbuffer;

    size_t totalBufferSize = (BLOCK_SIZE + sizeof(WAVEHDR)) * BLOCK_COUNT;

    mbuffer = static_cast<unsigned char*>(calloc(totalBufferSize, 1));
    if (mbuffer == nullptr)
        return;

    waveBlocks = (WAVEHDR*)mbuffer;
    mbuffer += sizeof(WAVEHDR) * BLOCK_COUNT;
    for (int i = 0; i < BLOCK_COUNT; i++)
    {
        waveBlocks[i].dwBufferLength = BLOCK_SIZE;
        waveBlocks[i].lpData = (LPSTR)mbuffer;
        mbuffer += BLOCK_SIZE;
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

    MMRESULT result = waveOutOpen(
            &hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)WaveCallback, (DWORD_PTR)&waveFreeBlockCount, CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR)
    {
        return;
    }

    waveOutSetVolume(hWaveOut, ((DWORD)volume << 16) | ((DWORD)volume));

    InitializeCriticalSection(&waveCriticalSection);
    bufcurpos = 0;

    m_SoundGenInitialized = true;
    //waveOutSetPlaybackRate(hWaveOut,0x00008000);
}

void SoundGen_Finalize()
{
    if (!m_SoundGenInitialized)
        return;

    while (waveFreeBlockCount < BLOCK_COUNT)
        Sleep(3);

    for (int i = 0; i < waveFreeBlockCount; i++)
    {
        if (waveBlocks[i].dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(hWaveOut, &waveBlocks[i], sizeof(WAVEHDR));
    }

    DeleteCriticalSection(&waveCriticalSection);
    waveOutClose(hWaveOut);

    ::free(waveBlocks);
    waveBlocks = nullptr;

    m_SoundGenInitialized = FALSE;
}

void SoundGen_SetVolume(WORD volume)
{
    if (!m_SoundGenInitialized)
        return;

    waveOutSetVolume(hWaveOut, ((DWORD)volume << 16) | ((DWORD)volume));
}

void SoundGen_SetSpeed(WORD speedpercent)
{
    DWORD dwRate = 0x00010000;
    if (speedpercent > 0 && speedpercent < 1000)
        dwRate = (((DWORD)speedpercent / 100) << 16) | ((speedpercent % 100) * 0x00010000 / 100);

    waveOutSetPlaybackRate(hWaveOut, dwRate);
}

void CALLBACK SoundGen_FeedDAC(unsigned short L, unsigned short R)
{
    if (!m_SoundGenInitialized)
        return;

    unsigned int word = ((unsigned int)R << 16) + L;
    memcpy(&buffer[bufcurpos], &word, 4);
    bufcurpos += 4;

    if (bufcurpos >= BUFSIZE)
    {
        WAVEHDR* current = &waveBlocks[waveCurrentBlock];

        if (current->dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(hWaveOut, current, sizeof(WAVEHDR));

        memcpy(current->lpData, buffer, BUFSIZE);
        current->dwBufferLength = BLOCK_SIZE;

        waveOutPrepareHeader(hWaveOut, current, sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, current, sizeof(WAVEHDR));

        EnterCriticalSection(&waveCriticalSection);
        waveFreeBlockCount--;
        LeaveCriticalSection(&waveCriticalSection);

        while (!waveFreeBlockCount)
            Sleep(1);

        waveCurrentBlock++;
        if (waveCurrentBlock >= BLOCK_COUNT)
            waveCurrentBlock = 0;

        bufcurpos = 0;
    }
}


//////////////////////////////////////////////////////////////////////
