/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// SpriteView.cpp

#include "stdafx.h"
#include <vfw.h>
#include "Main.h"
#include "Views.h"
#include "ToolWindow.h"
#include "Dialogs.h"
#include "Emulator.h"
#include "emubase\Emubase.h"

//////////////////////////////////////////////////////////////////////


HWND g_hwndSprite = (HWND)INVALID_HANDLE_VALUE;  // Sprite view window handler
WNDPROC m_wndprocSpriteToolWindow = NULL;  // Old window proc address of the ToolWindow

HWND m_hwndSpriteViewer = (HWND)INVALID_HANDLE_VALUE;
HDRAWDIB m_hSpriteDrawDib = NULL;
BITMAPINFO m_bmpinfoSprite;
HBITMAP m_hSpriteBitmap = NULL;
DWORD * m_pSprite_bits = NULL;

const int m_nSprite_scale = 2;
const int m_nSprite_ImageCX = 262;
const int m_nSprite_ImageCY = 256;
const int m_nSprite_ViewCX = m_nSprite_ImageCX * m_nSprite_scale;
const int m_nSprite_ViewCY = m_nSprite_ImageCY * m_nSprite_scale;

WORD m_wSprite_BaseAddress = 0;
int m_nSprite_width = 2;
const int m_nSprite_format_max = 1;
int m_nSprite_format = 0;
int m_nSprite_PageSizeBytes = m_nSprite_ImageCY * (m_nSprite_ImageCX / (8 + 2));

void SpriteView_OnDraw(HDC hdc);
BOOL SpriteView_OnKeyDown(WPARAM vkey, LPARAM lParam);
BOOL SpriteView_OnVScroll(WPARAM wParam, LPARAM lParam);
BOOL SpriteView_OnHScroll(WPARAM wParam, LPARAM lParam);
BOOL SpriteView_OnMouseWheel(WPARAM wParam, LPARAM lParam);
void SpriteView_InitBitmap();
void SpriteView_DoneBitmap();
void SpriteView_UpdateWindowText();
void SpriteView_PrepareBitmap();
void SpriteView_UpdateScrollPos();

int m_nSprite_Mode = 0;
const int m_nSprite_ModeMax = 4;
static LPCTSTR SpriteModeNames[] =
{
    _T("Black and White 8 bits"),
    _T("Green and Red on Black"),
    _T("Cyan and Magenta on Blue"),
    _T("Red and Green on Black"),
    _T("Magenta and Cyan on Blue")
};
static DWORD SpriteModePalettes[][3] =
{
    { 0x000000, 0xFFFFFF, 0x000000 },
    { 0x000000, 0x00FF00, 0xFF0000 },
    { 0x0000FF, 0x00FFFF, 0xFF00FF },
    { 0x000000, 0xFF0000, 0x00FF00 },
    { 0x0000FF, 0xFF00FF, 0x00FFFF },
};

//////////////////////////////////////////////////////////////////////

void SpriteView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = SpriteViewViewerWndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = g_hInst;
    wcex.hIcon = NULL;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = CLASSNAME_SPRITEVIEW;
    wcex.hIconSm = NULL;

    RegisterClassEx(&wcex);
}

void SpriteView_Create(int x, int y)
{
    int cxBorder = ::GetSystemMetrics(SM_CXDLGFRAME);
    int cyBorder = ::GetSystemMetrics(SM_CYDLGFRAME);
    int cxScroll = ::GetSystemMetrics(SM_CXVSCROLL);
    int cyCaption = ::GetSystemMetrics(SM_CYSMCAPTION);

    int width = m_nSprite_ViewCX + cxScroll + cxBorder * 2;
    int height = m_nSprite_ViewCY + cyBorder * 2 + cyCaption;
    g_hwndSprite = CreateWindowEx(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            CLASSNAME_OVERLAPPEDWINDOW, _T("Sprite Viewer"),
            WS_POPUPWINDOW | WS_CAPTION | WS_VISIBLE,
            x, y, width, height,
            NULL, NULL, g_hInst, NULL);

    // ToolWindow subclassing
    m_wndprocSpriteToolWindow = (WNDPROC)LongToPtr(SetWindowLongPtr(
            g_hwndSprite, GWLP_WNDPROC, PtrToLong(SpriteViewWndProc)));

    RECT rcClient;  GetClientRect(g_hwndSprite, &rcClient);

    m_hwndSpriteViewer = CreateWindow(
            CLASSNAME_SPRITEVIEW, NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP,
            0, 0, rcClient.right, rcClient.bottom,
            g_hwndSprite, NULL, g_hInst, NULL);

    m_wSprite_BaseAddress = Settings_GetSpriteAddress();
    m_nSprite_width = (int)Settings_GetSpriteWidth();
    if (m_nSprite_width <= 0) m_nSprite_width = 1;
    if (m_nSprite_width > 32) m_nSprite_width = 32;
    m_nSprite_Mode = Settings_GetSpriteMode();
    if (m_nSprite_Mode > m_nSprite_ModeMax) m_nSprite_Mode = m_nSprite_ModeMax;

    SpriteView_InitBitmap();
    SpriteView_UpdateWindowText();
    SpriteView_UpdateScrollPos();
    ::SetFocus(m_hwndSpriteViewer);
}

