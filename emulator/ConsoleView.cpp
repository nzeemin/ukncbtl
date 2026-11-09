/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// ConsoleView.cpp

#include "stdafx.h"
#include <print>
#include <system_error>
#include "Main.h"
#include "Views.h"
#include "ToolWindow.h"
#include "Emulator.h"
#include "emubase/Emubase.h"
#include <vector>
#include "CommandHistory.h"
#include "ConsoleCommands.h"

//////////////////////////////////////////////////////////////////////


COLORREF COLOR_COMMANDFOCUS = RGB(255, 242, 157);

HWND g_hwndConsole = (HWND)INVALID_HANDLE_VALUE;  // Console View window handle
WNDPROC m_wndprocConsoleToolWindow = nullptr;  // Old window proc address of the ToolWindow

HWND m_hwndConsoleLog = (HWND)INVALID_HANDLE_VALUE;  // Console log window - read-only edit control
HWND m_hwndConsoleEdit = (HWND)INVALID_HANDLE_VALUE;  // Console line - edit control
HWND m_hwndConsolePrompt = (HWND)INVALID_HANDLE_VALUE;  // Console prompt - static control
HFONT m_hfontConsole = nullptr;
WNDPROC m_wndprocConsoleEdit = nullptr;  // Old window proc address of the console prompt
bool m_okCurrentProc = false;  // Current processor: TRUE - CPU, FALSE - PPU
HBRUSH m_hbrConsoleFocused = nullptr;

static CommandHistory commandHistory;

void ConsoleView_HistoryAdd(LPCTSTR command);
void ConsoleView_HistoryUp();
void ConsoleView_HistoryDown();

CProcessor* ConsoleView_GetCurrentProcessor();
void ConsoleView_AdjustWindowLayout();
LRESULT CALLBACK ConsoleEditWndProc(HWND, UINT, WPARAM, LPARAM);
void ConsoleView_DoConsoleCommand();

void ConsoleView_PrintConsolePrompt();
void ConsoleView_PrintRegister(LPCTSTR strName, WORD value);
void ConsoleView_PrintMemoryDump(CProcessor* pProc, WORD address, int lines);
BOOL ConsoleView_SaveMemoryDump(CProcessor* pProc, uint16_t addr1, uint16_t addr2);

const LPCTSTR MESSAGE_UNKNOWN_COMMAND = _T("  Unknown command.\r\n");
const LPCTSTR MESSAGE_WRONG_VALUE = _T("  Wrong value.\r\n");
const LPCTSTR MESSAGE_INVALID_REGNUM = _T("  Invalid register number, 0..7 expected.\r\n");


//////////////////////////////////////////////////////////////////////


void ConsoleView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = ConsoleViewWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInst;
    wcex.hIcon          = nullptr;
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszMenuName   = nullptr;
    wcex.lpszClassName  = CLASSNAME_CONSOLEVIEW;
    wcex.hIconSm        = nullptr;

    RegisterClassEx(&wcex);
}

// Create Console View as child of Main Window
void ConsoleView_Create(HWND hwndParent, int x, int y, int width, int height)
{
    ASSERT(hwndParent != nullptr);

    m_okCurrentProc = Settings_GetDebugCpuPpu() != 0;

    g_hwndConsole = CreateWindow(
            CLASSNAME_TOOLWINDOW, nullptr,
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            x, y, width, height,
            hwndParent, nullptr, g_hInst, nullptr);
    SetWindowText(g_hwndConsole, _T("Debug Console"));

    // ToolWindow subclassing
    m_wndprocConsoleToolWindow = reinterpret_cast<WNDPROC>(SetWindowLongPtr(g_hwndConsole, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ConsoleViewWndProc)));

    RECT rcConsole;  GetClientRect(g_hwndConsole, &rcConsole);

    m_hwndConsoleEdit = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            _T("EDIT"), nullptr,
            WS_CHILD | WS_VISIBLE,
            90, rcConsole.bottom - 20,
            rcConsole.right - 90, 20,
            g_hwndConsole, nullptr, g_hInst, nullptr);
    m_hwndConsoleLog = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            _T("EDIT"), nullptr,
            WS_CHILD | WS_VSCROLL | WS_VISIBLE | ES_READONLY | ES_MULTILINE,
            0, 0,
            rcConsole.right, rcConsole.bottom - 20,
            g_hwndConsole, nullptr, g_hInst, nullptr);
    m_hwndConsolePrompt = CreateWindowEx(
            0,
            _T("STATIC"), nullptr,
            WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_CENTER | SS_NOPREFIX,
            0, rcConsole.bottom - 20,
            90, 20,
            g_hwndConsole, nullptr, g_hInst, nullptr);

    m_hfontConsole = CreateMonospacedFont();
    SendMessage(m_hwndConsolePrompt, WM_SETFONT, (WPARAM)m_hfontConsole, 0);
    SendMessage(m_hwndConsoleEdit, WM_SETFONT, (WPARAM)m_hfontConsole, 0);
    SendMessage(m_hwndConsoleLog, WM_SETFONT, (WPARAM)m_hfontConsole, 0);

    // Edit box subclassing
    m_wndprocConsoleEdit = reinterpret_cast<WNDPROC>(SetWindowLongPtr(m_hwndConsoleEdit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ConsoleEditWndProc)));

    ShowWindow(g_hwndConsole, SW_SHOW);
    UpdateWindow(g_hwndConsole);

    ConsoleView_Print(_T("Use 'h' command to show help.\r\n\r\n"));
    ConsoleView_PrintConsolePrompt();
    SetFocus(m_hwndConsoleEdit);
}

