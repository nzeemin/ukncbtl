﻿/*  This file is part of UKNCBTL.
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
#include <commctrl.h>
#include <shellapi.h>
#include "Dialogs.h"
#include "Main.h"
#include "Views.h"
#include "Emulator.h"

//////////////////////////////////////////////////////////////////////


INT_PTR CALLBACK AboutBoxProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK InputBoxProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK CreateDiskProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK SettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK SettingsColorsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK SettingsOsdProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK DcbEditorProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

void Dialogs_DoCreateDisk(int tracks);
BOOL InputBoxValidate(HWND hDlg);

LPCTSTR m_strInputBoxTitle = NULL;
WORD* m_pInputBoxValueOctal = NULL;

// Show the standard Choose Color dialog box
BOOL ShowColorDialog(COLORREF& color);

COLORREF m_DialogSettings_acrCustClr[16];  // array of custom colors to use in ChooseColor()
COLORREF m_DialogSettings_OsdLineColor = RGB(120, 0, 0);

DCB m_DialogSettings_SerialConfig;
DCB m_DialogSettings_NetComConfig;
DCB* m_pDcbEditorData = NULL;


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
            const size_t buffersize = 64;
            TCHAR buffer[buffersize];
            _sntprintf(buffer, buffersize - 1, _T("UKNCBTL version %s revision %d"), _T(APP_VERSION_STRING), APP_REVISION);
            ::SetDlgItemText(hDlg, IDC_VERSION, buffer);
            _sntprintf(buffer, buffersize - 1, _T("Build date: %S %S"), __DATE__, __TIME__);
            ::SetDlgItemText(hDlg, IDC_BUILDDATE, buffer);
            return (INT_PTR)TRUE;
        }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    case WM_NOTIFY:
        if (wParam == IDC_SYSLINK1 &&
            ((LPNMHDR)lParam)->code == NM_CLICK || ((LPNMHDR)lParam)->code == NM_RETURN)
        {
            ::ShellExecute(NULL, _T("open"), _T("https://github.com/nzeemin/ukncbtl"), NULL, NULL, SW_SHOW);
            break;
        }
        break;
    }
    return (INT_PTR)FALSE;
}


//////////////////////////////////////////////////////////////////////


BOOL InputBoxOctal(HWND hwndOwner, LPCTSTR strTitle, WORD* pValue)
{
    m_strInputBoxTitle = strTitle;
    m_pInputBoxValueOctal = pValue;
    INT_PTR result = DialogBox(g_hInst, MAKEINTRESOURCE(IDD_INPUTBOX), hwndOwner, InputBoxProc);
    if (result != IDOK)
        return FALSE;

    return TRUE;
}

INT_PTR CALLBACK InputBoxProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM /*lParam*/)
{
    switch (message)
    {
    case WM_INITDIALOG:
        {
            SetWindowText(hDlg, m_strInputBoxTitle);
            HWND hEdit = GetDlgItem(hDlg, IDC_EDIT1);

            TCHAR buffer[8];
            _sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR) - 1, _T("%06ho"), *m_pInputBoxValueOctal);
            SetWindowText(hEdit, buffer);
            SendMessage(hEdit, EM_SETSEL, 0, -1);

            SetFocus(hEdit);
            return (INT_PTR)FALSE;
        }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_EDIT1:
            {
                const size_t buffersize = 8;
                TCHAR buffer[buffersize];
                GetDlgItemText(hDlg, IDC_EDIT1, buffer, buffersize);
                if (_sntscanf_s(buffer, buffersize, _T("%ho"), m_pInputBoxValueOctal) > 0)
                {
                    GetDlgItemText(hDlg, IDC_EDIT2, buffer, buffersize);
                    WORD otherValue;
                    if (_sntscanf_s(buffer, buffersize, _T("%hx"), &otherValue) <= 0 || *m_pInputBoxValueOctal != otherValue)
                    {
                        _sntprintf(buffer, buffersize - 1, _T("%04hx"), *m_pInputBoxValueOctal);
                        SetDlgItemText(hDlg, IDC_EDIT2, buffer);
                    }
                }
            }
            return (INT_PTR)TRUE;
        case IDC_EDIT2:
            {
                const size_t buffersize = 8;
                TCHAR buffer[buffersize];
                GetDlgItemText(hDlg, IDC_EDIT2, buffer, buffersize);
                if (_sntscanf_s(buffer, buffersize, _T("%hx"), m_pInputBoxValueOctal) > 0)
                {
                    GetDlgItemText(hDlg, IDC_EDIT1, buffer, buffersize);
                    WORD otherValue;
                    if (_sntscanf_s(buffer, buffersize, _T("%ho"), &otherValue) <= 0 || *m_pInputBoxValueOctal != otherValue)
                    {
                        _sntprintf(buffer, buffersize - 1, _T("%06ho"), *m_pInputBoxValueOctal);
                        SetDlgItemText(hDlg, IDC_EDIT1, buffer);
                    }
                }
            }
            return (INT_PTR)TRUE;
        case IDOK:
            if (! InputBoxValidate(hDlg))
                return (INT_PTR)FALSE;
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
    return (INT_PTR)FALSE;
}

