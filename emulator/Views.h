/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Views.h
// Defines for all views of the application

#pragma once

//////////////////////////////////////////////////////////////////////
// Window class names

const LPCTSTR CLASSNAME_SCREENVIEW  = _T("UKNCBTLSCREEN");
const LPCTSTR CLASSNAME_KEYBOARDVIEW = _T("UKNCBTLKEYBOARD");
const LPCTSTR CLASSNAME_DEBUGVIEW   = _T("UKNCBTLDEBUG");
const LPCTSTR CLASSNAME_DISASMVIEW  = _T("UKNCBTLDISASM");
const LPCTSTR CLASSNAME_MEMORYVIEW  = _T("UKNCBTLMEMORY");
const LPCTSTR CLASSNAME_SPRITEVIEW  = _T("UKNCBTLSPRITE");
const LPCTSTR CLASSNAME_CONSOLEVIEW = _T("UKNCBTLCONSOLE");
const LPCTSTR CLASSNAME_TAPEVIEW    = _T("UKNCBTLTAPE");


//////////////////////////////////////////////////////////////////////
// ScreenView

enum ScreenViewMode
{
    RGBScreen = 1,
    GrayScreen = 2,
    GRBScreen = 3,
};

extern HWND g_hwndScreen;  // Screen View window handle

void ScreenView_RegisterClass();
void ScreenView_Init();
void ScreenView_Done();
BOOL ScreenView_InitRender(LPCTSTR szRenderLibraryName);
void ScreenView_DoneRender();
ScreenViewMode ScreenView_GetMode();
void ScreenView_SetMode(ScreenViewMode);
int ScreenView_GetRenderMode();
void ScreenView_SetRenderMode(int renderMode);
LPCTSTR ScreenView_GetRenderModeDescription(int renderMode);
void ScreenView_PrepareScreen();
void ScreenView_ScanKeyboard();
void ScreenView_RedrawScreen();  // Force to call PrepareScreen and to draw the image
void ScreenView_Create(HWND hwndParent, int x, int y);
LRESULT CALLBACK ScreenViewWndProc(HWND, UINT, WPARAM, LPARAM);
LPCTSTR ScreenView_GetScreenshotModeName(int screenshotMode);
BOOL ScreenView_SaveScreenshot(LPCTSTR sFileName, int screenshotMode);
HGLOBAL ScreenView_GetScreenshotAsDIB(int screenshotMode);
BOOL ScreenView_ScreenToText(uint8_t* buffer);
void ScreenView_KeyEvent(BYTE keyscan, BOOL pressed);


//////////////////////////////////////////////////////////////////////
// KeyboardView

extern HWND g_hwndKeyboard;  // Keyboard View window handle

void KeyboardView_RegisterClass();
void KeyboardView_Create(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK KeyboardViewWndProc(HWND, UINT, WPARAM, LPARAM);


//////////////////////////////////////////////////////////////////////
// DebugView

extern HWND g_hwndDebug;  // Debug View window handle

void DebugView_RegisterClass();
void DebugView_Init();
void DebugView_Create(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK DebugViewWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK DebugViewViewerWndProc(HWND, UINT, WPARAM, LPARAM);
void DebugView_OnUpdate();
void DebugView_SetCurrentProc(BOOL okCPU);
void DebugView_SwitchCpuPpu();


//////////////////////////////////////////////////////////////////////
// DisasmView

extern HWND g_hwndDisasm;  // Disasm View window handle

void DisasmView_RegisterClass();
void DisasmView_Create(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK DisasmViewWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK DisasmViewViewerWndProc(HWND, UINT, WPARAM, LPARAM);
void DisasmView_OnUpdate();
void DisasmView_SetCurrentProc(BOOL okCPU);


//////////////////////////////////////////////////////////////////////
// MemoryView

extern HWND g_hwndMemory;  // Memory view window handler

enum MemoryViewMode
{
    MEMMODE_RAM0 = 0,  // RAM plane 0
    MEMMODE_RAM1 = 1,  // RAM plane 1
    MEMMODE_RAM2 = 2,  // RAM plane 2
    MEMMODE_ROM  = 3,  // ROM
    MEMMODE_CPU  = 4,  // CPU memory
    MEMMODE_PPU  = 5,  // PPU memory
    MEMMODE_LAST = 5,  // Last mode number
};

void MemoryView_RegisterClass();
void MemoryView_Create(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK MemoryViewWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK MemoryViewViewerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void MemoryView_SetViewMode(MemoryViewMode);
void MemoryView_SwitchRamMode();
void MemoryView_SwitchWordByte();
void MemoryView_SelectAddress();


//////////////////////////////////////////////////////////////////////
// SpriteView

extern HWND g_hwndSprite;  // Sprite view window handler

void SpriteView_RegisterClass();
void SpriteView_Create(int x, int y);
LRESULT CALLBACK SpriteViewWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK SpriteViewViewerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


//////////////////////////////////////////////////////////////////////
// ConsoleView

extern HWND g_hwndConsole;  // Console View window handle

void ConsoleView_RegisterClass();
void ConsoleView_Create(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK ConsoleViewWndProc(HWND, UINT, WPARAM, LPARAM);
void ConsoleView_PrintFormat(LPCTSTR pszFormat, ...);
void ConsoleView_Print(LPCTSTR message);
void ConsoleView_Activate();
void ConsoleView_SetCurrentProc(BOOL okCPU);
void ConsoleView_StepInto();
void ConsoleView_StepOver();


//////////////////////////////////////////////////////////////////////
// TapeView

extern HWND g_hwndTape;  // Tape View window handle

void TapeView_RegisterClass();
void TapeView_Create(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK TapeViewWndProc(HWND, UINT, WPARAM, LPARAM);


//////////////////////////////////////////////////////////////////////
