/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// ScreenView.cpp

#include "stdafx.h"
#include "util/Profiler.h"
#include <vector>
#include <memory>
#include <windowsx.h>
#include "Main.h"
#include "Views.h"
#include "Emulator.h"
#include "util/BitmapFile.h"
#include "util/ScreenScale.h"
#include "emubase/Emubase.h"

//////////////////////////////////////////////////////////////////////


HWND g_hwndScreen = nullptr;  // Screen View window handle

uint32_t* m_bits = nullptr;
static std::unique_ptr<uint32_t[]> m_screenBitsOwner;
int m_cxScreenWidth = UKNC_SCREEN_WIDTH;
int m_cyScreenHeight = UKNC_SCREEN_HEIGHT;
int m_xScreenOffset = 0;
int m_yScreenOffset = 0;
ScreenViewMode m_ScreenMode = RGBScreen;
int m_ScreenHeightMode = 0;  // Render mode

HMODULE g_hModuleRender = nullptr;
static bool g_renderInitialized = false;
RENDER_INIT_CALLBACK RenderInitProc = nullptr;
RENDER_DONE_CALLBACK RenderDoneProc = nullptr;
RENDER_DRAW_CALLBACK RenderDrawProc = nullptr;
RENDER_ENUM_MODES_CALLBACK RenderEnumModesProc = nullptr;
RENDER_SELECT_MODE_CALLBACK RenderSelectModeProc = nullptr;
HMENU g_hScreenModeMenu = nullptr;
int g_nScreenModeIndex = 0;

BYTE m_ScreenKeyState[256];
const int KEYEVENT_QUEUE_SIZE = 32;
WORD m_ScreenKeyQueue[KEYEVENT_QUEUE_SIZE];
int m_ScreenKeyQueueTop = 0;
int m_ScreenKeyQueueBottom = 0;
int m_ScreenKeyQueueCount = 0;
void ScreenView_PutKeyEventToQueue(WORD keyevent);
WORD ScreenView_GetKeyEventFromQueue();

POINT m_LastMousePos;

BOOL bEnter = FALSE;
BOOL bNumpadEnter = FALSE;
BOOL bEntPressed = FALSE;

void ScreenView_OnDraw(HDC hdc);

void ScreenView_OnRButtonDown(int mousex, int mousey);


//////////////////////////////////////////////////////////////////////
// Colors

/*
yrgb  R   G   B  0xRRGGBB
0000 000 000 000 0x000000
0001 000 000 128 0x000080
0010 000 128 000 0x008000
0011 000 128 128 0x008080
0100 128 000 000 0x800000
0101 128 000 128 0x800080
0110 128 128 000 0x808000
0111 128 128 128 0x808080
1000 000 000 000 0x000000
1001 000 000 255 0x0000FF
1010 000 255 000 0x00FF00
1011 000 255 255 0x00FFFF
1100 255 000 000 0xFF0000
1101 255 000 255 0xFF00FF
1110 255 255 000 0xFFFF00
1111 255 255 255 0xFFFFFF
*/