BOOL InputBoxValidate(HWND hDlg)
{
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


BOOL ShowSaveDialog(HWND hwndOwner, LPCTSTR strTitle, LPCTSTR strFilter, LPCTSTR strDefExt, TCHAR* bufFileName)
{
    *bufFileName = 0;
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndOwner;
    ofn.hInstance = g_hInst;
    ofn.lpstrTitle = strTitle;
    ofn.lpstrFilter = strFilter;
    ofn.lpstrDefExt = strDefExt;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
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
    ofn.hwndOwner = hwndOwner;
    ofn.hInstance = g_hInst;
    ofn.lpstrTitle = strTitle;
    ofn.lpstrFilter = strFilter;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
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

INT_PTR CALLBACK CreateDiskProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM /*lParam*/)
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

const uint8_t DiskInitBlockAt0000[] =
{
    0xA0, 0x00, 0x05, 0x00, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x10, 0x43, 0x10, 0x9C, 0x00, 0x01,
    0x37, 0x08, 0x24, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x3F, 0x42, 0x4F, 0x4F, 0x54, 0x2D,
    0x55, 0x2D, 0x4E, 0x6F, 0x20, 0x62, 0x6F, 0x6F, 0x74, 0x20, 0x6F, 0x6E, 0x20, 0x76, 0x6F, 0x6C,
    0x75, 0x6D, 0x65, 0x0D, 0x0A, 0x0A, 0x80, 0x00, 0xDF, 0x8B, 0x74, 0xFF, 0xFD, 0x80, 0x1F, 0x94,
    0x76, 0xFF, 0xFA, 0x80, 0xFF, 0x01
};
const uint8_t DiskInitBlockAt03D2[] =
{
    0x01, 0x00, 0x06, 0x00, 0x53, 0x8E, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20
};
const uint8_t DiskInitBlockAt0C00For400K[] =
{
    0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x02, 0xD5, 0x19, 0xC0, 0x62,
    0xEA, 0x27, 0x12, 0x03, 0x00, 0x00, 0x4D, 0x2D, 0x00, 0x08
};
const uint8_t DiskInitBlockAt0C00For800K[] =
{
    0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x02, 0xD5, 0x00, 0x39, 0x67,
    0xF4, 0x26, 0x32, 0x06, 0x00, 0x00, 0xFB, 0x33, 0x00, 0x08
};

void Dialogs_DoCreateDisk(int tracks)
{
    TCHAR bufFileName[MAX_PATH];
    BOOL okResult = ShowSaveDialog(g_hwnd,
            _T("Save new disk as"),
            _T("UKNC disks (*.dsk)\0*.dsk\0All Files (*.*)\0*.*\0\0"),
            _T("dsk"),
            bufFileName);
    if (! okResult) return;

    // Create the file
    LONG fileSize = tracks * 10240;
    HANDLE hFile = ::CreateFile(bufFileName,
            GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD dwError = ::GetLastError();
        AlertWarningFormat(_T("Failed to create a file for the new disk (0x%08lx)."), dwError);
        return;
    }

    // Zero-fill the file
    ::SetFilePointer(hFile, fileSize, NULL, FILE_BEGIN);
    ::SetEndOfFile(hFile);

    // Prepare disk the same way as INIT does
    ::SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    ::WriteFile(hFile, DiskInitBlockAt0000, sizeof(DiskInitBlockAt0000), NULL, NULL);
    ::SetFilePointer(hFile, 0x03D2, NULL, FILE_BEGIN);
    ::WriteFile(hFile, DiskInitBlockAt03D2, sizeof(DiskInitBlockAt03D2), NULL, NULL);
    const uint8_t* DiskInitBlockAt0C00 = (tracks <= 40) ? DiskInitBlockAt0C00For400K : DiskInitBlockAt0C00For800K;
    ::SetFilePointer(hFile, 0x0C00, NULL, FILE_BEGIN);
    ::WriteFile(hFile, DiskInitBlockAt0C00, 26, NULL, NULL);

    ::CloseHandle(hFile);

    AlertInfo(_T("New disk file created successfully."));
}


//////////////////////////////////////////////////////////////////////
// Color Dialog

BOOL ShowColorDialog(COLORREF& color, HWND hWndOwner)
{
    CHOOSECOLOR cc;  memset(&cc, 0, sizeof(cc));
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = hWndOwner;
    cc.lpCustColors = (LPDWORD)m_DialogSettings_acrCustClr;
    cc.rgbResult = color;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (!::ChooseColor(&cc))
        return FALSE;

    color = cc.rgbResult;
    return TRUE;
}


//////////////////////////////////////////////////////////////////////
// Settings Dialog

void ShowSettingsDialog()
{
    DialogBox(g_hInst, MAKEINTRESOURCE(IDD_SETTINGS), g_hwnd, SettingsProc);
}

void FillRenderCombo(HWND hRender)
{
    TCHAR searchPath[MAX_PATH];
    ::GetModuleFileName(GetModuleHandle(NULL), searchPath, MAX_PATH);
    TCHAR* pos = _tcsrchr(searchPath, _T('\\')) + 1;  // Cutoff the file name
    _tcscpy(pos, _T("Render*.dll"));  // Add search mask

    WIN32_FIND_DATA finddata;
    HANDLE hfind = ::FindFirstFile(searchPath, &finddata);
    if (hfind != INVALID_HANDLE_VALUE)
    {
        for (;;)
        {
            ::SendMessage(hRender, CB_ADDSTRING, 0, (LPARAM)finddata.cFileName);

            if (!::FindNextFile(hfind, &finddata))
                break;
        }
    }

    TCHAR buffer[32];
    Settings_GetRender(buffer);
    ::SendMessage(hRender, CB_SELECTSTRING, 0, (LPARAM)buffer);
}

void FillScreenshotModeCombo(HWND hScreenshotMode)
{
    int screenshotMode = 0;
    for (;;)
    {
        LPCTSTR sModeName = ScreenView_GetScreenshotModeName(screenshotMode);
        if (sModeName == NULL)
            break;
        ::SendMessage(hScreenshotMode, CB_ADDSTRING, 0, (LPARAM)sModeName);
        screenshotMode++;
    }

    screenshotMode = Settings_GetScreenshotMode();
    ::SendMessage(hScreenshotMode, CB_SETCURSEL, screenshotMode, 0);
}

void SettingsDialog_InitGeneralDialog(HWND hDlg)
{
    HWND hVolume = GetDlgItem(hDlg, IDC_VOLUME);
    SendMessage(hVolume, TBM_SETRANGEMIN, 0, (LPARAM)0);
    SendMessage(hVolume, TBM_SETRANGEMAX, 0, (LPARAM)0xffff);
    SendMessage(hVolume, TBM_SETTICFREQ, 0x1000, 0);
    SendMessage(hVolume, TBM_SETPOS, TRUE, (LPARAM)Settings_GetSoundVolume());

    HWND hRender = GetDlgItem(hDlg, IDC_RENDER);
    FillRenderCombo(hRender);

    HWND hScreenshotMode = GetDlgItem(hDlg, IDC_SCREENSHOTMODE);
    FillScreenshotModeCombo(hScreenshotMode);

    TCHAR buffer[10];

    Settings_GetSerialPort(buffer);
    SetDlgItemText(hDlg, IDC_SERIALPORT, buffer);

    SetDlgItemInt(hDlg, IDC_NETWORKSTATION, Settings_GetNetStation(), FALSE);

    Settings_GetNetComPort(buffer);
    SetDlgItemText(hDlg, IDC_NETWORKPORT, buffer);

    Settings_GetSerialConfig(&m_DialogSettings_SerialConfig);
    Settings_GetNetComConfig(&m_DialogSettings_NetComConfig);
}

void SettingsDialog_SaveGeneralSettings(HWND hDlg)
{
    HWND hVolume = GetDlgItem(hDlg, IDC_VOLUME);
    DWORD volume = SendMessage(hVolume, TBM_GETPOS, 0, 0);
    Settings_SetSoundVolume((WORD)volume);

    TCHAR buffer[32];

    GetDlgItemText(hDlg, IDC_RENDER, buffer, 32);
    Settings_SetRender(buffer);

    int screenshotMode = ::SendMessage(GetDlgItem(hDlg, IDC_SCREENSHOTMODE), CB_GETCURSEL, 0, 0);
    Settings_SetScreenshotMode(screenshotMode);

    GetDlgItemText(hDlg, IDC_SERIALPORT, buffer, 10);
    Settings_SetSerialPort(buffer);

    int netStation = GetDlgItemInt(hDlg, IDC_NETWORKSTATION, 0, FALSE);
    Settings_SetNetStation(netStation);

    GetDlgItemText(hDlg, IDC_NETWORKPORT, buffer, 10);
    Settings_SetNetComPort(buffer);

    Settings_SetSerialConfig(&m_DialogSettings_SerialConfig);
    Settings_SetNetComConfig(&m_DialogSettings_NetComConfig);
}

INT_PTR CALLBACK SettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM /*lParam*/)
{
    switch (message)
    {
    case WM_INITDIALOG:
        SettingsDialog_InitGeneralDialog(hDlg);
        return (INT_PTR)FALSE;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            SettingsDialog_SaveGeneralSettings(hDlg);
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        case IDC_BUTTON1:
            ShowSerialPortSettings(&m_DialogSettings_SerialConfig);
            SetFocus(GetDlgItem(hDlg, IDC_BUTTON1));
            break;
        case IDC_BUTTON2:
            ShowSerialPortSettings(&m_DialogSettings_NetComConfig);
            SetFocus(GetDlgItem(hDlg, IDC_BUTTON2));
            break;
        default:
            return (INT_PTR)FALSE;
        }
        break;
    }
    return (INT_PTR) FALSE;
}


