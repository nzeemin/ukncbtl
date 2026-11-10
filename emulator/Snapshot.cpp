/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include "Snapshot.h"
#include "InstructionHistory.h"
#include "emubase/Board.h"
#include "emubase/ImageSerialization.h"
#include "util/File.h"
#include <cstdio>
#include <share.h>
#include <vector>

namespace snapshot {
bool Save(const TCHAR* path, CMotherboard& board, uint32_t uptime)
{
    if (!path) return false;
    std::vector<uint8_t> bytes(UKNCIMAGE_SIZE, 0);
    image::Writer header(std::span{bytes}.first<20>());
    header.dword(UKNCIMAGE_HEADER1);
    header.dword(UKNCIMAGE_HEADER2);
    header.dword(UKNCIMAGE_VERSION);
    header.dword(UKNCIMAGE_SIZE);
    header.dword(uptime);
    board.SaveToImage(std::span<uint8_t, UKNCIMAGE_SIZE>{bytes});

    UniqueFile file(::_tfsopen(path, _T("w+b"), _SH_DENYWR));
    if (!file) return false;
    const auto written = std::fwrite(bytes.data(), 1, bytes.size(), file.get());
    const auto closed = std::fclose(file.release());
    return written == bytes.size() && closed == 0;
}

bool Load(const TCHAR* path, CMotherboard& board, uint32_t& uptime,
          InstructionHistory& history, const std::function<void()>& stop)
{
    if (!path) return false;
    UniqueFile file(::_tfsopen(path, _T("rb"), _SH_DENYWR));
    if (!file) return false;
    std::vector<uint8_t> bytes(UKNCIMAGE_SIZE);
    if (std::fread(bytes.data(), 1, bytes.size(), file.get()) != bytes.size()) return false;

    image::Reader header(std::span<const uint8_t>{bytes}.first<20>());
    if (header.dword() != UKNCIMAGE_HEADER1 || header.dword() != UKNCIMAGE_HEADER2 ||
        header.dword() != UKNCIMAGE_VERSION || header.dword() != UKNCIMAGE_SIZE)
        return false;
    const auto savedUptime = header.dword();

    stop();
    board.Reset();
    board.LoadFromImage(std::span<const uint8_t, UKNCIMAGE_SIZE>{bytes});
    history.Clear();
    uptime = savedUptime;
    return true;
}
}
