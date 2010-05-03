// ConsoleView.cpp

#include "stdafx.h"
#include "UKNCBTL.h"
#include "Views.h"
#include "ToolWindow.h"
#include "Emulator.h"
#include "emubase\Emubase.h"


//////////////////////////////////////////////////////////////////////


HWND g_hwndConsole = (HWND) INVALID_HANDLE_VALUE;  // Console View window handle
WNDPROC m_wndprocConsoleToolWindow = NULL;  // Old window proc address of the ToolWindow

HWND m_hwndConsoleLog = (HWND) INVALID_HANDLE_VALUE;  // Console log window - read-only edit control
HWND m_hwndConsoleEdit = (HWND) INVALID_HANDLE_VALUE;  // Console prompt - edit control
HFONT m_hfontConsole = NULL;
WNDPROC m_wndprocConsoleEdit = NULL;  // Old window proc address of the console prompt
BOOL m_okCurrentProc = FALSE;  // Current processor: TRUE - CPU, FALSE - PPU

CProcessor* ConsoleView_GetCurrentProcessor();
void ClearConsole();
void PrintConsolePrompt();
void PrintRegister(LPCTSTR strName, WORD value);
void PrintMemoryDump(CProcessor* pProc, WORD address, int lines);
void SaveMemoryDump(CProcessor* pProc);
void DoConsoleCommand();
void ConsoleView_AdjustWindowLayout();
LRESULT CALLBACK ConsoleEditWndProc(HWND, UINT, WPARAM, LPARAM);
void ConsoleView_ShowHelp();


const LPCTSTR MESSAGE_UNKNOWN_COMMAND = _T("  Unknown command.\r\n");
const LPCTSTR MESSAGE_WRONG_VALUE = _T("  Wrong value.\r\n");


//////////////////////////////////////////////////////////////////////


void ConsoleView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= ConsoleViewWndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= g_hInst;
    wcex.hIcon			= NULL;
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE+1);
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= CLASSNAME_CONSOLEVIEW;
    wcex.hIconSm		= NULL;

    RegisterClassEx(&wcex);
}

// Create Console View as child of Main Window
void CreateConsoleView(HWND hwndParent, int x, int y, int width, int height)
{
    ASSERT(hwndParent != NULL);

    g_hwndConsole = CreateWindow(
            CLASSNAME_TOOLWINDOW, NULL,
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            x, y, width, height,
            hwndParent, NULL, g_hInst, NULL);
	SetWindowText(g_hwndConsole, _T("Debug Console"));

    // ToolWindow subclassing
    m_wndprocConsoleToolWindow = (WNDPROC) LongToPtr( SetWindowLongPtr(
            g_hwndConsole, GWLP_WNDPROC, PtrToLong(ConsoleViewWndProc)) );

    RECT rcConsole;  GetClientRect(g_hwndConsole, &rcConsole);

    m_hwndConsoleEdit = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            _T("EDIT"), NULL,
            WS_CHILD | WS_VISIBLE,
            0, rcConsole.bottom - 20,
            rcConsole.right, 20,
            g_hwndConsole, NULL, g_hInst, NULL);
    m_hwndConsoleLog = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            _T("EDIT"), NULL,
            WS_CHILD | WS_VSCROLL | WS_VISIBLE | ES_READONLY | ES_MULTILINE,
            0, 0,
            rcConsole.right, rcConsole.bottom - 24,
            g_hwndConsole, NULL, g_hInst, NULL);

    m_hfontConsole = CreateMonospacedFont();
    SendMessage(m_hwndConsoleEdit, WM_SETFONT, (WPARAM) m_hfontConsole, 0);
    SendMessage(m_hwndConsoleLog, WM_SETFONT, (WPARAM) m_hfontConsole, 0);

    // Edit box subclassing
    m_wndprocConsoleEdit = (WNDPROC) LongToPtr( SetWindowLongPtr(
            m_hwndConsoleEdit, GWLP_WNDPROC, PtrToLong(ConsoleEditWndProc)) );

    ShowWindow(g_hwndConsole, SW_SHOW);
    UpdateWindow(g_hwndConsole);

    ConsoleView_Print(_T("Use 'h' command to show help.\r\n\r\n"));
    PrintConsolePrompt();
    SetFocus(m_hwndConsoleEdit);
}

