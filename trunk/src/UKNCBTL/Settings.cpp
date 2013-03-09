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


const TCHAR m_Settings_IniAppName[] = _T("UKNCBTL");
TCHAR m_Settings_IniPath[MAX_PATH];

DCB  m_Settings_SerialConfig;
DCB  m_Settings_NetComConfig;


//////////////////////////////////////////////////////////////////////


void Settings_Init()
{
    // Prepare m_Settings_IniPath: get .exe file path and change extension to .ini
    ::GetModuleFileName(GetModuleHandle(NULL), m_Settings_IniPath, MAX_PATH);
    TCHAR* pExt = m_Settings_IniPath + _tcslen(m_Settings_IniPath) - 3;
    *pExt++ = _T('i');
    *pExt++ = _T('n');
    *pExt++ = _T('i');

    // Set m_Settings_SerialConfig defaults
    ::memset(&m_Settings_SerialConfig, 0, sizeof(DCB));
    m_Settings_SerialConfig.DCBlength = sizeof(DCB);
    m_Settings_SerialConfig.BaudRate = 9600;
    m_Settings_SerialConfig.ByteSize = 8;
    m_Settings_SerialConfig.fBinary = 1;
    m_Settings_SerialConfig.fParity = 0;
    m_Settings_SerialConfig.fOutxCtsFlow = m_Settings_SerialConfig.fOutxDsrFlow = 0;
    m_Settings_SerialConfig.fDtrControl = DTR_CONTROL_ENABLE;
    m_Settings_SerialConfig.fDsrSensitivity = 0;
    m_Settings_SerialConfig.fTXContinueOnXoff = 0;
    m_Settings_SerialConfig.fOutX = m_Settings_SerialConfig.fInX = 0;
    m_Settings_SerialConfig.fErrorChar = 0;
    m_Settings_SerialConfig.fNull = 0;
    m_Settings_SerialConfig.fRtsControl = RTS_CONTROL_HANDSHAKE;
    m_Settings_SerialConfig.fAbortOnError = 0;
    m_Settings_SerialConfig.Parity = NOPARITY;
    m_Settings_SerialConfig.StopBits = TWOSTOPBITS;

    // Set m_Settings_NetComConfig defaults
    ::memset(&m_Settings_NetComConfig, 0, sizeof(DCB));
    m_Settings_NetComConfig.DCBlength = sizeof(DCB);
    m_Settings_NetComConfig.BaudRate = 57600;
    m_Settings_NetComConfig.ByteSize = 8;
    m_Settings_NetComConfig.fBinary = 1;
    m_Settings_NetComConfig.fParity = 1;
    m_Settings_NetComConfig.fOutxCtsFlow = m_Settings_NetComConfig.fOutxDsrFlow = 0;
    m_Settings_NetComConfig.fDtrControl = DTR_CONTROL_DISABLE;
    m_Settings_NetComConfig.fDsrSensitivity = 0;
    m_Settings_NetComConfig.fTXContinueOnXoff = 0;
    m_Settings_NetComConfig.fOutX = m_Settings_NetComConfig.fInX = 0;
    m_Settings_NetComConfig.fErrorChar = 0;
    m_Settings_NetComConfig.fNull = 0;
    m_Settings_NetComConfig.fRtsControl = RTS_CONTROL_DISABLE;
    m_Settings_NetComConfig.fAbortOnError = 0;
    m_Settings_NetComConfig.Parity = ODDPARITY;
    m_Settings_NetComConfig.StopBits = TWOSTOPBITS;
}
void Settings_Done()
{
}

BOOL Settings_SaveStringValue(LPCTSTR sName, LPCTSTR sValue)
{
    BOOL result = WritePrivateProfileString(
        m_Settings_IniAppName, sName, sValue, m_Settings_IniPath);
    return result;
}
BOOL Settings_LoadStringValue(LPCTSTR sName, LPTSTR sBuffer, int nBufferLengthChars)
{
    DWORD result = GetPrivateProfileString(
        m_Settings_IniAppName, sName, NULL, sBuffer, nBufferLengthChars, m_Settings_IniPath);
    if (result > 0)
        return TRUE;

    sBuffer[0] = _T('\0');
    return FALSE;
}

BOOL Settings_SaveDwordValue(LPCTSTR sName, DWORD dwValue)
{
    TCHAR buffer[12];
    wsprintf(buffer, _T("%lu"), dwValue);

    return Settings_SaveStringValue(sName, buffer);
}
BOOL Settings_LoadDwordValue(LPCTSTR sName, DWORD* dwValue)
{
    TCHAR buffer[12];
    if (!Settings_LoadStringValue(sName, buffer, 12))
        return FALSE;

    int result = swscanf(buffer, _T("%lu"), dwValue);
    if (result == 0)
        return FALSE;

    return TRUE;
}

