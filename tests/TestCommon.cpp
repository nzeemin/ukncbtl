/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// TestCommon.cpp : test-side replacements for the UI-coupled functions of Common.cpp.
// Common.cpp depends on the main window and message boxes, so it can not be linked here.

#include "stdafx.h"
#include <CppUnitTest.h>

// Processor register names
const TCHAR* REGISTER_NAME[] = { _T("R0"), _T("R1"), _T("R2"), _T("R3"), _T("R4"), _T("R5"), _T("SP"), _T("PC") };

#ifdef _DEBUG

// An ASSERT hit inside the emulation core fails the current test instead of opening a dialog
BOOL AssertFailedLine(LPCSTR lpszFileName, int nLine)
{
    char buffer[512];
    sprintf_s(buffer, "ASSERT failed at %s(%d)", lpszFileName, nLine);
    wchar_t wbuffer[512];
    MultiByteToWideChar(CP_ACP, 0, buffer, -1, wbuffer, 512);
    Microsoft::VisualStudio::CppUnitTestFramework::Assert::Fail(wbuffer);
    return FALSE;
}

#endif // _DEBUG

void DebugLog(LPCTSTR message)
{
    OutputDebugString(message);
}

void DebugLogFormat(LPCTSTR pszFormat, ...)
{
    TCHAR buffer[512];
    va_list ptr;
    va_start(ptr, pszFormat);
    _vsntprintf_s(buffer, 512, _TRUNCATE, pszFormat, ptr);
    va_end(ptr);
    OutputDebugString(buffer);
}
