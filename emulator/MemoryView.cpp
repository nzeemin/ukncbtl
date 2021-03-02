/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// MemoryView.cpp

#include "stdafx.h"
#include <commctrl.h>
#include "Main.h"
#include "Views.h"
#include "ToolWindow.h"
#include "Dialogs.h"
#include "Emulator.h"
#include "emubase\Emubase.h"

//////////////////////////////////////////////////////////////////////


HWND g_hwndMemory = (HWND) INVALID_HANDLE_VALUE;  // Memory view window handler
WNDPROC m_wndprocMemoryToolWindow = NULL;  // Old window proc address of the ToolWindow

int m_cyLineMemory = 0;  // Line height in pixels
int m_nPageSize = 0;  // Page size in lines

HWND m_hwndMemoryViewer = (HWND) INVALID_HANDLE_VALUE;
HWND m_hwndMemoryToolbar = (HWND) INVALID_HANDLE_VALUE;

void MemoryView_OnDraw(HDC hdc);
BOOL MemoryView_OnKeyDown(WPARAM vkey, LPARAM lParam);
BOOL MemoryView_OnMouseWheel(WPARAM wParam, LPARAM lParam);
BOOL MemoryView_OnVScroll(WPARAM wParam, LPARAM lParam);
void MemoryView_ScrollTo(WORD wAddress);
void MemoryView_Scroll(int nDeltaLines);
void MemoryView_UpdateScrollPos();
void MemoryView_UpdateWindowText();
void MemoryView_UpdateToolbar();
LPCTSTR MemoryView_GetMemoryModeName();
void MemoryView_AdjustWindowLayout();

int     m_Mode = MEMMODE_ROM;  // See MemoryViewMode enum
WORD    m_wBaseAddress = 0;
BOOL    m_okMemoryByteMode = FALSE;


//////////////////////////////////////////////////////////////////////


void MemoryView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = MemoryViewViewerWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInst;
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = CLASSNAME_MEMORYVIEW;
    wcex.hIconSm        = NULL;

    RegisterClassEx(&wcex);
}

