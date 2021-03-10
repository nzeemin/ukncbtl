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
#include "Main.h"
#include "Views.h"
#include "Emulator.h"
#include "util\BitmapFile.h"
#include "emubase\Emubase.h"

//////////////////////////////////////////////////////////////////////


HWND g_hwndScreen = NULL;  // Screen View window handle

DWORD * m_bits = NULL;
int m_cxScreenWidth;
int m_cyScreenHeight;
int m_xScreenOffset = 0;
int m_yScreenOffset = 0;
ScreenViewMode m_ScreenMode = RGBScreen;
int m_ScreenHeightMode = 0;  // Render mode

HMODULE g_hModuleRender = NULL;
RENDER_INIT_CALLBACK RenderInitProc = NULL;
RENDER_DONE_CALLBACK RenderDoneProc = NULL;
RENDER_DRAW_CALLBACK RenderDrawProc = NULL;
RENDER_ENUM_MODES_CALLBACK RenderEnumModesProc = NULL;
RENDER_SELECT_MODE_CALLBACK RenderSelectModeProc = NULL;
HMENU g_hScreenModeMenu = NULL;
int g_nScreenModeIndex = 0;

BYTE m_ScreenKeyState[256];
const int KEYEVENT_QUEUE_SIZE = 32;
WORD m_ScreenKeyQueue[KEYEVENT_QUEUE_SIZE];
int m_ScreenKeyQueueTop = 0;
int m_ScreenKeyQueueBottom = 0;
int m_ScreenKeyQueueCount = 0;
void ScreenView_PutKeyEventToQueue(WORD keyevent);
WORD ScreenView_GetKeyEventFromQueue();

BOOL bEnter = FALSE;
BOOL bNumpadEnter = FALSE;
BOOL bEntPressed = FALSE;

void ScreenView_OnDraw(HDC hdc);

typedef void (CALLBACK* PREPARE_SCREENSHOT_CALLBACK)(const void * pSrcBits, void * pDestBits);

void ScreenView_GetScreenshotSize(int screenshotMode, int* pwid, int* phei);
PREPARE_SCREENSHOT_CALLBACK ScreenView_GetScreenshotCallback(int screenshotMode);


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
const DWORD ScreenView_StandardRGBColors[16 * 8] =
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
const DWORD ScreenView_StandardGRBColors[16 * 8] =
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
const DWORD ScreenView_GrayColors[16 * 8] =
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

//Прототип функции преобразования экрана
typedef void (CALLBACK* PREPARE_SCREEN_CALLBACK)(void* pImageBits);

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
        return NULL;
    LPCTSTR modedesc = ScreenModeReference[renderMode].description;
    if (*modedesc == 0) return NULL;  // Empty string
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
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = NULL; //(HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = CLASSNAME_SCREENVIEW;
    wcex.hIconSm        = NULL;

    RegisterClassEx(&wcex);
}

void ScreenView_Init()
{
    m_bits = static_cast<DWORD*>(::calloc(UKNC_SCREEN_WIDTH * UKNC_SCREEN_HEIGHT * 4, 1));
}

void ScreenView_Done()
{
    if (m_bits != NULL)
    {
        ::free(m_bits);
        m_bits = NULL;
    }
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
    if (g_nScreenModeIndex >= 32)
        return;

    ScreenModeStruct* pmode = ScreenModeReference + g_nScreenModeIndex;
    pmode->modeNum = modeNum;
    pmode->width = modeWidth;
    pmode->height = modeHeight;
    _tcscpy_s(pmode->description, 40, modeDesc);

    g_nScreenModeIndex++;
}

