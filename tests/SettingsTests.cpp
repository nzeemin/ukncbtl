/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include "Main.h"
#include "util/SettingsCodec.h"
#include "util/File.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <functional>
#include <climits>

// Internal persistence entry points compiled from the application's Settings.cpp.
BOOL Settings_SaveStringValue(LPCTSTR, LPCTSTR);
BOOL Settings_LoadStringValue(LPCTSTR, LPTSTR, int);
BOOL Settings_SaveDwordValue(LPCTSTR, DWORD);
BOOL Settings_LoadDwordValue(LPCTSTR, DWORD*);
BOOL Settings_SaveColorValue(LPCTSTR, COLORREF);
BOOL Settings_LoadColorValue(LPCTSTR, COLORREF*);
BOOL Settings_SaveBinaryValue(LPCTSTR, const void*, int);
BOOL Settings_LoadBinaryValue(LPCTSTR, void*, int);

TEST(SettingsCodec, DecimalBoundariesAndLeadingZeros)
{
    EXPECT_EQ(settings_codec::DecodeDword(_T("0")), 0u);
    EXPECT_EQ(settings_codec::DecodeDword(_T("4294967295")), UINT32_MAX);
    EXPECT_EQ(settings_codec::DecodeDword(_T("0000123")), 123u);
}

TEST(SettingsCodec, RejectsMalformedDecimalAndOverflow)
{
    for (const auto text : {_T(""), _T("-1"), _T("+1"), _T("1x"), _T(" 1"),
             _T("1 "), _T("0x10"), _T("1.0"), _T("4294967296"), _T("99999999999999999999")})
        EXPECT_FALSE(settings_codec::DecodeDword(text)) << testing::PrintToString(text);
    EXPECT_FALSE(settings_codec::DecodeDword(std::basic_string_view<TCHAR>(_T("1\0x"), 3)));
}

TEST(SettingsCodec, ColorEncodingUsesRgbOrderAndSixUppercaseDigits)
{
    EXPECT_EQ(settings_codec::EncodeColor(RGB(0x12, 0x34, 0xab)), _T("1234AB"));
    EXPECT_EQ(settings_codec::EncodeColor(0), _T("000000"));
    EXPECT_EQ(settings_codec::EncodeColor(0xffffffff), _T("FFFFFF"));
    EXPECT_EQ(settings_codec::DecodeColor(_T("1234ab")), RGB(0x12, 0x34, 0xab));
    EXPECT_EQ(settings_codec::DecodeColor(_T("F")), RGB(0, 0, 15));
}

TEST(SettingsCodec, RejectsInvalidOrOutOfRangeColors)
{
    for (const auto text : {_T(""), _T("#FFFFFF"), _T("-1"), _T("FFFFFG"), _T("FFFFFFx"),
                            _T("1000000"), _T("FFFFFFFF"), _T("FF FF")})
        EXPECT_FALSE(settings_codec::DecodeColor(text));
}

TEST(SettingsCodec, BinaryRoundTripsEveryByteAndAcceptsLowercase)
{
    std::array<uint8_t, 256> source{}, target{};
    for (size_t i = 0; i < source.size(); ++i) source[i] = static_cast<uint8_t>(i);
    auto text = settings_codec::EncodeBinary(source);
    EXPECT_EQ(text.size(), 512u);
    EXPECT_EQ(text.substr(0, 8), _T("00010203"));
    EXPECT_EQ(text.substr(508), _T("FEFF"));
    for (auto& ch : text) if (ch >= _T('A') && ch <= _T('F')) ch += _T('a') - _T('A');
    EXPECT_TRUE(settings_codec::DecodeBinary(text, target));
    EXPECT_EQ(target, source);
    EXPECT_EQ(settings_codec::EncodeBinary({}), _T(""));
    EXPECT_TRUE(settings_codec::DecodeBinary(_T(""), {}));
}

TEST(SettingsCodec, BinaryFailureDoesNotPartiallyOverwriteOutput)
{
    for (const auto text : {_T(""), _T("01"), _T("010"), _T("010203"), _T("01G0"), _T("010 ")})
    {
        std::array<uint8_t, 4> guarded{0xaa, 0xbb, 0xcc, 0xdd};
        const auto before = guarded;
        EXPECT_FALSE(settings_codec::DecodeBinary(text, std::span{guarded}.subspan<1, 2>()));
        EXPECT_EQ(guarded, before);
    }
}