// Table for color conversion yrgb (4 bits) -> DWORD (32 bits)
constexpr uint32_t ScreenView_StandardRGBColors[16 * 8] =
{
    0x000000, 0x000080, 0x008000, 0x008080, 0x800000, 0x800080, 0x808000, 0x808080,
    0x000000, 0x0000FF, 0x00FF00, 0x00FFFF, 0xFF0000, 0xFF00FF, 0xFFFF00, 0xFFFFFF,
    0x000000, 0x000060, 0x008000, 0x008060, 0x800000, 0x800060, 0x808000, 0x808060,
    0x000000, 0x0000DF, 0x00FF00, 0x00FFDF, 0xFF0000, 0xFF00DF, 0xFFFF00, 0xFFFFDF,
    0x000000, 0x000080, 0x006000, 0x006080, 0x800000, 0x800080, 0x806000, 0x806080,
    0x000000, 0x0000FF, 0x00DF00, 0x00DFFF, 0xFF0000, 0xFF00FF, 0xFFDF00, 0xFFDFFF,
    0x000000, 0x000060, 0x006000, 0x006060, 0x800000, 0x800060, 0x806000, 0x806060,
    0x000000, 0x0000DF, 0x00DF00, 0x00DFDF, 0xFF0000, 0xFF00DF, 0xFFDF00, 0xFFDFDF,
    0x000000, 0x000080, 0x008000, 0x008080, 0x600000, 0x600080, 0x608000, 0x608080,
    0x000000, 0x0000FF, 0x00FF00, 0x00FFFF, 0xDF0000, 0xDF00FF, 0xDFFF00, 0xDFFFFF,
    0x000000, 0x000060, 0x008000, 0x008060, 0x600000, 0x600060, 0x608000, 0x608060,
    0x000000, 0x0000DF, 0x00FF00, 0x00FFDF, 0xDF0000, 0xDF00DF, 0xDFFF00, 0xDFFFDF,
    0x000000, 0x000080, 0x006000, 0x006080, 0x600000, 0x600080, 0x606000, 0x606080,
    0x000000, 0x0000FF, 0x00DF00, 0x00DFFF, 0xDF0000, 0xDF00FF, 0xDFDF00, 0xDFDFFF,
    0x000000, 0x000060, 0x006000, 0x006060, 0x600000, 0x600060, 0x606000, 0x606060,
    0x000000, 0x0000DF, 0x00DF00, 0x00DFDF, 0xDF0000, 0xDF00DF, 0xDFDF00, 0xDFDFDF,
};
constexpr uint32_t ScreenView_StandardGRBColors[16 * 8] =
{
    0x000000, 0x000080, 0x800000, 0x800080, 0x008000, 0x008080, 0x808000, 0x808080,
    0x000000, 0x0000FF, 0xFF0000, 0xFF00FF, 0x00FF00, 0x00FFFF, 0xFFFF00, 0xFFFFFF,
    0x000000, 0x000060, 0x800000, 0x800060, 0x008000, 0x008060, 0x808000, 0x808060,
    0x000000, 0x0000DF, 0xFF0000, 0xFF00DF, 0x00FF00, 0x00FFDF, 0xFFFF00, 0xFFFFDF,
    0x000000, 0x000080, 0x600000, 0x600080, 0x008000, 0x008080, 0x608000, 0x608080,
    0x000000, 0x0000FF, 0xDF0000, 0xDF00FF, 0x00FF00, 0x00FFFF, 0xDFFF00, 0xDFFFFF,
    0x000000, 0x000060, 0x600000, 0x600060, 0x008000, 0x008060, 0x608000, 0x608060,
    0x000000, 0x0000DF, 0xDF0000, 0xDF00DF, 0x00FF00, 0x00FFDF, 0xDFFF00, 0xDFFFDF,
    0x000000, 0x000080, 0x800000, 0x800080, 0x006000, 0x006080, 0x806000, 0x806080,
    0x000000, 0x0000FF, 0xFF0000, 0xFF00FF, 0x00DF00, 0x00DFFF, 0xFFDF00, 0xFFDFFF,
    0x000000, 0x000060, 0x800000, 0x800060, 0x006000, 0x006060, 0x806000, 0x806060,
    0x000000, 0x0000DF, 0xFF0000, 0xFF00DF, 0x00DF00, 0x00DFDF, 0xFFDF00, 0xFFDFDF,
    0x000000, 0x000080, 0x600000, 0x600080, 0x006000, 0x006080, 0x606000, 0x606080,
    0x000000, 0x0000FF, 0xDF0000, 0xDF00FF, 0x00DF00, 0x00DFFF, 0xDFDF00, 0xDFDFFF,
    0x000000, 0x000060, 0x600000, 0x600060, 0x006000, 0x006060, 0x606000, 0x606060,
    0x000000, 0x0000DF, 0xDF0000, 0xDF00DF, 0x00DF00, 0x00DFDF, 0xDFDF00, 0xDFDFDF,
};
// Table for color conversion, gray (black and white) display
constexpr uint32_t ScreenView_GrayColors[16 * 8] =
{
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
};


//////////////////////////////////////////////////////////////////////

//Prototype of the screen conversion function
using PREPARE_SCREEN_CALLBACK = void (CALLBACK*)(void* pImageBits);

struct ScreenModeStruct
{
    int modeNum;
    int width;
    int height;
    TCHAR description[40];
}
static ScreenModeReference[32];

LPCTSTR ScreenView_GetRenderModeDescription(int renderMode)
{
    if (renderMode < 0 || renderMode >= 32)
        return nullptr;
    LPCTSTR modedesc = ScreenModeReference[renderMode].description;
    if (*modedesc == 0) return nullptr;  // Empty string
    return modedesc;
}


//////////////////////////////////////////////////////////////////////


void ScreenView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = ScreenViewWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInst;
    wcex.hIcon          = nullptr;
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = nullptr; //(HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = nullptr;
    wcex.lpszClassName  = CLASSNAME_SCREENVIEW;
    wcex.hIconSm        = nullptr;

    RegisterClassEx(&wcex);
}

