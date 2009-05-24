// MainWindow.cpp
//

#include "stdafx.h"
#include <commdlg.h>
#include <crtdbg.h>
#include <mmintrin.h>
#include <vfw.h>
#include <commctrl.h>

#include "UKNCBTL.h"
#include "Emulator.h"
#include "Dialogs.h"
#include "Views.h"
#include "ToolWindow.h"


//////////////////////////////////////////////////////////////////////


TCHAR g_szTitle[MAX_LOADSTRING];            // The title bar text
TCHAR g_szWindowClass[MAX_LOADSTRING];      // Main window class name

HWND m_hwndStatusbar = NULL;


//////////////////////////////////////////////////////////////////////
// Forward declarations

LRESULT CALLBACK MainWindow_WndProc(HWND, UINT, WPARAM, LPARAM);
void MainWindow_AdjustWindowLayout();
bool MainWindow_DoCommand(int commandId);
void MainWindow_DoViewDebug();
void MainWindow_DoViewKeyboard();
void MainWindow_DoViewTape();
void MainWindow_DoViewScreenMode(ScreenViewMode);
void MainWindow_DoViewHeightMode(int newMode);
void MainWindow_DoEmulatorRun();
void MainWindow_DoEmulatorAutostart();
void MainWindow_DoEmulatorReset();
void MainWindow_DoEmulatorRealSpeed();
void MainWindow_DoFileSaveState();
void MainWindow_DoFileLoadState();
void MainWindow_DoEmulatorFloppy(int slot);
void MainWindow_DoEmulatorCartridge(int slot);
void MainWindow_DoFileScreenshot();
void MainWindow_OnStatusbarClick(LPNMMOUSE lpnm);
void MainWindow_OnStatusbarDrawItem(LPDRAWITEMSTRUCT);


//////////////////////////////////////////////////////////////////////


void MainWindow_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= MainWindow_WndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= g_hInst;
    wcex.hIcon			= LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_UKNCBTL));
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE+1);
    wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_UKNCBTL);
    wcex.lpszClassName	= g_szWindowClass;
    wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    RegisterClassEx(&wcex);

	ToolWindow_RegisterClass();

    // Register view classes
    ScreenView_RegisterClass();
    KeyboardView_RegisterClass();
    MemoryView_RegisterClass();
    DebugView_RegisterClass();
    ConsoleView_RegisterClass();
	TapeView_RegisterClass();
}

BOOL MainWindow_InitStatusbar()
{
    m_hwndStatusbar = CreateStatusWindow(WS_CHILD | WS_VISIBLE | SBT_TOOLTIPS,
            _T("Welcome to UKNC Back to Life emulator!"),
            g_hwnd, 1);
    if (! m_hwndStatusbar)
        return FALSE;
    int statusbarParts[7];
    statusbarParts[0] = 350;
    statusbarParts[1] = statusbarParts[0] + 70;
    statusbarParts[2] = statusbarParts[1] + 16 + 16;
    statusbarParts[3] = statusbarParts[2] + 16 + 16;
    statusbarParts[4] = statusbarParts[3] + 16 + 16;
    statusbarParts[5] = statusbarParts[4] + 16 + 16;
    statusbarParts[6] = -1;
    SendMessage(m_hwndStatusbar, SB_SETPARTS, sizeof(statusbarParts)/sizeof(int), (LPARAM) statusbarParts);
    MainWindow_SetStatusbarBitmap(StatusbarPartMZ0, 0);
    MainWindow_SetStatusbarBitmap(StatusbarPartMZ1, 0);
    MainWindow_SetStatusbarBitmap(StatusbarPartMZ2, 0);
    MainWindow_SetStatusbarBitmap(StatusbarPartMZ3, 0);
    //MainWindow_SetStatusbarTip(StatusbarPartMZ0, _T("MZ0:"));
    //MainWindow_SetStatusbarTip(StatusbarPartMZ1, _T("MZ1:"));
    //MainWindow_SetStatusbarTip(StatusbarPartMZ2, _T("MZ2:"));
    //MainWindow_SetStatusbarTip(StatusbarPartMZ3, _T("MZ3:"));

    return TRUE;
}

