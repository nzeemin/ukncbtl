#pragma once

#define BUFSIZE	1024
#define BLOCK_COUNT	8
#define BLOCK_SIZE  (2048*1)


class CSoundGen
{
public:
	CSoundGen(void);
public:
	~CSoundGen(void);
public:
	void FeedDAC(unsigned short L, unsigned short R);
};
