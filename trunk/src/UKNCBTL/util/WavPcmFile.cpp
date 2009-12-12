// WavPcmFile.cpp

#include "stdafx.h"
#include "WavPcmFile.h"


//////////////////////////////////////////////////////////////////////

// WAV PCM format description: https://ccrma.stanford.edu/courses/422/projects/WaveFormat/

static const char magic1[4] = { 'R', 'I', 'F', 'F' };
static const char magic2[4] = { 'W', 'A', 'V', 'E' };
static const char format_tag_id[4] = { 'f', 'm', 't', ' ' };
static const char data_tag_id[4] = { 'd', 'a', 't', 'a' };

const int WAV_FORMAT_PCM = 1;

struct WAVPCMFILE
{
	HANDLE hFile;
	int nChannels;
	int nBitsPerSample;
	int nSampleFrequency;
	int nBlockAlign;
	DWORD dwDataOffset;
	DWORD dwDataSize;
	DWORD dwCurrentPosition;
    BOOL okWriting;
};

int WavPcmFile_GetFrequency(HWAVPCMFILE wavpcmfile)
{
	if (wavpcmfile == INVALID_HANDLE_VALUE)
		return 0;

	WAVPCMFILE* pWavPcm = (WAVPCMFILE*) wavpcmfile;

	return pWavPcm->nSampleFrequency;
}

DWORD WavPcmFile_GetLength(HWAVPCMFILE wavpcmfile)
{
	if (wavpcmfile == INVALID_HANDLE_VALUE)
		return 0;

	WAVPCMFILE* pWavPcm = (WAVPCMFILE*) wavpcmfile;

	return pWavPcm->dwDataSize / pWavPcm->nBlockAlign;
}

DWORD WavPcmFile_GetPosition(HWAVPCMFILE wavpcmfile)
{
	if (wavpcmfile == INVALID_HANDLE_VALUE)
		return 0;

	WAVPCMFILE* pWavPcm = (WAVPCMFILE*) wavpcmfile;

	return pWavPcm->dwCurrentPosition;
}

void WavPcmFile_SetPosition(HWAVPCMFILE wavpcmfile, DWORD position)
{
	if (wavpcmfile == INVALID_HANDLE_VALUE)
		return;

	WAVPCMFILE* pWavPcm = (WAVPCMFILE*) wavpcmfile;

	DWORD offsetInData = position * pWavPcm->nBlockAlign;
	::SetFilePointer(pWavPcm->hFile, pWavPcm->dwDataOffset + offsetInData, NULL, FILE_BEGIN);

	pWavPcm->dwCurrentPosition = position;
}

HWAVPCMFILE WavPcmFile_Create(LPCTSTR filename, int sampleRate)
{
	const int bitsPerSample = 8;
	const int channels = 1;
    const int blockAlign = channels * bitsPerSample / 8;

	HANDLE hFileNew = ::CreateFile(filename,
		GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFileNew == INVALID_HANDLE_VALUE)
		return (HWAVPCMFILE) INVALID_HANDLE_VALUE;  // Failed to create file

	// Prepare and write file header
	BYTE consolidated_header[12 + 8 + 16 + 8];
    ::ZeroMemory(consolidated_header, sizeof(consolidated_header));
	DWORD bytesWritten;

    memcpy(&consolidated_header[0], magic1, 4);  // RIFF
    memcpy(&consolidated_header[8], magic2, 4);  // WAVE
    
    memcpy(&consolidated_header[12], format_tag_id, 4);  // fmt
    *((DWORD*)(consolidated_header + 16)) = 16;  // Size of "fmt" chunk
    *((WORD*)(consolidated_header + 20)) = WAV_FORMAT_PCM;  // AudioFormat = PCM
    *((WORD*)(consolidated_header + 22)) = channels;  // NumChannels = mono
    *((DWORD*)(consolidated_header + 24)) = sampleRate;  // SampleRate
    *((DWORD*)(consolidated_header + 28)) = sampleRate * channels * bitsPerSample / 8;  // ByteRate
    *((WORD*)(consolidated_header + 32)) = blockAlign;
    *((WORD*)(consolidated_header + 34)) = bitsPerSample;

    memcpy(&consolidated_header[36], data_tag_id, 4);  // data

	// Write consolidated header
    ::WriteFile(hFileNew, consolidated_header, sizeof(consolidated_header), &bytesWritten, NULL);
	if (bytesWritten != sizeof(consolidated_header))
	{
		::CloseHandle(hFileNew);
		return (HWAVPCMFILE) INVALID_HANDLE_VALUE;  // Failed to write consolidated header
	}

	WAVPCMFILE* pWavPcm = (WAVPCMFILE*) ::LocalAlloc(LPTR, sizeof(WAVPCMFILE));
	pWavPcm->hFile = hFileNew;
	pWavPcm->nChannels = channels;
	pWavPcm->nSampleFrequency = sampleRate;
	pWavPcm->nBitsPerSample = bitsPerSample;
	pWavPcm->nBlockAlign = blockAlign;
	pWavPcm->dwDataOffset = sizeof(consolidated_header);
	pWavPcm->dwDataSize = 0;
    pWavPcm->okWriting = TRUE;

	WavPcmFile_SetPosition((HWAVPCMFILE) pWavPcm, 0);

	return (HWAVPCMFILE) pWavPcm;
}

