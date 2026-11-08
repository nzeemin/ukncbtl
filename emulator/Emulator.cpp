/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Emulator.cpp

#include "stdafx.h"
#include "util/Profiler.h"
#include <cstdio>
#include <array>
#include <format>
#include <memory>
#include <vector>
#include <share.h>
#include "Main.h"
#include "Emulator.h"
#include "Views.h"
#include "emubase/Emubase.h"
#include "Snapshot.h"
#include "SoundGen.h"

//NOTE: I know, we use unsafe string functions
#pragma warning( disable: 4996 )

//////////////////////////////////////////////////////////////////////


CMotherboard* g_pBoard = nullptr;
static std::unique_ptr<CMotherboard> g_boardOwner;

bool g_okEmulatorInitialized = false;
bool g_okEmulatorRunning = false;

static BreakpointSet cpuBreakpoints;
static BreakpointSet ppuBreakpoints;

bool m_okEmulatorSound = false;
uint16_t m_wEmulatorSoundSpeed = 100;
int m_nEmulatorSoundChanges = 0;

uint16_t m_Settings_NetStation_Bits = 0;
bool m_okEmulatorNetwork = false;
HANDLE m_hEmulatorNetPort = INVALID_HANDLE_VALUE;

bool m_okEmulatorSerial = false;
HANDLE m_hEmulatorComPort = INVALID_HANDLE_VALUE;

bool m_okEmulatorParallel = false;
UniqueFile m_parallelOutput;

long m_nFrameCount = 0;
uint32_t m_dwTickCount = 0;
uint32_t m_dwEmulatorUptime = 0;  // UKNC uptime, seconds, from turn on or reset, increments every 25 frames
long m_nUptimeFrameCount = 0;

uint8_t* g_pEmulatorRam[3] = { nullptr, nullptr, nullptr };  // RAM values - for change tracking
uint8_t* g_pEmulatorChangedRam[3] = { nullptr, nullptr, nullptr };  // RAM change flags
static std::unique_ptr<uint8_t[]> g_emulatorRamOwners[3];
static std::unique_ptr<uint8_t[]> g_emulatorChangedRamOwners[3];
uint16_t g_wEmulatorCpuPC = 0177777;      // Current PC value
uint16_t g_wEmulatorPrevCpuPC = 0177777;  // Previous PC value
uint16_t g_wEmulatorPpuPC = 0177777;      // Current PC value
uint16_t g_wEmulatorPrevPpuPC = 0177777;  // Previous PC value

// Digit keys scan codes used for AutoBoot feature
const BYTE m_arrDigitKeyScans[] =
{
    0176, 0030, 0031, 0032, 0013, 0034, 0035, 0016, 0017, 0177  // 0, 1, ... 9
};


//////////////////////////////////////////////////////////////////////


const LPCTSTR FILE_NAME_UKNC_ROM = _T("uknc_rom.bin");

bool Emulator_LoadUkncRom()
{
    std::array<uint8_t, 32768> data{};

    UniqueFile fpFile(::_tfsopen(FILE_NAME_UKNC_ROM, _T("rb"), _SH_DENYWR));
    if (fpFile != nullptr)
    {
        size_t dwBytesRead = ::fread(data.data(), 1, 32256, fpFile.get());
        fpFile.reset();
        if (dwBytesRead != 32256)
        {
            return false;
        }
    }
    else  // uknc_rom.bin not found, use ROM image from resources
    {
        HRSRC hRes = nullptr;
        DWORD dwDataSize = 0;
        HGLOBAL hResLoaded = nullptr;
        void * pResData = nullptr;
        if ((hRes = ::FindResource(nullptr, MAKEINTRESOURCE(IDR_UKNC_ROM), _T("BIN"))) == nullptr ||
            (dwDataSize = ::SizeofResource(nullptr, hRes)) < 32256 ||
            (hResLoaded = ::LoadResource(nullptr, hRes)) == nullptr ||
            (pResData = ::LockResource(hResLoaded)) == nullptr)
        {
            return false;
        }
        ::memcpy(data.data(), pResData, 32256);
    }

    g_pBoard->LoadROM(data);

    return true;
}

void CALLBACK Emulator_Instruction_Callback(const CProcessor* pProc, uint16_t address, uint16_t opcode);
void CALLBACK Emulator_Interrupt_Callback(const CProcessor* pProc, uint16_t oldPc, uint16_t vector);

