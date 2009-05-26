// WavPcmFile.h

#pragma once

//////////////////////////////////////////////////////////////////////

DECLARE_HANDLE(HWAVPCMFILE);

HWAVPCMFILE WavPcmFile_Create(LPCTSTR filename);
// Prepare WAV file of PCM format for reading
HWAVPCMFILE WavPcmFile_Open(LPCTSTR filename);
// Close WAV file
void WavPcmFile_Close(HWAVPCMFILE wavpcmfile);

// Samples per second, Hz
int WavPcmFile_GetFrequency(HWAVPCMFILE wavpcmfile);
// Length of the stream, in samples
DWORD WavPcmFile_GetLength(HWAVPCMFILE wavpcmfile);
// Current position in the stream, in samples, zero-based
DWORD WavPcmFile_GetPosition(HWAVPCMFILE wavpcmfile);
// Set current position in the stream, in samples, zero-based
void WavPcmFile_SetPosition(HWAVPCMFILE wavpcmfile, DWORD position);

// Read one sample scaled to int type range
int WavPcmFile_ReadOne(HWAVPCMFILE wavpcmfile);


//////////////////////////////////////////////////////////////////////
