/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// DebugView.cpp

#include "stdafx.h"
#include <commctrl.h>
#include "Main.h"
#include "Views.h"
#include "ToolWindow.h"
#include "Emulator.h"
#include "emubase\Emubase.h"


//////////////////////////////////////////////////////////////////////


HWND g_hwndDebug = (HWND) INVALID_HANDLE_VALUE;  // Debug View window handle
WNDPROC m_wndprocDebugToolWindow = NULL;  // Old window proc address of the ToolWindow

HWND m_hwndDebugViewer = (HWND) INVALID_HANDLE_VALUE;
HWND m_hwndDebugToolbar = (HWND) INVALID_HANDLE_VALUE;

BOOL m_okDebugProcessor = FALSE;  // TRUE - CPU, FALSE - PPU
WORD m_wDebugCpuR[11];  // Saved register values - R0..R7, PSW, CPC, CPSW
WORD m_wDebugPpuR[11];  // Saved register values - R0..R7, PSW, CPC, CPSW
BOOL m_okDebugCpuRChanged[11];  // Register change flags
BOOL m_okDebugPpuRChanged[11];  // Register change flags
WORD m_wDebugCpuPswOld;  // PSW value on previous step
WORD m_wDebugPpuPswOld;  // PSW value on previous step
WORD m_wDebugCpuR6Old;  // SP value on previous step
WORD m_wDebugPpuR6Old;  // SP value on previous step

void DebugView_DoDraw(HDC hdc);
BOOL DebugView_OnKeyDown(WPARAM vkey, LPARAM lParam);
void DebugView_DrawProcessor(HDC hdc, const CProcessor* pProc, int x, int y, WORD* arrR, BOOL* arrRChanged, WORD oldPsw);
void DebugView_DrawMemoryForRegister(HDC hdc, int reg, CProcessor* pProc, int x, int y, WORD oldValue);
void DebugView_DrawPorts(HDC hdc, BOOL okProcessor, const CMemoryController* pMemCtl, CMotherboard* pBoard, int x, int y);
void DebugView_DrawChannels(HDC hdc, int x, int y);
BOOL DebugView_DrawBreakpoints(HDC hdc, int x, int y);
void DebugView_DrawCPUMemoryMap(HDC hdc, int x, int y, BOOL okHalt);
void DebugView_DrawPPUMemoryMap(HDC hdc, int x, int y, const CMemoryController* pMemCtl);
void DebugView_UpdateWindowText();


//////////////////////////////////////////////////////////////////////


void DebugView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = DebugViewViewerWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInst;
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = CLASSNAME_DEBUGVIEW;
    wcex.hIconSm        = NULL;

    RegisterClassEx(&wcex);
}

void DebugView_Init()
{
    memset(m_wDebugCpuR, 255, sizeof(m_wDebugCpuR));
    memset(m_okDebugCpuRChanged, 1, sizeof(m_okDebugCpuRChanged));
    memset(m_wDebugPpuR, 255, sizeof(m_wDebugPpuR));
    memset(m_okDebugPpuRChanged, 1, sizeof(m_okDebugPpuRChanged));
    m_wDebugCpuPswOld = m_wDebugPpuPswOld = 0;
    m_wDebugCpuR6Old = m_wDebugPpuR6Old = 0;

    m_okDebugProcessor = Settings_GetDebugCpuPpu();
}