//////////////////////////////////////////////////////////////////////
// Settings Colors Dialog

BOOL ShowSettingsColorsDialog()
{
    return IDOK == DialogBox(g_hInst, MAKEINTRESOURCE(IDD_SETTINGS_COLORS), g_hwnd, SettingsColorsProc);
}

int CALLBACK SettingsDialog_EnumFontProc(const LOGFONT* lpelfe, const TEXTMETRIC* /*lpntme*/, DWORD /*FontType*/, LPARAM lParam)
{
    if ((lpelfe->lfPitchAndFamily & FIXED_PITCH) == 0)
        return TRUE;
    if (lpelfe->lfFaceName[0] == _T('@'))  // Skip vertical fonts
        return TRUE;

    HWND hCombo = (HWND)lParam;

    int item = ::SendMessage(hCombo, CB_FINDSTRING, 0, (LPARAM)lpelfe->lfFaceName);
    if (item < 0)
        ::SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)lpelfe->lfFaceName);

    return TRUE;
}

void SettingsDialog_FillDebugFontCombo(HWND hCombo)
{
    LOGFONT logfont;  ZeroMemory(&logfont, sizeof logfont);
    logfont.lfCharSet = DEFAULT_CHARSET;
    logfont.lfWeight = FW_NORMAL;
    logfont.lfPitchAndFamily = FIXED_PITCH | FF_DONTCARE;

    HDC hdc = ::GetDC(NULL);
    EnumFontFamiliesEx(hdc, &logfont, (FONTENUMPROC)SettingsDialog_EnumFontProc, (LPARAM)hCombo, 0);
    VERIFY(::ReleaseDC(NULL, hdc));

    Settings_GetDebugFontName(logfont.lfFaceName);
    ::SendMessage(hCombo, CB_SELECTSTRING, 0, (LPARAM)logfont.lfFaceName);
}

