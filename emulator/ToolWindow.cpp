/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// ToolWindow.cpp

#include "stdafx.h"
#include "Main.h"
#include "ToolWindow.h"
#include <Windowsx.h>

//////////////////////////////////////////////////////////////////////

const int TOOLWINDOW_CAPTION_HEIGHT = 16;


void ToolWindow_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= ToolWindow_WndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= g_hInst;
    wcex.hIcon			= NULL;
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= CLASSNAME_TOOLWINDOW;
    wcex.hIconSm		= NULL;

    RegisterClassEx(&wcex);
}

LRESULT CALLBACK ToolWindow_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_NCCALCSIZE:
        {
            LPRECT pRect = (LPRECT) lParam;
            pRect->top += TOOLWINDOW_CAPTION_HEIGHT;
        }
        break;
    case WM_NCPAINT:
        {
            RECT rcClient;  ::GetClientRect(hWnd, &rcClient);
            RECT rcWindow;  ::GetWindowRect(hWnd, &rcWindow);
            MapWindowPoints(NULL, hWnd, (LPPOINT)&rcWindow, 2);

            rcClient.left -= rcWindow.left;  rcClient.top -= rcWindow.top;
            rcClient.right -= rcWindow.left;  rcClient.bottom -= rcWindow.top;
            rcWindow.left -= rcWindow.left;  rcWindow.top -= rcWindow.top;
            rcWindow.right -= rcWindow.left;  rcWindow.bottom -= rcWindow.top;

            HDC hdc = ::GetWindowDC(hWnd);
            ::ExcludeClipRect(hdc, rcClient.left, rcClient.top, rcClient.right, rcClient.bottom);

            // Draw caption background
            BOOL okActive = (::GetActiveWindow() == hWnd);
            HBRUSH brushCaption = ::GetSysColorBrush(okActive ? COLOR_ACTIVECAPTION : COLOR_INACTIVECAPTION);
            RECT rc;
            rc.left = rc.top = 0;
            rc.right = rcWindow.right;  rc.bottom = TOOLWINDOW_CAPTION_HEIGHT;
            ::FillRect(hdc, &rc, brushCaption);

            // Draw caption text
            TCHAR buffer[64];
            ::GetWindowText(hWnd, buffer, 64);
            rc.left += 8;  rc.right -= 8;
            HFONT hfont = CreateDialogFont();
            HGDIOBJ hOldFont = ::SelectObject(hdc, hfont);
            ::SetTextColor(hdc, ::GetSysColor(COLOR_CAPTIONTEXT));
            ::SetBkMode(hdc, TRANSPARENT);
            ::DrawText(hdc, buffer, (int) _tcslen(buffer), &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            ::SelectObject(hdc, hOldFont);
            ::DeleteObject(hfont);

            //RECT rcClose;
            //rcClose.right = rcWindow.right;  rcClose.top = 0;  rcClose.bottom = TOOLWINDOW_CAPTION_HEIGHT;
            //rcClose.left = rcWindow.right - 18;
            //::DrawFrameControl(hdc, &rcClose, DFC_CAPTION, DFCS_CAPTIONCLOSE | DFCS_FLAT);

            ReleaseDC(hWnd, hdc);
        }
        break;
    case WM_SETTEXT:
        {
            LRESULT lResult = DefWindowProc(hWnd, message, wParam, lParam);

            // Invalidate non-client area
            RECT rcWindow;  ::GetWindowRect(hWnd, &rcWindow);
            MapWindowPoints(NULL, hWnd, (LPPOINT)&rcWindow, 2);
            ::RedrawWindow(hWnd, &rcWindow, NULL, RDW_FRAME | RDW_INVALIDATE);

            return lResult;
        }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}


//////////////////////////////////////////////////////////////////////


void OverlappedWindow_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= DefWindowProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= g_hInst;
    wcex.hIcon			= NULL;
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= CLASSNAME_OVERLAPPEDWINDOW;
    wcex.hIconSm		= NULL;

    RegisterClassEx(&wcex);
}


//////////////////////////////////////////////////////////////////////

bool m_SplitterWindow_IsMoving = false;
int m_SplitterWindow_MovingStartY = 0;