void ScreenView_Init()
{
    m_screenBitsOwner = std::make_unique<uint32_t[]>(UKNC_SCREEN_WIDTH * UKNC_SCREEN_HEIGHT);
    m_bits = m_screenBitsOwner.get();

    ::GetCursorPos(&m_LastMousePos);
}

void ScreenView_Done()
{
    m_screenBitsOwner.reset();
    m_bits = nullptr;
}

ScreenViewMode ScreenView_GetMode()
{
    return m_ScreenMode;
}
void ScreenView_SetMode(ScreenViewMode newMode)
{
    m_ScreenMode = newMode;
}

void CALLBACK ScreenView_EnumModesProc(int modeNum, LPCTSTR modeDesc, int modeWidth, int modeHeight)
{
    if (g_nScreenModeIndex >= 32 || modeDesc == nullptr)
        return;

    ScreenModeStruct* pmode = ScreenModeReference + g_nScreenModeIndex;
    pmode->modeNum = modeNum;
    pmode->width = modeWidth;
    pmode->height = modeHeight;
    _tcsncpy_s(pmode->description, _countof(pmode->description), modeDesc, _TRUNCATE);

    g_nScreenModeIndex++;
}

BOOL ScreenView_InitRender(LPCTSTR szRenderLibraryName)
{
    g_hModuleRender = ::LoadLibrary(szRenderLibraryName);
    if (g_hModuleRender == nullptr)
    {
        AlertWarningFormat(_T("Failed to load render library \"{}\" (0x{:08x})."),
                szRenderLibraryName, ::GetLastError());
        return FALSE;
    }

    RenderInitProc = (RENDER_INIT_CALLBACK) ::GetProcAddress(g_hModuleRender, "RenderInit");
    if (RenderInitProc == nullptr)
    {
        AlertWarningFormat(_T("Failed to retrieve RenderInit address (0x{:08x})."), ::GetLastError());
        ScreenView_DoneRender();
        return FALSE;
    }
    RenderDoneProc = (RENDER_DONE_CALLBACK) ::GetProcAddress(g_hModuleRender, "RenderDone");
    if (RenderDoneProc == nullptr)
    {
        AlertWarningFormat(_T("Failed to retrieve RenderDone address (0x{:08x})."), ::GetLastError());
        ScreenView_DoneRender();
        return FALSE;
    }
    RenderDrawProc = (RENDER_DRAW_CALLBACK) ::GetProcAddress(g_hModuleRender, "RenderDraw");
    if (RenderDrawProc == nullptr)
    {
        AlertWarningFormat(_T("Failed to retrieve RenderDraw address (0x{:08x})."), ::GetLastError());
        ScreenView_DoneRender();
        return FALSE;
    }
    RenderEnumModesProc = (RENDER_ENUM_MODES_CALLBACK) ::GetProcAddress(g_hModuleRender, "RenderEnumModes");
    if (RenderEnumModesProc == nullptr)
    {
        AlertWarningFormat(_T("Failed to retrieve RenderEnumModes address (0x{:08x})."), ::GetLastError());
        ScreenView_DoneRender();
        return FALSE;
    }
    RenderSelectModeProc = (RENDER_SELECT_MODE_CALLBACK) ::GetProcAddress(g_hModuleRender, "RenderSelectMode");
    if (RenderSelectModeProc == nullptr)
    {
        AlertWarningFormat(_T("Failed to retrieve RenderSelectMode address (0x{:08x})."), ::GetLastError());
        ScreenView_DoneRender();
        return FALSE;
    }

    // Enumerate render modes
    memset(ScreenModeReference, 0, sizeof(ScreenModeReference));
    g_nScreenModeIndex = 0;
    RenderEnumModesProc(ScreenView_EnumModesProc);
    g_hScreenModeMenu = nullptr;

    // Fill Render Mode menu
    MainWindow_UpdateRenderModeMenu();

    //ScreenView_CreateScreen();

    if (!RenderInitProc(UKNC_SCREEN_WIDTH, UKNC_SCREEN_HEIGHT, g_hwndScreen))
    {
        AlertWarning(_T("Failed to initialize the render."));
        ScreenView_DoneRender();
        return FALSE;
    }

    g_renderInitialized = true;
    return TRUE;
}