void SettingsDialog_FillColorsList(HWND hList)
{
    for (int itemIndex = 0; itemIndex < ColorIndicesCount; itemIndex++)
    {
        LPCTSTR colorName = Settings_GetColorFriendlyName((ColorIndices)itemIndex);
        ::SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)colorName);
        ::SendMessage(hList, LB_SETITEMDATA, itemIndex, (LPARAM)Settings_GetColor((ColorIndices)itemIndex));
    }

    ::SendMessage(hList, LB_SETCURSEL, 0, 0);
}

void SettingsDialog_InitColorDialog(HWND hDlg)
{
    HWND hDebugFont = GetDlgItem(hDlg, IDC_DEBUGFONT);
    SettingsDialog_FillDebugFontCombo(hDebugFont);

    HWND hColorList = GetDlgItem(hDlg, IDC_LIST1);
    SettingsDialog_FillColorsList(hColorList);
}

void SettingsDialog_OnColorListDrawItem(PDRAWITEMSTRUCT lpDrawItem)
{
    if (lpDrawItem->itemID == -1) return;

    HDC hdc = lpDrawItem->hDC;
    switch (lpDrawItem->itemAction)
    {
    case ODA_DRAWENTIRE:
    case ODA_SELECT:
        {
            HBRUSH hBrushBk = ::GetSysColorBrush((lpDrawItem->itemState & ODS_SELECTED) ? COLOR_HIGHLIGHT : COLOR_WINDOW);
            ::FillRect(hdc, &lpDrawItem->rcItem, hBrushBk);

            int colorIndex = lpDrawItem->itemID;
            COLORREF color = (COLORREF)(lpDrawItem->itemData);

            HBRUSH hBrush = ::CreateSolidBrush(color);
            RECT rcFill;  ::CopyRect(&rcFill, &lpDrawItem->rcItem);
            ::InflateRect(&rcFill, -1, -1);
            rcFill.left = rcFill.right - 50;
            ::FillRect(hdc, &rcFill, hBrush);

            ::SetTextColor(hdc, ::GetSysColor((lpDrawItem->itemState & ODS_SELECTED) ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
            RECT rcText;  ::CopyRect(&rcText, &lpDrawItem->rcItem);
            ::InflateRect(&rcText, -2, 0);
            LPCTSTR colorName = Settings_GetColorFriendlyName((ColorIndices)colorIndex);
            ::DrawText(hdc, colorName, _tcslen(colorName), &rcText, DT_LEFT | DT_NOPREFIX);
        }
        break;
    case ODA_FOCUS:
        break;
    }
}

void SettingsDialog_OnChooseColor(HWND hDlg)
{
    HWND hList = GetDlgItem(hDlg, IDC_LIST1);
    int itemIndex = ::SendMessage(hList, LB_GETCURSEL, 0, 0);
    COLORREF color = ::SendMessage(hList, LB_GETITEMDATA, itemIndex, 0);
    if (ShowColorDialog(color, hDlg))
    {
        ::SendMessage(hList, LB_SETITEMDATA, itemIndex, (LPARAM)color);
        ::InvalidateRect(hList, NULL, TRUE);
    }
}

void SettingsDialog_OnResetColor(HWND hDlg)
{
    HWND hList = GetDlgItem(hDlg, IDC_LIST1);
    int itemIndex = ::SendMessage(hList, LB_GETCURSEL, 0, 0);
    COLORREF color = Settings_GetDefaultColor((ColorIndices)itemIndex);

    ::SendMessage(hList, LB_SETITEMDATA, itemIndex, color);
    ::InvalidateRect(hList, NULL, TRUE);
}

void SettingsDialog_SaveFontsAndColors(HWND hDlg)
{
    TCHAR buffer[32];
    GetDlgItemText(hDlg, IDC_DEBUGFONT, buffer, 32);
    Settings_SetDebugFontName(buffer);

    HWND hList = GetDlgItem(hDlg, IDC_LIST1);
    for (int itemIndex = 0; itemIndex < ColorIndicesCount; itemIndex++)
    {
        COLORREF color = ::SendMessage(hList, LB_GETITEMDATA, itemIndex, 0);
        Settings_SetColor((ColorIndices)itemIndex, color);
    }
}

INT_PTR CALLBACK SettingsColorsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        SettingsDialog_InitColorDialog(hDlg);
        return (INT_PTR)FALSE;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BUTTON1:
            SettingsDialog_OnChooseColor(hDlg);
            break;
        case IDC_BUTTON2:
            SettingsDialog_OnResetColor(hDlg);
            break;
        case IDOK:
            SettingsDialog_SaveFontsAndColors(hDlg);
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        default:
            return (INT_PTR)FALSE;
        }
        break;
    case WM_CTLCOLORLISTBOX:
        return (LRESULT)CreateSolidBrush(GetSysColor(COLOR_WINDOW));
    case WM_DRAWITEM:
        SettingsDialog_OnColorListDrawItem((PDRAWITEMSTRUCT)lParam);
        break;
    }
    return (INT_PTR)FALSE;
}


