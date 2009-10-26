// DisasmView.cpp

#include "stdafx.h"
#include "UKNCBTL.h"
#include "Views.h"
#include "ToolWindow.h"
#include "Dialogs.h"
#include "Emulator.h"
#include "emubase\Emubase.h"


//////////////////////////////////////////////////////////////////////

// Colors
#define COLOR_BLUE  RGB(0,0,255)


HWND g_hwndDisasm = (HWND) INVALID_HANDLE_VALUE;  // Disasm View window handle
WNDPROC m_wndprocDisasmToolWindow = NULL;  // Old window proc address of the ToolWindow

HWND m_hwndDisasmViewer = (HWND) INVALID_HANDLE_VALUE;


BOOL m_okDisasmProcessor = FALSE;  // TRUE - CPU, FALSE - PPU
WORD m_wDisasmBaseAddr = 0;
WORD m_wDisasmNextBaseAddr = 0;

void DoDrawDisasmView(HDC hdc);
void DrawDisassemble(HDC hdc, CProcessor* pProc, WORD base, WORD previous, int x, int y);
void DisasmView_UpdateWindowText();
BOOL DisasmView_OnKeyDown(WPARAM vkey, LPARAM lParam);
void DisasmView_SetBaseAddr(WORD base);


//////////////////////////////////////////////////////////////////////


void DisasmView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= DisasmViewViewerWndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= g_hInst;
    wcex.hIcon			= NULL;
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= CLASSNAME_DISASMVIEW;
    wcex.hIconSm		= NULL;

    RegisterClassEx(&wcex);
}

void CreateDisasmView(HWND hwndParent, int x, int y, int width, int height)
{
    ASSERT(hwndParent != NULL);

    g_hwndDisasm = CreateWindow(
            CLASSNAME_TOOLWINDOW, NULL,
            WS_CHILD | WS_VISIBLE,
            x, y, width, height,
            hwndParent, NULL, g_hInst, NULL);
	DisasmView_UpdateWindowText();

    // ToolWindow subclassing
    m_wndprocDisasmToolWindow = (WNDPROC) LongToPtr( SetWindowLongPtr(
            g_hwndDisasm, GWLP_WNDPROC, PtrToLong(DisasmViewWndProc)) );

    RECT rcClient;  GetClientRect(g_hwndDisasm, &rcClient);

	m_hwndDisasmViewer = CreateWindowEx(
            WS_EX_STATICEDGE,
            CLASSNAME_DISASMVIEW, NULL,
            WS_CHILD | WS_VISIBLE,
            0, 0, rcClient.right, rcClient.bottom,
            g_hwndDisasm, NULL, g_hInst, NULL);
}

