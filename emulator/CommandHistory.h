/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include <tchar.h>

class CommandHistory
{
public:
    static constexpr size_t Capacity = 32;
    void ResetNavigation() { position_ = -1; }
    void Add(const TCHAR* text)
    {
        ResetNavigation();
        if (!*text) return;
        std::erase(commands_, text);
        commands_.emplace_back(text);
        if (commands_.size() > Capacity) commands_.erase(commands_.begin());
    }
    void Up()
    {
        if (position_ + 1 < static_cast<int>(commands_.size())) ++position_;
    }
    bool Down()
    {
        if (position_ < 0) return false;
        --position_;
        return true;
    }
    [[nodiscard]] const TCHAR* Current() const
    { return position_ < 0 ? _T("") : commands_[commands_.size() - 1 - position_].c_str(); }
    [[nodiscard]] size_t Size() const { return commands_.size(); }
private:
    std::vector<std::basic_string<TCHAR>> commands_;
    int position_ = -1;
};
