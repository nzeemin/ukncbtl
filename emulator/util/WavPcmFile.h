/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// WavPcmFile.h

#pragma once

//////////////////////////////////////////////////////////////////////

DECLARE_HANDLE(HWAVPCMFILE);

// Creates WAV file, one-channel, 8 bits per sample
HWAVPCMFILE WavPcmFile_Create(LPCTSTR filename, int sampleRate);
// Prepare WAV file of PCM format for reading
HWAVPCMFILE WavPcmFile_Open(LPCTSTR filename);
// Close WAV file
void WavPcmFile_Close(HWAVPCMFILE wavpcmfile);

// Samples per second, Hz
int WavPcmFile_GetFrequency(HWAVPCMFILE wavpcmfile);
// Length of the stream, in samples
uint32_t WavPcmFile_GetLength(HWAVPCMFILE wavpcmfile);

// Current position in the stream, in samples, zero-based
uint32_t WavPcmFile_GetPosition(HWAVPCMFILE wavpcmfile);
// Set current position in the stream, in samples, zero-based
void WavPcmFile_SetPosition(HWAVPCMFILE wavpcmfile, uint32_t position);

// Read one sample scaled to int type range
unsigned int WavPcmFile_ReadOne(HWAVPCMFILE wavpcmfile);
// Write one sample scaled to int type range
bool WavPcmFile_WriteOne(HWAVPCMFILE wavpcmfile, unsigned int value);


//////////////////////////////////////////////////////////////////////