void DebugView_Create(HWND hwndParent, int x, int y, int width, int height)
{
    ASSERT(hwndParent != NULL);

    g_hwndDebug = CreateWindow(
            CLASSNAME_TOOLWINDOW, NULL,
            WS_CHILD | WS_VISIBLE,
            x, y, width, height,
            hwndParent, NULL, g_hInst, NULL);
    DebugView_UpdateWindowText();

    // ToolWindow subclassing
    m_wndprocDebugToolWindow = (WNDPROC) LongToPtr( SetWindowLongPtr(
            g_hwndDebug, GWLP_WNDPROC, PtrToLong(DebugViewWndProc)) );

    RECT rcClient;  GetClientRect(g_hwndDebug, &rcClient);

    m_hwndDebugViewer = CreateWindowEx(
            WS_EX_STATICEDGE,
            CLASSNAME_DEBUGVIEW, NULL,
            WS_CHILD | WS_VISIBLE,
            0, 0, rcClient.right, rcClient.bottom,
            g_hwndDebug, NULL, g_hInst, NULL);

    m_hwndDebugToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
            WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT | TBSTYLE_TOOLTIPS | CCS_NOPARENTALIGN | CCS_NODIVIDER | CCS_VERT,
            4, 4, 32, rcClient.bottom, m_hwndDebugViewer,
            (HMENU) 102,
            g_hInst, NULL);

    TBADDBITMAP addbitmap;
    addbitmap.hInst = g_hInst;
    addbitmap.nID = IDB_TOOLBAR;
    SendMessage(m_hwndDebugToolbar, TB_ADDBITMAP, 2, (LPARAM) &addbitmap);

    SendMessage(m_hwndDebugToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM) sizeof(TBBUTTON), 0);
    SendMessage(m_hwndDebugToolbar, TB_SETBUTTONSIZE, 0, (LPARAM) MAKELONG (26, 26));

    TBBUTTON buttons[6];
    ZeroMemory(buttons, sizeof(buttons));
    for (int i = 0; i < sizeof(buttons) / sizeof(TBBUTTON); i++)
    {
        buttons[i].fsState = TBSTATE_ENABLED | TBSTATE_WRAP;
        buttons[i].fsStyle = BTNS_BUTTON;
        buttons[i].iString = -1;
    }
    buttons[0].idCommand = ID_VIEW_DEBUG;
    buttons[0].iBitmap = ToolbarImageDebugger;
    buttons[0].fsState = TBSTATE_ENABLED | TBSTATE_WRAP | TBSTATE_CHECKED;
    buttons[1].idCommand = ID_DEBUG_CPUPPU;
    buttons[1].iBitmap = ToolbarImageCpuPpu;
    buttons[2].idCommand = ID_DEBUG_SPRITES;
    buttons[2].iBitmap = ToolbarImageSpriteViewer;
    buttons[3].fsStyle = BTNS_SEP;
    buttons[4].idCommand = ID_DEBUG_STEPINTO;
    buttons[4].iBitmap = ToolbarImageStepInto;
    buttons[5].idCommand = ID_DEBUG_STEPOVER;
    buttons[5].iBitmap = ToolbarImageStepOver;

    SendMessage(m_hwndDebugToolbar, TB_ADDBUTTONS, (WPARAM) sizeof(buttons) / sizeof(TBBUTTON), (LPARAM) &buttons);
}

