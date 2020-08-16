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

#include "Main.h"
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
HWND m_hwndSplitter = (HWND)INVALID_HANDLE_VALUE;

int m_MainWindowMinCx = UKNC_SCREEN_WIDTH + 16;
int m_MainWindowMinCy = UKNC_SCREEN_HEIGHT + 40;

BOOL m_MainWindow_Fullscreen = FALSE;
LONG m_MainWindow_FullscreenOldStyle = 0;
BOOL m_MainWindow_FullscreenOldMaximized = FALSE;
RECT m_MainWindow_FullscreenOldRect;


//////////////////////////////////////////////////////////////////////
// Forward declarations

BOOL MainWindow_InitToolbar();
BOOL MainWindow_InitStatusbar();
void MainWindow_RestorePositionAndShow();
LRESULT CALLBACK MainWindow_WndProc(HWND, UINT, WPARAM, LPARAM);
void MainWindow_GetMinMaxInfo(MINMAXINFO* mminfo);
void MainWindow_AdjustWindowLayout();
bool MainWindow_DoCommand(int commandId);
void MainWindow_DoViewDebug();
void MainWindow_DoViewToolbar();
void MainWindow_DoViewKeyboard();
void MainWindow_DoViewTape();
void MainWindow_DoViewOnScreenDisplay();
void MainWindow_DoViewFullscreen();
void MainWindow_DoViewScreenMode(ScreenViewMode);
void MainWindow_DoSelectRenderMode(int newMode);
void MainWindow_DoViewSpriteViewer();
void MainWindow_DoEmulatorRun();
void MainWindow_DoEmulatorAutostart();
void MainWindow_DoEmulatorReset();
void MainWindow_DoEmulatorSpeed(WORD speed);
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
void MainWindow_DoFileScreenshotToClipboard();
void MainWindow_DoFileScreenshotSaveAs();
void MainWindow_DoFileScreenToClipboard();
void MainWindow_DoFileCreateDisk();
void MainWindow_DoFileSettings();
void MainWindow_DoFileSettingsColors();
void MainWindow_DoFileSettingsOsd();
void MainWindow_DoEmulatorConfiguration();
void MainWindow_OnStatusbarClick(LPNMMOUSE lpnm);
void MainWindow_OnStatusbarDrawItem(LPDRAWITEMSTRUCT);
void MainWindow_OnToolbarGetInfoTip(LPNMTBGETINFOTIP);


//////////////////////////////////////////////////////////////////////


