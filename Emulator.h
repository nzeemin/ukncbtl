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

extern bool g_okEmulatorRunning;

extern BYTE* g_pEmulatorRam[3];  // RAM values - for change tracking
extern BYTE* g_pEmulatorChangedRam[3];  // RAM change flags
extern uint16_t g_wEmulatorCpuPC;      // Current PC value
extern uint16_t g_wEmulatorPrevCpuPC;  // Previous PC value
extern uint16_t g_wEmulatorPpuPC;      // Current PC value
extern uint16_t g_wEmulatorPrevPpuPC;  // Previous PC value


//////////////////////////////////////////////////////////////////////


bool Emulator_Init();
void Emulator_Done();
void Emulator_SetCPUBreakpoint(uint16_t address);
void Emulator_SetPPUBreakpoint(uint16_t address);
bool Emulator_IsBreakpoint();
void Emulator_SetSound(bool soundOnOff);
bool Emulator_SetSerial(bool serialOnOff, LPCTSTR serialPort);
void Emulator_SetParallel(bool parallelOnOff);
bool Emulator_SetNetwork(bool networkOnOff, LPCTSTR networkPort);
void Emulator_Start();
void Emulator_Stop();
void Emulator_Reset();
bool Emulator_SystemFrame();
uint32_t Emulator_GetUptime();  // UKNC uptime, in seconds
void Emulator_SetSpeed(uint16_t realspeed);

void Emulator_PrepareScreenRGB32(void* pBits, const uint32_t* colors);

bool Emulator_LoadROMCartridge(int slot, LPCTSTR sFilePath);

// Update cached values after Run or Step
void Emulator_OnUpdate();
uint16_t Emulator_GetChangeRamStatus(int addrtype, uint16_t address);

bool Emulator_SaveImage(LPCTSTR sFilePath);
bool Emulator_LoadImage(LPCTSTR sFilePath);


//////////////////////////////////////////////////////////////////////