void DebugView_Redraw()
{
    RedrawWindow(g_hwndDebug, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

// Adjust position of client windows
void DebugView_AdjustWindowLayout()
{
    RECT rc;  GetClientRect(g_hwndDebug, &rc);

    if (m_hwndDebugViewer != (HWND) INVALID_HANDLE_VALUE)
        SetWindowPos(m_hwndDebugViewer, NULL, 0, 0, rc.right, rc.bottom, SWP_NOZORDER);
}

LRESULT CALLBACK DebugViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    LRESULT lResult;
    switch (message)
    {
    case WM_DESTROY:
        g_hwndDebug = (HWND) INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
        return CallWindowProc(m_wndprocDebugToolWindow, hWnd, message, wParam, lParam);
    case WM_SIZE:
        lResult = CallWindowProc(m_wndprocDebugToolWindow, hWnd, message, wParam, lParam);
        DebugView_AdjustWindowLayout();
        return lResult;
    default:
        return CallWindowProc(m_wndprocDebugToolWindow, hWnd, message, wParam, lParam);
    }
    //return (LRESULT)FALSE;
}

LRESULT CALLBACK DebugViewViewerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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

            DebugView_DoDraw(hdc);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_LBUTTONDOWN:
        SetFocus(hWnd);
        break;
    case WM_KEYDOWN:
        return (LRESULT) DebugView_OnKeyDown(wParam, lParam);
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        ::InvalidateRect(hWnd, NULL, TRUE);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

BOOL DebugView_OnKeyDown(WPARAM vkey, LPARAM /*lParam*/)
{
    switch (vkey)
    {
    case VK_SPACE:
        DebugView_SwitchCpuPpu();
        break;
    case VK_ESCAPE:
        ConsoleView_Activate();
        break;
    default:
        return TRUE;
    }
    return FALSE;
}

void DebugView_UpdateWindowText()
{
    CProcessor* pDebugPU = (m_okDebugProcessor) ? g_pBoard->GetCPU() : g_pBoard->GetPPU();
    ASSERT(pDebugPU != NULL);
    LPCTSTR sProcName = pDebugPU->GetName();

    TCHAR buffer[64];
    _stprintf_s(buffer, 64, _T("Debug - %s"), sProcName);
    ::SetWindowText(g_hwndDebug, buffer);
}

void DebugView_SwitchCpuPpu()
{
    m_okDebugProcessor = ! m_okDebugProcessor;
    InvalidateRect(m_hwndDebugViewer, NULL, TRUE);
    DebugView_UpdateWindowText();

    DisasmView_SetCurrentProc(m_okDebugProcessor);
    ConsoleView_SetCurrentProc(m_okDebugProcessor);

    Settings_SetDebugCpuPpu(m_okDebugProcessor);
}


//////////////////////////////////////////////////////////////////////

// Update after Run or Step
void DebugView_OnUpdate()
{
    CProcessor* pCPU = g_pBoard->GetCPU();
    ASSERT(pCPU != nullptr);

    // Get new register values and set change flags
    m_wDebugCpuR6Old = m_wDebugCpuR[6];
    for (int r = 0; r < 8; r++)
    {
        WORD value = pCPU->GetReg(r);
        m_okDebugCpuRChanged[r] = (m_wDebugCpuR[r] != value);
        m_wDebugCpuR[r] = value;
    }
    WORD pswCPU = pCPU->GetPSW();
    m_okDebugCpuRChanged[8] = (m_wDebugCpuR[8] != pswCPU);
    m_wDebugCpuPswOld = m_wDebugCpuR[8];
    m_wDebugCpuR[8] = pswCPU;
    WORD cpcCPU = pCPU->GetCPC();
    m_okDebugCpuRChanged[9] = (m_wDebugCpuR[9] != cpcCPU);
    m_wDebugCpuR[9] = cpcCPU;
    WORD cpswCPU = pCPU->GetCPSW();
    m_okDebugCpuRChanged[10] = (m_wDebugCpuR[10] != cpswCPU);
    m_wDebugCpuR[10] = cpswCPU;

    CProcessor* pPPU = g_pBoard->GetPPU();
    ASSERT(pPPU != NULL);

    // Get new register values and set change flags
    m_wDebugPpuR6Old = m_wDebugPpuR[6];
    for (int r = 0; r < 8; r++)
    {
        WORD value = pPPU->GetReg(r);
        m_okDebugPpuRChanged[r] = (m_wDebugPpuR[r] != value);
        m_wDebugPpuR[r] = value;
    }
    WORD pswPPU = pPPU->GetPSW();
    m_okDebugPpuRChanged[8] = (m_wDebugPpuR[8] != pswPPU);
    m_wDebugPpuPswOld = m_wDebugPpuR[8];
    m_wDebugPpuR[8] = pswPPU;
    WORD cpcPPU = pPPU->GetCPC();
    m_okDebugPpuRChanged[9] = (m_wDebugPpuR[9] != cpcPPU);
    m_wDebugPpuR[9] = cpcPPU;
    WORD cpswPPU = pPPU->GetCPSW();
    m_okDebugPpuRChanged[10] = (m_wDebugPpuR[10] != cpswPPU);
    m_wDebugPpuR[10] = cpswPPU;
}

void DebugView_SetCurrentProc(BOOL okCPU)
{
    m_okDebugProcessor = okCPU;
    InvalidateRect(m_hwndDebugViewer, NULL, TRUE);
    DebugView_UpdateWindowText();

    Settings_SetDebugCpuPpu(m_okDebugProcessor);
}


//////////////////////////////////////////////////////////////////////
// Draw functions

void DebugView_DoDraw(HDC hdc)
{
    ASSERT(g_pBoard != nullptr);

    // Create and select font
    HFONT hFont = CreateMonospacedFont();
    HGDIOBJ hOldFont = SelectObject(hdc, hFont);
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorOld = SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
    COLORREF colorBkOld = SetBkColor(hdc, GetSysColor(COLOR_WINDOW));

    CProcessor* pDebugPU = (m_okDebugProcessor) ? g_pBoard->GetCPU() : g_pBoard->GetPPU();
    ASSERT(pDebugPU != nullptr);
    WORD* arrR = (m_okDebugProcessor) ? m_wDebugCpuR : m_wDebugPpuR;
    BOOL* arrRChanged = (m_okDebugProcessor) ? m_okDebugCpuRChanged : m_okDebugPpuRChanged;
    WORD oldPsw = (m_okDebugProcessor) ? m_wDebugCpuPswOld : m_wDebugPpuPswOld;
    WORD oldSP = (m_okDebugProcessor) ? m_wDebugCpuR6Old : m_wDebugPpuR6Old;

    //LPCTSTR sProcName = pDebugPU->GetName();
    //TextOut(hdc, cxChar * 1, 2 + 1 * cyLine, sProcName, 3);

    DebugView_DrawProcessor(hdc, pDebugPU, 30 + cxChar * 2, 2 + 1 * cyLine, arrR, arrRChanged, oldPsw);

    // Draw stack for the current processor
    DebugView_DrawMemoryForRegister(hdc, 6, pDebugPU, 30 + 35 * cxChar, 2 + 0 * cyLine, oldSP);

    CMemoryController* pDebugMemCtl = pDebugPU->GetMemoryController();
    DebugView_DrawPorts(hdc, m_okDebugProcessor, pDebugMemCtl, g_pBoard, 30 + 54 * cxChar, 2 + 0 * cyLine);

    //DebugView_DrawChannels(hdc, 75 * cxChar, 2 + 0 * cyLine);

    BOOL okBreakpoints = DebugView_DrawBreakpoints(hdc, 30 + 70 * cxChar, 0 * cyLine);

    int xMemoryMap = 30 + (70 + (okBreakpoints ? 10 : 0)) * cxChar;
    if (m_okDebugProcessor)
        DebugView_DrawCPUMemoryMap(hdc, xMemoryMap, 0 * cyLine, pDebugPU->IsHaltMode());
    else
        DebugView_DrawPPUMemoryMap(hdc, xMemoryMap, 0 * cyLine, pDebugMemCtl);

    SetTextColor(hdc, colorOld);
    SetBkColor(hdc, colorBkOld);
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);

    if (::GetFocus() == m_hwndDebugViewer)
    {
        RECT rcClient;
        GetClientRect(m_hwndDebugViewer, &rcClient);
        DrawFocusRect(hdc, &rcClient);
    }
}

