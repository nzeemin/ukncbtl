#include "stdafx.h"
#include <gtest/gtest.h>
#include "emubase/Emubase.h"
#include "SoundGen.h"
#include "SoundBackend.h"
#include <deque>
#include <vector>

namespace {
struct FakeAudio
{
    static FakeAudio* active;
    WAVEFORMATEX format{};
    DWORD_PTR instance = 0;
    using Callback = void (CALLBACK*)(HWAVEOUT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
    Callback callback = nullptr;
    HWAVEOUT handle = reinterpret_cast<HWAVEOUT>(1);
    int opens = 0, closes = 0, prepares = 0, unprepares = 0, writes = 0, waits = 0;
    MMRESULT openError = 0, closeError = 0, prepareError = 0, unprepareError = 0, writeError = 0;
    MMRESULT volumeError = 0, rateError = 0;
    bool immediate = true;
    std::vector<DWORD> volumes, rates;
    std::vector<WAVEHDR*> headers;
    std::vector<std::vector<uint8_t>> payloads;
    std::deque<WAVEHDR*> pending;
    void Notify(UINT message, WAVEHDR* header = nullptr)
    { callback(handle, message, instance, reinterpret_cast<DWORD_PTR>(header), 0); }
    void Complete()
    {
        if (pending.empty()) return;
        auto* header = pending.front(); pending.pop_front();
        header->dwFlags &= ~WHDR_INQUEUE;
        header->dwFlags |= WHDR_DONE;
        Notify(WOM_DONE, header);
    }
    static MMRESULT WINAPI Open(LPHWAVEOUT output, UINT device, LPCWAVEFORMATEX format,
        DWORD_PTR callback, DWORD_PTR instance, DWORD flags)
    {
        auto& f = *active; ++f.opens;
        EXPECT_EQ(WAVE_MAPPER, device); EXPECT_EQ(CALLBACK_FUNCTION, flags);
        f.format = *format; f.instance = instance; f.callback = reinterpret_cast<Callback>(callback);
        if (f.openError) return f.openError;
        *output = f.handle; f.Notify(WOM_OPEN); return MMSYSERR_NOERROR;
    }
    static MMRESULT WINAPI Close(HWAVEOUT handle)
    {
        auto& f = *active; ++f.closes; EXPECT_EQ(f.handle, handle); EXPECT_TRUE(f.pending.empty());
        if (f.closeError) return f.closeError;
        f.Notify(WOM_CLOSE); return MMSYSERR_NOERROR;
    }
    static MMRESULT WINAPI Prepare(HWAVEOUT handle, LPWAVEHDR header, UINT size)
    {
        auto& f = *active; ++f.prepares;
        EXPECT_EQ(f.handle, handle); EXPECT_EQ(sizeof(WAVEHDR), size);
        EXPECT_EQ(BLOCK_SIZE, header->dwBufferLength);
        EXPECT_EQ(0, header->dwFlags & WHDR_INQUEUE);
        if (f.prepareError) return f.prepareError;
        header->dwFlags |= WHDR_PREPARED; return MMSYSERR_NOERROR;
    }
    static MMRESULT WINAPI Unprepare(HWAVEOUT handle, LPWAVEHDR header, UINT size)
    {
        auto& f = *active; ++f.unprepares;
        EXPECT_EQ(f.handle, handle); EXPECT_EQ(sizeof(WAVEHDR), size);
        EXPECT_EQ(0, header->dwFlags & WHDR_INQUEUE);
        if (f.unprepareError) return f.unprepareError;
        header->dwFlags &= ~WHDR_PREPARED; return MMSYSERR_NOERROR;
    }
    static MMRESULT WINAPI Write(HWAVEOUT handle, LPWAVEHDR header, UINT size)
    {
        auto& f = *active; ++f.writes;
        EXPECT_EQ(f.handle, handle); EXPECT_EQ(sizeof(WAVEHDR), size);
        EXPECT_NE(0, header->dwFlags & WHDR_PREPARED);
        if (f.writeError) return f.writeError;
        f.headers.push_back(header);
        const auto* data = reinterpret_cast<const uint8_t*>(header->lpData);
        f.payloads.emplace_back(data, data + header->dwBufferLength);
        header->dwFlags |= WHDR_INQUEUE;
        f.pending.push_back(header);
        if (f.immediate) f.Complete();
        return MMSYSERR_NOERROR;
    }
    static MMRESULT WINAPI Volume(HWAVEOUT handle, DWORD value)
    { EXPECT_EQ(active->handle, handle); active->volumes.push_back(value); return active->volumeError; }
    static MMRESULT WINAPI Rate(HWAVEOUT handle, DWORD value)
    { EXPECT_EQ(active->handle, handle); active->rates.push_back(value); return active->rateError; }
    static void WINAPI Wait(DWORD milliseconds)
    {
        EXPECT_EQ(1, milliseconds); ++active->waits;
        // Deterministically model the next completion; no wall-clock sleeps.
        active->Complete();
    }
    static sound::Backend Backend()
    { return {Open, Close, Prepare, Unprepare, Write, Volume, Rate, Wait}; }
};
FakeAudio* FakeAudio::active = nullptr;
}

class SoundOutput : public testing::Test
{
protected:
    FakeAudio fake;
    void SetUp() override { FakeAudio::active = &fake; ASSERT_TRUE(sound::SetBackend(FakeAudio::Backend())); }
    void TearDown() override
    {
        fake.closeError = fake.unprepareError = 0;
        while (!fake.pending.empty()) fake.Complete();
        SoundGen_Finalize();
        EXPECT_TRUE(sound::SetBackend({}));
        FakeAudio::active = nullptr;
    }
    void FeedBlock(uint16_t left = 0x1234, uint16_t right = 0xabcd)
    { for (int i = 0; i < BUFSIZE / 4; ++i) SoundGen_FeedDAC(left, right); }
};
TEST_F(SoundOutput, InactiveOperationsDoNotCallDevice)
{
    FeedBlock(); SoundGen_SetVolume(1); SoundGen_SetSpeed(200); SoundGen_Finalize();
    EXPECT_EQ(0, fake.opens); EXPECT_EQ(0, fake.writes); EXPECT_EQ(0, fake.closes);
    EXPECT_TRUE(fake.volumes.empty()); EXPECT_TRUE(fake.rates.empty());
}
TEST_F(SoundOutput, InitializesPcmFormatAndMirroredVolumeOnce)
{
    SoundGen_Initialize(0x1234); SoundGen_Initialize(0xffff);
    EXPECT_EQ(1, fake.opens); EXPECT_EQ((std::vector<DWORD>{0x12341234}), fake.volumes);
    EXPECT_EQ(WAVE_FORMAT_PCM, fake.format.wFormatTag); EXPECT_EQ(2, fake.format.nChannels);
    EXPECT_EQ(22050, fake.format.nSamplesPerSec); EXPECT_EQ(16, fake.format.wBitsPerSample);
    EXPECT_EQ(4, fake.format.nBlockAlign); EXPECT_EQ(88200, fake.format.nAvgBytesPerSec); EXPECT_EQ(0, fake.format.cbSize);
    EXPECT_FALSE(sound::SetBackend({}));
}
TEST_F(SoundOutput, FailedOpenLeavesInactiveAndCanRetry)
{
    fake.openError = MMSYSERR_ERROR; SoundGen_Initialize(1);
    FeedBlock(); SoundGen_SetSpeed(200); SoundGen_Finalize();
    EXPECT_EQ(0, fake.writes); EXPECT_EQ(0, fake.closes); EXPECT_TRUE(fake.volumes.empty());
    fake.openError = 0; SoundGen_Initialize(2); FeedBlock();
    EXPECT_EQ(2, fake.opens); EXPECT_EQ(1, fake.writes);
}
TEST_F(SoundOutput, VolumeCopiesAllSixteenBitsToBothChannels)
{
    SoundGen_Initialize(0);
    for (WORD value : {0,1,0x7fff,0x8000,0xffff})
    { SoundGen_SetVolume(value); EXPECT_EQ((DWORD{value} << 16) | value, fake.volumes.back()); }
}
TEST_F(SoundOutput, EverySpeedUsesFixedPointOrNormalFallback)
{
    SoundGen_Initialize(0);
    for (unsigned speed = 0; speed <= 65535; ++speed)
    {
        SoundGen_SetSpeed(static_cast<WORD>(speed));
        EXPECT_EQ(speed > 0 && speed < 1000 ? speed * 65536u / 100 : 65536u, fake.rates.back()) << speed;
    }
}
TEST_F(SoundOutput, ControlFailuresDoNotDisableAudio)
{
    fake.volumeError = fake.rateError = MMSYSERR_NOTSUPPORTED;
    SoundGen_Initialize(100); SoundGen_SetVolume(200); SoundGen_SetSpeed(150); FeedBlock();
    EXPECT_EQ(1, fake.writes); EXPECT_EQ(2, fake.volumes.size()); EXPECT_EQ(1, fake.rates.size());
}
TEST_F(SoundOutput, ExactBlockBoundaryAndStereoLittleEndianPacking)
{
    SoundGen_Initialize(0);
    for (unsigned i = 0; i < BUFSIZE / 4 - 1; ++i) SoundGen_FeedDAC(static_cast<WORD>(i), static_cast<WORD>(~i));
    EXPECT_EQ(0, fake.writes);
    const unsigned last = BUFSIZE / 4 - 1;
    SoundGen_FeedDAC(static_cast<WORD>(last), static_cast<WORD>(~last));
    ASSERT_EQ(1, fake.payloads.size()); ASSERT_EQ(BUFSIZE, fake.payloads[0].size());
    for (unsigned i = 0; i < BUFSIZE / 4; ++i)
    {
        EXPECT_EQ(i & 255, fake.payloads[0][i*4]); EXPECT_EQ((i >> 8) & 255, fake.payloads[0][i*4+1]);
        EXPECT_EQ((~i) & 255, fake.payloads[0][i*4+2]); EXPECT_EQ(((~i) >> 8) & 255, fake.payloads[0][i*4+3]);
    }
}
TEST_F(SoundOutput, RepeatedWrapsReuseHeadersAfterUnprepare)
{
    SoundGen_Initialize(0);
    for (int i = 0; i < 25; ++i) FeedBlock(static_cast<WORD>(i),0xffff);
    ASSERT_EQ(25, fake.headers.size()); EXPECT_EQ(17, fake.unprepares);
    for (int i = 8; i < 25; ++i) EXPECT_EQ(fake.headers[i % 8], fake.headers[i]);
    for (int i = 0; i < 25; ++i) EXPECT_EQ(i, fake.payloads[i][0]);
    SoundGen_Finalize(); EXPECT_EQ(25, fake.unprepares); EXPECT_EQ(1, fake.closes);
}
TEST_F(SoundOutput, FullQueueWaitsForCompletionAndFinalizeDrains)
{
    fake.immediate = false; SoundGen_Initialize(0);
    for (int i = 0; i < 7; ++i) FeedBlock();
    EXPECT_EQ(0, fake.waits); EXPECT_EQ(7, fake.pending.size());
    fake.Notify(WOM_OPEN); fake.Notify(WOM_CLOSE); // Neither returns a buffer.
    FeedBlock(); EXPECT_EQ(1, fake.waits); EXPECT_EQ(7, fake.pending.size());
    SoundGen_Finalize(); EXPECT_EQ(8, fake.waits); EXPECT_TRUE(fake.pending.empty());
    EXPECT_EQ(8, fake.unprepares); EXPECT_EQ(1, fake.closes);
}
TEST_F(SoundOutput, PrepareFailureDropsBlockAndNextBlockRecovers)
{
    SoundGen_Initialize(0); fake.prepareError = MMSYSERR_ERROR; FeedBlock();
    EXPECT_EQ(0, fake.writes); EXPECT_EQ(0, fake.waits);
    fake.prepareError = 0; FeedBlock(0x5678,0);
    ASSERT_EQ(1, fake.payloads.size()); EXPECT_EQ(0x78, fake.payloads[0][0]);
    SoundGen_Finalize(); EXPECT_EQ(1, fake.unprepares); EXPECT_EQ(0, fake.waits);
}
TEST_F(SoundOutput, WriteFailureRestoresFreeCountAndReusesPreparedHeader)
{
    SoundGen_Initialize(0); fake.writeError = MMSYSERR_ERROR;
    for (int i = 0; i < 10; ++i) FeedBlock();
    EXPECT_EQ(0, fake.waits); EXPECT_TRUE(fake.payloads.empty());
    fake.writeError = 0; FeedBlock(); EXPECT_EQ(10, fake.unprepares);
    SoundGen_Finalize(); EXPECT_EQ(0, fake.waits); EXPECT_EQ(1, fake.closes);
}
TEST_F(SoundOutput, ReuseUnprepareFailureDoesNotOverwriteDeviceBuffer)
{
    SoundGen_Initialize(0); for (int i = 0; i < 8; ++i) FeedBlock(0x1234,0);
    auto* header = fake.headers[0]; const std::vector<char> before(header->lpData,header->lpData+BLOCK_SIZE);
    fake.unprepareError = WAVERR_STILLPLAYING; FeedBlock(0xffff,0xffff);
    EXPECT_EQ(8, fake.writes); EXPECT_EQ(before,std::vector<char>(header->lpData,header->lpData+BLOCK_SIZE));
    fake.unprepareError = 0; FeedBlock(0x5678,0);
    ASSERT_EQ(9, fake.payloads.size()); EXPECT_EQ(0x78,fake.payloads.back()[0]);
}
TEST_F(SoundOutput, FinalizeUnprepareFailureKeepsDeviceForRetry)
{
    SoundGen_Initialize(0); FeedBlock(); fake.unprepareError = WAVERR_STILLPLAYING;
    SoundGen_Finalize(); EXPECT_EQ(0,fake.closes); EXPECT_FALSE(sound::SetBackend({}));
    fake.unprepareError = 0; SoundGen_Finalize(); EXPECT_EQ(1,fake.closes);
    EXPECT_TRUE(sound::SetBackend(FakeAudio::Backend()));
}
TEST_F(SoundOutput, CloseFailureKeepsDeviceForRetry)
{
    SoundGen_Initialize(0); FeedBlock(); fake.closeError = MMSYSERR_ERROR;
    SoundGen_Finalize(); EXPECT_EQ(1,fake.closes); EXPECT_FALSE(sound::SetBackend({}));
    fake.closeError = 0; SoundGen_Finalize(); EXPECT_EQ(2,fake.closes); EXPECT_EQ(1,fake.unprepares);
}
TEST_F(SoundOutput, FinalizeIsIdempotentAndReinitializeDropsPartialBlock)
{
    SoundGen_Initialize(0); SoundGen_FeedDAC(0xffff,0xffff);
    SoundGen_Finalize(); SoundGen_Finalize(); EXPECT_EQ(1,fake.closes); EXPECT_EQ(0,fake.writes);
    SoundGen_Initialize(0); FeedBlock(0,0);
    ASSERT_EQ(1,fake.payloads.size());
    for (auto byte : fake.payloads[0]) EXPECT_EQ(0,byte);
    EXPECT_EQ(2,fake.opens);
}
TEST_F(SoundOutput, RejectsIncompleteBackend)
{
    for (int i = 0; i < 8; ++i)
    {
        auto backend = FakeAudio::Backend();
        switch (i)
        {
        case 0: backend.open = nullptr; break; case 1: backend.close = nullptr; break;
        case 2: backend.prepare = nullptr; break; case 3: backend.unprepare = nullptr; break;
        case 4: backend.write = nullptr; break; case 5: backend.volume = nullptr; break;
        case 6: backend.rate = nullptr; break; case 7: backend.wait = nullptr; break;
        }
        EXPECT_FALSE(sound::SetBackend(backend));
    }
    SoundGen_Initialize(0); EXPECT_EQ(1,fake.opens);
}