void MainWindow_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = MainWindow_WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInst;
    wcex.hIcon          = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_UKNCBTL));
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszMenuName   = MAKEINTRESOURCE(IDC_UKNCBTL);
    wcex.lpszClassName  = g_szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    RegisterClassEx(&wcex);

    ToolWindow_RegisterClass();
    OverlappedWindow_RegisterClass();
    SplitterWindow_RegisterClass();

    // Register view classes
    ScreenView_RegisterClass();
    KeyboardView_RegisterClass();
    MemoryView_RegisterClass();
    DebugView_RegisterClass();
    //MemoryMapView_RegisterClass();
    SpriteView_RegisterClass();
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

    DebugView_Init();
    DisasmView_Init();
    ScreenView_Init();

    // Create screen window as a child of the main window
    ScreenView_Create(g_hwnd, 0, 0);

    MainWindow_RestoreSettings();

    MainWindow_ShowHideToolbar();
    MainWindow_ShowHideKeyboard();
    MainWindow_ShowHideTape();
    MainWindow_ShowHideDebug();
    //MainWindow_ShowHideMemoryMap();

    MainWindow_RestorePositionAndShow();

    TCHAR renderDllName[32];
    Settings_GetRender(renderDllName);
    if (!ScreenView_InitRender(renderDllName))
    {
        ::PostQuitMessage(0);
        return FALSE;
    }

    // Restore Render Mode
    int heimode = Settings_GetScreenHeightMode();
    if (heimode < 0 || heimode > 32) heimode = 0;
    ScreenView_SetRenderMode(heimode);

    UpdateWindow(g_hwnd);
    MainWindow_UpdateAllViews();
    MainWindow_UpdateMenu();

    // Autostart
    if (Settings_GetAutostart() || Option_AutoBoot)
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

    TBBUTTON buttons[19];
    ZeroMemory(buttons, sizeof(buttons));
    for (int i = 0; i < sizeof(buttons) / sizeof(TBBUTTON); i++)
    {
        buttons[i].fsState = TBSTATE_ENABLED;
        buttons[i].iString = -1;
    }
    buttons[0].idCommand = ID_EMULATOR_RUN;
    buttons[0].iBitmap = ToolbarImageRun;
    buttons[0].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[0].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("Run"));
    buttons[1].idCommand = ID_EMULATOR_RESET;
    buttons[1].iBitmap = ToolbarImageReset;
    buttons[1].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[1].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("Reset"));
    buttons[2].fsStyle = BTNS_SEP;
    buttons[3].idCommand = ID_EMULATOR_FLOPPY0;
    buttons[3].iBitmap = ToolbarImageFloppySlot;
    buttons[3].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[3].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("0"));
    buttons[4].idCommand = ID_EMULATOR_FLOPPY1;
    buttons[4].iBitmap = ToolbarImageFloppySlot;
    buttons[4].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[4].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("1"));
    buttons[5].idCommand = ID_EMULATOR_FLOPPY2;
    buttons[5].iBitmap = ToolbarImageFloppySlot;
    buttons[5].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[5].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("2"));
    buttons[6].idCommand = ID_EMULATOR_FLOPPY3;
    buttons[6].iBitmap = ToolbarImageFloppySlot;
    buttons[6].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[6].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("3"));
    buttons[7].fsStyle = BTNS_SEP;
    buttons[8].idCommand = ID_EMULATOR_CARTRIDGE1;
    buttons[8].iBitmap = ToolbarImageCartSlot;
    buttons[8].fsStyle = BTNS_BUTTON;
    buttons[9].idCommand = ID_EMULATOR_HARDDRIVE1;
    buttons[9].iBitmap = ToolbarImageHardSlot;
    buttons[9].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[9].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("1"));
    buttons[10].idCommand = ID_EMULATOR_CARTRIDGE2;
    buttons[10].iBitmap = ToolbarImageCartSlot;
    buttons[10].fsStyle = BTNS_BUTTON;
    buttons[11].idCommand = ID_EMULATOR_HARDDRIVE2;
    buttons[11].iBitmap = ToolbarImageHardSlot;
    buttons[11].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[11].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("2"));
    buttons[12].fsStyle = BTNS_SEP;
    buttons[13].idCommand = ID_EMULATOR_SERIAL;
    buttons[13].iBitmap = ToolbarImageSerial;
    buttons[13].fsStyle = BTNS_BUTTON;
    buttons[14].idCommand = ID_EMULATOR_PARALLEL;
    buttons[14].iBitmap = ToolbarImageParallel;
    buttons[14].fsStyle = BTNS_BUTTON;
    buttons[15].idCommand = ID_EMULATOR_NETWORK;
    buttons[15].iBitmap = ToolbarImageNetwork;
    buttons[15].fsStyle = BTNS_BUTTON;
    buttons[16].fsStyle = BTNS_SEP;
    buttons[17].idCommand = ID_EMULATOR_SOUND;
    buttons[17].iBitmap = ToolbarImageSoundOff;
    buttons[17].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[17].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("Sound"));
    buttons[18].idCommand = ID_FILE_SCREENSHOT;
    buttons[18].iBitmap = ToolbarImageScreenshot;
    buttons[18].fsStyle = BTNS_BUTTON;

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

    int statusbarParts[9];
    statusbarParts[0] = 270;
    statusbarParts[1] = 315;
    statusbarParts[2] = 360;
    statusbarParts[3] = statusbarParts[2] + 16 + 16;
    statusbarParts[4] = statusbarParts[3] + 16 + 16;
    statusbarParts[5] = statusbarParts[4] + 16 + 16;
    statusbarParts[6] = statusbarParts[5] + 16 + 16;
    statusbarParts[7] = statusbarParts[6] + 45;
    statusbarParts[8] = -1;
    SendMessage(m_hwndStatusbar, SB_SETPARTS, sizeof(statusbarParts) / sizeof(int), (LPARAM) statusbarParts);
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
        buf[0] = _T('\0');
        Settings_GetFloppyFilePath(slot, buf);
        if (buf[0] != _T('\0'))
        {
            if (! g_pBoard->AttachFloppyImage(slot, buf))
                Settings_SetFloppyFilePath(slot, NULL);
        }
    }

    // Reattach cartridge and HDD images
    for (int slot = 0; slot < 2; slot++)
    {
        buf[0] = _T('\0');
        Settings_GetCartridgeFilePath(slot, buf);
        if (buf[0] != _T('\0'))
        {
            if (!Emulator_LoadROMCartridge(slot, buf))
            {
                Settings_SetCartridgeFilePath(slot, NULL);
                Settings_SetHardFilePath(slot, NULL);
            }

            Settings_GetHardFilePath(slot, buf);
            if (buf[0] != _T('\0'))
            {
                if (!g_pBoard->AttachHardImage(slot, buf))
                {
                    Settings_SetHardFilePath(slot, NULL);
                }
            }
        }
    }

    // Restore ScreenViewMode
    int scrmode = Settings_GetScreenViewMode();
    if (scrmode <= 0 || scrmode > 3) scrmode = GRBScreen;
    ScreenView_SetMode((ScreenViewMode) scrmode);

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