void DebugView_DrawRectangle(HDC hdc, int x1, int y1, int x2, int y2)
{
    HGDIOBJ hOldBrush = ::SelectObject(hdc, ::GetSysColorBrush(COLOR_BTNSHADOW));
    PatBlt(hdc, x1, y1, x2 - x1, 1, PATCOPY);
    PatBlt(hdc, x1, y1, 1, y2 - y1, PATCOPY);
    PatBlt(hdc, x1, y2, x2 - x1, 1, PATCOPY);
    PatBlt(hdc, x2, y1, 1, y2 - y1 + 1, PATCOPY);
    ::SelectObject(hdc, hOldBrush);
}

void DebugView_DrawProcessor(HDC hdc, const CProcessor* pProc, int x, int y, WORD* arrR, BOOL* arrRChanged, WORD oldPsw)
{
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorText = Settings_GetColor(ColorDebugText);
    COLORREF colorChanged = Settings_GetColor(ColorDebugValueChanged);
    ::SetTextColor(hdc, colorText);

    DebugView_DrawRectangle(hdc, x - cxChar, y - 8, x + cxChar + 31 * cxChar, y + 8 + cyLine * 14);

    // Registers
    for (int r = 0; r < 8; r++)
    {
        ::SetTextColor(hdc, arrRChanged[r] ? colorChanged : colorText);

        LPCTSTR strRegName = REGISTER_NAME[r];
        TextOut(hdc, x, y + r * cyLine, strRegName, (int) _tcslen(strRegName));

        WORD value = arrR[r]; //pProc->GetReg(r);
        DrawOctalValue(hdc, x + cxChar * 3, y + r * cyLine, value);
        DrawHexValue(hdc, x + cxChar * 10, y + r * cyLine, value);
        DrawBinaryValue(hdc, x + cxChar * 15, y + r * cyLine, value);
    }
    ::SetTextColor(hdc, colorText);

    // CPC value
    ::SetTextColor(hdc, arrRChanged[9] ? colorChanged : colorText);
    TextOut(hdc, x, y + 8 * cyLine, _T("PC'"), 3);
    WORD cpc = arrR[9];
    DrawOctalValue(hdc, x + cxChar * 3, y + 8 * cyLine, cpc);
    DrawHexValue(hdc, x + cxChar * 10, y + 8 * cyLine, cpc);
    DrawBinaryValue(hdc, x + cxChar * 15, y + 8 * cyLine, cpc);

    // PSW value
    ::SetTextColor(hdc, arrRChanged[8] ? colorChanged : colorText);
    TextOut(hdc, x, y + 10 * cyLine, _T("PS"), 2);
    WORD psw = arrR[8]; // pProc->GetPSW();
    DrawOctalValue(hdc, x + cxChar * 3, y + 10 * cyLine, psw);
    //DrawHexValue(hdc, x + cxChar * 10, y + 10 * cyLine, psw);
    ::SetTextColor(hdc, colorText);
    TextOut(hdc, x + cxChar * 15, y + 9 * cyLine, _T("       HP  TNZVC"), 16);

    // PSW value bits colored bit-by-bit
    TCHAR buffera[2];  buffera[1] = 0;
    for (int i = 0; i < 16; i++)
    {
        WORD bitpos = 1 << i;
        buffera[0] = (psw & bitpos) ? '1' : '0';
        ::SetTextColor(hdc, ((psw & bitpos) != (oldPsw & bitpos)) ? colorChanged : colorText);
        TextOut(hdc, x + cxChar * (15 + 15 - i), y + 10 * cyLine, buffera, 1);
    }

    // CPSW value
    ::SetTextColor(hdc, arrRChanged[10] ? colorChanged : colorText);
    TextOut(hdc, x, y + 11 * cyLine, _T("PS'"), 3);
    WORD cpsw = arrR[10];
    DrawOctalValue(hdc, x + cxChar * 3, y + 11 * cyLine, cpsw);
    //DrawHexValue(hdc, x + cxChar * 10, y + 11 * cyLine, cpsw);
    DrawBinaryValue(hdc, x + cxChar * 15, y + 11 * cyLine, cpsw);

    ::SetTextColor(hdc, colorText);

    // Processor mode - HALT or USER
    BOOL okHaltMode = pProc->IsHaltMode();
    TextOut(hdc, x, y + 13 * cyLine, okHaltMode ? _T("HALT") : _T("USER"), 4);

    // "Stopped" flag
    BOOL okStopped = pProc->IsStopped();
    if (okStopped)
        TextOut(hdc, x + 6 * cxChar, y + 13 * cyLine, _T("STOP"), 4);
}

