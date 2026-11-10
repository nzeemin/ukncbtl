/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once
#include <cstdint>
#include <functional>
#include <tchar.h>

class CMotherboard;
class InstructionHistory;

namespace snapshot {
bool Save(const TCHAR* path, CMotherboard& board, uint32_t uptime);
// Validation completes before stop or any state mutation. stop performs the
// application's run-state/UI transition; the remaining restoration is headless.
bool Load(const TCHAR* path, CMotherboard& board, uint32_t& uptime,
          InstructionHistory& history, const std::function<void()>& stop);
}
