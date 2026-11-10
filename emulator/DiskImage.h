/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once
#include <Windows.h>
#include <cstdint>
#include <expected>
#include <span>

namespace disk_image
{
enum class ErrorKind { InvalidArgument, Open, Write, Close };
struct Error { ErrorKind kind; DWORD code; };
struct Backend
{
    decltype(&CreateFileW) open = &CreateFileW;
    decltype(&WriteFile) write = &WriteFile;
    decltype(&CloseHandle) close = &CloseHandle;
    decltype(&GetLastError) lastError = &GetLastError;
};
size_t Size(int tracks);
// Invalid tracks or short output leave the buffer untouched. Extra bytes are preserved.
bool Build(int tracks, std::span<uint8_t> output);
// Supports 40/80 tracks. Existing files are replaced; failed writes may leave a partial file.
std::expected<void, Error> Create(const wchar_t* path, int tracks, const Backend& backend = {});
}
