/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// DisasmView.cpp

#include "stdafx.h"
#include <commdlg.h>
#include <windowsx.h>
#include "Main.h"
#include "Views.h"
#include "ToolWindow.h"
#include "Dialogs.h"
#include "Emulator.h"
#include "emubase\Emubase.h"

//////////////////////////////////////////////////////////////////////


HWND g_hwndDisasm = (HWND) INVALID_HANDLE_VALUE;  // Disasm View window handle
WNDPROC m_wndprocDisasmToolWindow = NULL;  // Old window proc address of the ToolWindow

HWND m_hwndDisasmViewer = (HWND) INVALID_HANDLE_VALUE;

BOOL m_okDisasmProcessor = FALSE;  // TRUE - CPU, FALSE - PPU
WORD m_wDisasmBaseAddr = 0;

void DisasmView_DoDraw(HDC hdc);
int  DisasmView_DrawDisassemble(HDC hdc, CProcessor* pProc, WORD base, WORD previous, int x, int y);
void DisasmView_UpdateWindowText();
BOOL DisasmView_OnKeyDown(WPARAM vkey, LPARAM lParam);
void DisasmView_OnLButtonDown(WPARAM wParam, LPARAM lParam);
BOOL DisasmView_ParseSubtitles();

enum DisasmSubtitleType
{
    SUBTYPE_NONE = 0,
    SUBTYPE_COMMENT = 1,
    SUBTYPE_BLOCKCOMMENT = 2,
    SUBTYPE_DATA = 4,
};

struct DisasmSubtitleItem
{
    WORD address;
    DisasmSubtitleType type;
    LPCTSTR comment;
};

BOOL m_okDisasmSubtitles = FALSE;
TCHAR* m_strDisasmSubtitles = NULL;
DisasmSubtitleItem* m_pDisasmSubtitleItems = NULL;
int m_nDisasmSubtitleMax = 0;
int m_nDisasmSubtitleCount = 0;

enum DisasmLineType
{
    LINETYPE_NONE     = 0,  // Empty line
    LINETYPE_DATA     = 1,  // Line contains a data (non-instruction)
    LINETYPE_INSTR    = 2,  // Line contains a disassembled instruction
    LINETYPE_JUMP     = 4,  // Line has jump
    LINETYPE_SUBTITLE = 8,  // Line has subtitle comment
};

struct DisasmLineItem
{
    int   type;             // Combination of DisasmLineType values
    WORD  address;          // Line address for LINETYPE_DATA
    int   addrtype;         // Address type for LINETYPE_DATA, see ADDRTYPE_XXX constants
    WORD  value;            // Data on the address for LINETYPE_DATA
    TCHAR strInstr[8];      // Disassembled instruction for LINETYPE_DISASM
    TCHAR strArg[32];       // Disassembled instruction arguments for LINETYPE_DISASM
    int   jumpdelta;        // Jump delta for LINETYPE_JUMP
    DisasmSubtitleItem* pSubItem;  // Link to subtitles item for LINETYPE_SUBTITLE
};

const int MAX_DISASMLINECOUNT = 50;
DisasmLineItem* m_pDisasmLineItems = nullptr;

BOOL  m_okDisasmJumpPredict;
TCHAR m_strDisasmHint[42] = { 0 };
TCHAR m_strDisasmHint2[42] = { 0 };

int m_cxDisasmBreakpointZone = 16;  // Width of breakpoint zone at the left, for mouse click
int m_cyDisasmLine = 10;


//////////////////////////////////////////////////////////////////////


void DisasmView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = DisasmViewViewerWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInst;
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = CLASSNAME_DISASMVIEW;
    wcex.hIconSm        = NULL;

    RegisterClassEx(&wcex);
}

void DisasmView_Init()
{
    m_pDisasmLineItems = static_cast<DisasmLineItem*>(::calloc(MAX_DISASMLINECOUNT, sizeof(DisasmLineItem)));
}
void DisasmView_Done()
{
    if (m_strDisasmSubtitles != nullptr)
    {
        free(m_strDisasmSubtitles);  m_strDisasmSubtitles = nullptr;
    }
    if (m_pDisasmSubtitleItems != nullptr)
    {
        free(m_pDisasmSubtitleItems);
        m_pDisasmSubtitleItems = nullptr;
    }

    if (m_pDisasmLineItems != nullptr)
    {
        free(m_pDisasmLineItems);
        m_pDisasmLineItems = nullptr;
    }
}

