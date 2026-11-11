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
#include <optional>
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
    m_Settings_SerialConfig = {};
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
    m_Settings_NetComConfig = {};
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


template <typename T>
class CachedDwordSetting
{
public:
    CachedDwordSetting(LPCTSTR name, T defaultValue)
        : name_(name), defaultValue_(defaultValue) {}

    T Get()
    {
        if (!value_)
        {
            DWORD stored = static_cast<DWORD>(defaultValue_);
            Settings_LoadDwordValue(name_, &stored);
            value_ = static_cast<T>(stored);
        }
        return *value_;
    }

    void Set(T value)
    {
        value_ = value;
        Settings_SaveDwordValue(name_, static_cast<DWORD>(value));
    }

    void Reset() { value_.reset(); }

private:
    LPCTSTR name_;
    T defaultValue_;
    std::optional<T> value_;
};

BOOL Settings_GetWindowRect(RECT * pRect)
{
    RECT rc;
    if (Settings_LoadBinaryValue(_T("WindowRect"), &rc, sizeof(RECT)))
    {
        *pRect = rc;
        return TRUE;
    }

    return FALSE;
}
void Settings_SetWindowRect(const RECT * pRect)
{
    Settings_SaveBinaryValue(_T("WindowRect"), pRect, sizeof(RECT));
}

static CachedDwordSetting<BOOL> settingWindowMaximized{_T("WindowMaximized"), FALSE};
void Settings_SetWindowMaximized(BOOL value) { settingWindowMaximized.Set(value); }
BOOL Settings_GetWindowMaximized() { return settingWindowMaximized.Get(); }

static CachedDwordSetting<BOOL> settingWindowFullscreen{_T("WindowFullscreen"), FALSE};
void Settings_SetWindowFullscreen(BOOL value) { settingWindowFullscreen.Set(value); }
BOOL Settings_GetWindowFullscreen() { return settingWindowFullscreen.Get(); }

void Settings_GetRender(std::span<TCHAR> buffer)
{
    if (!Settings_LoadStringValue(_T("Render"), buffer.data(), static_cast<int>((std::min)(buffer.size(), size_t{32}))))
    {
        constexpr auto fallback = _T("RenderVfw.dll");
        const auto length = std::char_traits<TCHAR>::length(fallback);
        if (buffer.size() > length)
            std::copy_n(fallback, length + 1, buffer.begin());
    }
}
void Settings_SetRender(LPCTSTR sValue)
{
    Settings_SaveStringValue(_T("Render"), sValue);
}

void Settings_GetFloppyFilePath(int slot, std::span<TCHAR> buffer)
{
    TCHAR bufValueName[] = _T("Floppy0");
    bufValueName[6] = _T('0') + (TCHAR)slot;
    Settings_LoadStringValue(bufValueName, buffer.data(), static_cast<int>((std::min)(buffer.size(), size_t{MAX_PATH})));
}
void Settings_SetFloppyFilePath(int slot, LPCTSTR sFilePath)
{
    TCHAR bufValueName[] = _T("Floppy0");
    bufValueName[6] = _T('0') + (TCHAR)slot;
    Settings_SaveStringValue(bufValueName, sFilePath);
}

void Settings_GetHardFilePath(int slot, std::span<TCHAR> buffer)
{
    TCHAR bufValueName[] = _T("Hard1");
    bufValueName[4] = _T('1') + (TCHAR)slot;
    Settings_LoadStringValue(bufValueName, buffer.data(), static_cast<int>((std::min)(buffer.size(), size_t{MAX_PATH})));
}
void Settings_SetHardFilePath(int slot, LPCTSTR sFilePath)
{
    TCHAR bufValueName[] = _T("Hard1");
    bufValueName[4] = _T('1') + (TCHAR)slot;
    Settings_SaveStringValue(bufValueName, sFilePath);
}

void Settings_GetCartridgeFilePath(int slot, std::span<TCHAR> buffer)
{
    TCHAR bufValueName[] = _T("Cartridge0");
    bufValueName[9] = _T('0') + (TCHAR)slot;
    Settings_LoadStringValue(bufValueName, buffer.data(), static_cast<int>((std::min)(buffer.size(), size_t{MAX_PATH})));
}
void Settings_SetCartridgeFilePath(int slot, LPCTSTR sFilePath)
{
    TCHAR bufValueName[] = _T("Cartridge0");
    bufValueName[9] = _T('0') + (TCHAR)slot;
    Settings_SaveStringValue(bufValueName, sFilePath);
}

