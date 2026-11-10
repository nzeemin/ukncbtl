/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once
#include "util/WavPcmFile.h"
#include <algorithm>
#include <optional>
#include <span>

namespace tape
{
class State
{
public:
    void Insert(bool recording) { inserted_ = true; recording_ = recording; playing_ = false; }
    void Eject() { inserted_ = recording_ = playing_ = false; }
    bool Play() { if (!inserted_ || playing_) return false; playing_ = true; return true; }
    bool Stop() { const bool wasPlaying = playing_; playing_ = false; return wasPlaying; }
    bool Inserted() const { return inserted_; }
    bool Recording() const { return recording_; }
    bool Playing() const { return playing_; }
private:
    bool inserted_ = false, recording_ = false, playing_ = false;
};
struct BatchResult
{
    unsigned processed = 0;
    bool bit = false;
    bool stop = false;
    std::optional<WavError> error;
};
inline void FinishGraph(std::span<uint8_t> graph, unsigned count)
{
    if (!graph.empty()) std::rotate(graph.begin(), graph.begin() + count % graph.size(), graph.end());
}
template<class Stream>
BatchResult Read(Stream& stream, unsigned samples, std::span<uint8_t> graph)
{
    BatchResult result;
    if (samples == 0) return result;
    for (unsigned i = 0; i < samples; ++i)
    {
        const auto sample = stream.ReadOne();
        if (!sample)
        {
            result.stop = true;
            if (sample.error() != WavError::end_of_data) result.error = sample.error();
            break;
        }
        result.bit = *sample >= 0x80000000u;
        if (!graph.empty()) graph[result.processed % graph.size()] = static_cast<uint8_t>(*sample >> 24);
        ++result.processed;
    }
    FinishGraph(graph, result.processed);
    result.stop |= stream.Position() >= stream.Length();
    return result;
}
template<class Stream>
BatchResult Write(Stream& stream, uint32_t value, unsigned samples, std::span<uint8_t> graph)
{
    BatchResult result;
    for (unsigned i = 0; i < samples; ++i)
    {
        const auto written = stream.WriteOne(value);
        if (!written)
        {
            result.stop = true;
            result.error = written.error();
            break;
        }
        if (!graph.empty()) graph[result.processed % graph.size()] = static_cast<uint8_t>(value >> 24);
        ++result.processed;
    }
    FinishGraph(graph, result.processed);
    return result;
}
inline uint64_t Centiseconds(uint32_t position, int frequency)
{ return frequency > 0 ? static_cast<uint64_t>(position) * 100 / frequency : 0; }
inline bool UpdatePosition(uint32_t position, uint32_t shown, int frequency, bool playing)
{ return !playing || position < shown || position - shown > static_cast<uint32_t>(std::max(0, frequency) / 6); }
}
