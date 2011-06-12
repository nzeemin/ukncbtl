/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Settings.cpp

#include "stdafx.h"
#include "UKNCBTL.h"


//////////////////////////////////////////////////////////////////////


const TCHAR m_Settings_RegKeyName[] = _T("Software\\UKNCBTL");
HKEY m_Settings_RegKey = (HKEY) INVALID_HANDLE_VALUE;

BOOL m_Settings_Toolbar = TRUE;
BOOL m_Settings_Debug = FALSE;
BOOL m_Settings_Debug_Valid = FALSE;
BOOL m_Settings_RealSpeed = FALSE;
BOOL m_Settings_RealSpeed_Valid = FALSE;
BOOL m_Settings_Sound = FALSE;
BOOL m_Settings_Sound_Valid = FALSE;
BOOL m_Settings_Keyboard = TRUE;
BOOL m_Settings_Keyboard_Valid = FALSE;
BOOL m_Settings_Tape = FALSE;
BOOL m_Settings_Tape_Valid = FALSE;
BOOL m_Settings_Serial = FALSE;
BOOL m_Settings_Serial_Valid = FALSE;
BOOL m_Settings_Parallel = FALSE;
BOOL m_Settings_Parallel_Valid = FALSE;
DWORD m_Settings_CartridgeMode = 0;
BOOL m_Settings_CartridgeMode_Valid = FALSE;


//////////////////////////////////////////////////////////////////////


void Settings_Init()
{
    LONG lResult;
    lResult = ::RegCreateKeyEx(HKEY_CURRENT_USER, m_Settings_RegKeyName, 0, NULL, 0, KEY_ALL_ACCESS,
            NULL, &m_Settings_RegKey, NULL);
}
void Settings_Done()
{
    ::RegCloseKey(m_Settings_RegKey);
}

BOOL Settings_SaveStringValue(LPCTSTR sName, LPCTSTR sValue)
{
    LONG lResult;
    if (sValue == NULL)
        lResult = ::RegSetValueEx(m_Settings_RegKey, sName, 0, REG_SZ, (const BYTE*) "", sizeof(TCHAR));
    else
        lResult = ::RegSetValueEx(m_Settings_RegKey, sName, 0, REG_SZ, (const BYTE*) sValue,
                (lstrlen(sValue) + 1) * sizeof(TCHAR));
    return (lResult == ERROR_SUCCESS);
}
BOOL Settings_LoadStringValue(LPCTSTR sName, LPTSTR sBuffer, int nBufferLengthChars)
{
    DWORD dwType;
    DWORD dwBufLength = nBufferLengthChars * sizeof(TCHAR);
    LONG lResult;
    lResult = ::RegQueryValueEx(m_Settings_RegKey, sName, NULL, &dwType, (LPBYTE) sBuffer, &dwBufLength);
    if (lResult == ERROR_SUCCESS && dwType == REG_SZ)
        return TRUE;
    else
    {
        sBuffer[0] = '\0';
        return FALSE;
    }
}

BOOL Settings_SaveDwordValue(LPCTSTR sName, DWORD dwValue)
{
    LONG lResult;
    lResult = ::RegSetValueEx(m_Settings_RegKey, sName, 0, REG_DWORD, (const BYTE*) (&dwValue), sizeof(DWORD));
    return (lResult == ERROR_SUCCESS);
}
BOOL Settings_LoadDwordValue(LPCTSTR sName, DWORD* dwValue)
{
    DWORD dwType;
    DWORD dwBufLength = sizeof(DWORD);
    LONG lResult;
    lResult = ::RegQueryValueEx(m_Settings_RegKey, sName, NULL, &dwType, (LPBYTE) dwValue, &dwBufLength);
    if (lResult == ERROR_SUCCESS && dwType == REG_DWORD)
        return TRUE;
    else
        return FALSE;
}

void Settings_GetFloppyFilePath(int slot, LPTSTR buffer)
{
    TCHAR bufValueName[8];
    lstrcpy(bufValueName, _T("Floppy0"));
    bufValueName[6] = slot + _T('0');
    Settings_LoadStringValue(bufValueName, buffer, MAX_PATH);
}
void Settings_SetFloppyFilePath(int slot, LPCTSTR sFilePath)
{
    TCHAR bufValueName[8];
    lstrcpy(bufValueName, _T("Floppy0"));
    bufValueName[6] = slot + _T('0');
    Settings_SaveStringValue(bufValueName, sFilePath);
}

