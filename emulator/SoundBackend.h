/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once
#include <Windows.h>
#include <mmsystem.h>

namespace sound
{
// The default backend is WinMM. Substitutes must preserve WinMM callback and
// header-lifetime semantics. Selection is allowed only while sound is closed.
struct Backend
{
    decltype(&waveOutOpen) open = &waveOutOpen;
    decltype(&waveOutClose) close = &waveOutClose;
    decltype(&waveOutPrepareHeader) prepare = &waveOutPrepareHeader;
    decltype(&waveOutUnprepareHeader) unprepare = &waveOutUnprepareHeader;
    decltype(&waveOutWrite) write = &waveOutWrite;
    decltype(&waveOutSetVolume) volume = &waveOutSetVolume;
    decltype(&waveOutSetPlaybackRate) rate = &waveOutSetPlaybackRate;
    decltype(&Sleep) wait = &Sleep;
};
bool SetBackend(const Backend& backend);
}
