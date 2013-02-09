/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// MainWindow.cpp
//

#include "stdafx.h"
#include <commdlg.h>
#include <commctrl.h>

#include "UKNCBTL.h"
#include "Emulator.h"
#include "Dialogs.h"
#include "Views.h"
#include "ToolWindow.h"
#include "util\BitmapFile.h"


//////////////////////////////////////////////////////////////////////


TCHAR g_szTitle[MAX_LOADSTRING];            // The title bar text
TCHAR g_szWindowClass[MAX_LOADSTRING];      // Main window class name

HWND m_hwndToolbar = NULL;
HWND m_hwndStatusbar = NULL;

HANDLE g_hAnimatedScreenshot = INVALID_HANDLE_VALUE;

int m_MainWindowMinCx = UKNC_SCREEN_WIDTH + 16;
int m_MainWindowMinCy = UKNC_SCREEN_HEIGHT + 40;


//////////////////////////////////////////////////////////////////////
// Forward declarations

BOOL MainWindow_InitToolbar();
BOOL MainWindow_InitStatusbar();
LRESULT CALLBACK MainWindow_WndProc(HWND, UINT, WPARAM, LPARAM);
void MainWindow_AdjustWindowLayout();
bool MainWindow_DoCommand(int commandId);
void MainWindow_DoViewDebug();
void MainWindow_DoViewToolbar();
void MainWindow_DoViewKeyboard();
void MainWindow_DoViewTape();
void MainWindow_DoViewScreenMode(ScreenViewMode);
void MainWindow_DoViewHeightMode(int newMode);
void MainWindow_DoEmulatorRun();
void MainWindow_DoEmulatorAutostart();
void MainWindow_DoEmulatorReset();
void MainWindow_DoEmulatorRealSpeed();
void MainWindow_DoEmulatorSound();
void MainWindow_DoEmulatorSerial();
void MainWindow_DoEmulatorParallel();
void MainWindow_DoEmulatorNetwork();
void MainWindow_DoFileSaveState();
void MainWindow_DoFileLoadState();
void MainWindow_DoEmulatorFloppy(int slot);
void MainWindow_DoEmulatorCartridge(int slot);
void MainWindow_DoEmulatorHardDrive(int slot);
void MainWindow_DoFileScreenshot();
void MainWindow_DoFileScreenshotSaveAs();
void MainWindow_DoFileScreenshotAnimated();
void MainWindow_DoFileScreenshotAnimatedStop();
void MainWindow_DoFileCreateDisk();
void MainWindow_DoFileSettings();
void MainWindow_OnStatusbarClick(LPNMMOUSE lpnm);
void MainWindow_OnStatusbarDrawItem(LPDRAWITEMSTRUCT);
void MainWindow_OnToolbarGetInfoTip(LPNMTBGETINFOTIP);


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
    DisasmView_RegisterClass();
    ConsoleView_RegisterClass();
    TapeView_RegisterClass();
}

BOOL CreateMainWindow()
{
    // Create the window    
    g_hwnd = CreateWindow(
            g_szWindowClass, g_szTitle,
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0, 0, 0, 0,
            NULL, NULL, g_hInst, NULL);
    if (!g_hwnd)
        return FALSE;

    // Create and set up the toolbar and the statusbar
    if (!MainWindow_InitToolbar())
        return FALSE;
    if (!MainWindow_InitStatusbar())
        return FALSE;

    ScreenView_Init();

    // Create screen window as a child of the main window
    CreateScreenView(g_hwnd, 4, 4);

    MainWindow_RestoreSettings();

    MainWindow_ShowHideToolbar();
    MainWindow_ShowHideKeyboard();
	MainWindow_ShowHideTape();
    MainWindow_ShowHideDebug();
    MainWindow_AdjustWindowSize();

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    MainWindow_UpdateAllViews();
    MainWindow_UpdateMenu();

    // Autostart
    if (Settings_GetAutostart())
        ::PostMessage(g_hwnd, WM_COMMAND, ID_EMULATOR_RUN, 0);

    return TRUE;
}

