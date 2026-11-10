/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include "Subtitles.h"
#include "util/Octal.h"

namespace subtitles
{
std::expected<std::vector<DisasmSubtitleItem>, ParseError> Parse(std::wstring_view text)
{
    text = text.substr(0, text.find(L'\0'));
    if (text.empty()) return std::unexpected(ParseError::Empty);
    if (text.front() == 0xfffe) return std::unexpected(ParseError::BigEndian);
    if (text.front() == 0xfeff) text.remove_prefix(1);
    std::vector<DisasmSubtitleItem> items;
    std::wstring block;
    while (!text.empty())
    {
        const auto end = text.find_first_of(L"\r\n");
        auto line = text.substr(0, end);
        text.remove_prefix(end == text.npos ? text.size() : end + 1);
        if (line.empty()) continue;
        if (line.front() < L'0' || line.front() > L'9')
        {
            block = line.front() == L';' ? std::wstring(line) : std::wstring{};
            continue;
        }
        const auto digits = line.find_first_not_of(L"0123456789");
        const auto address = ParseOctalValue(line.substr(0, digits));
        if (!address) return std::unexpected(ParseError::InvalidAddress);
        if (!block.empty())
        {
            items.push_back({*address, SUBTYPE_BLOCKCOMMENT, std::move(block)});
            block.clear();
        }
        line.remove_prefix(digits == line.npos ? line.size() : digits);
        const auto content = line.find_first_not_of(L" \t$:");
        line.remove_prefix(content == line.npos ? line.size() : content);
        const bool data = !line.empty() && line.front() == L'.';
        const auto comment = line.find(L';');
        if (comment != line.npos)
            items.push_back({*address, SUBTYPE_COMMENT | (data ? SUBTYPE_DATA : 0), std::wstring(line.substr(comment))});
        else if (data)
            items.push_back({*address, SUBTYPE_DATA, {}});
    }
    return items;
}

const DisasmSubtitleItem* Find(std::span<const DisasmSubtitleItem> items, uint16_t address, int mask)
{
    for (const auto& item : items)
        if (item.address == address && (item.type & mask) != 0) return &item;
    return nullptr;
}
}
