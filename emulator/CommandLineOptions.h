/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once
#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tchar.h>

struct CommandLineOptions
{
    bool help = false;
    int boot = 0;
    std::optional<bool> autostart, debug, sound, fullscreen;
    std::array<std::optional<std::basic_string<TCHAR>>, 4> disks;
    std::array<std::optional<std::basic_string<TCHAR>>, 2> cartridges, hardDrives;
};

// Arguments are already tokenized; the executable name is excluded. Unknown or
// malformed options have no effect. For repeated options, the last valid wins.
inline CommandLineOptions ParseCommandLineOptions(std::span<const std::basic_string_view<TCHAR>> arguments)
{
    CommandLineOptions options;
    for (const auto arg : arguments)
    {
        if (arg == _T("/help") || arg == _T("/h")) options.help = true;
        else if (arg == _T("/boot")) options.boot = 1;
        else if (arg.size() == 6 && arg.starts_with(_T("/boot")) && arg[5] >= _T('1') && arg[5] <= _T('7'))
            options.boot = arg[5] - _T('0');
        else if (arg == _T("/autostart") || arg == _T("/autostarton")) options.autostart = true;
        else if (arg == _T("/autostartoff") || arg == _T("/noautostart")) options.autostart = false;
        else if (arg == _T("/debug") || arg == _T("/debugon") || arg == _T("/debugger")) options.debug = true;
        else if (arg == _T("/debugoff") || arg == _T("/nodebug")) options.debug = false;
        else if (arg == _T("/sound") || arg == _T("/soundon")) options.sound = true;
        else if (arg == _T("/soundoff") || arg == _T("/nosound")) options.sound = false;
        else if (arg == _T("/fullscreen") || arg == _T("/fullscreenon")) options.fullscreen = true;
        else if (arg == _T("/windowed") || arg == _T("/fullscreenoff")) options.fullscreen = false;
        else if (arg.size() > 7 && arg[6] == _T(':'))
        {
            if (arg.starts_with(_T("/disk")) && arg[5] >= _T('0') && arg[5] <= _T('3'))
                options.disks[arg[5] - _T('0')] = arg.substr(7);
            else if (arg.starts_with(_T("/cart")) && arg[5] >= _T('1') && arg[5] <= _T('2'))
                options.cartridges[arg[5] - _T('1')] = arg.substr(7);
            else if (arg.starts_with(_T("/hard")) && arg[5] >= _T('1') && arg[5] <= _T('2'))
                options.hardDrives[arg[5] - _T('1')] = arg.substr(7);
        }
    }
    return options;
}