void Settings_GetHardFilePath(int slot, LPTSTR buffer)
{
    TCHAR bufValueName[6];
    lstrcpy(bufValueName, _T("Hard1"));
    bufValueName[4] = slot + _T('1');
    Settings_LoadStringValue(bufValueName, buffer, MAX_PATH);
}
void Settings_SetHardFilePath(int slot, LPCTSTR sFilePath)
{
    TCHAR bufValueName[6];
    lstrcpy(bufValueName, _T("Hard1"));
    bufValueName[4] = slot + _T('1');
    Settings_SaveStringValue(bufValueName, sFilePath);
}

void Settings_GetCartridgeFilePath(int slot, LPTSTR buffer)
{
    TCHAR bufValueName[11];
    lstrcpy(bufValueName, _T("Cartridge0"));
    bufValueName[9] = slot + _T('0');
    Settings_LoadStringValue(bufValueName, buffer, MAX_PATH);
}
void Settings_SetCartridgeFilePath(int slot, LPCTSTR sFilePath)
{
    TCHAR bufValueName[11];
    lstrcpy(bufValueName, _T("Cartridge0"));
    bufValueName[9] = slot + _T('0');
    Settings_SaveStringValue(bufValueName, sFilePath);
}

void Settings_SetCartridgeMode(DWORD cartridgeMode)
{
    m_Settings_CartridgeMode = cartridgeMode;
    m_Settings_CartridgeMode_Valid = TRUE;
    Settings_SaveDwordValue(_T("CartridgeMode"), (DWORD) cartridgeMode);
}
DWORD Settings_GetCartridgeMode()
{
    if (!m_Settings_CartridgeMode_Valid)
    {
        DWORD dwValue = (DWORD) 0;
        Settings_LoadDwordValue(_T("CartridgeMode"), &dwValue);
        m_Settings_CartridgeMode = dwValue;
        m_Settings_CartridgeMode_Valid = TRUE;
    }
    return m_Settings_CartridgeMode;
}

void Settings_SetScreenViewMode(int mode)
{
    Settings_SaveDwordValue(_T("ScreenViewMode"), (DWORD) mode);
}
int Settings_GetScreenViewMode()
{
    DWORD dwValue;
    Settings_LoadDwordValue(_T("ScreenViewMode"), &dwValue);
    return (int) dwValue;
}

void Settings_SetScreenHeightMode(int mode)
{
    Settings_SaveDwordValue(_T("ScreenHeightMode"), (DWORD) mode);
}
int Settings_GetScreenHeightMode()
{
    DWORD dwValue;
    Settings_LoadDwordValue(_T("ScreenHeightMode"), &dwValue);
    return (int) dwValue;
}

void Settings_SetToolbar(BOOL flag)
{
    Settings_SaveDwordValue(_T("Toolbar"), (DWORD) flag);
}
BOOL Settings_GetToolbar()
{
    DWORD dwValue = (DWORD) TRUE;
    Settings_LoadDwordValue(_T("Toolbar"), &dwValue);
    return (BOOL) dwValue;
}

void Settings_SetDebug(BOOL flag)
{
    m_Settings_Debug = flag;
    m_Settings_Debug_Valid = TRUE;
    Settings_SaveDwordValue(_T("Debug"), (DWORD) flag);
}
BOOL Settings_GetDebug()
{
    if (!m_Settings_Debug_Valid)
    {
        DWORD dwValue = (DWORD) FALSE;
        Settings_LoadDwordValue(_T("Debug"), &dwValue);
        m_Settings_Debug = (BOOL) dwValue;
        m_Settings_Debug_Valid = TRUE;
    }
    return m_Settings_Debug;
}

void Settings_SetAutostart(BOOL flag)
{
    Settings_SaveDwordValue(_T("Autostart"), (DWORD) flag);
}
BOOL Settings_GetAutostart()
{
    DWORD dwValue = (DWORD) FALSE;
    Settings_LoadDwordValue(_T("Autostart"), &dwValue);
    return (BOOL) dwValue;
}

