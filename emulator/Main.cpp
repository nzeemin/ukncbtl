/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// UKNCBTL.cpp : Defines the entry point for the application.

#include "stdafx.h"
#include "util/Profiler.h"
#include <crtdbg.h>
#include <CommCtrl.h>
#include <shellapi.h>
#include <timeapi.h>

#include "Main.h"
#include "CommandLineOptions.h"
#include <vector>
#include "Emulator.h"
#include "Views.h"
#include "util/BitmapFile.h"

//////////////////////////////////////////////////////////////////////
// Global Variables

HINSTANCE g_hInst = nullptr; // current instance
HWND g_hwnd = nullptr;
long m_nMainLastFrameTicks = 0;


//////////////////////////////////////////////////////////////////////
// Forward declarations

BOOL InitInstance(HINSTANCE, int);
void DoneInstance();
void ParseCommandLine();

LPCTSTR g_CommandLineHelp =
    _T("Usage: UKNCBTL [options]\r\n\r\n")
    _T("Command line options:\r\n\r\n")
    _T("/h /help\r\n\tShow command line options (this box)\r\n")
    _T("/boot\r\n\tAuto-start the emulation, select boot from disk\r\n")
    _T("/bootN\r\n\tAuto-start the emulation, select boot menu item N=1..7\r\n")
    _T("/autostart /autostarton\r\n\tStart emulation on window open\r\n")
    _T("/noautostart /autostartoff\r\n\tDo not start emulation on window open\r\n")
    _T("/debug /debugon /debugger\r\n\tSwitch to debug mode\r\n")
    _T("/nodebug /debugoff\r\n\tSwitch off the debug mode\r\n")
    _T("/sound /soundon\r\n\tTurn sound on\r\n")
    _T("/nosound /soundoff\r\n\tTurn sound off\r\n")
    _T("/fullscreen /fullscreenon\r\n\tSwitch to fullscreen mode\r\n")
    _T("/windowed /fullscreenoff\r\n\tSwitch to windowed mode\r\n")
    _T("/diskN:filePath\r\n\tAttach disk image, N=0..3\r\n")
    _T("/cartN:filePath\r\n\tAttach cartridge image, N=1..2\r\n")
    _T("/hardN:filePath\r\n\tAttach hard disk image, N=1..2\r\n");


//////////////////////////////////////////////////////////////////////


int APIENTRY _tWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPTSTR    lpCmdLine,
    int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF);
    int n = 0;
    _CrtSetBreakAlloc(n);
#endif

    g_hInst = hInstance; // Store instance handle in our global variable

    LARGE_INTEGER nFrameStartTime;
    nFrameStartTime.QuadPart = 0;

    // Initialize global strings
    LoadString(g_hInst, IDS_APP_TITLE, g_szTitle, MAX_LOADSTRING);
    LoadString(g_hInst, IDC_APPLICATION, g_szWindowClass, MAX_LOADSTRING);
    MainWindow_RegisterClass();

    // Perform application initialization
    if (! InitInstance(hInstance, nCmdShow))
        return FALSE;

    HACCEL hAccelTable = ::LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_APPLICATION));

    if (Option_ShowHelp)
        ::PostMessage(g_hwnd, WM_COMMAND, ID_HELP_COMMAND_LINE_HELP, 0);

    LARGE_INTEGER nPerformanceFrequency;
    ::QueryPerformanceFrequency(&nPerformanceFrequency);

    TIMECAPS caps;
    VERIFY(!::timeGetDevCaps(&caps, sizeof(caps)));
    VERIFY(!::timeBeginPeriod(caps.wPeriodMin));

    // Main message loop
    MSG msg;
    for (;;)
    {
        UKNC_PROFILE_FRAME();
        ::QueryPerformanceCounter(&nFrameStartTime);

        if (!g_okEmulatorRunning)
            ::Sleep(1);
        else
        {
            if (!Emulator_SystemFrame())  // Breakpoint hit
            {
                Emulator_Stop();
                // Turn on debugger if not yet
                if (!Settings_GetDebug())
                    ::PostMessage(g_hwnd, WM_COMMAND, ID_VIEW_DEBUG, 0);
                else
                    ::FlashWindow(g_hwnd, TRUE);
            }

            ScreenView_RedrawScreen();
        }

        // Process all queue
        while (::PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ))
        {
            if (msg.message == WM_QUIT)
                goto endprog;

            if (::TranslateAccelerator(g_hwnd, hAccelTable, &msg))
                continue;

            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }

        if (g_okEmulatorRunning && !Settings_GetSound())
        {
            WORD speed = Settings_GetRealSpeed();
            if (speed == 0)
                ;  // Speed MAX, consume 100% of one CPU core
            else
            {
                UKNC_PROFILE_SCOPE("Frame pacing wait");
                LONGLONG nFrameDelay;
                switch (speed)
                {
                case 0x7ffd: nFrameDelay = 1000ll / FRAMERATE * 10; break;  // Speed 10%
                case 0x7ffe: nFrameDelay = 1000ll / FRAMERATE * 4;  break;  // Speed 25%
                case 0x7fff: nFrameDelay = 1000ll / FRAMERATE * 2;  break;  // Speed 50%
                case 2:      nFrameDelay = 1000ll / FRAMERATE / 2;  break;  // Speed 200%
                case 3:      nFrameDelay = 1000ll / FRAMERATE / 4;  break;  // Speed 400%
                default:  // Speed 100%
                    nFrameDelay = 1000ll / FRAMERATE;  // 1000 millisec / 50 = 20 millisec
                    break;
                }

                for (;;)
                {
                    LARGE_INTEGER nFrameFinishTime;  // Frame start time
                    ::QueryPerformanceCounter(&nFrameFinishTime);
                    LONGLONG nTimeElapsed = (nFrameFinishTime.QuadPart - nFrameStartTime.QuadPart)
                            * 1000ll / nPerformanceFrequency.QuadPart;
                    if (nTimeElapsed <= 0 || nTimeElapsed >= nFrameDelay)
                        break;
                    LONGLONG nDelayRemaining = nFrameDelay - nTimeElapsed;
                    ::Sleep((nDelayRemaining <= 2) ? 0 : (DWORD)(nDelayRemaining / 2));
                }
            }
        }

