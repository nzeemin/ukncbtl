cmake_minimum_required(VERSION 3.20)
project(UKNCBTL LANGUAGES CXX RC)

# Use the same language contract for the application, renderer, tests and tools.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "UKNCBTL requires Windows and the Microsoft C++ compiler")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
  message(FATAL_ERROR "UKNCBTL requires an x86-64 (x64) build")
endif()

enable_language(ASM_MASM)
add_library(CpuOverflow STATIC emulator/emubase/CpuOverflow.asm)

# Keep the CRT consistent across the application, tests and fetched libraries.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
include(CTest)
option(UKNCBTL_BUILD_BENCHMARKS "Build headless Google Benchmark workloads" OFF)
option(UKNCBTL_ENABLE_TRACY "Instrument the emulator with Tracy" OFF)
option(UKNCBTL_ENABLE_ASAN "Instrument C++ targets and dependencies with AddressSanitizer" OFF)
if(UKNCBTL_ENABLE_ASAN)
  # MSVC's runtime checks and edit-and-continue conflict with AddressSanitizer.
  string(REGEX REPLACE "/RTC[1su]+" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(REPLACE "/ZI" "/Zi" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  # MSVC STL container annotations must agree in every linked object, including
  # GoogleTest, Benchmark and Tracy. Apply these before fetching dependencies.
  add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/fsanitize=address;/Zi>")
  add_link_options(/INCREMENTAL:NO)
endif()
include(cmake/Dependencies.cmake)

function(ukncbtl_toolchain target)
  target_compile_features(${target} PUBLIC cxx_std_23)
  target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/permissive-;/Zc:__cplusplus;/GS>")
endfunction()

function(ukncbtl_target target)
  ukncbtl_toolchain(${target})
  target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/emulator")
  target_compile_definitions(${target} PRIVATE UNICODE _UNICODE NOMINMAX)
  target_precompile_headers(${target} PRIVATE "${PROJECT_SOURCE_DIR}/emulator/stdafx.h")
endfunction()

add_library(EmulatorCore STATIC
  emulator/emubase/Board.cpp emulator/emubase/Disasm.cpp
  emulator/emubase/Floppy.cpp emulator/emubase/Hard.cpp
  emulator/emubase/Memory.cpp emulator/emubase/Processor.cpp emulator/emubase/SoundAY.cpp)
ukncbtl_target(EmulatorCore)
target_link_libraries(EmulatorCore PUBLIC CpuOverflow)

add_library(EmulatorProfiling INTERFACE)
if(UKNCBTL_ENABLE_TRACY)
  target_compile_definitions(EmulatorProfiling INTERFACE UKNCBTL_ENABLE_TRACY)
  target_link_libraries(EmulatorProfiling INTERFACE Tracy::TracyClient)
endif()
target_link_libraries(EmulatorCore PUBLIC EmulatorProfiling)

add_library(EmulatorSnapshot STATIC emulator/Snapshot.cpp)
ukncbtl_target(EmulatorSnapshot)
target_link_libraries(EmulatorSnapshot PUBLIC EmulatorCore)

add_library(VideoDecoder STATIC emulator/VideoDecoder.cpp)
ukncbtl_target(VideoDecoder)
target_link_libraries(VideoDecoder PUBLIC EmulatorCore)

add_library(ScreenScale STATIC emulator/util/ScreenScale.cpp)
ukncbtl_target(ScreenScale)
target_link_libraries(ScreenScale PUBLIC EmulatorProfiling)

add_executable(UKNCBTL WIN32
  emulator/Common.cpp emulator/util/CommonText.cpp emulator/ConsoleView.cpp emulator/DebugView.cpp
  emulator/Dialogs.cpp emulator/DiskImage.cpp emulator/DisasmView.cpp emulator/InstrHistoryView.cpp
  emulator/DisplayListView.cpp emulator/Emulator.cpp emulator/KeyboardView.cpp
  emulator/Main.cpp emulator/MainWindow.cpp emulator/MemoryView.cpp
  emulator/ScreenView.cpp emulator/ScreenText.cpp emulator/Subtitles.cpp emulator/Input.cpp emulator/Settings.cpp emulator/SoundGen.cpp
  emulator/SpriteView.cpp emulator/ViewCalculations.cpp emulator/TapeView.cpp emulator/ToolWindow.cpp
  emulator/util/BitmapFile.cpp emulator/util/WavPcmFile.cpp emulator/res/UKNCBTL.rc)
ukncbtl_target(UKNCBTL)
target_include_directories(UKNCBTL PRIVATE emulator/res)
target_link_libraries(UKNCBTL PRIVATE EmulatorCore EmulatorSnapshot VideoDecoder ScreenScale vfw32 comctl32 winmm advapi32 shell32 windowscodecs)
target_link_options(UKNCBTL PRIVATE "LINKER:/MANIFESTDEPENDENCY:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'")

# The default renderer is loaded dynamically, so it must be built alongside the
# executable even when only the UKNCBTL target is requested.
add_library(RenderVfw SHARED third_party/RenderVfw/RenderVfw.cpp third_party/RenderVfw/RenderVfw.def)
ukncbtl_toolchain(RenderVfw)
target_compile_definitions(RenderVfw PRIVATE UNICODE _UNICODE)
target_link_libraries(RenderVfw PRIVATE vfw32)
add_dependencies(UKNCBTL RenderVfw)
install(TARGETS UKNCBTL RenderVfw RUNTIME DESTINATION .)

if(BUILD_TESTING)
  include(GoogleTest)
  function(ukncbtl_test target)
    add_executable(${target} ${ARGN})
    ukncbtl_target(${target})
    target_link_libraries(${target} PRIVATE GTest::gtest_main)
    gtest_discover_tests(${target} DISCOVERY_TIMEOUT 30)
  endfunction()

  ukncbtl_test(CpuOverflowTests tests/CpuOverflowTests.cpp)
  target_link_libraries(CpuOverflowTests PRIVATE CpuOverflow)
  ukncbtl_test(BreakpointTests tests/BreakpointTests.cpp)
  ukncbtl_test(InstructionHistoryTests tests/InstructionHistoryTests.cpp)
  ukncbtl_test(SerializationTests tests/SerializationTests.cpp)
  ukncbtl_test(SnapshotTests tests/SnapshotTests.cpp)
  target_link_libraries(SnapshotTests PRIVATE EmulatorSnapshot)
  ukncbtl_test(VideoDecoderTests tests/VideoDecoderTests.cpp)
  target_link_libraries(VideoDecoderTests PRIVATE VideoDecoder)
  ukncbtl_test(CommandTests tests/CommandTests.cpp)
  target_link_libraries(CommandTests PRIVATE shell32)
  ukncbtl_test(ConsoleProductTests tests/ConsoleProductTests.cpp)
  target_compile_definitions(ConsoleProductTests PRIVATE PRODUCT)
  set_target_properties(ConsoleProductTests PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
  ukncbtl_test(SettingsTests tests/SettingsTests.cpp emulator/Settings.cpp)
  ukncbtl_test(CommonTextTests tests/CommonTextTests.cpp emulator/util/CommonText.cpp)
  ukncbtl_test(SpriteTests tests/SpriteTests.cpp)
  ukncbtl_test(ViewCalculationTests tests/ViewCalculationTests.cpp emulator/ViewCalculations.cpp)
  ukncbtl_test(SubtitleTests tests/SubtitleTests.cpp emulator/Subtitles.cpp)
  ukncbtl_test(ScreenTextTests tests/ScreenTextTests.cpp emulator/ScreenText.cpp)
  ukncbtl_test(InputTests tests/InputTests.cpp emulator/Input.cpp)
  ukncbtl_test(TextFormatTests tests/TextFormatTests.cpp)
  ukncbtl_test(ScreenScaleTests tests/ScreenScaleTests.cpp)
  target_link_libraries(ScreenScaleTests PRIVATE ScreenScale)
  ukncbtl_test(BitmapFileTests tests/BitmapFileTests.cpp emulator/util/BitmapFile.cpp)
  target_link_libraries(BitmapFileTests PRIVATE windowscodecs)
  ukncbtl_test(DiskImageTests tests/DiskImageTests.cpp emulator/DiskImage.cpp)
  ukncbtl_test(SoundOutputTests tests/SoundOutputTests.cpp emulator/SoundGen.cpp)
  target_link_libraries(SoundOutputTests PRIVATE winmm EmulatorProfiling)
  ukncbtl_test(TapeTests tests/TapeTests.cpp)
  ukncbtl_test(WavPcmFileTests tests/WavPcmFileTests.cpp emulator/util/WavPcmFile.cpp)
  ukncbtl_test(CoreSmokeTests tests/CoreSmokeTests.cpp)
  target_link_libraries(CoreSmokeTests PRIVATE EmulatorCore)
  ukncbtl_test(DisasmCorpusTests tests/DisasmCorpusTests.cpp)
  target_link_libraries(DisasmCorpusTests PRIVATE EmulatorCore)

  # Preserve the Microsoft test sources while registering each case with GoogleTest.
  # New tests can use native TEST/TEST_F/TEST_P macros directly.
  set(legacy_suites BoardTests DisasmTests FloppyTests HardDriveTests HintTests
      MemoryTests ProcessorControlTests ProcessorSystemTests ProcessorTests SoundAYTests)
  set(legacy_dir "${CMAKE_CURRENT_BINARY_DIR}/google-tests")
  file(MAKE_DIRECTORY "${legacy_dir}")
  set(legacy_wrappers)
  foreach(suite IN LISTS legacy_suites)
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/tests/${suite}.cpp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
    file(READ "${source}" contents)
    string(REGEX MATCHALL "TEST_METHOD\\([A-Za-z_][A-Za-z_0-9]*\\)" methods "${contents}")
    set(wrapper "#include \"stdafx.h\"\n#include \"${source}\"\n#include \"LegacyTestRunner.h\"\n")
    foreach(method IN LISTS methods)
      string(REGEX REPLACE "TEST_METHOD\\(([A-Za-z_][A-Za-z_0-9]*)\\)" "\\1" name "${method}")
      set(test_name "${name}")
      if(contents MATCHES "BEGIN_TEST_METHOD_ATTRIBUTE\\(${name}\\)[ \t\r\n]*TEST_IGNORE\\(\\)")
        set(test_name "DISABLED_${name}")
      endif()
      string(APPEND wrapper "TEST(${suite}, ${test_name}) { RunLegacyTest([] { ${suite} test; test.${name}(); }); }\n")
    endforeach()
    set(wrapper_path "${legacy_dir}/${suite}Runner.cpp")
    file(CONFIGURE OUTPUT "${wrapper_path}" CONTENT "${wrapper}" @ONLY)
    list(APPEND legacy_wrappers "${wrapper_path}")
  endforeach()
  add_executable(LegacyCoreTests tests/GoogleTestMain.cpp ${legacy_wrappers}
    tests/TestCommon.cpp tests/TestHelpers.cpp tests/BenchmarkWorkloadTests.cpp
    tests/GoogleTestAdapterTests.cpp)
  ukncbtl_target(LegacyCoreTests)
  target_include_directories(LegacyCoreTests PRIVATE tests/compat tests)
  target_link_libraries(LegacyCoreTests PRIVATE GTest::gtest EmulatorCore winmm)
  gtest_discover_tests(LegacyCoreTests DISCOVERY_TIMEOUT 30)
endif()

if(UKNCBTL_BUILD_BENCHMARKS)
  add_executable(EmulatorBenchmarks benchmarks/EmulatorBenchmarks.cpp)
  ukncbtl_target(EmulatorBenchmarks)
  target_link_libraries(EmulatorBenchmarks PRIVATE benchmark::benchmark EmulatorCore ScreenScale)
endif()