BOOL ScreenView_InitRender(LPCTSTR szRenderLibraryName)
{
    g_hModuleRender = ::LoadLibrary(szRenderLibraryName);
    if (g_hModuleRender == NULL)
    {
        AlertWarningFormat(_T("Failed to load render library \"%s\" (0x%08lx)."),
                szRenderLibraryName, ::GetLastError());
        return FALSE;
    }

    RenderInitProc = (RENDER_INIT_CALLBACK) ::GetProcAddress(g_hModuleRender, "RenderInit");
    if (RenderInitProc == NULL)
    {
        AlertWarningFormat(_T("Failed to retrieve RenderInit address (0x%08lx)."), ::GetLastError());
        return FALSE;
    }
    RenderDoneProc = (RENDER_DONE_CALLBACK) ::GetProcAddress(g_hModuleRender, "RenderDone");
    if (RenderDoneProc == NULL)
    {
        AlertWarningFormat(_T("Failed to retrieve RenderDone address (0x%08lx)."), ::GetLastError());
        return FALSE;
    }
    RenderDrawProc = (RENDER_DRAW_CALLBACK) ::GetProcAddress(g_hModuleRender, "RenderDraw");
    if (RenderDrawProc == NULL)
    {
        AlertWarningFormat(_T("Failed to retrieve RenderDraw address (0x%08lx)."), ::GetLastError());
        return FALSE;
    }
    RenderEnumModesProc = (RENDER_ENUM_MODES_CALLBACK) ::GetProcAddress(g_hModuleRender, "RenderEnumModes");
    if (RenderEnumModesProc == NULL)
    {
        AlertWarningFormat(_T("Failed to retrieve RenderEnumModes address (0x%08lx)."), ::GetLastError());
        return FALSE;
    }
    RenderSelectModeProc = (RENDER_SELECT_MODE_CALLBACK) ::GetProcAddress(g_hModuleRender, "RenderSelectMode");
    if (RenderSelectModeProc == NULL)
    {
        AlertWarningFormat(_T("Failed to retrieve RenderSelectMode address (0x%08lx)."), ::GetLastError());
        return FALSE;
    }

    // Enumerate render modes
    memset(ScreenModeReference, 0, sizeof(ScreenModeReference));
    g_nScreenModeIndex = 0;
    RenderEnumModesProc(ScreenView_EnumModesProc);
    g_hScreenModeMenu = NULL;

    // Fill Render Mode menu
    MainWindow_UpdateRenderModeMenu();

    //ScreenView_CreateScreen();

    if (!RenderInitProc(UKNC_SCREEN_WIDTH, UKNC_SCREEN_HEIGHT, g_hwndScreen))
    {
        AlertWarning(_T("Failed to initialize the render."));
        //ScreenView_DestroyScreen();
        return FALSE;
    }

    return TRUE;
}

void ScreenView_DoneRender()
{
    if (g_hModuleRender != NULL)
    {
        if (RenderDoneProc != NULL)
            RenderDoneProc();

        RenderInitProc = NULL;
        RenderDoneProc = NULL;
        RenderDrawProc = NULL;
        RenderEnumModesProc = NULL;
        RenderSelectModeProc = NULL;

        ::FreeLibrary(g_hModuleRender);
        g_hModuleRender = NULL;

        //DestroyScreen();
    }

    //TODO: Clear Render Mode menu
}

// Create Screen View as child of Main Window
void ScreenView_Create(HWND hwndParent, int x, int y)
{
    ASSERT(hwndParent != NULL);

    int xLeft = x;
    int yTop = y;
    int cxWidth = m_cxScreenWidth;
    int cyScreenHeight = m_cyScreenHeight;
    int cyHeight = cyScreenHeight;

    g_hwndScreen = CreateWindow(
            CLASSNAME_SCREENVIEW, NULL,
            WS_CHILD | WS_VISIBLE,
            xLeft, yTop, cxWidth, cyHeight,
            hwndParent, NULL, g_hInst, NULL);

    // Initialize m_ScreenKeyState
    VERIFY(::GetKeyboardState(m_ScreenKeyState));
}

LRESULT CALLBACK ScreenViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
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
            SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_IBEAM)));
            return (LRESULT) TRUE;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