BOOL Settings_SaveBinaryValue(LPCTSTR sName, const void * pData, int size)
{
    TCHAR* buffer = (TCHAR*) ::malloc((size * 2 + 1) * sizeof(TCHAR));
    const BYTE* p = (const BYTE*) pData;
    TCHAR* buf = buffer;
    for (int i = 0; i < size; i++)
    {
        int a = *p;
        wsprintf(buf, _T("%02X"), a);
        p++;
        buf += 2;
    }

    BOOL result = Settings_SaveStringValue(sName, buffer);

    free(buffer);

    return result;
}

BOOL Settings_LoadBinaryValue(LPCTSTR sName, void * pData, int size)
{
    size_t buffersize = (size * 2 + 1) * sizeof(TCHAR);
    TCHAR* buffer = (TCHAR*) ::malloc(buffersize);
    if (!Settings_LoadStringValue(sName, buffer, buffersize))
    {
        free(buffer);
        return FALSE;
    }

    BYTE* p = (BYTE*) pData;
    TCHAR* buf = buffer;
    for (int i = 0; i < size; i++)
    {
        BYTE v = 0;

        TCHAR ch = *buf;
        if (ch >= _T('0') && ch <= _T('9'))
            v = ch - _T('0');
        else if (ch >= _T('A') && ch <= _T('F'))
            v = ch - _T('A') + 10;
        else  // Not hex
        {
            free(buffer);
            return FALSE;
        }
        buf++;

        v = v << 4;

        ch = *buf;
        if (ch >= _T('0') && ch <= _T('9'))
            v |= ch - _T('0');
        else if (ch >= _T('A') && ch <= _T('F'))
            v |= ch - _T('A') + 10;
        else  // Not hex
        {
            free(buffer);
            return FALSE;
        }
        buf++;

        *p = v;
        p++;
    }

    free(buffer);

    return TRUE;
}


//////////////////////////////////////////////////////////////////////


#define SETTINGS_GETSET_DWORD(PARAMNAME, PARAMNAMESTR, OUTTYPE, DEFVALUE) \
    OUTTYPE m_Settings_##PARAMNAME = DEFVALUE; \
    BOOL m_Settings_##PARAMNAME##_Valid = FALSE; \
    void Settings_Set##PARAMNAME(OUTTYPE newvalue) { \
        m_Settings_##PARAMNAME = newvalue; \
        m_Settings_##PARAMNAME##_Valid = TRUE; \
        Settings_SaveDwordValue(PARAMNAMESTR, (DWORD) newvalue); \
    } \
    OUTTYPE Settings_Get##PARAMNAME##() { \
        if (!m_Settings_##PARAMNAME##_Valid) { \
            DWORD dwValue = (DWORD) DEFVALUE; \
            Settings_LoadDwordValue(PARAMNAMESTR, &dwValue); \
            m_Settings_##PARAMNAME = (OUTTYPE) dwValue; \
            m_Settings_##PARAMNAME##_Valid = TRUE; \
        } \
        return m_Settings_##PARAMNAME; \
    }


BOOL Settings_GetWindowRect(RECT * pRect)
{
    RECT rc;
    if (Settings_LoadBinaryValue(_T("WindowRect"), &rc, sizeof(RECT)))
    {
        ::memcpy(pRect, &rc, sizeof(RECT));
        return TRUE;
    }

    return FALSE;
}
void Settings_SetWindowRect(const RECT * pRect)
{
    Settings_SaveBinaryValue(_T("WindowRect"), (const void *)pRect, sizeof(RECT));
}

SETTINGS_GETSET_DWORD(WindowMaximized, _T("WindowMaximized"), BOOL, FALSE);

SETTINGS_GETSET_DWORD(WindowFullscreen, _T("WindowFullscreen"), BOOL, FALSE);

void Settings_GetRender(LPTSTR buffer)
{
    if (!Settings_LoadStringValue(_T("Render"), buffer, 32))
    {
        _tcscpy(buffer, _T("RenderVfw.dll"));
    }
}

void Settings_GetFloppyFilePath(int slot, LPTSTR buffer)
{
    TCHAR bufValueName[8];
    lstrcpy(bufValueName, _T("Floppy0"));
    bufValueName[6] = _T('0') + (TCHAR)slot;
    Settings_LoadStringValue(bufValueName, buffer, MAX_PATH);
}
void Settings_SetFloppyFilePath(int slot, LPCTSTR sFilePath)
{
    TCHAR bufValueName[8];
    lstrcpy(bufValueName, _T("Floppy0"));
    bufValueName[6] = _T('0') + (TCHAR)slot;
    Settings_SaveStringValue(bufValueName, sFilePath);
}

void Settings_GetHardFilePath(int slot, LPTSTR buffer)
{
    TCHAR bufValueName[6];
    lstrcpy(bufValueName, _T("Hard1"));
    bufValueName[4] = _T('1') + (TCHAR)slot;
    Settings_LoadStringValue(bufValueName, buffer, MAX_PATH);
}
void Settings_SetHardFilePath(int slot, LPCTSTR sFilePath)
{
    TCHAR bufValueName[6];
    lstrcpy(bufValueName, _T("Hard1"));
    bufValueName[4] = _T('1') + (TCHAR)slot;
    Settings_SaveStringValue(bufValueName, sFilePath);
}