bool Emulator_Init()
{
    ASSERT(g_pBoard == nullptr);

    ::memset(g_pEmulatorRam, 0, sizeof(g_pEmulatorRam));
    ::memset(g_pEmulatorChangedRam, 0, sizeof(g_pEmulatorChangedRam));
    CProcessor::Init();

    cpuBreakpoints.Clear();
    ppuBreakpoints.Clear();
    for (int i = 0; i < MAX_BREAKPOINTCOUNT; ++i)
    {
        cpuBreakpoints.Add(Settings_GetDebugBreakpoint(i, TRUE));
        ppuBreakpoints.Add(Settings_GetDebugBreakpoint(i, FALSE));
    }
    g_boardOwner = std::make_unique<CMotherboard>();
    g_pBoard = g_boardOwner.get();

    if (! Emulator_LoadUkncRom())
    {
        AlertWarning(_T("Failed to load UKNC ROM image."));
        g_boardOwner.reset();
        g_pBoard = nullptr;
        CProcessor::Done();
        return false;
    }

    g_pBoard->SetNetStation((uint16_t)Settings_GetNetStation());

    g_pBoard->SetInstructionCallback(Emulator_Instruction_Callback);
    g_pBoard->SetInterruptCallback(Emulator_Interrupt_Callback);

    g_pBoard->Reset();
    Emulator_ClearHistory();

    if (m_okEmulatorSound)
    {
        SoundGen_Initialize(Settings_GetSoundVolume());
        g_pBoard->SetSoundGenCallback(SoundGen_FeedDAC);
    }

    m_nUptimeFrameCount = 0;
    m_dwEmulatorUptime = 0;

    // Allocate memory for old RAM values
    for (int i = 0; i < 3; i++)
    {
        g_emulatorRamOwners[i] = std::make_unique<uint8_t[]>(65536);
        g_emulatorChangedRamOwners[i] = std::make_unique<uint8_t[]>(65536);
        g_pEmulatorRam[i] = g_emulatorRamOwners[i].get();
        g_pEmulatorChangedRam[i] = g_emulatorChangedRamOwners[i].get();
    }

    g_okEmulatorInitialized = TRUE;
    return true;
}

void Emulator_Done()
{
    ASSERT(g_pBoard != nullptr);

    // Temporary run-to targets are not persistent debugger settings.
    cpuBreakpoints.SetTemporary(BreakpointSet::NoBreakpoint);
    ppuBreakpoints.SetTemporary(BreakpointSet::NoBreakpoint);
    for (int i = 0; i < MAX_BREAKPOINTCOUNT; ++i)
    {
        Settings_SetDebugBreakpoint(i, TRUE, cpuBreakpoints.Data()[i]);
        Settings_SetDebugBreakpoint(i, FALSE, ppuBreakpoints.Data()[i]);
    }
    CProcessor::Done();

    g_pBoard->SetInstructionCallback(nullptr);
    g_pBoard->SetInterruptCallback(nullptr);
    g_pBoard->SetSoundGenCallback(nullptr);
    SoundGen_Finalize();

    g_pBoard->SetSerialCallbacks(nullptr, nullptr);
    if (m_hEmulatorComPort != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_hEmulatorComPort);
        m_hEmulatorComPort = INVALID_HANDLE_VALUE;
    }

    g_pBoard->SetNetworkCallbacks(nullptr, nullptr);
    if (m_hEmulatorNetPort != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_hEmulatorNetPort);
        m_hEmulatorNetPort = INVALID_HANDLE_VALUE;
    }

    g_pBoard->SetParallelOutCallback(nullptr);
    m_parallelOutput.reset();
    m_okEmulatorParallel = false;

    g_boardOwner.reset();
    g_pBoard = nullptr;

    // Free memory used for old RAM values
    for (int i = 0; i < 3; i++)
    {
        g_emulatorRamOwners[i].reset();
        g_emulatorChangedRamOwners[i].reset();
        g_pEmulatorRam[i] = nullptr;
        g_pEmulatorChangedRam[i] = nullptr;
    }

    g_okEmulatorInitialized = false;
}

void Emulator_Start()
{
    g_okEmulatorRunning = TRUE;

    // Set title bar text
    MainWindow_UpdateWindowTitle();
    MainWindow_UpdateMenu();

    m_nFrameCount = 0;
    m_dwTickCount = GetTickCount();

    // For proper breakpoint processing
    if (!cpuBreakpoints.Addresses().empty() || !ppuBreakpoints.Addresses().empty())
    {
        g_pBoard->GetCPU()->ClearInternalTick();
        g_pBoard->GetPPU()->ClearInternalTick();
    }
}
void Emulator_Stop()
{
    g_okEmulatorRunning = false;

    Emulator_SetTempCPUBreakpoint(0177777);
    Emulator_SetTempPPUBreakpoint(0177777);

    if (m_parallelOutput)
        std::fflush(m_parallelOutput.get());

    // Reset title bar message
    MainWindow_UpdateWindowTitle();
    MainWindow_UpdateMenu();

    // Reset FPS indicator
    MainWindow_SetStatusbarText(StatusbarPartFPS, nullptr);

    MainWindow_UpdateAllViews();
}

void Emulator_Reset()
{
    ASSERT(g_pBoard != nullptr);

    g_pBoard->Reset();

    Emulator_ClearHistory();

    m_nUptimeFrameCount = 0;
    m_dwEmulatorUptime = 0;

    MainWindow_UpdateAllViews();
}