void MainWindow_RestoreSettings()
{
    TCHAR buf[MAX_PATH];

    // Reattach floppy images
    for (int slot = 0; slot < 4; slot++)
    {
        buf[0] = '\0';
        Settings_GetFloppyFilePath(slot, buf);
        if (lstrlen(buf) > 0)
        {
            if (! g_pBoard->AttachFloppyImage(slot, buf))
                Settings_SetFloppyFilePath(slot, NULL);
        }
    }

    // Reattach cartridge images
    for (int slot = 0; slot < 2; slot++)
    {
        buf[0] = '\0';
        Settings_GetCartridgeFilePath(slot, buf);
        if (lstrlen(buf) > 0)
        {
            Emulator_LoadROMCartridge(slot, buf);
            //TODO: If failed to load Then
            //    Settings_SetCartridgeFilePath(slot, NULL);
        }
    }

    // Restore ScreenViewMode
    int mode = Settings_GetScreenViewMode();
    if (mode <= 0 || mode > 3) mode = RGBScreen;
    ScreenView_SetMode((ScreenViewMode) mode);

    // Restore ScreenHeightMode
    int heimode = Settings_GetScreenHeightMode();
    if (heimode < 1) heimode = 1;
    if (heimode > 2) heimode = 2;
    ScreenView_SetHeightMode(heimode);
}

// Processes messages for the main window
LRESULT CALLBACK MainWindow_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_ACTIVATE:
        if (Settings_GetDebug())
            ConsoleView_Activate();
        else
            SetFocus(g_hwndScreen);
        break;
    case WM_COMMAND:
        {
            int wmId    = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);
            bool okProcessed = MainWindow_DoCommand(wmId);
            if (!okProcessed)
                return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    //case WM_KEYDOWN:
    //    switch (wParam)
    //    {
    //    case VK_ESCAPE:
    //        PostMessage(hWnd, WM_COMMAND, ID_VIEW_DEBUG, 0);
    //        break;
    //    default:
    //        return (LRESULT) TRUE;
    //    }
    //    break;
    case WM_NOTIFY:
        {
            int idCtrl = (int) wParam;
            HWND hwndFrom = ((LPNMHDR) lParam)->hwndFrom;
            UINT code = ((LPNMHDR) lParam)->code;
            if (hwndFrom == m_hwndStatusbar)
            {
                switch (code)
                {
                case NM_CLICK: MainWindow_OnStatusbarClick((LPNMMOUSE) lParam); break;
                default:
                    return DefWindowProc(hWnd, message, wParam, lParam);
                }
            }
            else
                return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_DRAWITEM:
        {
            int idCtrl = (int) wParam;
            HWND hwndItem = ((LPDRAWITEMSTRUCT) lParam)->hwndItem;
            if (hwndItem == m_hwndStatusbar)
                MainWindow_OnStatusbarDrawItem((LPDRAWITEMSTRUCT) lParam);
            else
                return DefWindowProc(hWnd, message, wParam, lParam);
        }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void MainWindow_AdjustWindowSize()
{
    // Get metrics
    RECT rcWorkArea;  SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0);
    int cxBorder  = ::GetSystemMetrics(SM_CXBORDER);
    int cyBorder  = ::GetSystemMetrics(SM_CYBORDER);
    int cxFrame   = ::GetSystemMetrics(SM_CXDLGFRAME);
    int cyFrame   = ::GetSystemMetrics(SM_CYDLGFRAME);
    int cyCaption = ::GetSystemMetrics(SM_CYCAPTION);
    int cyMenu    = ::GetSystemMetrics(SM_CYMENU);
    RECT rcScreen;  GetWindowRect(g_hwndScreen, &rcScreen);
    int cxScreen = rcScreen.right - rcScreen.left;
    int cyScreen = rcScreen.bottom - rcScreen.top;
    RECT rcStatus;  GetWindowRect(m_hwndStatusbar, &rcStatus);
    int cyStatus = rcStatus.bottom - rcStatus.top;
    int cyKeyboard = 0;
	int cyTape = 0;

	if (Settings_GetKeyboard())
    {
        RECT rcKeyboard;  GetWindowRect(g_hwndKeyboard, &rcKeyboard);
        cyKeyboard = rcKeyboard.bottom - rcKeyboard.top;
    }
	if (Settings_GetTape())
	{
		RECT rcTape;  GetWindowRect(g_hwndTape, &rcTape);
        cyTape = rcTape.bottom - rcTape.top;
	}

    // Adjust main window size
    int xLeft = rcWorkArea.left;
    int yTop = rcWorkArea.top;
    int cxWidth, cyHeight;
    if (Settings_GetDebug())
    {
        cxWidth = rcWorkArea.right - rcWorkArea.left;
        cyHeight = rcWorkArea.bottom - rcWorkArea.top;
    }
    else
    {
        cxWidth = cxScreen + cxFrame * 2 + 8;
        cyHeight = cyCaption + cyMenu + cyScreen + cyStatus + cyFrame * 2 + 8;
        if (Settings_GetKeyboard())
            cyHeight += cyKeyboard + 4;
		if (Settings_GetTape())
			cyHeight += cyTape + 4;
    }
 
    SetWindowPos(g_hwnd, NULL, xLeft, yTop, cxWidth, cyHeight, SWP_NOZORDER);
}

