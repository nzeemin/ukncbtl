// Common.h

#pragma once


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


void AlertWarning(LPCTSTR sMessage);


//////////////////////////////////////////////////////////////////////
// DebugPrint

#if !defined(PRODUCT)

void DebugPrint(LPCTSTR message);
void DebugPrintFormat(LPCTSTR pszFormat, ...);
void DebugLog(LPCTSTR message);
void DebugLogFormat(LPCTSTR pszFormat, ...);

#endif // !defined(PRODUCT)


//////////////////////////////////////////////////////////////////////


// Processor register names
const TCHAR* REGISTER_NAME[];

const int UKNC_SCREEN_WIDTH = 640;
const int UKNC_SCREEN_HEIGHT = 288;


HFONT CreateMonospacedFont();
HFONT CreateDialogFont();
void GetFontWidthAndHeight(HDC hdc, int* pWidth, int* pHeight);
void PrintOctalValue(TCHAR* buffer, WORD value);
void PrintBinaryValue(TCHAR* buffer, WORD value);
BOOL ParseOctalValue(LPCTSTR text, WORD* pValue);
void DrawOctalValue(HDC hdc, int x, int y, WORD value);
void DrawBinaryValue(HDC hdc, int x, int y, WORD value);
BOOL ParseOctalValue(LPCTSTR text, WORD* pValue);
TCHAR Translate_KOI8R(BYTE ch);
void DrawCharKOI8R(HDC hdc, int x, int y, BYTE ch);

LPCTSTR GetFileNameFromFilePath(LPCTSTR lpfilepath);


//////////////////////////////////////////////////////////////////////