// Adjust position of client windows
void ConsoleView_AdjustWindowLayout()
{
    RECT rc;  GetClientRect(g_hwndConsole, &rc);
    int promptWidth = 90;

    if (m_hwndConsolePrompt != (HWND)INVALID_HANDLE_VALUE)
        SetWindowPos(m_hwndConsolePrompt, nullptr, 0, rc.bottom - 20, promptWidth, 20, SWP_NOZORDER);
    if (m_hwndConsoleEdit != (HWND)INVALID_HANDLE_VALUE)
        SetWindowPos(m_hwndConsoleEdit, nullptr, promptWidth, rc.bottom - 20, rc.right - promptWidth, 20, SWP_NOZORDER);
    if (m_hwndConsoleLog != (HWND)INVALID_HANDLE_VALUE)
        SetWindowPos(m_hwndConsoleLog, nullptr, 0, 0, rc.right, rc.bottom - 24, SWP_NOZORDER);
}

LRESULT CALLBACK ConsoleViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    LRESULT lResult;
    switch (message)
    {
    case WM_DESTROY:
        g_hwndConsole = (HWND)INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
        break;
    case WM_CTLCOLORSTATIC:
        if (((HWND)lParam) == m_hwndConsoleLog)
        {
            SetBkColor((HDC)wParam, ::GetSysColor(COLOR_WINDOW));
            return (LRESULT)::GetSysColorBrush(COLOR_WINDOW);
        }
        break;
    case WM_CTLCOLOREDIT:
        if (((HWND)lParam) == m_hwndConsoleEdit && ::GetFocus() == m_hwndConsoleEdit)
        {
            if (m_hbrConsoleFocused == nullptr)
                m_hbrConsoleFocused = ::CreateSolidBrush(COLOR_COMMANDFOCUS);
            SetBkColor((HDC)wParam, COLOR_COMMANDFOCUS);
            return (LRESULT)m_hbrConsoleFocused;
        }
        return CallWindowProc(m_wndprocConsoleToolWindow, hWnd, message, wParam, lParam);
    case WM_SIZE:
        lResult = CallWindowProc(m_wndprocConsoleToolWindow, hWnd, message, wParam, lParam);
        ConsoleView_AdjustWindowLayout();
        return lResult;
    }

    return CallWindowProc(m_wndprocConsoleToolWindow, hWnd, message, wParam, lParam);
}

static TcharString ConsoleView_ReadCommand()
{
    const int length = GetWindowTextLength(m_hwndConsoleEdit);
    TcharString text(static_cast<size_t>(length) + 1, _T('\0'));
    const int copied = GetWindowText(m_hwndConsoleEdit, text.data(), length + 1);
    text.resize(copied);
    return text;
}
LRESULT CALLBACK ConsoleEditWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CHAR:
        if (wParam == 13)
        {
            const auto command = ConsoleView_ReadCommand();
            ConsoleView_HistoryAdd(command.c_str());
            ConsoleView_DoConsoleCommand();
            return 0;
        }
        if (wParam == VK_ESCAPE)
        {
            commandHistory.ResetNavigation();
            TCHAR command[32];
            GetWindowText(m_hwndConsoleEdit, command, 32);
            if (*command == 0)  // If command is empty
                SetFocus(g_hwndScreen);
            else
                SendMessage(m_hwndConsoleEdit, WM_SETTEXT, 0, (LPARAM)_T(""));  // Clear command
            return 0;
        }
        commandHistory.ResetNavigation();  // The line is being edited, next Up starts from the newest
        break;
    case WM_KEYDOWN:
        if (wParam == VK_UP)
        {
            ConsoleView_HistoryUp();
            return 0;
        }
        if (wParam == VK_DOWN)
        {
            ConsoleView_HistoryDown();
            return 0;
        }
        break;
    }

    return CallWindowProc(m_wndprocConsoleEdit, hWnd, message, wParam, lParam);
}

void ConsoleView_HistoryAdd(LPCTSTR command)
{
    commandHistory.Add(command);
}