void DisasmView_Create(HWND hwndParent, int x, int y, int width, int height)
{
    ASSERT(hwndParent != NULL);

    m_okDisasmProcessor = Settings_GetDebugCpuPpu();

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

void DisasmView_Redraw()
{
    RedrawWindow(g_hwndDisasm, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

// Adjust position of client windows
void DisasmView_AdjustWindowLayout()
{
    RECT rc;  GetClientRect(g_hwndDisasm, &rc);

    if (m_hwndDisasmViewer != (HWND) INVALID_HANDLE_VALUE)
        SetWindowPos(m_hwndDisasmViewer, NULL, 0, 0, rc.right, rc.bottom, SWP_NOZORDER);
}

LRESULT CALLBACK DisasmViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    LRESULT lResult;
    switch (message)
    {
    case WM_DESTROY:
        g_hwndDisasm = (HWND) INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
        return CallWindowProc(m_wndprocDisasmToolWindow, hWnd, message, wParam, lParam);
    case WM_SIZE:
        lResult = CallWindowProc(m_wndprocDisasmToolWindow, hWnd, message, wParam, lParam);
        DisasmView_AdjustWindowLayout();
        return lResult;
    default:
        return CallWindowProc(m_wndprocDisasmToolWindow, hWnd, message, wParam, lParam);
    }
    //return (LRESULT)FALSE;
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

            DisasmView_DoDraw(hdc);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_LBUTTONDOWN:
        DisasmView_OnLButtonDown(wParam, lParam);
        break;
    case WM_KEYDOWN:
        return (LRESULT) DisasmView_OnKeyDown(wParam, lParam);
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        ::InvalidateRect(hWnd, NULL, TRUE);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

BOOL DisasmView_OnKeyDown(WPARAM vkey, LPARAM /*lParam*/)
{
    switch (vkey)
    {
    case VK_SPACE:
        DisasmView_SetCurrentProc(!m_okDisasmProcessor);
        DebugView_SetCurrentProc(m_okDisasmProcessor);   // Switch DebugView to current processor
        break;
    case 0x53:  // S - Load/Unload Subtitles
        DisasmView_LoadUnloadSubtitles();
        break;
    case VK_ESCAPE:
        ConsoleView_Activate();
        break;
    default:
        return TRUE;
    }
    return FALSE;
}

void DisasmView_OnLButtonDown(WPARAM /*wParam*/, LPARAM lParam)
{
    ::SetFocus(m_hwndDisasmViewer);

    // For click in the breakpoint zone at the left - try to find the line/address and add/remove breakpoint
    if (GET_X_LPARAM(lParam) < m_cxDisasmBreakpointZone)
    {
        int lineindex = (GET_Y_LPARAM(lParam) - 2) / m_cyDisasmLine;
        if (lineindex >= 0 && lineindex < MAX_DISASMLINECOUNT)
        {
            DisasmLineItem* pLineItem = m_pDisasmLineItems + lineindex;
            if (pLineItem->type != LINETYPE_NONE)
            {
                WORD address = pLineItem->address;
                if (!Emulator_IsBreakpoint(m_okDisasmProcessor, address))
                {
                    bool result = m_okDisasmProcessor ? Emulator_AddCPUBreakpoint(address) : Emulator_AddPPUBreakpoint(address);
                    if (!result)
                        AlertWarningFormat(_T("Failed to add breakpoint at %06ho."), address);
                }
                else
                {
                    bool result = m_okDisasmProcessor ? Emulator_RemoveCPUBreakpoint(address) : Emulator_RemovePPUBreakpoint(address);
                    if (!result)
                        AlertWarningFormat(_T("Failed to remove breakpoint at %06ho."), address);
                }
                DebugView_Redraw();
                DisasmView_Redraw();
            }
        }
    }
}

void DisasmView_UpdateWindowText()
{
    CProcessor* pDisasmPU = (m_okDisasmProcessor) ? g_pBoard->GetCPU() : g_pBoard->GetPPU();
    ASSERT(pDisasmPU != NULL);
    LPCTSTR sProcName = pDisasmPU->GetName();

    TCHAR buffer[64];
    if (m_okDisasmSubtitles)
        _stprintf_s(buffer, 64, _T("Disassemble - %s - Subtitles"), sProcName);
    else
        _stprintf_s(buffer, 64, _T("Disassemble - %s"), sProcName);
    ::SetWindowText(g_hwndDisasm, buffer);
}

void DisasmView_ResizeSubtitleArray(int newSize)
{
    DisasmSubtitleItem* pNewMemory = (DisasmSubtitleItem*) ::calloc(newSize, sizeof(DisasmSubtitleItem));
    if (m_pDisasmSubtitleItems != NULL)
    {
        ::memcpy(pNewMemory, m_pDisasmSubtitleItems, sizeof(DisasmSubtitleItem) * m_nDisasmSubtitleMax);
        ::free(m_pDisasmSubtitleItems);
    }

    m_pDisasmSubtitleItems = pNewMemory;
    m_nDisasmSubtitleMax = newSize;
}
void DisasmView_AddSubtitle(WORD address, int type, LPCTSTR pCommentText)
{
    if (m_nDisasmSubtitleCount >= m_nDisasmSubtitleMax)
    {
        // Расширить массив
        int newsize = m_nDisasmSubtitleMax + m_nDisasmSubtitleMax / 2;
        DisasmView_ResizeSubtitleArray(newsize);
    }

    m_pDisasmSubtitleItems[m_nDisasmSubtitleCount].address = address;
    m_pDisasmSubtitleItems[m_nDisasmSubtitleCount].type = (DisasmSubtitleType) type;
    m_pDisasmSubtitleItems[m_nDisasmSubtitleCount].comment = pCommentText;
    m_nDisasmSubtitleCount++;
}

void DisasmView_LoadUnloadSubtitles()
{
    if (m_okDisasmSubtitles)  // Reset subtitles
    {
        ::free(m_strDisasmSubtitles);  m_strDisasmSubtitles = NULL;
        ::free(m_pDisasmSubtitleItems);  m_pDisasmSubtitleItems = NULL;
        m_nDisasmSubtitleMax = m_nDisasmSubtitleCount = 0;
        m_okDisasmSubtitles = FALSE;
        DisasmView_UpdateWindowText();
        DisasmView_OnUpdate();  // We have to re-build the list of lines to show
        return;
    }

    // File Open dialog
    TCHAR bufFileName[MAX_PATH];
    BOOL okResult = ShowOpenDialog(g_hwnd,
            _T("Open Disassemble Subtitles"),
            _T("Subtitles (*.lst)\0*.lst\0All Files (*.*)\0*.*\0\0"),
            bufFileName);
    if (! okResult) return;

    // Load subtitles text from the file
    HANDLE hSubFile = CreateFile(bufFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSubFile == INVALID_HANDLE_VALUE)
    {
        AlertWarning(_T("Failed to load subtitles file."));
        return;
    }
    DWORD dwSubFileSize = ::GetFileSize(hSubFile, NULL);
    if (dwSubFileSize > 1024 * 1024)
    {
        ::CloseHandle(hSubFile);
        AlertWarning(_T("Subtitles file is too big (over 1 MB)."));
        return;
    }

    m_strDisasmSubtitles = (TCHAR*) ::calloc(dwSubFileSize + 2, 1);
    DWORD dwBytesRead;
    ::ReadFile(hSubFile, m_strDisasmSubtitles, dwSubFileSize, &dwBytesRead, NULL);
    ASSERT(dwBytesRead == dwSubFileSize);
    ::CloseHandle(hSubFile);

    // Estimate comment count and allocate memory
    int estimateSubtitleCount = dwSubFileSize / (75 * sizeof(TCHAR));
    if (estimateSubtitleCount < 256)
        estimateSubtitleCount = 256;
    DisasmView_ResizeSubtitleArray(estimateSubtitleCount);

    // Parse subtitles
    if (!DisasmView_ParseSubtitles())
    {
        ::free(m_strDisasmSubtitles);  m_strDisasmSubtitles = NULL;
        ::free(m_pDisasmSubtitleItems);  m_pDisasmSubtitleItems = NULL;
        AlertWarning(_T("Failed to parse subtitles file."));
        return;
    }

    m_okDisasmSubtitles = TRUE;
    DisasmView_UpdateWindowText();
    DisasmView_OnUpdate();  // We have to re-build the list of lines to show
}

// Разбор текста "субтитров".
// На входе -- текст в m_strDisasmSubtitles в формате UTF16 LE, заканчивается символом с кодом 0.
// На выходе -- массив описаний [адрес в памяти, тип, адрес строки комментария] в m_pDisasmSubtitleItems.
BOOL DisasmView_ParseSubtitles()
{
    ASSERT(m_strDisasmSubtitles != NULL);
    TCHAR* pText = m_strDisasmSubtitles;
    if (*pText == 0 || *pText == 0xFFFE)  // EOF or Unicode Big Endian
        return FALSE;
    if (*pText == 0xFEFF)
        pText++;  // Skip Unicode LE mark

    m_nDisasmSubtitleCount = 0;
    TCHAR* pBlockCommentStart = NULL;

    for (;;)  // Text reading loop - line by line
    {
        // Line starts
        if (*pText == 0) break;
        if (*pText == _T('\n') || *pText == _T('\r'))
        {
            pText++;
            continue;
        }

        if (*pText >= _T('0') && *pText <= _T('9'))  // Цифра -- считаем что это адрес
        {
            // Парсим адрес
            TCHAR* pAddrStart = pText;
            while (*pText != 0 && *pText >= _T('0') && *pText <= _T('9')) pText++;
            if (*pText == 0) break;
            TCHAR chSave = *pText;
            *pText++ = 0;
            WORD address;
            ParseOctalValue(pAddrStart, &address);
            *pText = chSave;

            if (pBlockCommentStart != NULL)  // На предыдущей строке был комментарий к блоку
            {
                // Сохраняем комментарий к блоку в массиве
                DisasmView_AddSubtitle(address, SUBTYPE_BLOCKCOMMENT, pBlockCommentStart);
                pBlockCommentStart = NULL;
            }

            // Пропускаем разделители
            while (*pText != 0 &&
                   (*pText == _T(' ') || *pText == _T('\t') || *pText == _T('$') || *pText == _T(':')))
                pText++;
            BOOL okDirective = (*pText == _T('.'));

            // Ищем начало комментария и конец строки
            while (*pText != 0 && *pText != _T(';') && *pText != _T('\n') && *pText != _T('\r')) pText++;
            if (*pText == 0) break;
            if (*pText == _T('\n') || *pText == _T('\r'))  // EOL, комментарий не обнаружен
            {
                pText++;

                if (okDirective)
                    DisasmView_AddSubtitle(address, SUBTYPE_DATA, NULL);
                continue;
            }

            // Нашли начало комментария -- ищем конец строки или файла
            TCHAR* pCommentStart = pText;
            while (*pText != 0 && *pText != _T('\n') && *pText != _T('\r')) pText++;

            // Сохраняем комментарий в массиве
            DisasmView_AddSubtitle(address,
                    (okDirective ? SUBTYPE_COMMENT | SUBTYPE_DATA : SUBTYPE_COMMENT),
                    pCommentStart);

            if (*pText == 0) break;
            *pText = 0;  // Обозначаем конец комментария
            pText++;
        }
        else  // Не цифра -- пропускаем до конца строки
        {
            if (*pText == _T(';'))  // Строка начинается с комментария - предположительно, комментарий к блоку
                pBlockCommentStart = pText;
            else
                pBlockCommentStart = NULL;

            while (*pText != 0 && *pText != _T('\n') && *pText != _T('\r')) pText++;
            if (*pText == 0) break;
            if (*pText == _T('\n') || *pText == _T('\r'))  // EOL
            {
                *pText = 0;  // Обозначаем конец комментария - для комментария к блоку
                pText++;
            }
        }
    }

    return TRUE;
}

DisasmSubtitleItem* DisasmView_FindSubtitle(WORD address, int typemask)
{
    DisasmSubtitleItem* pItem = m_pDisasmSubtitleItems;
    while (pItem->type != 0)
    {
        if (pItem->address == address && (pItem->type & typemask) != 0)
            return pItem;
        pItem++;
    }

    return nullptr;
}


//////////////////////////////////////////////////////////////////////


void DisasmView_SetCurrentProc(BOOL okCPU)
{
    m_okDisasmProcessor = okCPU;
    CProcessor* pDisasmPU = (m_okDisasmProcessor) ? g_pBoard->GetCPU() : g_pBoard->GetPPU();
    ASSERT(pDisasmPU != NULL);
    m_wDisasmBaseAddr = pDisasmPU->GetPC();
    InvalidateRect(m_hwndDisasmViewer, NULL, TRUE);
    DisasmView_UpdateWindowText();
    DisasmView_OnUpdate();  // We have to re-build the list of lines to show
}

BOOL DisasmView_CheckForJump(const WORD* memory, int* pDelta)
{
    WORD instr = *memory;

    // BR, BNE, BEQ, BGE, BLT, BGT, BLE
    // BPL, BMI, BHI, BLOS, BVC, BVS, BHIS, BLO
    if ((instr & 0177400) >= 0000400 && (instr & 0177400) < 0004000 ||
        (instr & 0177400) >= 0100000 && (instr & 0177400) < 0104000)
    {
        *pDelta = ((int)(char)(instr & 0xff)) + 1;
        return TRUE;
    }

    // SOB
    if ((instr & ~(WORD)0777) == PI_SOB)
    {
        *pDelta = -(GetDigit(instr, 1) * 8 + GetDigit(instr, 0)) + 1;
        return TRUE;
    }

    // CALL, JMP
    if (instr == 0004767 || instr == 0000167)
    {
        *pDelta = ((short)(memory[1]) + 4) / 2;
        return TRUE;
    }

    return FALSE;
}

// Prepare "Jump Hint" string, and also calculate condition for conditional jump
// Returns: jump prediction flag: TRUE = will jump, FALSE = will not jump
BOOL DisasmView_GetJumpConditionHint(const WORD* memory, const CProcessor * pProc, const CMemoryController * pMemCtl, LPTSTR buffer)
{
    *buffer = 0;
    WORD instr = *memory;
    WORD psw = pProc->GetPSW();

    if (instr >= 0001000 && instr <= 0001777)  // BNE, BEQ
    {
        _sntprintf(buffer, 32, _T("Z=%c"), (psw & PSW_Z) ? '1' : '0');
        // BNE: IF (Z == 0)
        // BEQ: IF (Z == 1)
        BOOL value = ((psw & PSW_Z) != 0);
        return ((instr & 0400) == 0) ? !value : value;
    }
    if (instr >= 0002000 && instr <= 0002777)  // BGE, BLT
    {
        _sntprintf(buffer, 32, _T("N=%c, V=%c"), (psw & PSW_N) ? '1' : '0', (psw & PSW_V) ? '1' : '0');
        // BGE: IF ((N xor V) == 0)
        // BLT: IF ((N xor V) == 1)
        BOOL value = (((psw & PSW_N) != 0) != ((psw & PSW_V) != 0));
        return ((instr & 0400) == 0) ? !value : value;
    }
    if (instr >= 0003000 && instr <= 0003777)  // BGT, BLE
    {
        _sntprintf(buffer, 32, _T("N=%c, V=%c, Z=%c"), (psw & PSW_N) ? '1' : '0', (psw & PSW_V) ? '1' : '0', (psw & PSW_Z) ? '1' : '0');
        // BGT: IF (((N xor V) or Z) == 0)
        // BLE: IF (((N xor V) or Z) == 1)
        BOOL value = ((((psw & PSW_N) != 0) != ((psw & PSW_V) != 0)) || ((psw & PSW_Z) != 0));
        return ((instr & 0400) == 0) ? !value : value;
    }
    if (instr >= 0100000 && instr <= 0100777)  // BPL, BMI
    {
        _sntprintf(buffer, 32, _T("N=%c"), (psw & PSW_N) ? '1' : '0');
        // BPL: IF (N == 0)
        // BMI: IF (N == 1)
        BOOL value = ((psw & PSW_N) != 0);
        return ((instr & 0400) == 0) ? !value : value;
    }
    if (instr >= 0101000 && instr <= 0101777)  // BHI, BLOS
    {
        _sntprintf(buffer, 32, _T("C=%c, Z=%c"), (psw & PSW_C) ? '1' : '0', (psw & PSW_Z) ? '1' : '0');
        // BHI:  IF ((С or Z) == 0)
        // BLOS: IF ((С or Z) == 1)
        BOOL value = (((psw & PSW_C) != 0) || ((psw & PSW_Z) != 0));
        return ((instr & 0400) == 0) ? !value : value;
    }
    if (instr >= 0102000 && instr <= 0102777)  // BVC, BVS
    {
        _sntprintf(buffer, 32, _T("V=%c"), (psw & PSW_V) ? '1' : '0');
        // BVC: IF (V == 0)
        // BVS: IF (V == 1)
        BOOL value = ((psw & PSW_V) != 0);
        return ((instr & 0400) == 0) ? !value : value;
    }
    if (instr >= 0103000 && instr <= 0103777)  // BCC/BHIS, BCS/BLO
    {
        _sntprintf(buffer, 32, _T("C=%c"), (psw & PSW_C) ? '1' : '0');
        // BCC/BHIS: IF (C == 0)
        // BCS/BLO:  IF (C == 1)
        BOOL value = ((psw & PSW_C) != 0);
        return ((instr & 0400) == 0) ? !value : value;
    }
    if (instr >= 0077000 && instr <= 0077777)  // SOB
    {
        int reg = (instr >> 6) & 7;
        WORD regvalue = pProc->GetReg(reg);
        _sntprintf(buffer, 32, _T("R%d=%06o"), reg, regvalue);  // "RN=XXXXXX"
        return (regvalue != 1);
    }

    if (instr >= 004000 && instr <= 004677)  // JSR (except CALL)
    {
        int reg = (instr >> 6) & 7;
        WORD regvalue = pProc->GetReg(reg);
        _sntprintf(buffer, 32, _T("R%d=%06o"), reg, regvalue);  // "RN=XXXXXX"
        return TRUE;
    }
    if (instr >= 000200 && instr <= 000207)  // RTS / RETURN
    {
        WORD spvalue = pProc->GetSP();
        int addrtype;
        WORD value = pMemCtl->GetWordView(spvalue, pProc->IsHaltMode(), FALSE, &addrtype);
        if (instr == 000207)  // RETURN
            _sntprintf(buffer, 32, _T("(SP)=%06o"), value);  // "(SP)=XXXXXX"
        else  // RTS
        {
            int reg = instr & 7;
            WORD regvalue = pProc->GetReg(reg);
            _sntprintf(buffer, 32, _T("R%d=%06o, (SP)=%06o"), reg, regvalue, value);  // "RN=XXXXXX, (SP)=XXXXXX"
        }
        return TRUE;
    }

    if (instr == 000002 || instr == 000006)  // RTI, RTT
    {
        WORD spvalue = pProc->GetSP();
        int addrtype;
        WORD value = pMemCtl->GetWordView(spvalue, pProc->IsHaltMode(), FALSE, &addrtype);
        _sntprintf(buffer, 32, _T("(SP)=%06o"), value);  // "(SP)=XXXXXX"
        return TRUE;
    }
    if (instr == 000003 || instr == 000004 ||  // IOT, BPT
        (instr >= 0104000 && instr <= 0104777))  // TRAP, EMT
    {
        WORD intvec;
        if (instr == 000003) intvec = 000014;
        else if (instr == 000004) intvec = 000020;
        else if (instr < 0104400) intvec = 000030;
        else intvec = 000034;

        int addrtype;
        WORD value = pMemCtl->GetWordView(intvec, pProc->IsHaltMode(), FALSE, &addrtype);
        _sntprintf(buffer, 32, _T("(%06o)=%06o"), intvec, value);  // "(VVVVVV)=XXXXXX"
        return TRUE;
    }

    return TRUE;  // All other jumps are non-conditional
}

void DisasmView_RegisterHint(const CProcessor * pProc, const CMemoryController * pMemCtl,
        LPTSTR hint1, LPTSTR hint2,
        int regnum, int regmod, bool byteword, WORD indexval)
{
    int addrtype = 0;
    WORD regval = pProc->GetReg(regnum);
    WORD srcval2 = 0;

    _sntprintf(hint1, 20, _T("%s=%06o"), REGISTER_NAME[regnum], regval);  // "RN=XXXXXX"
    switch (regmod)
    {
    case 1:
    case 2:
        srcval2 = pMemCtl->GetWordView(regval, pProc->IsHaltMode(), false, &addrtype);
        if (byteword)
        {
            srcval2 = (regval & 1) ? (srcval2 >> 8) : (srcval2 & 0xff);
            _sntprintf(hint2, 20, _T("(%s)=%03o"), REGISTER_NAME[regnum], srcval2);  // "(RN)=XXX"
        }
        else
        {
            _sntprintf(hint2, 20, _T("(%s)=%06o"), REGISTER_NAME[regnum], srcval2);  // "(RN)=XXXXXX"
        }
        break;
    case 3:
        srcval2 = pMemCtl->GetWordView(regval, pProc->IsHaltMode(), false, &addrtype);
        _sntprintf(hint2, 20, _T("(%s)=%06o"), REGISTER_NAME[regnum], srcval2);  // "(RN)=XXXXXX"
        //TODO: Show the real value in hint line 3
        break;
    case 4:
        if (byteword)
        {
            srcval2 = (regval & 1) ?
                    ((pMemCtl->GetWordView(regval - 1, pProc->IsHaltMode(), false, &addrtype)) & 0xff) :
                    ((pMemCtl->GetWordView(regval - 2, pProc->IsHaltMode(), false, &addrtype)) >> 8);
            _sntprintf(hint2, 20, _T("(%s-1)=%03o"), REGISTER_NAME[regnum], srcval2);  // "(RN-1)=XXX"
        }
        else
        {
            srcval2 = pMemCtl->GetWordView(regval - 2, pProc->IsHaltMode(), false, &addrtype);
            _sntprintf(hint2, 20, _T("(%s-2)=%06o"), REGISTER_NAME[regnum], srcval2);  // "(RN-2)=XXXXXX"
        }
        break;
    case 5:
        srcval2 = pMemCtl->GetWordView(regval - 2, pProc->IsHaltMode(), false, &addrtype);
        _sntprintf(hint2, 20, _T("(%s-2)=%06o"), REGISTER_NAME[regnum], srcval2);  // "(RN+2)=XXXXXX"
        //TODO: Show the real value in hint line 3
        break;
    case 6:
        {
            WORD addr2 = regval + indexval;
            srcval2 = pMemCtl->GetWordView(addr2 & ~1, pProc->IsHaltMode(), false, &addrtype);
            if (byteword)
            {
                srcval2 = (addr2 & 1) ? (srcval2 >> 8) : (srcval2 & 0xff);
                _sntprintf(hint2, 20, _T("(%s+%06o)=%03o"), REGISTER_NAME[regnum], indexval, srcval2);  // "(RN+NNNNNN)=XXX"
            }
            else
            {
                _sntprintf(hint2, 20, _T("(%s+%06o)=%06o"), REGISTER_NAME[regnum], indexval, srcval2);  // "(RN+NNNNNN)=XXXXXX"
            }
            break;
        }
    case 7:
        srcval2 = pMemCtl->GetWordView(regval + indexval, pProc->IsHaltMode(), false, &addrtype);
        _sntprintf(hint2, 20, _T("(%s+%06o)=%06o"), REGISTER_NAME[regnum], indexval, srcval2);  // "(RN+NNNNNN)=XXXXXX"
        //TODO: Show the real value in hint line 3
        break;
    }
}

void DisasmView_RegisterHintPC(const CProcessor * pProc, const CMemoryController * pMemCtl,
        LPTSTR hint1, LPTSTR /*hint2*/,
        int regmod, bool byteword, WORD curaddr, WORD value)
{
    int addrtype = 0;
    WORD srcval2 = 0;

    //TODO: else if (regmod == 2)
    if (regmod == 3)
    {
        srcval2 = pMemCtl->GetWordView(value, pProc->IsHaltMode(), false, &addrtype);
        if (byteword)
        {
            srcval2 = (value & 1) ? (srcval2 >> 8) : (srcval2 & 0xff);
            _sntprintf(hint1, 20, _T("(%06o)=%03o"), value, srcval2);  // "(NNNNNN)=XXX"
        }
        else
        {
            _sntprintf(hint1, 20, _T("(%06o)=%06o"), value, srcval2);  // "(NNNNNN)=XXXXXX"
        }
    }
    else if (regmod == 6)
    {
        WORD addr2 = curaddr + value;
        srcval2 = pMemCtl->GetWordView(addr2, pProc->IsHaltMode(), false, &addrtype);
        if (byteword)
        {
            srcval2 = (addr2 & 1) ? (srcval2 >> 8) : (srcval2 & 0xff);
            _sntprintf(hint1, 20, _T("(%06o)=%03o"), addr2, srcval2);  // "(NNNNNN)=XXX"
        }
        else
        {
            _sntprintf(hint1, 20, _T("(%06o)=%06o"), addr2, srcval2);  // "(NNNNNN)=XXXXXX"
        }
    }
    //TODO: else if (regmod == 7)
}

void DisasmView_InstructionHint(const WORD* memory, const CProcessor * pProc, const CMemoryController * pMemCtl,
        LPTSTR buffer, LPTSTR buffer2,
        int srcreg, int srcmod, int dstreg, int dstmod)
{
    TCHAR srchint1[20] = { 0 }, dsthint1[20] = { 0 };
    TCHAR srchint2[20] = { 0 }, dsthint2[20] = { 0 };
    bool byteword = ((*memory) & 0100000) != 0;  // Byte mode (true) or Word mode (false)
    const WORD* curmemory = memory + 1;
    WORD curaddr = pProc->GetPC() + 2;
    WORD indexval = 0;

    if (srcreg >= 0)
    {
        if (srcreg == 7)
        {
            WORD value = *(curmemory++);  curaddr += 2;
            DisasmView_RegisterHintPC(pProc, pMemCtl, srchint1, srchint2, srcmod, byteword, curaddr, value);
        }
        else
        {
            if (srcmod == 6 || srcmod == 7) { indexval = *(curmemory++);  curaddr += 2; }
            DisasmView_RegisterHint(pProc, pMemCtl, srchint1, srchint2, srcreg, srcmod, byteword, indexval);
        }
    }
    if (dstreg >= 0)
    {
        if (dstreg == 7)
        {
            WORD value = *(curmemory++);  curaddr += 2;
            DisasmView_RegisterHintPC(pProc, pMemCtl, dsthint1, dsthint2, dstmod, byteword, curaddr, value);
        }
        else
        {
            if (dstmod == 6 || dstmod == 7) { indexval = *(curmemory++);  curaddr += 2; }
            DisasmView_RegisterHint(pProc, pMemCtl, dsthint1, dsthint2, dstreg, dstmod, byteword, indexval);
        }
    }

    if (*srchint1 != 0 && *dsthint1 != 0)
    {
        if (_tcscmp(srchint1, dsthint1) == 0)
            _tcscpy_s(buffer, 42, srchint1);
        else
            _sntprintf(buffer, 42, _T("%s, %s"), srchint1, dsthint1);
    }
    else if (*srchint1 != 0)
        _tcscpy_s(buffer, 42, srchint1);
    else if (*dsthint1 != 0)
        _tcscpy_s(buffer, 42, dsthint1);

    if (*srchint2 != 0 && *dsthint2 != 0)
    {
        if (_tcscmp(srchint2, dsthint2) == 0)
            _tcscpy_s(buffer2, 42, srchint2);
        else
            _sntprintf(buffer2, 42, _T("%s, %s"), srchint2, dsthint2);
    }
    else if (*srchint2 != 0)
        _tcscpy_s(buffer2, 42, srchint2);
    else if (*dsthint2 != 0)
        _tcscpy_s(buffer2, 42, dsthint2);
}

// Prepare "Instruction Hint" for a regular instruction (not a branch/jump one)
// Returns: number of hint lines; 0 = no hints
int DisasmView_GetInstructionHint(const WORD* memory, const CProcessor * pProc, const CMemoryController * pMemCtl,
        LPTSTR buffer, LPTSTR buffer2)
{
    *buffer = 0;  *buffer2 = 0;
    WORD instr = *memory;

    // Source and Destination
    if ((instr & ~(uint16_t)0107777) == PI_MOV || (instr & ~(uint16_t)0107777) == PI_CMP ||
        (instr & ~(uint16_t)0107777) == PI_BIT || (instr & ~(uint16_t)0107777) == PI_BIC || (instr & ~(uint16_t)0107777) == PI_BIS ||
        (instr & ~(uint16_t)0007777) == PI_ADD || (instr & ~(uint16_t)0007777) == PI_SUB)
    {
        int srcreg = (instr >> 6) & 7;
        int srcmod = (instr >> 9) & 7;
        int dstreg = instr & 7;
        int dstmod = (instr >> 3) & 7;
        DisasmView_InstructionHint(memory, pProc, pMemCtl, buffer, buffer2, srcreg, srcmod, dstreg, dstmod);
    }

    // Register and Destination
    if ((instr & ~(uint16_t)0777) == PI_MUL || (instr & ~(uint16_t)0777) == PI_DIV ||
        (instr & ~(uint16_t)0777) == PI_ASH || (instr & ~(uint16_t)0777) == PI_ASHC ||
        (instr & ~(uint16_t)0777) == PI_XOR)
    {
        int srcreg = (instr >> 6) & 7;
        int dstreg = instr & 7;
        int dstmod = (instr >> 3) & 7;
        DisasmView_InstructionHint(memory, pProc, pMemCtl, buffer, buffer2, srcreg, 0, dstreg, dstmod);
    }

    // Destination only
    if ((instr & ~(uint16_t)0100077) == PI_CLR || (instr & ~(uint16_t)0100077) == PI_COM ||
        (instr & ~(uint16_t)0100077) == PI_INC || (instr & ~(uint16_t)0100077) == PI_DEC || (instr & ~(uint16_t)0100077) == PI_NEG ||
        (instr & ~(uint16_t)0100077) == PI_TST ||
        (instr & ~(uint16_t)0100077) == PI_ASR || (instr & ~(uint16_t)0100077) == PI_ASL ||
        (instr & ~(uint16_t)077) == PI_SWAB || (instr & ~(uint16_t)077) == PI_SXT ||
        (instr & ~(uint16_t)077) == PI_MTPS || (instr & ~(uint16_t)077) == PI_MFPS)
    {
        int dstreg = instr & 7;
        int dstmod = (instr >> 3) & 7;
        DisasmView_InstructionHint(memory, pProc, pMemCtl, buffer, buffer2, -1, -1, dstreg, dstmod);
    }

    // ADC, SBC, ROR, ROL: destination only, and also show C flag
    if ((instr & ~(uint16_t)0100077) == PI_ADC || (instr & ~(uint16_t)0100077) == PI_SBC ||
        (instr & ~(uint16_t)0100077) == PI_ROR || (instr & ~(uint16_t)0100077) == PI_ROL)
    {
        int dstreg = instr & 7;
        int dstmod = (instr >> 3) & 7;
        if (dstreg != 7)
        {
            TCHAR tempbuf[42];
            DisasmView_InstructionHint(memory, pProc, pMemCtl, tempbuf, buffer2, -1, -1, dstreg, dstmod);
            WORD psw = pProc->GetPSW();
            _sntprintf(buffer, 42, _T("%s, C=%c"), tempbuf, (psw & PSW_C) ? '1' : '0');  // "..., C=X"
        }
    }

    // CLC..CCC, SEC..SCC -- show flags
    if (instr >= 0000241 && instr <= 0000257 || instr >= 0000261 && instr <= 0000277)
    {
        WORD psw = pProc->GetPSW();
        _sntprintf(buffer, 32, _T("C=%c, V=%c, Z=%c, N=%c"),
                (psw & PSW_C) ? '1' : '0', (psw & PSW_V) ? '1' : '0', (psw & PSW_Z) ? '1' : '0', (psw & PSW_N) ? '1' : '0');
    }

    // JSR, JMP -- show non-trivial cases only
    if ((instr & ~(uint16_t)0777) == PI_JSR && (instr & 077) != 067 && (instr & 077) != 037 ||
        (instr & ~(uint16_t)077) == PI_JMP && (instr & 077) != 067 && (instr & 077) != 037)
    {
        int dstreg = instr & 7;
        int dstmod = (instr >> 3) & 7;
        DisasmView_InstructionHint(memory, pProc, pMemCtl, buffer, buffer2, -1, -1, dstreg, dstmod);
    }

    // HALT mode commands
    if (instr == PI_MFUS)
    {
        _sntprintf(buffer, 32, _T("R5=%06o, R0=%06o"), pProc->GetReg(5), pProc->GetReg(0));  // "R5=XXXXXX, R0=XXXXXX"
    }
    if (instr == PI_MTUS)
    {
        _sntprintf(buffer, 32, _T("R0=%06o, R5=%06o"), pProc->GetReg(0), pProc->GetReg(5));  // "R0=XXXXXX, R5=XXXXXX"
    }
    //TODO: MFPC, MTPC

    //TODO: MARK

    int result = 0;
    if (*buffer != 0)
        result = 1;
    if (*buffer2 != 0)
        result = 2;
    return result;
}

// Update after Run or Step
void DisasmView_OnUpdate()
{
    CProcessor* pProc = (m_okDisasmProcessor) ? g_pBoard->GetCPU() : g_pBoard->GetPPU();
    ASSERT(pProc != nullptr);
    m_wDisasmBaseAddr = pProc->GetPC();

    ASSERT(m_pDisasmLineItems != nullptr);
    memset(m_pDisasmLineItems, 0, sizeof(DisasmLineItem) * MAX_DISASMLINECOUNT);
    m_strDisasmHint[0] = 0;
    m_strDisasmHint2[0] = 0;

    const CMemoryController* pMemCtl = pProc->GetMemoryController();
    WORD proccurrent = pProc->GetPC();
    WORD current = m_wDisasmBaseAddr;
    WORD previous = (m_okDisasmProcessor) ? g_wEmulatorPrevCpuPC : g_wEmulatorPrevPpuPC;

    // Read from the processor memory to the buffer
    const int nWindowSize = 30;
    WORD memory[nWindowSize + 2];
    int addrtype[nWindowSize + 2];
    for (int idx = 0; idx < nWindowSize; idx++)
    {
        memory[idx] = pMemCtl->GetWordView(
                (WORD)(current + idx * 2 - 10), pProc->IsHaltMode(), TRUE, addrtype + idx);
    }

    WORD address = current - 10;
    WORD disasmfrom = current;
    if (previous >= address && previous < current)
        disasmfrom = previous;

    // Prepare the list of lines in m_pDisasmLineItems
    int lineindex = 0;
    int length = 0;
    for (int index = 0; index < nWindowSize; index++)  // Preparing lines
    {
        DisasmLineItem* pLineItem = m_pDisasmLineItems + lineindex;
        pLineItem->address = address;
        pLineItem->value = memory[index];
        pLineItem->addrtype = addrtype[index];

        bool okData = false;
        if (m_okDisasmSubtitles)
        {
            // Subtitles - find a comment for a block
            DisasmSubtitleItem* pSubItem = DisasmView_FindSubtitle(address, SUBTYPE_BLOCKCOMMENT);
            if (pSubItem != nullptr && pSubItem->comment != nullptr)
            {
                pLineItem->type = LINETYPE_SUBTITLE;
                pLineItem->pSubItem = pSubItem;
                // Opening next line
                lineindex++;
                if (lineindex >= MAX_DISASMLINECOUNT)
                    break;
                pLineItem = m_pDisasmLineItems + lineindex;
                pLineItem->address = address;
                pLineItem->value = memory[index];
                pLineItem->addrtype = addrtype[index];
            }

            // Subtitles - find a comment for an instruction or data
            pSubItem = DisasmView_FindSubtitle(address, SUBTYPE_COMMENT | SUBTYPE_DATA);
            if (pSubItem != nullptr && (pSubItem->type & SUBTYPE_DATA) != 0)
            {
                okData = true;
                pLineItem->type |= LINETYPE_DATA;
            }
            if (pSubItem != nullptr && (pSubItem->type & SUBTYPE_COMMENT) != 0 && pSubItem->comment != nullptr)
            {
                pLineItem->type |= LINETYPE_SUBTITLE;
                pLineItem->pSubItem = pSubItem;
                // Строку с субтитром мы можем использовать как опорную для дизассемблера
                if (disasmfrom > address)
                    disasmfrom = address;
            }
        }

        if ((pLineItem->type & LINETYPE_DATA) == 0)
            pLineItem->type |= LINETYPE_INSTR;  // if it's not a data then an instruction

        if (address >= disasmfrom && length == 0)
        {
            if (okData)  // We have non-instruction on the address -- no need to disassemble
            {
                length = 1;
            }
            else
            {
                pLineItem->type |= LINETYPE_INSTR;
                length = DisassembleInstruction(memory + index, address, pLineItem->strInstr, pLineItem->strArg);

                if (!m_okDisasmSubtitles)  //NOTE: Subtitles can move lines down
                {
                    if (DisasmView_CheckForJump(memory + index, &pLineItem->jumpdelta))
                    {
                        pLineItem->type |= LINETYPE_JUMP;
                    }

                    if (address == proccurrent)  // For current instruction, prepare the instruction hints
                    {
                        m_okDisasmJumpPredict = DisasmView_GetJumpConditionHint(memory + index, pProc, pMemCtl, m_strDisasmHint);
                        if (*m_strDisasmHint == 0)  // we don't have the jump hint
                        {
                            DisasmView_GetInstructionHint(memory + index, pProc, pMemCtl, m_strDisasmHint, m_strDisasmHint2);
                        }
                    }
                }
            }
        }
        if (length > 0) length--;

        address += 2;
        lineindex++;
        if (lineindex >= MAX_DISASMLINECOUNT)
            break;
    }
}


//////////////////////////////////////////////////////////////////////
// Draw functions

void DisasmView_DrawJump(HDC hdc, int yFrom, int delta, int x, int cyLine, COLORREF color)
{
    int dist = abs(delta);
    if (dist < 2) dist = 2;
    if (dist > 20) dist = 16;

    int yTo = yFrom + delta * cyLine;
    yFrom += cyLine / 2;

    HPEN hPenJump = ::CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = ::SelectObject(hdc, hPenJump);

    POINT points[4];
    points[0].x = x;  points[0].y = yFrom;
    points[1].x = x + dist * 4;  points[1].y = yFrom;
    points[2].x = x + dist * 12;  points[2].y = yTo;
    points[3].x = x;  points[3].y = yTo;
    PolyBezier(hdc, points, 4);
    MoveToEx(hdc, x - 4, points[3].y, NULL);
    LineTo(hdc, x + 4, yTo - 1);
    MoveToEx(hdc, x - 4, points[3].y, NULL);
    LineTo(hdc, x + 4, yTo + 1);

    ::SelectObject(hdc, oldPen);
    ::DeleteObject(hPenJump);
}

void DisasmView_DoDraw(HDC hdc)
{
    ASSERT(g_pBoard != NULL);

    // Create and select font
    HFONT hFont = CreateMonospacedFont();
    HGDIOBJ hOldFont = SelectObject(hdc, hFont);
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorOld = SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
    SetBkMode(hdc, TRANSPARENT);
    //COLORREF colorBkOld = SetBkColor(hdc, GetSysColor(COLOR_WINDOW));

    CProcessor* pDisasmPU = (m_okDisasmProcessor) ? g_pBoard->GetCPU() : g_pBoard->GetPPU();

    // Draw disassembly for the current processor
    WORD prevPC = (m_okDisasmProcessor) ? g_wEmulatorPrevCpuPC : g_wEmulatorPrevPpuPC;
    int yFocus = DisasmView_DrawDisassemble(hdc, pDisasmPU, m_wDisasmBaseAddr, prevPC, 0, 2 + 0 * cyLine);

    SetTextColor(hdc, colorOld);
    //SetBkColor(hdc, colorBkOld);
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);

    if (::GetFocus() == m_hwndDisasmViewer)
    {
        RECT rcFocus;
        GetClientRect(m_hwndDisasmViewer, &rcFocus);
        if (yFocus >= 0)
        {
            rcFocus.top = yFocus - 1;
            rcFocus.bottom = yFocus + cyLine;
        }
        DrawFocusRect(hdc, &rcFocus);
    }
}

void DisasmView_DrawBreakpoint(HDC hdc, int x, int y, int size)
{
    COLORREF colorBreakpoint = Settings_GetColor(ColorDebugBreakpoint);
    HBRUSH hBreakBrush = CreateSolidBrush(colorBreakpoint);
    HGDIOBJ hOldBrush = SelectObject(hdc, hBreakBrush);
    HGDIOBJ hOldPen = SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, x, y, x + size, y + size);
    ::SelectObject(hdc, hOldPen);
    ::SelectObject(hdc, hOldBrush);
    ::DeleteObject(hBreakBrush);
}

int DisasmView_DrawDisassemble(HDC hdc, CProcessor* pProc, WORD current, WORD previous, int x, int y)
{
    int result = -1;

    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    m_cxDisasmBreakpointZone = x + cxChar * 2;
    m_cyDisasmLine = cyLine;
    COLORREF colorText = Settings_GetColor(ColorDebugText);
    COLORREF colorPrev = Settings_GetColor(ColorDebugPrevious);
    COLORREF colorValue = Settings_GetColor(ColorDebugValue);
    COLORREF colorValueRom = Settings_GetColor(ColorDebugValueRom);
    COLORREF colorSubtitle = Settings_GetColor(ColorDebugSubtitles);
    COLORREF colorJump = Settings_GetColor(ColorDebugJump);
    ::SetTextColor(hdc, colorText);

    WORD proccurrent = pProc->GetPC();

    // Draw current line background
    if (!m_okDisasmSubtitles)  //NOTE: Subtitles can move lines down
    {
        int yCurrent = (proccurrent - (current - 5)) * cyLine;
        COLORREF colorBackCurr = Settings_GetColor(ColorDebugBackCurrent);
        HBRUSH hBrushCurrent = ::CreateSolidBrush(colorBackCurr);
        HGDIOBJ oldBrush = ::SelectObject(hdc, hBrushCurrent);
        PatBlt(hdc, 0, yCurrent, 1000, cyLine, PATCOPY);
        ::SelectObject(hdc, oldBrush);
        ::DeleteObject(hBrushCurrent);
    }

    for (int lineindex = 0; lineindex < MAX_DISASMLINECOUNT; lineindex++)  // Draw the lines
    {
        DisasmLineItem* pLineItem = m_pDisasmLineItems + lineindex;
        if (pLineItem->type == LINETYPE_NONE)
            break;
        WORD address = pLineItem->address;

        if ((pLineItem->type & LINETYPE_SUBTITLE) != 0 && (pLineItem->type & (LINETYPE_DATA | LINETYPE_INSTR)) == 0 &&
            pLineItem->pSubItem != nullptr)  // Subtitles - comment for a block
        {
            LPCTSTR strBlockSubtitle = pLineItem->pSubItem->comment;

            ::SetTextColor(hdc, colorSubtitle);
            TextOut(hdc, x + 21 * cxChar, y, strBlockSubtitle, (int) _tcslen(strBlockSubtitle));
            ::SetTextColor(hdc, colorText);

            y += cyLine;
            continue;
        }

        if (Emulator_IsBreakpoint(m_okDisasmProcessor, address))  // Breakpoint
        {
            DisasmView_DrawBreakpoint(hdc, x + cxChar / 2, y, cyLine);
        }

        DrawOctalValue(hdc, x + 5 * cxChar, y, address);  // Address
        // Value at the address
        WORD value = pLineItem->value;
        int memorytype = pLineItem->addrtype;
        ::SetTextColor(hdc, (memorytype == ADDRTYPE_ROM) ? colorValueRom : colorValue);
        DrawOctalValue(hdc, x + 13 * cxChar, y, value);
        ::SetTextColor(hdc, colorText);

        // Current position
        if (address == current)
        {
            //TextOut(hdc, x + 2 * cxChar, y, _T(" > "), 3);
            result = y;  // Remember line for the focus rect
        }
        if (address == proccurrent)
            TextOut(hdc, x + 2 * cxChar, y, _T("PC>"), 3);
        else if (address == previous)
        {
            ::SetTextColor(hdc, colorPrev);
            TextOut(hdc, x + 2 * cxChar, y, _T(" > "), 3);
        }

        int posAfterArgs = 30;
        if ((pLineItem->type & (LINETYPE_DATA | LINETYPE_INSTR)) != 0)
        {
            LPCTSTR strInstr = pLineItem->strInstr;
            LPCTSTR strArg = pLineItem->strArg;
            ::SetTextColor(hdc, colorText);
            TextOut(hdc, x + 21 * cxChar, y, strInstr, (int)_tcslen(strInstr));
            TextOut(hdc, x + 29 * cxChar, y, strArg, (int)_tcslen(strArg));
            posAfterArgs += _tcslen(strArg);
        }

        if ((pLineItem->type & LINETYPE_SUBTITLE) != 0 && (pLineItem->type & (LINETYPE_DATA | LINETYPE_INSTR)) != 0 &&
            pLineItem->pSubItem != nullptr)  // Show subtitle comment for instruction or data
        {
            LPCTSTR strComment = pLineItem->pSubItem->comment;
            if (strComment != nullptr)
            {
                ::SetTextColor(hdc, colorSubtitle);
                TextOut(hdc, x + 52 * cxChar, y, strComment, (int)_tcslen(strComment));
                ::SetTextColor(hdc, colorText);
            }
        }

        if (!m_okDisasmSubtitles)  // We don't show jumps and hints with subtitles
        {
            bool isjump = (pLineItem->type & LINETYPE_JUMP) != 0;

            if (isjump)
            {
                int delta = pLineItem->jumpdelta;
                if (abs(delta) < 40)
                {
                    COLORREF jumpcolor = colorJump;
                    if (address == proccurrent)
                        jumpcolor = Settings_GetColor(m_okDisasmJumpPredict ? ColorDebugJumpYes : ColorDebugJumpNo);
                    DisasmView_DrawJump(hdc, y, delta, x + posAfterArgs * cxChar, cyLine, jumpcolor);
                }
            }

            if (address == proccurrent && *m_strDisasmHint != 0)  // For current instruction, draw "Instruction Hints"
            {
                COLORREF hintcolor = Settings_GetColor(isjump ? ColorDebugJumpHint : ColorDebugHint);
                ::SetTextColor(hdc, hintcolor);
                TextOut(hdc, x + 52 * cxChar, y, m_strDisasmHint, (int)_tcslen(m_strDisasmHint));
                if (*m_strDisasmHint2 != 0)
                    TextOut(hdc, x + 52 * cxChar, y + cyLine, m_strDisasmHint2, (int)_tcslen(m_strDisasmHint2));
                ::SetTextColor(hdc, colorText);
            }
        }

        y += cyLine;
    }

    return result;
}


//////////////////////////////////////////////////////////////////////
