/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// ToolWindow.h

#pragma once

//////////////////////////////////////////////////////////////////////



inline constexpr LPCTSTR CLASSNAME_TOOLWINDOW = _T("UKNCBTLTOOLWINDOW");
inline constexpr LPCTSTR CLASSNAME_OVERLAPPEDWINDOW = _T("UKNCBTLOVERLAPPEDWINDOW");
inline constexpr LPCTSTR CLASSNAME_SPLITTERWINDOW = _T("UKNCBTLSPLITTERWINDOW");

const int TOOLWINDOW_CAPTION_HEIGHT = 16;

void ToolWindow_RegisterClass();
LRESULT CALLBACK ToolWindow_WndProc(HWND, UINT, WPARAM, LPARAM);

void OverlappedWindow_RegisterClass();

void SplitterWindow_RegisterClass();
LRESULT CALLBACK SplitterWindow_WndProc(HWND, UINT, WPARAM, LPARAM);
HWND SplitterWindow_Create(HWND hwndParent, HWND hwndTop, HWND hwndBottom);


//////////////////////////////////////////////////////////////////////
