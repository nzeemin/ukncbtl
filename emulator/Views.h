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

#define CLASSNAMEPREFIX _T("UKNCBTL")

const LPCTSTR CLASSNAME_SCREENVIEW      = CLASSNAMEPREFIX _T("SCREEN");
const LPCTSTR CLASSNAME_KEYBOARDVIEW    = CLASSNAMEPREFIX _T("KEYBOARD");
const LPCTSTR CLASSNAME_DEBUGVIEW       = CLASSNAMEPREFIX _T("DEBUG");
const LPCTSTR CLASSNAME_DISASMVIEW      = CLASSNAMEPREFIX _T("DISASM");
const LPCTSTR CLASSNAME_MEMORYVIEW      = CLASSNAMEPREFIX _T("MEMORY");
const LPCTSTR CLASSNAME_MEMORYMAPVIEW   = CLASSNAMEPREFIX _T("MEMORYMAP");
const LPCTSTR CLASSNAME_SPRITEVIEW      = CLASSNAMEPREFIX _T("SPRITE");
const LPCTSTR CLASSNAME_CONSOLEVIEW     = CLASSNAMEPREFIX _T("CONSOLE");
const LPCTSTR CLASSNAME_TAPEVIEW        = CLASSNAMEPREFIX _T("TAPE");


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
void ScreenView_UpdateMouse();
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
void KeyboardView_KeyEvent(BYTE keyscan, BOOL pressed);


//////////////////////////////////////////////////////////////////////
// DebugView

extern HWND g_hwndDebug;  // Debug View window handle

void DebugView_RegisterClass();
void DebugView_Init();
void DebugView_Create(HWND hwndParent, int x, int y, int width, int height);
void DebugView_Redraw();
LRESULT CALLBACK DebugViewWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK DebugViewViewerWndProc(HWND, UINT, WPARAM, LPARAM);
void DebugView_OnUpdate();
void DebugView_SetCurrentProc(BOOL okCPU);
void DebugView_SwitchCpuPpu();


//////////////////////////////////////////////////////////////////////
// DisasmView

extern HWND g_hwndDisasm;  // Disasm View window handle

void DisasmView_RegisterClass();
void DisasmView_Init();
void DisasmView_Done();
void DisasmView_Create(HWND hwndParent, int x, int y, int width, int height);
void DisasmView_Redraw();
LRESULT CALLBACK DisasmViewWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK DisasmViewViewerWndProc(HWND, UINT, WPARAM, LPARAM);
void DisasmView_OnUpdate();
void DisasmView_SetCurrentProc(BOOL okCPU);
void DisasmView_LoadUnloadSubtitles();


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

enum MemoryViewNumeralMode
{
    MEMMODENUM_OCT = 0,
    MEMMODENUM_HEX = 1,
};

void MemoryView_RegisterClass();
void MemoryView_Create(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK MemoryViewWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK MemoryViewViewerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void MemoryView_SetViewMode(MemoryViewMode);
void MemoryView_SwitchRamMode();
void MemoryView_SwitchWordByte();
void MemoryView_SwitchNumeralMode();
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
void ConsoleView_ClearConsole();
void ConsoleView_DeleteAllBreakpoints();


//////////////////////////////////////////////////////////////////////
// TapeView

extern HWND g_hwndTape;  // Tape View window handle

void TapeView_RegisterClass();
void TapeView_Create(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK TapeViewWndProc(HWND, UINT, WPARAM, LPARAM);


//////////////////////////////////////////////////////////////////////
