cmake_minimum_required(VERSION 3.20)
project(UKNCBTL LANGUAGES CXX RC)

if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "UKNCBTL requires Windows and the Microsoft C++ compiler")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "UKNCBTL requires a 32-bit (Win32) build")
endif()

# Track the newest language mode provided by the installed MSVC toolset.
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/std:c++latest>)

add_executable(UKNCBTL WIN32
  emulator/Common.cpp emulator/ConsoleView.cpp emulator/DebugView.cpp
  emulator/Dialogs.cpp emulator/DisasmView.cpp emulator/InstrHistoryView.cpp
  emulator/DisplayListView.cpp emulator/Emulator.cpp emulator/KeyboardView.cpp
  emulator/Main.cpp emulator/MainWindow.cpp emulator/MemoryView.cpp
  emulator/ScreenView.cpp emulator/Settings.cpp emulator/SoundGen.cpp
  emulator/SpriteView.cpp emulator/TapeView.cpp emulator/ToolWindow.cpp
  emulator/util/BitmapFile.cpp emulator/util/ScreenScale.cpp emulator/util/WavPcmFile.cpp
  emulator/emubase/Board.cpp emulator/emubase/Disasm.cpp
  emulator/emubase/Floppy.cpp emulator/emubase/Hard.cpp
  emulator/emubase/Memory.cpp emulator/emubase/Processor.cpp
  emulator/emubase/SoundAY.cpp emulator/res/UKNCBTL.rc)
