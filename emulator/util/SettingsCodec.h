/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tchar.h>

namespace settings_codec {
using Text = std::basic_string<TCHAR>;
using TextView = std::basic_string_view<TCHAR>;

inline int HexDigit(TCHAR ch)
{
    if (ch >= _T('0') && ch <= _T('9')) return ch - _T('0');
    if (ch >= _T('A') && ch <= _T('F')) return ch - _T('A') + 10;
    if (ch >= _T('a') && ch <= _T('f')) return ch - _T('a') + 10;
    return -1;
}

inline std::optional<uint32_t> ParseUnsigned(TextView text, uint32_t base, uint32_t maximum)
{
    if (text.empty()) return std::nullopt;
    uint32_t value = 0;
    for (const auto ch : text)
    {
        const int digit = HexDigit(ch);
        if (digit < 0 || static_cast<uint32_t>(digit) >= base ||
            value > (maximum - static_cast<uint32_t>(digit)) / base)
            return std::nullopt;
        value = value * base + digit;
    }
    return value;
}

inline std::optional<uint32_t> DecodeDword(TextView text)
{ return ParseUnsigned(text, 10, (std::numeric_limits<uint32_t>::max)()); }

inline uint32_t SwapRedBlue(uint32_t color)
{ return ((color & 0xff) << 16) | (color & 0xff00) | ((color >> 16) & 0xff); }

inline Text EncodeColor(uint32_t color)
{
    constexpr TextView hex = _T("0123456789ABCDEF");
    const auto rgb = SwapRedBlue(color);
    Text text(6, _T('0'));
    for (size_t i = 0; i < text.size(); ++i)
        text[i] = hex[(rgb >> ((5 - i) * 4)) & 15];
    return text;
}

inline std::optional<uint32_t> DecodeColor(TextView text)
{
    const auto rgb = ParseUnsigned(text, 16, 0xffffff);
    if (!rgb) return std::nullopt;
    return SwapRedBlue(*rgb);
}

inline Text EncodeBinary(std::span<const uint8_t> bytes)
{
    constexpr TextView hex = _T("0123456789ABCDEF");
    Text text;
    text.reserve(bytes.size() * 2);
    for (const auto byte : bytes)
    {
        text.push_back(hex[byte >> 4]);
        text.push_back(hex[byte & 15]);
    }
    return text;
}

inline bool DecodeBinary(TextView text, std::span<uint8_t> bytes)
{
    if (text.size() % 2 != 0 || text.size() / 2 != bytes.size()) return false;
    // Validate first: malformed settings must leave the destination unchanged.
    for (const auto ch : text) if (HexDigit(ch) < 0) return false;
    for (size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<uint8_t>((HexDigit(text[i * 2]) << 4) | HexDigit(text[i * 2 + 1]));
    return true;
}
}
