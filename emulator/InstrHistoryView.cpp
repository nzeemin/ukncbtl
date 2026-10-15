/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// InstrHistoryView.cpp

#include "stdafx.h"
#include "Main.h"
#include "Views.h"
#include "ToolWindow.h"
#include "Emulator.h"
#include "emubase/Emubase.h"

//////////////////////////////////////////////////////////////////////


HWND g_hwndInstrHistory = (HWND) INVALID_HANDLE_VALUE;  // Instr History View window handle
WNDPROC m_wndprocInstrHistoryToolWindow = NULL;  // Old window proc address of the ToolWindow

HWND m_hwndInstrHistoryViewer = (HWND) INVALID_HANDLE_VALUE;

bool m_okInstrHistoryProcessor = false;  // true - CPU, false - PPU

// Snapshot of the history of the current processor, taken on every update after Stop or Step;
// the view shows the snapshot, so it keeps the state of the previous stop while the emulator is running
InstructionRecord m_InstrHistorySnapshot[INSTR_HISTORY_SIZE];
int m_nInstrHistorySnapshotLength = 0;  // Index 0 is the most recent instruction

int m_nInstrHistoryBottomLine = 0;  // Index of the instruction shown at the bottom line


//////////////////////////////////////////////////////////////////////


void InstrHistoryView_DoDraw(HDC hdc);
void InstrHistoryView_OnMouseWheel(WPARAM wParam);
void InstrHistoryView_OnVScroll(WORD scrollcmd);
void InstrHistoryView_SetBottomLine(int index);
void InstrHistoryView_UpdateScrollBar();


void InstrHistoryView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = InstrHistoryViewViewerWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInst;
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = CLASSNAME_INSTRHISTORYVIEW;
    wcex.hIconSm        = NULL;

    RegisterClassEx(&wcex);
}

void InstrHistoryView_Create(HWND hwndParent, int x, int y, int width, int height)
{
    ASSERT(hwndParent != NULL);

    m_okInstrHistoryProcessor = Settings_GetDebugCpuPpu() != 0;

    g_hwndInstrHistory = CreateWindow(
            CLASSNAME_TOOLWINDOW, _T("Instructions History"),
            WS_CHILD | WS_VISIBLE,
            x, y, width, height,
            hwndParent, NULL, g_hInst, NULL);

    // ToolWindow subclassing
    m_wndprocInstrHistoryToolWindow = (WNDPROC) LongToPtr( SetWindowLongPtr(
            g_hwndInstrHistory, GWLP_WNDPROC, PtrToLong(InstrHistoryViewWndProc)) );

    RECT rcClient;  GetClientRect(g_hwndInstrHistory, &rcClient);

    m_hwndInstrHistoryViewer = CreateWindowEx(
            WS_EX_STATICEDGE,
            CLASSNAME_INSTRHISTORYVIEW, NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL,
            0, 0, rcClient.right, rcClient.bottom,
            g_hwndInstrHistory, NULL, g_hInst, NULL);

    InstrHistoryView_OnUpdate();
}