void DebugView_DrawAddressAndValue(HDC hdc, const CProcessor* pProc, const CMemoryController* pMemCtl, uint16_t address, int x, int y, int cxChar)
{
    COLORREF colorText = Settings_GetColor(ColorDebugText);
    SetTextColor(hdc, colorText);
    DrawOctalValue(hdc, x + 0 * cxChar, y, address);
    x += 7 * cxChar;

    int addrtype = ADDRTYPE_NONE;
    uint16_t value = pMemCtl->GetWordView(address, pProc->IsHaltMode(), FALSE, &addrtype);
    if (addrtype == ADDRTYPE_RAM0 || (addrtype & ADDRTYPE_MASK_RAM) != 0)
    {
        DrawOctalValue(hdc, x, y, value);
    }
    else if (addrtype == ADDRTYPE_ROM || addrtype == ADDRTYPE_ROMCART1 || addrtype == ADDRTYPE_ROMCART2)
    {
        SetTextColor(hdc, Settings_GetColor(ColorDebugMemoryRom));
        DrawOctalValue(hdc, x, y, value);
    }
    else if (addrtype == ADDRTYPE_IO)
    {
        value = pMemCtl->GetPortView(address);
        SetTextColor(hdc, Settings_GetColor(ColorDebugMemoryIO));
        DrawOctalValue(hdc, x, y, value);
    }
    else //if (addrtype == ADDRTYPE_DENY || addrtype == ADDRTYPE_NONE)
    {
        SetTextColor(hdc, Settings_GetColor(ColorDebugMemoryNA));
        TextOut(hdc, x, y, _T("  NA  "), 6);
    }

    SetTextColor(hdc, colorText);
}