BOOL MainWindow_InitToolbar()
{
    m_hwndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, 
        WS_CHILD | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS | CCS_NOPARENTALIGN | CCS_NODIVIDER,
        4, 4, 0, 0, g_hwnd,
        (HMENU) 102,
        g_hInst, NULL); 
    if (! m_hwndToolbar)
        return FALSE;

    SendMessage(m_hwndToolbar, TB_SETEXTENDEDSTYLE, 0, (LPARAM) (DWORD) TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_DRAWDDARROWS);
    SendMessage(m_hwndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM) sizeof(TBBUTTON), 0); 
    SendMessage(m_hwndToolbar, TB_SETBUTTONSIZE, 0, (LPARAM) MAKELONG (26, 26)); 

    TBADDBITMAP addbitmap;
    addbitmap.hInst = g_hInst;
    addbitmap.nID = IDB_TOOLBAR;
    SendMessage(m_hwndToolbar, TB_ADDBITMAP, 2, (LPARAM) &addbitmap);

    TBBUTTON buttons[15];
    ZeroMemory(buttons, sizeof(buttons));
    for (int i = 0; i < sizeof(buttons) / sizeof(TBBUTTON); i++)
    {
        buttons[i].fsState = TBSTATE_ENABLED;
        buttons[i].iString = -1;
    }
    buttons[0].idCommand = ID_EMULATOR_RUN;
    buttons[0].iBitmap = 0;
    buttons[0].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[0].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("Run"));
    buttons[1].idCommand = ID_EMULATOR_RESET;
    buttons[1].iBitmap = 2;
    buttons[1].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[1].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("Reset"));
    buttons[2].fsStyle = BTNS_SEP;
    buttons[3].idCommand = ID_EMULATOR_FLOPPY0;
    buttons[3].iBitmap = 4;
    buttons[3].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[3].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("0"));
    buttons[4].idCommand = ID_EMULATOR_FLOPPY1;
    buttons[4].iBitmap = 4;
    buttons[4].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[4].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("1"));
    buttons[5].idCommand = ID_EMULATOR_FLOPPY2;
    buttons[5].iBitmap = 4;
    buttons[5].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[5].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("2"));
    buttons[6].idCommand = ID_EMULATOR_FLOPPY3;
    buttons[6].iBitmap = 4;
    buttons[6].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[6].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("3"));
    buttons[7].fsStyle = BTNS_SEP;
    buttons[8].idCommand = ID_EMULATOR_CARTRIDGE1;
    buttons[8].iBitmap = 6;
    buttons[8].fsStyle = BTNS_BUTTON;
    buttons[9].idCommand = ID_EMULATOR_HARDDRIVE1;
    buttons[9].iBitmap = ToolbarImageHardSlot;
    buttons[9].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[9].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("1"));
    buttons[10].idCommand = ID_EMULATOR_CARTRIDGE2;
    buttons[10].iBitmap = 6;
    buttons[10].fsStyle = BTNS_BUTTON;
    buttons[11].idCommand = ID_EMULATOR_HARDDRIVE2;
    buttons[11].iBitmap = ToolbarImageHardSlot;
    buttons[11].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[11].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("2"));
    buttons[12].fsStyle = BTNS_SEP;
    buttons[13].idCommand = ID_EMULATOR_SOUND;
    buttons[13].iBitmap = 8;
    buttons[13].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[13].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("Sound"));
    buttons[14].idCommand = ID_FILE_SCREENSHOT;
    buttons[14].iBitmap = ToolbarImageScreenshot;
    buttons[14].fsStyle = BTNS_BUTTON;

    SendMessage(m_hwndToolbar, TB_ADDBUTTONS, (WPARAM) sizeof(buttons) / sizeof(TBBUTTON), (LPARAM) &buttons); 

    if (Settings_GetToolbar())
        ShowWindow(m_hwndToolbar, SW_SHOW);

    return TRUE;
}

