/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once

#include <cstdio>
#include <memory>

struct FileCloser
{
    void operator()(std::FILE* file) const noexcept { std::fclose(file); }
};

// Call fclose(file.release()) explicitly when the close result must be checked.
using UniqueFile = std::unique_ptr<std::FILE, FileCloser>;