void Settings_SetRealSpeed(BOOL flag)
{
    m_Settings_RealSpeed = flag;
    m_Settings_RealSpeed_Valid = TRUE;
    Settings_SaveDwordValue(_T("RealSpeed"), (DWORD) flag);
}
BOOL Settings_GetRealSpeed()
{
    if (!m_Settings_RealSpeed_Valid)
    {
        DWORD dwValue = (DWORD) FALSE;
        Settings_LoadDwordValue(_T("RealSpeed"), &dwValue);
        m_Settings_RealSpeed = (BOOL) dwValue;
        m_Settings_RealSpeed_Valid = TRUE;
    }
    return m_Settings_RealSpeed;
}

void Settings_SetSound(BOOL flag)
{
    m_Settings_Sound = flag;
    m_Settings_Sound_Valid = TRUE;
    Settings_SaveDwordValue(_T("Sound"), (DWORD) flag);
}
BOOL Settings_GetSound()
{
    if (!m_Settings_Sound_Valid)
    {
        DWORD dwValue = (DWORD) FALSE;
        Settings_LoadDwordValue(_T("Sound"), &dwValue);
        m_Settings_Sound = (BOOL) dwValue;
        m_Settings_Sound_Valid = TRUE;
    }
    return m_Settings_Sound;
}

void Settings_SetKeyboard(BOOL flag)
{
    m_Settings_Keyboard = flag;
    m_Settings_Keyboard_Valid = TRUE;
    Settings_SaveDwordValue(_T("Keyboard"), (DWORD) flag);
}
BOOL Settings_GetKeyboard()
{
    if (!m_Settings_Keyboard_Valid)
    {
        DWORD dwValue = (DWORD) TRUE;
        Settings_LoadDwordValue(_T("Keyboard"), &dwValue);
        m_Settings_Keyboard = (BOOL) dwValue;
        m_Settings_Keyboard_Valid = TRUE;
    }
    return m_Settings_Keyboard;
}

void Settings_SetTape(BOOL flag)
{
    m_Settings_Tape = flag;
    m_Settings_Tape_Valid = TRUE;
    Settings_SaveDwordValue(_T("Tape"), (DWORD) flag);
}
BOOL Settings_GetTape()
{
    if (!m_Settings_Tape_Valid)
    {
        DWORD dwValue = (DWORD) FALSE;
        Settings_LoadDwordValue(_T("Tape"), &dwValue);
        m_Settings_Tape = (BOOL) dwValue;
        m_Settings_Tape_Valid = TRUE;
    }
    return m_Settings_Tape;
}

void Settings_SetSerial(BOOL flag)
{
    m_Settings_Serial = flag;
    m_Settings_Serial_Valid = TRUE;
    Settings_SaveDwordValue(_T("Serial"), (DWORD) flag);
}
BOOL Settings_GetSerial()
{
    if (!m_Settings_Serial_Valid)
    {
        DWORD dwValue = (DWORD) FALSE;
        Settings_LoadDwordValue(_T("Serial"), &dwValue);
        m_Settings_Serial = (BOOL) dwValue;
        m_Settings_Serial_Valid = TRUE;
    }
    return m_Settings_Serial;
}

void Settings_SetParallel(BOOL flag)
{
    m_Settings_Parallel = flag;
    m_Settings_Parallel_Valid = TRUE;
    Settings_SaveDwordValue(_T("Parallel"), (DWORD) flag);
}
BOOL Settings_GetParallel()
{
    if (!m_Settings_Parallel_Valid)
    {
        DWORD dwValue = (DWORD) FALSE;
        Settings_LoadDwordValue(_T("Parallel"), &dwValue);
        m_Settings_Parallel = (BOOL) dwValue;
        m_Settings_Parallel_Valid = TRUE;
    }
    return m_Settings_Parallel;
}

void Settings_GetSerialPort(LPTSTR buffer)
{
    Settings_LoadStringValue(_T("SerialPort"), buffer, 10);
}
void Settings_SetSerialPort(LPCTSTR sValue)
{
    Settings_SaveStringValue(_T("SerialPort"), sValue);
}


//////////////////////////////////////////////////////////////////////