void MainWindow_AdjustWindowLayout()
{
    RECT rcScreen;  GetWindowRect(g_hwndScreen, &rcScreen);
    int cxScreen = rcScreen.right - rcScreen.left;
    int cyScreen = rcScreen.bottom - rcScreen.top;
    RECT rcStatus;  GetWindowRect(m_hwndStatusbar, &rcStatus);
    int cyStatus = rcStatus.bottom - rcStatus.top;
    int cyKeyboard = 0;
	int cyTape = 0;

	if (Settings_GetKeyboard())
    {
        RECT rcKeyboard;  GetWindowRect(g_hwndKeyboard, &rcKeyboard);
        cyKeyboard = rcKeyboard.bottom - rcKeyboard.top;
    }
	if (Settings_GetTape())
	{
		RECT rcTape;  GetWindowRect(g_hwndTape, &rcTape);
        cyTape = rcTape.bottom - rcTape.top;
	}

    RECT rc;  GetClientRect(g_hwnd, &rc);
    SetWindowPos(m_hwndStatusbar, NULL, 0, rc.bottom - cyStatus, cxScreen, cyStatus, SWP_NOZORDER);
    if (Settings_GetKeyboard())
    {
        SetWindowPos(g_hwndKeyboard, NULL, 4, cyScreen + 8, 0,0, SWP_NOZORDER|SWP_NOSIZE);
    }
    if (Settings_GetTape())
    {
        SetWindowPos(g_hwndTape, NULL, 4, cyScreen + cyKeyboard + 12, 0,0, SWP_NOZORDER|SWP_NOSIZE);
    }
    if (Settings_GetDebug())
    {
        int yConsole = cyScreen + 8;
        if (Settings_GetKeyboard()) yConsole += cyKeyboard + 4;
        if (Settings_GetTape()) yConsole += cyTape + 4;
        int cyConsole = rc.bottom - cyStatus - yConsole - 4;
        SetWindowPos(g_hwndConsole, NULL, 4, yConsole, cxScreen, cyConsole, SWP_NOZORDER);
    }
}