void MemoryView_Create(HWND hwndParent, int x, int y, int width, int height)
{
    ASSERT(hwndParent != NULL);

    m_Mode = Settings_GetDebugMemoryMode();
    if (m_Mode > MEMMODE_LAST) m_Mode = MEMMODE_LAST;
    m_okMemoryByteMode = Settings_GetDebugMemoryByte();

    g_hwndMemory = CreateWindow(
            CLASSNAME_TOOLWINDOW, NULL,
            WS_CHILD | WS_VISIBLE,
            x, y, width, height,
            hwndParent, NULL, g_hInst, NULL);
    MemoryView_UpdateWindowText();

    // ToolWindow subclassing
    m_wndprocMemoryToolWindow = (WNDPROC) LongToPtr( SetWindowLongPtr(
            g_hwndMemory, GWLP_WNDPROC, PtrToLong(MemoryViewWndProc)) );

    RECT rcClient;  GetClientRect(g_hwndMemory, &rcClient);

    m_hwndMemoryViewer = CreateWindowEx(
            WS_EX_STATICEDGE,
            CLASSNAME_MEMORYVIEW, NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP,
            0, 0, rcClient.right, rcClient.bottom,
            g_hwndMemory, NULL, g_hInst, NULL);

    m_hwndMemoryToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
            WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT | TBSTYLE_TOOLTIPS | CCS_NOPARENTALIGN | CCS_NODIVIDER | CCS_VERT,
            4, 4, 32, 32 * 6, m_hwndMemoryViewer,
            (HMENU) 102,
            g_hInst, NULL);

    TBADDBITMAP addbitmap;
    addbitmap.hInst = g_hInst;
    addbitmap.nID = IDB_TOOLBAR;
    SendMessage(m_hwndMemoryToolbar, TB_ADDBITMAP, 2, (LPARAM) &addbitmap);

    SendMessage(m_hwndMemoryToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM) sizeof(TBBUTTON), 0);
    SendMessage(m_hwndMemoryToolbar, TB_SETBUTTONSIZE, 0, (LPARAM) MAKELONG (26, 26));

    TBBUTTON buttons[8];
    ZeroMemory(buttons, sizeof(buttons));
    for (int i = 0; i < sizeof(buttons) / sizeof(TBBUTTON); i++)
    {
        buttons[i].fsState = TBSTATE_ENABLED | TBSTATE_WRAP;
        buttons[i].fsStyle = BTNS_BUTTON | TBSTYLE_GROUP;
        buttons[i].iString = -1;
    }
    buttons[0].idCommand = ID_DEBUG_MEMORY_GOTO;
    buttons[0].iBitmap = ToolbarImageGotoAddress;
    buttons[1].fsStyle = BTNS_SEP;
    buttons[2].idCommand = ID_DEBUG_MEMORY_RAM;
    buttons[2].iBitmap = ToolbarImageMemoryRam;
    buttons[3].idCommand = ID_DEBUG_MEMORY_ROM;
    buttons[3].iBitmap = ToolbarImageMemoryRom;
    buttons[4].idCommand = ID_DEBUG_MEMORY_CPU;
    buttons[4].iBitmap = ToolbarImageMemoryCpu;
    buttons[5].idCommand = ID_DEBUG_MEMORY_PPU;
    buttons[5].iBitmap = ToolbarImageMemoryPpu;
    buttons[6].fsStyle = BTNS_SEP;
    buttons[7].idCommand = ID_DEBUG_MEMORY_WORDBYTE;
    buttons[7].iBitmap = ToolbarImageWordByte;

    SendMessage(m_hwndMemoryToolbar, TB_ADDBUTTONS, (WPARAM) sizeof(buttons) / sizeof(TBBUTTON), (LPARAM) &buttons);

    MemoryView_ScrollTo(Settings_GetDebugMemoryAddress());
    MemoryView_UpdateToolbar();
}

// Adjust position of client windows
void MemoryView_AdjustWindowLayout()
{
    RECT rc;  GetClientRect(g_hwndMemory, &rc);

    if (m_hwndMemoryViewer != (HWND) INVALID_HANDLE_VALUE)
        SetWindowPos(m_hwndMemoryViewer, NULL, 0, 0, rc.right, rc.bottom, SWP_NOZORDER);
}

LRESULT CALLBACK MemoryViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    LRESULT lResult;
    switch (message)
    {
    case WM_DESTROY:
        g_hwndMemory = (HWND) INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
        return CallWindowProc(m_wndprocMemoryToolWindow, hWnd, message, wParam, lParam);
    case WM_SIZE:
        lResult = CallWindowProc(m_wndprocMemoryToolWindow, hWnd, message, wParam, lParam);
        MemoryView_AdjustWindowLayout();
        return lResult;
    default:
        return CallWindowProc(m_wndprocMemoryToolWindow, hWnd, message, wParam, lParam);
    }
    //return (LRESULT)FALSE;
}

LRESULT CALLBACK MemoryViewViewerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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

            MemoryView_OnDraw(hdc);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        ::SetFocus(hWnd);
        break;
    case WM_KEYDOWN:
        return (LRESULT) MemoryView_OnKeyDown(wParam, lParam);
    case WM_MOUSEWHEEL:
        return (LRESULT) MemoryView_OnMouseWheel(wParam, lParam);
    case WM_VSCROLL:
        return (LRESULT) MemoryView_OnVScroll(wParam, lParam);
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        ::InvalidateRect(hWnd, NULL, TRUE);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

