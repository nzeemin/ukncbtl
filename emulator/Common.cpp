/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Common.cpp

#include "stdafx.h"
#include <algorithm>
#include <filesystem>
#include "Main.h"
#include "Views.h"

//////////////////////////////////////////////////////////////////////


BOOL AssertFailedLine(LPCSTR lpszFileName, int nLine)
{
    const auto message = std::format(
            _T("ASSERTION FAILED\n\nFile: {}\nLine: {}\n\n")
            _T("Press Abort to stop the program, Retry to break to the debugger, or Ignore to continue execution."),
            std::filesystem::path(lpszFileName).wstring(), nLine);
    int result = MessageBox(nullptr, message.c_str(), _T("ASSERT"), MB_ICONSTOP | MB_ABORTRETRYIGNORE);

    switch (result)
    {
    case IDRETRY:
        return TRUE;
    case IDIGNORE:
        return FALSE;
    case IDABORT:
        PostQuitMessage(255);
    }
    return FALSE;
}

void AlertInfo(LPCTSTR sMessage)
{
    ::MessageBox(nullptr, sMessage, g_szTitle, MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
}
void AlertWarning(LPCTSTR sMessage)
{
    ::MessageBox(nullptr, sMessage, g_szTitle, MB_OK | MB_ICONEXCLAMATION | MB_TOPMOST);
}
BOOL AlertOkCancel(LPCTSTR sMessage)
{
    int result = ::MessageBox(nullptr, sMessage, g_szTitle, MB_OKCANCEL | MB_ICONQUESTION | MB_TOPMOST);
    return (result == IDOK);
}


//////////////////////////////////////////////////////////////////////
// DebugPrint and DebugLog

#if defined(PRODUCT)

void DebugPrint(LPCTSTR) {}
void DebugLogClear() {}
void DebugLogCloseFile() {}
void DebugLog(LPCTSTR) {}

#else

void DebugPrint(LPCTSTR message)
{
    if (g_hwndConsole == nullptr)
        return;

    ConsoleView_Print(message);
}


const LPCTSTR TRACELOG_FILE_NAME = _T("trace.log");

HANDLE Common_LogFile = nullptr;

void DebugLogCreateFile()
{
    if (Common_LogFile == nullptr)
    {
        Common_LogFile = ::CreateFile(TRACELOG_FILE_NAME,
                GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (Common_LogFile == INVALID_HANDLE_VALUE)
            Common_LogFile = nullptr;
    }
}

void DebugLogCloseFile()
{
    if (Common_LogFile == nullptr)
        return;

    ::CloseHandle(Common_LogFile);
    Common_LogFile = nullptr;
}

void DebugLogClear()
{
    DebugLogCreateFile();

    if (Common_LogFile != nullptr)
    {
        // Truncate to zero length
        ::SetFilePointer(Common_LogFile, 0, 0, 0);
        ::SetEndOfFile(Common_LogFile);
    }
}

void DebugLog(LPCTSTR message)
{
    DebugLogCreateFile();
    if (!Common_LogFile || !message) return;

    // Size the conversion in characters, including the terminator. Formatting
    // has no fixed buffer limit, so the output sink must not truncate it either.
    const int length = WideCharToMultiByte(CP_ACP, 0, message, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) return;
    std::string text(static_cast<size_t>(length), '\0');
    if (!WideCharToMultiByte(CP_ACP, 0, message, -1, text.data(), length, nullptr, nullptr)) return;

    SetFilePointer(Common_LogFile, 0, nullptr, FILE_END);
    DWORD written = 0;
    WriteFile(Common_LogFile, text.data(), static_cast<DWORD>(length - 1), &written, nullptr);
}


#endif // !defined(PRODUCT)


//////////////////////////////////////////////////////////////////////


// Processor register names
const TCHAR* REGISTER_NAME[] = { _T("R0"), _T("R1"), _T("R2"), _T("R3"), _T("R4"), _T("R5"), _T("SP"), _T("PC") };


HFONT CreateMonospacedFont()
{
    HFONT font;
    LOGFONT logfont;  memset(&logfont, 0, sizeof(logfont));
    logfont.lfHeight = 12;
    logfont.lfWeight = FW_NORMAL;
    logfont.lfCharSet = DEFAULT_CHARSET;
    logfont.lfOutPrecision = OUT_DEFAULT_PRECIS;
    logfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    logfont.lfQuality = DEFAULT_QUALITY;
    logfont.lfPitchAndFamily = FIXED_PITCH;

    Settings_GetDebugFontName(logfont.lfFaceName);
    font = CreateFontIndirect(&logfont);
    if (font != nullptr)
        return font;

    _tcscpy_s(logfont.lfFaceName, 32, _T("Lucida Console"));
    font = CreateFontIndirect(&logfont);
    if (font != nullptr)
        return font;

    _tcscpy_s(logfont.lfFaceName, 32, _T("Courier"));
    font = CreateFontIndirect(&logfont);
    if (font != nullptr)
        return font;

    return nullptr;
}

HFONT CreateDialogFont()
{
    HFONT font = CreateFont(
            14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            VARIABLE_PITCH,
            _T("MS Shell Dlg 2"));

    return font;
}

void GetFontWidthAndHeight(HDC hdc, int* pWidth, int* pHeight)
{
    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);
    if (pWidth != nullptr)
        *pWidth = tm.tmAveCharWidth;
    if (pHeight != nullptr)
        *pHeight = tm.tmHeight;
}

void DrawOctalValue(HDC hdc, int x, int y, WORD value)
{
    TCHAR buffer[7];
    PrintOctalValue(buffer, value);
    TextOut(hdc, x, y, buffer, (int)_tcsnlen(buffer, 6));
}
void DrawHexValue(HDC hdc, int x, int y, WORD value)
{
    TCHAR buffer[7];
    PrintHexValue(buffer, value);
    TextOut(hdc, x, y, buffer, (int)_tcsnlen(buffer, 6));
}
void DrawBinaryValue(HDC hdc, int x, int y, WORD value)
{
    TCHAR buffer[17];
    PrintBinaryValue(buffer, value);
    TextOut(hdc, x, y, buffer, 16);
}

bool SetGlobalClipboardData(UINT format, HGLOBAL memory)
{
    if (!memory) return false;
    if (!::OpenClipboard(g_hwnd))
    {
        ::GlobalFree(memory);
        return false;
    }
    const bool transferred = ::EmptyClipboard() && ::SetClipboardData(format, memory) != nullptr;
    ::CloseClipboard();
    if (!transferred) ::GlobalFree(memory);
    return transferred;
}

void CopyTextToClipboard(std::basic_string_view<TCHAR> text)
{
    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(TCHAR));
    if (memory == nullptr)
    {
        AlertWarning(_T("Failed to copy text to the clipboard."));
        return;
    }
    auto* destination = static_cast<TCHAR*>(::GlobalLock(memory));
    if (destination == nullptr)
    {
        ::GlobalFree(memory);
        AlertWarning(_T("Failed to copy text to the clipboard."));
        return;
    }
    std::ranges::copy(text, destination);
    destination[text.size()] = 0;
    ::GlobalUnlock(memory);
    if (!SetGlobalClipboardData(CF_UNICODETEXT, memory))
        AlertWarning(_T("Failed to copy text to the clipboard."));
}
void CopyOctalValueToClipboard(WORD value)
{
    TCHAR buffer[7];
    PrintOctalValue(buffer, value);
    CopyTextToClipboard(buffer);
}
void CopyHexValueToClipboard(WORD value)
{
    TCHAR buffer[5];
    PrintHexValue(buffer, value);
    CopyTextToClipboard(buffer);
}

void DrawCharKOI8R(HDC hdc, int x, int y, uint8_t ch)
{
    TCHAR wch;
    if (ch < 32)
        wch = _T('·');
    else
        wch = Translate_KOI8R(ch);

    TextOut(hdc, x, y, &wch, 1);
}


//////////////////////////////////////////////////////////////////////
