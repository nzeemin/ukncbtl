#pragma once

#define BUFSIZE	((SAMPLERATE/25)*4)
#define BLOCK_COUNT	8
#define BLOCK_SIZE  BUFSIZE


class CSoundGen
{
public:
	CSoundGen(void);
public:
	~CSoundGen(void);
public:
	void FeedDAC(unsigned short L, unsigned short R);
};
