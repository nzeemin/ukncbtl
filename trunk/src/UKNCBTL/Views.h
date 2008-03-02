// Views.h
// Defines for all views of the application

#pragma once

//////////////////////////////////////////////////////////////////////


const LPCTSTR CLASSNAME_SCREENVIEW  = _T("UKNCBTLSCREEN");
const LPCTSTR CLASSNAME_KEYBOARDVIEW = _T("UKNCBTLKEYBOARD");
const LPCTSTR CLASSNAME_DEBUGVIEW   = _T("UKNCBTLDEBUG");
const LPCTSTR CLASSNAME_MEMORYVIEW  = _T("UKNCBTLMEMORY");
const LPCTSTR CLASSNAME_CONSOLEVIEW = _T("UKNCBTLCONSOLE");


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
void ScreenView_SaveScreenshot(LPCTSTR sFileName);
void ScreenView_KeyEvent(BYTE keyscan, BOOL pressed);


//////////////////////////////////////////////////////////////////////
// KeyboardView

extern HWND g_hwndKeyboard;  // Keyboard View window handle

void KeyboardView_RegisterClass();
void KeyboardView_Init();
void KeyboardView_Done();
void CreateKeyboardView(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK KeyboardViewWndProc(HWND, UINT, WPARAM, LPARAM);


//////////////////////////////////////////////////////////////////////
// DebugView

extern HWND g_hwndDebug;  // Debug View window handle

void DebugView_RegisterClass();
void CreateDebugView(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK DebugViewWndProc(HWND, UINT, WPARAM, LPARAM);
void DebugView_OnUpdate();
void DebugView_SetCurrentProc(BOOL okCPU);


//////////////////////////////////////////////////////////////////////
// MemoryView

extern HWND g_hwndMemory;  // Memory view window handler

void MemoryView_RegisterClass();
void CreateMemoryView(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK MemoryViewWndProc(HWND, UINT, WPARAM, LPARAM);


//////////////////////////////////////////////////////////////////////
// ConsoleView

extern HWND g_hwndConsole;  // Console View window handle

void ConsoleView_RegisterClass();
void CreateConsoleView(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK ConsoleViewWndProc(HWND, UINT, WPARAM, LPARAM);
void ConsoleView_Print(LPCTSTR message);
void ConsoleView_Activate();
void ConsoleView_Step();


//////////////////////////////////////////////////////////////////////
