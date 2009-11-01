// SoundGen.cpp
//

#include "StdAfx.h"
#include "emubase\Emubase.h"
#include "SoundGen.h"
#include "Mmsystem.h"


//////////////////////////////////////////////////////////////////////

static void CALLBACK waveOutProc(HWAVEOUT, UINT, DWORD, DWORD, DWORD);

static CRITICAL_SECTION waveCriticalSection;
static WAVEHDR*         waveBlocks;
static volatile int     waveFreeBlockCount;
static int              waveCurrentBlock;

static bool m_SoundGenInitialized = FALSE;


HWAVEOUT hWaveOut; 

WAVEFORMATEX wfx;  
char buffer[BUFSIZE]; 

int bufcurpos;


//////////////////////////////////////////////////////////////////////


static void CALLBACK WaveCallback(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    int* freeBlockCounter = (int*)dwInstance;
    if (uMsg != WOM_DONE)
        return;

    EnterCriticalSection(&waveCriticalSection);
    (*freeBlockCounter)++;
	
    LeaveCriticalSection(&waveCriticalSection);
}

void SoundGen_Initialize()
{
    if (m_SoundGenInitialized)
        return;

    unsigned char* mbuffer;

    DWORD totalBufferSize = (BLOCK_SIZE + sizeof(WAVEHDR)) * BLOCK_COUNT;
    
    mbuffer = (unsigned char*) HeapAlloc(
        GetProcessHeap(), 
        HEAP_ZERO_MEMORY, 
        totalBufferSize);
    if (mbuffer == NULL)
    {
        //ExitProcess(1);
		return;
    }

    waveBlocks = (WAVEHDR*)mbuffer;
    mbuffer += sizeof(WAVEHDR) * BLOCK_COUNT;
    for (int i = 0; i < BLOCK_COUNT; i++) {
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
        Sleep(10);

	for (int i = 0; i < waveFreeBlockCount; i++)
    {
        if (waveBlocks[i].dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(hWaveOut, &waveBlocks[i], sizeof(WAVEHDR));
    }

	DeleteCriticalSection(&waveCriticalSection);
    waveOutClose(hWaveOut);

    HeapFree(GetProcessHeap(), 0, waveBlocks);
    waveBlocks = NULL;

    m_SoundGenInitialized = FALSE;
}

void CALLBACK SoundGen_FeedDAC(unsigned short L, unsigned short R)
{
	unsigned int word;
	WAVEHDR* current;

//return;
	if(!m_SoundGenInitialized)
		return;

	word = ((unsigned int)R<<16)+L;
	memcpy(&buffer[bufcurpos], &word, 4);
	bufcurpos += 4;

	if (bufcurpos >= BUFSIZE)
	{
		current = &waveBlocks[waveCurrentBlock];

		if (current->dwFlags & WHDR_PREPARED) 
			waveOutUnprepareHeader(hWaveOut, current, sizeof(WAVEHDR));
		
		memcpy(current->lpData, buffer, BUFSIZE);
		current->dwBufferLength = BLOCK_SIZE;
       
		waveOutPrepareHeader(hWaveOut, current, sizeof(WAVEHDR));
		waveOutWrite(hWaveOut, current, sizeof(WAVEHDR));
        
		EnterCriticalSection(&waveCriticalSection);
		waveFreeBlockCount--;
		LeaveCriticalSection(&waveCriticalSection);
        
		while(!waveFreeBlockCount)
			Sleep(1);

		waveCurrentBlock++;
		if (waveCurrentBlock >= BLOCK_COUNT)
			waveCurrentBlock=0;

		bufcurpos = 0;
	}
}


//////////////////////////////////////////////////////////////////////
