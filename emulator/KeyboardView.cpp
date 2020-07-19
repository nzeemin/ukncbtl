/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// KeyboardView.cpp

#include "stdafx.h"
#include "Main.h"
#include "Views.h"
#include "Emulator.h"


//////////////////////////////////////////////////////////////////////


HWND g_hwndKeyboard = (HWND) INVALID_HANDLE_VALUE;  // Keyboard View window handle

int m_nKeyboardBitmapLeft = 0;
int m_nKeyboardBitmapTop = 0;
BYTE m_nKeyboardKeyPressed = 0;  // UKNC scan-code for the key pressed, or 0

void KeyboardView_OnDraw(HDC hdc);
BYTE KeyboardView_GetKeyByPoint(int x, int y);
void Keyboard_DrawKey(HDC hdc, BYTE keyscan);


//////////////////////////////////////////////////////////////////////

// Keyboard key mapping to bitmap
const WORD m_arrKeyboardKeys[] =
{
    /*   x1,y1    w,h    UKNCscan  */
    18,  15,  42, 27,    0010, // K1
    62,  15,  42, 27,    0011, // K2
    106, 15,  42, 27,    0012, // K3
    151, 15,  42, 27,    0014, // K4
    195, 15,  42, 27,    0015, // K5
    343, 15,  42, 27,    0172, // POM
    387, 15,  42, 27,    0152, // UST
    431, 15,  42, 27,    0151, // ISP
    506, 15,  42, 27,    0171, // SBROS (RESET)
    551, 15,  42, 27,    0004, // STOP

    18,  56,  28, 27,    0006, // AR2
    47,  56,  28, 27,    0007, // ; +
    77,  56,  27, 27,    0030, // 1
    106, 56,  28, 27,    0031, // 2
    136, 56,  27, 27,    0032, // 3
    165, 56,  28, 27,    0013, // 4
    195, 56,  27, 27,    0034, // 5
    224, 56,  28, 27,    0035, // 6
    254, 56,  27, 27,    0016, // 7
    283, 56,  28, 27,    0017, // 8
    313, 56,  27, 27,    0177, // 9
    342, 56,  28, 27,    0176, // 0
    372, 56,  27, 27,    0175, // - =
    401, 56,  28, 27,    0173, // / ?
    431, 56,  42, 27,    0132, // Backspace

    18,  86,  42, 27,    0026, // TAB
    62,  86,  27, 27,    0027, // Й J
    91,  86,  28, 27,    0050, // Ц C
    121, 86,  27, 27,    0051, // У U
    150, 86,  28, 27,    0052, // К K
    180, 86,  27, 27,    0033, // Е E
    210, 86,  28, 27,    0054, // Н N
    239, 86,  27, 27,    0055, // Г G
    269, 86,  27, 27,    0036, // Ш [
    298, 86,  28, 27,    0037, // Щ ]
    328, 86,  27, 27,    0157, // З Z
    357, 86,  28, 27,    0156, // Х H
    387, 86,  27, 27,    0155, // Ъ
    416, 86,  28, 27,    0174, // : *

    18,  115, 49, 27,    0046, // UPR
    69,  115, 28, 27,    0047, // Ф F
    99,  115, 27, 27,    0070, // Ы Y
    128, 115, 28, 27,    0071, // В W
    158, 115, 27, 27,    0072, // А A
    187, 115, 28, 27,    0053, // П P
    217, 115, 27, 27,    0074, // Р R
    246, 115, 28, 27,    0075, // О O
    276, 115, 27, 27,    0056, // Л L
    305, 115, 28, 27,    0057, // Д D
    335, 115, 27, 27,    0137, // Ж V
    364, 115, 28, 27,    0136, // Э Backslash
    394, 115, 35, 27,    0135, // . >
    431, 115, 16, 27,    0153, // ENTER - left part
    446, 86,  27, 56,    0153, // ENTER - right part

    18,  145, 34, 27,    0106, // ALF
    55,  145, 27, 27,    0066, // GRAF
    84,  145, 27, 27,    0067, // Я Q
    114, 145, 27, 27,    0110, // Ч ^
    143, 145, 27, 27,    0111, // С S
    173, 145, 27, 27,    0112, // М
    202, 145, 27, 27,    0073, // И I
    232, 145, 27, 27,    0114, // Т
    261, 145, 27, 27,    0115, // Ь X
    291, 145, 27, 27,    0076, // Б B
    320, 145, 28, 27,    0077, // Ю @
    350, 145, 34, 27,    0117, // , <

    18,  174, 56, 27,    0105, // Left Shift
    77,  174, 34, 27,    0107, // FIKS
    114, 174, 211, 27,    0113, // Space bar
    328, 174, 56, 27,    0105, // Right Shift

    387, 145, 27, 56,    0116, // Left
    416, 145, 28, 27,    0154, // Up
    416, 174, 28, 27,    0134, // Down
    446, 145, 27, 56,    0133, // Right

    506, 56,  28, 27,    0131, // + NumPad
    536, 56,  27, 27,    0025, // - NumPad
    565, 56,  28, 27,    0005, // , NumPad
    506, 86,  28, 27,    0125, // 7 NumPad
    536, 86,  27, 27,    0145, // 8 NumPad
    565, 86,  28, 27,    0165, // 9 NumPad
    506, 115, 28, 27,    0130, // 4 NumPad
    536, 115, 27, 27,    0150, // 5 NumPad
    565, 115, 28, 27,    0170, // 6 NumPad
    506, 145, 28, 27,    0127, // 1 NumPad
    536, 145, 27, 27,    0147, // 2 NumPad
    565, 145, 28, 27,    0167, // 3 NumPad
    506, 174, 28, 27,    0126, // 0 NumPad
    536, 174, 27, 27,    0146, // . NumPad
    565, 174, 28, 27,    0166, // ENTER NumPad
};
const int m_nKeyboardKeysCount = sizeof(m_arrKeyboardKeys) / sizeof(WORD) / 5;

