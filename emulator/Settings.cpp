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
#include "Main.h"
#include <vector>
#include <algorithm>
#include <climits>
#include "util/SettingsCodec.h"

//////////////////////////////////////////////////////////////////////


const TCHAR m_Settings_IniAppName[] = _T("UKNCBTL");
static std::basic_string<TCHAR> m_Settings_IniPath;
static void Settings_ResetCaches();

DCB  m_Settings_SerialConfig;
DCB  m_Settings_NetComConfig;

//////////////////////////////////////////////////////////////////////
// Options

bool Option_ShowHelp = false;
int Option_AutoBoot = 0;


//////////////////////////////////////////////////////////////////////


void Settings_Init(LPCTSTR iniPath)
{
    Settings_ResetCaches();
    if (iniPath)
        m_Settings_IniPath = iniPath;
    else
    {
        TCHAR executable[MAX_PATH]{};
        ::GetModuleFileName(GetModuleHandle(nullptr), executable, MAX_PATH);
        m_Settings_IniPath = executable;
        const auto extension = m_Settings_IniPath.find_last_of(_T('.'));
        if (extension != std::basic_string<TCHAR>::npos) m_Settings_IniPath.resize(extension);
        m_Settings_IniPath += _T(".ini");
    }
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
            m_Settings_IniAppName, sName, sValue, m_Settings_IniPath.c_str());
    return result;
}
BOOL Settings_LoadStringValue(LPCTSTR sName, LPTSTR sBuffer, int nBufferLengthChars)
{
    if (!sBuffer || nBufferLengthChars <= 0) return FALSE;
    // Probe one character beyond the destination so an exact fit is distinct
    // from a truncated value. Never expose a valid-looking truncated prefix.
    std::vector<TCHAR> text(static_cast<size_t>(nBufferLengthChars) + 1, 0);
    const DWORD result = GetPrivateProfileString(
        m_Settings_IniAppName, sName, nullptr, text.data(),
        static_cast<DWORD>(text.size()), m_Settings_IniPath.c_str());
    if (result > 0 && result < static_cast<DWORD>(nBufferLengthChars))
    {
        std::copy_n(text.data(), result + 1, sBuffer);
        return TRUE;
    }
    sBuffer[0] = _T('\0');
    return FALSE;
}

BOOL Settings_SaveDwordValue(LPCTSTR sName, DWORD dwValue)
{
    TCHAR buffer[12];
    FormatBuffer(buffer, _T("{}"), dwValue);

    return Settings_SaveStringValue(sName, buffer);
}
BOOL Settings_LoadDwordValue(LPCTSTR sName, DWORD* dwValue)
{
    if (!dwValue) return FALSE;
    TCHAR buffer[64];
    if (!Settings_LoadStringValue(sName, buffer, 64)) return FALSE;
    const auto value = settings_codec::DecodeDword(buffer);
    if (!value) return FALSE;
    *dwValue = *value;
    return TRUE;
}
BOOL Settings_SaveColorValue(LPCTSTR sName, COLORREF color)
{
    const auto text = settings_codec::EncodeColor(color);
    return Settings_SaveStringValue(sName, text.c_str());
}
BOOL Settings_LoadColorValue(LPCTSTR sName, COLORREF* pColor)
{
    if (!pColor) return FALSE;
    TCHAR buffer[64];
    if (!Settings_LoadStringValue(sName, buffer, 64)) return FALSE;
    const auto color = settings_codec::DecodeColor(buffer);
    if (!color) return FALSE;
    *pColor = *color;
    return TRUE;
}

BOOL Settings_SaveBinaryValue(LPCTSTR sName, const void* pData, int size)
{
    if (size < 0 || (size > 0 && !pData)) return FALSE;
    const auto text = settings_codec::EncodeBinary(
        std::span{static_cast<const uint8_t*>(pData), static_cast<size_t>(size)});
    return Settings_SaveStringValue(sName, text.c_str());
}