void MemoryView_OnDraw(HDC hdc)
{
    ASSERT(g_pBoard != NULL);

    HFONT hFont = CreateMonospacedFont();
    HGDIOBJ hOldFont = SelectObject(hdc, hFont);
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorText = Settings_GetColor(ColorDebugText);
    COLORREF colorChanged = Settings_GetColor(ColorDebugValueChanged);
    COLORREF colorMemoryRom = Settings_GetColor(ColorDebugMemoryRom);
    COLORREF colorMemoryIO = Settings_GetColor(ColorDebugMemoryIO);
    COLORREF colorMemoryNA = Settings_GetColor(ColorDebugMemoryNA);
    COLORREF colorOld = SetTextColor(hdc, colorText);
    COLORREF colorBkOld = SetBkColor(hdc, GetSysColor(COLOR_WINDOW));

    m_cyLineMemory = cyLine;

    TCHAR buffer[7];
    const TCHAR* ADDRESS_LINE = _T(" addr   0      2      4      6      10     12     14     16");
    TextOut(hdc, cxChar * 5, 0, ADDRESS_LINE, (int) _tcslen(ADDRESS_LINE));

    RECT rcClip;
    GetClipBox(hdc, &rcClip);
    RECT rcClient;
    GetClientRect(m_hwndMemoryViewer, &rcClient);
    m_nPageSize = rcClient.bottom / cyLine - 1;

    WORD address = m_wBaseAddress;
    int y = 1 * cyLine;
    for (;;)    // Draw lines
    {
        DrawOctalValue(hdc, 5 * cxChar, y, address);

        int x = 13 * cxChar;
        TCHAR wchars[16];
        for (int j = 0; j < 8; j++)    // Draw words as octal value
        {
            // Get word from memory
            WORD word = 0;
            int addrtype = ADDRTYPE_NONE;
            BOOL okValid = TRUE;
            BOOL okHalt = FALSE;
            WORD wChanged = 0;
            switch (m_Mode)
            {
            case MEMMODE_RAM0:
            case MEMMODE_RAM1:
            case MEMMODE_RAM2:
                word = g_pBoard->GetRAMWord(m_Mode, address);
                wChanged = Emulator_GetChangeRamStatus(m_Mode, address);
                break;
            case MEMMODE_ROM:  // ROM - only 32 Kbytes
                if (address < 0100000)
                    okValid = FALSE;
                else
                {
                    addrtype = ADDRTYPE_ROM;
                    word = g_pBoard->GetROMWord(address - 0100000);
                }
                break;
            case MEMMODE_CPU:
                okHalt = g_pBoard->GetCPU()->IsHaltMode();
                word = g_pBoard->GetCPUMemoryController()->GetWordView(address, okHalt, FALSE, &addrtype);
                okValid = (addrtype != ADDRTYPE_IO) && (addrtype != ADDRTYPE_DENY);
                wChanged = Emulator_GetChangeRamStatus(ADDRTYPE_RAM12, address);
                break;
            case MEMMODE_PPU:
                okHalt = g_pBoard->GetPPU()->IsHaltMode();
                word = g_pBoard->GetPPUMemoryController()->GetWordView(address, okHalt, FALSE, &addrtype);
                okValid = (addrtype != ADDRTYPE_IO) && (addrtype != ADDRTYPE_DENY);
                if (address < 0100000)
                    wChanged = Emulator_GetChangeRamStatus(ADDRTYPE_RAM0, address);
                break;
            }

            if (okValid)
            {
                if (addrtype == ADDRTYPE_ROM)
                    ::SetTextColor(hdc, colorMemoryRom);
                else
                    ::SetTextColor(hdc, (wChanged != 0) ? colorChanged : colorText);
                if (m_okMemoryByteMode)
                {
                    PrintOctalValue(buffer, (word & 0xff));
                    TextOut(hdc, x, y, buffer + 3, 3);
                    PrintOctalValue(buffer, (word >> 8));
                    TextOut(hdc, x + 3 * cxChar + 3, y, buffer + 3, 3);
                }
                else
                    DrawOctalValue(hdc, x, y, word);
            }
            else  // !okValid
            {
                if (addrtype == ADDRTYPE_IO)
                {
                    ::SetTextColor(hdc, colorMemoryIO);
                    TextOut(hdc, x, y, _T("  IO"), 4);
                }
                else
                {
                    ::SetTextColor(hdc, colorMemoryNA);
                    TextOut(hdc, x, y, _T("  NA"), 4);
                }
            }

            // Prepare characters to draw at right
            BYTE ch1 = LOBYTE(word);
            TCHAR wch1 = Translate_KOI8R(ch1);
            if (ch1 < 32) wch1 = _T('·');
            wchars[j * 2] = wch1;
            BYTE ch2 = HIBYTE(word);
            TCHAR wch2 = Translate_KOI8R(ch2);
            if (ch2 < 32) wch2 = _T('·');
            wchars[j * 2 + 1] = wch2;

            address += 2;
            x += 7 * cxChar;
        }
        ::SetTextColor(hdc, colorText);

        // Draw characters at right
        int xch = x + cxChar;
        TextOut(hdc, xch, y, wchars, 16);

        y += cyLine;
        if (y > rcClip.bottom) break;
    }

    SetTextColor(hdc, colorOld);
    SetBkColor(hdc, colorBkOld);
    SelectObject(hdc, hOldFont);
    VERIFY(DeleteObject(hFont));

    if (::GetFocus() == m_hwndMemoryViewer)
    {
        RECT rcFocus = rcClient;
        rcFocus.left += cxChar * 5 - 1;
        rcFocus.top += cyLine - 1;
        rcFocus.right = cxChar * (66 + 22);
        DrawFocusRect(hdc, &rcFocus);
    }
}