LRESULT CALLBACK DisasmViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_DESTROY:
        g_hwndDisasm = (HWND) INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
        return CallWindowProc(m_wndprocDisasmToolWindow, hWnd, message, wParam, lParam);
    default:
        return CallWindowProc(m_wndprocDisasmToolWindow, hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

LRESULT CALLBACK DisasmViewViewerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            DoDrawDisasmView(hdc);  // Draw memory dump

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_LBUTTONDOWN:
        SetFocus(hWnd);
        break;
    case WM_KEYDOWN:
        return (LRESULT) DisasmView_OnKeyDown(wParam, lParam);
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

BOOL DisasmView_OnKeyDown(WPARAM vkey, LPARAM lParam)
{
    switch (vkey)
    {
    case VK_SPACE:
        DisasmView_SetCurrentProc(!m_okDisasmProcessor);
        DebugView_SetCurrentProc(m_okDisasmProcessor);   // Switch DebugView to current processor
        break;
    case VK_DOWN:
        DisasmView_SetBaseAddr(m_wDisasmNextBaseAddr);
        break;
    case 0x47:  // G - Go To Address
        {
            WORD value = m_wDisasmBaseAddr;
            if (InputBoxOctal(m_hwndDisasmViewer, _T("Go To Address"), _T("Address (octal):"), &value))
                DisasmView_SetBaseAddr(value);
            break;
        }
    case VK_ESCAPE:
        ConsoleView_Activate();
        break;
    default:
        return TRUE;
    }
    return FALSE;
}

void DisasmView_UpdateWindowText()
{
    CProcessor* pDisasmPU = (m_okDisasmProcessor) ? g_pBoard->GetCPU() : g_pBoard->GetPPU();
    ASSERT(pDisasmPU != NULL);
    LPCTSTR sProcName = pDisasmPU->GetName();

	TCHAR buffer[64];
	swprintf_s(buffer, 64, _T("Disassemble - %s"), sProcName);
	::SetWindowText(g_hwndDisasm, buffer);
}


//////////////////////////////////////////////////////////////////////


// Update after Run or Step
void DisasmView_OnUpdate()
{
    CProcessor* pDisasmPU = (m_okDisasmProcessor) ? g_pBoard->GetCPU() : g_pBoard->GetPPU();
    ASSERT(pDisasmPU != NULL);
	m_wDisasmBaseAddr = pDisasmPU->GetPC();
}

void DisasmView_SetCurrentProc(BOOL okCPU)
{
    m_okDisasmProcessor = okCPU;
    CProcessor* pDisasmPU = (m_okDisasmProcessor) ? g_pBoard->GetCPU() : g_pBoard->GetPPU();
    ASSERT(pDisasmPU != NULL);
    m_wDisasmBaseAddr = pDisasmPU->GetPC();
    InvalidateRect(m_hwndDisasmViewer, NULL, TRUE);
	DisasmView_UpdateWindowText();
}

void DisasmView_SetBaseAddr(WORD base)
{
    m_wDisasmBaseAddr = base;
    InvalidateRect(m_hwndDisasmViewer, NULL, TRUE);
}


//////////////////////////////////////////////////////////////////////
// Draw functions

void DoDrawDisasmView(HDC hdc)
{
    ASSERT(g_pBoard != NULL);

    // Create and select font
    HFONT hFont = CreateMonospacedFont();
    HGDIOBJ hOldFont = SelectObject(hdc, hFont);
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorOld = SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
    COLORREF colorBkOld = SetBkColor(hdc, GetSysColor(COLOR_WINDOW));

    CProcessor* pDisasmPU = (m_okDisasmProcessor) ? g_pBoard->GetCPU() : g_pBoard->GetPPU();

    // Draw disasseble for the current processor
    WORD prevPC = (m_okDisasmProcessor) ? g_wEmulatorPrevCpuPC : g_wEmulatorPrevPpuPC;
    DrawDisassemble(hdc, pDisasmPU, m_wDisasmBaseAddr, prevPC, 0, 2 + 0 * cyLine);

	SetTextColor(hdc, colorOld);
    SetBkColor(hdc, colorBkOld);
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

void DrawDisassemble(HDC hdc, CProcessor* pProc, WORD base, WORD previous, int x, int y)
{
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorText = GetSysColor(COLOR_WINDOWTEXT);

    CMemoryController* pMemCtl = pProc->GetMemoryController();
    WORD proccurrent = pProc->GetPC();
    WORD current = base;

    // Читаем из памяти процессора в буфер
    const int nWindowSize = 30;
    WORD memory[nWindowSize + 2];
    for (int idx = 0; idx < nWindowSize; idx++) {
        BOOL okValidAddress;
        memory[idx] = pMemCtl->GetWordView(
                current + idx * 2 - 10, pProc->IsHaltMode(), TRUE, &okValidAddress);
    }

    //TextOut(hdc, x, y, _T(" address  value   instruction"), 29);
    //y += cyLine;

    WORD address = current - 10;
    WORD disasmfrom = current;
    if (previous >= address && previous < current)
        disasmfrom = previous;

    int length = 0;
    WORD wNextBaseAddr = 0;
    for (int index = 0; index < nWindowSize; index++) {  // Рисуем строки
        DrawOctalValue(hdc, x + 5 * cxChar, y, address);  // Address
        // Value at the address
        WORD value = memory[index];
        DrawOctalValue(hdc, x + 13 * cxChar, y, value);
        // Current position
        if (address == current)
            TextOut(hdc, x + 1 * cxChar, y, _T("  >"), 3);
        if (address == proccurrent)
            TextOut(hdc, x + 1 * cxChar, y, _T("PC>>"), 4);
        else if (address == previous)
        {
            ::SetTextColor(hdc, COLOR_BLUE);
            TextOut(hdc, x + 1 * cxChar, y, _T("  > "), 4);
        }

        if (address >= disasmfrom && length == 0) {
            TCHAR strInstr[8];
            TCHAR strArg[32];
            length = DisassembleInstruction(memory + index, address, strInstr, strArg);
            if (index + length <= nWindowSize)
            {
                TextOut(hdc, x + 21 * cxChar, y, strInstr, (int) wcslen(strInstr));
                TextOut(hdc, x + 29 * cxChar, y, strArg, (int) wcslen(strArg));
            }
            ::SetTextColor(hdc, colorText);

            if (wNextBaseAddr == 0)
                wNextBaseAddr = address + length * 2;
        }
        if (length > 0) length--;

        address += 2;
        y += cyLine;
    }

    m_wDisasmNextBaseAddr = wNextBaseAddr;
}


//////////////////////////////////////////////////////////////////////
