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
const LPCTSTR CLASSNAME_CONSOLEVIEW = _T("UKNCBTLCONSOLE");
const LPCTSTR CLASSNAME_TAPEVIEW    = _T("UKNCBTLTAPE");


//////////////////////////////////////////////////////////////////////
// ScreenView

enum ScreenViewMode {
    RGBScreen = 1,
    GrayScreen = 2,
    GRBScreen = 3,
};

extern HWND g_hwndScreen;  // Screen View window handle

void ScreenView_RegisterClass();
void ScreenView_Init();
void ScreenView_Done();
ScreenViewMode ScreenView_GetMode();
void ScreenView_SetMode(ScreenViewMode);
int ScreenView_GetHeightMode();
void ScreenView_SetHeightMode(int);
void ScreenView_PrepareScreen();
void ScreenView_ScanKeyboard();
void ScreenView_RedrawScreen();  // Force to call PrepareScreen and to draw the image
void CreateScreenView(HWND hwndParent, int x, int y);
LRESULT CALLBACK ScreenViewWndProc(HWND, UINT, WPARAM, LPARAM);
BOOL ScreenView_SaveScreenshot(LPCTSTR sFileName);
void ScreenView_KeyEvent(BYTE keyscan, BOOL pressed);


//////////////////////////////////////////////////////////////////////
// KeyboardView

extern HWND g_hwndKeyboard;  // Keyboard View window handle

void KeyboardView_RegisterClass();
void CreateKeyboardView(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK KeyboardViewWndProc(HWND, UINT, WPARAM, LPARAM);


//////////////////////////////////////////////////////////////////////
// DebugView

extern HWND g_hwndDebug;  // Debug View window handle

void DebugView_RegisterClass();
void CreateDebugView(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK DebugViewWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK DebugViewViewerWndProc(HWND, UINT, WPARAM, LPARAM);
void DebugView_OnUpdate();
void DebugView_SetCurrentProc(BOOL okCPU);


//////////////////////////////////////////////////////////////////////
// DisasmView

extern HWND g_hwndDisasm;  // Disasm View window handle

void DisasmView_RegisterClass();
void CreateDisasmView(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK DisasmViewWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK DisasmViewViewerWndProc(HWND, UINT, WPARAM, LPARAM);
void DisasmView_OnUpdate();
void DisasmView_SetCurrentProc(BOOL okCPU);


//////////////////////////////////////////////////////////////////////
// MemoryView

extern HWND g_hwndMemory;  // Memory view window handler

void MemoryView_RegisterClass();
void CreateMemoryView(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK MemoryViewWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK MemoryViewViewerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


//////////////////////////////////////////////////////////////////////
// ConsoleView

extern HWND g_hwndConsole;  // Console View window handle

void ConsoleView_RegisterClass();
void CreateConsoleView(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK ConsoleViewWndProc(HWND, UINT, WPARAM, LPARAM);
void ConsoleView_Print(LPCTSTR message);
void ConsoleView_Activate();
void ConsoleView_StepInto();
void ConsoleView_StepOver();


//////////////////////////////////////////////////////////////////////
// TapeView

extern HWND g_hwndTape;  // Tape View window handle

void TapeView_RegisterClass();
void CreateTapeView(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK TapeViewWndProc(HWND, UINT, WPARAM, LPARAM);


//////////////////////////////////////////////////////////////////////