//////////////////////////////////////////////////////////////////////
// Settings OSD Dialog

BOOL ShowSettingsOsdDialog()
{
    return IDOK == DialogBox(g_hInst, MAKEINTRESOURCE(IDD_SETTINGS_OSD), g_hwnd, SettingsOsdProc);
}

void SettingsDialog_InitOsdDialog(HWND hDlg)
{
    SetDlgItemInt(hDlg, IDC_EDIT1, Settings_GetOsdSize(), FALSE);

    SetDlgItemInt(hDlg, IDC_EDIT2, Settings_GetOsdLineWidth(), FALSE);

    m_DialogSettings_OsdLineColor = Settings_GetOsdLineColor();

    HWND hPosition = GetDlgItem(hDlg, IDC_COMBO1);
    ::SendMessage(hPosition, CB_ADDSTRING, 0, (LPARAM)_T("Top Left"));
    ::SendMessage(hPosition, CB_ADDSTRING, 0, (LPARAM)_T("Top Right"));
    ::SendMessage(hPosition, CB_ADDSTRING, 0, (LPARAM)_T("Bottom Left"));
    ::SendMessage(hPosition, CB_ADDSTRING, 0, (LPARAM)_T("Bottom Right"));
    ::SendMessage(hPosition, CB_SETCURSEL, Settings_GetOsdPosition(), 0);
}