BOOL MainWindow_InitStatusbar()
{
    TCHAR welcomeTemplate[100];
    LoadString(g_hInst, IDS_WELCOME, welcomeTemplate, 100);
    TCHAR buffer[100];
    wsprintf(buffer, welcomeTemplate, _T(UKNCBTL_VERSION_STRING));
    m_hwndStatusbar = CreateStatusWindow(
            WS_CHILD | WS_VISIBLE | SBT_TOOLTIPS | CCS_NOPARENTALIGN | CCS_NODIVIDER,
            buffer,
            g_hwnd, 101);
    if (! m_hwndStatusbar)
        return FALSE;

    int statusbarParts[8];
    statusbarParts[0] = 300;
    statusbarParts[1] = 350;
    statusbarParts[2] = statusbarParts[1] + 16 + 16;
    statusbarParts[3] = statusbarParts[2] + 16 + 16;
    statusbarParts[4] = statusbarParts[3] + 16 + 16;
    statusbarParts[5] = statusbarParts[4] + 16 + 16;
    statusbarParts[6] = statusbarParts[5] + 45;
    statusbarParts[7] = -1;
    SendMessage(m_hwndStatusbar, SB_SETPARTS, sizeof(statusbarParts)/sizeof(int), (LPARAM) statusbarParts);
    MainWindow_SetStatusbarBitmap(StatusbarPartMZ0, 0);
    MainWindow_SetStatusbarBitmap(StatusbarPartMZ1, 0);
    MainWindow_SetStatusbarBitmap(StatusbarPartMZ2, 0);
    MainWindow_SetStatusbarBitmap(StatusbarPartMZ3, 0);

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

    // Reattach cartridge and HDD images
    for (int slot = 0; slot < 2; slot++)
    {
        buf[0] = '\0';
        Settings_GetCartridgeFilePath(slot, buf);
        if (lstrlen(buf) > 0)
        {
            if (!Emulator_LoadROMCartridge(slot, buf))
            {
                Settings_SetCartridgeFilePath(slot, NULL);
                Settings_SetHardFilePath(slot, NULL);
            }

            Settings_GetHardFilePath(slot, buf);
            if (lstrlen(buf) > 0)
            {
                if (!g_pBoard->AttachHardImage(slot, buf))
                {
                    Settings_SetHardFilePath(slot, NULL);
                }
            }
        }
    }

    // Restore ScreenViewMode
    int mode = Settings_GetScreenViewMode();
    if (mode <= 0 || mode > 3) mode = RGBScreen;
    ScreenView_SetMode((ScreenViewMode) mode);

    // Restore ScreenHeightMode
    int heimode = Settings_GetScreenHeightMode();
    if (heimode < 1 || heimode > 5) heimode = 1;
    ScreenView_SetHeightMode(heimode);

    // Restore Serial flag
    if (Settings_GetSerial())
    {
        TCHAR portname[10];
        Settings_GetSerialPort(portname);
        if (!Emulator_SetSerial(TRUE, portname))
            Settings_SetSerial(FALSE);
    }

    // Restore Network flag
    if (Settings_GetNetwork())
    {
        TCHAR portname[10];
        Settings_GetNetComPort(portname);
        if (!Emulator_SetNetwork(TRUE, portname))
            Settings_SetNetwork(FALSE);
    }

    // Restore Parallel
    if (Settings_GetParallel())
    {
        Emulator_SetParallel(TRUE);
    }
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
            //int wmEvent = HIWORD(wParam);
            bool okProcessed = MainWindow_DoCommand(wmId);
            if (!okProcessed)
                return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        MainWindow_AdjustWindowLayout();
        break;
    case WM_GETMINMAXINFO:
        {
            DefWindowProc(hWnd, message, wParam, lParam);
            MINMAXINFO* mminfo = (MINMAXINFO*)lParam;
            mminfo->ptMinTrackSize.x = m_MainWindowMinCx;
            mminfo->ptMinTrackSize.y = m_MainWindowMinCy;
        }
        break;
    case WM_NOTIFY:
        {
            //int idCtrl = (int) wParam;
            HWND hwndFrom = ((LPNMHDR) lParam)->hwndFrom;
            UINT code = ((LPNMHDR) lParam)->code;
            if (hwndFrom == m_hwndStatusbar && code == NM_CLICK)
            {
                MainWindow_OnStatusbarClick((LPNMMOUSE) lParam);
            }
            else if (code == TTN_SHOW)
            {
                return 0;
            }
            else if (hwndFrom == m_hwndToolbar && code == TBN_GETINFOTIP)
            {
                MainWindow_OnToolbarGetInfoTip((LPNMTBGETINFOTIP) lParam);
            }
            //else if (hwndFrom == m_hwndToolbar && code == TBN_DROPDOWN)
            //{
            //    MainWindow_OnToolbarDropDown((LPNMTOOLBAR) lParam);
            //}
            else
                return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_DRAWITEM:
        {
            //int idCtrl = (int) wParam;
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
    // If Maximized then do nothing
    WINDOWPLACEMENT placement;
    placement.length = sizeof(WINDOWPLACEMENT);
    ::GetWindowPlacement(g_hwnd, &placement);
    if (placement.showCmd == SW_MAXIMIZE)
        return;

    // Get metrics
    int cxFrame   = ::GetSystemMetrics(SM_CXSIZEFRAME);
    int cyFrame   = ::GetSystemMetrics(SM_CYSIZEFRAME);
    int cyCaption = ::GetSystemMetrics(SM_CYCAPTION);
    int cyMenu    = ::GetSystemMetrics(SM_CYMENU);

    RECT rcToolbar;  GetWindowRect(m_hwndToolbar, &rcToolbar);
    int cyToolbar = rcToolbar.bottom - rcToolbar.top;
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
    int xLeft, yTop;
    int cxWidth, cyHeight;
    if (Settings_GetDebug())
    {
        RECT rcWorkArea;  SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0);
        xLeft = rcWorkArea.left;
        yTop = rcWorkArea.top;
        cxWidth = rcWorkArea.right - rcWorkArea.left;
        cyHeight = rcWorkArea.bottom - rcWorkArea.top;
    }
    else
    {
        RECT rcCurrent;  ::GetWindowRect(g_hwnd, &rcCurrent);
        xLeft = rcCurrent.left;
        yTop = rcCurrent.top;
        cxWidth = cxScreen + cxFrame * 2 + 8;
        cyHeight = cyCaption + cyMenu + 4 + cyScreen + 4 + cyStatus + cyFrame * 2;
        if (Settings_GetToolbar())
            cyHeight += cyToolbar + 4;
        if (Settings_GetKeyboard())
            cyHeight += cyKeyboard + 4;
        if (Settings_GetTape())
            cyHeight += cyTape + 4;
    }
 
    SetWindowPos(g_hwnd, NULL, xLeft, yTop, cxWidth, cyHeight, SWP_NOZORDER | SWP_NOMOVE);
}

void MainWindow_AdjustWindowLayout()
{
    RECT rcStatus;  GetWindowRect(m_hwndStatusbar, &rcStatus);
    int cyStatus = rcStatus.bottom - rcStatus.top;

    RECT rcScreen;  GetWindowRect(g_hwndScreen, &rcScreen);
    int cxScreen = rcScreen.right - rcScreen.left;
    int cyScreen = rcScreen.bottom - rcScreen.top;

    RECT rcToolbar;  GetWindowRect(m_hwndToolbar, &rcToolbar);
    int cyToolbar = rcToolbar.bottom - rcToolbar.top;

    int cxKeyboard = 0, cyKeyboard = 0;
    if (Settings_GetKeyboard())
    {
        RECT rcKeyboard;  GetWindowRect(g_hwndKeyboard, &rcKeyboard);
        cxKeyboard = rcKeyboard.right - rcKeyboard.left;
        cyKeyboard = rcKeyboard.bottom - rcKeyboard.top;
    }
    int cyTape = 0;
    if (Settings_GetTape())
    {
        RECT rcTape;  GetWindowRect(g_hwndTape, &rcTape);
        cyTape = rcTape.bottom - rcTape.top;
    }
    int yScreen = 4;
    int yKeyboard = yScreen + cyScreen + 4;
    int yTape = yScreen + cyScreen + 4;
    int yConsole = yScreen + cyScreen + 4;

    RECT rc;  GetClientRect(g_hwnd, &rc);

    SetWindowPos(m_hwndStatusbar, NULL, 0, rc.bottom - cyStatus, cxScreen + 4, cyStatus, SWP_NOZORDER);

    SetWindowPos(m_hwndToolbar, NULL, 4, 4, cxScreen, cyToolbar, SWP_NOZORDER);

    m_MainWindowMinCx = cxScreen + 8 + ::GetSystemMetrics(SM_CXSIZEFRAME) * 2;
    m_MainWindowMinCy = ::GetSystemMetrics(SM_CYCAPTION) + ::GetSystemMetrics(SM_CYMENU) +
        cyScreen + 8 + cyStatus + ::GetSystemMetrics(SM_CYSIZEFRAME) * 2;

    if (Settings_GetToolbar())
    {
        yScreen   += cyToolbar + 4;
        yKeyboard += cyToolbar + 4;
        yTape     += cyToolbar + 4;
        yConsole  += cyToolbar + 4;
        m_MainWindowMinCy += cyToolbar + 4;
    }

    SetWindowPos(g_hwndScreen, NULL, 4, yScreen, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOCOPYBITS);

    if (Settings_GetKeyboard())
    {
        int xKeyboard = 4 + (cxScreen - cxKeyboard) / 2;
        SetWindowPos(g_hwndKeyboard, NULL, xKeyboard, yKeyboard, 0,0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOCOPYBITS);
        yTape     += cyKeyboard + 4;
        yConsole  += cyKeyboard + 4;
    }
    if (Settings_GetTape())
    {
        RECT rcTape;  GetWindowRect(g_hwndTape, &rcTape);
        int cyTape = rcTape.bottom - rcTape.top;
        SetWindowPos(g_hwndTape, NULL, 4, yTape, cxScreen, cyTape, SWP_NOZORDER);
        yConsole  += cyTape + 4;
    }
    if (Settings_GetDebug())
    {
        int cyConsole = rc.bottom - cyStatus - yConsole - 4;
        SetWindowPos(g_hwndConsole, NULL, 4, yConsole, cxScreen, cyConsole, SWP_NOZORDER);

        RECT rcDebug;  GetWindowRect(g_hwndDebug, &rcDebug);
        int cxDebug = rc.right - cxScreen - 12;
        int cyDebug = rcDebug.bottom - rcDebug.top;
        SetWindowPos(g_hwndDebug, NULL, cxScreen + 8, 4, cxDebug, cyDebug, SWP_NOZORDER);

        RECT rcDisasm;  GetWindowRect(g_hwndDisasm, &rcDisasm);
        int yDebug = 4 + cyDebug + 4;
        int cyDisasm = rcDisasm.bottom - rcDisasm.top;
        SetWindowPos(g_hwndDisasm, NULL, cxScreen + 8, yDebug, cxDebug, cyDisasm, SWP_NOZORDER);

        int yMemory = cyDebug + cyDisasm + 12;
        int cyMemory = rc.bottom - yMemory - 4;
        SetWindowPos(g_hwndMemory, NULL, cxScreen + 8, yMemory, cxDebug, cyMemory, SWP_NOZORDER);
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
        if (g_hwndDisasm != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndDisasm);
        if (g_hwndMemory != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndMemory);

        MainWindow_AdjustWindowSize();
        MainWindow_AdjustWindowLayout();

        SetFocus(g_hwndScreen);
    }
    else  // Debug Views ON
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
        int cyDebugHeight = 212;
        int yDisasmTop = 4 + cyDebugHeight + 4;
        int cyDisasmHeight = 342;
        int yMemoryTop = cyDebugHeight + 4 + cyDisasmHeight + 8;
        int cyMemoryHeight = rc.bottom - cyStatus - yMemoryTop - 4;

        // Create debug views
        if (g_hwndConsole == INVALID_HANDLE_VALUE)
            CreateConsoleView(g_hwnd, 4, yConsoleTop, cxConsoleWidth, cyConsoleHeight);
        if (g_hwndDebug == INVALID_HANDLE_VALUE)
            CreateDebugView(g_hwnd, xDebugLeft, 4, cxDebugWidth, cyDebugHeight);
        if (g_hwndDisasm == INVALID_HANDLE_VALUE)
            CreateDisasmView(g_hwnd, xDebugLeft, yDisasmTop, cxDebugWidth, cyDisasmHeight);
        if (g_hwndMemory == INVALID_HANDLE_VALUE)
            CreateMemoryView(g_hwnd, xDebugLeft, yMemoryTop, cxDebugWidth, cyMemoryHeight);

        MainWindow_AdjustWindowLayout();
    }

    MainWindow_UpdateMenu();
}

