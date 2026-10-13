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
            WS_CHILD | WS_VISIBLE,
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

// Update after Run or Step
void InstrHistoryView_OnUpdate()
{
    int length = Emulator_GetHistoryLength(m_okInstrHistoryProcessor);
    for (int i = 0; i < length; i++)
        m_InstrHistorySnapshot[i] = Emulator_GetHistoryRecord(m_okInstrHistoryProcessor, i);
    m_nInstrHistorySnapshotLength = length;

    if (g_hwndInstrHistory != (HWND) INVALID_HANDLE_VALUE && m_hwndInstrHistoryViewer != (HWND) INVALID_HANDLE_VALUE)
        InvalidateRect(m_hwndInstrHistoryViewer, NULL, TRUE);
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
