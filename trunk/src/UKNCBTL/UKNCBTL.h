/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

#pragma once

#include "resource.h"


//////////////////////////////////////////////////////////////////////


#define MAX_LOADSTRING 100

extern TCHAR g_szTitle[MAX_LOADSTRING];            // The title bar text
extern TCHAR g_szWindowClass[MAX_LOADSTRING];      // Main window class name

extern HINSTANCE g_hInst; // current instance


//////////////////////////////////////////////////////////////////////
// Main Window

extern HWND g_hwnd;  // Main window handle
extern HANDLE g_hAnimatedScreenshot;

void MainWindow_RegisterClass();
BOOL CreateMainWindow();
void MainWindow_RestoreSettings();
void MainWindow_UpdateMenu();
void MainWindow_UpdateAllViews();
void MainWindow_ShowHideDebug();
void MainWindow_ShowHideToolbar();
void MainWindow_ShowHideKeyboard();
void MainWindow_ShowHideTape();
void MainWindow_AdjustWindowSize();

void MainWindow_SetToolbarImage(int commandId, int imageIndex);
void MainWindow_SetStatusbarText(int part, LPCTSTR message);
void MainWindow_SetStatusbarBitmap(int part, UINT resourceId);
void MainWindow_SetStatusbarIcon(int part, HICON hIcon);

enum ToolbarButtons
{
    ToolbarButtonRun = 0,
    ToolbarButtonReset = 1,
    // Separator
    ToolbarButtonMZ0 = 3,
    ToolbarButtonMZ1 = 4,
    ToolbarButtonMZ2 = 5,
    ToolbarButtonMZ3 = 6,
    // Separator
    ToolbarButtonCart1 = 8,
    ToolbarButtonHard1 = 9,
    ToolbarButtonCart2 = 10,
    ToolbarButtonHard2 = 11,
    // Separator
    ToolbarButtonSound = 13,
    ToolbarButtonScreenshot = 14,
};

enum ToolbarButtonImages
{
    ToolbarImageRun = 0,
    ToolbarImageStep = 1,
    ToolbarImageReset = 2,
    ToolbarImageFloppyDisk = 3,
    ToolbarImageFloppySlot = 4,
    ToolbarImageCartridge = 5,
    ToolbarImageCartSlot = 6,
    ToolbarImageSoundOn = 7,
    ToolbarImageSoundOff = 8,
    ToolbarImageFloppyDiskWP = 9,
    ToolbarImageHardSlot = 10,
    ToolbarImageHardDrive = 11,
    ToolbarImageHardDriveWP = 12,
    ToolbarImageScreenshot = 13,
    ToolbarImageScreenshotStop = 14,
};

enum StatusbarParts
{
    StatusbarPartMessage = 0,
    StatusbarPartFloppyEngine = 1,
    StatusbarPartMZ0 = 2,
    StatusbarPartMZ1 = 3,
    StatusbarPartMZ2 = 4,
    StatusbarPartMZ3 = 5,
    StatusbarPartFPS = 6,
    StatusbarPartUptime = 7,
};

const DWORD CARTRIDGE1MODE_HARDDRIVE = 0x00000001;
const DWORD CARTRIDGE2MODE_HARDDRIVE = 0x00010000;


//////////////////////////////////////////////////////////////////////
// Settings

void Settings_Init();
void Settings_Done();
void Settings_SetFloppyFilePath(int slot, LPCTSTR sFilePath);
void Settings_GetFloppyFilePath(int slot, LPTSTR buffer);
void Settings_SetCartridgeFilePath(int slot, LPCTSTR sFilePath);
void Settings_GetCartridgeFilePath(int slot, LPTSTR buffer);
void Settings_SetHardFilePath(int slot, LPCTSTR sFilePath);
void Settings_GetHardFilePath(int slot, LPTSTR buffer);
void Settings_SetScreenViewMode(int mode);
int Settings_GetScreenViewMode();
void Settings_SetScreenHeightMode(int mode);
int Settings_GetScreenHeightMode();
void Settings_SetDebug(BOOL flag);
BOOL Settings_GetDebug();
void Settings_SetAutostart(BOOL flag);
BOOL Settings_GetAutostart();
void Settings_SetRealSpeed(BOOL flag);
BOOL Settings_GetRealSpeed();
void Settings_SetSound(BOOL flag);
BOOL Settings_GetSound();
void Settings_SetSoundVolume(WORD value);
WORD Settings_GetSoundVolume();
void Settings_SetToolbar(BOOL flag);
BOOL Settings_GetToolbar();
void Settings_SetKeyboard(BOOL flag);
BOOL Settings_GetKeyboard();
void Settings_SetTape(BOOL flag);
BOOL Settings_GetTape();
void Settings_SetSerial(BOOL flag);
BOOL Settings_GetSerial();
void Settings_GetSerialPort(LPTSTR buffer);
void Settings_SetSerialPort(LPCTSTR sValue);
void Settings_SetParallel(BOOL flag);
BOOL Settings_GetParallel();
int  Settings_GetNetStation();
void Settings_SetNetStation(int value);


//////////////////////////////////////////////////////////////////////