void MainWindow_SavePosition()
{
    if (m_MainWindow_Fullscreen)
    {
        Settings_SetWindowRect(&m_MainWindow_FullscreenOldRect);
        Settings_SetWindowMaximized(m_MainWindow_FullscreenOldMaximized);
    }
    else
    {
        WINDOWPLACEMENT placement;
        placement.length = sizeof(WINDOWPLACEMENT);
        ::GetWindowPlacement(g_hwnd, &placement);

        Settings_SetWindowRect(&(placement.rcNormalPosition));
        Settings_SetWindowMaximized(placement.showCmd == SW_SHOWMAXIMIZED);
    }
    Settings_SetWindowFullscreen(m_MainWindow_Fullscreen);
}
void MainWindow_RestorePositionAndShow()
{
    RECT rc;
    if (Settings_GetWindowRect(&rc))
    {
        HMONITOR hmonitor = MonitorFromRect(&rc, MONITOR_DEFAULTTONULL);
        if (hmonitor != NULL)
        {
            ::SetWindowPos(g_hwnd, NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                    SWP_NOACTIVATE | SWP_NOZORDER);
        }
    }

    ShowWindow(g_hwnd, Settings_GetWindowMaximized() ? SW_SHOWMAXIMIZED : SW_SHOW);

    if (Settings_GetWindowFullscreen())
        MainWindow_DoViewFullscreen();
}

void MainWindow_UpdateWindowTitle()
{
    LPCTSTR emustate = g_okEmulatorRunning ? _T("run") : _T("stop");
    TCHAR buffer[100];
    wsprintf(buffer, _T("%s [%s]"), g_szTitle, emustate);
    SetWindowText(g_hwnd, buffer);
}

// Processes messages for the main window
LRESULT CALLBACK MainWindow_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_ACTIVATE:
        SetFocus(g_hwndScreen);
        break;
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            //int wmEvent = HIWORD(wParam);
            bool okProcessed = MainWindow_DoCommand(wmId);
            if (!okProcessed)
                return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_DESTROY:
        MainWindow_SavePosition();
        ScreenView_DoneRender();
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        MainWindow_AdjustWindowLayout();
        break;
    case WM_GETMINMAXINFO:
        DefWindowProc(hWnd, message, wParam, lParam);
        MainWindow_GetMinMaxInfo((MINMAXINFO*)lParam);
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

void MainWindow_GetMinMaxInfo(MINMAXINFO* mminfo)
{
    if (!m_MainWindow_Fullscreen)
    {
        mminfo->ptMinTrackSize.x = m_MainWindowMinCx;
        mminfo->ptMinTrackSize.y = m_MainWindowMinCy;
    }
}