void SpriteView_InitBitmap()
{
    m_hSpriteDrawDib = DrawDibOpen();
    HDC hdc = GetDC(g_hwnd);

    m_bmpinfoSprite.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    m_bmpinfoSprite.bmiHeader.biWidth = m_nSprite_ImageCX;
    m_bmpinfoSprite.bmiHeader.biHeight = m_nSprite_ImageCY;
    m_bmpinfoSprite.bmiHeader.biPlanes = 1;
    m_bmpinfoSprite.bmiHeader.biBitCount = 32;
    m_bmpinfoSprite.bmiHeader.biCompression = BI_RGB;
    m_bmpinfoSprite.bmiHeader.biSizeImage = 0;
    m_bmpinfoSprite.bmiHeader.biXPelsPerMeter = 0;
    m_bmpinfoSprite.bmiHeader.biYPelsPerMeter = 0;
    m_bmpinfoSprite.bmiHeader.biClrUsed = 0;
    m_bmpinfoSprite.bmiHeader.biClrImportant = 0;

    m_hSpriteBitmap = CreateDIBSection(hdc, &m_bmpinfoSprite, DIB_RGB_COLORS, (void **)&m_pSprite_bits, NULL, 0);

    ReleaseDC(g_hwnd, hdc);
}

void SpriteView_DoneBitmap()
{
    if (m_hSpriteBitmap != NULL)
    {
        DeleteObject(m_hSpriteBitmap);  m_hSpriteBitmap = NULL;
    }

    DrawDibClose(m_hSpriteDrawDib);
}

