// Settings.cpp

#include "stdafx.h"
#include "UKNCBTL.h"


//////////////////////////////////////////////////////////////////////


const TCHAR m_Settings_RegKeyName[] = _T("Software\\UKNCBTL");
HKEY m_Settings_RegKey = (HKEY) INVALID_HANDLE_VALUE;

BOOL m_Settings_Debug = FALSE;
BOOL m_Settings_Debug_Valid = FALSE;
BOOL m_Settings_RealSpeed = FALSE;
BOOL m_Settings_RealSpeed_Valid = FALSE;
BOOL m_Settings_Keyboard = FALSE;
BOOL m_Settings_Keyboard_Valid = FALSE;
BOOL m_Settings_Tape = FALSE;
BOOL m_Settings_Tape_Valid = FALSE;


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
    {
        *dwValue = 0;
        return FALSE;
    }
}

void Settings_GetFloppyFilePath(int slot, LPTSTR buffer)
{
    TCHAR bufValueName[] = _T("Floppy0");
    bufValueName[6] = slot + _T('0');
    Settings_LoadStringValue(bufValueName, buffer, MAX_PATH);
}
void Settings_SetFloppyFilePath(int slot, LPCTSTR sFilePath)
{
    TCHAR bufValueName[] = _T("Floppy0");
    bufValueName[6] = slot + _T('0');
    Settings_SaveStringValue(bufValueName, sFilePath);
}

void Settings_GetCartridgeFilePath(int slot, LPTSTR buffer)
{
    TCHAR bufValueName[] = _T("Cartridge0");
    bufValueName[9] = slot + _T('0');
    Settings_LoadStringValue(bufValueName, buffer, MAX_PATH);
}
void Settings_SetCartridgeFilePath(int slot, LPCTSTR sFilePath)
{
    TCHAR bufValueName[] = _T("Cartridge0");
    bufValueName[9] = slot + _T('0');
    Settings_SaveStringValue(bufValueName, sFilePath);
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
#if !defined(PRODUCT)
        dwValue = (DWORD) TRUE;
#endif
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
        DWORD dwValue = (DWORD) FALSE;
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


//////////////////////////////////////////////////////////////////////
