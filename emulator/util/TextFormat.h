/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once

#include <format>
#include <span>
#include <utility>

// Checked formatting for fixed Win32 text buffers. A nonempty destination is
// always terminated, including when the formatted text has to be truncated.
template<class... Args>
void FormatBuffer(std::span<wchar_t> buffer, std::wformat_string<Args...> format, Args&&... args)
{
    if (buffer.empty()) return;
    const auto result = std::format_to_n(buffer.data(), buffer.size() - 1,
        format, std::forward<Args>(args)...);
    *result.out = L'\0';
}