void DebugView_DrawMemoryForRegister(HDC hdc, int reg, CProcessor* pProc, int x, int y, WORD oldValue)
{
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorText = Settings_GetColor(ColorDebugText);
    COLORREF colorChanged = Settings_GetColor(ColorDebugValueChanged);
    COLORREF colorPrev = Settings_GetColor(ColorDebugPrevious);
    COLORREF colorOld = SetTextColor(hdc, colorText);

    uint16_t current = pProc->GetReg(reg) & ~1;
    uint16_t previous = oldValue;
    bool okExec = (reg == 7);

    // Reading from memory into the buffer
    uint16_t memory[16];
    int addrtype[16];
    const CMemoryController* pMemCtl = pProc->GetMemoryController();
    for (int idx = 0; idx < 16; idx++)
    {
        memory[idx] = pMemCtl->GetWordView(
                (uint16_t)(current + idx * 2 - 16), pProc->IsHaltMode(), okExec, addrtype + idx);
    }

    WORD address = current - 16;
    for (int index = 0; index < 16; index++)
    {
        DebugView_DrawAddressAndValue(hdc, pProc, pMemCtl, address, x + 3 * cxChar, y, cxChar);

        if (address == current)  // Current position
        {
            SetTextColor(hdc, colorText);
            TextOut(hdc, x + 2 * cxChar, y, _T(">"), 1);
            if (current != previous) SetTextColor(hdc, colorChanged);
            TextOut(hdc, x, y, REGISTER_NAME[reg], 2);
        }
        else if (address == previous)
        {
            SetTextColor(hdc, colorPrev);
            TextOut(hdc, x + 2 * cxChar, y, _T(">"), 1);
        }

        address += 2;
        y += cyLine;
    }

    SetTextColor(hdc, colorOld);
}

uint16_t m_DebugViewCPUPorts[] =
{
    0176640, 0176642
};
uint16_t m_DebugViewPPUPorts[] =
{
    0177010, 0177012, 0177014, 0177016, 0177020, 0177022, 0177024, 0177026, 0177054, 0177700, 0177716
};

void DebugView_DrawPorts(HDC hdc, BOOL okProcessor, const CMemoryController* pMemCtl, CMotherboard* pBoard, int x, int y)
{
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);

    TextOut(hdc, x, y, _T("Ports:"), 6);

    uint16_t* addresses = okProcessor ? m_DebugViewCPUPorts : m_DebugViewPPUPorts;
    int addrcount = okProcessor
            ? sizeof(m_DebugViewCPUPorts) / sizeof(m_DebugViewCPUPorts[0])
            : sizeof(m_DebugViewPPUPorts) / sizeof(m_DebugViewPPUPorts[0]);
    for (int i = 0; i < addrcount; i++)
    {
        uint16_t address = addresses[i];
        DebugView_DrawAddressAndValue(hdc, pBoard->GetCPU(), pMemCtl, address, x, y + (i + 1) * cyLine, cxChar);
    }

    if (!okProcessor)  // PPU timer
    {
        WORD value177710 = pBoard->GetTimerStateView();
        DrawOctalValue(hdc, x + 0 * cxChar, y + 13 * cyLine, 0177710);
        DrawOctalValue(hdc, x + 7 * cxChar, y + 13 * cyLine, value177710);
        WORD value177712 = pBoard->GetTimerReloadView();
        DrawOctalValue(hdc, x + 0 * cxChar, y + 14 * cyLine, 0177712);
        DrawOctalValue(hdc, x + 7 * cxChar, y + 14 * cyLine, value177712);
        WORD value177714 = pBoard->GetTimerValueView();
        DrawOctalValue(hdc, x + 0 * cxChar, y + 15 * cyLine, 0177714);
        DrawOctalValue(hdc, x + 7 * cxChar, y + 15 * cyLine, value177714);
    }
}

