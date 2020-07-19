/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// SoundGen.h
//

#pragma once

#define BUFSIZE	((SAMPLERATE/25)*4)
#define BLOCK_COUNT	8
#define BLOCK_SIZE  BUFSIZE

void SoundGen_Initialize(WORD volume);
void SoundGen_Finalize();
void SoundGen_SetVolume(WORD volume);
void SoundGen_SetSpeed(WORD speedpercent);
void CALLBACK SoundGen_FeedDAC(unsigned short L, unsigned short R);