target_include_directories(UKNCBTL PRIVATE emulator emulator/res)
target_compile_definitions(UKNCBTL PRIVATE UNICODE _UNICODE)
target_precompile_headers(UKNCBTL PRIVATE emulator/stdafx.h)
target_link_libraries(UKNCBTL PRIVATE vfw32 comctl32 winmm advapi32 shell32 windowscodecs)
target_link_options(UKNCBTL PRIVATE "LINKER:/MANIFESTDEPENDENCY:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'")
set_property(TARGET UKNCBTL PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# The default renderer is loaded dynamically, so it must be built alongside the
# executable even when only the UKNCBTL target is requested.
add_library(RenderVfw SHARED third_party/RenderVfw/RenderVfw.cpp third_party/RenderVfw/RenderVfw.def)
target_compile_definitions(RenderVfw PRIVATE UNICODE _UNICODE)
target_link_libraries(RenderVfw PRIVATE vfw32)
set_property(TARGET RenderVfw PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_dependencies(UKNCBTL RenderVfw)
install(TARGETS UKNCBTL RenderVfw RUNTIME DESTINATION .)

include(CTest)
if(BUILD_TESTING)
  add_executable(ScreenScaleTests tests/ScreenScaleTests.cpp emulator/util/ScreenScale.cpp)
  target_include_directories(ScreenScaleTests PRIVATE emulator)
  target_compile_definitions(ScreenScaleTests PRIVATE UNICODE _UNICODE)
  target_precompile_headers(ScreenScaleTests PRIVATE emulator/stdafx.h)
  add_test(NAME ScreenScaleTests COMMAND ScreenScaleTests)

  add_executable(BitmapFileTests tests/BitmapFileTests.cpp emulator/util/BitmapFile.cpp)
  target_include_directories(BitmapFileTests PRIVATE emulator)
  target_compile_definitions(BitmapFileTests PRIVATE UNICODE _UNICODE)
  target_precompile_headers(BitmapFileTests PRIVATE emulator/stdafx.h)
  target_link_libraries(BitmapFileTests PRIVATE windowscodecs)
  add_test(NAME BitmapFileTests COMMAND BitmapFileTests)

  add_executable(WavPcmFileTests tests/WavPcmFileTests.cpp emulator/util/WavPcmFile.cpp)
  target_include_directories(WavPcmFileTests PRIVATE emulator)
  target_compile_definitions(WavPcmFileTests PRIVATE UNICODE _UNICODE)
  target_precompile_headers(WavPcmFileTests PRIVATE emulator/stdafx.h)
  add_test(NAME WavPcmFileTests COMMAND WavPcmFileTests)

  add_executable(CoreSmokeTests tests/CoreSmokeTests.cpp
    emulator/emubase/Board.cpp emulator/emubase/Disasm.cpp emulator/emubase/Floppy.cpp emulator/emubase/Hard.cpp
    emulator/emubase/Memory.cpp emulator/emubase/Processor.cpp emulator/emubase/SoundAY.cpp)
  target_include_directories(CoreSmokeTests PRIVATE emulator)
  target_compile_definitions(CoreSmokeTests PRIVATE UNICODE _UNICODE)
  target_precompile_headers(CoreSmokeTests PRIVATE emulator/stdafx.h)
  add_test(NAME CoreSmokeTests COMMAND CoreSmokeTests)

  add_executable(DisasmCorpusTests tests/DisasmCorpusTests.cpp
    emulator/emubase/Board.cpp emulator/emubase/Disasm.cpp emulator/emubase/Floppy.cpp
    emulator/emubase/Hard.cpp emulator/emubase/Memory.cpp emulator/emubase/Processor.cpp
    emulator/emubase/SoundAY.cpp)
  target_include_directories(DisasmCorpusTests PRIVATE emulator)
  target_compile_definitions(DisasmCorpusTests PRIVATE UNICODE _UNICODE)
  target_precompile_headers(DisasmCorpusTests PRIVATE emulator/stdafx.h)
  add_test(NAME DisasmCorpusTests COMMAND DisasmCorpusTests)

  # Run the existing Microsoft C++ unit tests through a small CTest adapter.
  # Generate one wrapper per suite so the original tests stay independently compiled.
  set(legacy_suites BoardTests DisasmTests FloppyTests HardDriveTests HintTests
      MemoryTests ProcessorControlTests ProcessorSystemTests ProcessorTests SoundAYTests)
  set(legacy_dir "${CMAKE_CURRENT_BINARY_DIR}/legacy-tests")
  file(MAKE_DIRECTORY "${legacy_dir}")
  set(legacy_wrappers)
  set(legacy_declarations "")
  set(legacy_calls "")
  foreach(suite IN LISTS legacy_suites)
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/tests/${suite}.cpp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
    file(READ "${source}" contents)
    string(REGEX MATCHALL "TEST_METHOD\\([A-Za-z_][A-Za-z_0-9]*\\)" methods "${contents}")
    set(wrapper "#include \"stdafx.h\"\n#include \"${source}\"\n#include \"LegacyTestRunner.h\"\nvoid run_${suite}() {\n")
    foreach(method IN LISTS methods)
      string(REGEX REPLACE "TEST_METHOD\\(([A-Za-z_][A-Za-z_0-9]*)\\)" "\\1" name "${method}")
      if(NOT contents MATCHES "BEGIN_TEST_METHOD_ATTRIBUTE\\(${name}\\)[ \t\r\n]*TEST_IGNORE\\(\\)")
        string(APPEND wrapper "  RunLegacyTest(\"${suite}.${name}\", [] { ${suite} test; test.${name}(); });\n")
      endif()
    endforeach()
    string(APPEND wrapper "}\n")
    set(wrapper_path "${legacy_dir}/${suite}Runner.cpp")
    file(CONFIGURE OUTPUT "${wrapper_path}" CONTENT "${wrapper}" @ONLY)
    list(APPEND legacy_wrappers "${wrapper_path}")
    string(APPEND legacy_declarations "void run_${suite}();\n")
    string(APPEND legacy_calls "  run_${suite}();\n")
  endforeach()
  set(legacy_main "#include \"stdafx.h\"\n#include \"LegacyTestRunner.h\"\n${legacy_declarations}void TestModuleInitialize();\nvoid TestModuleCleanup();\nint testCount = 0;\nint failureCount = 0;\nint main() {\n  TestModuleInitialize();\n${legacy_calls}  TestModuleCleanup();\n  std::printf(\"%d tests, %d failures\\n\", testCount, failureCount);\n  return failureCount ? 1 : 0;\n}\n")
  file(CONFIGURE OUTPUT "${legacy_dir}/main.cpp" CONTENT "${legacy_main}" @ONLY)
  add_executable(LegacyCoreTests "${legacy_dir}/main.cpp" ${legacy_wrappers}
    tests/TestCommon.cpp tests/TestHelpers.cpp
    emulator/emubase/Board.cpp emulator/emubase/Disasm.cpp emulator/emubase/Floppy.cpp
    emulator/emubase/Hard.cpp emulator/emubase/Memory.cpp emulator/emubase/Processor.cpp
    emulator/emubase/SoundAY.cpp)
  target_include_directories(LegacyCoreTests PRIVATE tests/compat tests emulator)
  target_compile_definitions(LegacyCoreTests PRIVATE UNICODE _UNICODE)
  target_precompile_headers(LegacyCoreTests PRIVATE emulator/stdafx.h)
  target_link_libraries(LegacyCoreTests PRIVATE winmm)
  add_test(NAME LegacyCoreTests COMMAND LegacyCoreTests)
endif()