LRESULT CALLBACK SpriteViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_SETFOCUS:
        ::SetFocus(m_hwndSpriteViewer);
        break;
    case WM_DESTROY:
        g_hwndSprite = (HWND)INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
        return CallWindowProc(m_wndprocSpriteToolWindow, hWnd, message, wParam, lParam);
    default:
        return CallWindowProc(m_wndprocSpriteToolWindow, hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

LRESULT CALLBACK SpriteViewViewerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            SpriteView_OnDraw(hdc);  // Draw memory dump

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        // Free resources
        SpriteView_DoneBitmap();
        return DefWindowProc(hWnd, message, wParam, lParam);
    case WM_LBUTTONDOWN:
        SetFocus(hWnd);
        break;
    case WM_KEYDOWN:
        return (LRESULT)SpriteView_OnKeyDown(wParam, lParam);
    case WM_VSCROLL:
        return (LRESULT)SpriteView_OnVScroll(wParam, lParam);
    case WM_MOUSEWHEEL:
        return (LRESULT)SpriteView_OnMouseWheel(wParam, lParam);
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

void SpriteView_OnDraw(HDC hdc)
{
    ASSERT(g_pBoard != NULL);

    SpriteView_PrepareBitmap();

    DrawDibDraw(m_hSpriteDrawDib, hdc,
            0, 0,
            m_nSprite_ImageCX * m_nSprite_scale, m_nSprite_ImageCY * m_nSprite_scale,
            &m_bmpinfoSprite.bmiHeader, m_pSprite_bits, 0, 0,
            m_nSprite_ImageCX, m_nSprite_ImageCY,
            0);
}

void SpriteView_GoToAddress(WORD address)
{
    if (address == m_wSprite_BaseAddress)
        return;

    m_wSprite_BaseAddress = address;
    Settings_SetSpriteAddress(m_wSprite_BaseAddress);

    SpriteView_UpdateWindowText();
    SpriteView_PrepareBitmap();
    InvalidateRect(m_hwndSpriteViewer, NULL, TRUE);
    SpriteView_UpdateScrollPos();
}
void SpriteView_SetSpriteWidth(int width)
{
    if (width < 1) width = 1;
    if (width > m_nSprite_ImageCX / 8) width = m_nSprite_ImageCX / 8;
    if (width == m_nSprite_width)
        return;

    m_nSprite_width = width;
    Settings_SetSpriteWidth((WORD)width);

    SpriteView_UpdateWindowText();
    SpriteView_PrepareBitmap();
    InvalidateRect(m_hwndSpriteViewer, NULL, TRUE);
    SpriteView_UpdateScrollPos();
}

void SpriteView_UpdateWindowText()
{
    LPCTSTR spriteModeName = _T("");
    if (m_nSprite_Mode <= m_nSprite_ModeMax)
        spriteModeName = SpriteModeNames[m_nSprite_Mode];

    const size_t buffer_size = 128;
    TCHAR buffer[buffer_size];
    _stprintf_s(buffer, buffer_size,
            _T("Sprite Viewer - address %06o, width %d, mode%d %s"),
            m_wSprite_BaseAddress, m_nSprite_width, m_nSprite_Mode, spriteModeName);
    ::SetWindowText(g_hwndSprite, buffer);
}

BOOL SpriteView_OnKeyDown(WPARAM vkey, LPARAM /*lParam*/)
{
    switch (vkey)
    {
    case VK_LEFT:
    case VK_RIGHT:
        {
            int spriteMult = ((m_nSprite_Mode == 0) ? 1 : 2) * (vkey == VK_LEFT ? -1 : 1);
            SpriteView_GoToAddress(m_wSprite_BaseAddress + (WORD)(m_nSprite_ImageCY * spriteMult * m_nSprite_width));
            break;
        }
    case VK_UP:
    case VK_DOWN:
        {
            int spriteMult = ((m_nSprite_Mode == 0) ? 1 : 2) * (vkey == VK_UP ? -1 : 1);
            if (GetKeyState(VK_CONTROL) & 0x8000)
                SpriteView_GoToAddress(m_wSprite_BaseAddress + (WORD)(spriteMult * m_nSprite_width));
            else
                SpriteView_GoToAddress((WORD)(m_wSprite_BaseAddress + spriteMult));
            break;
        }
    case VK_OEM_4: // '[' -- Decrement Sprite Width
        SpriteView_SetSpriteWidth(m_nSprite_width - 1);
        break;
    case VK_OEM_6: // ']' -- Increment Sprite Width
        SpriteView_SetSpriteWidth(m_nSprite_width + 1);
        break;
    case 0x4D: // 'M' - Switch sprite decode mode
        if (m_nSprite_Mode == m_nSprite_ModeMax)
            m_nSprite_Mode = 0;
        else
            ++m_nSprite_Mode;
        Settings_SetSpriteMode((WORD)m_nSprite_Mode);

        SpriteView_UpdateWindowText();
        SpriteView_PrepareBitmap();
        InvalidateRect(m_hwndSpriteViewer, NULL, TRUE);
        SpriteView_UpdateScrollPos();

        break;
    case 0x47:  // 'G' - Go To Address
        {
            WORD value = m_wSprite_BaseAddress;
            if (InputBoxOctal(m_hwndSpriteViewer, _T("Go To Address"), &value))
                SpriteView_GoToAddress(value);
            break;
        }
    case VK_HOME:
        SpriteView_GoToAddress(0);
        break;
    case VK_END:
        SpriteView_GoToAddress((WORD)(0x10000 - ((m_nSprite_Mode == 0) ? 1 : 2)));
        break;
    case VK_PRIOR:
        SpriteView_GoToAddress(m_wSprite_BaseAddress - (WORD)m_nSprite_PageSizeBytes);
        break;
    case VK_NEXT:
        SpriteView_GoToAddress(m_wSprite_BaseAddress + (WORD)m_nSprite_PageSizeBytes);
        break;
    default:
        return TRUE;
    }
    return FALSE;
}

BOOL SpriteView_OnMouseWheel(WPARAM wParam, LPARAM /*lParam*/)
{
    short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

    int nDelta = zDelta / 120;
    if (nDelta > 4) nDelta = 4;
    if (nDelta < -4) nDelta = -4;

    int spriteWidth = ((m_nSprite_Mode == 0) ? 1 : 2) * m_nSprite_width;
    SpriteView_GoToAddress(m_wSprite_BaseAddress - (WORD)(nDelta * 3 * spriteWidth));

    return FALSE;
}

BOOL SpriteView_OnVScroll(WPARAM wParam, LPARAM /*lParam*/)
{
    int spriteWidth = ((m_nSprite_Mode == 0) ? 1 : 2) * m_nSprite_width;

    WORD scrollcmd = LOWORD(wParam);
    switch (scrollcmd)
    {
    case SB_LINEDOWN:
        SpriteView_GoToAddress(m_wSprite_BaseAddress + (WORD)spriteWidth);
        break;
    case SB_LINEUP:
        SpriteView_GoToAddress(m_wSprite_BaseAddress - (WORD)spriteWidth);
        break;
    case SB_PAGEDOWN:
        SpriteView_GoToAddress(m_wSprite_BaseAddress + (WORD)m_nSprite_PageSizeBytes);
        break;
    case SB_PAGEUP:
        SpriteView_GoToAddress(m_wSprite_BaseAddress - (WORD)m_nSprite_PageSizeBytes);
        break;
    case SB_THUMBPOSITION:
        {
            WORD scrollpos = HIWORD(wParam);
            if (m_nSprite_Mode != 0) scrollpos = scrollpos & ~1;
            SpriteView_GoToAddress(scrollpos);
        }
        break;
    }

    return FALSE;
}

void SpriteView_UpdateScrollPos()
{
    int columns = 0;
    int cx = m_nSprite_ImageCX;
    while (cx > m_nSprite_width * 8)
    {
        columns++;
        cx -= m_nSprite_width * 8;
        if (cx <= 2)
            break;
        cx -= 2;
    }
    int stepSizeBytes = ((m_nSprite_Mode == 0) ? 1 : 2);
    m_nSprite_PageSizeBytes = m_nSprite_ImageCY * columns * m_nSprite_width * stepSizeBytes;

    SCROLLINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
    si.nPage = m_nSprite_PageSizeBytes;
    si.nPos = m_wSprite_BaseAddress;
    si.nMin = 0;
    si.nMax = 0x10000 - stepSizeBytes + m_nSprite_PageSizeBytes;
    SetScrollInfo(m_hwndSpriteViewer, SB_VERT, &si, TRUE);
}

void SpriteView_PrepareBitmap()
{
    // Clear bitmap
    memset(m_pSprite_bits, 0x7f, m_nSprite_ImageCX * m_nSprite_ImageCY * 4);

    WORD address = m_wSprite_BaseAddress;

    for (int x = 0; x + m_nSprite_width * 8 <= m_nSprite_ImageCX; x += m_nSprite_width * 8 + 2)
    {
        for (int y = 0; y < m_nSprite_ImageCY; y++)
        {
            DWORD* pBits = m_pSprite_bits + (m_nSprite_ImageCY - 1 - y) * m_nSprite_ImageCX + x;

            BOOL okHalt = g_pBoard->GetCPU()->IsHaltMode();
            for (int w = 0; w < m_nSprite_width; w++)
            {
                if (m_nSprite_Mode == 0)
                {
                    // Get byte from memory -- CPU memory only for now
                    int addrtype = 0;
                    WORD value = g_pBoard->GetCPUMemoryController()->GetWordView(address & ~1, okHalt, FALSE, &addrtype);
                    if (address & 1)
                        value = value >> 8;
                    ++address;

                    for (int i = 0; i < 8; ++i)
                    {
                        COLORREF color = (value & 1) ? 0xffffff : 0;
                        *pBits = color;
                        pBits++;
                        value = value >> 1;
                    }
                }
                else
                {
                    int addrtype = 0;
                    WORD value = g_pBoard->GetCPUMemoryController()->GetWordView(address & ~1, okHalt, FALSE, &addrtype);
                    if (address & 1)
                        value = value >> 8;
                    ++address;
                    WORD value1 = g_pBoard->GetCPUMemoryController()->GetWordView(address & ~1, okHalt, FALSE, &addrtype);
                    if (address & 1)
                        value1 = value1 >> 8;
                    ++address;

                    DWORD background = SpriteModePalettes[m_nSprite_Mode][0];
                    for (int i = 0; i < 8; ++i)
                    {
                        COLORREF color = background;
                        color |= (value & 1) ? SpriteModePalettes[m_nSprite_Mode][1] : 0;
                        color |= (value1 & 1) ? SpriteModePalettes[m_nSprite_Mode][2] : 0;
                        *pBits = color;
                        pBits++;
                        value = value >> 1;
                        value1 = value1 >> 1;
                    }
                }
            }
        }
    }
}


//////////////////////////////////////////////////////////////////////