int ScreenView_GetRenderMode()
{
    return m_ScreenHeightMode;
}
void ScreenView_SetRenderMode(int newRenderMode)
{
    if (m_ScreenHeightMode == newRenderMode) return;

    if (RenderSelectModeProc == NULL)
        return;

    ScreenModeStruct* pmode = ScreenModeReference + newRenderMode;
    RenderSelectModeProc(pmode->modeNum);

    m_ScreenHeightMode = newRenderMode;

    ScreenView_RedrawScreen();

    //if (pmode->width > 0 && pmode->height > 0)
    //{
    //    int cxScreen = pmode->width;
    //    int cyScreen = pmode->height;
    //    int cyBorder = ::GetSystemMetrics(SM_CYBORDER);
    //    int cyHeight = cyScreen + cyBorder * 2;
    //    ::SetWindowPos(g_hwndScreen, NULL, 0,0, cxScreen, cyScreen, SWP_NOZORDER | SWP_NOMOVE);
    //}
}

void ScreenView_DrawOsd(HDC hdc)
{
    int osdSize = Settings_GetOsdSize();
    int osdPosition = Settings_GetOsdPosition();

    RECT rcClient;  ::GetClientRect(g_hwndScreen, &rcClient);

    int osdSize12 = osdSize / 2;
    int osdSize13 = osdSize / 3;
    int osdSize14 = osdSize / 4;
    int osdSize23 = osdSize * 2 / 3;
    int osdSize34 = osdSize * 3 / 4;
    int osdLeft = (osdPosition & 1) ? rcClient.right - osdSize14 - osdSize : osdSize14;
    int osdTop = (osdPosition & 2) ? rcClient.bottom - osdSize14 - osdSize : osdSize14;
    int osdNext = (osdPosition & 2) ? -osdSize14 - osdSize : osdSize14 + osdSize;

    COLORREF osdLineColor = Settings_GetOsdLineColor();
    int osdLineWidth = Settings_GetOsdLineWidth();
    HPEN hPenOsd = ::CreatePen(PS_SOLID, osdLineWidth, osdLineColor);
    HGDIOBJ oldPen = ::SelectObject(hdc, hPenOsd);
    HGDIOBJ oldBrush = ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));

    if (g_okEmulatorRunning)
    {
        MoveToEx(hdc, osdLeft, osdTop, NULL);
        LineTo(hdc, osdLeft + osdSize, osdTop + osdSize12);
        LineTo(hdc, osdLeft, osdTop + osdSize);
        LineTo(hdc, osdLeft, osdTop);
    }
    else
    {
        Rectangle(hdc, osdLeft, osdTop, osdLeft + osdSize13, osdTop + osdSize);
        Rectangle(hdc, osdLeft + osdSize23, osdTop, osdLeft + osdSize, osdTop + osdSize);
    }

    osdTop += osdNext;
    if (g_pBoard->IsFloppyEngineOn())
    {
        Rectangle(hdc, osdLeft, osdTop, osdLeft + osdSize, osdTop + osdSize);
        Arc(hdc, osdLeft + osdSize14, osdTop + osdSize14, osdLeft + osdSize34, osdTop + osdSize34,
            osdLeft + osdSize14, osdTop + osdSize12, osdLeft + osdSize12, osdTop + osdSize14);
        //MoveToEx(hdc, osdLeft + osdSize12, osdTop + osdSize14, NULL);
        //LineTo(hdc, osdLeft + osdSize34, osdTop + osdSize14);
        //MoveToEx(hdc, osdLeft + osdSize12, osdTop + osdSize14, NULL);
        //LineTo(hdc, osdLeft + osdSize23, osdTop + osdSize12);
        //TODO
    }

    osdTop += osdNext;
    if (Emulator_IsSound())
    {
        const double sin135 = 0.707106781186548;  // sin(135 degree)
        int osdSize12sin135 = (int)(osdSize12 * sin135);
        MoveToEx(hdc, osdLeft + osdSize13, osdTop + osdSize13, NULL);
        LineTo(hdc, osdLeft, osdTop + osdSize13);
        LineTo(hdc, osdLeft, osdTop + osdSize23);
        LineTo(hdc, osdLeft + osdSize13, osdTop + osdSize23);
        MoveToEx(hdc, osdLeft + osdSize13, osdTop + osdSize13, NULL);
        LineTo(hdc, osdLeft + osdSize23, osdTop);
        LineTo(hdc, osdLeft + osdSize23, osdTop + osdSize);
        LineTo(hdc, osdLeft + osdSize13, osdTop + osdSize23);
        Arc(hdc, osdLeft, osdTop, osdLeft + osdSize, osdTop + osdSize,
            osdLeft + osdSize12 + osdSize12sin135, osdTop + osdSize12 + osdSize12sin135,
            osdLeft + osdSize12 + osdSize12sin135, osdTop + osdSize12 - osdSize12sin135);
    }

    ::SelectObject(hdc, oldBrush);
    ::SelectObject(hdc, oldPen);
    VERIFY(::DeleteObject(hPenOsd));
}