#if 0
        // Time bomb for performance analysis
        if (Emulator_GetUptime() >= 300)  // 5 minutes
            ::PostQuitMessage(0);
#endif
    }
endprog:

    ::timeEndPeriod(caps.wPeriodMin);
    DoneInstance();

#ifdef _DEBUG
    if (_CrtDumpMemoryLeaks())
        ::MessageBeep(MB_ICONEXCLAMATION);
#endif

    return (int) msg.wParam;
}


//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE /*hInstance*/, int /*nCmdShow*/)
{
    INITCOMMONCONTROLSEX ics;  ics.dwSize = sizeof(ics);
    ics.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&ics);

    BitmapFile_Init();

#if !defined(PRODUCT)
    DebugLogClear();
#endif
    Settings_Init();

    ParseCommandLine();  // Override settings by command-line option if needed

    if (!Emulator_Init())
        return FALSE;
    Emulator_SetSound(Settings_GetSound() != 0);
    Emulator_SetSoundAY(Settings_GetSoundAY() != 0);
    Emulator_SetSoundCovox(Settings_GetSoundCovox() != 0);
    Emulator_SetMouse(Settings_GetMouse() != 0);
    Emulator_SetSpeed(Settings_GetRealSpeed());

    if (!CreateMainWindow())
        return FALSE;

    return TRUE;
}

// Instance finalization
void DoneInstance()
{
    ScreenView_Done();
    DisasmView_Done();
    TapeView_Done();

    Emulator_Done();

    BitmapFile_Done();

    Settings_Done();
}

void ParseCommandLine()
{
    LPTSTR commandline = ::GetCommandLine();

    int argnum = 0;
    LPTSTR* args = CommandLineToArgvW(commandline, &argnum);

    if (!args) return;
    std::vector<std::basic_string_view<TCHAR>> arguments;
    for (int i = 1; i < argnum; ++i) arguments.emplace_back(args[i]);
    const auto options = ParseCommandLineOptions(arguments);
    Option_ShowHelp = options.help;
    Option_AutoBoot = options.boot;
    if (options.autostart) Settings_SetAutostart(*options.autostart);
    if (options.debug) Settings_SetDebug(*options.debug);
    if (options.sound) Settings_SetSound(*options.sound);
    if (options.fullscreen) Settings_SetWindowFullscreen(*options.fullscreen);
    for (int i = 0; i < 4; ++i)
        if (options.disks[i]) Settings_SetFloppyFilePath(i, options.disks[i]->c_str());
    for (int i = 0; i < 2; ++i)
    {
        if (options.cartridges[i]) Settings_SetCartridgeFilePath(i + 1, options.cartridges[i]->c_str());
        if (options.hardDrives[i]) Settings_SetHardFilePath(i + 1, options.hardDrives[i]->c_str());
    }
    ::LocalFree(args);
}


//////////////////////////////////////////////////////////////////////