// Adjust position of client windows
void ConsoleView_AdjustWindowLayout()
{
    RECT rc;  GetClientRect(g_hwndConsole, &rc);

    if (m_hwndConsoleEdit != (HWND) INVALID_HANDLE_VALUE)
        SetWindowPos(m_hwndConsoleEdit, NULL, 0, rc.bottom - 20, rc.right, 20, SWP_NOZORDER);
    if (m_hwndConsoleLog != (HWND) INVALID_HANDLE_VALUE)
        SetWindowPos(m_hwndConsoleLog, NULL, 0, 0, rc.right, rc.bottom - 24, SWP_NOZORDER);
}

LRESULT CALLBACK ConsoleViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
	LRESULT lResult;
    switch (message)
    {
    case WM_DESTROY:
        g_hwndConsole = (HWND) INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
        return CallWindowProc(m_wndprocConsoleToolWindow, hWnd, message, wParam, lParam);
    case WM_CTLCOLORSTATIC:
        if (((HWND)lParam) != m_hwndConsoleLog)
            return CallWindowProc(m_wndprocConsoleToolWindow, hWnd, message, wParam, lParam);
        SetBkColor((HDC)wParam, ::GetSysColor(COLOR_WINDOW));
        return (LRESULT) ::GetSysColorBrush(COLOR_WINDOW);
    case WM_SIZE:
        lResult = CallWindowProc(m_wndprocConsoleToolWindow, hWnd, message, wParam, lParam);
        ConsoleView_AdjustWindowLayout();
		return lResult;
    default:
        return CallWindowProc(m_wndprocConsoleToolWindow, hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

LRESULT CALLBACK ConsoleEditWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CHAR:
        if (wParam == 13)
        {
            DoConsoleCommand();
            return 0;
        }
        else
            return CallWindowProc(m_wndprocConsoleEdit, hWnd, message, wParam, lParam);
    default:
        return CallWindowProc(m_wndprocConsoleEdit, hWnd, message, wParam, lParam);
    }
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
    if (m_hwndConsoleLog == NULL) return;

    // Put selection to the end of text
    SendMessage(m_hwndConsoleLog, EM_SETSEL, 0x100000, 0x100000);
    // Insert the message
    SendMessage(m_hwndConsoleLog, EM_REPLACESEL, (WPARAM) FALSE, (LPARAM) message);
    // Scroll to caret
    SendMessage(m_hwndConsoleLog, EM_SCROLLCARET, 0, 0);
}
void ClearConsole()
{
    if (m_hwndConsoleLog == NULL) return;

    SendMessage(m_hwndConsoleLog, WM_SETTEXT, 0, (LPARAM) _T(""));
}

void PrintConsolePrompt()
{
    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    TCHAR bufferAddr[7];
    PrintOctalValue(bufferAddr, pProc->GetPC());
    TCHAR buffer[14];
    wsprintf(buffer, _T("%s:%s> "), pProc->GetName(), bufferAddr);
    ConsoleView_Print(buffer);
}

// Print register name, octal value and binary value
void PrintRegister(LPCTSTR strName, WORD value)
{
    TCHAR buffer[31];
    TCHAR* p = buffer;
    *p++ = _T(' ');
    *p++ = _T(' ');
    lstrcpy(p, strName);  p += 2;
    *p++ = _T(' ');
    PrintOctalValue(p, value);  p += 6;
    *p++ = _T(' ');
    PrintBinaryValue(p, value);  p += 16;
    *p++ = _T('\r');
    *p++ = _T('\n');
    *p++ = 0;
    ConsoleView_Print(buffer);
}