void InstrHistoryView_Redraw()
{
    RedrawWindow(g_hwndInstrHistory, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

// Adjust position of client windows
void InstrHistoryView_AdjustWindowLayout()
{
    RECT rc;  GetClientRect(g_hwndInstrHistory, &rc);

    if (m_hwndInstrHistoryViewer != (HWND) INVALID_HANDLE_VALUE)
        SetWindowPos(m_hwndInstrHistoryViewer, NULL, 0, 0, rc.right, rc.bottom, SWP_NOZORDER);
}

LRESULT CALLBACK InstrHistoryViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lResult;
    switch (message)
    {
    case WM_DESTROY:
        g_hwndInstrHistory = (HWND) INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
        m_hwndInstrHistoryViewer = (HWND) INVALID_HANDLE_VALUE;
        return CallWindowProc(m_wndprocInstrHistoryToolWindow, hWnd, message, wParam, lParam);
    case WM_SIZE:
        lResult = CallWindowProc(m_wndprocInstrHistoryToolWindow, hWnd, message, wParam, lParam);
        InstrHistoryView_AdjustWindowLayout();
        return lResult;
    default:
        return CallWindowProc(m_wndprocInstrHistoryToolWindow, hWnd, message, wParam, lParam);
    }
}

LRESULT CALLBACK InstrHistoryViewViewerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            InstrHistoryView_DoDraw(hdc);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_LBUTTONDOWN:
        ::SetFocus(hWnd);  // Mouse wheel goes to the focused window
        break;
    case WM_MOUSEWHEEL:
        InstrHistoryView_OnMouseWheel(wParam);
        break;
    case WM_VSCROLL:
        InstrHistoryView_OnVScroll(LOWORD(wParam));
        break;
    case WM_SIZE:
        InstrHistoryView_SetBottomLine(m_nInstrHistoryBottomLine);  // Re-clamp for the new number of rows
        break;
    case WM_COMMAND:
        // Forward commands to the main window
        ::PostMessage(g_hwnd, WM_COMMAND, wParam, lParam);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}


//////////////////////////////////////////////////////////////////////


void InstrHistoryView_SetCurrentProc(BOOL okCPU)
{
    m_okInstrHistoryProcessor = okCPU != 0;
    InstrHistoryView_OnUpdate();
}

// Number of lines that fit into the viewer
int InstrHistoryView_GetRowCount()
{
    HDC hdc = GetDC(m_hwndInstrHistoryViewer);
    HFONT hFont = CreateMonospacedFont();
    HGDIOBJ hOldFont = SelectObject(hdc, hFont);
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    SelectObject(hdc, hOldFont);
    VERIFY(::DeleteObject(hFont));
    ReleaseDC(m_hwndInstrHistoryViewer, hdc);

    RECT rcClient;  GetClientRect(m_hwndInstrHistoryViewer, &rcClient);
    int rows = (rcClient.bottom - 2) / cyLine;
    return rows > 0 ? rows : 1;
}

// The biggest index of the bottom line: the oldest instruction is at the top line
int InstrHistoryView_GetMaxBottomLine()
{
    int max = m_nInstrHistorySnapshotLength - InstrHistoryView_GetRowCount();
    return max > 0 ? max : 0;
}

// The scrollbar top is the oldest instruction, the bottom is the most recent one
void InstrHistoryView_UpdateScrollBar()
{
    if (m_hwndInstrHistoryViewer == (HWND) INVALID_HANDLE_VALUE)
        return;

    int max = InstrHistoryView_GetMaxBottomLine();
    SCROLLINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_DISABLENOSCROLL;
    si.nMin = 0;
    si.nMax = max + InstrHistoryView_GetRowCount() - 1;
    si.nPage = InstrHistoryView_GetRowCount();
    si.nPos = max - m_nInstrHistoryBottomLine;
    SetScrollInfo(m_hwndInstrHistoryViewer, SB_VERT, &si, TRUE);
}

void InstrHistoryView_SetBottomLine(int index)
{
    if (m_hwndInstrHistoryViewer == (HWND) INVALID_HANDLE_VALUE)
        return;

    int max = InstrHistoryView_GetMaxBottomLine();
    if (index > max) index = max;
    if (index < 0) index = 0;
    if (index != m_nInstrHistoryBottomLine)
    {
        m_nInstrHistoryBottomLine = index;
        InvalidateRect(m_hwndInstrHistoryViewer, NULL, TRUE);
    }
    InstrHistoryView_UpdateScrollBar();
}

void InstrHistoryView_OnMouseWheel(WPARAM wParam)
{
    short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
    int steps = zDelta / 40;  // 120 units per notch = 3 lines
    if (steps == 0)
        return;
    InstrHistoryView_SetBottomLine(m_nInstrHistoryBottomLine + steps);  // Wheel up shows older instructions
}