bool Emulator_AddCPUBreakpoint(uint16_t address) { return cpuBreakpoints.Add(address); }
bool Emulator_AddPPUBreakpoint(uint16_t address) { return ppuBreakpoints.Add(address); }
bool Emulator_RemoveCPUBreakpoint(uint16_t address) { return cpuBreakpoints.Remove(address); }
bool Emulator_RemovePPUBreakpoint(uint16_t address) { return ppuBreakpoints.Remove(address); }
void Emulator_SetTempCPUBreakpoint(uint16_t address) { cpuBreakpoints.SetTemporary(address); }
void Emulator_SetTempPPUBreakpoint(uint16_t address) { ppuBreakpoints.SetTemporary(address); }
const uint16_t* Emulator_GetCPUBreakpointList() { return cpuBreakpoints.Data(); }
const uint16_t* Emulator_GetPPUBreakpointList() { return ppuBreakpoints.Data(); }
bool Emulator_IsBreakpoint()
{
    return cpuBreakpoints.Contains(g_pBoard->GetCPU()->GetPC()) ||
        ppuBreakpoints.Contains(g_pBoard->GetPPU()->GetPC());
}
bool Emulator_IsBreakpoint(bool okCpuPpu, uint16_t address)
{
    return (okCpuPpu ? cpuBreakpoints : ppuBreakpoints).Contains(address);
}
void Emulator_RemoveAllBreakpoints(bool okCpuPpu)
{
    (okCpuPpu ? cpuBreakpoints : ppuBreakpoints).Clear();
}

bool Emulator_IsSound()
{
    return m_nEmulatorSoundChanges > 0;
}

void Emulator_SetSpeed(uint16_t realspeed)
{
    uint16_t speedpercent;
    switch (realspeed)
    {
    case 0: speedpercent = 500; break;
    case 1: speedpercent = 100; break;
    case 2: speedpercent = 200; break;
    case 3: speedpercent = 400; break;
    case 0x7fff: speedpercent = 50; break;
    case 0x7ffe: speedpercent = 25; break;
    case 0x7ffd: speedpercent = 10; break;
    default: speedpercent = 100; break;
    }
    m_wEmulatorSoundSpeed = speedpercent;

    if (m_okEmulatorSound)
        SoundGen_SetSpeed(m_wEmulatorSoundSpeed);
}

void Emulator_SetSound(bool soundOnOff)
{
    if (m_okEmulatorSound != soundOnOff)
    {
        if (soundOnOff)
        {
            SoundGen_Initialize(Settings_GetSoundVolume());
            SoundGen_SetSpeed(m_wEmulatorSoundSpeed);
            g_pBoard->SetSoundGenCallback(SoundGen_FeedDAC);
        }
        else
        {
            g_pBoard->SetSoundGenCallback(nullptr);
            SoundGen_Finalize();
        }
    }

    m_okEmulatorSound = soundOnOff;
}

void Emulator_SetSoundAY(bool soundAYOnOff)
{
    g_pBoard->SetSoundAY(soundAYOnOff);
}

void Emulator_SetSoundCovox(bool soundCovoxOnOff)
{
    g_pBoard->SetSoundCovox(soundCovoxOnOff);
}

void Emulator_SetMouse(bool mouseOnOff)
{
    g_pBoard->SetMouse(mouseOnOff);
}

static InstructionHistory instructionHistory;

int Emulator_GetHistoryLength(bool okCpuPpu)
{
    return instructionHistory.Length(okCpuPpu);
}

const InstructionRecord& Emulator_GetHistoryRecord(bool okCpuPpu, int index)
{
    return instructionHistory.Get(okCpuPpu, index);
}

void Emulator_ClearHistory()
{
    instructionHistory.Clear();
}

LPCTSTR Emulator_GetInterruptName(uint16_t vector)
{
    return InstructionHistory::InterruptName(vector);
}

// Called by CPU/PPU before each instruction is executed; installed always, records history and writes trace
void CALLBACK Emulator_Instruction_Callback(const CProcessor* pProc, uint16_t address, uint16_t opcode)
{
    bool okCpu = (pProc == g_pBoard->GetCPU());

    // Instruction is at most 3 words long
    const CMemoryController* pMemCtl = pProc->GetMemoryControllerConst();
    bool okHaltMode = pProc->IsHaltMode();
    std::array<uint16_t, 3> memory{};
    memory[0] = opcode;
    memory[1] = pMemCtl->GetWordView(address + 1 * 2, okHaltMode, true).value;
    memory[2] = pMemCtl->GetWordView(address + 2 * 2, okHaltMode, true).value;

    instructionHistory.RecordInstruction(okCpu, address, memory, pProc->GetPSW());

#if !defined(PRODUCT)
    uint32_t dwTrace = g_pBoard->GetTrace();
    uint32_t dwMask = okCpu ? TRACE_CPU : TRACE_PPU;
    if ((dwTrace & dwMask) == 0)
        return;

    TCHAR bufaddr[7];
    PrintOctalValue(bufaddr, address);

    const auto decoded = DisassembleInstruction(memory, address);
    if (!decoded) return;
    TCHAR buffer[64];
    FormatBuffer(buffer, _T("{}\t{}\t{}\t{}\r\n"), pProc->GetName(), bufaddr, decoded->mnemonic.c_str(), decoded->argument.c_str());

    DebugLog(buffer);
#endif // !PRODUCT
}