//////////////////////////////////////////////////////////////////////


void KeyboardView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= KeyboardViewWndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= g_hInst;
    wcex.hIcon			= NULL;
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= CLASSNAME_KEYBOARDVIEW;
    wcex.hIconSm		= NULL;

    RegisterClassEx(&wcex);
}

void KeyboardView_Create(HWND hwndParent, int x, int y, int width, int height)
{
    ASSERT(hwndParent != NULL);

    g_hwndKeyboard = CreateWindow(
            CLASSNAME_KEYBOARDVIEW, NULL,
            WS_CHILD | WS_VISIBLE,
            x, y, width, height,
            hwndParent, NULL, g_hInst, NULL);
}

LRESULT CALLBACK KeyboardViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            KeyboardView_OnDraw(hdc);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_SETCURSOR:
        {
            POINT ptCursor;  ::GetCursorPos(&ptCursor);
            ::ScreenToClient(g_hwndKeyboard, &ptCursor);
            BYTE keyscan = KeyboardView_GetKeyByPoint(ptCursor.x, ptCursor.y);
            LPCTSTR cursor = (keyscan == 0) ? IDC_ARROW : IDC_HAND;
            ::SetCursor(::LoadCursor(NULL, cursor));
        }
        return (LRESULT)TRUE;
    case WM_LBUTTONDOWN:
        {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            BYTE keyscan = KeyboardView_GetKeyByPoint(x, y);
            if (keyscan != 0)
            {
                // Fire keydown event and capture mouse
                ScreenView_KeyEvent(keyscan, TRUE);
                ::SetCapture(g_hwndKeyboard);

                // Draw focus frame for the key pressed
                HDC hdc = ::GetDC(g_hwndKeyboard);
                Keyboard_DrawKey(hdc, keyscan);
                ::ReleaseDC(g_hwndKeyboard, hdc);

                // Remember key pressed
                m_nKeyboardKeyPressed = keyscan;
            }
        }
        break;
    case WM_LBUTTONUP:
        if (m_nKeyboardKeyPressed != 0)
        {
            // Fire keyup event and release mouse
            ScreenView_KeyEvent(m_nKeyboardKeyPressed, FALSE);
            ::ReleaseCapture();

            // Draw focus frame for the released key
            HDC hdc = ::GetDC(g_hwndKeyboard);
            Keyboard_DrawKey(hdc, m_nKeyboardKeyPressed);
            ::ReleaseDC(g_hwndKeyboard, hdc);

            m_nKeyboardKeyPressed = 0;
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

void KeyboardView_OnDraw(HDC hdc)
{
    HBITMAP hBmp = ::LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_KEYBOARD));
    HBITMAP hBmpMask = ::LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_KEYBOARDMASK));

    HDC hdcMem = ::CreateCompatibleDC(hdc);
    HGDIOBJ hOldBitmap = ::SelectObject(hdcMem, hBmp);

    RECT rc;  ::GetClientRect(g_hwndKeyboard, &rc);

    BITMAP bitmap;
    VERIFY(::GetObject(hBmp, sizeof(BITMAP), &bitmap));
    int cxBitmap = (int) bitmap.bmWidth;
    int cyBitmap = (int) bitmap.bmHeight;
    m_nKeyboardBitmapLeft = (rc.right - cxBitmap) / 2;
    m_nKeyboardBitmapTop = (rc.bottom - cyBitmap) / 2;
    ::MaskBlt(hdc, m_nKeyboardBitmapLeft, m_nKeyboardBitmapTop, cxBitmap, cyBitmap, hdcMem, 0, 0,
            hBmpMask, 0, 0, MAKEROP4(SRCCOPY, SRCAND));

    ::SelectObject(hdcMem, hOldBitmap);
    ::DeleteDC(hdcMem);
    ::DeleteObject(hBmp);

    if (m_nKeyboardKeyPressed != 0)
        Keyboard_DrawKey(hdc, m_nKeyboardKeyPressed);

    //// Show key mappings
    //for (int i = 0; i < m_nKeyboardKeysCount; i++)
    //{
    //    RECT rcKey;
    //    rcKey.left = m_nKeyboardBitmapLeft + m_arrKeyboardKeys[i * 5];
    //    rcKey.top = m_nKeyboardBitmapTop + m_arrKeyboardKeys[i * 5 + 1];
    //    rcKey.right = rcKey.left + m_arrKeyboardKeys[i * 5 + 2];
    //    rcKey.bottom = rcKey.top + m_arrKeyboardKeys[i * 5 + 3];

    //    ::DrawFocusRect(hdc, &rcKey);
    //}
}