BOOL Settings_LoadBinaryValue(LPCTSTR sName, void* pData, int size)
{
    if (size < 0 || (size > 0 && !pData) || size > (INT_MAX - 2) / 2) return FALSE;
    // One extra character detects a value longer than the requested object.
    std::vector<TCHAR> buffer(static_cast<size_t>(size) * 2 + 2, 0);
    if (!Settings_LoadStringValue(sName, buffer.data(), static_cast<int>(buffer.size()))) return FALSE;
    return settings_codec::DecodeBinary(buffer.data(),
        std::span{static_cast<uint8_t*>(pData), static_cast<size_t>(size)});
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
    OUTTYPE Settings_Get##PARAMNAME() { \
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
    Settings_SaveBinaryValue(_T("WindowRect"), pRect, sizeof(RECT));
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
void Settings_SetRender(LPCTSTR sValue)
{
    Settings_SaveStringValue(_T("Render"), sValue);
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

SETTINGS_GETSET_DWORD(ScreenshotMode, _T("ScreenshotMode"), int, 1);

SETTINGS_GETSET_DWORD(Toolbar, _T("Toolbar"), BOOL, TRUE);

SETTINGS_GETSET_DWORD(Debug, _T("Debug"), BOOL, FALSE);

void Settings_GetDebugFontName(LPTSTR buffer)
{
    if (!Settings_LoadStringValue(_T("DebugFontName"), buffer, 32))
    {
        _tcscpy(buffer, _T("Lucida Console"));
    }
}
void Settings_SetDebugFontName(LPCTSTR sFontName)
{
    Settings_SaveStringValue(_T("DebugFontName"), sFontName);
}

SETTINGS_GETSET_DWORD(DebugCpuPpu, _T("DebugCpuPpu"), BOOL, FALSE);

void Settings_SetDebugBreakpoint(int bpno, BOOL okCpuPpu, WORD address)
{
    TCHAR bufValueName[17];
    lstrcpy(bufValueName, _T("DebugBreakptCpu0"));
    bufValueName[12] = okCpuPpu ? _T('C') : _T('P');
    bufValueName[15] = bpno < 10 ? _T('0') + (TCHAR)bpno : _T('A') + (TCHAR)(bpno - 10);
    if (address == 0177777)
        Settings_SaveStringValue(bufValueName, nullptr);  // delete value
    else
        Settings_SaveDwordValue(bufValueName, address);
}
WORD Settings_GetDebugBreakpoint(int bpno, BOOL okCpuPpu)
{
    TCHAR bufValueName[17];
    lstrcpy(bufValueName, _T("DebugBreakptCpu0"));
    bufValueName[12] = okCpuPpu ? _T('C') : _T('P');
    bufValueName[15] = bpno < 10 ? _T('0') + (TCHAR)bpno : _T('A') + (TCHAR)(bpno - 10);
    DWORD dwValue = 0xFFFFFFFF;
    Settings_LoadDwordValue(bufValueName, &dwValue);
    return (WORD)dwValue;
}

SETTINGS_GETSET_DWORD(DebugMemoryMode, _T("DebugMemoryMode"), WORD, 3);
SETTINGS_GETSET_DWORD(DebugMemoryAddress, _T("DebugMemoryAddress"), WORD, 0);
SETTINGS_GETSET_DWORD(DebugMemoryBase, _T("DebugMemoryBase"), WORD, 0);
SETTINGS_GETSET_DWORD(DebugMemoryByte, _T("DebugMemoryByte"), BOOL, FALSE);
SETTINGS_GETSET_DWORD(DebugMemoryNumeral, _T("DebugMemoryNumeral"), WORD, 0);

SETTINGS_GETSET_DWORD(Autostart, _T("Autostart"), BOOL, FALSE);

SETTINGS_GETSET_DWORD(RealSpeed, _T("RealSpeed"), WORD, 1);

SETTINGS_GETSET_DWORD(Sound, _T("Sound"), BOOL, FALSE);
SETTINGS_GETSET_DWORD(SoundVolume, _T("SoundVolume"), WORD, 0x3fff);
SETTINGS_GETSET_DWORD(SoundAY, _T("SoundAY"), BOOL, FALSE);
SETTINGS_GETSET_DWORD(SoundCovox, _T("SoundCovox"), BOOL, FALSE);

SETTINGS_GETSET_DWORD(Mouse, _T("Mouse"), BOOL, FALSE);

SETTINGS_GETSET_DWORD(Keyboard, _T("Keyboard"), BOOL, TRUE);

SETTINGS_GETSET_DWORD(Tape, _T("Tape"), BOOL, FALSE);

SETTINGS_GETSET_DWORD(InstrHistory, _T("InstrHistory"), BOOL, FALSE);

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

    ::memcpy(pDcb, &m_Settings_SerialConfig, sizeof(DCB));
}
void Settings_SetSerialConfig(const DCB * pDcb)
{
    ::memcpy(&m_Settings_SerialConfig, pDcb, sizeof(DCB));
    Settings_SaveBinaryValue(_T("SerialConfig"), pDcb, sizeof(DCB));
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

    ::memcpy(pDcb, &m_Settings_NetComConfig, sizeof(DCB));
}
void Settings_SetNetComConfig(const DCB * pDcb)
{
    ::memcpy(&m_Settings_NetComConfig, pDcb, sizeof(DCB));
    Settings_SaveBinaryValue(_T("NetComConfig"), pDcb, sizeof(DCB));
    m_Settings_NetComConfig_Valid = TRUE;
}

SETTINGS_GETSET_DWORD(SpriteAddress, _T("SpriteAddress"), WORD, 0);

SETTINGS_GETSET_DWORD(SpriteWidth, _T("SpriteWidth"), WORD, 2);

SETTINGS_GETSET_DWORD(SpriteMode, _T("SpriteMode"), WORD, 0);


//////////////////////////////////////////////////////////////////////
// Colors

struct ColorDescriptorStruct
{
    LPCTSTR  settingname;
    COLORREF defaultcolor;
    BOOL     valid;
    LPCTSTR  friendlyname;
    COLORREF currentcolor;
}
static ColorDescriptors[ColorIndicesCount] =
{
    { _T("ColorDebugText"),         RGB(0,   0,   0),   FALSE, _T("Debug Text") },
    { _T("ColorDebugBackCurrent"),  RGB(255, 255, 224), FALSE, _T("Debug Current Line Background") },
    { _T("ColorDebugValueChanged"), RGB(128, 0,   0),   FALSE, _T("Debug Value Changed") },
    { _T("ColorDebugPrevious"),     RGB(0,   0,   212), FALSE, _T("Debug Previous Address Marker") },
    { _T("ColorDebugMemoryROM"),    RGB(0,   0,   128), FALSE, _T("Debug Memory ROM") },
    { _T("ColorDebugMemoryIO"),     RGB(0,   128, 0),   FALSE, _T("Debug Memory IO") },
    { _T("ColorDebugMemoryNA"),     RGB(128, 128, 128), FALSE, _T("Debug Memory NA") },
    { _T("ColorDebugValue"),        RGB(128, 128, 128), FALSE, _T("Debug Value") },
    { _T("ColorDebugValueRom"),     RGB(128, 128, 192), FALSE, _T("Debug Value ROM") },
    { _T("ColorDebugSubtitles"),    RGB(0,   64,  0),   FALSE, _T("Debug Subtitles") },
    { _T("ColorDebugJump"),         RGB(80,  192, 224), FALSE, _T("Debug Jump") },
    { _T("ColorDebugJumpYes"),      RGB(80,  240, 80),  FALSE, _T("Debug Jump Yes") },
    { _T("ColorDebugJumpNo"),       RGB(180, 180, 180), FALSE, _T("Debug Jump No") },
    { _T("ColorDebugJumpHint"),     RGB(40,  128, 160), FALSE, _T("Debug Jump Hint") },
    { _T("ColorDebugHint"),         RGB(40,  40,  160), FALSE, _T("Debug Hint") },
    { _T("ColorDebugBreakpoint"),   RGB(255, 64,  64),  FALSE, _T("Debug Breakpoint") },
    { _T("ColorDebugHighlight"),    RGB(200, 230, 255), FALSE, _T("Debug Highlight") },
    { _T("ColorDebugBreakptZone"),  RGB(242, 242, 242), FALSE, _T("Debug Breakpoint Zone") },
};

LPCTSTR Settings_GetColorFriendlyName(ColorIndices colorIndex)
{
    ColorDescriptorStruct* desc = ColorDescriptors + colorIndex;

    return desc->friendlyname;
}

COLORREF Settings_GetColor(ColorIndices colorIndex)
{
    if (colorIndex < 0 || colorIndex >= ColorIndicesCount)
        return 0;

    ColorDescriptorStruct* desc = ColorDescriptors + colorIndex;

    if (desc->valid)
        return desc->currentcolor;

    COLORREF color;
    if (Settings_LoadColorValue(desc->settingname, &color))
        desc->currentcolor = color;
    else
        desc->currentcolor = desc->defaultcolor;

    desc->valid = TRUE;
    return desc->currentcolor;
}

void Settings_SetColor(ColorIndices colorIndex, COLORREF color)
{
    if (colorIndex < 0 || colorIndex >= ColorIndicesCount)
        return;

    ColorDescriptorStruct* desc = ColorDescriptors + colorIndex;

    desc->currentcolor = color;
    desc->valid = TRUE;

    Settings_SaveColorValue(desc->settingname, color);
}

COLORREF Settings_GetDefaultColor(ColorIndices colorIndex)
{
    if (colorIndex < 0 || colorIndex >= ColorIndicesCount)
        return 0;

    ColorDescriptorStruct* desc = ColorDescriptors + colorIndex;

    return desc->defaultcolor;
}


//////////////////////////////////////////////////////////////////////

static void Settings_ResetCaches()
{
    m_Settings_WindowMaximized_Valid = FALSE;
    m_Settings_WindowFullscreen_Valid = FALSE;
    m_Settings_ScreenViewMode_Valid = FALSE;
    m_Settings_ScreenHeightMode_Valid = FALSE;
    m_Settings_ScreenshotMode_Valid = FALSE;
    m_Settings_Toolbar_Valid = FALSE;
    m_Settings_Debug_Valid = FALSE;
    m_Settings_DebugCpuPpu_Valid = FALSE;
    m_Settings_DebugMemoryMode_Valid = FALSE;
    m_Settings_DebugMemoryAddress_Valid = FALSE;
    m_Settings_DebugMemoryBase_Valid = FALSE;
    m_Settings_DebugMemoryByte_Valid = FALSE;
    m_Settings_DebugMemoryNumeral_Valid = FALSE;
    m_Settings_Autostart_Valid = FALSE;
    m_Settings_RealSpeed_Valid = FALSE;
    m_Settings_Sound_Valid = FALSE;
    m_Settings_SoundVolume_Valid = FALSE;
    m_Settings_SoundAY_Valid = FALSE;
    m_Settings_SoundCovox_Valid = FALSE;
    m_Settings_Mouse_Valid = FALSE;
    m_Settings_Keyboard_Valid = FALSE;
    m_Settings_Tape_Valid = FALSE;
    m_Settings_InstrHistory_Valid = FALSE;
    m_Settings_Serial_Valid = FALSE;
    m_Settings_Parallel_Valid = FALSE;
    m_Settings_Network_Valid = FALSE;
    m_Settings_NetStation_Valid = FALSE;
    m_Settings_SpriteAddress_Valid = FALSE;
    m_Settings_SpriteWidth_Valid = FALSE;
    m_Settings_SpriteMode_Valid = FALSE;
    m_Settings_SerialConfig_Valid = FALSE;
    m_Settings_NetComConfig_Valid = FALSE;
    for (auto& color : ColorDescriptors) color.valid = FALSE;
}
