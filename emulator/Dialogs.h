/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Dialogs.h

//////////////////////////////////////////////////////////////////////


void ShowAboutBox();

// Input octal value
//   strTitle - dialog caption
//   strPrompt - label text
BOOL InputBoxOctal(HWND hwndOwner, LPCTSTR strTitle, WORD* pValue);

BOOL ShowSaveDialog(HWND hwndOwner, LPCTSTR strTitle, LPCTSTR strFilter, LPCTSTR strDefExt, TCHAR* bufFileName);
BOOL ShowOpenDialog(HWND hwndOwner, LPCTSTR strTitle, LPCTSTR strFilter, TCHAR* bufFileName);

void ShowCreateDiskDialog();

void ShowSettingsDialog();

void ShowSerialPortSettings(DCB * pDCB);

void ShowConfigurationDialog();


//////////////////////////////////////////////////////////////////////