const int SPLITTERWINDOW_MINWINDOWHEIGHT = 64;

void SplitterWindow_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = SplitterWindow_WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 8 * 2;
    wcex.hInstance = g_hInst;
    wcex.hIcon = NULL;
    wcex.hCursor = LoadCursor(NULL, IDC_SIZENS);
    wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = CLASSNAME_SPLITTERWINDOW;
    wcex.hIconSm = NULL;

    RegisterClassEx(&wcex);
}

HWND SplitterWindow_Create(HWND hwndParent, HWND hwndTop, HWND hwndBottom)
{
    ASSERT(hwndParent != NULL);
    ASSERT(hwndTop != NULL);
    ASSERT(hwndBottom != NULL);

    HWND hwnd = CreateWindow(
            CLASSNAME_SPLITTERWINDOW, NULL,
            WS_CHILD | WS_VISIBLE,
            0, 0, 100, 4,
            hwndParent, NULL, g_hInst, NULL);

    SetWindowLongPtr(hwnd, 0, (LONG_PTR)hwndTop);
    SetWindowLongPtr(hwnd, 8, (LONG_PTR)hwndBottom);

    m_SplitterWindow_IsMoving = false;

    return hwnd;
}

void SplitterWindow_MoveWindows(HWND hwndSplitter, int deltaY)
{
    if (deltaY == 0)
        return;

    HWND hwndTop = (HWND)GetWindowLongPtr(hwndSplitter, 0);
    HWND hwndBottom = (HWND)GetWindowLongPtr(hwndSplitter, 8);

    RECT rcTop;  ::GetWindowRect(hwndTop, &rcTop);
    ::ScreenToClient(g_hwnd, (POINT*)&rcTop);
    ::ScreenToClient(g_hwnd, ((POINT*)&rcTop) + 1);
    RECT rcBottom;  ::GetWindowRect(hwndBottom, &rcBottom);
    ::ScreenToClient(g_hwnd, (POINT*)&rcBottom);
    ::ScreenToClient(g_hwnd, ((POINT*)&rcBottom) + 1);

    if (deltaY < 0 && rcTop.bottom - rcTop.top + deltaY < SPLITTERWINDOW_MINWINDOWHEIGHT)
        deltaY = SPLITTERWINDOW_MINWINDOWHEIGHT - (rcTop.bottom - rcTop.top);
    if (deltaY > 0 && rcBottom.bottom - rcBottom.top - deltaY < SPLITTERWINDOW_MINWINDOWHEIGHT)
        deltaY = (rcBottom.bottom - rcBottom.top) - SPLITTERWINDOW_MINWINDOWHEIGHT;

    SetWindowPos(hwndTop, NULL, rcTop.left, rcTop.top, rcTop.right - rcTop.left, rcTop.bottom - rcTop.top + deltaY, SWP_NOZORDER);
    SetWindowPos(hwndBottom, NULL, rcBottom.left, rcBottom.top + deltaY, rcBottom.right - rcBottom.left, rcBottom.bottom - rcBottom.top - deltaY, SWP_NOZORDER);
    SetWindowPos(hwndSplitter, NULL, rcBottom.left, rcBottom.top + deltaY - 4, rcBottom.right - rcBottom.left, 4, SWP_NOZORDER);
}

LRESULT CALLBACK SplitterWindow_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_LBUTTONDOWN:
        m_SplitterWindow_IsMoving = true;
        m_SplitterWindow_MovingStartY = GET_Y_LPARAM(lParam);
        ::SetCapture(hWnd);
        //TODO: Draw frame
        return 0;
    case WM_LBUTTONUP:
        if (m_SplitterWindow_IsMoving)
        {
            m_SplitterWindow_IsMoving = false;
            ::ReleaseCapture();

            int deltaY = GET_Y_LPARAM(lParam) - m_SplitterWindow_MovingStartY;
            SplitterWindow_MoveWindows(hWnd, deltaY);
        }
        return 0;
    case WM_MOUSEMOVE:
        if ((wParam == MK_LBUTTON) && m_SplitterWindow_IsMoving)
        {
            //TODO: Redraw frame
        }
        return 0;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    //return (LRESULT)FALSE;
}


//////////////////////////////////////////////////////////////////////
