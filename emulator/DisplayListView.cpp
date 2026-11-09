/*  This file is part of NEONBTL.
NEONBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
NEONBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
You should have received a copy of the GNU Lesser General Public License along with
NEONBTL. If not, see <http://www.gnu.org/licenses/>. */

// DisplayListView.cpp

#include "stdafx.h"
#include <CommCtrl.h>
#include "Main.h"
#include "Views.h"
#include "ToolWindow.h"
#include "Emulator.h"
#include "VideoDecoder.h"
#include "emubase/Emubase.h"

//////////////////////////////////////////////////////////////////////

HWND g_hwndDisplayList = (HWND)INVALID_HANDLE_VALUE;  // DisplayList view window handler
WNDPROC m_wndprocDisplayListToolWindow = nullptr;  // Old window proc address of the ToolWindow

HWND m_hwndDisplayListViewer = (HWND)INVALID_HANDLE_VALUE;

HWND m_hwndDisplayListTextbox = (HWND)INVALID_HANDLE_VALUE;

void DiaplayList_FillTreeView();


//////////////////////////////////////////////////////////////////////

void DisplayListView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = DisplayListViewViewerWndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = g_hInst;
    wcex.hIcon = nullptr;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = CLASSNAME_DISPLAYLISTVIEW;
    wcex.hIconSm = nullptr;

    RegisterClassEx(&wcex);
}

void DisplayListView_Create(int x, int y)
{
    int width = 600;
    int height = 640;

    g_hwndDisplayList = CreateWindowEx(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            CLASSNAME_OVERLAPPEDWINDOW, _T("Display List Viewer"),
            WS_POPUPWINDOW | WS_CAPTION | WS_VISIBLE,
            x, y, width, height,
            nullptr, nullptr, g_hInst, nullptr);

    // ToolWindow subclassing
    m_wndprocDisplayListToolWindow = reinterpret_cast<WNDPROC>(SetWindowLongPtr(g_hwndDisplayList, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(DisplayListViewWndProc)));

    RECT rcClient;  GetClientRect(g_hwndDisplayList, &rcClient);

    m_hwndDisplayListTextbox = CreateWindowEx(
            WS_EX_CLIENTEDGE, _T("EDIT"), nullptr,
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | ES_READONLY | ES_MULTILINE,
            0, 0, rcClient.right, rcClient.bottom,
            g_hwndDisplayList, nullptr, g_hInst, nullptr);

    HFONT hFont = CreateMonospacedFont();
    ::SendMessage(m_hwndDisplayListTextbox, WM_SETFONT, (WPARAM)hFont, 0);

    DiaplayList_FillTreeView();
    ::SetFocus(m_hwndDisplayListViewer);
}

LRESULT CALLBACK DisplayListViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_SETFOCUS:
        ::SetFocus(m_hwndDisplayListViewer);
        break;
    case WM_DESTROY:
        g_hwndDisplayList = (HWND)INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
        return CallWindowProc(m_wndprocDisplayListToolWindow, hWnd, message, wParam, lParam);
    default:
        return CallWindowProc(m_wndprocDisplayListToolWindow, hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

LRESULT CALLBACK DisplayListViewViewerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_DESTROY:
        // Free resources
        return DefWindowProc(hWnd, message, wParam, lParam);
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        SetFocus(hWnd);
        break;
        //case WM_KEYDOWN:
        //    return (LRESULT)DisplayListView_OnKeyDown(wParam, lParam);
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

void DisplayList_Print(LPCTSTR message)
{
    if (m_hwndDisplayListTextbox == INVALID_HANDLE_VALUE) return;

    // Put selection to the end of text
    SendMessage(m_hwndDisplayListTextbox, EM_SETSEL, 0x100000, 0x100000);
    // Insert the message
    SendMessage(m_hwndDisplayListTextbox, EM_REPLACESEL, (WPARAM)FALSE, (LPARAM)message);
}


void DiaplayList_FillTreeView()
{
    SendMessage(m_hwndDisplayListTextbox, WM_SETREDRAW, FALSE, 0);

    DisplayList_Print(_T("line address  tag 1  tag 2  addr   next   \r\n"));
    video::DisplayListCursor cursor;
    for (int line = 0; line < video::TotalLines; ++line)
    {
        const auto tag = cursor.Next(*g_pBoard);
        TCHAR buffer[256], tags[24];
        if (tag.kind != video::TagKind::Short)
            FormatBuffer(tags, _T("{:06o} {:06o}"), tag.tag1, tag.tag2);
        else
            FormatBuffer(tags, _T("------ ------"));
        FormatBuffer(buffer, _T(" {:3}  {:06o}  {} {:06o} {:06o}  "),
                     line, tag.address, tags, tag.pixels, tag.link);
        DisplayList_Print(buffer);
        if (tag.kind == video::TagKind::Palette)
            DisplayList_Print(_T("palette; "));
        else if (tag.kind == video::TagKind::Parameters)
        {
            FormatBuffer(buffer, _T("scale {}; "), tag.Scale());
            DisplayList_Print(buffer);
        }
        if (tag.NextKind() != video::TagKind::Short)
        {
            FormatBuffer(buffer, _T("next: 4-word at {:06o}"), tag.NextAddress());
            DisplayList_Print(buffer);
        }
        DisplayList_Print(_T("\r\n"));
    }
    SendMessage(m_hwndDisplayListTextbox, WM_SETREDRAW, TRUE, 0);
}

//////////////////////////////////////////////////////////////////////