HWAVPCMFILE WavPcmFile_Open(LPCTSTR filename)
{
	HANDLE hFileOpen = ::CreateFile(filename,
		GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFileOpen == INVALID_HANDLE_VALUE)
		return (HWAVPCMFILE) INVALID_HANDLE_VALUE;  // Failed to open file

	DWORD offset = 0;
	DWORD bytesRead;
	DWORD fileSize = ::GetFileSize(hFileOpen, NULL);

	UINT8 fileHeader[12];
	::ReadFile(hFileOpen, fileHeader, sizeof(fileHeader), &bytesRead, NULL);
	if (bytesRead != sizeof(fileHeader) ||
		memcmp(&fileHeader[0], magic1, 4) != 0 ||
		memcmp(&fileHeader[8], magic2, 4) != 0)
	{
		::CloseHandle(hFileOpen);
		return (HWAVPCMFILE) INVALID_HANDLE_VALUE;  // Failed to read file header OR invalid 'RIFF' tag OR invalid 'WAVE' tag
	}
	offset += bytesRead;

	DWORD statedSize = *((DWORD*)(fileHeader + 4)) + 8;
	if (statedSize > fileSize)
		statedSize = fileSize;

	UINT8 tagHeader[8];
	UINT8 formatTag[16];
	BOOL formatSpecified = FALSE;
	int formatType, channels, bitsPerSample, blockAlign;
	DWORD sampleFrequency, bytesPerSecond, dataOffset, dataSize;
	while (offset < statedSize)
	{
		::ReadFile(hFileOpen, tagHeader, sizeof(tagHeader), &bytesRead, NULL);
		if (bytesRead != sizeof(tagHeader))
		{
			::CloseHandle(hFileOpen);
			return (HWAVPCMFILE) INVALID_HANDLE_VALUE;  // Failed to read tag header
		}
		offset += bytesRead;

		DWORD tagSize = *(DWORD*)(tagHeader + 4);
		if (!memcmp(tagHeader, format_tag_id, 4))
		{
			if (formatSpecified || tagSize < sizeof(formatTag))
			{
				::CloseHandle(hFileOpen);
				return (HWAVPCMFILE) INVALID_HANDLE_VALUE;  // Wrong tag header
			}
			formatSpecified = TRUE;

			::ReadFile(hFileOpen, formatTag, sizeof(formatTag), &bytesRead, NULL);
			if (bytesRead != sizeof(formatTag))
			{
				::CloseHandle(hFileOpen);
				return (HWAVPCMFILE) INVALID_HANDLE_VALUE;  // Failed to read format tag
			}

			formatType = *(UINT16*)(formatTag);
			channels = *(UINT16*)(formatTag + 2);
			sampleFrequency = *(DWORD*)(formatTag + 4);
			bytesPerSecond = *(DWORD*)(formatTag + 8);
			blockAlign = *(UINT16*)(formatTag + 12);
			bitsPerSample = *(UINT16*)(formatTag + 14);

			if (formatType != WAV_FORMAT_PCM)
			{
				::CloseHandle(hFileOpen);
				return (HWAVPCMFILE) INVALID_HANDLE_VALUE;  // Unsupported format
			}
			if (sampleFrequency * bitsPerSample * channels / 8 != bytesPerSecond ||
				(bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 32))
			{
				::CloseHandle(hFileOpen);
				return (HWAVPCMFILE) INVALID_HANDLE_VALUE;  // Wrong format tag
			}
		}
		else if (!memcmp(tagHeader, data_tag_id, 4))
		{
			if (!formatSpecified)
			{
				::CloseHandle(hFileOpen);
				return (HWAVPCMFILE) INVALID_HANDLE_VALUE;  // Wrong tag
			}

			dataOffset = offset;
			dataSize = tagSize;
		}
		else  // Ignore all other tags
		{
		}

		offset += tagSize;
		::SetFilePointer(hFileOpen, offset, NULL, FILE_BEGIN);
	}

	WAVPCMFILE* pWavPcm = (WAVPCMFILE*) ::LocalAlloc(LPTR, sizeof(WAVPCMFILE));
	pWavPcm->hFile = hFileOpen;
	pWavPcm->nChannels = channels;
	pWavPcm->nSampleFrequency = sampleFrequency;
	pWavPcm->nBitsPerSample = bitsPerSample;
	pWavPcm->nBlockAlign = blockAlign;
	pWavPcm->dwDataOffset = dataOffset;
	pWavPcm->dwDataSize = dataSize;
    pWavPcm->okWriting = FALSE;

	WavPcmFile_SetPosition((HWAVPCMFILE) pWavPcm, 0);

	return (HWAVPCMFILE) pWavPcm;
}

