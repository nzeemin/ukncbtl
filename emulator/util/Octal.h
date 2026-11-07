/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once
#include <cstdint>
#include <expected>
#include <limits>
#include <string_view>
#include <tchar.h>

enum class OctalError { empty, invalid_digit, out_of_range };

[[nodiscard]] inline std::expected<uint16_t, OctalError> ParseOctalValue(std::basic_string_view<TCHAR> text)
{
    if (text.empty()) return std::unexpected(OctalError::empty);
    uint32_t value = 0;
    for (const TCHAR ch : text)
    {
        if (ch < _T('0') || ch > _T('7')) return std::unexpected(OctalError::invalid_digit);
        const auto digit = static_cast<uint32_t>(ch - _T('0'));
        if (value > ((std::numeric_limits<uint16_t>::max)() - digit) / 8)
            return std::unexpected(OctalError::out_of_range);
        value = value * 8 + digit;
    }
    return static_cast<uint16_t>(value);
}