void MainWindow_AdjustWindowSize()
{
    // If Fullscreen or Maximized then do nothing
    if (m_MainWindow_Fullscreen)
        return;
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
    if (Settings_GetKeyboard())
    {
        RECT rcKeyboard;  GetWindowRect(g_hwndKeyboard, &rcKeyboard);
        cyKeyboard = rcKeyboard.bottom - rcKeyboard.top;
    }
    int cyTape = 0;
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
        cxWidth = cxScreen + cxFrame * 2;
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
    if (m_MainWindow_Fullscreen)
        cyStatus = 0;

    int yScreen = 0;
    int cxScreen = 0, cyScreen = 0;

    int cyToolbar = 0;
    if (Settings_GetToolbar())
    {
        RECT rcToolbar;  GetWindowRect(m_hwndToolbar, &rcToolbar);
        cyToolbar = rcToolbar.bottom - rcToolbar.top;
        yScreen += cyToolbar + 4;
    }

    RECT rc;  GetClientRect(g_hwnd, &rc);

    if (!Settings_GetDebug())  // No debug views -- tape/keyboard snapped to bottom
    {
        cxScreen = rc.right;

        int yTape = rc.bottom - cyStatus + 4;
        int cyTape = 0;
        if (Settings_GetTape())  // Snapped to bottom
        {
            RECT rcTape;  GetWindowRect(g_hwndTape, &rcTape);
            cyTape = rcTape.bottom - rcTape.top;
            yTape = rc.bottom - cyStatus - cyTape - 4;
        }

        int yKeyboard = yTape;
        int cxKeyboard = 0, cyKeyboard = 0;
        if (Settings_GetKeyboard())  // Snapped to bottom
        {
            RECT rcKeyboard;  GetWindowRect(g_hwndKeyboard, &rcKeyboard);
            cxKeyboard = rcKeyboard.right - rcKeyboard.left;
            cyKeyboard = rcKeyboard.bottom - rcKeyboard.top;
            yKeyboard = yTape - cyKeyboard - 4;
        }

        cyScreen = yKeyboard - yScreen - 4;
        if (cyScreen < UKNC_SCREEN_HEIGHT) cyScreen = UKNC_SCREEN_HEIGHT;

        //m_MainWindowMinCx = cxScreen + ::GetSystemMetrics(SM_CXSIZEFRAME) * 2;
        //m_MainWindowMinCy = ::GetSystemMetrics(SM_CYCAPTION) + ::GetSystemMetrics(SM_CYMENU) +
        //    cyScreen + cyStatus + ::GetSystemMetrics(SM_CYSIZEFRAME) * 2;

        if (Settings_GetKeyboard())
        {
            int xKeyboard = (cxScreen - cxKeyboard) / 2;
            SetWindowPos(g_hwndKeyboard, NULL, xKeyboard, yKeyboard, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOCOPYBITS);
        }
        if (Settings_GetTape())
        {
            SetWindowPos(g_hwndTape, NULL, 0, yTape, cxScreen, cyTape, SWP_NOZORDER);
        }
    }
    if (Settings_GetDebug())  // Debug views shown -- keyboard/tape snapped to top
    {
        cxScreen = UKNC_SCREEN_WIDTH;
        cyScreen = UKNC_SCREEN_HEIGHT;

        int yKeyboard = yScreen + cyScreen + 4;
        int yTape = yKeyboard;
        int yConsole = yTape;

        if (Settings_GetKeyboard())
        {
            RECT rcKeyboard;  GetWindowRect(g_hwndKeyboard, &rcKeyboard);
            int cxKeyboard = rcKeyboard.right - rcKeyboard.left;
            int cyKeyboard = rcKeyboard.bottom - rcKeyboard.top;
            int xKeyboard = (cxScreen - cxKeyboard) / 2;
            SetWindowPos(g_hwndKeyboard, NULL, xKeyboard, yKeyboard, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOCOPYBITS);
            yTape += cyKeyboard + 4;
            yConsole += cyKeyboard + 4;
        }
        if (Settings_GetTape())
        {
            RECT rcTape;  GetWindowRect(g_hwndTape, &rcTape);
            int cyTape = rcTape.bottom - rcTape.top;
            SetWindowPos(g_hwndTape, NULL, 0, yTape, cxScreen, cyTape, SWP_NOZORDER);
            yConsole += cyTape + 4;
        }

        int cyConsole = rc.bottom - cyStatus - yConsole - 4;
        SetWindowPos(g_hwndConsole, NULL, 0, yConsole, cxScreen, cyConsole, SWP_NOZORDER);

        RECT rcDebug;  GetWindowRect(g_hwndDebug, &rcDebug);
        int cxDebug = rc.right - cxScreen - 4;
        int cyDebug = rcDebug.bottom - rcDebug.top;
        SetWindowPos(g_hwndDebug, NULL, cxScreen + 4, 0, cxDebug, cyDebug, SWP_NOZORDER);

        RECT rcDisasm;  GetWindowRect(g_hwndDisasm, &rcDisasm);
        int yDebug = cyDebug + 4;
        int cyDisasm = rcDisasm.bottom - rcDisasm.top;
        SetWindowPos(g_hwndDisasm, NULL, cxScreen + 4, yDebug, cxDebug, cyDisasm, SWP_NOZORDER);

        int yMemory = cyDebug + 4 + cyDisasm + 4;
        int cyMemory = rc.bottom - yMemory;
        SetWindowPos(g_hwndMemory, NULL, cxScreen + 4, yMemory, cxDebug, cyMemory, SWP_NOZORDER);

        SetWindowPos(m_hwndSplitter, NULL, cxScreen + 4, yMemory - 4, cxDebug, 4, SWP_NOZORDER);
    }

    SetWindowPos(m_hwndToolbar, NULL, 4, 4, cxScreen, cyToolbar, SWP_NOZORDER);

    SetWindowPos(g_hwndScreen, NULL, 0, yScreen, cxScreen, cyScreen, SWP_NOZORDER /*| SWP_NOCOPYBITS*/);

    int cyStatusReal = rcStatus.bottom - rcStatus.top;
    SetWindowPos(m_hwndStatusbar, NULL, 0, rc.bottom - cyStatusReal, cxScreen, cyStatusReal,
            SWP_NOZORDER | (m_MainWindow_Fullscreen ? SWP_HIDEWINDOW : SWP_SHOWWINDOW));
}

