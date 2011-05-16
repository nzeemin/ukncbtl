// ScreenView.cpp

#include "stdafx.h"
#include <mmintrin.h>
#include <vfw.h>
#include "UKNCBTL.h"
#include "Views.h"
#include "Emulator.h"


//////////////////////////////////////////////////////////////////////


HWND g_hwndScreen = NULL;  // Screen View window handle

HDRAWDIB m_hdd = NULL;
BITMAPINFO m_bmpinfo;
HBITMAP m_hbmp = NULL;
DWORD * m_bits = NULL;
int m_cxScreenWidth;
int m_cyScreenHeight;
BYTE m_ScreenKeyState[256];
ScreenViewMode m_ScreenMode = RGBScreen;
int m_ScreenHeightMode = 1;  // 1 - Normal height, 2 - Double height

void ScreenView_CreateDisplay();
void ScreenView_OnDraw(HDC hdc);
//BOOL ScreenView_OnKeyEvent(WPARAM vkey, BOOL okExtKey, BOOL okPressed);
//BYTE TranslateVkeyToUkncScan(WORD vkey, BOOL okExtKey, BOOL orig);

const int KEYEVENT_QUEUE_SIZE = 32;
WORD m_ScreenKeyQueue[KEYEVENT_QUEUE_SIZE];
int m_ScreenKeyQueueTop = 0;
int m_ScreenKeyQueueBottom = 0;
int m_ScreenKeyQueueCount = 0;
void ScreenView_PutKeyEventToQueue(WORD keyevent);
WORD ScreenView_GetKeyEventFromQueue();


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
const DWORD ScreenView_StandardRGBColors[16] = {
    0x000000, 0x000080, 0x008000, 0x008080, 0x800000, 0x800080, 0x808000, 0x808080,
    0x000000, 0x0000FF, 0x00FF00, 0x00FFFF, 0xFF0000, 0xFF00FF, 0xFFFF00, 0xFFFFFF,
};
const DWORD ScreenView_StandardGRBColors[16] = {
    0x000000, 0x000080, 0x800000, 0x800080, 0x008000, 0x008080, 0x808000, 0x808080,
    0x000000, 0x0000FF, 0xFF0000, 0xFF00FF, 0x00FF00, 0x00FFFF, 0xFFFF00, 0xFFFFFF,
};
// Table for color conversion, gray (black and white) display
const DWORD ScreenView_GrayColors[16] = {
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
};

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
    m_hdd = DrawDibOpen();
    ScreenView_CreateDisplay();
}

void ScreenView_Done()
{
    if (m_hbmp != NULL)
    {
        DeleteObject(m_hbmp);
        m_hbmp = NULL;
    }

    DrawDibClose( m_hdd );
}

ScreenViewMode ScreenView_GetMode()
{
    return m_ScreenMode;
}
void ScreenView_SetMode(ScreenViewMode newMode)
{
    m_ScreenMode = newMode;
}

void ScreenView_CreateDisplay()
{
    ASSERT(g_hwnd != NULL);

    m_cxScreenWidth = UKNC_SCREEN_WIDTH;
    m_cyScreenHeight = UKNC_SCREEN_HEIGHT;

    HDC hdc = GetDC( g_hwnd );

    m_bmpinfo.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
    m_bmpinfo.bmiHeader.biWidth = m_cxScreenWidth;
    m_bmpinfo.bmiHeader.biHeight = m_cyScreenHeight;
    m_bmpinfo.bmiHeader.biPlanes = 1;
    m_bmpinfo.bmiHeader.biBitCount = 32;
    m_bmpinfo.bmiHeader.biCompression = BI_RGB;
    m_bmpinfo.bmiHeader.biSizeImage = 0;
    m_bmpinfo.bmiHeader.biXPelsPerMeter = 0;
    m_bmpinfo.bmiHeader.biYPelsPerMeter = 0;
    m_bmpinfo.bmiHeader.biClrUsed = 0;
    m_bmpinfo.bmiHeader.biClrImportant = 0;
    
    m_hbmp = CreateDIBSection( hdc, &m_bmpinfo, DIB_RGB_COLORS, (void **) &m_bits, NULL, 0 );

    ReleaseDC( g_hwnd, hdc );
}