void DebugView_DrawChannels(HDC hdc, int x, int y)
{
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);

    //CProcessor* pCPU = g_pBoard->GetCPU();
    //CProcessor* pPPU = g_pBoard->GetPPU();
    //CMemoryController* pCPUMemCtl = pCPU->GetMemoryController();
    //CMemoryController* pPPUMemCtl = pPPU->GetMemoryController();

    TextOut(hdc, x, y, _T("Channels:"), 9);

    TCHAR bufData[7];
    TCHAR buffer[32];
    chan_stc tmpstc;

    tmpstc = g_pBoard->GetChannelStruct(0, 0, 0);

    PrintOctalValue(bufData, tmpstc.data);
    wsprintf(buffer, _T("PPU CH:0 RX D:%s RDY:%d IRQ:%d"), bufData + 3, tmpstc.ready, tmpstc.irq);
    TextOut(hdc, x, y + 1 * cyLine, buffer, lstrlen(buffer));

    tmpstc = g_pBoard->GetChannelStruct(0, 1, 0);
    PrintOctalValue(bufData, tmpstc.data);
    wsprintf(buffer, _T("PPU CH:1 RX D:%s RDY:%d IRQ:%d"), bufData + 3, tmpstc.ready, tmpstc.irq);
    TextOut(hdc, x, y + 2 * cyLine, buffer, lstrlen(buffer));

    tmpstc = g_pBoard->GetChannelStruct(0, 2, 0);
    PrintOctalValue(bufData, tmpstc.data);
    wsprintf(buffer, _T("PPU CH:2 RX D:%s RDY:%d IRQ:%d"), bufData + 3, tmpstc.ready, tmpstc.irq);
    TextOut(hdc, x, y + 3 * cyLine, buffer, lstrlen(buffer));

    tmpstc = g_pBoard->GetChannelStruct(0, 0, 1);
    wsprintf(buffer, _T("PPU CH:0 TX       RDY:%d IRQ:%d"), tmpstc.ready, tmpstc.irq);
    TextOut(hdc, x, y + 4 * cyLine, buffer, lstrlen(buffer));

    tmpstc = g_pBoard->GetChannelStruct(0, 1, 1);
    wsprintf(buffer, _T("PPU CH:1 TX       RDY:%d IRQ:%d"), tmpstc.ready, tmpstc.irq);
    TextOut(hdc, x, y + 5 * cyLine, buffer, lstrlen(buffer));

    tmpstc = g_pBoard->GetChannelStruct(1, 0, 0);
    PrintOctalValue(bufData, tmpstc.data);
    wsprintf(buffer, _T("CPU CH:0 RX D:%s RDY:%d IRQ:%d"), bufData + 3, tmpstc.ready, tmpstc.irq);
    TextOut(hdc, x, y + 6 * cyLine, buffer, lstrlen(buffer));

    tmpstc = g_pBoard->GetChannelStruct(1, 1, 0);
    PrintOctalValue(bufData, tmpstc.data);
    wsprintf(buffer, _T("CPU CH:1 RX D:%s RDY:%d IRQ:%d"), bufData + 3, tmpstc.ready, tmpstc.irq);
    TextOut(hdc, x, y + 7 * cyLine, buffer, lstrlen(buffer));

    tmpstc = g_pBoard->GetChannelStruct(1, 0, 1);
    wsprintf(buffer, _T("CPU CH:0 TX       RDY:%d IRQ:%d"), tmpstc.ready, tmpstc.irq);
    TextOut(hdc, x, y + 8 * cyLine, buffer, lstrlen(buffer));

    tmpstc = g_pBoard->GetChannelStruct(1, 1, 1);
    wsprintf(buffer, _T("CPU CH:1 TX       RDY:%d IRQ:%d"), tmpstc.ready, tmpstc.irq);
    TextOut(hdc, x, y + 9 * cyLine, buffer, lstrlen(buffer));

    tmpstc = g_pBoard->GetChannelStruct(1, 2, 1);
    wsprintf(buffer, _T("CPU CH:2 TX       RDY:%d IRQ:%d"), tmpstc.ready, tmpstc.irq);
    TextOut(hdc, x, y + 10 * cyLine, buffer, lstrlen(buffer));
}

BOOL DebugView_DrawBreakpoints(HDC hdc, int x, int y)
{
    const WORD* pbps = m_okDebugProcessor ? Emulator_GetCPUBreakpointList() : Emulator_GetPPUBreakpointList();
    if (*pbps == 0177777)
        return FALSE;

    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);

    TextOut(hdc, x, y, _T("Breakpts:"), 9);
    y += cyLine;
    while (*pbps != 0177777)
    {
        DrawOctalValue(hdc, x, y, *pbps);
        y += cyLine;
        pbps++;
    }
    return TRUE;
}

