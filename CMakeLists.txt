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
  emulator/util/BitmapFile.cpp emulator/util/WavPcmFile.cpp
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
endif()