// Create Screen View as child of Main Window
void CreateScreenView(HWND hwndParent, int x, int y)
{
    ASSERT(hwndParent != NULL);

    int cxBorder = ::GetSystemMetrics(SM_CXBORDER);
    int cyBorder = ::GetSystemMetrics(SM_CYBORDER);
    int xLeft = x;
    int yTop = y;
    int cxWidth = UKNC_SCREEN_WIDTH + cxBorder * 2;
    int cyScreenHeight = UKNC_SCREEN_HEIGHT * m_ScreenHeightMode;
    int cyHeight = cyScreenHeight + cyBorder * 2;

    g_hwndScreen = CreateWindow(
            CLASSNAME_SCREENVIEW, NULL,
            WS_CHILD | WS_BORDER | WS_VISIBLE,
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

            ScreenView_PrepareScreen();  //DEBUG
            ScreenView_OnDraw(hdc);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_LBUTTONDOWN:
        SetFocus(hWnd);
        break;
    //case WM_KEYDOWN:
    //    //if ((lParam & (1 << 30)) != 0)  // Auto-repeats should be ignored
    //    //    return (LRESULT) TRUE;
    //    //return (LRESULT) ScreenView_OnKeyEvent(wParam, (lParam & (1 << 24)) != 0, TRUE);
    //    return (LRESULT) TRUE;
    //case WM_KEYUP:
    //    //return (LRESULT) ScreenView_OnKeyEvent(wParam, (lParam & (1 << 24)) != 0, FALSE);
    //    return (LRESULT) TRUE;
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

int ScreenView_GetHeightMode()
{
    return m_ScreenHeightMode;
}
void ScreenView_SetHeightMode(int newHeightMode)
{
    if (m_ScreenHeightMode == newHeightMode) return;

    m_ScreenHeightMode = newHeightMode;

    int cyBorder = ::GetSystemMetrics(SM_CYBORDER);
    int cyHeight = UKNC_SCREEN_HEIGHT * m_ScreenHeightMode + cyBorder * 2;
    RECT rc;  ::GetWindowRect(g_hwndScreen, &rc);
    ::SetWindowPos(g_hwndScreen, NULL, 0,0, rc.right - rc.left, cyHeight, SWP_NOZORDER | SWP_NOMOVE);
}

void ScreenView_OnDraw(HDC hdc)
{
    if (m_bits == NULL) return;

    int dyDst = -1;
    if (m_ScreenHeightMode > 1) dyDst = UKNC_SCREEN_HEIGHT * m_ScreenHeightMode;

    DrawDibDraw(m_hdd, hdc,
        0,0, -1, dyDst,
        &m_bmpinfo.bmiHeader, m_bits, 0,0,
        m_cxScreenWidth, m_cyScreenHeight,
        0);
}

void ScreenView_RedrawScreen()
{
    ScreenView_PrepareScreen();

    HDC hdc = GetDC(g_hwndScreen);
    ScreenView_OnDraw(hdc);
    ::ReleaseDC(g_hwndScreen, hdc);
}

void ScreenView_PrepareScreen()
{
    if (m_bits == NULL) return;

    // Choose color palette depending of screen mode
    //TODO: Вынести switch в ScreenView_SetMode()
    const DWORD* colors;
    switch (m_ScreenMode)
    {
        case RGBScreen:   colors = ScreenView_StandardRGBColors; break;
        case GrayScreen:  colors = ScreenView_GrayColors; break;
        case GRBScreen:   colors = ScreenView_StandardGRBColors; break;
        default:          colors = ScreenView_StandardRGBColors; break;
    }

    Emulator_PrepareScreenRGB32(m_bits, colors);
}

//BOOL ScreenView_OnKeyEvent(WPARAM vkey, BOOL okExtKey, BOOL okPressed)
//{
//    if (! g_okEmulatorRunning)
//        return TRUE;
//
//    BYTE scancode = TranslateVkeyToUkncScan((WORD)vkey, okExtKey, FALSE);
//    if (scancode == 0)
//        return TRUE;  // The key is not processed
//
//    g_pBoard->KeyboardEvent(scancode, okPressed);
//
//    return FALSE;
//}

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

const BYTE arrPcscan2UkncscanLat[256] = {  // ЛАТ
/*       0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f  */
/*0*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0132, 0026, 0000, 0000, 0000, 0153, 0000, 0000, 
/*1*/    0000, 0000, 0000, 0004, 0107, 0000, 0000, 0000, 0000, 0000, 0000, 0006, 0000, 0000, 0000, 0000, 
/*2*/    0113, 0004, 0151, 0172, 0000, 0116, 0154, 0133, 0134, 0000, 0000, 0000, 0000, 0171, 0152, 0000, 
/*3*/    0176, 0030, 0031, 0032, 0013, 0034, 0035, 0016, 0017, 0177, 0000, 0000, 0000, 0000, 0000, 0000, 
/*4*/    0000, 0072, 0076, 0050, 0057, 0033, 0047, 0055, 0156, 0073, 0027, 0052, 0056, 0112, 0054, 0075, 
/*5*/    0053, 0067, 0074, 0111, 0114, 0051, 0137, 0071, 0115, 0070, 0157, 0000, 0000, 0000, 0000, 0000, 
/*6*/    0126, 0127, 0147, 0167, 0130, 0150, 0170, 0125, 0145, 0165, 0025, 0155, 0000, 0005, 0146, 0131, 
/*7*/    0010, 0011, 0012, 0014, 0015, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*8*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*9*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*a*/    0105, 0106, 0046, 0066, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*b*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0173, 0117, 0175, 0135, 0117, 
/*c*/    0007, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*d*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0110, 0135, 0174, 0000, 0000, 
/*e*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*f*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
};
const BYTE arrPcscan2UkncscanRus[256] = {  // РУС
/*       0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f  */
/*0*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0132, 0026, 0000, 0000, 0000, 0153, 0000, 0000, 
/*1*/    0000, 0000, 0000, 0004, 0107, 0000, 0000, 0000, 0000, 0000, 0000, 0006, 0000, 0000, 0000, 0000, 
/*2*/    0113, 0004, 0151, 0172, 0000, 0116, 0154, 0133, 0134, 0000, 0000, 0000, 0000, 0171, 0152, 0000, 
/*3*/    0176, 0030, 0031, 0032, 0013, 0034, 0035, 0016, 0017, 0177, 0000, 0000, 0000, 0000, 0000, 0000, 
/*4*/    0000, 0047, 0073, 0111, 0071, 0051, 0072, 0053, 0074, 0036, 0075, 0056, 0057, 0115, 0114, 0037, 
/*5*/    0157, 0027, 0052, 0070, 0033, 0055, 0112, 0050, 0110, 0054, 0067, 0000, 0000, 0000, 0000, 0000, 
/*6*/    0126, 0127, 0147, 0167, 0130, 0150, 0170, 0125, 0145, 0165, 0025, 0155, 0000, 0005, 0146, 0131, 
/*7*/    0010, 0011, 0012, 0014, 0015, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*8*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*9*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*a*/    0105, 0106, 0046, 0066, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*b*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0137, 0173, 0076, 0175, 0077, 0117, 
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

        // Выбираем таблицу маппинга в зависимости от флага РУС/ЛАТ в УКНЦ
        WORD ukncRegister = g_pBoard->GetKeyboardRegister();
        const BYTE* pTable = ((ukncRegister & KEYB_LAT) != 0) ? arrPcscan2UkncscanLat : arrPcscan2UkncscanRus;

        // Check every key for state change
        for (int scan = 0; scan < 256; scan++)
        {
            BYTE newstate = keys[scan];
            BYTE oldstate = m_ScreenKeyState[scan];
            if ((newstate & 128) != (oldstate & 128))  // Key state changed - key pressed or released
            {
                BYTE pcscan = (BYTE) scan;
                BYTE ukncscan = pTable[pcscan];
                if (ukncscan != 0)
                {
                    BYTE pressed = newstate & 128;
                    WORD keyevent = MAKEWORD(ukncscan, pressed);
                    ScreenView_PutKeyEventToQueue(keyevent);
                }

    //#if !defined(PRODUCT)
    //                TCHAR bufoct[7];  PrintOctalValue(bufoct, ukncscan);
    //                DebugPrintFormat(_T("KeyEvent: pc:0x%02x uknc:%s %x\r\n"), scan, bufoct, (newstate & 128) != 0);
    //#endif
            }
        }

        // Save keyboard state
        ::memcpy(m_ScreenKeyState, keys, 256);
    }

    // Process next event in the keyboard queue
    WORD keyevent = ScreenView_GetKeyEventFromQueue();
    if (keyevent != 0)
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

//// Translate from Windows virtual key code to UKNC keybord scan code
//BYTE TranslateVkeyToUkncScan(WORD vkey, BOOL okExtKey, BOOL orig)
//{
//    const WORD EXTKEY = 0x8000;
//
//    //TCHAR buffer[32];
//    //wsprintf(buffer, _T("vkey: 0x%x\r\n"), vkey);
//    //DebugPrint(buffer);
//
//    if (okExtKey) vkey |= EXTKEY;
//
//    switch (vkey)
//    {
//    case VK_ESCAPE:     return 0004;  // STOP key
//    case VK_LSHIFT: case VK_RSHIFT:
//                        return 0105;  // HP key
//    case VK_NUMPAD1:    return 0127;
//    case VK_NUMPAD2:    return 0147;
//    case VK_NUMPAD3:    return 0167;
//    case VK_NUMPAD4:    return 0130;
//    case VK_NUMPAD5:    return 0150;
//    case VK_NUMPAD6:    return 0170;
//    case VK_NUMPAD7:    return 0125;
//    case VK_NUMPAD8:    return 0145;
//    case VK_NUMPAD9:    return 0165;
//    case VK_NUMPAD0:    return 0126;
//    case VK_ADD:        return 0131;  // Numpad +
//    //TODO: Numpad . 110
//    case VK_RETURN | EXTKEY:  return 0166;  // Numpad VVOD (Enter)
//    case 192 /*~*/:     return 0006;  // AP2 key
//    case VK_TAB:        return 0026;  // TAB key
//    case VK_F1:         return 0010;  // K1 / K6
//    case VK_F2:         return 0011;  // K2 / K7
//    case VK_F3:         return 0012;  // K3 / K8
//    case VK_F4:         return 0014;  // K4 / K9
//    case VK_F5:         return 0015;  // K5 / K10
//    case VK_F6:         return 0152;  // POM key
//    case VK_F7:         return 0151;  // ISP key
//    //case VK_??:         return 0172;  // PS (ENTER) key
//    case VK_F11:        return 0171;  // SBROS (RESET) key
//    //case VK_??:         return 0066;  // GRAF key
//    //case VK_??:         return 0106;  // ALF key
//    //case VK_??:         return 0107;  // FIKS key
//    case VK_CONTROL:    return 0046;  // SU (UPR) key
//    case VK_SPACE:      return 0113;  // SPACE
//    case VK_BACK:       return 0132;  // ZB (BACKSPACE) key
//    case VK_RETURN:     return 0153;  // VVOD (ENTER)
//    case VK_DOWN  | EXTKEY:  return 0134;  // Down arrow
//    case VK_UP    | EXTKEY:  return 0154;  // Up arrow
//    case VK_LEFT  | EXTKEY:  return 0116;  // Left arrow
//    case VK_RIGHT | EXTKEY:  return 0133;  // Right arrow
//    //TODO:             return 0007;  // ; / +
//    //TODO: case 0x?? /*?*/:  return 0173;  // / / ?
//    //TODO: case 0x?? /*?*/:  return 0135;  // . / >
//    //TODO: case 0x?? /*?*/:  return 0155;  // : / *
//    //TODO: case 0x?? /*?*/:  return 0175;  // - / =
//    case 0xBF /*/*/:    return 0117;  // , / <
//
//    case 0x31 /*1*/:    return 0030;  // 1 / !
//    case 0x32 /*2*/:    return 0031;  // 2 / "
//    case 0x33 /*3*/:    return 0032;  // 3 / #
//    case 0x34 /*4*/:    return 0013;  // 4 / turtle
//    case 0x35 /*5*/:    return 0034;  // 5 / %
//    case 0x36 /*6*/:    return 0035;  // 6 / &
//    case 0x37 /*7*/:    return 0016;  // 7 / '
//    case 0x38 /*8*/:    return 0017;  // 8 / (
//    case 0x39 /*9*/:    return 0177;  // 9 / )
//    case 0x30 /*0*/:    return 0176;  // 0
//    }
//
//    if (orig)  // Original UKNC key mapping
//    {
//        switch (vkey)
//        {
//        case 0x51 /*Q*/:    return 0027;  // Й / J
//        case 0x57 /*W*/:    return 0050;  // Ц / C
//        case 0x45 /*E*/:    return 0051;  // У / U
//        case 0x52 /*R*/:    return 0052;  // К / K
//        case 0x54 /*T*/:    return 0033;  // Е / E
//        case 0x59 /*Y*/:    return 0054;  // Н / N
//        case 0x55 /*U*/:    return 0055;  // Г / G
//        case 0x49 /*I*/:    return 0036;  // Ш / [
//        case 0x4F /*O*/:    return 0037;  // Щ / ]
//        case 0x50 /*P*/:    return 0157;  // З / Z
//        case 0xDB /*[*/:    return 0156;  // Х / H
//        case 0xDD /*]*/:    return 0174;  // Ъ
//
//        case 0x41 /*A*/:    return 0047;  // Ф / F
//        case 0x53 /*S*/:    return 0070;  // Ы / Y
//        case 0x44 /*D*/:    return 0071;  // В / W
//        case 0x46 /*F*/:    return 0072;  // А / A
//        case 0x47 /*G*/:    return 0053;  // П / P
//        case 0x48 /*H*/:    return 0074;  // Р / R
//        case 0x4A /*J*/:    return 0075;  // О / O
//        case 0x4B /*K*/:    return 0056;  // Л / L
//        case 0x4C /*L*/:    return 0057;  // Д / D
//        case 0xBA /*;*/:    return 0137;  // Ж / V
//        case 0xDE /*?*/:    return 0136;  // Э / \
//
//        case 0x5A /*Z*/:    return 0067;  // Я / Q
//        case 0x58 /*X*/:    return 0110;  // Ч / ^
//        case 0x43 /*C*/:    return 0111;  // С / S
//        case 0x56 /*V*/:    return 0112;  // М / M
//        case 0x42 /*B*/:    return 0073;  // И / I
//        case 0x4E /*N*/:    return 0114;  // Т / T
//        case 0x4D /*M*/:    return 0115;  // Ь / X
//        case 0xBC /*,*/:    return 0076;  // Б / B
//        case 0xBE /*.*/:    return 0077;  // Ю / @
//        }
//    }
//    else  // PC key mapping
//    {
//        switch (vkey)
//        {
//        case 0x4A /*J*/:    return 0027;  // Й / J
//        case 0x43 /*C*/:    return 0050;  // Ц / C
//        case 0x55 /*U*/:    return 0051;  // У / U
//        case 0x4B /*K*/:    return 0052;  // К / K
//        case 0x45 /*E*/:    return 0033;  // Е / E
//        case 0x4E /*N*/:    return 0054;  // Н / N
//        case 0x47 /*G*/:    return 0055;  // Г / G
//        case 0xDB /*[*/:    return 0036;  // Ш / [
//        case 0xDD /*]*/:    return 0037;  // Щ / ]
//        case 0x5A /*Z*/:    return 0157;  // З / Z
//        case 0x48 /*H*/:    return 0156;  // Х / H
//        //case ???:    return 0174;  // Ъ
//
//        case 0x46 /*F*/:    return 0047;  // Ф / F
//        case 0x59 /*Y*/:    return 0070;  // Ы / Y
//        case 0x57 /*W*/:    return 0071;  // В / W
//        case 0x41 /*A*/:    return 0072;  // А / A
//        case 0x50 /*P*/:    return 0053;  // П / P
//        case 0x52 /*R*/:    return 0074;  // Р / R
//        case 0x4F /*O*/:    return 0075;  // О / O
//        case 0x4C /*L*/:    return 0056;  // Л / L
//        case 0x44 /*D*/:    return 0057;  // Д / D
//        case 0x56 /*V*/:    return 0137;  // Ж / V
//        //case ???:    return 0136;  // Э / \
//
//        case 0x51 /*Q*/:    return 0067;  // Я / Q
//        //case ???:    return 0110;  // Ч / ^
//        case 0x53 /*S*/:    return 0111;  // С / S
//        case 0x4D /*M*/:    return 0112;  // М / M
//        case 0x49 /*I*/:    return 0073;  // И / I
//        case 0x54 /*T*/:    return 0114;  // Т / T
//        case 0x58 /*X*/:    return 0115;  // Ь / X
//        case 0x42 /*B*/:    return 0076;  // Б / B
//        //case ???:    return 0077;  // Ю / @
//        }
//    }
//
//    return 0;
//}

void ScreenView_SaveScreenshot(LPCTSTR sFileName)
{
    ASSERT(sFileName != NULL);
    ASSERT(m_bits != NULL);

    // Create file
    HANDLE hFile = ::CreateFile(sFileName,
            GENERIC_WRITE, FILE_SHARE_READ, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    //TODO: Check if hFile == INVALID_HANDLE_VALUE

    BITMAPFILEHEADER hdr;
    ::ZeroMemory(&hdr, sizeof(hdr));
    hdr.bfType = 0x4d42;  // "BM"
    BITMAPINFOHEADER bih;
    ::ZeroMemory(&bih, sizeof(bih));
    bih.biSize = sizeof( BITMAPINFOHEADER );
    bih.biWidth = m_cxScreenWidth;
    bih.biHeight = m_cyScreenHeight;
    bih.biSizeImage = bih.biWidth * bih.biHeight * 4;
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    bih.biXPelsPerMeter = bih.biXPelsPerMeter = 2000;
    hdr.bfSize = (DWORD) sizeof(BITMAPFILEHEADER) + bih.biSize + bih.biSizeImage;
    hdr.bfOffBits = (DWORD) sizeof(BITMAPFILEHEADER) + bih.biSize;

    DWORD dwBytesWritten = 0;

    WriteFile(hFile, &hdr, sizeof(BITMAPFILEHEADER), &dwBytesWritten, NULL);
    //TODO: Check if dwBytesWritten != sizeof(BITMAPFILEHEADER)
    WriteFile(hFile, &bih, sizeof(BITMAPINFOHEADER), &dwBytesWritten, NULL);
    //TODO: Check if dwBytesWritten != sizeof(BITMAPINFOHEADER)
    WriteFile(hFile, m_bits, bih.biSizeImage, &dwBytesWritten, NULL);
    //TODO: Check if dwBytesWritten != bih.biSizeImage

    // Close file
    CloseHandle(hFile);
}


//////////////////////////////////////////////////////////////////////
