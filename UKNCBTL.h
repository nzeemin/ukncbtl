#pragma once

#include "resource.h"


//////////////////////////////////////////////////////////////////////


#define MAX_LOADSTRING 100

extern TCHAR g_szTitle[MAX_LOADSTRING];            // The title bar text
extern TCHAR g_szWindowClass[MAX_LOADSTRING];      // Main window class name


extern HINSTANCE g_hInst; // current instance

extern HWND g_hwnd;  // Main window handle


//////////////////////////////////////////////////////////////////////


void MainWindow_RegisterClass();
void MainWindow_RestoreSettings();
void MainWindow_UpdateMenu();
void MainWindow_UpdateAllViews();
BOOL MainWindow_InitToolbar();
BOOL MainWindow_InitStatusbar();
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
    ToolbarButtonCart2 = 9,
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
};

enum StatusbarParts
{
    StatusbarPartMessage = 0,
    StatusbarPartFPS = 1,
    StatusbarPartMZ0 = 2,
    StatusbarPartMZ1 = 3,
    StatusbarPartMZ2 = 4,
    StatusbarPartMZ3 = 5,
    StatusbarPartUptime = 6,
};


//////////////////////////////////////////////////////////////////////
// Settings

void Settings_Init();
void Settings_Done();
void Settings_SetFloppyFilePath(int slot, LPCTSTR sFilePath);
void Settings_GetFloppyFilePath(int slot, LPTSTR buffer);
void Settings_SetCartridgeFilePath(int slot, LPCTSTR sFilePath);
void Settings_GetCartridgeFilePath(int slot, LPTSTR buffer);
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
void Settings_SetToolbar(BOOL flag);
BOOL Settings_GetToolbar();
void Settings_SetKeyboard(BOOL flag);
BOOL Settings_GetKeyboard();
void Settings_SetTape(BOOL flag);
BOOL Settings_GetTape();


//////////////////////////////////////////////////////////////////////
