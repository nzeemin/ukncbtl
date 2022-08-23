/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Common.h

#pragma once

//////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#define APP_VERSION_STRING "DEBUG"
#define APP_REVISION 0
#elif !defined(PRODUCT)
#define APP_VERSION_STRING "RELEASE"
#define APP_REVISION 0
#else
#include "Version.h"
#endif

//////////////////////////////////////////////////////////////////////
// Assertions checking - MFC-like ASSERT macro

#ifdef _DEBUG

BOOL AssertFailedLine(LPCSTR lpszFileName, int nLine);
#define ASSERT(f)          (void) ((f) || !AssertFailedLine(__FILE__, __LINE__) || (DebugBreak(), 0))
#define VERIFY(f)          ASSERT(f)

#else   // _DEBUG

#define ASSERT(f)          ((void)0)
#define VERIFY(f)          ((void)f)

#endif // !_DEBUG


//////////////////////////////////////////////////////////////////////
// Alerts

void AlertInfo(LPCTSTR sMessage);
void AlertWarning(LPCTSTR sMessage);
void AlertWarningFormat(LPCTSTR sFormat, ...);
BOOL AlertOkCancel(LPCTSTR sMessage);


//////////////////////////////////////////////////////////////////////
// DebugPrint

void DebugPrint(LPCTSTR message);
void DebugPrintFormat(LPCTSTR pszFormat, ...);
void DebugLogClear();
void DebugLogCloseFile();
void DebugLog(LPCTSTR message);
void DebugLogFormat(LPCTSTR pszFormat, ...);


//////////////////////////////////////////////////////////////////////


// Processor register names
extern const TCHAR* REGISTER_NAME[8];

const int UKNC_SCREEN_WIDTH = 640;
const int UKNC_SCREEN_HEIGHT = 288;


HFONT CreateMonospacedFont();
HFONT CreateDialogFont();
void GetFontWidthAndHeight(HDC hdc, int* pWidth, int* pHeight);
void PrintOctalValue(TCHAR* buffer, WORD value);
void PrintHexValue(TCHAR* buffer, WORD value);
void PrintBinaryValue(TCHAR* buffer, WORD value);
BOOL ParseOctalValue(LPCTSTR text, WORD* pValue);
void DrawOctalValue(HDC hdc, int x, int y, WORD value);
void DrawHexValue(HDC hdc, int x, int y, WORD value);
void DrawBinaryValue(HDC hdc, int x, int y, WORD value);
BOOL ParseOctalValue(LPCTSTR text, WORD* pValue);
TCHAR Translate_KOI8R(BYTE ch);
void DrawCharKOI8R(HDC hdc, int x, int y, BYTE ch);

LPCTSTR GetFileNameFromFilePath(LPCTSTR lpfilepath);


//////////////////////////////////////////////////////////////////////