void MainWindow_ShowHideDebug()
{
    if (!Settings_GetDebug())
    {
        // Delete debug views
        if (m_hwndSplitter != INVALID_HANDLE_VALUE)
            DestroyWindow(m_hwndSplitter);
        if (g_hwndConsole != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndConsole);
        if (g_hwndDebug != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndDebug);
        if (g_hwndDisasm != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndDisasm);
        if (g_hwndMemory != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndMemory);

        MainWindow_AdjustWindowSize();
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
        int cyDebugHeight = 216;
        int yDisasmTop = 4 + cyDebugHeight + 4;
        int cyDisasmHeight = 306;
        int yMemoryTop = cyDebugHeight + 4 + cyDisasmHeight + 8;
        int cyMemoryHeight = rc.bottom - cyStatus - yMemoryTop - 4;

        // Create debug views
        if (g_hwndConsole == INVALID_HANDLE_VALUE)
            ConsoleView_Create(g_hwnd, 4, yConsoleTop, cxConsoleWidth, cyConsoleHeight);
        if (g_hwndDebug == INVALID_HANDLE_VALUE)
            DebugView_Create(g_hwnd, xDebugLeft, 4, cxDebugWidth, cyDebugHeight);
        if (g_hwndDisasm == INVALID_HANDLE_VALUE)
            DisasmView_Create(g_hwnd, xDebugLeft, yDisasmTop, cxDebugWidth, cyDisasmHeight);
        if (g_hwndMemory == INVALID_HANDLE_VALUE)
            MemoryView_Create(g_hwnd, xDebugLeft, yMemoryTop, cxDebugWidth, cyMemoryHeight);
        m_hwndSplitter = SplitterWindow_Create(g_hwnd, g_hwndDisasm, g_hwndMemory);
    }

    MainWindow_AdjustWindowLayout();

    MainWindow_UpdateMenu();

    SetFocus(g_hwndScreen);
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
            KeyboardView_Create(g_hwnd, 4, yKeyboardTop, cxKeyboardWidth, cyKeyboardHeight);
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
            TapeView_Create(g_hwnd, 4, yTapeTop, cxTapeWidth, cyTapeHeight);
    }

    MainWindow_AdjustWindowSize();
    MainWindow_AdjustWindowLayout();
    MainWindow_UpdateMenu();
}

void MainWindow_ShowHideSpriteViewer()
{
    if (g_hwndSprite == INVALID_HANDLE_VALUE)
    {
        RECT rcScreen;  ::GetWindowRect(g_hwndScreen, &rcScreen);
        SpriteView_Create(rcScreen.right, rcScreen.top - 4 - ::GetSystemMetrics(SM_CYSMCAPTION));
    }
    else
    {
        ::SetFocus(g_hwndSprite);
    }
}