void MainWindow_ShowHideToolbar()
{
    ShowWindow(m_hwndToolbar, Settings_GetToolbar() ? SW_SHOW : SW_HIDE);

    MainWindow_AdjustWindowSize();
    MainWindow_AdjustWindowLayout();
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
        int cxKeyboardWidth = 640;
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
        RECT rcScreen;  GetWindowRect(g_hwndScreen, &rcScreen);

        RECT rcPrev;
        if (Settings_GetKeyboard())
            GetWindowRect(g_hwndKeyboard, &rcPrev);
        else
            GetWindowRect(g_hwndScreen, &rcPrev);

        // Calculate children positions
        RECT rc;  GetClientRect(g_hwnd, &rc);
        int yTapeTop = rcPrev.bottom + 4;
        int cxTapeWidth = rcScreen.right - rcScreen.left;
        int cyTapeHeight = 100;

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

    // Screenshot menu commands
    BOOL okAnimatedScreenshot = (g_hAnimatedScreenshot != INVALID_HANDLE_VALUE);
    CheckMenuItem(hMenu, ID_FILE_SCREENSHOTANIMATED, (okAnimatedScreenshot ? MF_CHECKED : MF_UNCHECKED));
    EnableMenuItem(hMenu, ID_FILE_SCREENSHOT, okAnimatedScreenshot ? MF_DISABLED : MF_ENABLED);
    EnableMenuItem(hMenu, ID_FILE_SAVESCREENSHOTAS, okAnimatedScreenshot ? MF_DISABLED : MF_ENABLED);
    MainWindow_SetToolbarImage(ID_FILE_SCREENSHOT, okAnimatedScreenshot ? ToolbarImageScreenshotStop : ToolbarImageScreenshot);

    // Emulator|Run check
    CheckMenuItem(hMenu, ID_EMULATOR_RUN, (g_okEmulatorRunning ? MF_CHECKED : MF_UNCHECKED));
    SendMessage(m_hwndToolbar, TB_CHECKBUTTON, ID_EMULATOR_RUN, (g_okEmulatorRunning ? 1 : 0));

    // View|Debug check
    CheckMenuItem(hMenu, ID_VIEW_TOOLBAR, (Settings_GetToolbar() ? MF_CHECKED : MF_UNCHECKED));
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
    case 3: scrheimodecmd = ID_VIEW_UPSCALED; break;
    case 4: scrheimodecmd = ID_VIEW_UPSCALED3; break;
    case 5: scrheimodecmd = ID_VIEW_UPSCALED4; break;
    }
    CheckMenuRadioItem(hMenu, ID_VIEW_NORMALHEIGHT, ID_VIEW_UPSCALED4, scrheimodecmd, MF_BYCOMMAND);

    // Emulator menu options
    CheckMenuItem(hMenu, ID_EMULATOR_AUTOSTART, (Settings_GetAutostart() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_REALSPEED, (Settings_GetRealSpeed() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_SOUND, (Settings_GetSound() ? MF_CHECKED : MF_UNCHECKED));
    MainWindow_SetToolbarImage(ID_EMULATOR_SOUND, (Settings_GetSound() ? ToolbarImageSoundOn : ToolbarImageSoundOff));
    CheckMenuItem(hMenu, ID_EMULATOR_SERIAL, (Settings_GetSerial() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_NETWORK, (Settings_GetNetwork() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_PARALLEL, (Settings_GetParallel() ? MF_CHECKED : MF_UNCHECKED));

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
    MainWindow_SetToolbarImage(ID_EMULATOR_FLOPPY0,
        g_pBoard->IsFloppyImageAttached(0) ? (g_pBoard->IsFloppyReadOnly(0) ? ToolbarImageFloppyDiskWP : ToolbarImageFloppyDisk) : ToolbarImageFloppySlot);
    MainWindow_SetToolbarImage(ID_EMULATOR_FLOPPY1,
        g_pBoard->IsFloppyImageAttached(1) ? (g_pBoard->IsFloppyReadOnly(1) ? ToolbarImageFloppyDiskWP : ToolbarImageFloppyDisk) : ToolbarImageFloppySlot);
    MainWindow_SetToolbarImage(ID_EMULATOR_FLOPPY2,
        g_pBoard->IsFloppyImageAttached(2) ? (g_pBoard->IsFloppyReadOnly(2) ? ToolbarImageFloppyDiskWP : ToolbarImageFloppyDisk) : ToolbarImageFloppySlot);
    MainWindow_SetToolbarImage(ID_EMULATOR_FLOPPY3,
        g_pBoard->IsFloppyImageAttached(3) ? (g_pBoard->IsFloppyReadOnly(3) ? ToolbarImageFloppyDiskWP : ToolbarImageFloppyDisk) : ToolbarImageFloppySlot);

    // Emulator|CartridgeX
    CheckMenuItem(hMenu, ID_EMULATOR_CARTRIDGE1, (g_pBoard->IsROMCartridgeLoaded(1) ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_CARTRIDGE2, (g_pBoard->IsROMCartridgeLoaded(2) ? MF_CHECKED : MF_UNCHECKED));
    MainWindow_SetToolbarImage(ID_EMULATOR_CARTRIDGE1,
        g_pBoard->IsROMCartridgeLoaded(1) ? ToolbarImageCartridge : ToolbarImageCartSlot);
    MainWindow_SetToolbarImage(ID_EMULATOR_CARTRIDGE2,
        g_pBoard->IsROMCartridgeLoaded(2) ? ToolbarImageCartridge : ToolbarImageCartSlot);
    EnableMenuItem(hMenu, ID_EMULATOR_HARDDRIVE1, g_pBoard->IsROMCartridgeLoaded(1) ? MF_ENABLED : MF_DISABLED);
    EnableMenuItem(hMenu, ID_EMULATOR_HARDDRIVE2, g_pBoard->IsROMCartridgeLoaded(2) ? MF_ENABLED : MF_DISABLED);
    MainWindow_SetToolbarImage(ID_EMULATOR_HARDDRIVE1,
        g_pBoard->IsHardImageAttached(1) ? (g_pBoard->IsHardImageReadOnly(1) ? ToolbarImageHardDriveWP : ToolbarImageHardDrive) : ToolbarImageHardSlot);
    MainWindow_SetToolbarImage(ID_EMULATOR_HARDDRIVE2,
        g_pBoard->IsHardImageAttached(2) ? (g_pBoard->IsHardImageReadOnly(2) ? ToolbarImageHardDriveWP : ToolbarImageHardDrive) : ToolbarImageHardSlot);
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
    case ID_VIEW_TOOLBAR:
        MainWindow_DoViewToolbar();
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
    case ID_VIEW_UPSCALED:
        MainWindow_DoViewHeightMode(3);
        break;
    case ID_VIEW_UPSCALED3:
        MainWindow_DoViewHeightMode(4);
        break;
    case ID_VIEW_UPSCALED4:
        MainWindow_DoViewHeightMode(5);
        break;
    case ID_EMULATOR_RUN:
        MainWindow_DoEmulatorRun();
        break;
    case ID_EMULATOR_AUTOSTART:
        MainWindow_DoEmulatorAutostart();
        break;
    case ID_DEBUG_STEPINTO:
        if (!g_okEmulatorRunning && Settings_GetDebug())
            ConsoleView_StepInto();
        break;
    case ID_DEBUG_STEPOVER:
        if (!g_okEmulatorRunning && Settings_GetDebug())
            ConsoleView_StepOver();
        break;
    case ID_DEBUG_CPUPPU:
        if (!g_okEmulatorRunning && Settings_GetDebug())
            DebugView_SwitchCpuPpu();
        break;
    case ID_EMULATOR_RESET:
        MainWindow_DoEmulatorReset();
        break;
    case ID_EMULATOR_REALSPEED:
        MainWindow_DoEmulatorRealSpeed();
        break;
    case ID_EMULATOR_SOUND:
        MainWindow_DoEmulatorSound();
        break;
    case ID_EMULATOR_SERIAL:
        MainWindow_DoEmulatorSerial();
        break;
    case ID_EMULATOR_NETWORK:
        MainWindow_DoEmulatorNetwork();
        break;
    case ID_EMULATOR_PARALLEL:
        MainWindow_DoEmulatorParallel();
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
    case ID_EMULATOR_HARDDRIVE1:
        MainWindow_DoEmulatorHardDrive(1);
        break;
    case ID_EMULATOR_HARDDRIVE2:
        MainWindow_DoEmulatorHardDrive(2);
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
    case ID_FILE_SAVESCREENSHOTAS:
        MainWindow_DoFileScreenshotSaveAs();
        break;
    case ID_FILE_SCREENSHOTANIMATED:
        MainWindow_DoFileScreenshotAnimated();
        break;
    case ID_FILE_CREATEDISK:
        MainWindow_DoFileCreateDisk();
        break;
    case ID_FILE_SETTINGS:
        MainWindow_DoFileSettings();
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
void MainWindow_DoViewToolbar()
{
    Settings_SetToolbar(!Settings_GetToolbar());
    MainWindow_ShowHideToolbar();
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
    if (Settings_GetDebug() && newMode != 1) return;  // Deny switching from Single Height in Debug mode

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
void MainWindow_DoEmulatorSound()
{
    Settings_SetSound(!Settings_GetSound());

    Emulator_SetSound(Settings_GetSound());

    MainWindow_UpdateMenu();
}

void MainWindow_DoEmulatorSerial()
{
    BOOL okSerial = Settings_GetSerial();
    if (!okSerial)
    {
        TCHAR portname[10];
        Settings_GetSerialPort(portname);
        if (Emulator_SetSerial(TRUE, portname))
            Settings_SetSerial(TRUE);
    }
    else
    {
        Emulator_SetSerial(FALSE, NULL);
        Settings_SetSerial(FALSE);
    }

    MainWindow_UpdateMenu();
}

void MainWindow_DoEmulatorNetwork()
{
    BOOL okNetwork = Settings_GetNetwork();
    if (!okNetwork)
    {
        TCHAR portname[10];
        Settings_GetNetComPort(portname);
        if (Emulator_SetNetwork(TRUE, portname))
            Settings_SetNetwork(TRUE);
    }
    else
    {
        Emulator_SetNetwork(FALSE, NULL);
        Settings_SetNetwork(FALSE);
    }

    MainWindow_UpdateMenu();
}

void MainWindow_DoEmulatorParallel()
{
    BOOL okParallel = Settings_GetParallel();
    if (!okParallel)
    {
        Emulator_SetParallel(TRUE);
        Settings_SetParallel(TRUE);
    }
    else
    {
        Emulator_SetParallel(FALSE);
        Settings_SetParallel(FALSE);
    }

    MainWindow_UpdateMenu();
}

void MainWindow_DoFileLoadState()
{
    TCHAR bufFileName[MAX_PATH];
    BOOL okResult = ShowOpenDialog(g_hwnd,
        _T("Open state image to load"),
        _T("UKNC state images (*.uknc)\0*.uknc\0All Files (*.*)\0*.*\0\0"),
        bufFileName);
    if (! okResult) return;

    if (!Emulator_LoadImage(bufFileName))
    {
        AlertWarning(_T("Failed to load image file."));
    }

    MainWindow_UpdateAllViews();
}

void MainWindow_DoFileSaveState()
{
    TCHAR bufFileName[MAX_PATH];
    BOOL okResult = ShowSaveDialog(g_hwnd,
        _T("Save state image as"),
        _T("UKNC state images (*.uknc)\0*.uknc\0All Files (*.*)\0*.*\0\0"),
        _T("uknc"),
        bufFileName);
    if (! okResult) return;

    if (!Emulator_SaveImage(bufFileName))
    {
        AlertWarning(_T("Failed to save image file."));
    }
}

void MainWindow_DoFileScreenshot()
{
    if (g_hAnimatedScreenshot != INVALID_HANDLE_VALUE)
    {
        MainWindow_DoFileScreenshotAnimatedStop();
        return;
    }

    TCHAR bufFileName[MAX_PATH];
    SYSTEMTIME st;
    ::GetSystemTime(&st);
    wsprintf(bufFileName, _T("%04d%02d%02d%02d%02d%02d%03d.png"),
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    if (!ScreenView_SaveScreenshot(bufFileName))
    {
        AlertWarning(_T("Failed to save screenshot bitmap."));
    }
}

void MainWindow_DoFileScreenshotSaveAs()
{
    TCHAR bufFileName[MAX_PATH];
    BOOL okResult = ShowSaveDialog(g_hwnd,
        _T("Save screenshot as"),
        _T("PNG bitmaps (*.png)\0*.png\0BMP bitmaps (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0\0"),
        _T("png"),
        bufFileName);
    if (! okResult) return;

    if (!ScreenView_SaveScreenshot(bufFileName))
    {
        AlertWarning(_T("Failed to save screenshot bitmap."));
    }
}

void MainWindow_DoFileScreenshotAnimated()
{
    if (g_hAnimatedScreenshot != INVALID_HANDLE_VALUE)
    {
        MainWindow_DoFileScreenshotAnimatedStop();
        return;
    }

    TCHAR bufFileName[MAX_PATH];
    BOOL okResult = ShowSaveDialog(g_hwnd,
        _T("Save screenshot series as"),
        _T("Animated PNGs (*.apng)\0*.apng\0All Files (*.*)\0*.*\0\0"),
        _T("png"),
        bufFileName);
    if (! okResult) return;

    g_hAnimatedScreenshot = ApngFile_Create(bufFileName);

    ScreenView_SaveApngFrame(g_hAnimatedScreenshot);

    MainWindow_UpdateMenu();
}
void MainWindow_DoFileScreenshotAnimatedStop()
{
    ApngFile_Close((HAPNGFILE) g_hAnimatedScreenshot);
    g_hAnimatedScreenshot = INVALID_HANDLE_VALUE;

    MainWindow_UpdateMenu();
}

void MainWindow_DoFileCreateDisk()
{
    ShowCreateDiskDialog();
}

void MainWindow_DoFileSettings()
{
    ShowSettingsDialog();
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
        BOOL okResult = ShowOpenDialog(g_hwnd,
            _T("Open floppy image to load"),
            _T("UKNC floppy images (*.dsk, *.rtd)\0*.dsk;*.rtd\0All Files (*.*)\0*.*\0\0"),
            bufFileName);
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
        BOOL okResult = ShowOpenDialog(g_hwnd,
            _T("Open ROM cartridge image to load"),
            _T("UKNC ROM cartridge images (*.bin)\0*.bin\0All Files (*.*)\0*.*\0\0"),
            bufFileName);
        if (! okResult) return;

        if (!Emulator_LoadROMCartridge(slot, bufFileName))
        {
            AlertWarning(_T("Failed to attach the ROM cartridge image."));
            return;
        }

        Settings_SetCartridgeFilePath(slot, bufFileName);
    }
    MainWindow_UpdateMenu();
}

void MainWindow_DoEmulatorHardDrive(int slot)
{
    BOOL okLoaded = g_pBoard->IsHardImageAttached(slot);
    if (okLoaded)
    {
        g_pBoard->DetachHardImage(slot);
        Settings_SetHardFilePath(slot, NULL);
    }
    else
    {
        // Check if cartridge (HDD ROM image) already selected
        BOOL okCartLoaded = g_pBoard->IsROMCartridgeLoaded(slot);
        if (!okCartLoaded)
        {
            AlertWarning(_T("Please select HDD ROM image as cartridge first."));
            return;
        }

        // Select HDD disk image
        TCHAR bufFileName[MAX_PATH];
        BOOL okResult = ShowOpenDialog(g_hwnd,
            _T("Open HDD image"),
            _T("UKNC HDD images (*.img)\0*.img\0All Files (*.*)\0*.*\0\0"),
            bufFileName);
        if (! okResult) return;

        // Attach HDD disk image
        if (!g_pBoard->AttachHardImage(slot, bufFileName))
        {
            AlertWarning(_T("Failed to attach the HDD image."));
            return;
        }

        Settings_SetHardFilePath(slot, bufFileName);
    }
    MainWindow_UpdateMenu();
}

void MainWindow_OnToolbarGetInfoTip(LPNMTBGETINFOTIP lpnm)
{
    int commandId = lpnm->iItem;

    if (commandId == ID_EMULATOR_FLOPPY0 || commandId == ID_EMULATOR_FLOPPY1 ||
        commandId == ID_EMULATOR_FLOPPY2 || commandId == ID_EMULATOR_FLOPPY3)
    {
        int floppyslot = 0;
        switch (commandId)
        {
        case ID_EMULATOR_FLOPPY0: floppyslot = 0; break;
        case ID_EMULATOR_FLOPPY1: floppyslot = 1; break;
        case ID_EMULATOR_FLOPPY2: floppyslot = 2; break;
        case ID_EMULATOR_FLOPPY3: floppyslot = 3; break;
        }

        if (g_pBoard->IsFloppyImageAttached(floppyslot))
        {
            TCHAR buffilepath[MAX_PATH];
            Settings_GetFloppyFilePath(floppyslot, buffilepath);

            LPCTSTR lpFileName = GetFileNameFromFilePath(buffilepath);
            _tcsncpy_s(lpnm->pszText, 80, lpFileName, _TRUNCATE);
        }
    }
    else if (commandId == ID_EMULATOR_CARTRIDGE1 || commandId == ID_EMULATOR_CARTRIDGE2)
    {
        int cartslot = 0;
        switch (commandId)
        {
        case ID_EMULATOR_CARTRIDGE1: cartslot = 1; break;
        case ID_EMULATOR_CARTRIDGE2: cartslot = 2; break;
        }

        if (g_pBoard->IsROMCartridgeLoaded(cartslot))
        {
            TCHAR buffilepath[MAX_PATH];
            Settings_GetCartridgeFilePath(cartslot, buffilepath);

            LPCTSTR lpFileName = GetFileNameFromFilePath(buffilepath);
            _tcsncpy_s(lpnm->pszText, 80, lpFileName, _TRUNCATE);
        }
    }
    else if (commandId == ID_EMULATOR_HARDDRIVE1 || commandId == ID_EMULATOR_HARDDRIVE2)
    {
        int cartslot = 0;
        switch (commandId)
        {
        case ID_EMULATOR_HARDDRIVE1: cartslot = 1; break;
        case ID_EMULATOR_HARDDRIVE2: cartslot = 2; break;
        }

        if (g_pBoard->IsHardImageAttached(cartslot))
        {
            TCHAR buffilepath[MAX_PATH];
            Settings_GetHardFilePath(cartslot, buffilepath);

            LPCTSTR lpFileName = GetFileNameFromFilePath(buffilepath);
            _tcsncpy_s(lpnm->pszText, 80, lpFileName, _TRUNCATE);
        }
    }
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

    // Draw floppy drive number
    TCHAR text[2];
    text[0] = _T('0') + (TCHAR)(lpDrawItem->itemID - StatusbarPartMZ0);
    text[1] = 0;
    ::DrawStatusText(hdc, &lpDrawItem->rcItem, text, SBT_NOBORDERS);

    // Draw floppy disk icon
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
    DisasmView_OnUpdate();

    // Update screen
    InvalidateRect(g_hwndScreen, NULL, TRUE);

    // Update debug windows
    if (g_hwndDebug != NULL)
        InvalidateRect(g_hwndDebug, NULL, TRUE);
    if (g_hwndDisasm != NULL)
        InvalidateRect(g_hwndDisasm, NULL, TRUE);
    if (g_hwndMemory != NULL)
        InvalidateRect(g_hwndMemory, NULL, TRUE);
}

void MainWindow_SetToolbarImage(int commandId, int imageIndex)
{
    TBBUTTONINFO info;
    info.cbSize = sizeof(info);
    info.iImage = imageIndex;
    info.dwMask = TBIF_IMAGE;
    SendMessage(m_hwndToolbar, TB_SETBUTTONINFO, commandId, (LPARAM) &info);
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
