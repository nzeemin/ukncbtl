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

BOOL m_Settings_Toolbar = TRUE;
BOOL m_Settings_Debug = FALSE;
BOOL m_Settings_Debug_Valid = FALSE;
BOOL m_Settings_RealSpeed = FALSE;
BOOL m_Settings_RealSpeed_Valid = FALSE;
BOOL m_Settings_Sound = FALSE;
BOOL m_Settings_Sound_Valid = FALSE;
WORD m_Settings_SoundVolume = 0x7fff;
BOOL m_Settings_SoundVolume_Valid = FALSE;
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
BOOL m_Settings_Network = FALSE;
BOOL m_Settings_Network_Valid = FALSE;
WORD m_Settings_NetStation = 0;
BOOL m_Settings_NetStation_Valid = FALSE;
DCB  m_Settings_SerialConfig;
BOOL m_Settings_SerialConfig_Valid = FALSE;
DCB  m_Settings_NetComConfig;
BOOL m_Settings_NetComConfig_Valid = FALSE;


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
    {
        //*dwValue = 0;
        return FALSE;
    }

    int result = swscanf(buffer, _T("%lu"), dwValue);
    if (result == 0)
    {
        //*dwValue = 0;
        return FALSE;
    }

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

void Settings_SetSoundVolume(WORD value)
{
    m_Settings_SoundVolume = value;
    m_Settings_SoundVolume_Valid = TRUE;
    Settings_SaveDwordValue(_T("SoundVolume"), (DWORD) value);
}
WORD Settings_GetSoundVolume()
{
    if (!m_Settings_SoundVolume_Valid)
    {
        DWORD dwValue = (DWORD) 0x7fff;
        Settings_LoadDwordValue(_T("SoundVolume"), &dwValue);
        m_Settings_SoundVolume = (WORD)dwValue;
        m_Settings_SoundVolume_Valid = TRUE;
    }
    return m_Settings_SoundVolume;
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

void Settings_GetSerialConfig(DCB * pDcb)
{
    if (!m_Settings_SerialConfig_Valid)
    {
        DCB dcb;
        if (Settings_LoadBinaryValue(_T("SerialConfig"), &dcb, sizeof(DCB)))
        {
            ::memcpy(&m_Settings_SerialConfig, &dcb, sizeof(DCB));
        }
        //NOTE: else -- use dafaults from m_Settings_SerialConfig

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


void Settings_SetNetwork(BOOL flag)
{
    m_Settings_Network = flag;
    m_Settings_Network_Valid = TRUE;
    Settings_SaveDwordValue(_T("Network"), (DWORD) flag);
}
BOOL Settings_GetNetwork()
{
    if (!m_Settings_Network_Valid)
    {
        DWORD dwValue = (DWORD) FALSE;
        Settings_LoadDwordValue(_T("Network"), &dwValue);
        m_Settings_Network = (BOOL) dwValue;
        m_Settings_Network_Valid = TRUE;
    }
    return m_Settings_Network;
}

void Settings_SetNetStation(int value)
{
    m_Settings_NetStation = (WORD)value;
    m_Settings_NetStation_Valid = TRUE;
    Settings_SaveDwordValue(_T("NetStation"), (DWORD) value);
}
int Settings_GetNetStation()
{
    if (!m_Settings_NetStation_Valid)
    {
        DWORD dwValue = (DWORD) 0;
        Settings_LoadDwordValue(_T("NetStation"), &dwValue);
        m_Settings_NetStation = (WORD)dwValue;
        m_Settings_NetStation_Valid = TRUE;
    }
    return (int)m_Settings_NetStation;
}

void Settings_GetNetComPort(LPTSTR buffer)
{
    Settings_LoadStringValue(_T("NetComPort"), buffer, 10);
}
void Settings_SetNetComPort(LPCTSTR sValue)
{
    Settings_SaveStringValue(_T("NetComPort"), sValue);
}

void Settings_GetNetComConfig(DCB * pDcb)
{
    if (!m_Settings_NetComConfig_Valid)
    {
        DCB dcb;
        if (Settings_LoadBinaryValue(_T("NetComConfig"), &dcb, sizeof(DCB)))
        {
            ::memcpy(&m_Settings_NetComConfig, &dcb, sizeof(DCB));
        }
        //NOTE: else -- use dafaults from m_Settings_NetComConfig

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