// Returns: UKNC scan-code of key under the cursor position, or 0 if not found
BYTE KeyboardView_GetKeyByPoint(int x, int y)
{
    for (int i = 0; i < m_nKeyboardKeysCount; i++)
    {
        RECT rcKey;
        rcKey.left = m_nKeyboardBitmapLeft + m_arrKeyboardKeys[i * 5];
        rcKey.top = m_nKeyboardBitmapTop + m_arrKeyboardKeys[i * 5 + 1];
        rcKey.right = rcKey.left + m_arrKeyboardKeys[i * 5 + 2];
        rcKey.bottom = rcKey.top + m_arrKeyboardKeys[i * 5 + 3];

        if (x >= rcKey.left && x < rcKey.right && y >= rcKey.top && y < rcKey.bottom)
        {
            return (BYTE) m_arrKeyboardKeys[i * 5 + 4];
        }
    }
    return 0;
}

void Keyboard_DrawKey(HDC hdc, BYTE keyscan)
{
    for (int i = 0; i < m_nKeyboardKeysCount; i++)
        if (keyscan == m_arrKeyboardKeys[i * 5 + 4])
        {
            RECT rcKey;
            rcKey.left = m_nKeyboardBitmapLeft + m_arrKeyboardKeys[i * 5];
            rcKey.top = m_nKeyboardBitmapTop + m_arrKeyboardKeys[i * 5 + 1];
            rcKey.right = rcKey.left + m_arrKeyboardKeys[i * 5 + 2];
            rcKey.bottom = rcKey.top + m_arrKeyboardKeys[i * 5 + 3];
            ::DrawFocusRect(hdc, &rcKey);
        }
}


//////////////////////////////////////////////////////////////////////