TEST(SettingsCodec, BinarySuccessPreservesSurroundingBytes)
{
    std::array<uint8_t, 4> bytes{0xaa, 0, 0, 0xdd};
    EXPECT_TRUE(settings_codec::DecodeBinary(_T("01fE"), std::span{bytes}.subspan<1, 2>()));
    EXPECT_EQ(bytes, (std::array<uint8_t, 4>{0xaa, 1, 0xfe, 0xdd}));
}

namespace {
class SettingsTests : public testing::Test
{
protected:
    std::array<TCHAR, MAX_PATH> path{}, other{};
    void SetUp() override
    {
        std::array<TCHAR, MAX_PATH> directory{};
        ASSERT_NE(GetTempPath(MAX_PATH, directory.data()), 0u);
        for (auto* name : {&path, &other})
        {
            ASSERT_NE(GetTempFileName(directory.data(), _T("ini"), 0, name->data()), 0u);
            // Explicit Unicode INIs preserve non-ASCII paths independently of ACP.
            UniqueFile file(_tfopen(name->data(), _T("wb")));
            ASSERT_TRUE(file);
            const uint8_t bom[]{0xff, 0xfe};
            ASSERT_EQ(std::fwrite(bom, 1, 2, file.get()), 2u);
        }
        Settings_Init(path.data());
    }
    void TearDown() override
    {
        Settings_Done();
        for (const auto* name : {&path, &other})
            if (name->front())
            {
                WritePrivateProfileString(nullptr, nullptr, nullptr, name->data());
                DeleteFile(name->data());
            }
    }
    void Raw(const TCHAR* key, const TCHAR* value)
    { ASSERT_TRUE(WritePrivateProfileString(_T("UKNCBTL"), key, value, path.data())); }
};

TEST_F(SettingsTests, StringsRoundTripDeleteAndDistinguishExactFitFromTruncation)
{
    ASSERT_TRUE(Settings_SaveStringValue(_T("Text"), _T("abcd")));
    std::array<TCHAR, 7> guarded{};
    guarded.fill(_T('!'));
    EXPECT_TRUE(Settings_LoadStringValue(_T("Text"), guarded.data() + 1, 5));
    EXPECT_STREQ(guarded.data() + 1, _T("abcd"));
    EXPECT_EQ(guarded.front(), _T('!'));
    EXPECT_EQ(guarded.back(), _T('!'));
    EXPECT_FALSE(Settings_LoadStringValue(_T("Text"), guarded.data() + 1, 4));
    EXPECT_EQ(guarded[1], 0);
    EXPECT_FALSE(Settings_LoadStringValue(_T("Text"), nullptr, 0));
    EXPECT_FALSE(Settings_LoadStringValue(_T("Text"), guarded.data() + 1, 1));
    EXPECT_EQ(guarded[1], 0);
    EXPECT_FALSE(Settings_LoadStringValue(_T("Text"), guarded.data(), -1));
    ASSERT_TRUE(Settings_SaveStringValue(_T("Text"), nullptr));
    EXPECT_FALSE(Settings_LoadStringValue(_T("Text"), guarded.data(), 7));
    EXPECT_EQ(guarded[0], 0);
}

TEST_F(SettingsTests, DwordRoundTripAndMalformedInputPreserveOutput)
{
    for (const DWORD value : std::array<DWORD, 3>{0, 1, MAXDWORD})
    {
        ASSERT_TRUE(Settings_SaveDwordValue(_T("Number"), value));
        DWORD result = 42;
        EXPECT_TRUE(Settings_LoadDwordValue(_T("Number"), &result));
        EXPECT_EQ(result, value);
    }
    for (const auto text : {_T("4294967296"), _T("-1"), _T("123junk"), _T(""),
                            _T("000000000000000000000000000000000000000000000000000000000000000000001")})
    {
        Raw(_T("Number"), text);
        DWORD result = 42;
        EXPECT_FALSE(Settings_LoadDwordValue(_T("Number"), &result));
        EXPECT_EQ(result, 42u);
    }
}

TEST_F(SettingsTests, BinaryRequiresExactLengthAndLeavesDestinationOnFailure)
{
    const std::array<uint8_t, 2> source{0, 0xff};
    ASSERT_TRUE(Settings_SaveBinaryValue(_T("Binary"), source.data(), 2));
    std::array<uint8_t, 2> output{};
    EXPECT_TRUE(Settings_LoadBinaryValue(_T("Binary"), output.data(), 2));
    EXPECT_EQ(output, source);
    for (const auto text : {_T("00"), _T("00F"), _T("00FF00"), _T("00G0"), _T("")})
    {
        Raw(_T("Binary"), text);
        output.fill(0xab);
        EXPECT_FALSE(Settings_LoadBinaryValue(_T("Binary"), output.data(), 2));
        EXPECT_EQ(output, (std::array<uint8_t, 2>{0xab, 0xab}));
    }
    EXPECT_FALSE(Settings_SaveBinaryValue(_T("Binary"), nullptr, 1));
    EXPECT_FALSE(Settings_SaveBinaryValue(_T("Binary"), source.data(), -1));
    EXPECT_FALSE(Settings_LoadBinaryValue(_T("Binary"), nullptr, 1));
    EXPECT_FALSE(Settings_LoadBinaryValue(_T("Binary"), output.data(), -1));
    EXPECT_FALSE(Settings_LoadBinaryValue(_T("Binary"), output.data(), INT_MAX));
}

TEST_F(SettingsTests, MissingValuesDoNotOverwriteNumericOrBinaryOutputs)
{
    DWORD number = 123;
    COLORREF color = RGB(1, 2, 3);
    std::array<uint8_t, 2> bytes{0xab, 0xcd};
    EXPECT_FALSE(Settings_LoadDwordValue(_T("Missing"), &number));
    EXPECT_FALSE(Settings_LoadColorValue(_T("Missing"), &color));
    EXPECT_FALSE(Settings_LoadBinaryValue(_T("Missing"), bytes.data(), 2));
    EXPECT_EQ(number, 123u);
    EXPECT_EQ(color, RGB(1, 2, 3));
    EXPECT_EQ(bytes, (std::array<uint8_t, 2>{0xab, 0xcd}));
    EXPECT_FALSE(Settings_LoadDwordValue(_T("Missing"), nullptr));
    EXPECT_FALSE(Settings_LoadColorValue(_T("Missing"), nullptr));
}

TEST_F(SettingsTests, EmptyBinaryEntryKeepsExistingEmptyAsMissingBehavior)
{
    EXPECT_TRUE(Settings_SaveBinaryValue(_T("Empty"), nullptr, 0));
    EXPECT_FALSE(Settings_LoadBinaryValue(_T("Empty"), nullptr, 0));
}

TEST_F(SettingsTests, UnwritableLocationReportsFailureWithoutTouchingOriginalFile)
{
    ASSERT_TRUE(Settings_SaveDwordValue(_T("Saved"), 123));
    const auto impossible = std::basic_string<TCHAR>(path.data()) + _T("\\settings.ini");
    // path is an existing file, so it cannot also be the parent directory.
    Settings_Init(impossible.c_str());
    EXPECT_FALSE(Settings_SaveStringValue(_T("Text"), _T("value")));
    EXPECT_FALSE(Settings_SaveDwordValue(_T("Saved"), 456));
    EXPECT_FALSE(Settings_SaveColorValue(_T("Color"), RGB(1, 2, 3)));
    Settings_Init(path.data());
    DWORD saved = 0;
    EXPECT_TRUE(Settings_LoadDwordValue(_T("Saved"), &saved));
    EXPECT_EQ(saved, 123u);
}

TEST_F(SettingsTests, WindowRectPersistsAndMalformedRectLeavesCallerUntouched)
{
    const RECT rect{-100, -200, 1920, 1080};
    Settings_SetWindowRect(&rect);
    Settings_Init(path.data());
    RECT actual{};
    EXPECT_TRUE(Settings_GetWindowRect(&actual));
    EXPECT_EQ(std::memcmp(&actual, &rect, sizeof(rect)), 0);
    Raw(_T("WindowRect"), _T("0000"));
    EXPECT_FALSE(Settings_GetWindowRect(&actual));
    EXPECT_EQ(std::memcmp(&actual, &rect, sizeof(rect)), 0);
}

TEST_F(SettingsTests, RenderAndFontDefaultsAndCustomValues)
{
    std::array<TCHAR, 32> text{};
    Settings_GetRender(text.data());
    EXPECT_STREQ(text.data(), _T("RenderVfw.dll"));
    Settings_GetDebugFontName(text.data());
    EXPECT_STREQ(text.data(), _T("Lucida Console"));
    Settings_SetRender(_T("Custom.dll"));
    Settings_SetDebugFontName(_T("Consolas"));
    Settings_Init(path.data());
    Settings_GetRender(text.data());
    EXPECT_STREQ(text.data(), _T("Custom.dll"));
    Settings_GetDebugFontName(text.data());
    EXPECT_STREQ(text.data(), _T("Consolas"));
    const std::basic_string<TCHAR> oversized(32, _T('x'));
    Raw(_T("Render"), oversized.c_str());
    Settings_GetRender(text.data());
    EXPECT_STREQ(text.data(), _T("RenderVfw.dll"));
}

TEST_F(SettingsTests, DevicePathsAndPortNamesPersistIndependently)
{
    const auto unicode = _T("C:\\images\\\u0423\u041a\u041d\u0426.dsk");
    Settings_SetFloppyFilePath(0, unicode);
    Settings_SetFloppyFilePath(3, _T("four.dsk"));
    Settings_SetHardFilePath(1, _T("one.hdi"));
    Settings_SetHardFilePath(2, _T("two.hdi"));
    Settings_SetCartridgeFilePath(1, _T("one.rom"));
    Settings_SetCartridgeFilePath(2, _T("two.rom"));
    Settings_SetSerialPort(_T("COM12"));
    Settings_SetNetComPort(_T("COM34"));
    Settings_Init(path.data());
    std::array<TCHAR, MAX_PATH> text{};
    Settings_GetFloppyFilePath(0, text.data()); EXPECT_STREQ(text.data(), unicode);
    Settings_GetFloppyFilePath(3, text.data()); EXPECT_STREQ(text.data(), _T("four.dsk"));
    Settings_GetFloppyFilePath(1, text.data()); EXPECT_STREQ(text.data(), _T(""));
    Settings_GetHardFilePath(1, text.data()); EXPECT_STREQ(text.data(), _T("one.hdi"));
    Settings_GetHardFilePath(2, text.data()); EXPECT_STREQ(text.data(), _T("two.hdi"));
    Settings_GetCartridgeFilePath(1, text.data()); EXPECT_STREQ(text.data(), _T("one.rom"));
    Settings_GetCartridgeFilePath(2, text.data()); EXPECT_STREQ(text.data(), _T("two.rom"));
    Settings_GetSerialPort(text.data()); EXPECT_STREQ(text.data(), _T("COM12"));
    Settings_GetNetComPort(text.data()); EXPECT_STREQ(text.data(), _T("COM34"));
}

TEST_F(SettingsTests, BreakpointSlotsAndProcessorsPersistIndependentlyAndDelete)
{
    for (int slot = 0; slot < 16; ++slot)
    {
        EXPECT_EQ(Settings_GetDebugBreakpoint(slot, TRUE), 0177777);
        Settings_SetDebugBreakpoint(slot, TRUE, static_cast<WORD>(01000 + slot));
        Settings_SetDebugBreakpoint(slot, FALSE, static_cast<WORD>(02000 + slot));
    }
    Settings_Init(path.data());
    for (int slot = 0; slot < 16; ++slot)
    {
        EXPECT_EQ(Settings_GetDebugBreakpoint(slot, TRUE), 01000 + slot);
        EXPECT_EQ(Settings_GetDebugBreakpoint(slot, FALSE), 02000 + slot);
        Settings_SetDebugBreakpoint(slot, TRUE, 0177777);
        EXPECT_EQ(Settings_GetDebugBreakpoint(slot, TRUE), 0177777);
        EXPECT_EQ(Settings_GetDebugBreakpoint(slot, FALSE), 02000 + slot);
    }
}

TEST_F(SettingsTests, ColorsDefaultCachePersistAndResetOnSwitch)
{
    for (int i = 0; i < ColorIndicesCount; ++i)
    {
        const auto index = static_cast<ColorIndices>(i);
        EXPECT_EQ(Settings_GetColor(index), Settings_GetDefaultColor(index));
        EXPECT_NE(Settings_GetColorFriendlyName(index), nullptr);
        Settings_SetColor(index, RGB(i, 0x34, 0xab));
    }
    Settings_Init(path.data());
    for (int i = 0; i < ColorIndicesCount; ++i)
        EXPECT_EQ(Settings_GetColor(static_cast<ColorIndices>(i)), RGB(i, 0x34, 0xab));
    Settings_Init(other.data());
    for (int i = 0; i < ColorIndicesCount; ++i)
        EXPECT_EQ(Settings_GetColor(static_cast<ColorIndices>(i)), Settings_GetDefaultColor(static_cast<ColorIndices>(i)));
}

TEST_F(SettingsTests, InvalidColorKeepsOutputAndFallsBackToDefault)
{
    Raw(_T("ColorDebugText"), _T("123456junk"));
    COLORREF color = 0xabcdef;
    EXPECT_FALSE(Settings_LoadColorValue(_T("ColorDebugText"), &color));
    EXPECT_EQ(color, 0xabcdefu);
    EXPECT_EQ(Settings_GetColor(static_cast<ColorIndices>(0)), Settings_GetDefaultColor(static_cast<ColorIndices>(0)));
    Raw(_T("ColorDebugText"), _T("1234ab"));
    EXPECT_EQ(Settings_GetColor(static_cast<ColorIndices>(0)), Settings_GetDefaultColor(static_cast<ColorIndices>(0)));
    Settings_Init(path.data());
    EXPECT_EQ(Settings_GetColor(static_cast<ColorIndices>(0)), RGB(0x12, 0x34, 0xab));
    EXPECT_EQ(Settings_GetColor(static_cast<ColorIndices>(-1)), 0u);
    EXPECT_EQ(Settings_GetDefaultColor(ColorIndicesCount), 0u);
    Settings_SetColor(ColorIndicesCount, 0);
}

TEST_F(SettingsTests, SerialAndNetworkDefaultsAreDistinct)
{
    DCB serial{}, network{};
    Settings_GetSerialConfig(&serial);
    Settings_GetNetComConfig(&network);
    EXPECT_EQ(serial.DCBlength, sizeof(DCB));
    EXPECT_EQ(network.DCBlength, sizeof(DCB));
    EXPECT_EQ(serial.BaudRate, 9600u);
    EXPECT_EQ(network.BaudRate, 57600u);
    EXPECT_EQ(serial.ByteSize, 8);
    EXPECT_EQ(network.ByteSize, 8);
    EXPECT_EQ(serial.Parity, NOPARITY);
    EXPECT_EQ(network.Parity, ODDPARITY);
    EXPECT_EQ(serial.StopBits, TWOSTOPBITS);
    EXPECT_EQ(network.StopBits, TWOSTOPBITS);
    EXPECT_EQ(static_cast<DWORD>(serial.fRtsControl), RTS_CONTROL_HANDSHAKE);
    EXPECT_EQ(static_cast<DWORD>(network.fRtsControl), RTS_CONTROL_DISABLE);
}

TEST_F(SettingsTests, ConfigurationsPersistCacheAndResetAfterFileSwitch)
{
    DCB serial{}, network{};
    Settings_GetSerialConfig(&serial);
    Settings_GetNetComConfig(&network);
    serial.BaudRate = 19200;
    network.BaudRate = 115200;
    Settings_SetSerialConfig(&serial);
    Settings_SetNetComConfig(&network);
    Raw(_T("SerialConfig"), _T("invalid"));
    DCB actual{};
    Settings_GetSerialConfig(&actual);
    EXPECT_EQ(actual.BaudRate, 19200u);
    Settings_Init(path.data());
    Settings_GetSerialConfig(&actual);
    EXPECT_EQ(actual.BaudRate, 9600u);
    Settings_GetNetComConfig(&actual);
    EXPECT_EQ(std::memcmp(&actual, &network, sizeof(DCB)), 0);
    Settings_Init(other.data());
    Settings_GetNetComConfig(&actual);
    EXPECT_EQ(actual.BaudRate, 57600u);
}

struct CachedSetting
{
    const char* name;
    const TCHAR* key;
    DWORD defaultValue;
    std::function<DWORD()> get;
    std::function<void(DWORD)> set;
};
class CachedSettingsTests : public SettingsTests, public testing::WithParamInterface<CachedSetting> {};

TEST_P(CachedSettingsTests, MissingAndMalformedValuesUseDefault)
{
    const auto& setting = GetParam();
    EXPECT_EQ(setting.get(), setting.defaultValue);
    for (const auto value : {_T("junk"), _T("1junk"), _T("4294967296"), _T("-1")})
    {
        Raw(setting.key, value);
        Settings_Init(path.data());
        EXPECT_EQ(setting.get(), setting.defaultValue);
    }
}

TEST_P(CachedSettingsTests, SetterPersistsAndCacheReloadsOnInitialization)
{
    const auto& setting = GetParam();
    setting.set(7);
    EXPECT_EQ(setting.get(), 7u);
    Settings_Init(path.data());
    EXPECT_EQ(setting.get(), 7u);
    Raw(setting.key, _T("9"));
    EXPECT_EQ(setting.get(), 7u);
    Settings_Init(path.data());
    EXPECT_EQ(setting.get(), 9u);
    Settings_Init(other.data());
    EXPECT_EQ(setting.get(), setting.defaultValue);
    Settings_Init(path.data());
    EXPECT_EQ(setting.get(), 9u);
}

INSTANTIATE_TEST_SUITE_P(AllCachedSettings, CachedSettingsTests, testing::Values(
    CachedSetting{"WindowMaximized", _T("WindowMaximized"), FALSE, [] { return static_cast<DWORD>(Settings_GetWindowMaximized()); }, [](DWORD value) { Settings_SetWindowMaximized(static_cast<BOOL>(value)); }},
    CachedSetting{"WindowFullscreen", _T("WindowFullscreen"), FALSE, [] { return static_cast<DWORD>(Settings_GetWindowFullscreen()); }, [](DWORD value) { Settings_SetWindowFullscreen(static_cast<BOOL>(value)); }},
    CachedSetting{"ScreenViewMode", _T("ScreenViewMode"), 0, [] { return static_cast<DWORD>(Settings_GetScreenViewMode()); }, [](DWORD value) { Settings_SetScreenViewMode(static_cast<int>(value)); }},
    CachedSetting{"ScreenHeightMode", _T("ScreenHeightMode"), 0, [] { return static_cast<DWORD>(Settings_GetScreenHeightMode()); }, [](DWORD value) { Settings_SetScreenHeightMode(static_cast<int>(value)); }},
    CachedSetting{"ScreenshotMode", _T("ScreenshotMode"), 1, [] { return static_cast<DWORD>(Settings_GetScreenshotMode()); }, [](DWORD value) { Settings_SetScreenshotMode(static_cast<int>(value)); }},
    CachedSetting{"Toolbar", _T("Toolbar"), TRUE, [] { return static_cast<DWORD>(Settings_GetToolbar()); }, [](DWORD value) { Settings_SetToolbar(static_cast<BOOL>(value)); }},
    CachedSetting{"Debug", _T("Debug"), FALSE, [] { return static_cast<DWORD>(Settings_GetDebug()); }, [](DWORD value) { Settings_SetDebug(static_cast<BOOL>(value)); }},
    CachedSetting{"DebugCpuPpu", _T("DebugCpuPpu"), FALSE, [] { return static_cast<DWORD>(Settings_GetDebugCpuPpu()); }, [](DWORD value) { Settings_SetDebugCpuPpu(static_cast<BOOL>(value)); }},
    CachedSetting{"DebugMemoryMode", _T("DebugMemoryMode"), 3, [] { return static_cast<DWORD>(Settings_GetDebugMemoryMode()); }, [](DWORD value) { Settings_SetDebugMemoryMode(static_cast<WORD>(value)); }},
    CachedSetting{"DebugMemoryAddress", _T("DebugMemoryAddress"), 0, [] { return static_cast<DWORD>(Settings_GetDebugMemoryAddress()); }, [](DWORD value) { Settings_SetDebugMemoryAddress(static_cast<WORD>(value)); }},
    CachedSetting{"DebugMemoryBase", _T("DebugMemoryBase"), 0, [] { return static_cast<DWORD>(Settings_GetDebugMemoryBase()); }, [](DWORD value) { Settings_SetDebugMemoryBase(static_cast<WORD>(value)); }},
    CachedSetting{"DebugMemoryByte", _T("DebugMemoryByte"), FALSE, [] { return static_cast<DWORD>(Settings_GetDebugMemoryByte()); }, [](DWORD value) { Settings_SetDebugMemoryByte(static_cast<BOOL>(value)); }},
    CachedSetting{"DebugMemoryNumeral", _T("DebugMemoryNumeral"), 0, [] { return static_cast<DWORD>(Settings_GetDebugMemoryNumeral()); }, [](DWORD value) { Settings_SetDebugMemoryNumeral(static_cast<WORD>(value)); }},
    CachedSetting{"Autostart", _T("Autostart"), FALSE, [] { return static_cast<DWORD>(Settings_GetAutostart()); }, [](DWORD value) { Settings_SetAutostart(static_cast<BOOL>(value)); }},
    CachedSetting{"RealSpeed", _T("RealSpeed"), 1, [] { return static_cast<DWORD>(Settings_GetRealSpeed()); }, [](DWORD value) { Settings_SetRealSpeed(static_cast<WORD>(value)); }},
    CachedSetting{"Sound", _T("Sound"), FALSE, [] { return static_cast<DWORD>(Settings_GetSound()); }, [](DWORD value) { Settings_SetSound(static_cast<BOOL>(value)); }},
    CachedSetting{"SoundVolume", _T("SoundVolume"), 0x3fff, [] { return static_cast<DWORD>(Settings_GetSoundVolume()); }, [](DWORD value) { Settings_SetSoundVolume(static_cast<WORD>(value)); }},
    CachedSetting{"SoundAY", _T("SoundAY"), FALSE, [] { return static_cast<DWORD>(Settings_GetSoundAY()); }, [](DWORD value) { Settings_SetSoundAY(static_cast<BOOL>(value)); }},
    CachedSetting{"SoundCovox", _T("SoundCovox"), FALSE, [] { return static_cast<DWORD>(Settings_GetSoundCovox()); }, [](DWORD value) { Settings_SetSoundCovox(static_cast<BOOL>(value)); }},
    CachedSetting{"Mouse", _T("Mouse"), FALSE, [] { return static_cast<DWORD>(Settings_GetMouse()); }, [](DWORD value) { Settings_SetMouse(static_cast<BOOL>(value)); }},
    CachedSetting{"Keyboard", _T("Keyboard"), TRUE, [] { return static_cast<DWORD>(Settings_GetKeyboard()); }, [](DWORD value) { Settings_SetKeyboard(static_cast<BOOL>(value)); }},
    CachedSetting{"Tape", _T("Tape"), FALSE, [] { return static_cast<DWORD>(Settings_GetTape()); }, [](DWORD value) { Settings_SetTape(static_cast<BOOL>(value)); }},
    CachedSetting{"InstrHistory", _T("InstrHistory"), FALSE, [] { return static_cast<DWORD>(Settings_GetInstrHistory()); }, [](DWORD value) { Settings_SetInstrHistory(static_cast<BOOL>(value)); }},
    CachedSetting{"Serial", _T("Serial"), FALSE, [] { return static_cast<DWORD>(Settings_GetSerial()); }, [](DWORD value) { Settings_SetSerial(static_cast<BOOL>(value)); }},
    CachedSetting{"Parallel", _T("Parallel"), FALSE, [] { return static_cast<DWORD>(Settings_GetParallel()); }, [](DWORD value) { Settings_SetParallel(static_cast<BOOL>(value)); }},
    CachedSetting{"Network", _T("Network"), FALSE, [] { return static_cast<DWORD>(Settings_GetNetwork()); }, [](DWORD value) { Settings_SetNetwork(static_cast<BOOL>(value)); }},
    CachedSetting{"NetStation", _T("NetStation"), 0, [] { return static_cast<DWORD>(Settings_GetNetStation()); }, [](DWORD value) { Settings_SetNetStation(static_cast<int>(value)); }},
    CachedSetting{"SpriteAddress", _T("SpriteAddress"), 0, [] { return static_cast<DWORD>(Settings_GetSpriteAddress()); }, [](DWORD value) { Settings_SetSpriteAddress(static_cast<WORD>(value)); }},
    CachedSetting{"SpriteWidth", _T("SpriteWidth"), 2, [] { return static_cast<DWORD>(Settings_GetSpriteWidth()); }, [](DWORD value) { Settings_SetSpriteWidth(static_cast<WORD>(value)); }},
    CachedSetting{"SpriteMode", _T("SpriteMode"), 0, [] { return static_cast<DWORD>(Settings_GetSpriteMode()); }, [](DWORD value) { Settings_SetSpriteMode(static_cast<WORD>(value)); }}
), [](const testing::TestParamInfo<CachedSetting>& info) { return info.param.name; });
}
