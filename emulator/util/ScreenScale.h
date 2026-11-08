/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once

#include <cstdint>
#include <span>
#include <tchar.h>

namespace screen_scale {
struct Mode
{
    int width;
    int height;
    const TCHAR* description;
};

[[nodiscard]] std::span<const Mode> Modes();
// Out-of-range mode numbers use the default 640 x 432 mode.
[[nodiscard]] const Mode& GetMode(int mode);
[[nodiscard]] bool Apply(int mode, std::span<const uint32_t> source, std::span<uint32_t> destination);
}