void MemoryView_UpdateToolbar()
{
    int command = ID_DEBUG_MEMORY_RAM;
    switch (m_Mode)
    {
    case MEMMODE_RAM0:
    case MEMMODE_RAM1:
    case MEMMODE_RAM2:
        command = ID_DEBUG_MEMORY_RAM;
        break;
    case MEMMODE_ROM:  command = ID_DEBUG_MEMORY_ROM;  break;
    case MEMMODE_CPU:  command = ID_DEBUG_MEMORY_CPU;  break;
    case MEMMODE_PPU:  command = ID_DEBUG_MEMORY_PPU;  break;
    }
    SendMessage(m_hwndMemoryToolbar, TB_CHECKBUTTON, command, TRUE);
}

void MemoryView_SetViewMode(MemoryViewMode mode)
{
    if (mode < 0) mode = MEMMODE_RAM0;
    if (mode > MEMMODE_LAST) mode = MEMMODE_LAST;
    m_Mode = mode;
    Settings_SetDebugMemoryMode((WORD)m_Mode);

    InvalidateRect(m_hwndMemoryViewer, NULL, TRUE);
    MemoryView_UpdateWindowText();

    MemoryView_UpdateToolbar();
}

void MemoryView_SwitchRamMode()
{
    if (m_Mode >= MEMMODE_RAM2)
        MemoryView_SetViewMode(MEMMODE_RAM0);
    else
        MemoryView_SetViewMode((MemoryViewMode)(m_Mode + 1));
}

LPCTSTR MemoryView_GetMemoryModeName()
{
    switch (m_Mode)
    {
    case MEMMODE_RAM0:  return _T("RAM0");
    case MEMMODE_RAM1:  return _T("RAM1");
    case MEMMODE_RAM2:  return _T("RAM2");
    case MEMMODE_ROM:   return _T("ROM");
    case MEMMODE_CPU:   return _T("CPU");
    case MEMMODE_PPU:   return _T("PPU");
    default:
        return _T("UKWN");  // Unknown mode
    }
}

void MemoryView_SwitchWordByte()
{
    m_okMemoryByteMode = !m_okMemoryByteMode;
    Settings_SetDebugMemoryByte(m_okMemoryByteMode);

    InvalidateRect(m_hwndMemoryViewer, NULL, TRUE);
}

void MemoryView_SelectAddress()
{
    WORD value = m_wBaseAddress;
    if (InputBoxOctal(m_hwndMemoryViewer, _T("Go To Address"), &value))
        MemoryView_ScrollTo(value);
}

