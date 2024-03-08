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
#include "emubase/Emubase.h"

//////////////////////////////////////////////////////////////////////

HWND g_hwndDisplayList = (HWND)INVALID_HANDLE_VALUE;  // DisplayList view window handler
WNDPROC m_wndprocDisplayListToolWindow = NULL;  // Old window proc address of the ToolWindow

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
    wcex.hIcon = NULL;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = CLASSNAME_DISPLAYLISTVIEW;
    wcex.hIconSm = NULL;

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
            NULL, NULL, g_hInst, NULL);

    // ToolWindow subclassing
    m_wndprocDisplayListToolWindow = (WNDPROC)LongToPtr(SetWindowLongPtr(
            g_hwndDisplayList, GWLP_WNDPROC, PtrToLong(DisplayListViewWndProc)));

    RECT rcClient;  GetClientRect(g_hwndDisplayList, &rcClient);

    m_hwndDisplayListTextbox = CreateWindowEx(
            WS_EX_CLIENTEDGE, _T("EDIT"), NULL,
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | ES_READONLY | ES_MULTILINE,
            0, 0, rcClient.right, rcClient.bottom,
            g_hwndDisplayList, NULL, g_hInst, NULL);

    HFONT hFont = CreateMonospacedFont();
    ::SendMessage(m_hwndDisplayListTextbox, WM_SETFONT, (WPARAM)hFont, NULL);

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

    const CMotherboard* pBoard = g_pBoard;

    DisplayList_Print(_T("line address  tag 1  tag 2  addr   next   \r\n"));

    WORD address = 0000270;  // Tag sequence start address
    bool okTagSize = false;  // Tag size: TRUE - 4-word, false - 2-word (first tag is always 2-word)
    bool okTagType = false;  // Type of 4-word tag: TRUE - set palette, false - set params

    for (int yy = 0; yy < 307; yy++)  // Цикл по строкам
    {
        const size_t buffersize = 256 - 1;
        TCHAR buffer[buffersize + 1];
        const size_t buftagssize = 24 - 1;
        TCHAR buftags[buftagssize + 1];

        uint32_t curaddress = address;

        WORD tag2 = 0;
        if (okTagSize)  // 4-word tag
        {
            WORD tag1 = pBoard->GetRAMWord(0, address);
            address += 2;
            tag2 = pBoard->GetRAMWord(0, address);
            address += 2;
            _sntprintf(buftags, buftagssize, _T("%06o %06o"), tag1, tag2);
        }
        else
            _sntprintf(buftags, buftagssize, _T("------ ------"));

        WORD addressBits = pBoard->GetRAMWord(0, address);  // The word before the last word - is address of bits from all three memory planes
        address += 2;

        WORD tagB = pBoard->GetRAMWord(0, address);  // Last word of the tag - is address and type of the next tag

        _sntprintf(buffer, buffersize, _T(" %3d  %06o  %s %06o %06o  "), yy, curaddress, buftags, addressBits, tagB);
        DisplayList_Print(buffer);

        const size_t bufcmtsize = 64 - 1;
        TCHAR bufcmt[bufcmtsize + 1];  bufcmt[0] = 0;
        if (okTagSize)  // 4-word tag
        {
            if (okTagType)  // 4-word palette tag
                _tcsncat_s(bufcmt, bufcmtsize, _T("palette; "), _TRUNCATE);
            else
            {
                int scale = (tag2 >> 4) & 3;  // Bits 4-5 - new scale value
                scale = 1 << scale;
                _sntprintf(buftags, buftagssize, _T("scale %d; "), scale);
                _tcsncat_s(bufcmt, bufcmtsize, buftags, _TRUNCATE);
            }
        }

        // Calculate size, type and address of the next tag
        uint16_t nextaddress;
        bool okNextTagSize = (tagB & 2) != 0;  // Bit 1 shows size of the next tag
        bool okNextTagType = okTagType;
        if (okNextTagSize)
        {
            nextaddress = tagB & ~7;
            okNextTagType = (tagB & 4) != 0;  // Bit 2 shows type of the next tag
            _sntprintf(buftags, buftagssize, _T("next: 4-word at %06o"), nextaddress);
            _tcsncat_s(bufcmt, bufcmtsize, buftags, _TRUNCATE);
        }
        else
            nextaddress = tagB & ~3;

        _tcsncat_s(bufcmt, bufcmtsize, _T("\r\n"), _TRUNCATE);
        DisplayList_Print(bufcmt);

        address = nextaddress;
        okTagSize = okNextTagSize;
        okTagType = okNextTagType;
    }

    SendMessage(m_hwndDisplayListTextbox, WM_SETREDRAW, TRUE, 0);
}

//////////////////////////////////////////////////////////////////////
