// SoundGen.h
//

#pragma once

#define BUFSIZE	((SAMPLERATE/25)*4)
#define BLOCK_COUNT	8
#define BLOCK_SIZE  BUFSIZE

void SoundGen_Initialize();
void SoundGen_Finalize();
void CALLBACK SoundGen_FeedDAC(unsigned short L, unsigned short R);