INT_PTR CALLBACK SettingsOsdProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM /*lParam*/)
{
    switch (message)
    {
    case WM_INITDIALOG:
        SettingsDialog_InitOsdDialog(hDlg);
        return (INT_PTR)FALSE;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BUTTON1:
            ShowColorDialog(m_DialogSettings_OsdLineColor, hDlg);
            break;
        case IDOK:
            {
                BOOL okValid;

                int osdSize = GetDlgItemInt(hDlg, IDC_EDIT1, &okValid, FALSE);
                if (!okValid) osdSize = 84;
                Settings_SetOsdSize((WORD)osdSize);

                int osdLineWidth = GetDlgItemInt(hDlg, IDC_EDIT2, &okValid, FALSE);
                if (!okValid) osdLineWidth = 3;
                Settings_SetOsdLineWidth((WORD)osdLineWidth);

                Settings_SetOsdLineColor(m_DialogSettings_OsdLineColor);

                HWND hPosition = GetDlgItem(hDlg, IDC_COMBO1);
                LRESULT osdPosition = SendMessage(hPosition, CB_GETCURSEL, 0, 0);
                Settings_SetOsdPosition((WORD)osdPosition);
            }

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
    return (INT_PTR)FALSE;
}

//////////////////////////////////////////////////////////////////////


void ShowSerialPortSettings(DCB * pDCB)
{
    m_pDcbEditorData = pDCB;
    DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DCB_EDITOR), g_hwnd, DcbEditorProc);
}

const DWORD BaudrateValues[] =
{
    300, 600, 1200, 2400, 4800, 9600, 14400, 19200, 38400, 57600, 115200, 128000, 256000
};

