/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Emulator.h

#pragma once

#include "emubase\Board.h"

//////////////////////////////////////////////////////////////////////


extern CMotherboard* g_pBoard;

extern BOOL g_okEmulatorRunning;

extern BYTE* g_pEmulatorRam[3];  // RAM values - for change tracking
extern BYTE* g_pEmulatorChangedRam[3];  // RAM change flags
extern WORD g_wEmulatorCpuPC;      // Current PC value
extern WORD g_wEmulatorPrevCpuPC;  // Previous PC value
extern WORD g_wEmulatorPpuPC;      // Current PC value
extern WORD g_wEmulatorPrevPpuPC;  // Previous PC value


//////////////////////////////////////////////////////////////////////


BOOL Emulator_Init();
void Emulator_Done();
void Emulator_SetCPUBreakpoint(WORD address);
void Emulator_SetPPUBreakpoint(WORD address);
BOOL Emulator_IsBreakpoint();
void Emulator_SetSound(BOOL soundOnOff);
BOOL Emulator_SetSerial(BOOL serialOnOff, LPCTSTR serialPort);
void Emulator_Start();
void Emulator_Stop();
void Emulator_Reset();
int Emulator_SystemFrame();

void Emulator_PrepareScreenRGB32(void* pBits, const DWORD* colors);

void Emulator_LoadROMCartridge(int slot, LPCTSTR sFilePath);

// Update cached values after Run or Step
void Emulator_OnUpdate();
WORD Emulator_GetChangeRamStatus(int addrtype, WORD address);

void Emulator_SaveImage(LPCTSTR sFilePath);
void Emulator_LoadImage(LPCTSTR sFilePath);


//////////////////////////////////////////////////////////////////////