void SaveMemoryDump(CProcessor *pProc)
{ //
	CMemoryController* pMemCtl = pProc->GetMemoryController();
	BYTE buf[65536];
	HANDLE file;
	TCHAR fname[256];

	for(int i=0;i<65536;i++)
	{
		buf[i]=pMemCtl->GetByte(i,1);
	}

	wsprintf(fname,_T("memdump%s.bin"),pProc->GetName());
    
        // Create file
        file = CreateFile(fname,
                GENERIC_WRITE, FILE_SHARE_READ, NULL,
                OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
    //SetFilePointer(Common_LogFile, 0, NULL, FILE_END);

    DWORD dwLength = 65536;
    DWORD dwBytesWritten = 0;
    WriteFile(file, buf, dwLength, &dwBytesWritten, NULL);
	CloseHandle(file);

}
// Print memory dump
void PrintMemoryDump(CProcessor* pProc, WORD address, int lines)
{
    address &= ~1;  // Line up to even address

    CMemoryController* pMemCtl = pProc->GetMemoryController();
    BOOL okHaltMode = pProc->IsHaltMode();

    for (int line = 0; line < lines; line++)
    {
        WORD dump[8];
        for (int i = 0; i < 8; i++)
            dump[i] = pMemCtl->GetWord(address + i*2, okHaltMode);

        TCHAR buffer[2+6+2 + 7*8 + 1 + 16 + 1 + 2];
        TCHAR* pBuf = buffer;
        *pBuf = _T(' ');  pBuf++;
        *pBuf = _T(' ');  pBuf++;
        PrintOctalValue(pBuf, address);  pBuf += 6;
        *pBuf = _T(' ');  pBuf++;
        *pBuf = _T(' ');  pBuf++;
        for (int i = 0; i < 8; i++) {
            PrintOctalValue(pBuf, dump[i]);  pBuf += 6;
            *pBuf = _T(' ');  pBuf++;
        }
        *pBuf = _T(' ');  pBuf++;
        for (int i = 0; i < 8; i++) {
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
// Return value: number of words in the last instruction
int PrintDisassemble(CProcessor* pProc, WORD address, BOOL okOneInstr, BOOL okShort)
{
    CMemoryController* pMemCtl = pProc->GetMemoryController();
    BOOL okHaltMode = pProc->IsHaltMode();

    const int nWindowSize = 30;
    WORD memory[nWindowSize + 2];
    BOOL okValid;
    for (int i = 0; i < nWindowSize + 2; i++)
        memory[i] = pMemCtl->GetWordView(address + i*2, okHaltMode, TRUE, &okValid);

    TCHAR bufaddr[7];
    TCHAR bufvalue[7];
    TCHAR buffer[64];

    int lastLength = 0;
    int length = 0;
    for (int index = 0; index < nWindowSize; index++) {  // Рисуем строки
        PrintOctalValue(bufaddr, address);
        WORD value = memory[index];
        PrintOctalValue(bufvalue, value);

        if (length > 0)
        {
            if (!okShort)
            {
                wsprintf(buffer, _T("  %s  %s\r\n"), bufaddr, bufvalue);
                ConsoleView_Print(buffer);
            }
        }
        else
        {
            if (okOneInstr && index > 0)
                break;
            TCHAR instr[8];
            TCHAR args[32];
            length = DisassembleInstruction(memory + index, address, instr, args);
            lastLength = length;
            if (index + length > nWindowSize)
                break;
            if (okShort)
                wsprintf(buffer, _T("  %s  %-7s %s\r\n"), bufaddr, instr, args);
            else
                wsprintf(buffer, _T("  %s  %s  %-7s %s\r\n"), bufaddr, bufvalue, instr, args);
            ConsoleView_Print(buffer);
        }
        length--;
        address += 2;
    }

    return lastLength;
}

void ConsoleView_StepInto()
{
    // Put command to console prompt
    SendMessage(m_hwndConsoleEdit, WM_SETTEXT, 0, (LPARAM) _T("s"));
    // Execute command
    DoConsoleCommand();
}
void ConsoleView_StepOver()
{
    // Put command to console prompt
    SendMessage(m_hwndConsoleEdit, WM_SETTEXT, 0, (LPARAM) _T("so"));
    // Execute command
    DoConsoleCommand();
}

void ConsoleView_ShowHelp()
{
    ConsoleView_Print(_T("Console command list:\r\n")
            _T("  c          Clear console log\r\n")
            _T("  dXXXXXX    Disassemble from address XXXXXX\r\n")
            _T("  g          Go; free run\r\n")
            _T("  gXXXXXX    Go; run processor until breakpoint at address XXXXXX\r\n")
            _T("  m          Memory dump at current address\r\n")
            _T("  mXXXXXX    Memory dump at address XXXXXX\r\n")
            _T("  mrN        Memory dump at address from register N; N=0..7\r\n")
            _T("  p          Switch to other processor\r\n")
            _T("  r          Show register values\r\n") 
            _T("  rN         Show value of register N; N=0..7,ps\r\n") 
            _T("  rN XXXXXX  Set register N to value XXXXXX; N=0..7,ps\r\n") 
            _T("  s          Step Into; executes one instruction\r\n") 
            _T("  so         Step Over; executes and stops after the current instruction\r\n") 
            _T("  u          Save memory dump to file memdumpXPU.bin\r\n")
        );
}

void DoConsoleCommand()
{
    // Get command text
    TCHAR command[32];
    GetWindowText(m_hwndConsoleEdit, command, 32);
    SendMessage(m_hwndConsoleEdit, WM_SETTEXT, 0, (LPARAM) _T(""));  // Clear command

    if (command[0] == 0) return;  // Nothing to do

    // Echo command to the log
    ConsoleView_Print(command);
    ConsoleView_Print(_T("\r\n"));

    BOOL okUpdateAllViews = FALSE;  // Flag - need to update all debug views
    CProcessor* pProc = ConsoleView_GetCurrentProcessor();

    // Execute the command
    switch (command[0])
    {
    case _T('h'):
        ConsoleView_ShowHelp();
        break;
    case _T('c'):  // Clear log
        ClearConsole();
        break;
    case _T('p'):  // Switch CPU/PPU
        m_okCurrentProc = ! m_okCurrentProc;
        DebugView_SetCurrentProc(m_okCurrentProc);   // Switch DebugView to current processor
        DisasmView_SetCurrentProc(m_okCurrentProc);   // Switch DisasmView to current processor
        break;
    case _T('r'):  // Register operations
        if (command[1] == 0)  // Print all registers
        {
            for (int r = 0; r < 8; r++)
            {
                LPCTSTR name = REGISTER_NAME[r];
                WORD value = pProc->GetReg(r);
                PrintRegister(name, value);
            }
        }
        else if (command[1] >= _T('0') && command[1] <= _T('7'))  // "r0".."r7"
        {
            int r = command[1] - _T('0');
            LPCTSTR name = REGISTER_NAME[r];
            if (command[2] == 0)  // "rN" - show register N
            {
                WORD value = pProc->GetReg(r);
                PrintRegister(name, value);
            }
            else if (command[2] == _T('=') || command[2] == _T(' '))  // "rN=XXXXXX" - set register N to value XXXXXX
            {
                WORD value;
                if (! ParseOctalValue(command + 3, &value))
                    ConsoleView_Print(MESSAGE_WRONG_VALUE);
                else
                {
                    pProc->SetReg(r, value);
                    PrintRegister(name, value);
                    okUpdateAllViews = TRUE;
                }
            }
            else
                ConsoleView_Print(MESSAGE_UNKNOWN_COMMAND);
        }
        else if (command[1] == _T('p') && command[2] == _T('s'))  // "rps"
        {
            if (command[3] == 0)  // "rps" - show PSW
            {
                WORD value = pProc->GetPSW();
                PrintRegister(_T("PS"), value);
            }
            else if (command[3] == _T('=') || command[3] == _T(' '))  // "rps=XXXXXX" - set PSW to value XXXXXX
            {
                WORD value;
                if (! ParseOctalValue(command + 4, &value))
                    ConsoleView_Print(MESSAGE_WRONG_VALUE);
                else
                {
                    pProc->SetPSW(value);
                    PrintRegister(_T("PS"), value);
                    okUpdateAllViews = TRUE;
                }
            }
            else
                ConsoleView_Print(MESSAGE_UNKNOWN_COMMAND);
        }
        else
            ConsoleView_Print(MESSAGE_UNKNOWN_COMMAND);
        break;
    case _T('s'):  // Step
        if (command[1] == 0)  // "s" - Step Into, execute one instruction
        {
            PrintDisassemble(pProc, pProc->GetPC(), TRUE, FALSE);
            //pProc->Execute();
    		
		    g_pBoard->DebugTicks();

            okUpdateAllViews = TRUE;
        }
        else if (command[1] == _T('o'))  // "so" - Step Over
        {
            int instrLength = PrintDisassemble(pProc, pProc->GetPC(), TRUE, FALSE);
            WORD bpaddress = pProc->GetPC() + instrLength * 2;

            if (m_okCurrentProc)
                Emulator_SetCPUBreakpoint(bpaddress);
            else
                Emulator_SetPPUBreakpoint(bpaddress);
            Emulator_Start();
        }
        break;
    case _T('d'):  // Disassemble
    case _T('D'):  // Disassemble, short format
        {
            BOOL okShort = (command[0] == _T('D'));
            if (command[1] == 0)  // "d" - disassemble at current address
                PrintDisassemble(pProc, pProc->GetPC(), FALSE, okShort);
            else if (command[1] >= _T('0') && command[1] <= _T('7'))  // "dXXXXXX" - disassemble at address XXXXXX
            {
                WORD value;
                if (! ParseOctalValue(command + 1, &value))
                    ConsoleView_Print(MESSAGE_WRONG_VALUE);
                else
                {
                    PrintDisassemble(pProc, value, FALSE, okShort);
                }
            }
            else
                ConsoleView_Print(MESSAGE_UNKNOWN_COMMAND);
        }
        break;
	case _T('u'):
		SaveMemoryDump(pProc);
		break;
    case _T('m'):
        if (command[1] == 0)  // "m" - dump memory at current address
        {
            PrintMemoryDump(pProc, pProc->GetPC(), 8);
        }
        else if (command[1] >= _T('0') && command[1] <= _T('7'))  // "mXXXXXX" - dump memory at address XXXXXX
        {
            WORD value;
            if (! ParseOctalValue(command + 1, &value))
                ConsoleView_Print(MESSAGE_WRONG_VALUE);
            else
            {
                PrintMemoryDump(pProc, value, 8);
            }
        }
        else if (command[1] == _T('r') &&
                command[2] >= _T('0') && command[2] <= _T('7'))  // "mrN" - dump memory at address from register N
        {
            int r = command[2] - _T('0');
            WORD address = pProc->GetReg(r);
            PrintMemoryDump(pProc, address, 8);
        }
        else
            ConsoleView_Print(MESSAGE_UNKNOWN_COMMAND);
        break;
    //TODO: "mXXXXXX YYYYYY" - set memory cell at XXXXXX to value YYYYYY
    //TODO: "mrN YYYYYY" - set memory cell at address from rN to value YYYYYY
    case _T('g'):
        if (command[1] == 0)
        {
            Emulator_Start();
        }
        else
        {
            WORD value;
            if (! ParseOctalValue(command + 1, &value))
                ConsoleView_Print(MESSAGE_WRONG_VALUE);
            else
            {
                if (m_okCurrentProc)
                {
                    Emulator_SetCPUBreakpoint(value);
                    Emulator_Start();
                }
                else
                {
                    Emulator_SetPPUBreakpoint(value);
                    Emulator_Start();
                }
            }
        }
        break;
    default:
        ConsoleView_Print(MESSAGE_UNKNOWN_COMMAND);
        break;
    }

    PrintConsolePrompt();

    if (okUpdateAllViews) {
        MainWindow_UpdateAllViews();
    }
}


//////////////////////////////////////////////////////////////////////
