/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include <benchmark/benchmark.h>
#include "Workloads.h"
#include "util/ScreenScale.h"
#include <vector>

// Headless replacements for the application's UI logging hooks.
#ifdef _DEBUG
BOOL AssertFailedLine(LPCSTR file, int line)
{
    std::fprintf(stderr, "Core assertion at %s:%d\n", file, line);
    std::abort();
}
#endif
void DebugLog(LPCTSTR) {}
void DebugLogFormat(LPCTSTR, ...) {}
const TCHAR* REGISTER_NAME[] = { _T("R0"), _T("R1"), _T("R2"), _T("R3"),
                                _T("R4"), _T("R5"), _T("SP"), _T("PC") };

static void BM_CpuInstructions(benchmark::State& state)
{
    CMotherboard board;
    workloads::Prepare(board);
    auto* cpu = board.GetCPU();
    constexpr int batch = 1024;
    for (auto _ : state)
    {
        for (int i = 0; i < batch; ++i) cpu->CommandExecution();
        benchmark::DoNotOptimize(cpu->GetReg(1));
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * batch);
}
BENCHMARK(BM_CpuInstructions);

static void BM_SystemFrame(benchmark::State& state)
{
    CMotherboard board;
    workloads::Prepare(board);
    for (auto _ : state)
    {
        if (!board.SystemFrame())
        {
            state.SkipWithError("Unexpected breakpoint in frame workload");
            break;
        }
        benchmark::DoNotOptimize(board.GetCPU()->GetReg(1));
        benchmark::DoNotOptimize(board.GetPPU()->GetReg(1));
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations()); // Emulated 20 ms frames, not instructions.
    state.SetLabel("CPU+PPU, synthetic RAM loop, no host I/O");
}
BENCHMARK(BM_SystemFrame)->Unit(benchmark::kMicrosecond);

static void BM_ScreenScale(benchmark::State& state)
{
    const int mode = static_cast<int>(state.range(0));
    const auto& dimensions = screen_scale::GetMode(mode);
    std::vector<uint32_t> source(UKNC_SCREEN_WIDTH * UKNC_SCREEN_HEIGHT);
    for (size_t i = 0; i < source.size(); ++i)
        source[i] = static_cast<uint32_t>(i * 2654435761u) & 0x00ffffffu;
    std::vector<uint32_t> destination(static_cast<size_t>(dimensions.width) * dimensions.height);
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(source.data());
        benchmark::DoNotOptimize(destination.data());
        if (!screen_scale::Apply(mode, source, destination))
        {
            state.SkipWithError("Screen scaler rejected valid buffers");
            break;
        }
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(destination.size()));
    state.SetLabel("output pixels");
}
BENCHMARK(BM_ScreenScale)->DenseRange(0, 7)->Unit(benchmark::kMicrosecond);

int main(int argc, char** argv)
{
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
    CProcessor::Init();
    benchmark::RunSpecifiedBenchmarks();
    CProcessor::Done();
    benchmark::Shutdown();
}