static CachedDwordSetting<int> settingScreenViewMode{_T("ScreenViewMode"), 0};
void Settings_SetScreenViewMode(int value) { settingScreenViewMode.Set(value); }
int Settings_GetScreenViewMode() { return settingScreenViewMode.Get(); }

static CachedDwordSetting<int> settingScreenHeightMode{_T("ScreenHeightMode"), 0};
void Settings_SetScreenHeightMode(int value) { settingScreenHeightMode.Set(value); }
int Settings_GetScreenHeightMode() { return settingScreenHeightMode.Get(); }

static CachedDwordSetting<int> settingScreenshotMode{_T("ScreenshotMode"), 1};
void Settings_SetScreenshotMode(int value) { settingScreenshotMode.Set(value); }
int Settings_GetScreenshotMode() { return settingScreenshotMode.Get(); }

static CachedDwordSetting<BOOL> settingToolbar{_T("Toolbar"), TRUE};
void Settings_SetToolbar(BOOL value) { settingToolbar.Set(value); }
BOOL Settings_GetToolbar() { return settingToolbar.Get(); }

static CachedDwordSetting<BOOL> settingDebug{_T("Debug"), FALSE};
void Settings_SetDebug(BOOL value) { settingDebug.Set(value); }
BOOL Settings_GetDebug() { return settingDebug.Get(); }

void Settings_GetDebugFontName(std::span<TCHAR> buffer)
{
    if (!Settings_LoadStringValue(_T("DebugFontName"), buffer.data(), static_cast<int>((std::min)(buffer.size(), size_t{32}))))
    {
        constexpr auto fallback = _T("Lucida Console");
        const auto length = std::char_traits<TCHAR>::length(fallback);
        if (buffer.size() > length)
            std::copy_n(fallback, length + 1, buffer.begin());
    }
}
void Settings_SetDebugFontName(LPCTSTR sFontName)
{
    Settings_SaveStringValue(_T("DebugFontName"), sFontName);
}

static CachedDwordSetting<BOOL> settingDebugCpuPpu{_T("DebugCpuPpu"), FALSE};
void Settings_SetDebugCpuPpu(BOOL value) { settingDebugCpuPpu.Set(value); }
BOOL Settings_GetDebugCpuPpu() { return settingDebugCpuPpu.Get(); }

void Settings_SetDebugBreakpoint(int bpno, BOOL okCpuPpu, WORD address)
{
    TCHAR bufValueName[] = _T("DebugBreakptCpu0");
    bufValueName[12] = okCpuPpu ? _T('C') : _T('P');
    bufValueName[15] = bpno < 10 ? _T('0') + (TCHAR)bpno : _T('A') + (TCHAR)(bpno - 10);
    if (address == 0177777)
        Settings_SaveStringValue(bufValueName, nullptr);  // delete value
    else
        Settings_SaveDwordValue(bufValueName, address);
}
WORD Settings_GetDebugBreakpoint(int bpno, BOOL okCpuPpu)
{
    TCHAR bufValueName[] = _T("DebugBreakptCpu0");
    bufValueName[12] = okCpuPpu ? _T('C') : _T('P');
    bufValueName[15] = bpno < 10 ? _T('0') + (TCHAR)bpno : _T('A') + (TCHAR)(bpno - 10);
    DWORD dwValue = 0xFFFFFFFF;
    Settings_LoadDwordValue(bufValueName, &dwValue);
    return (WORD)dwValue;
}