// Called by CPU/PPU when an interrupt has been taken, the new PC/PSW are already loaded;
// installed always, records history and writes trace
void CALLBACK Emulator_Interrupt_Callback(const CProcessor* pProc, uint16_t oldPc, uint16_t vector)
{
    bool okCpu = (pProc == g_pBoard->GetCPU());

    instructionHistory.RecordInterrupt(okCpu, oldPc, vector, pProc->GetPC(), pProc->GetPSW());

#if !defined(PRODUCT)
    uint32_t dwTrace = g_pBoard->GetTrace();
    uint32_t dwMask = okCpu ? TRACE_CPUINT : TRACE_PPUINT;
    if ((dwTrace & dwMask) == 0)
        return;

    TCHAR bufaddr[7], bufvector[7], bufnewpc[7];
    PrintOctalValue(bufaddr, oldPc);
    PrintOctalValue(bufvector, vector);
    PrintOctalValue(bufnewpc, pProc->GetPC());

    TCHAR buffer[80];
    FormatBuffer(buffer, _T("{}\t{}\tINT\t{}\t{}\t{}\r\n"),
            pProc->GetName(), bufaddr, bufvector, Emulator_GetInterruptName(vector), bufnewpc);

    DebugLog(buffer);
#endif // !PRODUCT
}

#if !defined(PRODUCT)

void Emulator_SetTrace(uint32_t dwTrace)
{
    g_pBoard->SetTrace(dwTrace);
}

#endif // !PRODUCT

bool CALLBACK Emulator_NetworkIn_Callback(uint8_t* pByte)
{
    DWORD dwBytesRead = 0;
    BOOL result = ::ReadFile(m_hEmulatorNetPort, pByte, 1, &dwBytesRead, nullptr);

    if (result && (dwBytesRead == 1))
        DebugLogFormat(_T("Net IN {:02x}\r\n"), (int)(*pByte));

    return result && (dwBytesRead == 1);
}

bool CALLBACK Emulator_NetworkOut_Callback(uint8_t byte)
{
    DWORD dwBytesWritten = 0;
    const BOOL result = ::WriteFile(m_hEmulatorNetPort, &byte, 1, &dwBytesWritten, nullptr);

    DebugLogFormat(_T("Net OUT {:02x}\r\n"), byte);

    return result && dwBytesWritten == 1;
}