void MemoryView_UpdateWindowText()
{
    TCHAR buffer[64];
    _stprintf_s(buffer, 64, _T("Memory - %s"), MemoryView_GetMemoryModeName());
    ::SetWindowText(g_hwndMemory, buffer);
}

BOOL MemoryView_OnKeyDown(WPARAM vkey, LPARAM /*lParam*/)
{
    switch (vkey)
    {
    case VK_ESCAPE:
        ConsoleView_Activate();
        break;
    case VK_HOME:
        MemoryView_ScrollTo(0);
        break;
    case VK_LEFT:
        MemoryView_Scroll(-2);
        break;
    case VK_RIGHT:
        MemoryView_Scroll(2);
        break;
    case VK_UP:
        MemoryView_Scroll(-16);
        break;
    case VK_DOWN:
        MemoryView_Scroll(16);
        break;
    case VK_SPACE:
        MemoryView_SetViewMode((MemoryViewMode)((m_Mode == MEMMODE_LAST) ? 0 : m_Mode + 1));
        break;
    case VK_PRIOR:
        MemoryView_Scroll(-m_nPageSize * 16);
        break;
    case VK_NEXT:
        MemoryView_Scroll(m_nPageSize * 16);
        break;
    case 0x47:  // G - Go To Address
        MemoryView_SelectAddress();
        break;
    case 0x42:  // B - change byte/word mode
    case 0x57:  // W
        MemoryView_SwitchWordByte();
        break;
    default:
        return TRUE;
    }
    return FALSE;
}

BOOL MemoryView_OnMouseWheel(WPARAM wParam, LPARAM /*lParam*/)
{
    short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

    int nDelta = zDelta / 120;
    if (nDelta > 5) nDelta = 5;
    if (nDelta < -5) nDelta = -5;

    MemoryView_Scroll(-nDelta * 2 * 16);

    return FALSE;
}

BOOL MemoryView_OnVScroll(WPARAM wParam, LPARAM /*lParam*/)
{
    WORD scrollpos = HIWORD(wParam);
    WORD scrollcmd = LOWORD(wParam);
    switch (scrollcmd)
    {
    case SB_LINEDOWN:
        MemoryView_Scroll(16);
        break;
    case SB_LINEUP:
        MemoryView_Scroll(-16);
        break;
    case SB_PAGEDOWN:
        MemoryView_Scroll(m_nPageSize * 16);
        break;
    case SB_PAGEUP:
        MemoryView_Scroll(-m_nPageSize * 16);
        break;
    case SB_THUMBPOSITION:
        MemoryView_ScrollTo(scrollpos * 16);
        break;
    }

    return FALSE;
}

// Scroll window to given base address
void MemoryView_ScrollTo(WORD wAddress)
{
    m_wBaseAddress = wAddress & ((WORD)~1);
    Settings_SetDebugMemoryAddress(m_wBaseAddress);

    InvalidateRect(m_hwndMemoryViewer, NULL, TRUE);

    MemoryView_UpdateScrollPos();
}
// Scroll window on given lines forward or backward
void MemoryView_Scroll(int nDelta)
{
    if (nDelta == 0) return;

    m_wBaseAddress = (WORD)(m_wBaseAddress + nDelta) & ((WORD)~1);
    Settings_SetDebugMemoryAddress(m_wBaseAddress);

    InvalidateRect(m_hwndMemoryViewer, NULL, TRUE);

    MemoryView_UpdateScrollPos();
}
void MemoryView_UpdateScrollPos()
{
    SCROLLINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
    si.nPage = m_nPageSize;
    si.nPos = m_wBaseAddress / 16;
    si.nMin = 0;
    si.nMax = 0x10000 / 16 - 1;
    SetScrollInfo(m_hwndMemoryViewer, SB_VERT, &si, TRUE);
}


//////////////////////////////////////////////////////////////////////
