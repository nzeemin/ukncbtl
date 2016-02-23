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
#include "UKNCBTL.h"
#include "Views.h"
#include "Emulator.h"
#include "util\BitmapFile.h"


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

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= ScreenViewWndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= g_hInst;
    wcex.hIcon			= NULL;
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= NULL; //(HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= CLASSNAME_SCREENVIEW;
    wcex.hIconSm		= NULL;

    RegisterClassEx(&wcex);
}

void ScreenView_Init()
{
    m_bits = (DWORD*) ::malloc(UKNC_SCREEN_WIDTH * UKNC_SCREEN_HEIGHT * 4);
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
void CreateScreenView(HWND hwndParent, int x, int y)
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
        else
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

void ScreenView_OnDraw(HDC hdc)
{
    if (RenderDrawProc != NULL)
    {
        RenderDrawProc(m_bits, hdc);
    }
}

void ScreenView_RedrawScreen()
{
    ScreenView_PrepareScreen();

    HDC hdc = GetDC(g_hwndScreen);
    ScreenView_OnDraw(hdc);
    ::ReleaseDC(g_hwndScreen, hdc);
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

    Emulator_PrepareScreenRGB32(m_bits, colors);
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
        BYTE ukncRegister = (BYTE) g_pBoard->GetKeyboardRegister();
        const BYTE* pTable;

        // Check every key for state change
        for (int scan = 0; scan < 256; scan++)
        {
            BYTE newstate = keys[scan];
            BYTE oldstate = m_ScreenKeyState[scan];
            if ((newstate & 128) != (oldstate & 128))  // Key state changed - key pressed or released
            {
                BYTE pcscan = (BYTE) scan;
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

BOOL ScreenView_SaveScreenshot(LPCTSTR sFileName)
{
    ASSERT(sFileName != NULL);

    DWORD* pBits = (DWORD*) ::malloc(UKNC_SCREEN_WIDTH * UKNC_SCREEN_HEIGHT * 4);
    const DWORD* colors = ScreenView_GetPalette();
    Emulator_PrepareScreenRGB32(pBits, colors);

    const DWORD * palette = ScreenView_GetPalette();

    LPCTSTR sFileNameExt = _tcsrchr(sFileName, _T('.'));
    BOOL result = FALSE;
    if (sFileNameExt != NULL && _tcsicmp(sFileNameExt, _T(".png")) == 0)
        result = PngFile_SaveScreenshot((const uint32_t *)pBits, (const uint32_t *)palette, sFileName);
    else
        result = BmpFile_SaveScreenshot((const uint32_t *)pBits, (const uint32_t *)palette, sFileName);

    ::free(pBits);

    return result;
}


//////////////////////////////////////////////////////////////////////
