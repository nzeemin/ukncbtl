/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

enum DisasmSubtitleType
{
    SUBTYPE_NONE = 0, SUBTYPE_COMMENT = 1, SUBTYPE_BLOCKCOMMENT = 2, SUBTYPE_DATA = 4,
};
struct DisasmSubtitleItem
{
    uint16_t address;
    int type;
    std::wstring comment;
};
namespace subtitles
{
enum class ParseError { Empty, BigEndian, InvalidAddress };
// Owns comment strings; input may be released or changed after parsing.
std::expected<std::vector<DisasmSubtitleItem>, ParseError> Parse(std::wstring_view text);
const DisasmSubtitleItem* Find(std::span<const DisasmSubtitleItem> items, uint16_t address, int mask);
}
