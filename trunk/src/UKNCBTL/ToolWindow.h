// ToolWindow.h

#pragma once


//////////////////////////////////////////////////////////////////////


const LPCTSTR CLASSNAME_TOOLWINDOW = _T("UKNCBTLTOOLWINDOW");

void ToolWindow_RegisterClass();
LRESULT CALLBACK ToolWindowWndProc(HWND, UINT, WPARAM, LPARAM);


//////////////////////////////////////////////////////////////////////

