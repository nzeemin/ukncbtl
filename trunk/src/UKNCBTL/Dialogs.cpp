/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Dialogs.cpp

#include "stdafx.h"
#include <commdlg.h>
#include "Dialogs.h"
#include "UKNCBTL.h"

//////////////////////////////////////////////////////////////////////


INT_PTR CALLBACK AboutBoxProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK InputBoxProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK CreateDiskProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void Dialogs_DoCreateDisk(int tracks);
BOOL InputBoxValidate(HWND hDlg);

LPCTSTR m_strInputBoxTitle = NULL;
LPCTSTR m_strInputBoxPrompt = NULL;
WORD* m_pInputBoxValueOctal = NULL;


//////////////////////////////////////////////////////////////////////
// About Box

void ShowAboutBox()
{
    DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), g_hwnd, AboutBoxProc);
}

INT_PTR CALLBACK AboutBoxProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        {
            TCHAR buf[64];
            wsprintf(buf, _T("%S %S"), __DATE__, __TIME__);
            ::SetWindowText(::GetDlgItem(hDlg, IDC_BUILDDATE), buf);
            return (INT_PTR)TRUE;
        }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}


//////////////////////////////////////////////////////////////////////


BOOL InputBoxOctal(HWND hwndOwner, LPCTSTR strTitle, LPCTSTR strPrompt, WORD* pValue)
{
    m_strInputBoxTitle = strTitle;
    m_strInputBoxPrompt = strPrompt;
    m_pInputBoxValueOctal = pValue;
    INT_PTR result = DialogBox(g_hInst, MAKEINTRESOURCE(IDD_INPUTBOX), hwndOwner, InputBoxProc);
    if (result != IDOK)
        return FALSE;

    return TRUE;
}


INT_PTR CALLBACK InputBoxProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        {
            SetWindowText(hDlg, m_strInputBoxTitle);
            HWND hStatic = GetDlgItem(hDlg, IDC_STATIC);
            SetWindowText(hStatic, m_strInputBoxPrompt);
            HWND hEdit = GetDlgItem(hDlg, IDC_EDIT1);

            TCHAR buffer[8];
            _sntprintf_s(buffer, 8, _T("%06o"), *m_pInputBoxValueOctal);
            SetWindowText(hEdit, buffer);
            SendMessage(hEdit, EM_SETSEL, 0, -1);

            SetFocus(hEdit);
            return (INT_PTR)FALSE;
        }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            if (! InputBoxValidate(hDlg))
                return (INT_PTR) FALSE;
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        default:
            return (INT_PTR) FALSE;
        }
        break;
    }
    return (INT_PTR) FALSE;
}

BOOL InputBoxValidate(HWND hDlg) {
    HWND hEdit = GetDlgItem(hDlg, IDC_EDIT1);
    TCHAR buffer[8];
    GetWindowText(hEdit, buffer, 8);

    WORD value;
    if (! ParseOctalValue(buffer, &value))
    {
        MessageBox(NULL, _T("Please enter correct octal value."), _T("Input Box Validation"),
                MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
        return FALSE;
    }

    *m_pInputBoxValueOctal = value;

    return TRUE;
}


//////////////////////////////////////////////////////////////////////


BOOL ShowSaveDialog(HWND hwndOwner, LPCTSTR strTitle, LPCTSTR strFilter, TCHAR* bufFileName)
{
    *bufFileName = 0;
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndOwner;
    ofn.hInstance = g_hInst;
    ofn.lpstrTitle = strTitle;
    ofn.lpstrFilter = strFilter;
    ofn.Flags = OFN_FILEMUSTEXIST;
    ofn.lpstrFile = bufFileName;
    ofn.nMaxFile = MAX_PATH;

    BOOL okResult = GetSaveFileName(&ofn);
    return okResult;
}

BOOL ShowOpenDialog(HWND hwndOwner, LPCTSTR strTitle, LPCTSTR strFilter, TCHAR* bufFileName)
{
    *bufFileName = 0;
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.hInstance = g_hInst;
    ofn.lpstrTitle = strTitle;
    ofn.lpstrFilter = strFilter;
    ofn.Flags = OFN_FILEMUSTEXIST;
    ofn.lpstrFile = bufFileName;
    ofn.nMaxFile = MAX_PATH;

    BOOL okResult = GetOpenFileName(&ofn);
    return okResult;
}


//////////////////////////////////////////////////////////////////////
// Create Disk Dialog

void ShowCreateDiskDialog()
{
    DialogBox(g_hInst, MAKEINTRESOURCE(IDD_CREATEDISK), g_hwnd, CreateDiskProc);
}

INT_PTR CALLBACK CreateDiskProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        {
            CheckRadioButton(hDlg, IDC_TRACKS40, IDC_TRACKS80, IDC_TRACKS80);
            return (INT_PTR)FALSE;
        }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            Dialogs_DoCreateDisk(IsDlgButtonChecked(hDlg, IDC_TRACKS40) == BST_CHECKED ? 40 : 80);

            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        default:
            return (INT_PTR)FALSE;
        }
        break;
    }
    return (INT_PTR) FALSE;
}

void Dialogs_DoCreateDisk(int tracks)
{
    TCHAR bufFileName[MAX_PATH];
    BOOL okResult = ShowSaveDialog(g_hwnd,
        _T("Save new disk as"),
        _T("Bitmaps (*.dsk)\0*.dsk\0All Files (*.*)\0*.*\0\0"),
        bufFileName);
    if (! okResult) return;

    // Create zero-filled file
    LONG fileSize = tracks * 10240;
	HANDLE hFile = ::CreateFile(bufFileName,
		GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	//if (hFileNew == INVALID_HANDLE_VALUE)
    ::SetFilePointer(hFile, fileSize, NULL, FILE_BEGIN);
    ::SetEndOfFile(hFile);
    ::CloseHandle(hFile);

    ::MessageBox(g_hwnd, _T("New disk file created successfully.\nPlease initialize the disk using INIT command."),
        _T("UKNCBTL"), MB_OK | MB_ICONINFORMATION);
}


//////////////////////////////////////////////////////////////////////