void InstrHistoryView_OnVScroll(WORD scrollcmd)
{
    int max = InstrHistoryView_GetMaxBottomLine();
    int rows = InstrHistoryView_GetRowCount();
    switch (scrollcmd)
    {
    case SB_LINEUP:
        InstrHistoryView_SetBottomLine(m_nInstrHistoryBottomLine + 1);
        break;
    case SB_LINEDOWN:
        InstrHistoryView_SetBottomLine(m_nInstrHistoryBottomLine - 1);
        break;
    case SB_PAGEUP:
        InstrHistoryView_SetBottomLine(m_nInstrHistoryBottomLine + rows);
        break;
    case SB_PAGEDOWN:
        InstrHistoryView_SetBottomLine(m_nInstrHistoryBottomLine - rows);
        break;
    case SB_TOP:
        InstrHistoryView_SetBottomLine(max);
        break;
    case SB_BOTTOM:
        InstrHistoryView_SetBottomLine(0);
        break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
        {
            SCROLLINFO si;
            ZeroMemory(&si, sizeof(si));
            si.cbSize = sizeof(si);
            si.fMask = SIF_TRACKPOS;  // 32-bit position, the message has only 16 bits
            GetScrollInfo(m_hwndInstrHistoryViewer, SB_VERT, &si);
            InstrHistoryView_SetBottomLine(max - si.nTrackPos);
        }
        break;
    }
}

// Update after Run or Step
void InstrHistoryView_OnUpdate()
{
    int length = Emulator_GetHistoryLength(m_okInstrHistoryProcessor);
    for (int i = 0; i < length; i++)
        m_InstrHistorySnapshot[i] = Emulator_GetHistoryRecord(m_okInstrHistoryProcessor, i);
    m_nInstrHistorySnapshotLength = length;
    m_nInstrHistoryBottomLine = 0;  // Every update (stop, step, reset) scrolls back to the most recent instruction

    if (g_hwndInstrHistory != (HWND) INVALID_HANDLE_VALUE && m_hwndInstrHistoryViewer != (HWND) INVALID_HANDLE_VALUE)
    {
        InstrHistoryView_UpdateScrollBar();
        InvalidateRect(m_hwndInstrHistoryViewer, NULL, TRUE);
    }
}

// Lines go from bottom to top: the line with index m_nInstrHistoryBottomLine is at the bottom
void InstrHistoryView_DoDraw(HDC hdc)
{
    HFONT hFont = CreateMonospacedFont();
    HGDIOBJ hOldFont = SelectObject(hdc, hFont);
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorOld = SetTextColor(hdc, Settings_GetColor(ColorDebugText));
    SetBkMode(hdc, TRANSPARENT);

    RECT rcClient;  GetClientRect(m_hwndInstrHistoryViewer, &rcClient);
    int x = 4;
    int y = rcClient.bottom - 2 - cyLine;  // The bottom line

    for (int index = m_nInstrHistoryBottomLine; index < m_nInstrHistorySnapshotLength; index++)
    {
        if (y + cyLine <= 0)
            break;  // The rest of the lines are above the top of the window

        const InstructionRecord& record = m_InstrHistorySnapshot[index];

        TCHAR strInstr[8];
        TCHAR strArg[32];
        DisassembleInstruction(record.words, record.pc, strInstr, strArg);

        TCHAR bufaddr[7];
        PrintOctalValue(bufaddr, record.pc);

        TCHAR buffer[64];
        _sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR) - 1, _T("%3d %s %s %s"), index, bufaddr, strInstr, strArg);
        TextOut(hdc, x, y, buffer, static_cast<int>(_tcslen(buffer)));

        y -= cyLine;
    }

    SetTextColor(hdc, colorOld);
    SelectObject(hdc, hOldFont);
    VERIFY(::DeleteObject(hFont));
}


//////////////////////////////////////////////////////////////////////