void DebugView_DrawCPUMemoryMap(HDC hdc, int x, int y, BOOL okHalt)
{
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);

    int x1 = x + cxChar * 7;
    int y1 = y + cxChar / 2;
    int x2 = x1 + cxChar * 14;
    int y2 = y1 + cyLine * 16;
    int xtype = x1 + cxChar * 3;
    int ybase = y + cyLine * 16;

    HGDIOBJ hOldBrush = ::SelectObject(hdc, ::GetSysColorBrush(COLOR_BTNSHADOW));
    PatBlt(hdc, x1, y1, 1, y2 - y1, PATCOPY);
    PatBlt(hdc, x2, y1, 1, y2 - y1 + 1, PATCOPY);
    PatBlt(hdc, x1, y1, x2 - x1, 1, PATCOPY);
    PatBlt(hdc, x1, y2, x2 - x1, 1, PATCOPY);

    TextOut(hdc, x, ybase - cyLine / 2, _T("000000"), 6);

    for (int i = 1; i < 8; i++)
    {
        if (i < 7)
            PatBlt(hdc, x1, y2 - cyLine * i * 2, 8, 1, PATCOPY);
        else
            PatBlt(hdc, x1, y2 - cyLine * i * 2, x2 - x1, 1, PATCOPY);
        WORD addr = (WORD)i * 020000;
        DrawOctalValue(hdc, x, y2 - cyLine * i * 2 - 4, addr);
    }
    ::SelectObject(hdc, hOldBrush);

    TextOut(hdc, xtype, ybase - cyLine * 7, _T("RAM12"), 5);

    if (okHalt)
        TextOut(hdc, xtype, ybase - cyLine * 15, _T("RAM12"), 5);
    else
        TextOut(hdc, xtype, ybase - cyLine * 15, _T("I/O"), 3);
}
void DebugView_DrawPPUMemoryMap(HDC hdc, int x, int y, const CMemoryController* pMemCtl)
{
    WORD value177054 = pMemCtl->GetPortView(0177054);

    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);

    int x1 = x + cxChar * 7;
    int y1 = y + cxChar / 2;
    int x2 = x1 + cxChar * 14;
    int y2 = y1 + cyLine * 16;
    int xtype = x1 + cxChar * 3;
    int ybase = y + cyLine * 16;

    HGDIOBJ hOldBrush = ::SelectObject(hdc, ::GetSysColorBrush(COLOR_BTNSHADOW));
    PatBlt(hdc, x1, y1, 1, y2 - y1, PATCOPY);
    PatBlt(hdc, x2, y1, 1, y2 - y1 + 1, PATCOPY);
    PatBlt(hdc, x1, y1, x2 - x1, 1, PATCOPY);
    PatBlt(hdc, x1, y2, x2 - x1, 1, PATCOPY);

    TextOut(hdc, x, ybase - cyLine / 2, _T("000000"), 6);

    for (int i = 1; i < 8; i++)
    {
        if (i < 4)
            PatBlt(hdc, x1, y2 - cyLine * i * 2, 8, 1, PATCOPY);
        else
            PatBlt(hdc, x1, y2 - cyLine * i * 2, x2 - x1, 1, PATCOPY);
        WORD addr = (WORD)i * 020000;
        DrawOctalValue(hdc, x, y2 - cyLine * i * 2 - 4, addr);
    }

    PatBlt(hdc, x1, y1 + cyLine / 4, x2 - x1, 1, PATCOPY);
    ::SelectObject(hdc, hOldBrush);

    TextOut(hdc, x, ybase - cyLine * 16 + cyLine / 4, _T("177000"), 6);
    TextOut(hdc, xtype, ybase - cyLine * 4, _T("RAM0"), 4);

    // 100000-117777 - Window block 0
    if ((value177054 & 16) != 0)  // Port 177054 bit 4 set => RAM selected
        TextOut(hdc, xtype, ybase - cyLine * 9, _T("RAM0"), 4);
    else if ((value177054 & 1) != 0)  // ROM selected
        TextOut(hdc, xtype, ybase - cyLine * 9, _T("ROM"), 3);
    else if ((value177054 & 14) != 0)  // ROM cartridge selected
    {
        int slot = ((value177054 & 8) == 0) ? 1 : 2;
        int bank = (value177054 & 6) >> 1;
        TCHAR buffer[10];
        wsprintf(buffer, _T("Cart %d/%d"), slot, bank);
        TextOut(hdc, xtype, ybase - cyLine * 9, buffer, _tcslen(buffer));
    }

    // 120000-137777 - Window block 1
    if ((value177054 & 32) != 0)  // Port 177054 bit 5 set => RAM selected
        TextOut(hdc, xtype, ybase - cyLine * 11, _T("RAM0"), 4);
    else
        TextOut(hdc, xtype, ybase - cyLine * 11, _T("ROM"), 3);

    // 140000-157777 - Window block 2
    if ((value177054 & 64) != 0)  // Port 177054 bit 6 set => RAM selected
        TextOut(hdc, xtype, ybase - cyLine * 13, _T("RAM0"), 4);
    else
        TextOut(hdc, xtype, ybase - cyLine * 13, _T("ROM"), 3);

    // 160000-176777 - Window block 3
    if ((value177054 & 128) != 0)  // Port 177054 bit 7 set => RAM selected
        TextOut(hdc, xtype, ybase - cyLine * 15, _T("RAM0"), 4);
    else
        TextOut(hdc, xtype, ybase - cyLine * 15, _T("ROM"), 3);
}


//////////////////////////////////////////////////////////////////////