INT_PTR CALLBACK DcbEditorProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM /*lParam*/)
{
    switch (message)
    {
    case WM_INITDIALOG:
        {
            const DCB * pDCB = m_pDcbEditorData;

            HWND hBaudrate = GetDlgItem(hDlg, IDC_BAUDRATE);
            TCHAR buffer[10];
            int selindex = 5;  // 9600 by default
            for (int i = 0; i < sizeof(BaudrateValues) / sizeof(DWORD); i++)
            {
                _sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR) - 1, _T("%lu"), BaudrateValues[i]);
                SendMessage(hBaudrate, LB_ADDSTRING, 0, (LPARAM)buffer);
                if (pDCB->BaudRate == BaudrateValues[i])
                    selindex = i;
            }
            SendMessage(hBaudrate, LB_SETCURSEL, (WPARAM)selindex, 0);

            HWND hParity = GetDlgItem(hDlg, IDC_PARITY);
            SendMessage(hParity, LB_ADDSTRING, 0, (LPARAM)_T("None"));
            SendMessage(hParity, LB_ADDSTRING, 0, (LPARAM)_T("Odd"));
            SendMessage(hParity, LB_ADDSTRING, 0, (LPARAM)_T("Even"));
            SendMessage(hParity, LB_ADDSTRING, 0, (LPARAM)_T("Mark"));
            SendMessage(hParity, LB_ADDSTRING, 0, (LPARAM)_T("Space"));

            HWND hStopBits = GetDlgItem(hDlg, IDC_STOPBITS);
            SendMessage(hStopBits, LB_ADDSTRING, 0, (LPARAM)_T("1"));
            SendMessage(hStopBits, LB_ADDSTRING, 0, (LPARAM)_T("1.5"));
            SendMessage(hStopBits, LB_ADDSTRING, 0, (LPARAM)_T("2"));

            HWND hDtrControl = GetDlgItem(hDlg, IDC_DTRCONTROL);
            SendMessage(hDtrControl, LB_ADDSTRING, 0, (LPARAM)_T("Disable"));
            SendMessage(hDtrControl, LB_ADDSTRING, 0, (LPARAM)_T("Enable"));
            SendMessage(hDtrControl, LB_ADDSTRING, 0, (LPARAM)_T("Handshake"));

            HWND hRtsControl = GetDlgItem(hDlg, IDC_RTSCONTROL);
            SendMessage(hRtsControl, LB_ADDSTRING, 0, (LPARAM)_T("Disable"));
            SendMessage(hRtsControl, LB_ADDSTRING, 0, (LPARAM)_T("Enable"));
            SendMessage(hRtsControl, LB_ADDSTRING, 0, (LPARAM)_T("Handshake"));
            SendMessage(hRtsControl, LB_ADDSTRING, 0, (LPARAM)_T("Toggle"));

            SendMessage(hParity, LB_SETCURSEL, (WPARAM)pDCB->Parity, 0);
            SendMessage(hStopBits, LB_SETCURSEL, (WPARAM)pDCB->StopBits, 0);
            SendMessage(hDtrControl, LB_SETCURSEL, (WPARAM)pDCB->fDtrControl, 0);
            SendMessage(hRtsControl, LB_SETCURSEL, (WPARAM)pDCB->fRtsControl, 0);
            CheckDlgButton(hDlg, IDC_OUTXCTSFLOW, pDCB->fOutxCtsFlow);
            CheckDlgButton(hDlg, IDC_OUTXDSRFLOW, pDCB->fOutxDsrFlow);
            CheckDlgButton(hDlg, IDC_DSRSENSITIVITY, pDCB->fDsrSensitivity);

            return (INT_PTR)FALSE;
        }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            {
                DCB * pDCB = m_pDcbEditorData;

                HWND hBaudrate = GetDlgItem(hDlg, IDC_BAUDRATE);
                int baudrateIndex = SendMessage(hBaudrate, LB_GETCURSEL, 0, 0);
                if (baudrateIndex >= sizeof(BaudrateValues) / sizeof(DWORD))
                    baudrateIndex = 5;  // 9600 by default
                pDCB->BaudRate = BaudrateValues[baudrateIndex];
                HWND hParity = GetDlgItem(hDlg, IDC_PARITY);
                pDCB->Parity = (BYTE)SendMessage(hParity, LB_GETCURSEL, 0, 0);
                HWND hStopBits = GetDlgItem(hDlg, IDC_STOPBITS);
                pDCB->StopBits = (BYTE)SendMessage(hStopBits, LB_GETCURSEL, 0, 0);
                HWND hDtrControl = GetDlgItem(hDlg, IDC_DTRCONTROL);
                pDCB->fDtrControl = (BYTE)SendMessage(hDtrControl, LB_GETCURSEL, 0, 0);
                HWND hRtsControl = GetDlgItem(hDlg, IDC_RTSCONTROL);
                pDCB->fRtsControl = (BYTE)SendMessage(hRtsControl, LB_GETCURSEL, 0, 0);
                pDCB->fOutxCtsFlow = IsDlgButtonChecked(hDlg, IDC_OUTXCTSFLOW);
                pDCB->fOutxDsrFlow = IsDlgButtonChecked(hDlg, IDC_OUTXDSRFLOW);
                pDCB->fDsrSensitivity = IsDlgButtonChecked(hDlg, IDC_DSRSENSITIVITY);
            }

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


//////////////////////////////////////////////////////////////////////