static CachedDwordSetting<WORD> settingDebugMemoryMode{_T("DebugMemoryMode"), 3};
void Settings_SetDebugMemoryMode(WORD value) { settingDebugMemoryMode.Set(value); }
WORD Settings_GetDebugMemoryMode() { return settingDebugMemoryMode.Get(); }
static CachedDwordSetting<WORD> settingDebugMemoryAddress{_T("DebugMemoryAddress"), 0};
void Settings_SetDebugMemoryAddress(WORD value) { settingDebugMemoryAddress.Set(value); }
WORD Settings_GetDebugMemoryAddress() { return settingDebugMemoryAddress.Get(); }
static CachedDwordSetting<WORD> settingDebugMemoryBase{_T("DebugMemoryBase"), 0};
void Settings_SetDebugMemoryBase(WORD value) { settingDebugMemoryBase.Set(value); }
WORD Settings_GetDebugMemoryBase() { return settingDebugMemoryBase.Get(); }
static CachedDwordSetting<BOOL> settingDebugMemoryByte{_T("DebugMemoryByte"), FALSE};
void Settings_SetDebugMemoryByte(BOOL value) { settingDebugMemoryByte.Set(value); }
BOOL Settings_GetDebugMemoryByte() { return settingDebugMemoryByte.Get(); }
static CachedDwordSetting<WORD> settingDebugMemoryNumeral{_T("DebugMemoryNumeral"), 0};
void Settings_SetDebugMemoryNumeral(WORD value) { settingDebugMemoryNumeral.Set(value); }
WORD Settings_GetDebugMemoryNumeral() { return settingDebugMemoryNumeral.Get(); }

static CachedDwordSetting<BOOL> settingAutostart{_T("Autostart"), FALSE};
void Settings_SetAutostart(BOOL value) { settingAutostart.Set(value); }
BOOL Settings_GetAutostart() { return settingAutostart.Get(); }

static CachedDwordSetting<WORD> settingRealSpeed{_T("RealSpeed"), 1};
void Settings_SetRealSpeed(WORD value) { settingRealSpeed.Set(value); }
WORD Settings_GetRealSpeed() { return settingRealSpeed.Get(); }

static CachedDwordSetting<BOOL> settingSound{_T("Sound"), FALSE};
void Settings_SetSound(BOOL value) { settingSound.Set(value); }
BOOL Settings_GetSound() { return settingSound.Get(); }
static CachedDwordSetting<WORD> settingSoundVolume{_T("SoundVolume"), 0x3fff};
void Settings_SetSoundVolume(WORD value) { settingSoundVolume.Set(value); }
WORD Settings_GetSoundVolume() { return settingSoundVolume.Get(); }
static CachedDwordSetting<BOOL> settingSoundAY{_T("SoundAY"), FALSE};
void Settings_SetSoundAY(BOOL value) { settingSoundAY.Set(value); }
BOOL Settings_GetSoundAY() { return settingSoundAY.Get(); }
static CachedDwordSetting<BOOL> settingSoundCovox{_T("SoundCovox"), FALSE};
void Settings_SetSoundCovox(BOOL value) { settingSoundCovox.Set(value); }
BOOL Settings_GetSoundCovox() { return settingSoundCovox.Get(); }

static CachedDwordSetting<BOOL> settingMouse{_T("Mouse"), FALSE};
void Settings_SetMouse(BOOL value) { settingMouse.Set(value); }
BOOL Settings_GetMouse() { return settingMouse.Get(); }

static CachedDwordSetting<BOOL> settingKeyboard{_T("Keyboard"), TRUE};
void Settings_SetKeyboard(BOOL value) { settingKeyboard.Set(value); }
BOOL Settings_GetKeyboard() { return settingKeyboard.Get(); }

static CachedDwordSetting<BOOL> settingTape{_T("Tape"), FALSE};
void Settings_SetTape(BOOL value) { settingTape.Set(value); }
BOOL Settings_GetTape() { return settingTape.Get(); }

static CachedDwordSetting<BOOL> settingInstrHistory{_T("InstrHistory"), FALSE};
void Settings_SetInstrHistory(BOOL value) { settingInstrHistory.Set(value); }
BOOL Settings_GetInstrHistory() { return settingInstrHistory.Get(); }

static CachedDwordSetting<BOOL> settingSerial{_T("Serial"), FALSE};
void Settings_SetSerial(BOOL value) { settingSerial.Set(value); }
BOOL Settings_GetSerial() { return settingSerial.Get(); }

static CachedDwordSetting<BOOL> settingParallel{_T("Parallel"), FALSE};
void Settings_SetParallel(BOOL value) { settingParallel.Set(value); }
BOOL Settings_GetParallel() { return settingParallel.Get(); }

