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
#include "Main.h"
#include "Views.h"

//////////////////////////////////////////////////////////////////////


BOOL AssertFailedLine(LPCSTR lpszFileName, int nLine)
{
    TCHAR buffer[360];
    wsprintf(buffer,
            _T("ASSERTION FAILED\n\nFile: %S\nLine: %d\n\n")
            _T("Press Abort to stop the program, Retry to break to the debugger, or Ignore to continue execution."),
            lpszFileName, nLine);
    int result = MessageBox(NULL, buffer, _T("ASSERT"), MB_ICONSTOP | MB_ABORTRETRYIGNORE);

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
    ::MessageBox(NULL, sMessage, g_szTitle, MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
}
void AlertWarning(LPCTSTR sMessage)
{
    ::MessageBox(NULL, sMessage, g_szTitle, MB_OK | MB_ICONEXCLAMATION | MB_TOPMOST);
}
void AlertWarningFormat(LPCTSTR sFormat, ...)
{
    TCHAR buffer[512];

    va_list ptr;
    va_start(ptr, sFormat);
    _vsntprintf_s(buffer, 512, 512 - 1, sFormat, ptr);
    va_end(ptr);

    ::MessageBox(NULL, buffer, g_szTitle, MB_OK | MB_ICONEXCLAMATION | MB_TOPMOST);
}
BOOL AlertOkCancel(LPCTSTR sMessage)
{
    int result = ::MessageBox(NULL, sMessage, g_szTitle, MB_OKCANCEL | MB_ICONQUESTION | MB_TOPMOST);
    return (result == IDOK);
}


//////////////////////////////////////////////////////////////////////
// DebugPrint and DebugLog

#if defined(PRODUCT)

void DebugPrint(LPCTSTR) {}
void DebugPrintFormat(LPCTSTR, ...) {}
void DebugLogClear() {}
void DebugLogCloseFile() {}
void DebugLog(LPCTSTR) {}
void DebugLogFormat(LPCTSTR, ...) {}

#else

void DebugPrint(LPCTSTR message)
{
    if (g_hwndConsole == NULL)
        return;

    ConsoleView_Print(message);
}

void DebugPrintFormat(LPCTSTR pszFormat, ...)
{
    TCHAR buffer[512];

    va_list ptr;
    va_start(ptr, pszFormat);
    _vsntprintf_s(buffer, 512, 512 - 1, pszFormat, ptr);
    va_end(ptr);

    DebugPrint(buffer);
}

const LPCTSTR TRACELOG_FILE_NAME = _T("trace.log");
const LPCTSTR TRACELOG_NEWLINE = _T("\r\n");

HANDLE Common_LogFile = NULL;

void DebugLogCreateFile()
{
    if (Common_LogFile == NULL)
    {
        Common_LogFile = ::CreateFile(TRACELOG_FILE_NAME,
                GENERIC_WRITE, FILE_SHARE_READ, NULL,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }
}

void DebugLogCloseFile()
{
    if (Common_LogFile == NULL)
        return;

    ::CloseHandle(Common_LogFile);
    Common_LogFile = NULL;
}

void DebugLogClear()
{
    DebugLogCreateFile();

    if (Common_LogFile != NULL)
    {
        // Trunkate to zero length
        ::SetFilePointer(Common_LogFile, 0, 0, 0);
        ::SetEndOfFile(Common_LogFile);
    }
}

void DebugLog(LPCTSTR message)
{
    DebugLogCreateFile();

    SetFilePointer(Common_LogFile, 0, NULL, FILE_END);

    DWORD dwLength = lstrlen(message) * sizeof(TCHAR);

    char ascii[256];  *ascii = 0;
    WideCharToMultiByte(CP_ACP, 0, message, dwLength, ascii, 256, NULL, NULL);

    DWORD dwBytesWritten = 0;
    //WriteFile(Common_LogFile, message, dwLength, &dwBytesWritten, NULL);
    WriteFile(Common_LogFile, ascii, strlen(ascii), &dwBytesWritten, NULL);

    //dwLength = lstrlen(TRACELOG_NEWLINE) * sizeof(TCHAR);
    //WriteFile(Common_LogFile, TRACELOG_NEWLINE, dwLength, &dwBytesWritten, NULL);
}

void DebugLogFormat(LPCTSTR pszFormat, ...)
{
    TCHAR buffer[512];

    va_list ptr;
    va_start(ptr, pszFormat);
    _vsntprintf_s(buffer, 512, 512 - 1, pszFormat, ptr);
    va_end(ptr);

    DebugLog(buffer);
}

#endif // !defined(PRODUCT)


//////////////////////////////////////////////////////////////////////


// Названия регистров процессора
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
    if (font != NULL)
        return font;

    _tcscpy_s(logfont.lfFaceName, 32, _T("Lucida Console"));
    font = CreateFontIndirect(&logfont);
    if (font != NULL)
        return font;

    _tcscpy_s(logfont.lfFaceName, 32, _T("Courier"));
    font = CreateFontIndirect(&logfont);
    if (font != NULL)
        return font;

    return NULL;
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
    if (pWidth != NULL)
        *pWidth = tm.tmAveCharWidth;
    if (pHeight != NULL)
        *pHeight = tm.tmHeight;
}

// Print octal 16-bit value to buffer
// buffer size at least 7 characters
void PrintOctalValue(TCHAR* buffer, WORD value)
{
    for (int p = 0; p < 6; p++)
    {
        int digit = value & 7;
        buffer[5 - p] = _T('0') + (TCHAR)digit;
        value = (value >> 3);
    }
    buffer[6] = 0;
}
// Print hex 16-bit value to buffer
// buffer size at least 5 characters
void PrintHexValue(TCHAR* buffer, WORD value)
{
    for (int p = 0; p < 4; p++)
    {
        int digit = value & 15;
        buffer[3 - p] = (digit < 10) ? _T('0') + (TCHAR)digit : _T('a') + (TCHAR)(digit - 10);
        value = (value >> 4);
    }
    buffer[4] = 0;
}
// Print binary 16-bit value to buffer
// buffer size at least 17 characters
void PrintBinaryValue(TCHAR* buffer, WORD value)
{
    for (int b = 0; b < 16; b++)
    {
        int bit = (value >> b) & 1;
        buffer[15 - b] = bit ? _T('1') : _T('0');
    }
    buffer[16] = 0;
}

// Parse octal value from text
BOOL ParseOctalValue(LPCTSTR text, WORD* pValue)
{
    WORD value = 0;
    TCHAR* pChar = (TCHAR*) text;
    for (int p = 0; ; p++)
    {
        if (p > 6) return FALSE;
        TCHAR ch = *pChar;  pChar++;
        if (ch == 0) break;
        if (ch < _T('0') || ch > _T('7')) return FALSE;
        value = (value << 3);
        TCHAR digit = ch - _T('0');
        value += digit;
    }
    *pValue = value;
    return TRUE;
}

void DrawOctalValue(HDC hdc, int x, int y, WORD value)
{
    TCHAR buffer[7];
    PrintOctalValue(buffer, value);
    TextOut(hdc, x, y, buffer, (int) _tcslen(buffer));
}
void DrawHexValue(HDC hdc, int x, int y, WORD value)
{
    TCHAR buffer[7];
    PrintHexValue(buffer, value);
    TextOut(hdc, x, y, buffer, (int) _tcslen(buffer));
}
void DrawBinaryValue(HDC hdc, int x, int y, WORD value)
{
    TCHAR buffer[17];
    PrintBinaryValue(buffer, value);
    TextOut(hdc, x, y, buffer, 16);
}

#ifdef _UNICODE
// UKNC KOI8-R (Russian) to Unicode conversion table
const TCHAR KOI8R_CODES[] =
{
    0x2191, 0x2193, 0x2190, 0x2192, 0x25B2, 0x25BC, 0x25C4, 0x25BA, 0x2555, 0x255C, 0x255B, 0x2559, 0x2552, 0x2553, 0x2558, 0x2556,
    0x00D7, 0x00F7, 0x2265, 0x2264, 0x21B2, 0x00B1, 0x2248, 0x2320, 0x2321, 0x00B0, 0x2260, 0x2591, 0x2591, 0x2592, 0x2593, 0x2588,
    0x2562, 0x2534, 0x255F, 0x2510, 0x2551, 0x251C, 0x2514, 0x2560, 0x2554, 0x256A, 0x250C, 0x252C, 0x2569, 0x2568, 0x2524, 0x256C,
    0x253C, 0x255E, 0x256B, 0x2565, 0x2567, 0x2500, 0x255A, 0x2502, 0x2564, 0x2518, 0x2550, 0x2563, 0x255D, 0x2566, 0x2561, 0x2557,
    _T('ю'), _T('а'), _T('б'), _T('ц'), _T('д'), _T('е'), _T('ф'), _T('г'), _T('х'), _T('и'), _T('й'), _T('к'), _T('л'), _T('м'), _T('н'), _T('о'),
    _T('п'), _T('я'), _T('р'), _T('с'), _T('т'), _T('у'), _T('ж'), _T('в'), _T('ь'), _T('ы'), _T('з'), _T('ш'), _T('э'), _T('щ'), _T('ч'), _T('ъ'),
    _T('Ю'), _T('А'), _T('Б'), _T('Ц'), _T('Д'), _T('Е'), _T('Ф'), _T('Г'), _T('Х'), _T('И'), _T('Й'), _T('К'), _T('Л'), _T('М'), _T('Н'), _T('О'),
    _T('П'), _T('Я'), _T('Р'), _T('С'), _T('Т'), _T('У'), _T('Ж'), _T('В'), _T('Ь'), _T('Ы'), _T('З'), _T('Ш'), _T('Э'), _T('Щ'), _T('Ч'), _T('Ъ'),
};
#else
// UKNC KOI8-R (Russian) to Windows-1251 conversion table
const TCHAR KOI8R_CODES[] =
{
    _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'),
    _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T(' '), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'),
    _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'),
    _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'), _T('·'),
    _T('ю'), _T('а'), _T('б'), _T('ц'), _T('д'), _T('е'), _T('ф'), _T('г'), _T('х'), _T('и'), _T('й'), _T('к'), _T('л'), _T('м'), _T('н'), _T('о'),
    _T('п'), _T('я'), _T('р'), _T('с'), _T('т'), _T('у'), _T('ж'), _T('в'), _T('ь'), _T('ы'), _T('з'), _T('ш'), _T('э'), _T('щ'), _T('ч'), _T('ъ'),
    _T('Ю'), _T('А'), _T('Б'), _T('Ц'), _T('Д'), _T('Е'), _T('Ф'), _T('Г'), _T('Х'), _T('И'), _T('Й'), _T('К'), _T('Л'), _T('М'), _T('Н'), _T('О'),
    _T('П'), _T('Я'), _T('Р'), _T('С'), _T('Т'), _T('У'), _T('Ж'), _T('В'), _T('Ь'), _T('Ы'), _T('З'), _T('Ш'), _T('Э'), _T('Щ'), _T('Ч'), _T('Ъ'),
};
#endif
// Translate one KOI8-R character to Unicode character
TCHAR Translate_KOI8R(uint8_t ch)
{
    if (ch < 128) return (TCHAR) ch;
    return KOI8R_CODES[ch - 128];
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
// Path funcations

LPCTSTR GetFileNameFromFilePath(LPCTSTR lpfilepath)
{
    LPCTSTR lpfilename = _tcsrchr(lpfilepath, _T('\\'));
    if (lpfilename == NULL)
        return lpfilepath;
    else
        return lpfilename + 1;
}


//////////////////////////////////////////////////////////////////////