void MainWindow_ShowHideDebug()
{
    if (!Settings_GetDebug())
    {
        // Delete debug views
        if (g_hwndConsole != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndConsole);
        if (g_hwndDebug != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndDebug);
        if (g_hwndMemory != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndMemory);

        MainWindow_AdjustWindowSize();
        MainWindow_AdjustWindowLayout();

        SetFocus(g_hwndScreen);
    }
    else
    {
        MainWindow_AdjustWindowSize();

        // Calculate children positions
        RECT rc;  GetClientRect(g_hwnd, &rc);
        RECT rcScreen;  GetWindowRect(g_hwndScreen, &rcScreen);
        RECT rcStatus;  GetWindowRect(m_hwndStatusbar, &rcStatus);
        int cyStatus = rcStatus.bottom - rcStatus.top;
        int yConsoleTop = rcScreen.bottom - rcScreen.top + 8;
        int cxConsoleWidth = rcScreen.right - rcScreen.left;
        int cyConsoleHeight = rc.bottom - cyStatus - yConsoleTop - 4;
        int xDebugLeft = (rcScreen.right - rcScreen.left) + 8;
        int cxDebugWidth = rc.right - xDebugLeft - 4;
        int cyDebugHeight = 594;
        int yMemoryTop = cyDebugHeight + 8;
        int cyMemoryHeight = rc.bottom - cyStatus - yMemoryTop - 4;

        // Create debug views
        if (g_hwndConsole == INVALID_HANDLE_VALUE)
            CreateConsoleView(g_hwnd, 4, yConsoleTop, cxConsoleWidth, cyConsoleHeight);
        if (g_hwndDebug == INVALID_HANDLE_VALUE)
            CreateDebugView(g_hwnd, xDebugLeft, 4, cxDebugWidth, cyDebugHeight);
        if (g_hwndMemory == INVALID_HANDLE_VALUE)
            CreateMemoryView(g_hwnd, xDebugLeft, yMemoryTop, cxDebugWidth, cyMemoryHeight);

        MainWindow_AdjustWindowLayout();
    }

    MainWindow_UpdateMenu();
}

void MainWindow_ShowHideKeyboard()
{
    if (!Settings_GetKeyboard())
    {
        if (g_hwndKeyboard != INVALID_HANDLE_VALUE)
        {
            ::DestroyWindow(g_hwndKeyboard);
            g_hwndKeyboard = (HWND) INVALID_HANDLE_VALUE;
        }
    }
    else
    {
        // Calculate children positions
        RECT rc;  GetClientRect(g_hwnd, &rc);
        RECT rcScreen;  GetWindowRect(g_hwndScreen, &rcScreen);
        int yKeyboardTop = rcScreen.bottom - rcScreen.top + 8;
        int cxKeyboardWidth = rcScreen.right - rcScreen.left;
        int cyKeyboardHeight = 204;

        if (g_hwndKeyboard == INVALID_HANDLE_VALUE)
            CreateKeyboardView(g_hwnd, 4, yKeyboardTop, cxKeyboardWidth, cyKeyboardHeight);
    }

    MainWindow_AdjustWindowSize();
    MainWindow_AdjustWindowLayout();
    MainWindow_UpdateMenu();
}

void MainWindow_ShowHideTape()
{
    if (!Settings_GetTape())
    {
        if (g_hwndTape != INVALID_HANDLE_VALUE)
        {
            ::DestroyWindow(g_hwndTape);
            g_hwndTape = (HWND) INVALID_HANDLE_VALUE;
        }
    }
    else
    {
		RECT rcPrev;
		if (Settings_GetKeyboard())
			GetWindowRect(g_hwndKeyboard, &rcPrev);
		else
			GetWindowRect(g_hwndScreen, &rcPrev);

        // Calculate children positions
        RECT rc;  GetClientRect(g_hwnd, &rc);
        int yTapeTop = rcPrev.bottom + 4;
        int cxTapeWidth = rcPrev.right - rcPrev.left;
        int cyTapeHeight = 64;

        if (g_hwndTape == INVALID_HANDLE_VALUE)
            CreateTapeView(g_hwnd, 4, yTapeTop, cxTapeWidth, cyTapeHeight);
    }

    MainWindow_AdjustWindowSize();
    MainWindow_AdjustWindowLayout();
    MainWindow_UpdateMenu();
}

