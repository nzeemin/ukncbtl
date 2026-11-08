# Correctness, benchmarks and profiling

Use GoogleTest for correctness, Google Benchmark for speed measurements, and
Tracy to locate bottlenecks. All three are integrated with CMake on Windows x64
and MSVC. Commands below run from the repository root in Developer PowerShell.

## Build options and dependencies

| CMake option | Default | Purpose |
| --- | --- | --- |
| `BUILD_TESTING` | `ON` | GoogleTest executables registered with CTest |
| `UKNCBTL_BUILD_BENCHMARKS` | `OFF` | Headless `EmulatorBenchmarks` executable |
| `UKNCBTL_ENABLE_TRACY` | `OFF` | Tracy instrumentation in the app and shared core/scaler |

CMake fetches GoogleTest 1.17.0, Google Benchmark 1.9.4 and Tracy 0.12.2 by
pinned commit, as needed. GoogleTest and Benchmark are linked only into their
respective executables. With Tracy off, profiling macros compile away and no
Tracy dependency is downloaded or linked. Turning all three options off builds
the application without fetching any of these dependencies.

The application and fetched libraries use the static MSVC runtime (`/MT`, or
`/MTd` for Debug). CMake's `FETCHCONTENT_SOURCE_DIR_GOOGLETEST`,
`FETCHCONTENT_SOURCE_DIR_GOOGLEBENCHMARK` and `FETCHCONTENT_SOURCE_DIR_TRACY`
can point at local checkouts of the pinned revisions for offline builds.
Existing populated build directories can use `-DFETCHCONTENT_UPDATES_DISCONNECTED=ON`.

## Correctness

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Use `ctest --test-dir build -C Release -N` to list cases and `-R ProcessorTests`
to select a suite. `build/Release/LegacyCoreTests.exe` also accepts native
GoogleTest options such as `--gtest_filter=ProcessorTests.*` and
`--gtest_output=xml:build/core-results.xml`.

The compatibility adapter reports legacy assertion locations, expected/actual
values and context through GoogleTest. Processor instruction tables are
initialized and released by a GoogleTest global environment, including when
CTest executes an individual case. The original standalone regression workloads
also run under GoogleTest; exhaustive corpus checks remain grouped to avoid
launching a process per opcode. Keep expected results independent of optimized
code, and preserve exact flags, memory effects, timing and pixel rounding.

## Speed measurements

```powershell
cmake -S . -B build -DUKNCBTL_BUILD_BENCHMARKS=ON -DUKNCBTL_ENABLE_TRACY=OFF
cmake --build build --config Release --target EmulatorBenchmarks --parallel
./build/Release/EmulatorBenchmarks.exe --benchmark_min_time=0.5s --benchmark_repetitions=5 --benchmark_out=build/benchmark.json --benchmark_out_format=json
```

The benchmark executable has no GUI, ROM, disk image or GoogleTest dependency:

- `BM_CpuInstructions` runs batches of 1,024 guest instructions through the
  interpreter. It reports instructions/second and excludes cycle scheduling.
- `BM_SystemFrame` runs the real CPU/PPU scheduler using a synthetic RAM program,
  with host I/O disconnected. Each item is one emulated 20 ms frame. Timings are
  host execution time, not a promise of real-world application FPS.
- `BM_ScreenScale/0` through `/7` cover all scaling modes and report output
  pixels/second. Buffer allocation and initialization happen outside timing.

Both processors execute a deterministic immediate/register/memory/branch loop;
register values wrap naturally. GoogleTest verifies that the loop executes,
stores results and keeps both processors running across frames. Benchmark output
is consumed with optimization barriers. No timing assertions are added to CTest.
Run correctness tests before comparing performance, use the same machine and
build configuration, and measure with Tracy disabled. A quick smoke run uses
`--benchmark_min_time=0.01s`.

## Locate bottlenecks with Tracy

Use a separate directory to keep instrumented timings out of benchmark baselines:

```powershell
cmake -S . -B build/profile -G "Visual Studio 18 2026" -A x64 -DBUILD_TESTING=OFF -DUKNCBTL_ENABLE_TRACY=ON
cmake --build build/profile --config RelWithDebInfo --target UKNCBTL --parallel
./build/profile/RelWithDebInfo/UKNCBTL.exe
```

Open the Tracy **0.12.2** profiler from the [upstream release](https://github.com/wolfpld/tracy/releases/tag/v0.12.2)
and connect to `127.0.0.1` (default port 8086), then start emulation and exercise
the workload of interest. The client uses on-demand collection and listens only
on localhost. Capture collection begins when the profiler connects.

Frame markers delimit iterations of the host main loop, including paused
iterations. Named zones identify `Emulator frame`, `Core frame`, `Video decode`,
`Prepare screen`, `Screen draw`, `Screen scale`, `Audio submit`,
`Audio buffer wait` and `Frame pacing wait`. Audio and pacing waits are deliberate
throttling, so inspect them separately from computation. The screen-scaling zone
appears only on paths that call the CPU scaler. No per-instruction or per-sample
zones are emitted, to keep the capture size and instrumentation cost manageable.

If needed, enable benchmarks in this profiling build to capture the synthetic
workloads too. They use the same instrumented core and scaler; their timings
should not be compared with the uninstrumented baseline.
