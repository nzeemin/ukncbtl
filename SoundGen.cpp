#include "StdAfx.h"
#include "SoundGen.h"
#include "Mmsystem.h"


static void CALLBACK waveOutProc(HWAVEOUT, UINT, DWORD, DWORD, DWORD);

static CRITICAL_SECTION waveCriticalSection;
static WAVEHDR*         waveBlocks;
static volatile int     waveFreeBlockCount;
static int              waveCurrentBlock;

static bool SoundInitialized;


HWAVEOUT hWaveOut; 

WAVEFORMATEX wfx;  
char buffer[BUFSIZE]; 
int i;

int bufcurpos;

static void CALLBACK WaveCallback(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
    int* freeBlockCounter = (int*)dwInstance;
     if(uMsg != WOM_DONE)
        return;

    EnterCriticalSection(&waveCriticalSection);
    (*freeBlockCounter)++;
	

    LeaveCriticalSection(&waveCriticalSection);

	
}


CSoundGen::CSoundGen(void)
{
    unsigned char* buffer;
    int i;

	SoundInitialized=false;
	//return;
     DWORD totalBufferSize = (BLOCK_SIZE + sizeof(WAVEHDR)) * BLOCK_COUNT;
    
      if((buffer = (unsigned char*)HeapAlloc(
        GetProcessHeap(), 
        HEAP_ZERO_MEMORY, 
        totalBufferSize
    )) == NULL) {
        //ExitProcess(1);
		return;
    }

    waveBlocks = (WAVEHDR*)buffer;
    buffer += sizeof(WAVEHDR) * BLOCK_COUNT;
    for(i = 0; i < BLOCK_COUNT; i++) {
        waveBlocks[i].dwBufferLength = BLOCK_SIZE;
        waveBlocks[i].lpData = (LPSTR)buffer;
        buffer += BLOCK_SIZE;
    }
  	
	
	waveFreeBlockCount = BLOCK_COUNT;
    waveCurrentBlock   = 0;
    
	wfx.nSamplesPerSec  = 44100;  
    wfx.wBitsPerSample  = 16;     
    wfx.nChannels       = 2;      
    wfx.cbSize          = 0;      
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nBlockAlign     = (wfx.wBitsPerSample * wfx.nChannels) >> 3;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
	if((waveOutOpen(&hWaveOut,WAVE_MAPPER,&wfx,(DWORD)WaveCallback,(DWORD)&waveFreeBlockCount,CALLBACK_FUNCTION)) != MMSYSERR_NOERROR) 
	{
 		return;
    }
	InitializeCriticalSection(&waveCriticalSection);
	bufcurpos=0;

	SoundInitialized=true;
	waveOutSetPlaybackRate(hWaveOut,0x00008000);

}

CSoundGen::~CSoundGen(void)
{
    while(waveFreeBlockCount < BLOCK_COUNT)
        Sleep(10);

	for(i = 0; i < waveFreeBlockCount; i++) 
        if(waveBlocks[i].dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(hWaveOut, &waveBlocks[i], sizeof(WAVEHDR));

	DeleteCriticalSection(&waveCriticalSection);
	HeapFree(GetProcessHeap(), 0, waveBlocks);
    waveOutClose(hWaveOut);

}

void CSoundGen::FeedDAC(unsigned short L, unsigned short R)
{
	unsigned int word;
	WAVEHDR* current;
	int remains;
	unsigned int flag;
	int size;
	char * data;

//return;
	if(!SoundInitialized)
		return;

	flag=1;

	if(bufcurpos<BUFSIZE)// buffer still has place to put some info
	{
		word=((unsigned int)R<<16)+L;
		memcpy(&buffer[bufcurpos],&word,4);
		bufcurpos+=4;
		flag=0;
	}
	

	if(bufcurpos>=BUFSIZE)
	{
		current = &waveBlocks[waveCurrentBlock];
		size=sizeof(buffer);
		data=buffer;
		while(size > 0) {
			if(current->dwFlags & WHDR_PREPARED) 
				waveOutUnprepareHeader(hWaveOut, current, size);

			if(size < (int)(BLOCK_SIZE - current->dwUser)) {
				memcpy(current->lpData + current->dwUser, data, size);
				current->dwUser += size;
				break;
			}

			remains = BLOCK_SIZE - current->dwUser;
			memcpy(current->lpData + current->dwUser, data, remains);
			size -= remains;
			data += remains;
			current->dwBufferLength = BLOCK_SIZE;
       
			waveOutPrepareHeader(hWaveOut, current, sizeof(WAVEHDR));
			waveOutWrite(hWaveOut, current, sizeof(WAVEHDR));
        
			EnterCriticalSection(&waveCriticalSection);
			waveFreeBlockCount--;
			LeaveCriticalSection(&waveCriticalSection);
        
			while(!waveFreeBlockCount)
				Sleep(1);
				//return;
			//newfel

			waveCurrentBlock++;
			if(waveCurrentBlock >= BLOCK_COUNT)
				waveCurrentBlock=0;

			current = &waveBlocks[waveCurrentBlock];
			current->dwUser = 0;
		}


		bufcurpos=0;
		if(flag)
		{
			word=((unsigned int)R<<16)+L;
			memcpy(&buffer[bufcurpos],&word,4);
			bufcurpos+=4;
		}


	}
}