void WavPcmFile_Close(HWAVPCMFILE wavpcmfile)
{
	if (wavpcmfile == INVALID_HANDLE_VALUE)
		return;

	WAVPCMFILE* pWavPcm = (WAVPCMFILE*) wavpcmfile;

    if (pWavPcm->okWriting)
    {
        DWORD bytesWritten;
        // Write data chunk size
	    ::SetFilePointer(pWavPcm->hFile, 4, NULL, FILE_BEGIN);
        DWORD chunkSize = 36 + pWavPcm->dwDataSize;
        ::WriteFile(pWavPcm->hFile, &chunkSize, 4, &bytesWritten, NULL);
        // Write data subchunk size
	    ::SetFilePointer(pWavPcm->hFile, 40, NULL, FILE_BEGIN);
        ::WriteFile(pWavPcm->hFile, &(pWavPcm->dwDataSize), 4, &bytesWritten, NULL);
    }

	::CloseHandle(pWavPcm->hFile);
	pWavPcm->hFile = INVALID_HANDLE_VALUE;
	::LocalFree((HLOCAL)pWavPcm);
}

void WavPcmFile_WriteOne(HWAVPCMFILE wavpcmfile, int value)
{
	if (wavpcmfile == INVALID_HANDLE_VALUE)
		return;

	WAVPCMFILE* pWavPcm = (WAVPCMFILE*) wavpcmfile;
    if (!pWavPcm->okWriting)
        return;
    ASSERT(pWavPcm->nBitsPerSample == 8);
    ASSERT(pWavPcm->nChannels == 1);

    BYTE data = (value >> 24) & 0xff;

    DWORD bytesWritten;
    ::WriteFile(pWavPcm->hFile, &data, 1, &bytesWritten, NULL);

    pWavPcm->dwCurrentPosition++;
    pWavPcm->dwDataSize += pWavPcm->nBlockAlign;
}

int WavPcmFile_ReadOne(HWAVPCMFILE wavpcmfile)
{
	if (wavpcmfile == INVALID_HANDLE_VALUE)
		return 0;

	WAVPCMFILE* pWavPcm = (WAVPCMFILE*) wavpcmfile;
    if (pWavPcm->okWriting)
        return 0;

	// Read one sample
	DWORD bytesToRead = pWavPcm->nBlockAlign;
	DWORD bytesRead;
	BYTE data[16];
	::ReadFile(pWavPcm->hFile, data, bytesToRead, &bytesRead, NULL);
	if (bytesRead != bytesToRead)
		return 0;

	pWavPcm->dwCurrentPosition++;

	// Decode first channel
	DWORD value;
	switch (pWavPcm->nBitsPerSample)
	{
	case 8:
		value = *data;
		value = value << 24;
		break;
	case 16:
		value = *((UINT16*)data);
		value = value << 16;
		break;
	case 32:
		value = *((UINT32*)data);
		break;
	}

	return *((INT32*)&value);
}


//////////////////////////////////////////////////////////////////////