void ScreenView_DoneRender()
{
    if (g_hModuleRender != nullptr)
    {
        if (g_renderInitialized && RenderDoneProc != nullptr)
            RenderDoneProc();
        g_renderInitialized = false;

        RenderInitProc = nullptr;
        RenderDoneProc = nullptr;
        RenderDrawProc = nullptr;
        RenderEnumModesProc = nullptr;
        RenderSelectModeProc = nullptr;

        ::FreeLibrary(g_hModuleRender);
        g_hModuleRender = nullptr;

        //DestroyScreen();
    }

    //TODO: Clear Render Mode menu
}

// Create Screen View as child of Main Window
void ScreenView_Create(HWND hwndParent, int x, int y)
{
    ASSERT(hwndParent != nullptr);

    int xLeft = x;
    int yTop = y;
    int cxWidth = m_cxScreenWidth;
    int cyScreenHeight = m_cyScreenHeight;
    int cyHeight = cyScreenHeight;

    g_hwndScreen = CreateWindow(
            CLASSNAME_SCREENVIEW, nullptr,
            WS_CHILD | WS_VISIBLE,
            xLeft, yTop, cxWidth, cyHeight,
            hwndParent, nullptr, g_hInst, nullptr);

    // Initialize m_ScreenKeyState
    VERIFY(::GetKeyboardState(m_ScreenKeyState));
}