void Settings_GetCartridgeFilePath(int slot, LPTSTR buffer)
{
    TCHAR bufValueName[11];
    lstrcpy(bufValueName, _T("Cartridge0"));
    bufValueName[9] = _T('0') + (TCHAR)slot;
    Settings_LoadStringValue(bufValueName, buffer, MAX_PATH);
}
void Settings_SetCartridgeFilePath(int slot, LPCTSTR sFilePath)
{
    TCHAR bufValueName[11];
    lstrcpy(bufValueName, _T("Cartridge0"));
    bufValueName[9] = _T('0') + (TCHAR)slot;
    Settings_SaveStringValue(bufValueName, sFilePath);
}

SETTINGS_GETSET_DWORD(ScreenViewMode, _T("ScreenViewMode"), int, 0);

SETTINGS_GETSET_DWORD(ScreenHeightMode, _T("ScreenHeightMode"), int, 0);

SETTINGS_GETSET_DWORD(Toolbar, _T("Toolbar"), BOOL, TRUE);

SETTINGS_GETSET_DWORD(Debug, _T("Debug"), BOOL, FALSE);

SETTINGS_GETSET_DWORD(Autostart, _T("Autostart"), BOOL, FALSE);

SETTINGS_GETSET_DWORD(RealSpeed, _T("RealSpeed"), BOOL, FALSE);

SETTINGS_GETSET_DWORD(Sound, _T("Sound"), BOOL, FALSE);
SETTINGS_GETSET_DWORD(SoundVolume, _T("SoundVolume"), WORD, 0x3fff);

SETTINGS_GETSET_DWORD(Keyboard, _T("Keyboard"), BOOL, TRUE);

SETTINGS_GETSET_DWORD(Tape, _T("Tape"), BOOL, FALSE);

SETTINGS_GETSET_DWORD(Serial, _T("Serial"), BOOL, FALSE);

SETTINGS_GETSET_DWORD(Parallel, _T("Parallel"), BOOL, FALSE);

void Settings_GetSerialPort(LPTSTR buffer)
{
    Settings_LoadStringValue(_T("SerialPort"), buffer, 10);
}
void Settings_SetSerialPort(LPCTSTR sValue)
{
    Settings_SaveStringValue(_T("SerialPort"), sValue);
}

BOOL m_Settings_SerialConfig_Valid = FALSE;
void Settings_GetSerialConfig(DCB * pDcb)
{
    if (!m_Settings_SerialConfig_Valid)
    {
        DCB dcb;
        if (Settings_LoadBinaryValue(_T("SerialConfig"), &dcb, sizeof(DCB)))
        {
            ::memcpy(&m_Settings_SerialConfig, &dcb, sizeof(DCB));
        }
        //NOTE: else -- use defaults from m_Settings_SerialConfig

        m_Settings_SerialConfig_Valid = TRUE;
    }
    if (m_Settings_SerialConfig_Valid)
    {
        ::memcpy(pDcb, &m_Settings_SerialConfig, sizeof(DCB));
    }
}
void Settings_SetSerialConfig(const DCB * pDcb)
{
    ::memcpy(&m_Settings_SerialConfig, pDcb, sizeof(DCB));
    Settings_SaveBinaryValue(_T("SerialConfig"), (const void *)pDcb, sizeof(DCB));
    m_Settings_SerialConfig_Valid = TRUE;
}

SETTINGS_GETSET_DWORD(Network, _T("Network"), BOOL, FALSE);

SETTINGS_GETSET_DWORD(NetStation, _T("NetStation"), int, 0);

void Settings_GetNetComPort(LPTSTR buffer)
{
    Settings_LoadStringValue(_T("NetComPort"), buffer, 10);
}
void Settings_SetNetComPort(LPCTSTR sValue)
{
    Settings_SaveStringValue(_T("NetComPort"), sValue);
}

BOOL m_Settings_NetComConfig_Valid = FALSE;
void Settings_GetNetComConfig(DCB * pDcb)
{
    if (!m_Settings_NetComConfig_Valid)
    {
        DCB dcb;
        if (Settings_LoadBinaryValue(_T("NetComConfig"), &dcb, sizeof(DCB)))
        {
            ::memcpy(&m_Settings_NetComConfig, &dcb, sizeof(DCB));
        }
        //NOTE: else -- use defaults from m_Settings_NetComConfig

        m_Settings_NetComConfig_Valid = TRUE;
    }
    if (m_Settings_NetComConfig_Valid)
    {
        ::memcpy(pDcb, &m_Settings_NetComConfig, sizeof(DCB));
    }
}
void Settings_SetNetComConfig(const DCB * pDcb)
{
    ::memcpy(&m_Settings_NetComConfig, pDcb, sizeof(DCB));
    Settings_SaveBinaryValue(_T("NetComConfig"), (const void *)pDcb, sizeof(DCB));
    m_Settings_NetComConfig_Valid = TRUE;
}


//////////////////////////////////////////////////////////////////////