void MainWindow_UpdateMenu()
{
    // Get main menu
    HMENU hMenu = GetMenu(g_hwnd);

    // Emulator|Run check
    CheckMenuItem(hMenu, ID_EMULATOR_RUN, (g_okEmulatorRunning ? MF_CHECKED : MF_UNCHECKED));
    // View|Debug check
    CheckMenuItem(hMenu, ID_VIEW_DEBUG, (Settings_GetDebug() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_VIEW_KEYBOARD, (Settings_GetKeyboard() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_VIEW_TAPE, (Settings_GetTape() ? MF_CHECKED : MF_UNCHECKED));
    // View|Color and View|Grayscale radio
    UINT scrmodecmd = 0;
    switch (ScreenView_GetMode())
    {
    case RGBScreen: scrmodecmd = ID_VIEW_RGBSCREEN; break;
    case GRBScreen: scrmodecmd = ID_VIEW_GRBSCREEN; break;
    case GrayScreen: scrmodecmd = ID_VIEW_GRAYSCREEN; break;
    }
    CheckMenuRadioItem(hMenu, ID_VIEW_RGBSCREEN, ID_VIEW_GRAYSCREEN, scrmodecmd, MF_BYCOMMAND);
    // View|Normal Height and View|Double Height radio
    UINT scrheimodecmd = 0;
    switch (ScreenView_GetHeightMode())
    {
    case 1: scrheimodecmd = ID_VIEW_NORMALHEIGHT; break;
    case 2: scrheimodecmd = ID_VIEW_DOUBLEHEIGHT; break;
    }
    CheckMenuRadioItem(hMenu, ID_VIEW_NORMALHEIGHT, ID_VIEW_DOUBLEHEIGHT, scrheimodecmd, MF_BYCOMMAND);

    // Emulator|Autostart
    CheckMenuItem(hMenu, ID_EMULATOR_AUTOSTART, (Settings_GetAutostart() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_REALSPEED, (Settings_GetRealSpeed() ? MF_CHECKED : MF_UNCHECKED));

    // Emulator|FloppyX
    CheckMenuItem(hMenu, ID_EMULATOR_FLOPPY0, (g_pBoard->IsFloppyImageAttached(0) ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_FLOPPY1, (g_pBoard->IsFloppyImageAttached(1) ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_FLOPPY2, (g_pBoard->IsFloppyImageAttached(2) ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_FLOPPY3, (g_pBoard->IsFloppyImageAttached(3) ? MF_CHECKED : MF_UNCHECKED));
    MainWindow_SetStatusbarBitmap(StatusbarPartMZ0,
        g_pBoard->IsFloppyImageAttached(0) ? ((g_pBoard->IsFloppyReadOnly(0)) ? IDI_DISKETTEWP : IDI_DISKETTE) : 0);
    MainWindow_SetStatusbarBitmap(StatusbarPartMZ1,
        g_pBoard->IsFloppyImageAttached(1) ? ((g_pBoard->IsFloppyReadOnly(1)) ? IDI_DISKETTEWP : IDI_DISKETTE) : 0);
    MainWindow_SetStatusbarBitmap(StatusbarPartMZ2,
        g_pBoard->IsFloppyImageAttached(2) ? ((g_pBoard->IsFloppyReadOnly(2)) ? IDI_DISKETTEWP : IDI_DISKETTE) : 0);
    MainWindow_SetStatusbarBitmap(StatusbarPartMZ3,
        g_pBoard->IsFloppyImageAttached(3) ? ((g_pBoard->IsFloppyReadOnly(3)) ? IDI_DISKETTEWP : IDI_DISKETTE) : 0);

    // Emulator|CartridgeX
    CheckMenuItem(hMenu, ID_EMULATOR_CARTRIDGE1, (g_pBoard->IsROMCartridgeLoaded(1) ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_CARTRIDGE2, (g_pBoard->IsROMCartridgeLoaded(2) ? MF_CHECKED : MF_UNCHECKED));
}

// Process menu command
// Returns: true - command was processed, false - command not found
bool MainWindow_DoCommand(int commandId)
{
    switch (commandId)
    {
    case IDM_ABOUT:
        ShowAboutBox();
        break;
    case IDM_EXIT:
        DestroyWindow(g_hwnd);
        break;
    case ID_VIEW_DEBUG:
        MainWindow_DoViewDebug();
        break;
    case ID_VIEW_KEYBOARD:
        MainWindow_DoViewKeyboard();
        break;
    case ID_VIEW_TAPE:
        MainWindow_DoViewTape();
        break;
    case ID_VIEW_RGBSCREEN:
        MainWindow_DoViewScreenMode(RGBScreen);
        break;
    case ID_VIEW_GRBSCREEN:
        MainWindow_DoViewScreenMode(GRBScreen);
        break;
    case ID_VIEW_GRAYSCREEN:
        MainWindow_DoViewScreenMode(GrayScreen);
        break;
    case ID_VIEW_NORMALHEIGHT:
        MainWindow_DoViewHeightMode(1);
        break;
    case ID_VIEW_DOUBLEHEIGHT:
        MainWindow_DoViewHeightMode(2);
        break;
    case ID_EMULATOR_RUN:
        MainWindow_DoEmulatorRun();
        break;
    case ID_EMULATOR_AUTOSTART:
        MainWindow_DoEmulatorAutostart();
        break;
    case ID_EMULATOR_STEP:
        if (!g_okEmulatorRunning && Settings_GetDebug())
            ConsoleView_Step();
        break;
    case ID_EMULATOR_RESET:
        MainWindow_DoEmulatorReset();
        break;
    case ID_EMULATOR_REALSPEED:
        MainWindow_DoEmulatorRealSpeed();
        break;
    case ID_EMULATOR_FLOPPY0:
        MainWindow_DoEmulatorFloppy(0);
        break;
    case ID_EMULATOR_FLOPPY1:
        MainWindow_DoEmulatorFloppy(1);
        break;
    case ID_EMULATOR_FLOPPY2:
        MainWindow_DoEmulatorFloppy(2);
        break;
    case ID_EMULATOR_FLOPPY3:
        MainWindow_DoEmulatorFloppy(3);
        break;
    case ID_EMULATOR_CARTRIDGE1:
        MainWindow_DoEmulatorCartridge(1);
        break;
    case ID_EMULATOR_CARTRIDGE2:
        MainWindow_DoEmulatorCartridge(2);
        break;
    case ID_FILE_LOADSTATE:
        MainWindow_DoFileLoadState();
        break;
    case ID_FILE_SAVESTATE:
        MainWindow_DoFileSaveState();
        break;
    case ID_FILE_SCREENSHOT:
        MainWindow_DoFileScreenshot();
        break;
    default:
        return false;
    }
    return true;
}

void MainWindow_DoViewDebug()
{
    MainWindow_DoViewHeightMode(1);  // Switch to Normal Height mode

    Settings_SetDebug(!Settings_GetDebug());
    MainWindow_ShowHideDebug();
}
void MainWindow_DoViewKeyboard()
{
    Settings_SetKeyboard(!Settings_GetKeyboard());
    MainWindow_ShowHideKeyboard();
}
void MainWindow_DoViewTape()
{
    Settings_SetTape(!Settings_GetTape());
    MainWindow_ShowHideTape();
}

void MainWindow_DoViewScreenMode(ScreenViewMode newMode)
{
    ScreenView_SetMode(newMode);

    MainWindow_UpdateMenu();

    Settings_SetScreenViewMode(newMode);
}

void MainWindow_DoViewHeightMode(int newMode)
{
    if (Settings_GetDebug() && newMode == 2) return;  // Deny switching to Double Height in Debug mode

    ScreenView_SetHeightMode(newMode);

    MainWindow_AdjustWindowSize();
    MainWindow_AdjustWindowLayout();
    MainWindow_UpdateMenu();

    Settings_SetScreenHeightMode(newMode);
}

void MainWindow_DoEmulatorRun()
{
    if (g_okEmulatorRunning)
    {
        Emulator_Stop();
    }
    else
    {
        Emulator_Start();
    }
}
void MainWindow_DoEmulatorAutostart()
{
    Settings_SetAutostart(!Settings_GetAutostart());

    MainWindow_UpdateMenu();
}
void MainWindow_DoEmulatorReset()
{
    Emulator_Reset();
}
void MainWindow_DoEmulatorRealSpeed()
{
    Settings_SetRealSpeed(!Settings_GetRealSpeed());

    MainWindow_UpdateMenu();
}

void MainWindow_DoFileLoadState()
{
    // File Open dialog
    TCHAR bufFileName[MAX_PATH];
    *bufFileName = 0;
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.hInstance = g_hInst;
    ofn.lpstrTitle = _T("Open state image to load");
    ofn.lpstrFilter = _T("UKNC state images (*.uknc)\0*.uknc\0All Files (*.*)\0*.*\0\0");
    ofn.Flags = OFN_FILEMUSTEXIST;
    ofn.lpstrFile = bufFileName;
    ofn.nMaxFile = sizeof(bufFileName) / sizeof(TCHAR);

    BOOL okResult = GetOpenFileName(&ofn);
    if (! okResult) return;

    Emulator_LoadImage(bufFileName);
}

void MainWindow_DoFileSaveState()
{
    // File Save As dialog
    TCHAR bufFileName[MAX_PATH];
    *bufFileName = 0;
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.hInstance = g_hInst;
    ofn.lpstrTitle = _T("Save state image as");
    ofn.lpstrFilter = _T("UKNC state images (*.uknc)\0*.uknc\0All Files (*.*)\0*.*\0\0");
    ofn.Flags = OFN_FILEMUSTEXIST;
    ofn.lpstrFile = bufFileName;
    ofn.nMaxFile = sizeof(bufFileName) / sizeof(TCHAR);

    BOOL okResult = GetSaveFileName(&ofn);
    if (! okResult) return;

    Emulator_SaveImage(bufFileName);
}

void MainWindow_DoFileScreenshot()
{
    // File Save As dialog
    TCHAR bufFileName[MAX_PATH];
    *bufFileName = 0;
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.hInstance = g_hInst;
    ofn.lpstrTitle = _T("Save screenshot as");
    ofn.lpstrFilter = _T("Bitmaps (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0\0");
    ofn.Flags = OFN_FILEMUSTEXIST;
    ofn.lpstrFile = bufFileName;
    ofn.nMaxFile = sizeof(bufFileName) / sizeof(TCHAR);

    BOOL okResult = GetSaveFileName(&ofn);
    if (! okResult) return;

    ScreenView_SaveScreenshot(bufFileName);
}

void MainWindow_DoEmulatorFloppy(int slot)
{
    BOOL okImageAttached = g_pBoard->IsFloppyImageAttached(slot);
    if (okImageAttached)
    {
        g_pBoard->DetachFloppyImage(slot);
        Settings_SetFloppyFilePath(slot, NULL);
    }
    else
    {
        // File Open dialog
        TCHAR bufFileName[MAX_PATH];
        *bufFileName = 0;
        OPENFILENAME ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_hwnd;
        ofn.hInstance = g_hInst;
        ofn.lpstrTitle = _T("Open floppy image to load");
        ofn.lpstrFilter = _T("UKNC floppy images (*.dsk, *.rtd)\0*.dsk;*.rtd\0All Files (*.*)\0*.*\0\0");
        ofn.Flags = OFN_FILEMUSTEXIST;
        ofn.lpstrFile = bufFileName;
        ofn.nMaxFile = sizeof(bufFileName) / sizeof(TCHAR);

        BOOL okResult = GetOpenFileName(&ofn);
        if (! okResult) return;

        if (! g_pBoard->AttachFloppyImage(slot, bufFileName))
        {
            AlertWarning(_T("Failed to attach floppy image."));
            return;
        }
        Settings_SetFloppyFilePath(slot, bufFileName);
    }
    MainWindow_UpdateMenu();
}

void MainWindow_DoEmulatorCartridge(int slot)
{
    BOOL okLoaded = g_pBoard->IsROMCartridgeLoaded(slot);
    if (okLoaded)
    {
        g_pBoard->UnloadROMCartridge(slot);
        Settings_SetCartridgeFilePath(slot, NULL);
    }
    else
    {
        // File Open dialog
        TCHAR bufFileName[MAX_PATH];
        *bufFileName = 0;
        OPENFILENAME ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_hwnd;
        ofn.hInstance = g_hInst;
        ofn.lpstrTitle = _T("Open ROM cartridge image to load");
        ofn.lpstrFilter = _T("UKNC ROM cartridge images (*.bin)\0*.bin\0All Files (*.*)\0*.*\0\0");
        ofn.Flags = OFN_FILEMUSTEXIST;
        ofn.lpstrFile = bufFileName;
        ofn.nMaxFile = sizeof(bufFileName) / sizeof(TCHAR);

        BOOL okResult = GetOpenFileName(&ofn);
        if (! okResult) return;

        Emulator_LoadROMCartridge(slot, bufFileName);

        Settings_SetCartridgeFilePath(slot, bufFileName);
    }
    MainWindow_UpdateMenu();
}

void MainWindow_OnStatusbarClick(LPNMMOUSE lpnm)
{
    int nSection = (int) (lpnm->dwItemSpec);
    if (nSection >= StatusbarPartMZ0 && nSection <= StatusbarPartMZ3)
    {
        UINT nCmd = 0;
        switch (nSection)
        {
        case StatusbarPartMZ0: nCmd = ID_EMULATOR_FLOPPY0; break;
        case StatusbarPartMZ1: nCmd = ID_EMULATOR_FLOPPY1; break;
        case StatusbarPartMZ2: nCmd = ID_EMULATOR_FLOPPY2; break;
        case StatusbarPartMZ3: nCmd = ID_EMULATOR_FLOPPY3; break;
        }
        ::PostMessage(g_hwnd, WM_COMMAND, (WPARAM) nCmd, 0);
    }
}

void MainWindow_OnStatusbarDrawItem(LPDRAWITEMSTRUCT lpDrawItem)
{
    HDC hdc = lpDrawItem->hDC;

    // Номер привода дисковода
    TCHAR text[2];
    text[0] = _T('0') + lpDrawItem->itemID - StatusbarPartMZ0;
    text[1] = 0;
    ::DrawStatusText(hdc, &lpDrawItem->rcItem, text, SBT_NOBORDERS);

    // Иконка диска
    UINT resourceId = (UINT) lpDrawItem->itemData;
    if (resourceId != 0)
    {
        HICON hicon = ::LoadIcon(g_hInst, MAKEINTRESOURCE(resourceId));
        int left = lpDrawItem->rcItem.right - 16 - 2; // lpDrawItem->rcItem.left + (lpDrawItem->rcItem.right - lpDrawItem->rcItem.left - 16) / 2;
        int top = lpDrawItem->rcItem.top + (lpDrawItem->rcItem.bottom - lpDrawItem->rcItem.top - 16) / 2;
        ::DrawIconEx(hdc, left, top, hicon, 16, 16, 0, NULL, DI_NORMAL);
        ::DeleteObject(hicon);
    }
}

void MainWindow_UpdateAllViews()
{
    // Update cached values in views
    Emulator_OnUpdate();
    DebugView_OnUpdate();

    // Update screen
    InvalidateRect(g_hwndScreen, NULL, TRUE);

    // Update debug windows
    if (g_hwndDebug != NULL)
        InvalidateRect(g_hwndDebug, NULL, TRUE);
    if (g_hwndMemory != NULL)
        InvalidateRect(g_hwndMemory, NULL, TRUE);
}

void MainWindow_SetStatusbarText(int part, LPCTSTR message)
{
    SendMessage(m_hwndStatusbar, SB_SETTEXT, part, (LPARAM) message);
}
void MainWindow_SetStatusbarBitmap(int part, UINT resourceId)
{
    SendMessage(m_hwndStatusbar, SB_SETTEXT, part | SBT_OWNERDRAW, (LPARAM) resourceId);
}
void MainWindow_SetStatusbarIcon(int part, HICON hIcon)
{
    SendMessage(m_hwndStatusbar, SB_SETICON, part, (LPARAM) hIcon);
}


//////////////////////////////////////////////////////////////////////