void MainWindow_UpdateMenu()
{
    // Get main menu
    HMENU hMenu = GetMenu(g_hwnd);

    // Screenshot menu commands
    EnableMenuItem(hMenu, ID_FILE_SCREENSHOT, MF_ENABLED);
    EnableMenuItem(hMenu, ID_FILE_SAVESCREENSHOTAS, MF_ENABLED);
    MainWindow_SetToolbarImage(ID_FILE_SCREENSHOT, ToolbarImageScreenshot);

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

    // Render Mode
    UINT rendermodecmd = ID_VIEW_RENDERMODE + ScreenView_GetRenderMode();
    CheckMenuRadioItem(hMenu, ID_VIEW_RENDERMODE, ID_VIEW_RENDERMODE + 32 - 1, rendermodecmd, MF_BYCOMMAND);

    CheckMenuItem(hMenu, ID_VIEW_ONSCREENDISPLAY, (Settings_GetOnScreenDisplay() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_VIEW_FULLSCREEN, (m_MainWindow_Fullscreen ? MF_CHECKED : MF_UNCHECKED));

    // Emulator menu options
    CheckMenuItem(hMenu, ID_EMULATOR_AUTOSTART, (Settings_GetAutostart() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_SOUND, (Settings_GetSound() ? MF_CHECKED : MF_UNCHECKED));
    MainWindow_SetToolbarImage(ID_EMULATOR_SOUND, (Settings_GetSound() ? ToolbarImageSoundOn : ToolbarImageSoundOff));
    CheckMenuItem(hMenu, ID_EMULATOR_SERIAL, (Settings_GetSerial() ? MF_CHECKED : MF_UNCHECKED));
    SendMessage(m_hwndToolbar, TB_CHECKBUTTON, ID_EMULATOR_SERIAL, (Settings_GetSerial() ? 1 : 0));
    CheckMenuItem(hMenu, ID_EMULATOR_NETWORK, (Settings_GetNetwork() ? MF_CHECKED : MF_UNCHECKED));
    SendMessage(m_hwndToolbar, TB_CHECKBUTTON, ID_EMULATOR_NETWORK, (Settings_GetNetwork() ? 1 : 0));
    CheckMenuItem(hMenu, ID_EMULATOR_PARALLEL, (Settings_GetParallel() ? MF_CHECKED : MF_UNCHECKED));
    SendMessage(m_hwndToolbar, TB_CHECKBUTTON, ID_EMULATOR_PARALLEL, (Settings_GetParallel() ? 1 : 0));

    UINT speedcmd = 0;
    switch (Settings_GetRealSpeed())
    {
    case 0x7ffe: speedcmd = ID_EMULATOR_SPEED25;   break;
    case 0x7fff: speedcmd = ID_EMULATOR_SPEED50;   break;
    case 0:      speedcmd = ID_EMULATOR_SPEEDMAX;  break;
    case 1:      speedcmd = ID_EMULATOR_REALSPEED; break;
    case 2:      speedcmd = ID_EMULATOR_SPEED200;  break;
    }
    CheckMenuRadioItem(hMenu, ID_EMULATOR_SPEED25, ID_EMULATOR_SPEED200, speedcmd, MF_BYCOMMAND);
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

void MainWindow_UpdateRenderModeMenu()
{
    HMENU hMenu = ::GetMenu(g_hwnd);
    HMENU hMenuRender = ::GetSubMenu(hMenu, 2);
    int count = ::GetMenuItemCount(hMenuRender);
    // Delete all items except the first one
    for (int i = 1; i < count; i++)
        ::DeleteMenu(hMenuRender, 1, MF_BYPOSITION);

    for (UINT i = 0; i < 32; i++)
    {
        LPCTSTR cmddesc = ScreenView_GetRenderModeDescription(i);
        UINT cmd = i + ID_VIEW_RENDERMODE;
        if (i == 0)
        {
            if (cmddesc == NULL)
                ::ModifyMenu(hMenuRender, i, MF_BYPOSITION | MF_GRAYED, cmd, _T("No Render Modes"));
            else
                ::ModifyMenu(hMenuRender, i, MF_BYPOSITION, cmd, cmddesc);
        }
        else
        {
            if (cmddesc != NULL)
                ::AppendMenu(hMenuRender, MF_BYPOSITION, (UINT_PTR)cmd, cmddesc);
        }
    }
    ::DrawMenuBar(g_hwnd);

    MainWindow_UpdateMenu();
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
    case ID_VIEW_FULLSCREEN:
        MainWindow_DoViewFullscreen();
        break;
    case ID_VIEW_ONSCREENDISPLAY:
        MainWindow_DoViewOnScreenDisplay();
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
    case ID_DEBUG_SPRITES:
        MainWindow_DoViewSpriteViewer();
        break;
    case ID_DEBUG_MEMORY_ROM:
        MemoryView_SetViewMode(MEMMODE_ROM);
        break;
    case ID_DEBUG_MEMORY_CPU:
        MemoryView_SetViewMode(MEMMODE_CPU);
        break;
    case ID_DEBUG_MEMORY_PPU:
        MemoryView_SetViewMode(MEMMODE_PPU);
        break;
    case ID_DEBUG_MEMORY_RAM:
        MemoryView_SwitchRamMode();
        break;
    case ID_DEBUG_MEMORY_WORDBYTE:
        MemoryView_SwitchWordByte();
        break;
    case ID_DEBUG_MEMORY_GOTO:
        MemoryView_SelectAddress();
        break;
    case ID_EMULATOR_RESET:
        MainWindow_DoEmulatorReset();
        break;
    case ID_EMULATOR_SPEED25:
        MainWindow_DoEmulatorSpeed(0x7ffe);
        break;
    case ID_EMULATOR_SPEED50:
        MainWindow_DoEmulatorSpeed(0x7fff);
        break;
    case ID_EMULATOR_SPEEDMAX:
        MainWindow_DoEmulatorSpeed(0);
        break;
    case ID_EMULATOR_REALSPEED:
        MainWindow_DoEmulatorSpeed(1);
        break;
    case ID_EMULATOR_SPEED200:
        MainWindow_DoEmulatorSpeed(2);
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
    case ID_FILE_SCREENSHOTTOCLIPBOARD:
        MainWindow_DoFileScreenshotToClipboard();
        break;
    case ID_FILE_SAVESCREENSHOTAS:
        MainWindow_DoFileScreenshotSaveAs();
        break;
    case ID_FILE_SCREENTOCLIPBOARD:
        MainWindow_DoFileScreenToClipboard();
        break;
    case ID_FILE_CREATEDISK:
        MainWindow_DoFileCreateDisk();
        break;
    case ID_FILE_SETTINGS:
        MainWindow_DoFileSettings();
        break;
    case ID_FILE_SETTINGS_COLORS:
        MainWindow_DoFileSettingsColors();
        break;
    case ID_FILE_SETTINGS_OSD:
        MainWindow_DoFileSettingsOsd();
        break;
    default:
        if (commandId >= ID_VIEW_RENDERMODE && commandId < ID_VIEW_RENDERMODE + 32)
            MainWindow_DoSelectRenderMode(commandId - ID_VIEW_RENDERMODE);
        return false;
    }
    return true;
}

void MainWindow_DoViewDebug()
{
    MainWindow_DoSelectRenderMode(0);  // Switch to Normal Height mode

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
void MainWindow_DoViewSpriteViewer()
{
    MainWindow_ShowHideSpriteViewer();
}

void MainWindow_DoViewScreenMode(ScreenViewMode newMode)
{
    ScreenView_SetMode(newMode);

    MainWindow_UpdateMenu();

    Settings_SetScreenViewMode(newMode);

    ScreenView_RedrawScreen();
}

void MainWindow_DoSelectRenderMode(int newMode)
{
    if (Settings_GetDebug() && newMode != 0) return;  // Deny switching from Single Height in Debug mode

    ScreenView_SetRenderMode(newMode);

    //MainWindow_AdjustWindowSize();
    //MainWindow_AdjustWindowLayout();
    MainWindow_UpdateMenu();

    Settings_SetScreenHeightMode(newMode);
}

void MainWindow_DoViewOnScreenDisplay()
{
    BOOL onoff = !Settings_GetOnScreenDisplay();
    Settings_SetOnScreenDisplay(onoff);

    if (!g_okEmulatorRunning)
        ScreenView_RedrawScreen();

    MainWindow_UpdateMenu();
}

void MainWindow_DoViewFullscreen()
{
    if (Settings_GetDebug())
        MainWindow_DoViewDebug();  // Leave Debug mode

    if (!m_MainWindow_Fullscreen)
    {
        // Store current window state and position
        m_MainWindow_FullscreenOldMaximized = ::IsZoomed(g_hwnd);
        if (m_MainWindow_FullscreenOldMaximized)
            ::SendMessage(g_hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
        ::GetWindowRect(g_hwnd, &m_MainWindow_FullscreenOldRect);
    }

    m_MainWindow_Fullscreen = !m_MainWindow_Fullscreen;

    if (m_MainWindow_Fullscreen)
    {
        MONITORINFO monitorinfo;
        monitorinfo.cbSize = sizeof(monitorinfo);
        ::GetMonitorInfo(::MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST), &monitorinfo);
        RECT rcnew = monitorinfo.rcMonitor;

        m_MainWindow_FullscreenOldStyle = ::GetWindowLong(g_hwnd, GWL_STYLE);
        ::SetWindowLong(g_hwnd, GWL_STYLE, m_MainWindow_FullscreenOldStyle & ~(WS_CAPTION | WS_THICKFRAME));
        ::SetWindowPos(g_hwnd, NULL, rcnew.left, rcnew.top, rcnew.right - rcnew.left, rcnew.bottom - rcnew.top,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    else
    {
        // Restore saved window position
        RECT rcnew = m_MainWindow_FullscreenOldRect;
        ::SetWindowLong(g_hwnd, GWL_STYLE, m_MainWindow_FullscreenOldStyle);
        ::SetWindowPos(g_hwnd, NULL, rcnew.left, rcnew.top, rcnew.right - rcnew.left, rcnew.bottom - rcnew.top,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        if (m_MainWindow_FullscreenOldMaximized)
            ::SendMessage(g_hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    }

    MainWindow_UpdateMenu();
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
void MainWindow_DoEmulatorSpeed(WORD speed)
{
    Settings_SetRealSpeed(speed);
    Emulator_SetSpeed(speed);

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
    TCHAR bufFileName[MAX_PATH];
    SYSTEMTIME st;
    ::GetSystemTime(&st);
    wsprintf(bufFileName, _T("%04d%02d%02d%02d%02d%02d%03d.png"),
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    int screenshotMode = Settings_GetScreenshotMode();
    if (!ScreenView_SaveScreenshot(bufFileName, screenshotMode))
    {
        AlertWarning(_T("Failed to save screenshot bitmap."));
    }
}

void MainWindow_DoFileScreenshotToClipboard()
{
    int screenshotMode = Settings_GetScreenshotMode();

    HGLOBAL hDIB = ScreenView_GetScreenshotAsDIB(screenshotMode);
    if (hDIB != NULL)
    {
        ::OpenClipboard(0);
        ::EmptyClipboard();
        ::SetClipboardData(CF_DIB, hDIB);
        ::CloseClipboard();
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

    int screenshotMode = Settings_GetScreenshotMode();
    if (!ScreenView_SaveScreenshot(bufFileName, screenshotMode))
    {
        AlertWarning(_T("Failed to save screenshot bitmap."));
    }
}

void MainWindow_DoFileScreenToClipboard()
{
    BYTE buffer[82 * 26 + 1];
    memset(buffer, 0, sizeof(buffer));

    if (!ScreenView_ScreenToText(buffer))
    {
        AlertWarning(_T("Failed to prepare text clipboard from screen."));
        return;
    }

    // Prepare Unicode text
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, sizeof(buffer) * sizeof(TCHAR));
    TCHAR * pUnicodeBuffer = (TCHAR*)GlobalLock(hMem);
    for (int i = 0; i < sizeof(buffer) - 1; i++)
        pUnicodeBuffer[i] = Translate_KOI8R(buffer[i]);
    pUnicodeBuffer[sizeof(buffer) - 1] = 0;
    GlobalUnlock(hMem);

    // Put text to Clipboard
    OpenClipboard(0);
    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();
}

void MainWindow_DoFileCreateDisk()
{
    ShowCreateDiskDialog();
}

void MainWindow_DoFileSettings()
{
    ShowSettingsDialog();
}

void MainWindow_DoFileSettingsColors()
{
    if (ShowSettingsColorsDialog())
    {
        RedrawWindow(g_hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
}

void MainWindow_DoFileSettingsOsd()
{
    if (ShowSettingsOsdDialog())
    {
        RedrawWindow(g_hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
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
    if (g_hwndSprite != NULL)
        InvalidateRect(g_hwndSprite, NULL, TRUE);
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