void Settings_GetSerialPort(std::span<TCHAR> buffer)
{
    Settings_LoadStringValue(_T("SerialPort"), buffer.data(), static_cast<int>((std::min)(buffer.size(), size_t{10})));
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
            m_Settings_SerialConfig = dcb;
        }
        //NOTE: else -- use defaults from m_Settings_SerialConfig

        m_Settings_SerialConfig_Valid = TRUE;
    }

    *pDcb = m_Settings_SerialConfig;
}
void Settings_SetSerialConfig(const DCB * pDcb)
{
    m_Settings_SerialConfig = *pDcb;
    Settings_SaveBinaryValue(_T("SerialConfig"), pDcb, sizeof(DCB));
    m_Settings_SerialConfig_Valid = TRUE;
}

static CachedDwordSetting<BOOL> settingNetwork{_T("Network"), FALSE};
void Settings_SetNetwork(BOOL value) { settingNetwork.Set(value); }
BOOL Settings_GetNetwork() { return settingNetwork.Get(); }

static CachedDwordSetting<int> settingNetStation{_T("NetStation"), 0};
void Settings_SetNetStation(int value) { settingNetStation.Set(value); }
int Settings_GetNetStation() { return settingNetStation.Get(); }

void Settings_GetNetComPort(std::span<TCHAR> buffer)
{
    Settings_LoadStringValue(_T("NetComPort"), buffer.data(), static_cast<int>((std::min)(buffer.size(), size_t{10})));
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
            m_Settings_NetComConfig = dcb;
        }
        //NOTE: else -- use defaults from m_Settings_NetComConfig

        m_Settings_NetComConfig_Valid = TRUE;
    }

    *pDcb = m_Settings_NetComConfig;
}
void Settings_SetNetComConfig(const DCB * pDcb)
{
    m_Settings_NetComConfig = *pDcb;
    Settings_SaveBinaryValue(_T("NetComConfig"), pDcb, sizeof(DCB));
    m_Settings_NetComConfig_Valid = TRUE;
}

static CachedDwordSetting<WORD> settingSpriteAddress{_T("SpriteAddress"), 0};
void Settings_SetSpriteAddress(WORD value) { settingSpriteAddress.Set(value); }
WORD Settings_GetSpriteAddress() { return settingSpriteAddress.Get(); }

static CachedDwordSetting<WORD> settingSpriteWidth{_T("SpriteWidth"), 2};
void Settings_SetSpriteWidth(WORD value) { settingSpriteWidth.Set(value); }
WORD Settings_GetSpriteWidth() { return settingSpriteWidth.Get(); }

static CachedDwordSetting<WORD> settingSpriteMode{_T("SpriteMode"), 0};
void Settings_SetSpriteMode(WORD value) { settingSpriteMode.Set(value); }
WORD Settings_GetSpriteMode() { return settingSpriteMode.Get(); }


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
    settingWindowMaximized.Reset();
    settingWindowFullscreen.Reset();
    settingScreenViewMode.Reset();
    settingScreenHeightMode.Reset();
    settingScreenshotMode.Reset();
    settingToolbar.Reset();
    settingDebug.Reset();
    settingDebugCpuPpu.Reset();
    settingDebugMemoryMode.Reset();
    settingDebugMemoryAddress.Reset();
    settingDebugMemoryBase.Reset();
    settingDebugMemoryByte.Reset();
    settingDebugMemoryNumeral.Reset();
    settingAutostart.Reset();
    settingRealSpeed.Reset();
    settingSound.Reset();
    settingSoundVolume.Reset();
    settingSoundAY.Reset();
    settingSoundCovox.Reset();
    settingMouse.Reset();
    settingKeyboard.Reset();
    settingTape.Reset();
    settingInstrHistory.Reset();
    settingSerial.Reset();
    settingParallel.Reset();
    settingNetwork.Reset();
    settingNetStation.Reset();
    settingSpriteAddress.Reset();
    settingSpriteWidth.Reset();
    settingSpriteMode.Reset();
    m_Settings_SerialConfig_Valid = FALSE;
    m_Settings_NetComConfig_Valid = FALSE;
    for (auto& color : ColorDescriptors) color.valid = FALSE;
}