LRESULT CALLBACK ScreenViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_COMMAND:
        ::PostMessage(g_hwnd, WM_COMMAND, wParam, lParam);
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            ScreenView_PrepareScreen();
            ScreenView_OnDraw(hdc);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_LBUTTONDOWN:
        SetFocus(hWnd);
        break;
    case WM_RBUTTONDOWN:
        ScreenView_OnRButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wParam == VK_RETURN)
        {
            if (lParam & 0x1000000)
                bNumpadEnter = TRUE;
            else
                bEnter = TRUE;
        }
        break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (wParam == VK_RETURN)
        {
            if (lParam & 0x1000000)
                bNumpadEnter = FALSE;
            else
                bEnter = FALSE;
        }
        break;
    case WM_SETCURSOR:
        if (::GetFocus() == g_hwndScreen)
        {
            SetCursor(::LoadCursor(nullptr, MAKEINTRESOURCE(IDC_IBEAM)));
            return (LRESULT) TRUE;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

void ScreenView_OnRButtonDown(int mousex, int mousey)
{
    ::SetFocus(g_hwndScreen);

    if (Settings_GetMouse())
        return;

    HMENU hMenu = ::CreatePopupMenu();
    ::AppendMenu(hMenu, 0, ID_FILE_SCREENSHOT, _T("Screenshot"));
    ::AppendMenu(hMenu, 0, ID_FILE_SCREENSHOTTOCLIPBOARD, _T("Screenshot to Clipboard"));
    ::AppendMenu(hMenu, 0, ID_FILE_SCREENTOCLIPBOARD, _T("Screen Text to Clipboard"));
    if (MainWindow_IsFullscreen())
    {
        ::AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
        ::AppendMenu(hMenu, 0, ID_VIEW_FULLSCREEN, _T("Exit Fullscreen"));
    }

    POINT pt = { mousex, mousey };
    ::ClientToScreen(g_hwndScreen, &pt);
    ::TrackPopupMenu(hMenu, 0, pt.x, pt.y, 0, g_hwndScreen, nullptr);

    VERIFY(::DestroyMenu(hMenu));
}

int ScreenView_GetRenderMode()
{
    return m_ScreenHeightMode;
}
void ScreenView_SetRenderMode(int newRenderMode)
{
    if (m_ScreenHeightMode == newRenderMode) return;

    if (RenderSelectModeProc == nullptr)
        return;

    ScreenModeStruct* pmode = ScreenModeReference + newRenderMode;
    RenderSelectModeProc(pmode->modeNum);
    m_cxScreenWidth = pmode->width;
    m_cyScreenHeight = pmode->height;
    m_ScreenHeightMode = newRenderMode;

    ScreenView_RedrawScreen();

    //if (pmode->width > 0 && pmode->height > 0)
    //{
    //    int cxScreen = pmode->width;
    //    int cyScreen = pmode->height;
    //    int cyBorder = ::GetSystemMetrics(SM_CYBORDER);
    //    int cyHeight = cyScreen + cyBorder * 2;
    //    ::SetWindowPos(g_hwndScreen, nullptr, 0,0, cxScreen, cyScreen, SWP_NOZORDER | SWP_NOMOVE);
    //}
}

void ScreenView_OnDraw(HDC hdc)
{
    UKNC_PROFILE_SCOPE("Screen draw");
    if (RenderDrawProc != nullptr)
    {
        RenderDrawProc(m_bits, hdc);
    }

    // Calculate m_xScreenOffset and m_yScreenOffset for mouse calculations
    RECT rc;  ::GetClientRect(g_hwndScreen, &rc);
    m_xScreenOffset = m_yScreenOffset = 0;
    if (rc.right > m_cxScreenWidth)
        m_xScreenOffset = (rc.right - m_cxScreenWidth) / 2;
    if (rc.bottom > m_cyScreenHeight)
        m_yScreenOffset = (rc.bottom - m_cyScreenHeight) / 2;
}

void ScreenView_RedrawScreen()
{
    ScreenView_PrepareScreen();

    HDC hdc = GetDC(g_hwndScreen);
    ScreenView_OnDraw(hdc);
    VERIFY(::ReleaseDC(g_hwndScreen, hdc));
}

// Choose color palette depending of screen mode
std::span<const uint32_t, 16 * 8> ScreenView_GetPalette()
{
    switch (m_ScreenMode)
    {
    case GrayScreen: return ScreenView_GrayColors;
    case GRBScreen: return ScreenView_StandardGRBColors;
    default: return ScreenView_StandardRGBColors;
    }
}

void ScreenView_PrepareScreen()
{
    UKNC_PROFILE_SCOPE("Prepare screen");
    if (m_bits == nullptr) return;

    Emulator_PrepareScreenRGB32(std::span{m_bits, static_cast<size_t>(UKNC_SCREEN_WIDTH * UKNC_SCREEN_HEIGHT)},
        ScreenView_GetPalette());
}

void ScreenView_PutKeyEventToQueue(WORD keyevent)
{
    if (m_ScreenKeyQueueCount == KEYEVENT_QUEUE_SIZE) return;  // Full queue

    m_ScreenKeyQueue[m_ScreenKeyQueueTop] = keyevent;
    m_ScreenKeyQueueTop++;
    if (m_ScreenKeyQueueTop >= KEYEVENT_QUEUE_SIZE)
        m_ScreenKeyQueueTop = 0;
    m_ScreenKeyQueueCount++;
}
WORD ScreenView_GetKeyEventFromQueue()
{
    if (m_ScreenKeyQueueCount == 0) return 0;  // Empty queue

    WORD keyevent = m_ScreenKeyQueue[m_ScreenKeyQueueBottom];
    m_ScreenKeyQueueBottom++;
    if (m_ScreenKeyQueueBottom >= KEYEVENT_QUEUE_SIZE)
        m_ScreenKeyQueueBottom = 0;
    m_ScreenKeyQueueCount--;

    return keyevent;
}

const BYTE arrPcscan2UkncscanLat[256] =    // LAT
{
    /*       0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f  */
    /*0*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0132, 0026, 0000, 0000, 0000, 0153, 0166, 0000,
    /*1*/    0105, 0000, 0000, 0000, 0107, 0000, 0000, 0000, 0000, 0000, 0000, 0006, 0000, 0000, 0000, 0000,
    /*2*/    0113, 0004, 0151, 0152, 0155, 0116, 0154, 0133, 0134, 0000, 0000, 0000, 0000, 0171, 0172, 0000,
    /*3*/    0176, 0030, 0031, 0032, 0013, 0034, 0035, 0016, 0017, 0177, 0000, 0000, 0000, 0000, 0000, 0000,
    /*4*/    0000, 0072, 0076, 0050, 0057, 0033, 0047, 0055, 0156, 0073, 0027, 0052, 0056, 0112, 0054, 0075,
    /*5*/    0053, 0067, 0074, 0111, 0114, 0051, 0137, 0071, 0115, 0070, 0157, 0000, 0000, 0106, 0000, 0000,
    /*6*/    0126, 0127, 0147, 0167, 0130, 0150, 0170, 0125, 0145, 0165, 0025, 0000, 0000, 0005, 0146, 0131,
    /*7*/    0010, 0011, 0012, 0014, 0015, 0172, 0152, 0151, 0171, 0000, 0004, 0155, 0000, 0000, 0000, 0000,
    /*8*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*9*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*a*/    0000, 0000, 0046, 0066, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*b*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0174, 0110, 0117, 0175, 0135, 0173,
    /*c*/    0007, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*d*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0036, 0136, 0037, 0077, 0000,
    /*e*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*f*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
};
const BYTE arrPcscan2UkncscanRus[256] =    // RUS
{
    /*       0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f  */
    /*0*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0132, 0026, 0000, 0000, 0000, 0153, 0166, 0000,
    /*1*/    0105, 0000, 0000, 0000, 0107, 0000, 0000, 0000, 0000, 0000, 0000, 0006, 0000, 0000, 0000, 0000,
    /*2*/    0113, 0004, 0151, 0152, 0174, 0116, 0154, 0133, 0134, 0000, 0000, 0000, 0000, 0171, 0172, 0000,
    /*3*/    0176, 0030, 0031, 0032, 0013, 0034, 0035, 0016, 0017, 0177, 0000, 0000, 0000, 0000, 0000, 0000,
    /*4*/    0000, 0047, 0073, 0111, 0071, 0051, 0072, 0053, 0074, 0036, 0075, 0056, 0057, 0115, 0114, 0037,
    /*5*/    0157, 0027, 0052, 0070, 0033, 0055, 0112, 0050, 0110, 0054, 0067, 0000, 0000, 0106, 0000, 0000,
    /*6*/    0126, 0127, 0147, 0167, 0130, 0150, 0170, 0125, 0145, 0165, 0025, 0000, 0000, 0005, 0146, 0131,
    /*7*/    0010, 0011, 0012, 0014, 0015, 0172, 0152, 0151, 0171, 0000, 0004, 0174, 0000, 0000, 0000, 0000,
    /*8*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*9*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*a*/    0000, 0000, 0046, 0066, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*b*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0137, 0117, 0076, 0175, 0077, 0173,
    /*c*/    0007, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*d*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0156, 0135, 0155, 0136, 0000,
    /*e*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*f*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
};

void ScreenView_ScanKeyboard()
{
    if (! g_okEmulatorRunning) return;
    if (::GetFocus() == g_hwndScreen)
    {
        // Read current keyboard state
        BYTE keys[256];
        VERIFY(::GetKeyboardState(keys));
        if (keys[VK_RETURN] & 128)
        {
            if (bEnter && bNumpadEnter)
                keys[VK_RETURN + 1] = 128;
            if (!bEnter && bNumpadEnter)
            {
                keys[VK_RETURN] = 0;
                keys[VK_RETURN + 1] = 128;
            }
            bEntPressed = TRUE;
        }
        else
        {
            if (bEntPressed)
            {
                if (bEnter) keys[VK_RETURN + 1] = 128;
                if (bNumpadEnter) keys[VK_RETURN + 1] = 128;
            }
            else
            {
                bEnter = FALSE;
                bNumpadEnter = FALSE;
            }
            bEntPressed = FALSE;
        }
        // Choose the mapping table depending on the RUS/LAT flag in the UKNC
        uint16_t ukncRegister = g_pBoard->GetKeyboardRegister();

        // Check every key for state change
        for (int scan = 0; scan < 256; scan++)
        {
            BYTE newstate = keys[scan];
            BYTE oldstate = m_ScreenKeyState[scan];
            if ((newstate & 128) != (oldstate & 128))  // Key state changed - key pressed or released
            {
                const BYTE* pTable = nullptr;
                BYTE pcscan = (BYTE)scan;
                BYTE ukncscan;
                if (oldstate & 128)
                {
                    pTable = ((oldstate & KEYB_LAT) != 0) ? arrPcscan2UkncscanLat : arrPcscan2UkncscanRus;
                    m_ScreenKeyState[scan] = 0;
                }
                else
                {
                    pTable = ((ukncRegister & KEYB_LAT) != 0) ? arrPcscan2UkncscanLat : arrPcscan2UkncscanRus;
                    m_ScreenKeyState[scan] = (BYTE)((newstate & 128) | ukncRegister);
                }
                ukncscan = pTable[pcscan];
                if (ukncscan != 0)
                {
                    BYTE pressed = newstate & 128;
                    WORD keyevent = MAKEWORD(ukncscan, pressed);
                    ScreenView_PutKeyEventToQueue(keyevent);
                    KeyboardView_KeyEvent(ukncscan, pressed);
                }
            }
        }
    }

    // Process the keyboard queue
    WORD keyevent;
    while ((keyevent = ScreenView_GetKeyEventFromQueue()) != 0)
    {
        bool pressed = ((keyevent & 0x8000) != 0);
        BYTE ukncscan = LOBYTE(keyevent);
        g_pBoard->KeyboardEvent(ukncscan, pressed);
    }
}

// External key event - e.g. from KeyboardView
void ScreenView_KeyEvent(BYTE keyscan, BOOL pressed)
{
    ScreenView_PutKeyEventToQueue(MAKEWORD(keyscan, pressed ? 128 : 0));
}

void ScreenView_UpdateMouse()
{
    POINT mousepos;
    ::GetCursorPos(&mousepos);

    int dx = (short)(mousepos.x - m_LastMousePos.x);
    int dy = (short)(mousepos.y - m_LastMousePos.y);

    RECT rcScreen = { m_xScreenOffset, m_yScreenOffset, m_xScreenOffset + m_cxScreenWidth, m_yScreenOffset + m_cyScreenHeight };
    ::MapWindowPoints(g_hwndScreen, nullptr, (LPPOINT)&rcScreen, 2);
    if (dx != 0 && mousepos.x < rcScreen.left)
        dx -= (rcScreen.left - mousepos.x) / 2;
    if (dx != 0 && mousepos.x > rcScreen.right)
        dx += (mousepos.x - rcScreen.right) / 2;
    if (dy != 0 && mousepos.y < rcScreen.top)
        dy -= (rcScreen.top - mousepos.y) / 2;
    if (dy != 0 && mousepos.y > rcScreen.bottom)
        dy += (mousepos.y - rcScreen.bottom) / 2;

    if (m_cxScreenWidth != UKNC_SCREEN_WIDTH)
        dx = dx * UKNC_SCREEN_WIDTH / m_cxScreenWidth;
    if (m_cyScreenHeight != UKNC_SCREEN_HEIGHT)
        dy = dy * UKNC_SCREEN_HEIGHT / m_cyScreenHeight;

    if (dx < -127)
        dx = -127;
    else if (dx > 127)
        dx = 127;
    if (dy < -127)
        dy = -127;
    else if (dy > 127)
        dy = 127;

    bool btnLeft = false, btnRight = false;
    if (::GetFocus() == g_hwndScreen)
    {
        btnLeft = ::GetAsyncKeyState(VK_LBUTTON) != 0;
        btnRight = ::GetAsyncKeyState(VK_RBUTTON) != 0;
    }

    g_pBoard->MouseMove((int16_t)dx, (int16_t)dy, btnLeft, btnRight);

    m_LastMousePos = mousepos;
}

BOOL ScreenView_SaveScreenshot(LPCTSTR sFileName, int screenshotMode)
{
    ASSERT(sFileName != nullptr);

    std::vector<uint32_t> bits(UKNC_SCREEN_WIDTH * UKNC_SCREEN_HEIGHT);
    Emulator_PrepareScreenRGB32(bits, ScreenView_GetPalette());

    const auto [scrwidth, scrheight, description] = screen_scale::GetMode(screenshotMode);
    std::vector<uint32_t> scrBits(static_cast<size_t>(scrwidth) * scrheight);
    if (!screen_scale::Apply(screenshotMode, bits, scrBits))
        return FALSE;

    LPCTSTR sFileNameExt = _tcsrchr(sFileName, _T('.'));
    BitmapFileFormat imageFormat = BitmapFileFormatPng;
    if (sFileNameExt != nullptr && _tcsicmp(sFileNameExt, _T(".bmp")) == 0)
        imageFormat = BitmapFileFormatBmp;
    BOOL result = BitmapFile_SaveImageFile(
            scrBits, sFileName, imageFormat, scrwidth, scrheight);

    return result;
}

HGLOBAL ScreenView_GetScreenshotAsDIB(int screenshotMode)
{
    std::vector<uint32_t> bits(UKNC_SCREEN_WIDTH * UKNC_SCREEN_HEIGHT);

    Emulator_PrepareScreenRGB32(bits, ScreenView_GetPalette());

    const auto [scrwidth, scrheight, description] = screen_scale::GetMode(screenshotMode);
    std::vector<uint32_t> scrBits(static_cast<size_t>(scrwidth) * scrheight);
    if (!screen_scale::Apply(screenshotMode, bits, scrBits))
        return nullptr;

    BITMAPINFOHEADER bi;
    ::ZeroMemory(&bi, sizeof(BITMAPINFOHEADER));
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = scrwidth;
    bi.biHeight = scrheight;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = bi.biWidth * bi.biHeight * 4;

    HGLOBAL hDIB = ::GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + bi.biSizeImage);
    if (hDIB == nullptr)
    {
        return nullptr;
    }

    LPBYTE p = (LPBYTE) ::GlobalLock(hDIB);
    if (p == nullptr)
    {
        ::GlobalFree(hDIB);
        return nullptr;
    }
    ::CopyMemory(p, &bi, sizeof(BITMAPINFOHEADER));
    p += sizeof(BITMAPINFOHEADER);
    for (int line = 0; line < scrheight; line++)
    {
        const auto row = std::span{scrBits}.subspan(static_cast<size_t>(scrheight - line - 1) * scrwidth, scrwidth);
        ::CopyMemory(p, row.data(), row.size_bytes());
        p += row.size_bytes();
    }
    ::GlobalUnlock(hDIB);

    return hDIB;
}

static BYTE RecognizeCharacter(const uint8_t* fontcur, const uint8_t* fontstd, const uint32_t* pBits)
{
    int16_t bestmatch = -32767;
    uint8_t bestchar = 0;
    for (uint8_t charidx = 0; charidx < 16 * 14; charidx++)
    {
        int16_t matchcur = 0;
        int16_t matchstd = 0;
        const uint32_t * pb = pBits;
        for (int16_t y = 0; y < 11; y++)
        {
            uint8_t fontcurdata = fontcur[charidx * 11 + y];
            uint8_t fontstddata = fontstd[charidx * 11 + y];
            for (int x = 0; x < 8; x++)
            {
                uint32_t color = pb[x];
                int sum = (color & 0xff) + ((color >> 8) & 0xff) + ((color >> 16) & 0xff);
                uint8_t fontcurbit = (fontcurdata >> x) & 1;
                uint8_t fontstdbit = (fontstddata >> x) & 1;
                if (sum > 384)
                {
                    matchcur += fontcurbit;  matchstd += fontstdbit;
                }
                else
                {
                    matchcur -= fontcurbit;  matchstd -= fontstdbit;
                }
            }
            pb += 640;
        }
        if (matchcur > bestmatch)
        {
            bestmatch = matchcur;
            bestchar = charidx;
        }
        if (matchstd > bestmatch)
        {
            bestmatch = matchstd;
            bestchar = charidx;
        }
    }

    return 0x20 + bestchar;
}

// buffer size is 82 * 26 + 1 means 26 lines, 80 chars in every line plus CR/LF plus trailing zero
BOOL ScreenView_ScreenToText(uint8_t* buffer)
{
    // Get screenshot
    std::vector<uint32_t> bits(UKNC_SCREEN_WIDTH * UKNC_SCREEN_HEIGHT);
    Emulator_PrepareScreenToText(bits, ScreenView_GrayColors);

    // Prepare font, get current font data from PPU memory
    CMemoryController* pPpuMemCtl = g_pBoard->GetPPUMemoryController();
    uint8_t fontcur[11 * 16 * 14];
    uint16_t fontaddr = 014142 + 32 * 2;
    for (BYTE charidx = 0; charidx < 16 * 14; charidx++)
    {
        uint16_t charaddr = pPpuMemCtl->GetWordView(fontaddr + charidx * 2, false, false).value;
        for (int16_t y = 0; y < 11; y++)
        {
            uint16_t fontdata = pPpuMemCtl->GetWordView((charaddr + y) & ~1, false, false).value;
            if (((charaddr + y) & 1) == 1) fontdata >>= 8;
            fontcur[charidx * 11 + y] = (uint8_t)(fontdata & 0xff);
        }
    }
    // Prepare font, get standard font data from PPU memory
    uint8_t fontstd[11 * 16 * 14];
    uint16_t charstdaddr = 0120170;
    for (uint16_t idx = 0; idx < 16 * 14 * 11; idx++)
    {
        uint16_t fontdata = pPpuMemCtl->GetWordView(charstdaddr & ~1, false, false).value;
        if ((charstdaddr & 1) == 1) fontdata >>= 8;
        fontstd[idx] = (uint8_t)(fontdata & 0xff);
        charstdaddr++;
    }

    // Loop for lines
    int charidx = 0;
    int y = 0;
    while (y <= 288 - 11)
    {
        const uint32_t* pCharBits = bits.data() + y * 640;

        for (int x = 0; x < 640; x += 8)
        {
            uint8_t ch = RecognizeCharacter(fontcur, fontstd, pCharBits + x);
            buffer[charidx] = ch;
            charidx++;
        }
        buffer[charidx++] = 0x0d;
        buffer[charidx++] = 0x0a;

        y += 11;
        if (y == 11) y++;  // Extra line after upper indicator lines
        if (y == 276) y++;  // Extra line before lower indicator lines
    }

    return TRUE;
}


//////////////////////////////////////////////////////////////////////


LPCTSTR ScreenView_GetScreenshotModeName(int screenshotMode)
{
    const auto modes = screen_scale::Modes();
    if (screenshotMode < 0 || static_cast<size_t>(screenshotMode) >= modes.size())
        return nullptr;
    return modes[screenshotMode].description;
}

//////////////////////////////////////////////////////////////////////