void ScreenView_OnDraw(HDC hdc)
{
    if (RenderDrawProc != NULL)
    {
        RenderDrawProc(m_bits, hdc);
    }

    if (!Settings_GetDebug() && Settings_GetOnScreenDisplay())
        ScreenView_DrawOsd(hdc);
}

void ScreenView_RedrawScreen()
{
    ScreenView_PrepareScreen();

    HDC hdc = GetDC(g_hwndScreen);
    ScreenView_OnDraw(hdc);
    VERIFY(::ReleaseDC(g_hwndScreen, hdc));
}

// Choose color palette depending of screen mode
const DWORD* ScreenView_GetPalette()
{
    //TODO: Вынести switch в ScreenView_SetMode()
    const DWORD* colors;
    switch (m_ScreenMode)
    {
    case RGBScreen:   colors = ScreenView_StandardRGBColors; break;
    case GrayScreen:  colors = ScreenView_GrayColors; break;
    case GRBScreen:   colors = ScreenView_StandardGRBColors; break;
    default:          colors = ScreenView_StandardRGBColors; break;
    }

    return colors;
}

#define AVERAGERGB(a, b)  ( (((a) & 0xfefefeffUL) + ((b) & 0xfefefeffUL)) >> 1 )

void ScreenView_PrepareScreen()
{
    if (m_bits == NULL) return;

    const DWORD* colors = ScreenView_GetPalette();

    Emulator_PrepareScreenRGB32(m_bits, (const uint32_t*)colors);
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

const BYTE arrPcscan2UkncscanLat[256] =    // ЛАТ
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
const BYTE arrPcscan2UkncscanRus[256] =    // РУС
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
        // Выбираем таблицу маппинга в зависимости от флага РУС/ЛАТ в УКНЦ
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
                    m_ScreenKeyState[scan] = (newstate & 128) | ukncRegister;
                }
                ukncscan = pTable[pcscan];
                if (ukncscan != 0)
                {
                    BYTE pressed = newstate & 128;
                    WORD keyevent = MAKEWORD(ukncscan, pressed);
                    ScreenView_PutKeyEventToQueue(keyevent);
                }
            }
        }
    }

    // Process the keyboard queue
    WORD keyevent;
    while ((keyevent = ScreenView_GetKeyEventFromQueue()) != 0)
    {
        BOOL pressed = ((keyevent & 0x8000) != 0);
        BYTE ukncscan = LOBYTE(keyevent);
        g_pBoard->KeyboardEvent(ukncscan, pressed);
    }
}

// External key event - e.g. from KeyboardView
void ScreenView_KeyEvent(BYTE keyscan, BOOL pressed)
{
    ScreenView_PutKeyEventToQueue(MAKEWORD(keyscan, pressed ? 128 : 0));
}

BOOL ScreenView_SaveScreenshot(LPCTSTR sFileName, int screenshotMode)
{
    ASSERT(sFileName != NULL);

    void* pBits = ::calloc(UKNC_SCREEN_WIDTH * UKNC_SCREEN_HEIGHT, 4);
    const DWORD* palette = ScreenView_GetPalette();
    Emulator_PrepareScreenRGB32(pBits, (const uint32_t*)palette);

    int scrwidth, scrheight;
    ScreenView_GetScreenshotSize(screenshotMode, &scrwidth, &scrheight);
    PREPARE_SCREENSHOT_CALLBACK callback = ScreenView_GetScreenshotCallback(screenshotMode);

    void* pScrBits = ::calloc(scrwidth * scrheight, 4);
    callback(pBits, pScrBits);
    ::free(pBits);

    LPCTSTR sFileNameExt = _tcsrchr(sFileName, _T('.'));
    BOOL result = FALSE;
    if (sFileNameExt != NULL && _tcsicmp(sFileNameExt, _T(".png")) == 0)
        result = PngFile_SaveScreenshot((const uint32_t *)pScrBits, (const uint32_t *)palette, sFileName, scrwidth, scrheight);
    else
        result = BmpFile_SaveScreenshot((const uint32_t *)pScrBits, (const uint32_t *)palette, sFileName, scrwidth, scrheight);

    ::free(pScrBits);

    return result;
}