void ConsoleView_HistoryShow()
{
    const auto text = commandHistory.Current();
    const auto len = _tcslen(text);
    SendMessage(m_hwndConsoleEdit, WM_SETTEXT, 0, (LPARAM)text);
    SendMessage(m_hwndConsoleEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
}
void ConsoleView_HistoryUp()
{
    if (commandHistory.Size() == 0) return;
    commandHistory.Up();
    ConsoleView_HistoryShow();
}
void ConsoleView_HistoryDown()
{
    if (!commandHistory.Down()) return;
    ConsoleView_HistoryShow();
}
void ConsoleView_Activate()
{
    if (g_hwndConsole == INVALID_HANDLE_VALUE) return;

    SetFocus(m_hwndConsoleEdit);
}

CProcessor* ConsoleView_GetCurrentProcessor()
{
    if (m_okCurrentProc)
        return g_pBoard->GetCPU();
    else
        return g_pBoard->GetPPU();
}

void ConsoleView_Print(LPCTSTR message)
{
    if (m_hwndConsoleLog == INVALID_HANDLE_VALUE) return;

    // Put selection to the end of text
    SendMessage(m_hwndConsoleLog, EM_SETSEL, 0x100000, 0x100000);
    // Insert the message
    SendMessage(m_hwndConsoleLog, EM_REPLACESEL, (WPARAM)FALSE, (LPARAM)message);
    // Scroll to caret
    SendMessage(m_hwndConsoleLog, EM_SCROLLCARET, 0, 0);
}

void ConsoleView_ClearConsole()
{
    if (m_hwndConsoleLog == INVALID_HANDLE_VALUE) return;

    SendMessage(m_hwndConsoleLog, WM_SETTEXT, 0, (LPARAM)_T(""));
}

void ConsoleView_PrintConsolePrompt()
{
    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    TCHAR bufferAddr[7];
    PrintOctalValue(bufferAddr, pProc->GetPC());
    TCHAR buffer[14];
    FormatBuffer(buffer, _T("{}:{}>"), pProc->GetName(), bufferAddr);
    ::SetWindowText(m_hwndConsolePrompt, buffer);
}

// Print register name, octal value and binary value
void ConsoleView_PrintRegister(LPCTSTR strName, WORD value)
{
    TCHAR buffer[36];
    TCHAR* p = buffer;
    *p++ = _T(' ');
    *p++ = _T(' ');
    lstrcpy(p, strName);
    if (lstrlen(strName) > 2)
        p += 3;
    else
    {
        p += 2;
        *p++ = _T(' ');
    }
    PrintOctalValue(p, value);  p += 6;
    *p++ = _T(' ');
    PrintHexValue(p, value);  p += 4;
    *p++ = _T(' ');
    PrintBinaryValue(p, value);  p += 16;
    *p++ = _T('\r');
    *p++ = _T('\n');
    *p++ = 0;
    ConsoleView_Print(buffer);
}

void ConsoleView_SetCurrentProc(BOOL okCPU)
{
    m_okCurrentProc = okCPU != 0;
    ConsoleView_PrintConsolePrompt();
}

void ConsoleView_StepInto()
{
    // Put command to console prompt
    SendMessage(m_hwndConsoleEdit, WM_SETTEXT, 0, (LPARAM)_T("s"));
    // Execute command
    ConsoleView_DoConsoleCommand();
}
void ConsoleView_StepOver()
{
    // Put command to console prompt
    SendMessage(m_hwndConsoleEdit, WM_SETTEXT, 0, (LPARAM)_T("so"));
    // Execute command
    ConsoleView_DoConsoleCommand();
}
void ConsoleView_DeleteAllBreakpoints()
{
    // Put command to console prompt
    SendMessage(m_hwndConsoleEdit, WM_SETTEXT, 0, (LPARAM)_T("bc"));
    // Execute command
    ConsoleView_DoConsoleCommand();
}

BOOL ConsoleView_SaveMemoryDump(CProcessor *pProc, uint16_t addr1, uint16_t addr2)
{
    if (addr2 <= addr1)
    {
        ConsoleView_PrintFormat(_T("  End address should be greater than start address.\r\n"));
        return FALSE;
    }

    uint16_t waddr1 = addr1 / 2;
    uint16_t waddr2 = addr2 & 1 ? addr2 / 2 : (addr2 - 1) / 2;
    size_t bsize = (waddr2 - waddr1 + 1) * 2;

    CMemoryController* pMemCtl = pProc->GetMemoryController();
    std::vector<uint16_t> data(bsize / sizeof(uint16_t));

    bool isHaltMode = pProc->IsHaltMode();
    for (uint16_t i = waddr1; i <= waddr2; i++)
        data[i - waddr1] = pMemCtl->GetWordView(i * 2, isHaltMode, false).value;

    TCHAR fname[20];
    FormatBuffer(fname, _T("memdump{}.bin"), pProc->GetName());
    HANDLE file = ::CreateFile(fname,
            GENERIC_WRITE, FILE_SHARE_READ, nullptr,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return FALSE;

    DWORD dwBytesWritten = 0;
    BOOL written = ::WriteFile(file, data.data(), static_cast<DWORD>(bsize), &dwBytesWritten, nullptr);
    ::CloseHandle(file);
    if (!written || dwBytesWritten != bsize)
        return FALSE;

    ConsoleView_PrintFormat(_T("  {} memory saved to {}\r\n"), pProc->GetName(), fname);
    return TRUE;
}

void ConsoleView_SaveDisplayListDump() try
{
    LPCTSTR fname = _T("displaylist.txt");
    UniqueFile fpFile(::_tfopen(fname, _T("wt")));
    if (fpFile == nullptr)
    {
        ConsoleView_Print(_T("  Failed to create display list file.\r\n"));
        return;
    }
    std::print(fpFile.get(), "line address  tag 1  tag 2  addr   next   \n");

    WORD address = 0000270;  // Tag sequence start address
    bool okTagSize = false;  // Tag size: TRUE - 4-word, false - 2-word (first tag is always 2-word)
    bool okTagType = false;  // Type of 4-word tag: TRUE - set palette, false - set params

    for (int yy = 0; yy < 307; yy++)
    {
        std::print(fpFile.get(), "{:4d}  {:06o}  ", yy, address);

        WORD tag2 = 0;
        if (okTagSize)  // 4-word tag
        {
            WORD tag1 = g_pBoard->GetRAMWord(0, address);
            address += 2;
            tag2 = g_pBoard->GetRAMWord(0, address);
            address += 2;
            std::print(fpFile.get(), "{:06o} {:06o} ", tag1, tag2);
        }
        else
            std::print(fpFile.get(), "------ ------ ");

        WORD addressBits = g_pBoard->GetRAMWord(0, address);  // The word before the last word - is address of bits from all three memory planes
        address += 2;
        std::print(fpFile.get(), "{:06o} ", addressBits);

        WORD tagB = g_pBoard->GetRAMWord(0, address);  // Last word of the tag - is address and type of the next tag
        std::print(fpFile.get(), "{:06o}  ", tagB);

        if (okTagSize)  // 4-word tag
        {
            if (okTagType)  // 4-word palette tag
                std::print(fpFile.get(), "palette; ");
            else
            {
                int scale = (tag2 >> 4) & 3;  // Bits 4-5 - new scale value
                scale = 1 << scale;
                std::print(fpFile.get(), "scale {}; ", scale);
            }
        }

        // Calculate size, type and address of the next tag
        bool okNextTagSize = (tagB & 2) != 0;  // Bit 1 shows size of the next tag
        bool okNextTagType = okTagType;
        if (okNextTagSize)
        {
            address = tagB & ~7;
            okNextTagType = (tagB & 4) != 0;  // Bit 2 shows type of the next tag
            std::print(fpFile.get(), "next: 4-word at {:06o}", address);
        }
        else
            address = tagB & ~3;

        okTagSize = okNextTagSize;
        okTagType = okNextTagType;

        std::print(fpFile.get(), "\n");
    }

    const bool writeFailed = std::ferror(fpFile.get()) != 0;
    const bool closeFailed = std::fclose(fpFile.release()) != 0;
    if (writeFailed || closeFailed)
    {
        ConsoleView_Print(_T("  Failed to write display list file.\r\n"));
        return;
    }

    ConsoleView_PrintFormat(_T("  Display list saved to {}\r\n"), fname);
}

catch (const std::system_error&)
{
    ConsoleView_Print(_T("  Failed to write display list file.\r\n"));
}

// Print memory dump
void ConsoleView_PrintMemoryDump(CProcessor* pProc, WORD address, int lines = 8)
{
    address &= ~1;  // Line up to even address

    CMemoryController* pMemCtl = pProc->GetMemoryController();
    bool okHaltMode = pProc->IsHaltMode();

    for (int line = 0; line < lines; line++)
    {
        WORD dump[8];
        for (WORD i = 0; i < 8; i++)
            dump[i] = pMemCtl->GetWordView(address + i * 2, okHaltMode, false).value;

        TCHAR buffer[2 + 6 + 2 + 7 * 8 + 1 + 16 + 1 + 2];
        TCHAR* pBuf = buffer;
        *pBuf = _T(' ');  pBuf++;
        *pBuf = _T(' ');  pBuf++;
        PrintOctalValue(pBuf, address);  pBuf += 6;
        *pBuf = _T(' ');  pBuf++;
        *pBuf = _T(' ');  pBuf++;
        for (int i = 0; i < 8; i++)
        {
            PrintOctalValue(pBuf, dump[i]);  pBuf += 6;
            *pBuf = _T(' ');  pBuf++;
        }
        *pBuf = _T(' ');  pBuf++;
        for (int i = 0; i < 8; i++)
        {
            WORD word = dump[i];
            BYTE ch1 = LOBYTE(word);
            TCHAR wch1 = Translate_KOI8R(ch1);
            if (ch1 < 32) wch1 = _T('·');
            *pBuf = wch1;  pBuf++;
            BYTE ch2 = HIBYTE(word);
            TCHAR wch2 = Translate_KOI8R(ch2);
            if (ch2 < 32) wch2 = _T('·');
            *pBuf = wch2;  pBuf++;
        }
        *pBuf++ = _T('\r');
        *pBuf++ = _T('\n');
        *pBuf = 0;

        ConsoleView_Print(buffer);

        address += 16;
    }
}

// Print disassembled instructions
// Return value: number of words disassembled
int ConsoleView_PrintDisassemble(CProcessor* pProc, WORD address, BOOL okOneInstr, BOOL okShort)
{
    CMemoryController* pMemCtl = pProc->GetMemoryController();
    bool okHaltMode = pProc->IsHaltMode();

    const int nWindowSize = 30;
    WORD memory[nWindowSize + 2];
    for (WORD i = 0; i < nWindowSize + 2; i++)
        memory[i] = pMemCtl->GetWordView(address + i * 2, okHaltMode, true).value;

    TCHAR bufaddr[7];
    TCHAR bufvalue[7];
    TCHAR buffer[64];

    int totalLength = 0;
    int lastLength = 0;
    int length = 0;
    for (int index = 0; index < nWindowSize; index++)  // Draw the lines
    {
        PrintOctalValue(bufaddr, address);
        WORD value = memory[index];
        PrintOctalValue(bufvalue, value);

        if (length > 0)
        {
            if (!okShort)
            {
                FormatBuffer(buffer, _T("  {}  {}\r\n"), bufaddr, bufvalue);
                ConsoleView_Print(buffer);
            }
        }
        else
        {
            if (okOneInstr && index > 0)
                break;
            const auto decoded = DisassembleInstruction(std::span{memory}.subspan(index), address);
            if (!decoded)
                break;
            length = decoded->length;
            lastLength = length;
            if (index + length > nWindowSize)
                break;
            if (okShort)
                FormatBuffer(buffer, _T("  {}  {:<7} {}\r\n"), bufaddr, decoded->mnemonic.c_str(), decoded->argument.c_str());
            else
                FormatBuffer(buffer, _T("  {}  {}  {:<7} {}\r\n"), bufaddr, bufvalue, decoded->mnemonic.c_str(), decoded->argument.c_str());
            ConsoleView_Print(buffer);
        }
        length--;
        address += 2;
        totalLength++;
    }

    return totalLength;
}

#if !defined(PRODUCT)
void ConsoleView_TraceLog(DWORD value)
{
    Emulator_SetTrace(value);
    if (value != TRACE_NONE)
        ConsoleView_PrintFormat(_T("  Trace ON, trace flags {:06o}\r\n"), (uint16_t)g_pBoard->GetTrace());
    else
    {
        ConsoleView_Print(_T("  Trace OFF.\r\n"));
        DebugLogCloseFile();
    }
}
#endif


//////////////////////////////////////////////////////////////////////
// Console commands handlers


void ConsoleView_CmdShowHelp(const ConsoleCommandParams& /*params*/)
{
    ConsoleView_Print(_T("Console command list:\r\n")
            _T("  c          Clear console log\r\n")
            _T("  d          Disassemble from PC; use D for short format\r\n")
            _T("  dXXXXXX    Disassemble from address XXXXXX\r\n")
            _T("  g          Go; free run\r\n")
            _T("  gXXXXXX    Go; run and stop at address XXXXXX\r\n")
            _T("  m          Memory dump at current address\r\n")
            _T("  mXXXXXX    Memory dump at address XXXXXX\r\n")
            _T("  mrN        Memory dump at address from register N; N=0..7\r\n")
            _T("  mXXXXXX YYYYYY  Set memory value at address XXXXXX\r\n")
            _T("  p          Switch to other processor\r\n")
            _T("  r          Show register values\r\n")
            _T("  rN         Show value of register N; N=0..7,ps\r\n")
            _T("  rN XXXXXX  Set register N to value XXXXXX; N=0..7,ps\r\n")
            _T("  s          Step Into; executes one instruction\r\n")
            _T("  so         Step Over; executes and stops after the current instruction\r\n")
            _T("  b          List breakpoints set for the current processor\r\n")
            _T("  bXXXXXX    Set breakpoint at address XXXXXX\r\n")
            _T("  bcXXXXXX   Remove breakpoint at address XXXXXX\r\n")
            _T("  bc         Remove all breakpoints for the current processor\r\n")
            _T("  u          Save memory dump to file memdumpXPU.bin\r\n")
            _T("  uXXXXXX YYYYYY  Save memory fragment to file memdumpXPU.bin\r\n")
            _T("  udl        Save display list to file displaylist.txt\r\n")
            _T("  fcXXXXXX YYYYYY  Calculate floating number for the two octal words\r\n")
#if !defined(PRODUCT)
            _T("  t          Tracing on/off to trace.log file\r\n")
            _T("  tXXXXXX    Set tracing flags\r\n")
            _T("  tc         Clear trace.log file\r\n")
#endif
                     );
}

void ConsoleView_CmdClearConsoleLog(const ConsoleCommandParams& /*params*/)
{
    ConsoleView_ClearConsole();
}

void ConsoleView_CmdPrintRegister(const ConsoleCommandParams& params)
{
    int r = params.paramReg1;

    LPCTSTR name = REGISTER_NAME[r];
    const CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    uint16_t value = pProc->GetReg(r);
    ConsoleView_PrintRegister(name, value);
}
void ConsoleView_CmdPrintRegisterPSW(const ConsoleCommandParams& /*params*/)
{
    const CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    WORD value = pProc->GetPSW();
    ConsoleView_PrintRegister(_T("PS"), value);
}

void ConsoleView_CmdSetRegisterValue(const ConsoleCommandParams& params)
{
    int r = params.paramReg1;
    uint16_t value = params.paramOct1;

    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    pProc->SetReg(r, value);

    MainWindow_UpdateAllViews();
}
void ConsoleView_CmdSetRegisterPSW(const ConsoleCommandParams& params)
{
    uint16_t value = params.paramOct1;

    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    pProc->SetPSW(value);

    MainWindow_UpdateAllViews();
}

void ConsoleView_CmdSaveMemoryDump(const ConsoleCommandParams& /*params*/)
{
    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    ConsoleView_SaveMemoryDump(pProc, 0, 0xffff);
}

void ConsoleView_CmdSaveMemoryFragment(const ConsoleCommandParams& params)
{
    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    ConsoleView_SaveMemoryDump(pProc, params.paramOct1, params.paramOct2);
}

void ConsoleView_CmdPrintMemoryDumpAtPC(const ConsoleCommandParams& /*params*/)
{
    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    uint16_t address = pProc->GetPC();
    ConsoleView_PrintMemoryDump(pProc, address);
}
void ConsoleView_CmdPrintMemoryDumpAtAddress(const ConsoleCommandParams& params)
{
    uint16_t address = params.paramOct1;

    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    ConsoleView_PrintMemoryDump(pProc, address);
}
void ConsoleView_CmdPrintMemoryDumpAtRegister(const ConsoleCommandParams& params)
{
    int r = params.paramReg1;

    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    uint16_t address = pProc->GetReg(r);
    ConsoleView_PrintMemoryDump(pProc, address);
}

void ConsoleView_CmdPrintDisassembleAtPC(const ConsoleCommandParams& params)
{
    BOOL okShort = (params.commandText[0] == _T('D'));
    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    uint16_t address = pProc->GetPC();
    ConsoleView_PrintDisassemble(pProc, address, FALSE, okShort);
}
void ConsoleView_CmdPrintDisassembleAtAddress(const ConsoleCommandParams& params)
{
    uint16_t address = params.paramOct1;

    BOOL okShort = (params.commandText[0] == _T('D'));
    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    ConsoleView_PrintDisassemble(pProc, address, FALSE, okShort);
}

void ConsoleView_CmdSwitchCpuPpu(const ConsoleCommandParams& /*params*/)
{
    m_okCurrentProc = !m_okCurrentProc;
    DebugView_SetCurrentProc(m_okCurrentProc);   // Switch DebugView to current processor
    DisasmView_SetCurrentProc(m_okCurrentProc);   // Switch DisasmView to current processor

    LPCTSTR procName = ConsoleView_GetCurrentProcessor()->GetName();
    ConsoleView_PrintFormat(_T("  Switched to {}\r\n"), procName);
}

void ConsoleView_CmdPrintAllRegisters(const ConsoleCommandParams& /*params*/)
{
    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    for (int r = 0; r < 8; r++)
    {
        LPCTSTR name = REGISTER_NAME[r];
        WORD value = pProc->GetReg(r);
        ConsoleView_PrintRegister(name, value);
    }
    ConsoleView_PrintRegister(_T("PC'"), pProc->GetCPC());
    ConsoleView_PrintRegister(_T("PS"), pProc->GetPSW());
    ConsoleView_PrintRegister(_T("PS'"), pProc->GetCPSW());
}

void ConsoleView_CmdStepInto(const ConsoleCommandParams& /*params*/)
{
    CProcessor* pProc = ConsoleView_GetCurrentProcessor();

    ConsoleView_PrintDisassemble(pProc, pProc->GetPC(), TRUE, FALSE);

    g_pBoard->DebugTicks();

    MainWindow_UpdateAllViews();
}
void ConsoleView_CmdStepOver(const ConsoleCommandParams& /*params*/)
{
    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    CMemoryController* pMemCtl = pProc->GetMemoryController();

    int instrLength = ConsoleView_PrintDisassemble(pProc, pProc->GetPC(), TRUE, FALSE);

    uint16_t instr = pMemCtl->GetWordView(pProc->GetPC(), pProc->IsHaltMode(), true).value;

    // For JMP and BR use Step Into logic, not Step Over
    const auto target = console_command::StepOverTarget(instr, pProc->GetPC(), instrLength);
    if (!target)
    {
        g_pBoard->DebugTicks();

        MainWindow_UpdateAllViews();

        return;
    }

    const uint16_t bpaddress = *target;

    if (m_okCurrentProc)
        Emulator_SetTempCPUBreakpoint(bpaddress);
    else
        Emulator_SetTempPPUBreakpoint(bpaddress);
    Emulator_Start();
}
void ConsoleView_CmdRun(const ConsoleCommandParams& /*params*/)
{
    Emulator_Start();
}
void ConsoleView_CmdRunToAddress(const ConsoleCommandParams& params)
{
    uint16_t address = params.paramOct1;

    if (m_okCurrentProc)
        Emulator_SetTempCPUBreakpoint(address);
    else
        Emulator_SetTempPPUBreakpoint(address);
    Emulator_Start();
}

void ConsoleView_CmdPrintAllBreakpoints(const ConsoleCommandParams& /*params*/)
{
    const uint16_t* pbps = m_okCurrentProc ? Emulator_GetCPUBreakpointList() : Emulator_GetPPUBreakpointList();
    if (pbps == nullptr || *pbps == 0177777)
    {
        ConsoleView_Print(_T("  No breakpoints.\r\n"));
    }
    else
    {
        while (*pbps != 0177777)
        {
            ConsoleView_PrintFormat(_T("  {:06o}\r\n"), *pbps);
            pbps++;
        }
    }
}
void ConsoleView_CmdRemoveAllBreakpoints(const ConsoleCommandParams& /*params*/)
{
    Emulator_RemoveAllBreakpoints(m_okCurrentProc);

    DebugView_Redraw();
    DisasmView_Redraw();
}
void ConsoleView_CmdSetBreakpointAtAddress(const ConsoleCommandParams& params)
{
    uint16_t address = params.paramOct1;

    bool result = m_okCurrentProc ? Emulator_AddCPUBreakpoint(address) : Emulator_AddPPUBreakpoint(address);
    if (!result)
        ConsoleView_Print(_T("  Failed to add breakpoint.\r\n"));

    DebugView_Redraw();
    DisasmView_Redraw();
}
void ConsoleView_CmdRemoveBreakpointAtAddress(const ConsoleCommandParams& params)
{
    uint16_t address = params.paramOct1;

    bool result = m_okCurrentProc ? Emulator_RemoveCPUBreakpoint(address) : Emulator_RemovePPUBreakpoint(address);
    if (!result)
        ConsoleView_Print(_T("  Failed to remove breakpoint.\r\n"));
    DebugView_Redraw();
    DisasmView_Redraw();
}

void ConsoleView_CmdSaveDisplayListDump(const ConsoleCommandParams& /*params*/)
{
    ConsoleView_SaveDisplayListDump();
}

void ConsoleView_CmdSetMemoryAtAddress(const ConsoleCommandParams& params)
{
    uint16_t address = params.paramOct1;
    uint16_t value = params.paramOct2;

    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    CMemoryController* pMemCtl = pProc->GetMemoryController();
    bool okHaltMode = pProc->IsHaltMode();

    const int addrtype = pMemCtl->GetWordView(address, okHaltMode, false).addressType;
    if (addrtype != ADDRTYPE_RAM12 && addrtype != ADDRTYPE_RAM0 && addrtype != ADDRTYPE_RAM1 && addrtype != ADDRTYPE_RAM2)
    {
        ConsoleView_Print(_T("  Can't change memory value for this memory type.\r\n"));
        return;
    }

    pMemCtl->SetWord(address, okHaltMode, value);

    MainWindow_UpdateAllViews();
}

void ConsoleView_CmdCalculateFloatNumber(const ConsoleCommandParams& params)
{
    unsigned short O1 = params.paramOct1;
    unsigned short O2 = params.paramOct2;

    unsigned long mant = ((((unsigned long)O1 << 16) | O2) & 0x7FFFFFL) | 0x800000L;
    short exp = (O1 >> 7) & 0xFF;
    if (exp == 0)  // zero exponent means real zero (0.0)
    {
        ConsoleView_PrintFormat(_T("exp: 0, mant: 0x{:x}\r\n0\r\n"), mant);
        return;
    }
    exp -= 128;
    ConsoleView_PrintFormat(_T("exp: {}, mant: 0x{:x}\r\n"), exp, mant);
    const auto value = Disasm_FloatToString(O1, O2);
    ConsoleView_PrintFormat(_T("{}\r\n"), value.c_str());
}

#if !defined(PRODUCT)
void ConsoleView_CmdClearTraceLog(const ConsoleCommandParams& /*params*/)
{
    DebugLogClear();
    ConsoleView_Print(_T("  Trace log cleared.\r\n"));
}
void ConsoleView_CmdTraceLogWithMask(const ConsoleCommandParams& params)
{
    ConsoleView_TraceLog(params.paramOct1);
}
void ConsoleView_CmdTraceLogOnOff(const ConsoleCommandParams& /*params*/)
{
    DWORD dwTrace = (g_pBoard->GetTrace() == TRACE_NONE ? TRACE_ALL : TRACE_NONE);
    ConsoleView_TraceLog(dwTrace);
}
#endif


//////////////////////////////////////////////////////////////////////

void ConsoleView_DoConsoleCommand()
{
    // Get command text
    const auto command = ConsoleView_ReadCommand();
    SendMessage(m_hwndConsoleEdit, WM_SETTEXT, 0, (LPARAM)_T(""));  // Clear command
    commandHistory.ResetNavigation();  // Also covers commands put by toolbar and menu

    if (command.empty()) return;  // Nothing to do

    // Echo command to the log
    TCHAR buffer[36];
    ::GetWindowText(m_hwndConsolePrompt, buffer, 14);
    ConsoleView_Print(buffer);
    ConsoleView_PrintFormat(_T(" {}\r\n"), command);

    const auto parsed = console_command::Parse(command.c_str());
    if (!parsed)
    {
        ConsoleView_Print(parsed.error() == console_command::Error::InvalidRegister ? MESSAGE_INVALID_REGNUM :
            parsed.error() == console_command::Error::InvalidValue ? MESSAGE_WRONG_VALUE : MESSAGE_UNKNOWN_COMMAND);
        ConsoleView_PrintConsolePrompt();
        return;
    }
    switch (parsed->action)
    {
    case console_command::Action::ShowHelp: ConsoleView_CmdShowHelp(parsed->params); break;
    case console_command::Action::ClearConsoleLog: ConsoleView_CmdClearConsoleLog(parsed->params); break;
    case console_command::Action::SwitchCpuPpu: ConsoleView_CmdSwitchCpuPpu(parsed->params); break;
    case console_command::Action::SetRegisterValue: ConsoleView_CmdSetRegisterValue(parsed->params); break;
    case console_command::Action::PrintRegister: ConsoleView_CmdPrintRegister(parsed->params); break;
    case console_command::Action::PrintAllRegisters: ConsoleView_CmdPrintAllRegisters(parsed->params); break;
    case console_command::Action::SetRegisterPSW: ConsoleView_CmdSetRegisterPSW(parsed->params); break;
    case console_command::Action::PrintRegisterPSW: ConsoleView_CmdPrintRegisterPSW(parsed->params); break;
    case console_command::Action::StepInto: ConsoleView_CmdStepInto(parsed->params); break;
    case console_command::Action::StepOver: ConsoleView_CmdStepOver(parsed->params); break;
    case console_command::Action::PrintDisassembleAtAddress: ConsoleView_CmdPrintDisassembleAtAddress(parsed->params); break;
    case console_command::Action::PrintDisassembleAtPC: ConsoleView_CmdPrintDisassembleAtPC(parsed->params); break;
    case console_command::Action::SaveMemoryFragment: ConsoleView_CmdSaveMemoryFragment(parsed->params); break;
    case console_command::Action::SaveMemoryDump: ConsoleView_CmdSaveMemoryDump(parsed->params); break;
    case console_command::Action::SaveDisplayListDump: ConsoleView_CmdSaveDisplayListDump(parsed->params); break;
    case console_command::Action::SetMemoryAtAddress: ConsoleView_CmdSetMemoryAtAddress(parsed->params); break;
    case console_command::Action::PrintMemoryDumpAtAddress: ConsoleView_CmdPrintMemoryDumpAtAddress(parsed->params); break;
    case console_command::Action::PrintMemoryDumpAtRegister: ConsoleView_CmdPrintMemoryDumpAtRegister(parsed->params); break;
    case console_command::Action::PrintMemoryDumpAtPC: ConsoleView_CmdPrintMemoryDumpAtPC(parsed->params); break;
    case console_command::Action::RunToAddress: ConsoleView_CmdRunToAddress(parsed->params); break;
    case console_command::Action::Run: ConsoleView_CmdRun(parsed->params); break;
    case console_command::Action::SetBreakpointAtAddress: ConsoleView_CmdSetBreakpointAtAddress(parsed->params); break;
    case console_command::Action::PrintAllBreakpoints: ConsoleView_CmdPrintAllBreakpoints(parsed->params); break;
    case console_command::Action::RemoveBreakpointAtAddress: ConsoleView_CmdRemoveBreakpointAtAddress(parsed->params); break;
    case console_command::Action::RemoveAllBreakpoints: ConsoleView_CmdRemoveAllBreakpoints(parsed->params); break;
    case console_command::Action::CalculateFloatNumber: ConsoleView_CmdCalculateFloatNumber(parsed->params); break;
#if !defined(PRODUCT)
    case console_command::Action::TraceLogWithMask: ConsoleView_CmdTraceLogWithMask(parsed->params); break;
    case console_command::Action::TraceLogOnOff: ConsoleView_CmdTraceLogOnOff(parsed->params); break;
    case console_command::Action::ClearTraceLog: ConsoleView_CmdClearTraceLog(parsed->params); break;
#endif
    default: break;
    }

    ConsoleView_PrintConsolePrompt();
}


//////////////////////////////////////////////////////////////////////