bool Emulator_SetNetwork(bool networkOnOff, LPCTSTR networkPort)
{
    if (m_okEmulatorNetwork != networkOnOff)
    {
        m_Settings_NetStation_Bits = (uint16_t)Settings_GetNetStation();
        uint16_t rotateBits = (m_Settings_NetStation_Bits / 16);
        m_Settings_NetStation_Bits = (256 * (16 * rotateBits + m_Settings_NetStation_Bits));
        g_pBoard->SetNetStation(m_Settings_NetStation_Bits);

        if (networkOnOff)
        {
            // Prepare port name
            if (networkPort == nullptr) return false;
            const auto port = std::format(_T("\\\\.\\{}"), networkPort);

            // Open port
            m_hEmulatorNetPort = ::CreateFile(port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (m_hEmulatorNetPort == INVALID_HANDLE_VALUE)
            {
                DWORD dwError = ::GetLastError();
                AlertWarningFormat(_T("Failed to open network COM port (0x{:08x})."), dwError);
                return false;
            }

            // Set port settings
            DCB dcb;
            Settings_GetNetComConfig(&dcb);
            if (!::SetCommState(m_hEmulatorNetPort, &dcb))
            {
                DWORD dwError = ::GetLastError();
                ::CloseHandle(m_hEmulatorNetPort);
                m_hEmulatorNetPort = INVALID_HANDLE_VALUE;
                AlertWarningFormat(_T("Failed to configure the Network COM port (0x{:08x})."), dwError);
                return false;
            }

            // Set timeouts: ReadIntervalTimeout value of MAXDWORD, combined with zero values for both the ReadTotalTimeoutConstant
            // and ReadTotalTimeoutMultiplier members, specifies that the read operation is to return immediately with the bytes
            // that have already been received, even if no bytes have been received.
            COMMTIMEOUTS timeouts;
            ::memset(&timeouts, 0, sizeof(timeouts));
            timeouts.ReadIntervalTimeout = MAXDWORD;
            timeouts.WriteTotalTimeoutConstant = 100;
            if (!::SetCommTimeouts(m_hEmulatorNetPort, &timeouts))
            {
                DWORD dwError = ::GetLastError();
                ::CloseHandle(m_hEmulatorNetPort);
                m_hEmulatorNetPort = INVALID_HANDLE_VALUE;
                AlertWarningFormat(_T("Failed to set the Network COM port timeouts (0x{:08x})."), dwError);
                return false;
            }

            // Clear port input buffer
            ::PurgeComm(m_hEmulatorNetPort, PURGE_RXABORT | PURGE_RXCLEAR);

            // Set callbacks
            g_pBoard->SetNetworkCallbacks(Emulator_NetworkIn_Callback, Emulator_NetworkOut_Callback);
        }
        else
        {
            g_pBoard->SetNetworkCallbacks(nullptr, nullptr);  // Reset callbacks

            // Close port
            if (m_hEmulatorNetPort != INVALID_HANDLE_VALUE)
            {
                ::CloseHandle(m_hEmulatorNetPort);
                m_hEmulatorNetPort = INVALID_HANDLE_VALUE;
            }
        }
    }

    m_okEmulatorNetwork = networkOnOff;

    return true;
}

bool CALLBACK Emulator_SerialIn_Callback(BYTE* pByte)
{
    DWORD dwBytesRead = 0;
    BOOL result = ::ReadFile(m_hEmulatorComPort, pByte, 1, &dwBytesRead, nullptr);

    return result && (dwBytesRead == 1);
}

bool CALLBACK Emulator_SerialOut_Callback(BYTE byte)
{
    DWORD dwBytesWritten = 0;
    const BOOL result = ::WriteFile(m_hEmulatorComPort, &byte, 1, &dwBytesWritten, nullptr);

    return result && dwBytesWritten == 1;
}

bool Emulator_SetSerial(bool serialOnOff, LPCTSTR serialPort)
{
    if (m_okEmulatorSerial != serialOnOff)
    {
        if (serialOnOff)
        {
            // Prepare port name
            if (serialPort == nullptr) return false;
            const auto port = std::format(_T("\\\\.\\{}"), serialPort);

            // Open port
            m_hEmulatorComPort = ::CreateFile(port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (m_hEmulatorComPort == INVALID_HANDLE_VALUE)
            {
                DWORD dwError = ::GetLastError();
                AlertWarningFormat(_T("Failed to open COM port (0x{:08x})."), dwError);
                return false;
            }

            // Set port settings
            DCB dcb;
            Settings_GetSerialConfig(&dcb);
            if (!::SetCommState(m_hEmulatorComPort, &dcb))
            {
                DWORD dwError = ::GetLastError();
                ::CloseHandle(m_hEmulatorComPort);
                m_hEmulatorComPort = INVALID_HANDLE_VALUE;
                AlertWarningFormat(_T("Failed to configure the COM port (0x{:08x})."), dwError);
                return false;
            }

            // Set timeouts: ReadIntervalTimeout value of MAXDWORD, combined with zero values for both the ReadTotalTimeoutConstant
            // and ReadTotalTimeoutMultiplier members, specifies that the read operation is to return immediately with the bytes
            // that have already been received, even if no bytes have been received.
            COMMTIMEOUTS timeouts;
            ::memset(&timeouts, 0, sizeof(timeouts));
            timeouts.ReadIntervalTimeout = MAXDWORD;
            timeouts.WriteTotalTimeoutConstant = 100;
            if (!::SetCommTimeouts(m_hEmulatorComPort, &timeouts))
            {
                DWORD dwError = ::GetLastError();
                ::CloseHandle(m_hEmulatorComPort);
                m_hEmulatorComPort = INVALID_HANDLE_VALUE;
                AlertWarningFormat(_T("Failed to set the COM port timeouts (0x{:08x})."), dwError);
                return false;
            }

            // Clear port input buffer
            ::PurgeComm(m_hEmulatorComPort, PURGE_RXABORT | PURGE_RXCLEAR);

            // Set callbacks
            g_pBoard->SetSerialCallbacks(Emulator_SerialIn_Callback, Emulator_SerialOut_Callback);
        }
        else
        {
            g_pBoard->SetSerialCallbacks(nullptr, nullptr);  // Reset callbacks

            // Close port
            if (m_hEmulatorComPort != INVALID_HANDLE_VALUE)
            {
                ::CloseHandle(m_hEmulatorComPort);
                m_hEmulatorComPort = INVALID_HANDLE_VALUE;
            }
        }
    }

    m_okEmulatorSerial = serialOnOff;

    return true;
}

bool CALLBACK Emulator_ParallelOut_Callback(BYTE byte)
{
    return m_parallelOutput && std::fwrite(&byte, 1, 1, m_parallelOutput.get()) == 1;
}

void Emulator_SetParallel(bool parallelOnOff)
{
    if (m_okEmulatorParallel == parallelOnOff)
        return;

    if (!parallelOnOff)
    {
        g_pBoard->SetParallelOutCallback(nullptr);
        m_parallelOutput.reset();
    }
    else
    {
        m_parallelOutput.reset(::_tfopen(_T("printer.log"), _T("wb")));
        if (!m_parallelOutput)
        {
            AlertWarning(_T("Failed to open the printer output file."));
            return;
        }
        g_pBoard->SetParallelOutCallback(Emulator_ParallelOut_Callback);
    }

    m_okEmulatorParallel = parallelOnOff;
}

bool Emulator_SystemFrame()
{
    UKNC_PROFILE_SCOPE("Emulator frame");
    SoundGen_SetVolume(Settings_GetSoundVolume());

    ScreenView_ScanKeyboard();
    if (Settings_GetMouse())
        ScreenView_UpdateMouse();

    g_pBoard->SetCPUBreakpoints(cpuBreakpoints.Addresses());
    g_pBoard->SetPPUBreakpoints(ppuBreakpoints.Addresses());

    if (!g_pBoard->SystemFrame())  // Breakpoint hit
    {
        Emulator_SetTempCPUBreakpoint(0177777);
        Emulator_SetTempPPUBreakpoint(0177777);
        return false;
    }

    // Calculate frames per second
    m_nFrameCount++;
    uint32_t dwCurrentTicks = GetTickCount();
    uint32_t nTicksElapsed = dwCurrentTicks - m_dwTickCount;
    if (nTicksElapsed >= 1000)
    {
        double dFramesPerSecond = m_nFrameCount * 1000.0 / nTicksElapsed;
        double dSpeed = dFramesPerSecond / FRAMERATE * 100;
        TCHAR buffer[16];
        FormatBuffer(buffer, _T("{:03.0f}%"), dSpeed);
        MainWindow_SetStatusbarText(StatusbarPartFPS, buffer);

        bool floppyEngine = g_pBoard->IsFloppyEngineOn();
        MainWindow_SetStatusbarText(StatusbarPartFloppyEngine, floppyEngine ? _T("Motor") : nullptr);

        m_nFrameCount = 0;
        m_dwTickCount = dwCurrentTicks;
    }

    // Calculate emulator uptime
    m_nUptimeFrameCount++;
    if (m_nUptimeFrameCount >= FRAMERATE)
    {
        m_dwEmulatorUptime++;
        m_nUptimeFrameCount = 0;

        int seconds = (int) (m_dwEmulatorUptime % 60);
        int minutes = (int) (m_dwEmulatorUptime / 60 % 60);
        int hours   = (int) (m_dwEmulatorUptime / 3600 % 60);

        TCHAR buffer[20];
        FormatBuffer(buffer, _T("Uptime: {:02}:{:02}:{:02}"), hours, minutes, seconds);
        MainWindow_SetStatusbarText(StatusbarPartUptime, buffer);
    }

    // Update "Sound" indicator every 5 frames
    m_nEmulatorSoundChanges += g_pBoard->GetSoundChanges();
    if (m_nUptimeFrameCount % 5 == 0)
    {
        bool soundOn = m_nEmulatorSoundChanges > 0;
        MainWindow_SetStatusbarText(StatusbarPartSound, soundOn ? _T("Sound") : nullptr);
        m_nEmulatorSoundChanges = 0;
    }

    // Auto-boot option processing: select boot menu item and press Enter
    if (Option_AutoBoot > 0)
    {
        BYTE digitKeyScan = m_arrDigitKeyScans[Option_AutoBoot];
        if (m_dwEmulatorUptime == 2 && m_nUptimeFrameCount == 6)
            ScreenView_KeyEvent(digitKeyScan, TRUE);  // Press the digit key
        else if (m_dwEmulatorUptime == 2 && m_nUptimeFrameCount == 10)
            ScreenView_KeyEvent(digitKeyScan, FALSE);  // Release the digit key
        else if (m_dwEmulatorUptime == 2 && m_nUptimeFrameCount == 16)
            ScreenView_KeyEvent(0153, TRUE);  // Press "Enter"
        else if (m_dwEmulatorUptime == 2 && m_nUptimeFrameCount == 20)
        {
            ScreenView_KeyEvent(0153, FALSE);  // Release "Enter"
            Option_AutoBoot = 0;  // All done
        }
    }

    return true;
}

uint32_t Emulator_GetUptime()
{
    return m_dwEmulatorUptime;
}

// Update cached values after Run or Step
void Emulator_OnUpdate()
{
    // Update stored PC value
    g_wEmulatorPrevCpuPC = g_wEmulatorCpuPC;
    g_wEmulatorCpuPC = g_pBoard->GetCPU()->GetPC();
    g_wEmulatorPrevPpuPC = g_wEmulatorPpuPC;
    g_wEmulatorPpuPC = g_pBoard->GetPPU()->GetPC();

    // Update memory change flags
    for (int plane = 0; plane < 3; plane++)
    {
        uint8_t* pOld = g_pEmulatorRam[plane];
        uint8_t* pChanged = g_pEmulatorChangedRam[plane];
        uint16_t addr = 0;
        do
        {
            uint8_t newvalue = g_pBoard->GetRAMByte(plane, addr);
            uint8_t oldvalue = *pOld;
            *pChanged = newvalue ^ oldvalue;
            *pOld = newvalue;
            addr++;
            pOld++;  pChanged++;
        }
        while (addr < 65535);
    }
}

// Get RAM change flag for RAM word
//   addrtype - address mode - see ADDRTYPE_XXX constants
uint16_t Emulator_GetChangeRamStatus(int addrtype, uint16_t address)
{
    switch (addrtype)
    {
    case ADDRTYPE_RAM0:
    case ADDRTYPE_RAM1:
    case ADDRTYPE_RAM2:
        return *((uint16_t*)(g_pEmulatorChangedRam[addrtype] + address));
    case ADDRTYPE_RAM12:
        if (address < 0170000)
            return MAKEWORD(
                    *(g_pEmulatorChangedRam[1] + address / 2),
                    *(g_pEmulatorChangedRam[2] + address / 2));
        else
            return 0;
    default:
        return 0;
    }
}

bool Emulator_LoadROMCartridge(int slot, LPCTSTR sFilePath)
{
    // Open file
    UniqueFile fpFile(::_tfsopen(sFilePath, _T("rb"), _SH_DENYWR));
    if (!fpFile)
    {
        AlertWarning(_T("Failed to load ROM cartridge image."));
        return false;
    }

    // Allocate memory
    std::array<BYTE, 24 * 1024> image{};
    if (::fread(image.data(), 1, image.size(), fpFile.get()) != image.size())
        return false;
    g_pBoard->LoadROMCartridge(slot, std::span<const uint8_t, 24 * 1024>{image});

    //TODO: Save the file name for a future SaveImage() call

    return true;
}

void Emulator_PrepareScreenRGB32(std::span<uint32_t> pixels, std::span<const uint32_t> colors)
{
    UKNC_PROFILE_SCOPE("Video decode");
    if (pixels.size() < UKNC_SCREEN_WIDTH * UKNC_SCREEN_HEIGHT || colors.size() < 16 * 8) return;
    if (!g_okEmulatorInitialized) return;

    // Tag parsing loop
    BYTE cursorYRGB = 0;
    bool okCursorType = false;
    BYTE cursorPos = 128;
    bool cursorOn = false;
    BYTE cursorAddress = 0;  // Address of graphical cursor
    WORD address = 0000270;  // Tag sequence start address
    bool okTagSize = false;  // Tag size: TRUE - 4-word, false - 2-word (first tag is always 2-word)
    bool okTagType = false;  // Type of 4-word tag: TRUE - set palette, false - set params
    int scale = 1;           // Horizontal scale: 1, 2, 4, or 8
    uint32_t palette = 0;       // Palette
    uint32_t palettecurrent[8];  memset(palettecurrent, 0, sizeof(palettecurrent)); // Current palette; update each time we change the "palette" variable
    BYTE pbpgpr = 0;         // 3-bit Y-value modifier
    for (int yy = 0; yy < 307; yy++)
    {
        if (okTagSize)  // 4-word tag
        {
            WORD tag1 = g_pBoard->GetRAMWord(0, address);
            address += 2;
            WORD tag2 = g_pBoard->GetRAMWord(0, address);
            address += 2;

            if (okTagType)  // 4-word palette tag
            {
                palette = MAKELONG(tag1, tag2);
            }
            else  // 4-word params tag
            {
                scale = (tag2 >> 4) & 3;  // Bits 4-5 - new scale value
                pbpgpr = (BYTE)((7 - (tag2 & 7)) << 4);  // Y-value modifier
                cursorYRGB = (BYTE)(tag1 & 15);  // Cursor color
                okCursorType = ((tag1 & 16) != 0);  // true - graphical cursor, false - symbolic cursor
                //ASSERT(okCursorType == 0);  //DEBUG
                cursorPos = (BYTE)(((tag1 >> 8) >> scale) & 0x7f);  // Cursor position in the line
                cursorAddress = (BYTE)((tag1 >> 5) & 7);
                scale = 1 << scale;
            }
            for (BYTE c = 0; c < 8; c++)  // Update palettecurrent
            {
                BYTE valueYRGB = (BYTE) (palette >> (c << 2)) & 15;
                palettecurrent[c] = colors[pbpgpr | valueYRGB];
                //if (pbpgpr != 0) DebugLogFormat(_T("pbpgpr {:02x}\r\n"), pbpgpr | valueYRGB);
            }
        }

        WORD addressBits = g_pBoard->GetRAMWord(0, address);  // The word before the last word - is address of bits from all three memory planes
        address += 2;

        // Calculate size, type and address of the next tag
        WORD tagB = g_pBoard->GetRAMWord(0, address);  // Last word of the tag - is address and type of the next tag
        okTagSize = (tagB & 2) != 0;  // Bit 1 shows size of the next tag
        if (okTagSize)
        {
            address = tagB & ~7;
            okTagType = (tagB & 4) != 0;  // Bit 2 shows type of the next tag
        }
        else
            address = tagB & ~3;
        if ((tagB & 1) != 0)
            cursorOn = !cursorOn;

        // Draw bits into the bitmap, from line 20 to line 307
        if (yy < 19 /*|| yy > 306*/)
            continue;

        // Loop thru bits from addressBits, planes 0,1,2
        // For each pixel:
        //   Get bit from planes 0,1,2 and make value
        //   Map value to palette; result is 4-bit value YRGB
        //   Translate value to 24-bit RGB
        //   Put value to m_bits; repeat using scale value

        int xr = 640;
        int y = yy - 19;
        uint32_t* pBits = pixels.data() + y * 640;
        int pos = 0;
        for (;;)
        {
            // Get bit from planes 0,1,2
            BYTE src0 = g_pBoard->GetRAMByte(0, addressBits);
            BYTE src1 = g_pBoard->GetRAMByte(1, addressBits);
            BYTE src2 = g_pBoard->GetRAMByte(2, addressBits);
            // Loop through the bits of the byte
            int bit = 0;
            for (;;)
            {
                uint32_t valueRGB;
                if (cursorOn && (pos == cursorPos) && (!okCursorType || (okCursorType && bit == cursorAddress)))
                    valueRGB = colors[cursorYRGB];  // 4-bit to 32-bit color
                else
                {
                    // Make 3-bit value from the bits
                    BYTE value012 = (src0 & 1) | ((src1 & 1) << 1) | ((src2 & 1) << 2);
                    valueRGB = palettecurrent[value012];  // 3-bit to 32-bit color
                }

                // Put value to m_bits; repeat using scale value
                //WAS: for (int s = 0; s < scale; s++) *pBits++ = valueRGB;
                switch (scale)
                {
                case 8:
                    *pBits++ = valueRGB;
                    *pBits++ = valueRGB;
                    *pBits++ = valueRGB;
                    *pBits++ = valueRGB;
                case 4:
                    *pBits++ = valueRGB;
                    *pBits++ = valueRGB;
                case 2:
                    *pBits++ = valueRGB;
                case 1:
                    *pBits++ = valueRGB;
                default:
                    break;
                }

                xr -= scale;

                if (bit == 7)
                    break;
                bit++;

                // Shift to the next bit
                src0 >>= 1;
                src1 >>= 1;
                src2 >>= 1;
            }
            if (xr <= 0)
                break;  // End of line
            addressBits++;  // Go to the next byte
            pos++;
        }
    }
}

void Emulator_PrepareScreenToText(std::span<uint32_t> pixels, std::span<const uint32_t> colors)
{
    if (pixels.size() < UKNC_SCREEN_WIDTH * UKNC_SCREEN_HEIGHT || colors.size() < 8) return;
    if (!g_okEmulatorInitialized) return;

    // Tag parsing loop
    WORD address = 0000270;  // Tag sequence start address
    bool okTagSize = false;  // Tag size: TRUE - 4-word, false - 2-word (first tag is always 2-word)
    bool okTagType = false;  // Type of 4-word tag: TRUE - set palette, false - set params
    int scale = 1;           // Horizontal scale: 1, 2, 4, or 8
    for (int yy = 0; yy < 307; yy++)
    {
        if (okTagSize)  // 4-word tag
        {
            //WORD tag1 = g_pBoard->GetRAMWord(0, address);
            address += 2;
            WORD tag2 = g_pBoard->GetRAMWord(0, address);
            address += 2;

            if (okTagType)  // 4-word palette tag
            {
                //palette = MAKELONG(tag1, tag2);
            }
            else  // 4-word params tag
            {
                scale = (tag2 >> 4) & 3;  // Bits 4-5 - new scale value
                scale = 1 << scale;
            }
        }

        WORD addressBits = g_pBoard->GetRAMWord(0, address);  // The word before the last word - is address of bits from all three memory planes
        address += 2;

        // Calculate size, type and address of the next tag
        WORD tagB = g_pBoard->GetRAMWord(0, address);  // Last word of the tag - is address and type of the next tag
        okTagSize = (tagB & 2) != 0;  // Bit 1 shows size of the next tag
        if (okTagSize)
        {
            address = tagB & ~7;
            okTagType = (tagB & 4) != 0;  // Bit 2 shows type of the next tag
        }
        else
            address = tagB & ~3;

        // Draw bits into the bitmap, from line 20 to line 307
        if (yy < 19 /*|| yy > 306*/)
            continue;

        // Loop thru bits from addressBits, planes 0,1,2
        int xr = 640;
        int y = yy - 19;
        uint32_t* pBits = pixels.data() + y * 640;
        int pos = 0;
        for (;;)
        {
            // Get bit from planes 0,1,2
            BYTE src0 = g_pBoard->GetRAMByte(0, addressBits);
            BYTE src1 = g_pBoard->GetRAMByte(1, addressBits);
            BYTE src2 = g_pBoard->GetRAMByte(2, addressBits);
            // Loop through the bits of the byte
            int bit = 0;
            for (;;)
            {
                // Make 3-bit value from the bits
                BYTE value012 = (src0 & 1) | ((src1 & 1) << 1) | ((src2 & 1) << 2);
                uint32_t valueRGB = colors[value012];  // 3-bit to 32-bit color

                // Put value to m_bits; (do not repeat using scale value)
                *pBits++ = valueRGB;
                xr -= scale;

                if (bit == 7)
                    break;
                bit++;

                // Shift to the next bit
                src0 >>= 1;
                src1 >>= 1;
                src2 >>= 1;
            }
            if (xr <= 0)
                break;  // End of line
            addressBits++;  // Go to the next byte
            pos++;
        }
    }
}


//////////////////////////////////////////////////////////////////////
//
// Emulator image format - see CMotherboard::SaveToImage()
// Image header format (32 bytes):
//   4 bytes        UKNC_IMAGE_HEADER1
//   4 bytes        UKNC_IMAGE_HEADER2
//   4 bytes        UKNC_IMAGE_VERSION
//   4 bytes        UKNC_IMAGE_SIZE
//   4 bytes        UKNC uptime
//   12 bytes       Not used
//TODO: 256 bytes * 2 - Cartridge 1..2 path
//TODO: 256 bytes * 4 - Floppy 1..4 path
//TODO: 256 bytes * 2 - Hard 1..2 path

bool Emulator_SaveImage(LPCTSTR sFilePath)
{
    return snapshot::Save(sFilePath, *g_pBoard, m_dwEmulatorUptime);
}

bool Emulator_LoadImage(LPCTSTR sFilePath)
{
    return snapshot::Load(sFilePath, *g_pBoard, m_dwEmulatorUptime,
                          instructionHistory, Emulator_Stop);
}

//////////////////////////////////////////////////////////////////////
