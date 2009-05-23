// ToolWindow.cpp

#include "stdafx.h"
#include "UKNCBTL.h"
#include "ToolWindow.h"

//////////////////////////////////////////////////////////////////////

const int TOOLWINDOW_CAPTION_HEIGHT = 16;


void ToolWindow_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= ToolWindowWndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= g_hInst;
    wcex.hIcon			= NULL;
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE+1);
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= CLASSNAME_TOOLWINDOW;
    wcex.hIconSm		= NULL;

    RegisterClassEx(&wcex);
}

LRESULT CALLBACK ToolWindowWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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
			::DrawText(hdc, buffer, wcslen(buffer), &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
			::SelectObject(hdc, hOldFont);
			::DeleteObject(hfont);

            ReleaseDC(hWnd, hdc);
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}


//////////////////////////////////////////////////////////////////////