HGLOBAL ScreenView_GetScreenshotAsDIB(int screenshotMode)
{
    void* pBits = ::calloc(UKNC_SCREEN_WIDTH * UKNC_SCREEN_HEIGHT, 4);
    if (pBits == nullptr)
        return NULL;

    const DWORD* palette = ScreenView_GetPalette();
    Emulator_PrepareScreenRGB32(pBits, (const uint32_t*)palette);

    int scrwidth, scrheight;
    ScreenView_GetScreenshotSize(screenshotMode, &scrwidth, &scrheight);
    PREPARE_SCREENSHOT_CALLBACK callback = ScreenView_GetScreenshotCallback(screenshotMode);

    void* pScrBits = ::calloc(scrwidth * scrheight, 4);
    if (pScrBits == nullptr)
    {
        ::free(pBits);
        return NULL;
    }
    callback(pBits, pScrBits);
    ::free(pBits);

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
    if (hDIB == NULL)
    {
        ::free(pScrBits);
        return NULL;
    }

    LPBYTE p = (LPBYTE) ::GlobalLock(hDIB);
    ::CopyMemory(p, &bi, sizeof(BITMAPINFOHEADER));
    p += sizeof(BITMAPINFOHEADER);
    for (int line = 0; line < scrheight; line++)
    {
        LPBYTE psrc = (LPBYTE)pScrBits + (scrheight - line - 1) * scrwidth * 4;
        ::CopyMemory(p, psrc, scrwidth * 4);
        p += scrwidth * 4;
    }
    ::GlobalUnlock(hDIB);

    ::free(pScrBits);

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
    void* pBits = ::calloc(UKNC_SCREEN_WIDTH * UKNC_SCREEN_HEIGHT, 4);
    const uint32_t* colors = (const uint32_t*)ScreenView_GrayColors;
    Emulator_PrepareScreenToText(pBits, colors);

    // Prepare font, get current font data from PPU memory
    CMemoryController* pPpuMemCtl = g_pBoard->GetPPUMemoryController();
    uint8_t fontcur[11 * 16 * 14];
    uint16_t fontaddr = 014142 + 32 * 2;
    int addrtype = 0;
    for (BYTE charidx = 0; charidx < 16 * 14; charidx++)
    {
        uint16_t charaddr = pPpuMemCtl->GetWordView(fontaddr + charidx * 2, FALSE, FALSE, &addrtype);
        for (int16_t y = 0; y < 11; y++)
        {
            uint16_t fontdata = pPpuMemCtl->GetWordView((charaddr + y) & ~1, FALSE, FALSE, &addrtype);
            if (((charaddr + y) & 1) == 1) fontdata >>= 8;
            fontcur[charidx * 11 + y] = (uint8_t)(fontdata & 0xff);
        }
    }
    // Prepare font, get standard font data from PPU memory
    uint8_t fontstd[11 * 16 * 14];
    uint16_t charstdaddr = 0120170;
    for (uint16_t idx = 0; idx < 16 * 14 * 11; idx++)
    {
        uint16_t fontdata = pPpuMemCtl->GetWordView(charstdaddr & ~1, FALSE, FALSE, &addrtype);
        if ((charstdaddr & 1) == 1) fontdata >>= 8;
        fontstd[idx] = (uint8_t)(fontdata & 0xff);
        charstdaddr++;
    }

    // Loop for lines
    int charidx = 0;
    int y = 0;
    while (y <= 288 - 11)
    {
        uint32_t * pCharBits = ((uint32_t*)pBits) + y * 640;

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

    ::free(pBits);

    return TRUE;
}


//////////////////////////////////////////////////////////////////////


void CALLBACK PrepareScreenCopy(const void * pSrcBits, void * pDestBits);
void CALLBACK PrepareScreenUpscale(const void * pSrcBits, void * pDestBits);
void CALLBACK PrepareScreenUpscale2(const void * pSrcBits, void * pDestBits);
void CALLBACK PrepareScreenUpscale2d(const void * pSrcBits, void * pDestBits);
void CALLBACK PrepareScreenUpscale3(const void * pSrcBits, void * pDestBits);
void CALLBACK PrepareScreenUpscale4(const void * pSrcBits, void * pDestBits);
void CALLBACK PrepareScreenUpscale175(const void * pSrcBits, void * pDestBits);
void CALLBACK PrepareScreenUpscale5(const void * pSrcBits, void * pDestBits);

struct ScreenshotModeStruct
{
    int width;
    int height;
    PREPARE_SCREENSHOT_CALLBACK callback;
    LPCTSTR description;
}
static ScreenshotModeReference[] =
{
    {  640,  288, PrepareScreenCopy,        _T("640 x 288 Standard") },
    {  640,  432, PrepareScreenUpscale,     _T("640 x 432 Upscaled to 1.5") },
    {  640,  576, PrepareScreenUpscale2,    _T("640 x 576 Interlaced") },
    {  640,  576, PrepareScreenUpscale2d,   _T("640 x 576 Doubled") },
    {  960,  576, PrepareScreenUpscale3,    _T("960 x 576 Interlaced") },
    {  960,  720, PrepareScreenUpscale4,    _T("960 x 720, 4:3") },
    { 1120,  864, PrepareScreenUpscale175,  _T("1120 x 864 Interlaced") },
    { 1280,  864, PrepareScreenUpscale5,    _T("1280 x 864 Interlaced") },
};
const int ScreenshotModeCount = sizeof(ScreenshotModeReference) / sizeof(ScreenshotModeStruct);

void ScreenView_GetScreenshotSize(int screenshotMode, int* pwid, int* phei)
{
    if (screenshotMode < 0 || screenshotMode >= ScreenshotModeCount)
        screenshotMode = 1;
    ScreenshotModeStruct* pinfo = ScreenshotModeReference + screenshotMode;
    *pwid = pinfo->width;
    *phei = pinfo->height;
}

PREPARE_SCREENSHOT_CALLBACK ScreenView_GetScreenshotCallback(int screenshotMode)
{
    if (screenshotMode < 0 || screenshotMode >= ScreenshotModeCount)
        screenshotMode = 1;
    ScreenshotModeStruct* pinfo = ScreenshotModeReference + screenshotMode;
    return pinfo->callback;
}

LPCTSTR ScreenView_GetScreenshotModeName(int screenshotMode)
{
    if (screenshotMode < 0 || screenshotMode >= ScreenshotModeCount)
        return NULL;
    ScreenshotModeStruct* pinfo = ScreenshotModeReference + screenshotMode;
    return pinfo->description;
}

#define AVERAGERGB(a, b)  ( (((a) & 0xfefefeffUL) + ((b) & 0xfefefeffUL)) >> 1 )

void CALLBACK PrepareScreenCopy(const void * pSrcBits, void * pDestBits)
{
    for (int line = 0; line < UKNC_SCREEN_HEIGHT; line++)
    {
        DWORD * pSrc = ((DWORD*)pSrcBits) + UKNC_SCREEN_WIDTH * line;
        DWORD * pDest = ((DWORD*)pDestBits) + UKNC_SCREEN_WIDTH * line;
        ::memcpy(pDest, pSrc, UKNC_SCREEN_WIDTH * 4);
    }
    //::memcpy(pDestBits, pSrcBits, UKNC_SCREEN_WIDTH * UKNC_SCREEN_HEIGHT * 4);
}

// Upscale screen from height 288 to 432
void CALLBACK PrepareScreenUpscale(const void * pSrcBits, void * pDestBits)
{
    int ukncline = 0;
    for (int line = 0; line < 432; line++)
    {
        DWORD* pdest = ((DWORD*)pDestBits) + line * UKNC_SCREEN_WIDTH;
        if (line % 3 == 1)
        {
            DWORD* psrc1 = ((DWORD*)pSrcBits) + (ukncline - 1) * UKNC_SCREEN_WIDTH;
            DWORD* psrc2 = ((DWORD*)pSrcBits) + (ukncline + 0) * UKNC_SCREEN_WIDTH;
            DWORD* pdst1 = (DWORD*)pdest;
            for (int i = 0; i < UKNC_SCREEN_WIDTH; i++)
            {
                *pdst1 = AVERAGERGB(*psrc1, *psrc2);
                psrc1++;  psrc2++;  pdst1++;
            }
        }
        else
        {
            DWORD* psrc = ((DWORD*)pSrcBits) + ukncline * UKNC_SCREEN_WIDTH;
            memcpy(pdest, psrc, UKNC_SCREEN_WIDTH * 4);
            ukncline++;
        }
    }
}

// Upscale screen to twice height with "interlaced" effect
void CALLBACK PrepareScreenUpscale2(const void * pSrcBits, void * pDestBits)
{
    for (int ukncline = 0; ukncline < UKNC_SCREEN_HEIGHT; ukncline++)
    {
        DWORD* psrc = ((DWORD*)pSrcBits) + ukncline * UKNC_SCREEN_WIDTH;
        DWORD* pdest = ((DWORD*)pDestBits) + (ukncline * 2) * UKNC_SCREEN_WIDTH;
        memcpy(pdest, psrc, UKNC_SCREEN_WIDTH * 4);

        pdest += UKNC_SCREEN_WIDTH;
        memset(pdest, 0, UKNC_SCREEN_WIDTH * 4);
    }
}

// Upscale screen to twice height
void CALLBACK PrepareScreenUpscale2d(const void * pSrcBits, void * pDestBits)
{
    for (int ukncline = 0; ukncline < UKNC_SCREEN_HEIGHT; ukncline++)
    {
        DWORD* psrc = ((DWORD*)pSrcBits) + ukncline * UKNC_SCREEN_WIDTH;
        DWORD* pdest = ((DWORD*)pDestBits) + (ukncline * 2) * UKNC_SCREEN_WIDTH;
        memcpy(pdest, psrc, UKNC_SCREEN_WIDTH * 4);

        pdest += UKNC_SCREEN_WIDTH;
        memcpy(pdest, psrc, UKNC_SCREEN_WIDTH * 4);
    }
}

// Upscale screen width 640->960, height 288->720
void CALLBACK PrepareScreenUpscale4(const void * pSrcBits, void * pDestBits)
{
    for (int ukncline = 0; ukncline < UKNC_SCREEN_HEIGHT; ukncline += 2)
    {
        DWORD* psrc1 = ((DWORD*)pSrcBits) + ukncline * UKNC_SCREEN_WIDTH;
        DWORD* psrc2 = psrc1 + UKNC_SCREEN_WIDTH;
        DWORD* pdest0 = ((DWORD*)pDestBits) + ukncline / 2 * 5 * 960;
        DWORD* pdest1 = pdest0 + 960;
        DWORD* pdest2 = pdest1 + 960;
        DWORD* pdest3 = pdest2 + 960;
        DWORD* pdest4 = pdest3 + 960;
        for (int i = 0; i < UKNC_SCREEN_WIDTH / 2; i++)
        {
            DWORD c1a = *(psrc1++);  DWORD c1b = *(psrc1++);
            DWORD c2a = *(psrc2++);  DWORD c2b = *(psrc2++);
            DWORD c1 = AVERAGERGB(c1a, c1b);
            DWORD c2 = AVERAGERGB(c2a, c2b);
            DWORD ca = AVERAGERGB(c1a, c2a);
            DWORD cb = AVERAGERGB(c1b, c2b);
            DWORD c  = AVERAGERGB(ca,  cb);
            (*pdest0++) = c1a;  (*pdest0++) = c1;  (*pdest0++) = c1b;
            (*pdest1++) = c1a;  (*pdest1++) = c1;  (*pdest1++) = c1b;
            (*pdest2++) = ca;   (*pdest2++) = c;   (*pdest2++) = cb;
            (*pdest3++) = c2a;  (*pdest3++) = c2;  (*pdest3++) = c2b;
            (*pdest4++) = c2a;  (*pdest4++) = c2;  (*pdest4++) = c2b;
        }
    }
}

// Upscale screen width 640->960, height 288->576 with "interlaced" effect
void CALLBACK PrepareScreenUpscale3(const void * pSrcBits, void * pDestBits)
{
    for (int ukncline = UKNC_SCREEN_HEIGHT - 1; ukncline >= 0; ukncline--)
    {
        DWORD* psrc = ((DWORD*)pSrcBits) + ukncline * UKNC_SCREEN_WIDTH;
        psrc += UKNC_SCREEN_WIDTH - 1;
        DWORD* pdest = ((DWORD*)pDestBits) + (ukncline * 2) * 960;
        pdest += 960 - 1;
        for (int i = 0; i < UKNC_SCREEN_WIDTH / 2; i++)
        {
            DWORD c1 = *psrc;  psrc--;
            DWORD c2 = *psrc;  psrc--;
            DWORD c12 = AVERAGERGB(c1, c2);
            *pdest = c1;  pdest--;
            *pdest = c12; pdest--;
            *pdest = c2;  pdest--;
        }

        pdest += 960;
        memset(pdest, 0, 960 * 4);
    }
}

// Upscale screen width 640->1120 (x1.75), height 288->864 (x3) with "interlaced" effect
void CALLBACK PrepareScreenUpscale175(const void * pSrcBits, void * pDestBits)
{
    for (int ukncline = 0; ukncline < UKNC_SCREEN_HEIGHT; ukncline++)
    {
        DWORD* psrc = ((DWORD*)pSrcBits) + ukncline * UKNC_SCREEN_WIDTH;
        DWORD* pdest1 = ((DWORD*)pDestBits) + ukncline * 3 * 1120;
        DWORD* pdest2 = pdest1 + 1120;
        //DWORD* pdest3 = pdest2 + 1120;
        for (int i = 0; i < UKNC_SCREEN_WIDTH / 4; i++)
        {
            DWORD c1 = *(psrc++);
            DWORD c2 = *(psrc++);
            DWORD c3 = *(psrc++);
            DWORD c4 = *(psrc++);

            *(pdest1++) = *(pdest2++) = c1;
            *(pdest1++) = *(pdest2++) = AVERAGERGB(c1, c2);
            *(pdest1++) = *(pdest2++) = c2;
            *(pdest1++) = *(pdest2++) = AVERAGERGB(c2, c3);
            *(pdest1++) = *(pdest2++) = c3;
            *(pdest1++) = *(pdest2++) = AVERAGERGB(c3, c4);
            *(pdest1++) = *(pdest2++) = c4;
        }
    }
}

// Upscale screen width 640->1280, height 288->864 with "interlaced" effect
void CALLBACK PrepareScreenUpscale5(const void * pSrcBits, void * pDestBits)
{
    for (int ukncline = 0; ukncline < UKNC_SCREEN_HEIGHT; ukncline++)
    {
        DWORD* psrc = ((DWORD*)pSrcBits) + ukncline * UKNC_SCREEN_WIDTH;
        DWORD* pdest = ((DWORD*)pDestBits) + ukncline * 3 * 1280;
        psrc += UKNC_SCREEN_WIDTH - 1;
        pdest += 1280 - 1;
        DWORD* pdest2 = pdest + 1280;
        DWORD* pdest3 = pdest2 + 1280;
        for (int i = 0; i < UKNC_SCREEN_WIDTH; i++)
        {
            DWORD color = *psrc;  psrc--;
            *pdest = color;  pdest--;
            *pdest = color;  pdest--;
            *pdest2 = color;  pdest2--;
            *pdest2 = color;  pdest2--;
            *pdest3 = 0;  pdest3--;
            *pdest3 = 0;  pdest3--;
        }
    }
}


//////////////////////////////////////////////////////////////////////
