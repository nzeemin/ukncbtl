/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

/// \file Board.cpp  Motherboard class implementation

#include "stdafx.h"
#include "util/Profiler.h"
#include "Emubase.h"
#include "Board.h"
#include "ImageSerialization.h"


//////////////////////////////////////////////////////////////////////
// Bus devices

static constexpr BusAddressRange ProcessorTimerAddressRanges[] =
{
    {0177710, 6},  // 177710-177715
};
class CBusDeviceProcessorTimer : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("Processor timer"); }
    std::span<const BusAddressRange> GetAddressRanges() const override { return ProcessorTimerAddressRanges; }
};

static constexpr BusAddressRange CpuChannelsAddressRanges[] =
{
    {0176660, 16},
    {0177560, 8},
};
class CBusDeviceCpuChannels : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("CPU-PPU channels"); }
    std::span<const BusAddressRange> GetAddressRanges() const override { return CpuChannelsAddressRanges; }
};

static constexpr BusAddressRange PpuChannelsAddressRanges[] =
{
    {0177060, 16},
};
class CBusDevicePpuChannels : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("CPU-PPU channels"); }
    std::span<const BusAddressRange> GetAddressRanges() const override { return PpuChannelsAddressRanges; }
};

static constexpr BusAddressRange NetworkAdapterAddressRanges[] =
{
    {0176560, 8},
};
class CBusDeviceNetworkAdapter : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("Network adapter"); }
    std::span<const BusAddressRange> GetAddressRanges() const override { return NetworkAdapterAddressRanges; }
};

static constexpr BusAddressRange SerialPortAddressRanges[] =
{
    {0176570, 8},
};
class CBusDeviceSerialPort : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("Serial port"); }
    std::span<const BusAddressRange> GetAddressRanges() const override { return SerialPortAddressRanges; }
};

static constexpr BusAddressRange CpuMemoryAccessAddressRanges[] =
{
    {0176640, 8},
};
class CBusDeviceCpuMemoryAccess : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("Memory access"); }
    std::span<const BusAddressRange> GetAddressRanges() const override { return CpuMemoryAccessAddressRanges; }
};

static constexpr BusAddressRange PpuMemoryAccessAddressRanges[] =
{
    {0177010, 16},
    {0177054, 2},
};
class CBusDevicePpuMemoryAccess : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("Memory access"); }
    std::span<const BusAddressRange> GetAddressRanges() const override { return PpuMemoryAccessAddressRanges; }
};

static constexpr BusAddressRange ProgrammablePortAddressRanges[] =
{
    {0177100, 4},  // i8255 ports
};
class CBusDeviceProgrammablePort : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("Programmable port"); }
    std::span<const BusAddressRange> GetAddressRanges() const override { return ProgrammablePortAddressRanges; }
};

static constexpr BusAddressRange KeyboardAddressRanges[] =
{
    {0177700, 6},
};
class CBusDeviceKeyboard : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("Keyboard"); }
    std::span<const BusAddressRange> GetAddressRanges() const override { return KeyboardAddressRanges; }
};

static constexpr BusAddressRange FloppyControllerAddressRanges[] =
{
    {0177130, 4},
};
class CBusDeviceFloppyController : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("Floppy controller"); }
    std::span<const BusAddressRange> GetAddressRanges() const override { return FloppyControllerAddressRanges; }
};

static constexpr BusAddressRange HardDriveAddressRanges[] =
{
    {0110000, 010000},
};
class CBusDeviceHardDrive : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("Hard drive"); }
    std::span<const BusAddressRange> GetAddressRanges() const override { return HardDriveAddressRanges; }
};


//////////////////////////////////////////////////////////////////////

CMotherboard::CMotherboard ()
{
    memset(freq_per, 0, sizeof(freq_per));
    memset(freq_out, 0, sizeof(freq_out));
    memset(freq_enable, 0, sizeof(freq_enable));
    m_multiply = 1;
    memset(m_chancputx, 0, sizeof(m_chancputx));
    memset(m_chancpurx, 0, sizeof(m_chancpurx));
    memset(m_chanpputx, 0, sizeof(m_chanpputx));
    memset(m_chanppurx, 0, sizeof(m_chanppurx));

    m_dwTrace = TRACE_NONE;
    m_CPUbps = m_PPUbps = nullptr;
    m_TapeReadCallback = nullptr;
    m_TapeWriteCallback = nullptr;
    m_nTapeSampleRate = 0;
    m_SoundGenCallback = nullptr;
    m_SoundPrevValue = 0;
    m_SerialInCallback = nullptr;
    m_SerialOutCallback = nullptr;
    m_ParallelOutCallback = nullptr;
    m_NetworkInCallback = nullptr;
    m_NetworkOutCallback = nullptr;
    m_TerminalOutCallback = nullptr;
    m_okSoundAY = m_okSoundCovox = false;

    // Create devices
    m_pCPU = std::make_unique<CProcessor>(_T("CPU"));
    m_pPPU = std::make_unique<CProcessor>(_T("PPU"));
    m_pFirstMemCtl = std::make_unique<CFirstMemoryController>();
    m_pSecondMemCtl = std::make_unique<CSecondMemoryController>();
    m_pFloppyCtl = std::make_unique<CFloppyController>();
    m_pHardDrives[0] = nullptr;
    m_pHardDrives[1] = nullptr;
    m_pSoundAY[0] = std::make_unique<CSoundAY>();
    m_pSoundAY[1] = std::make_unique<CSoundAY>();
    m_pSoundAY[2] = std::make_unique<CSoundAY>();

    // Connect devices
    m_pCPU->AttachMemoryController(m_pFirstMemCtl.get());
    m_pFirstMemCtl->Attach(this, m_pCPU.get());
    m_pPPU->AttachMemoryController(m_pSecondMemCtl.get());
    m_pSecondMemCtl->Attach(this, m_pPPU.get());

    // Allocate memory for RAM and ROM
    m_pRAM[0] = std::make_unique<uint8_t[]>(65536);
    m_pRAM[1] = std::make_unique<uint8_t[]>(65536);
    m_pRAM[2] = std::make_unique<uint8_t[]>(65536);
    m_pROM = std::make_unique<uint8_t[]>(32768);
    m_pROMCart[0] = nullptr;
    m_pROMCart[1] = nullptr;

    // Prepare bus devices
    m_cpuDeviceOwners[0] = std::make_unique<CBusDeviceProcessorTimer>();
    m_cpuDeviceOwners[1] = std::make_unique<CBusDeviceCpuChannels>();
    m_cpuDeviceOwners[2] = std::make_unique<CBusDeviceNetworkAdapter>();
    m_cpuDeviceOwners[3] = std::make_unique<CBusDeviceSerialPort>();
    m_cpuDeviceOwners[4] = std::make_unique<CBusDeviceCpuMemoryAccess>();
    m_ppuDeviceOwners[0] = std::make_unique<CBusDeviceProcessorTimer>();
    m_ppuDeviceOwners[1] = std::make_unique<CBusDevicePpuChannels>();
    m_ppuDeviceOwners[2] = std::make_unique<CBusDevicePpuMemoryAccess>();
    m_ppuDeviceOwners[3] = std::make_unique<CBusDeviceProgrammablePort>();
    m_ppuDeviceOwners[4] = std::make_unique<CBusDeviceKeyboard>();
    m_ppuDeviceOwners[5] = std::make_unique<CBusDeviceFloppyController>();
    m_ppuDeviceOwners[6] = std::make_unique<CBusDeviceHardDrive>();

    for (size_t i = 0; i < m_cpuDeviceOwners.size(); ++i)
        m_pCpuDevices[i] = m_cpuDeviceOwners[i].get();
    for (size_t i = 0; i < m_ppuDeviceOwners.size(); ++i)
        m_pPpuDevices[i] = m_ppuDeviceOwners[i].get();

    m_pFirstMemCtl->AttachDevices(m_pCpuDevices);
    m_pSecondMemCtl->AttachDevices(m_pPpuDevices);
}

CMotherboard::~CMotherboard () = default;

void CMotherboard::SetTrace(uint32_t dwTrace)
{
    m_dwTrace = dwTrace;
    if (m_pFloppyCtl != nullptr)
        m_pFloppyCtl->SetTrace((dwTrace & TRACE_FLOPPY) != 0);
}

void CMotherboard::SetInstructionCallback(INSTRUCTIONCALLBACK callback)
{
    m_pCPU->SetInstructionCallback(callback);
    m_pPPU->SetInstructionCallback(callback);
}

void CMotherboard::SetInterruptCallback(INTERRUPTCALLBACK callback)
{
    m_pCPU->SetInterruptCallback(callback);
    m_pPPU->SetInterruptCallback(callback);
}

void CMotherboard::Reset ()
{
    m_pPPU->SetDCLOPin(true);
    m_pPPU->SetACLOPin(true);

    ResetFloppy();

    if (m_pHardDrives[0] != nullptr)
        m_pHardDrives[0]->Reset();
    if (m_pHardDrives[1] != nullptr)
        m_pHardDrives[1]->Reset();

    m_cputicks = 0;
    m_pputicks = 0;
    m_lineticks = 0;
    m_timer = 0;
    m_timerreload = 0;
    m_timerflags = 0;
    m_timerdivider = 0;

    m_chan0disabled = 0;

    m_scanned_key = 0;
    memset(m_kbd_matrix, 0, sizeof(m_kbd_matrix));
    m_kbd_matrix[3].row_Y = 0xFF;

    memset(m_nSoundAYReg, 0, sizeof(m_nSoundAYReg));
    m_pSoundAY[0]->Reset();
    m_pSoundAY[1]->Reset();
    m_pSoundAY[2]->Reset();

    //ChanResetByCPU();
    //ChanResetByPPU();

    // We always start with PPU
    m_pPPU->SetDCLOPin(false);
    m_pPPU->SetACLOPin(false);
}

void CMotherboard::LoadROM(const uint8_t* pBuffer)  // Load 32 KB ROM image from the buffer
{
    memcpy(m_pROM.get(), pBuffer, 32768);
}

void CMotherboard::LoadROMCartridge(int cartno, const uint8_t* pBuffer)  // Load 24 KB ROM cartridge image
{
    ASSERT(cartno == 1 || cartno == 2);  // Only two cartridges, #1 and #2
    ASSERT(pBuffer != nullptr);

    int cartindex = cartno - 1;
    if (m_pROMCart[cartindex] == nullptr)
        m_pROMCart[cartindex] = std::make_unique<uint8_t[]>(24 * 1024);

    memcpy(m_pROMCart[cartindex].get(), pBuffer, 24 * 1024);
}

void CMotherboard::LoadRAM(int plan, const uint8_t* pBuffer)  // Load 32 KB RAM image from the buffer
{
    ASSERT(plan >= 0 && plan <= 2);
    memcpy(m_pRAM[plan].get(), pBuffer, 32768);
}

void CMotherboard::SetNetStation(uint16_t station)
{
    CFirstMemoryController* pMemCtl = static_cast<CFirstMemoryController*>(m_pFirstMemCtl.get());
    pMemCtl->m_NetStation = station;
}


// Floppy ////////////////////////////////////////////////////////////

bool CMotherboard::IsFloppyImageAttached(int slot) const
{
    ASSERT(slot >= 0 && slot < 4);
    return m_pFloppyCtl->IsAttached(slot);
}

bool CMotherboard::IsFloppyReadOnly(int slot) const
{
    ASSERT(slot >= 0 && slot < 4);
    return m_pFloppyCtl->IsReadOnly(slot);
}

bool CMotherboard::IsFloppyEngineOn() const
{
    return m_pFloppyCtl->IsEngineOn();
}

bool CMotherboard::AttachFloppyImage(int slot, LPCTSTR sFileName)
{
    ASSERT(slot >= 0 && slot < 4);
    return m_pFloppyCtl->AttachImage(slot, sFileName);
}

void CMotherboard::DetachFloppyImage(int slot)
{
    ASSERT(slot >= 0 && slot < 4);
    m_pFloppyCtl->DetachImage(slot);
}


// ROM cartridge /////////////////////////////////////////////////////

bool CMotherboard::IsROMCartridgeLoaded(int cartno) const
{
    ASSERT(cartno == 1 || cartno == 2);  // Only two cartridges, #1 and #2
    int cartindex = cartno - 1;
    return (m_pROMCart[cartindex] != nullptr);
}

void CMotherboard::UnloadROMCartridge(int cartno)
{
    ASSERT(cartno == 1 || cartno == 2);  // Only two cartridges, #1 and #2
    int cartindex = cartno - 1;
    m_pROMCart[cartindex].reset();
}


// Hard Drives ///////////////////////////////////////////////////////

bool CMotherboard::IsHardImageAttached(int slot) const
{
    ASSERT(slot >= 1 && slot <= 2);
    return (m_pHardDrives[slot - 1] != nullptr);
}

bool CMotherboard::IsHardImageReadOnly(int slot) const
{
    ASSERT(slot >= 1 && slot <= 2);
    CHardDrive* pHardDrive = m_pHardDrives[slot - 1].get();
    if (pHardDrive == nullptr) return false;
    return pHardDrive->IsReadOnly();
}

bool CMotherboard::AttachHardImage(int slot, LPCTSTR sFileName)
{
    ASSERT(slot >= 1 && slot <= 2);

    auto drive = std::make_unique<CHardDrive>();
    if (!drive->AttachImage(sFileName))
        return false;

    drive->Reset();
    m_pHardDrives[slot - 1] = std::move(drive);
    m_pSecondMemCtl->UpdateMemoryMap();
    return true;
}
void CMotherboard::DetachHardImage(int slot)
{
    ASSERT(slot >= 1 && slot <= 2);

    m_pHardDrives[slot - 1].reset();
}

uint16_t CMotherboard::GetHardPortWord(int slot, uint16_t port)
{
    ASSERT(slot >= 1 && slot <= 2);

    if (m_pHardDrives[slot - 1] == nullptr) return 0;
    port = (uint16_t) (~(port >> 1) & 7) | 0x1f0;
    uint16_t data = m_pHardDrives[slot - 1]->ReadPort(port);
    return ~data;  // QBUS inverts the bits
}
void CMotherboard::SetHardPortWord(int slot, uint16_t port, uint16_t data)
{
    ASSERT(slot >= 1 && slot <= 2);

    if (m_pHardDrives[slot - 1] == nullptr) return;
    port = (uint16_t) (~(port >> 1) & 7) | 0x1f0;
    data = ~data;  // QBUS inverts the bits
    m_pHardDrives[slot - 1]->WritePort(port, data);
}


// Memory control ////////////////////////////////////////////////////

uint16_t CMotherboard::GetROMWord(uint16_t offset) const
{
    ASSERT(offset < 32768);
    offset &= 0xFFFE;
    return static_cast<uint16_t>(m_pROM[offset] | (m_pROM[offset + 1] << 8));
}
uint8_t CMotherboard::GetROMByte(uint16_t offset) const
{
    ASSERT(offset < 32768);
    return m_pROM[offset];
}

uint16_t CMotherboard::GetROMCartWord(int cartno, uint16_t offset) const
{
    ASSERT(cartno == 1 || cartno == 2);
    ASSERT(offset < 24 * 1024 - 1);
    int cartindex = cartno - 1;
    if (m_pROMCart[cartindex] == nullptr)
        return 0177777;
    offset &= 0xFFFE;
    return static_cast<uint16_t>(m_pROMCart[cartindex][offset] |
        (m_pROMCart[cartindex][offset + 1] << 8));
}
uint8_t CMotherboard::GetROMCartByte(int cartno, uint16_t offset) const
{
    ASSERT(cartno == 1 || cartno == 2);
    ASSERT(offset < 24 * 1024);
    int cartindex = cartno - 1;
    if (m_pROMCart[cartindex] == nullptr)
        return 0377;
    uint8_t* p = m_pROMCart[cartindex].get() + offset;
    return *p;
}


//////////////////////////////////////////////////////////////////////


void CMotherboard::ResetFloppy()
{
    m_pFloppyCtl->Reset();
}

void CMotherboard::Tick8000 ()
{
    m_pCPU->Execute();
}
void CMotherboard::Tick6250 ()
{
    m_pPPU->Execute();
}
void CMotherboard::Tick50 ()
{
    if ((m_pPPU->GetMemoryController()->GetPortView(0177054) & 0400) == 0)
        m_pPPU->TickEVNT();
    if ((m_pPPU->GetMemoryController()->GetPortView(0177054) & 01000) == 0)
        m_pCPU->TickEVNT();
}

void CMotherboard::ExecuteCPU ()
{
    m_pCPU->Execute();
}

void CMotherboard::ExecutePPU ()
{
    m_pPPU->Execute();
}

void CMotherboard::TimerTick() // Timer Tick, 2uS -- dividers are within timer routine
{
    if ((m_timerflags & 1) == 0)  // Timer is off
        return;
    if ((m_timerflags & 040) != 0)  // External event is ready
        return;

    m_timerdivider++;

    int flag = 0;
    switch ((m_timerflags >> 1) & 3)
    {
    case 0: //2uS
        flag = 1;
        m_timerdivider = 0;
        break;
    case 1: //4uS
        if (m_timerdivider >= 2)
        {
            flag = 1;
            m_timerdivider = 0;
        }
        break;
    case 2: //8uS
        if (m_timerdivider >= 4)
        {
            flag = 1;
            m_timerdivider = 0;
        }
        break;
    case 3:
        if (m_timerdivider >= 8)
        {
            flag = 1;
            m_timerdivider = 0;
        }
        break;
    }

    if (flag == 0)  // Nothing happened
        return;

    m_timer--;
    m_timer &= 07777;  // 12 bit only

    if (m_timer == 0)
    {
        if (m_timerflags & 0200)
            m_timerflags |= 010;  // Overflow
        m_timerflags |= 0200;  // 0

        if ((m_timerflags & 0100) && (m_timerflags & 0200))
        {
            m_pPPU->InterruptVIRQ(2, 0304);
        }

        m_timer = m_timerreload & 07777; // Reload it
    }
}
uint16_t CMotherboard::GetTimerValue()  // Returns current timer value
{
    if ((m_timerflags & 0240) == 0)
        return m_timer;

    m_timerflags &= ~0240;  // Clear flags
    uint16_t res = m_timer;
    m_timer = m_timerreload & 07777; // Reload it
    return res;
}
uint16_t CMotherboard::GetTimerReload()  // Returns timer reload value
{
    return m_timerreload;
}
uint16_t CMotherboard::GetTimerState() // Returns timer state
{
    uint16_t res = m_timerflags;
    m_timerflags &= ~010;  // Clear overflow
    return res;
}

void CMotherboard::SetTimerReload(uint16_t val)  // Sets timer reload value
{
    m_timerreload = val & 07777;
    if ((m_timerflags & 1) == 0)
        m_timer = m_timerreload;
}

void CMotherboard::SetTimerState(uint16_t val) // Sets timer state
{
    // 753   200 40 10
    if ((val & 1) && ((m_timerflags & 1) == 0))
        m_timer = m_timerreload & 07777;

    m_timerflags &= 0250;  // Clear everything but bits 7,5,3
    m_timerflags |= (val & (~0250));  // Preserve bits 753

    switch ((m_timerflags >> 1) & 3)
    {
    case 0: //2uS
        m_multiply = 8;
        break;
    case 1: //4uS
        m_multiply = 4;
        break;
    case 2: //8uS
        m_multiply = 2;
        break;
    case 3:
        m_multiply = 1;
        break;
    }
}

void CMotherboard::SetSoundAYReg(int chip, uint8_t reg)
{
    if (chip >= 0 && chip < 3)
        m_nSoundAYReg[chip] = reg;
}

void CMotherboard::SetSoundAYVal(int chip, uint8_t val)
{
    if (m_okSoundAY && chip >= 0 && chip < 3)
    {
        m_pSoundAY[chip]->SetReg(m_nSoundAYReg[chip], val);
    }
}

void CMotherboard::SetMouse(bool onoff)
{
    static_cast<CSecondMemoryController*>(m_pSecondMemCtl.get())->SetMouse(onoff);
}

void CMotherboard::MouseMove(int16_t dx, int16_t dy, bool btnLeft, bool btnRight)
{
    static_cast<CSecondMemoryController*>(m_pSecondMemCtl.get())->MouseMove(dx, dy, btnLeft, btnRight);
}


void CMotherboard::DebugTicks()
{
    if (!m_pPPU->IsStopped())
    {
        while (m_pPPU->InterruptProcessing()) {}
        m_pPPU->CommandExecution();
        while (m_pPPU->InterruptProcessing()) {}
    }
    if (!m_pCPU->IsStopped())
    {
        while (m_pCPU->InterruptProcessing()) {}
        m_pCPU->CommandExecution();
        while (m_pCPU->InterruptProcessing()) {}
    }
    if (!m_pPPU->IsStopped()) while (m_pPPU->InterruptProcessing()) {}
}

/*
Each frame is 1/50 second = 20 ms = 10000 ticks, 1 tick = 2 us of machine time.
* 10000 system timer ticks - on every tick
* 1 EVNT signal, at tick 0 of the frame
* 160000 CPU ticks - 16 times per tick
* 125000 PPU ticks - 12.5 times per tick
* Drawing 312 lines
* 625 FDD ticks - every 32nd tick
* 48 COM port exchange ticks - every 416th tick
* 8?? NET port exchange ticks - every 64th tick ???
*/

static constexpr auto FRAMETICKS = 10000;  // Number of ticks in one frame
#define SYSTEMFRAME_EXECUTE_CPU     { m_pCPU->Execute(); }
#define SYSTEMFRAME_EXECUTE_PPU     { m_pPPU->Execute(); }
#define SYSTEMFRAME_EXECUTE_BP_CPU  { m_pCPU->Execute(); if (m_CPUbps != nullptr) \
    { const uint16_t* pbps = m_CPUbps; while(*pbps != 0177777) { if (m_pCPU->GetPC() == *pbps++) return false; } } }
#define SYSTEMFRAME_EXECUTE_BP_PPU  { m_pPPU->Execute(); if (m_PPUbps != nullptr) \
    { const uint16_t* pbps = m_PPUbps; while(*pbps != 0177777) { if (m_pPPU->GetPC() == *pbps++) return false; } } }
bool CMotherboard::SystemFrame()
{
    UKNC_PROFILE_SCOPE("Core frame");
    int frameticks = 0;  // count 10000 ticks

    m_SoundChanges = 0;
    int soundSamplesPerFrame = SAMPLERATE / FRAMERATE, soundBrasErr = 0;

    const int serialOutTicks = FRAMETICKS / (9600 / FRAMERATE);
    int serialTxCount = 0;
    const int networkOutTicks = 7; // FRAMETICKS / (57600 / FRAMERATE);
    int networkTxCount = 0;

    constexpr uint64_t tapeTicksPerSecond = FRAMETICKS * FRAMERATE;

    do
    {
        TimerTick();  // System timer tick

        if (frameticks == 0)
            Tick50();  // 1/50 timer event

        // CPU - 16 times, PPU - 12.5 times
        if (m_CPUbps == nullptr && m_PPUbps == nullptr)  // No breakpoints, no need to check
        {
            /*  0 */  SYSTEMFRAME_EXECUTE_CPU  SYSTEMFRAME_EXECUTE_PPU
            /*  1 */  SYSTEMFRAME_EXECUTE_CPU  SYSTEMFRAME_EXECUTE_PPU
            /*  2 */  SYSTEMFRAME_EXECUTE_CPU  SYSTEMFRAME_EXECUTE_PPU  SYSTEMFRAME_EXECUTE_CPU
            /*  3 */  SYSTEMFRAME_EXECUTE_CPU  SYSTEMFRAME_EXECUTE_PPU
            /*  4 */  SYSTEMFRAME_EXECUTE_CPU  SYSTEMFRAME_EXECUTE_PPU
            /*  5 */  SYSTEMFRAME_EXECUTE_CPU  SYSTEMFRAME_EXECUTE_PPU  SYSTEMFRAME_EXECUTE_CPU
            /*  6 */  SYSTEMFRAME_EXECUTE_CPU  SYSTEMFRAME_EXECUTE_PPU
            /*  7 */  SYSTEMFRAME_EXECUTE_CPU  SYSTEMFRAME_EXECUTE_PPU
            /*  8 */  SYSTEMFRAME_EXECUTE_CPU  SYSTEMFRAME_EXECUTE_PPU  SYSTEMFRAME_EXECUTE_CPU
            /*  9 */  SYSTEMFRAME_EXECUTE_CPU  SYSTEMFRAME_EXECUTE_PPU
            /* 10 */  SYSTEMFRAME_EXECUTE_CPU  SYSTEMFRAME_EXECUTE_PPU
            /* 11 */  SYSTEMFRAME_EXECUTE_CPU  SYSTEMFRAME_EXECUTE_PPU  SYSTEMFRAME_EXECUTE_CPU
            if ((frameticks & 1) == 0)  // (frameticks % 2 == 0) PPU extra ticks
            {
                SYSTEMFRAME_EXECUTE_PPU
            }
        }
        else  // Have breakpoint, need to check
        {
            /*  0 */  SYSTEMFRAME_EXECUTE_BP_CPU  SYSTEMFRAME_EXECUTE_BP_PPU
            /*  1 */  SYSTEMFRAME_EXECUTE_BP_CPU  SYSTEMFRAME_EXECUTE_BP_PPU
            /*  2 */  SYSTEMFRAME_EXECUTE_BP_CPU  SYSTEMFRAME_EXECUTE_BP_PPU  SYSTEMFRAME_EXECUTE_BP_CPU
            /*  3 */  SYSTEMFRAME_EXECUTE_BP_CPU  SYSTEMFRAME_EXECUTE_BP_PPU
            /*  4 */  SYSTEMFRAME_EXECUTE_BP_CPU  SYSTEMFRAME_EXECUTE_BP_PPU
            /*  5 */  SYSTEMFRAME_EXECUTE_BP_CPU  SYSTEMFRAME_EXECUTE_BP_PPU  SYSTEMFRAME_EXECUTE_BP_CPU
            /*  6 */  SYSTEMFRAME_EXECUTE_BP_CPU  SYSTEMFRAME_EXECUTE_BP_PPU
            /*  7 */  SYSTEMFRAME_EXECUTE_BP_CPU  SYSTEMFRAME_EXECUTE_BP_PPU
            /*  8 */  SYSTEMFRAME_EXECUTE_BP_CPU  SYSTEMFRAME_EXECUTE_BP_PPU  SYSTEMFRAME_EXECUTE_BP_CPU
            /*  9 */  SYSTEMFRAME_EXECUTE_BP_CPU  SYSTEMFRAME_EXECUTE_BP_PPU
            /* 10 */  SYSTEMFRAME_EXECUTE_BP_CPU  SYSTEMFRAME_EXECUTE_BP_PPU
            /* 11 */  SYSTEMFRAME_EXECUTE_BP_CPU  SYSTEMFRAME_EXECUTE_BP_PPU  SYSTEMFRAME_EXECUTE_BP_CPU
            if ((frameticks & 1) == 0)  // (frameticks % 2 == 0) PPU extra ticks
            {
                SYSTEMFRAME_EXECUTE_BP_PPU
            }
        }

        if ((frameticks & 31) == 0)  // (frameticks % 32 == 0)
        {
            m_pFloppyCtl->Periodic();  // Each 32nd tick -- FDD tick

            // Keyboard processing
            CSecondMemoryController* pMemCtl = static_cast<CSecondMemoryController*>(m_pSecondMemCtl.get());
            if ((pMemCtl->m_Port177700 & 0200) == 0)
            {
                uint8_t row_Y = m_scanned_key & 0xF;
                uint8_t col_X = (m_scanned_key & 0x70) >> 4;
                uint8_t bit_X = 1 << col_X;
                pMemCtl->m_Port177702 = m_scanned_key;
                if ((m_scanned_key & 0200) == 0)
                {
                    if ((m_kbd_matrix[row_Y].processed == 0) && ((m_kbd_matrix[row_Y].row_Y & bit_X) != 0))
                    {
                        pMemCtl->m_Port177700 |= 0200;
                        m_kbd_matrix[row_Y].processed = 1;
                        if (pMemCtl->m_Port177700 & 0100)
                            m_pPPU->InterruptVIRQ(3, 0300);
                    }
                }
                else
                {
                    if ((m_kbd_matrix[row_Y].processed != 0) && (m_kbd_matrix[row_Y].row_Y == 0))
                    {
                        pMemCtl->m_Port177700 |= 0200;
                        m_kbd_matrix[row_Y].processed = 0;
                        if (pMemCtl->m_Port177700 & 0100)
                            m_pPPU->InterruptVIRQ(3, 0300);
                        pMemCtl->m_Port177702 = m_scanned_key & 0x8F;
                    }
                }
                m_scanned_key++;
            }
        }

        if (m_pHardDrives[0] != nullptr)
            m_pHardDrives[0]->Periodic();
        if (m_pHardDrives[1] != nullptr)
            m_pHardDrives[1]->Periodic();

        soundBrasErr += soundSamplesPerFrame;
        if (2 * soundBrasErr >= FRAMETICKS)
        {
            soundBrasErr -= FRAMETICKS;
            DoSound();
        }

        if (m_TapeReadCallback != nullptr || m_TapeWriteCallback != nullptr)
        {
            m_tapeSamplePhase += static_cast<uint64_t>(m_nTapeSampleRate);
            const auto samples = static_cast<unsigned int>(m_tapeSamplePhase / tapeTicksPerSecond);
            m_tapeSamplePhase %= tapeTicksPerSecond;
            if (samples != 0)
            {
                if (m_TapeReadCallback != nullptr)  // Tape reading
                {
                    bool tapeBit = (*m_TapeReadCallback)(samples);
                    CSecondMemoryController* pMemCtl = static_cast<CSecondMemoryController*>(m_pSecondMemCtl.get());
                    if (pMemCtl->TapeInput(tapeBit))
                    {
                        m_timerflags |= 040;  // Set bit 5 of timer state: external event ready to read
                    }
                }
                else if (m_TapeWriteCallback != nullptr)  // Tape writing
                {
                    CSecondMemoryController* pMemCtl = static_cast<CSecondMemoryController*>(m_pSecondMemCtl.get());
                    unsigned int value = pMemCtl->TapeOutput() ? 0xffffffff : 0;
                    (*m_TapeWriteCallback)(value, samples);
                }
            }
        }

        if (m_SerialInCallback != nullptr && frameticks % 416 == 0)
        {
            CFirstMemoryController* pMemCtl = static_cast<CFirstMemoryController*>(m_pFirstMemCtl.get());
            if ((pMemCtl->m_Port176574 & 004) == 0)  // Not loopback?
            {
                uint8_t b;
                if (m_SerialInCallback(&b))
                {
                    if (pMemCtl->SerialInput(b) && (pMemCtl->m_Port176570 & 0100))
                        m_pCPU->InterruptVIRQ(7, 0370);
                }
            }
        }
        if (m_SerialOutCallback != nullptr && frameticks % serialOutTicks == 0)
        {
            CFirstMemoryController* pMemCtl = static_cast<CFirstMemoryController*>(m_pFirstMemCtl.get());
            if (serialTxCount > 0)
            {
                serialTxCount--;
                if (serialTxCount == 0)  // Translation countdown finished - the byte translated
                {
                    if ((pMemCtl->m_Port176574 & 004) == 0)  // Not loopback?
                        (*m_SerialOutCallback)((uint8_t)(pMemCtl->m_Port176576 & 0xff));
                    else  // Loopback
                    {
                        if (pMemCtl->SerialInput((uint8_t)(pMemCtl->m_Port176576 & 0xff)) && (pMemCtl->m_Port176570 & 0100))
                            m_pCPU->InterruptVIRQ(7, 0370);
                    }
                    pMemCtl->m_Port176574 |= 0200;  // Set Ready flag
                    if (pMemCtl->m_Port176574 & 0100)  // Interrupt?
                        m_pCPU->InterruptVIRQ(8, 0374);
                }
            }
            else if ((pMemCtl->m_Port176574 & 0200) == 0)  // Ready is 0?
            {
                serialTxCount = 8;  // Start translation countdown
            }
        }

        if (m_NetworkInCallback != nullptr && frameticks % 64 == 0)
        {
            CFirstMemoryController* pMemCtl = static_cast<CFirstMemoryController*>(m_pFirstMemCtl.get());
            if ((pMemCtl->m_Port176564 & 004) == 0)  // Not loopback?
            {
                uint8_t b;
                if (m_NetworkInCallback(&b))
                {
                    if (pMemCtl->NetworkInput(b) && (pMemCtl->m_Port176560 & 0100))
                    {
                        m_pCPU->InterruptVIRQ(9, 0360);
                        //DebugLog(_T("Net INT 0360\r\n"));//DEBUG
                    }
                }
            }
        }
        if (m_NetworkOutCallback != nullptr && frameticks % networkOutTicks == 0)
        {
            CFirstMemoryController* pMemCtl = static_cast<CFirstMemoryController*>(m_pFirstMemCtl.get());
            if (networkTxCount > 0)
            {
                networkTxCount--;
                if (networkTxCount == 0)  // Translation countdown finished - the byte translated
                {
                    if ((pMemCtl->m_Port176564 & 004) == 0)  // Not loopback?
                        (*m_NetworkOutCallback)((uint8_t)(pMemCtl->m_Port176566 & 0xff));
                    else  // Loopback
                    {
                        if (pMemCtl->NetworkInput((uint8_t)(pMemCtl->m_Port176566 & 0xff)) && (pMemCtl->m_Port176560 & 0100))
                            m_pCPU->InterruptVIRQ(9, 0360);
                    }
                    pMemCtl->m_Port176564 |= 0200;  // Set Ready flag
                    if (pMemCtl->m_Port176564 & 0100)  // Interrupt?
                    {
                        m_pCPU->InterruptVIRQ(10, 0364);
                        //DebugLog(_T("Net INT 0364\r\n"));//DEBUG
                    }
                }
            }
            else if ((pMemCtl->m_Port176564 & 0200) == 0)  // Ready is 0?
            {
                networkTxCount = 4;  // Start translation countdown
            }
        }

        if (m_ParallelOutCallback != nullptr && !m_okSoundCovox)
        {
            CSecondMemoryController* pMemCtl = static_cast<CSecondMemoryController*>(m_pSecondMemCtl.get());
            if ((pMemCtl->m_Port177102 & 0x80) == 0x80 && (pMemCtl->m_Port177101 & 0x80) == 0x80)
            {
                // Strobe set, Printer Ack set => reset Printer Ack
                pMemCtl->m_Port177101 &= ~0x80;
                // Now printer waits for a next byte
            }
            else if ((pMemCtl->m_Port177102 & 0x80) == 0 && (pMemCtl->m_Port177101 & 0x80) == 0)
            {
                // Strobe reset, Printer Ack reset => byte is ready, print it
                (*m_ParallelOutCallback)(pMemCtl->m_Port177100);
                pMemCtl->m_Port177101 |= 0x80;  // Set Printer Acknowledge
                // Now the printer waits for Strobe
            }
        }

        frameticks++;
    }
    while (frameticks < FRAMETICKS);

    return true;
}

// Key pressed or released
void CMotherboard::KeyboardEvent(uint8_t scancode, bool okPressed)
{
    uint8_t row_Y = scancode & 0xF;
    uint8_t col_X = (scancode & 0x70) >> 4;
    uint8_t bit_X = 1 << col_X;
    if (okPressed)
        m_kbd_matrix[row_Y].row_Y |= bit_X;
    else
        m_kbd_matrix[row_Y].row_Y &= ~bit_X;
}


//////////////////////////////////////////////////////////////////////
//
// Emulator image format:
//  Offset Size
//     0    32 bytes  - Header
//    32   128 bytes  - Board status
//   160    64 bytes  - CPU status
//   224    64 bytes  - PPU status
//   288    64 bytes  - CPU memory/IO
//   352    64 bytes  - PPU memory/IO
//   416    96 bytes  - reserved
//   512    --        - end of the header
//   512   32 Kbytes  - ROM image
//         64 Kbytes * 3  - RAM planes 0, 1, 2
//TODO: 256 bytes * 2 - Cartridge 1..2 path
//TODO: 256 bytes * 4 - Floppy 1..4 path
//TODO: 256 bytes * 2 - Hard 1..2 path
//TODO: Floppy drive state
//TODO: Hard drive state

void CMotherboard::SaveToImage(std::span<uint8_t, UKNCIMAGE_SIZE> image)
{
    image::Writer writer(image.subspan<32, 128>());
    writer.word(m_timer);
    writer.word(m_timerreload);
    writer.word(m_timerflags);
    writer.word(m_timerdivider);
    const auto writeChannels = [&writer](auto& channels) {
        for (const auto& channel : channels)
        {
            writer.byte(channel.data);
            writer.byte(channel.ready);
            writer.byte(channel.irq);
            writer.byte(channel.rdwr);
        }
    };
    writeChannels(m_chancputx);
    writeChannels(m_chancpurx);
    writeChannels(m_chanpputx);
    writeChannels(m_chanppurx);
    writer.byte(m_chan0disabled);
    writer.byte(m_irq_cpureset);
    writer.byte(0);
    writer.byte(m_scanned_key);
    for (const auto& row : m_kbd_matrix)
    {
        writer.byte(row.processed);
        writer.byte(row.row_Y);
    }
    writer.word(m_multiply);
    for (auto value : freq_per) writer.word(value);
    for (auto value : freq_out) writer.word(value);
    for (auto value : freq_enable) writer.word(value);
    // CPU status
    m_pCPU->SaveToImage(image.subspan<160, 64>());
    // PPU status
    m_pPPU->SaveToImage(image.subspan<224, 64>());
    // CPU memory/IO controller status
    m_pFirstMemCtl->SaveToImage(image.subspan<288, 64>());
    // PPU memory/IO controller status
    m_pSecondMemCtl->SaveToImage(image.subspan<352, 64>());

    constexpr size_t romSize = 32 * 1024;
    constexpr size_t ramSize = 64 * 1024;
    std::ranges::copy(std::span{m_pROM.get(), romSize}, image.subspan<UKNCIMAGE_HEADER_SIZE, romSize>().begin());
    std::span<uint8_t> ramImage = image.subspan<UKNCIMAGE_HEADER_SIZE + romSize>();
    for (const auto& plane : m_pRAM)
    {
        std::ranges::copy(std::span{plane.get(), ramSize}, ramImage.begin());
        ramImage = ramImage.subspan(ramSize);
    }
}
void CMotherboard::LoadFromImage(std::span<const uint8_t, UKNCIMAGE_SIZE> image)
{
    image::Reader reader(image.subspan<32, 128>());
    m_timer = reader.word();
    m_timerreload = reader.word();
    m_timerflags = reader.word();
    m_timerdivider = reader.word();
    const auto readChannels = [&reader](auto& channels) {
        for (auto& channel : channels)
        {
            channel.data = reader.byte();
            channel.ready = reader.byte();
            channel.irq = reader.byte();
            channel.rdwr = reader.byte();
        }
    };
    readChannels(m_chancputx);
    readChannels(m_chancpurx);
    readChannels(m_chanpputx);
    readChannels(m_chanppurx);
    m_chan0disabled = reader.byte();
    m_irq_cpureset = reader.byte();
    reader.skip(1);
    m_scanned_key = reader.byte();
    for (auto& row : m_kbd_matrix)
    {
        row.processed = reader.byte();
        row.row_Y = reader.byte();
    }
    m_multiply = reader.word();
    for (auto& value : freq_per) value = reader.word();
    for (auto& value : freq_out) value = reader.word();
    for (auto& value : freq_enable) value = reader.word();
    // CPU status
    m_pCPU->LoadFromImage(image.subspan<160, 64>());
    // PPU status
    m_pPPU->LoadFromImage(image.subspan<224, 64>());
    // CPU memory/IO controller status
    m_pFirstMemCtl->LoadFromImage(image.subspan<288, 64>());
    // PPU memory/IO controller status
    m_pSecondMemCtl->LoadFromImage(image.subspan<352, 64>());

    constexpr size_t romSize = 32 * 1024;
    constexpr size_t ramSize = 64 * 1024;
    std::ranges::copy(image.subspan<UKNCIMAGE_HEADER_SIZE, romSize>(), m_pROM.get());
    std::span<const uint8_t> ramImage = image.subspan<UKNCIMAGE_HEADER_SIZE + romSize>();
    for (const auto& plane : m_pRAM)
    {
        std::ranges::copy(ramImage.first(ramSize), plane.get());
        ramImage = ramImage.subspan(ramSize);
    }
}

void CMotherboard::ChanWriteByCPU(uint8_t chan, uint8_t data)
{
    uint8_t oldp_ready = m_chanppurx[chan].ready;
    chan &= 3;
    ASSERT(chan < 3);

    m_chanppurx[chan].data = data;
    m_chanppurx[chan].ready = 1;
    m_chancputx[chan].ready = 0;
    m_chancputx[chan].rdwr = 1;
    m_pCPU->InterruptVIRQ((chan == 2) ? 5 : (2 + chan * 2), 0);
    if ((m_chanppurx[chan].irq) && (oldp_ready == 0))
    {
        m_chanppurx[chan].rdwr = 0;
        m_pPPU->InterruptVIRQ(5 + chan * 2, 0320 + (010 * chan));
    }

    if (chan == 0 && m_TerminalOutCallback != nullptr)
        m_TerminalOutCallback(data);
}
void CMotherboard::ChanWriteByPPU(uint8_t chan, uint8_t data)
{
    uint8_t oldc_ready = m_chancpurx[chan].ready;
    chan &= 3;
    ASSERT(chan < 2);

    m_chancpurx[chan].data = data;
    m_chancpurx[chan].ready = 1;
    m_chanpputx[chan].ready = 0;
    m_chanpputx[chan].rdwr = 1;
    m_pPPU->InterruptVIRQ((chan == 0) ? 6 : 8, 0);
    if ((m_chancpurx[chan].irq) && (oldc_ready == 0))
    {
        m_chancpurx[chan].rdwr = 0;
        m_pCPU->InterruptVIRQ(chan ? 3 : 1, chan ? 0460 : 060);
    }
}
uint8_t CMotherboard::ChanReadByCPU(uint8_t chan)
{
    uint8_t res, oldp_ready = m_chanpputx[chan].ready;

    chan &= 3;
    ASSERT(chan < 2);

    res = m_chancpurx[chan].data;
    m_chancpurx[chan].ready = 0;
    m_chancpurx[chan].rdwr = 1;
    m_chanpputx[chan].ready = 1;
    m_pCPU->InterruptVIRQ(chan * 2 + 1, 0);
    if ((m_chanpputx[chan].irq) && (oldp_ready == 0))
    {
        m_chanpputx[chan].rdwr = 0;
        m_pPPU->InterruptVIRQ(chan ? 8 : 6, chan ? 0334 : 0324);
    }
    return res;
}
uint8_t CMotherboard::ChanReadByPPU(uint8_t chan)
{
    uint8_t res, oldc_ready = m_chancputx[chan].ready;

    chan &= 3;
    ASSERT(chan < 3);

    //if((chan==0)&&(m_chan0disabled))
    //    return 0;

    res = m_chanppurx[chan].data;
    m_chanppurx[chan].ready = 0;
    m_chanppurx[chan].rdwr = 1;
    m_chancputx[chan].ready = 1;
    m_pPPU->InterruptVIRQ(chan * 2 + 5, 0);
    if ((m_chancputx[chan].irq) && (oldc_ready == 0))
    {
        m_chancputx[chan].rdwr = 0;
        switch (chan)
        {
        case 0:
            m_pCPU->InterruptVIRQ(2, 064);
            break;
        case 1:
            m_pCPU->InterruptVIRQ(4, 0464);
            break;
        case 2:
            m_pCPU->InterruptVIRQ(5, 0474);
            break;
        }
    }

    return res;
}

uint8_t CMotherboard::ChanRxStateGetCPU(uint8_t chan)
{
    chan &= 3;
    ASSERT(chan < 2);
    return (m_chancpurx[chan].ready << 7) | (m_chancpurx[chan].irq << 6);
}

uint8_t CMotherboard::ChanTxStateGetCPU(uint8_t chan)
{
    chan &= 3;
    ASSERT(chan < 3);
    return (m_chancputx[chan].ready << 7) | (m_chancputx[chan].irq << 6);
}

uint8_t CMotherboard::ChanRxStateGetPPU()
{
    uint8_t res =
        (m_irq_cpureset << 6) | (m_chanppurx[2].ready << 5) | (m_chanppurx[1].ready << 4) | (m_chanppurx[0].ready << 3) |
        (m_chanppurx[2].irq << 2)   | (m_chanppurx[1].irq << 1)   | (m_chanppurx[0].irq);
    return res;
}
uint8_t CMotherboard::ChanTxStateGetPPU()
{
    uint8_t res =
        (m_chanpputx[1].ready << 4) | (m_chanpputx[0].ready << 3) | (m_chan0disabled << 2) |
        (m_chanpputx[1].irq << 1)   | (m_chanpputx[0].irq);
    return res;
}
void CMotherboard::ChanRxStateSetCPU(uint8_t chan, uint8_t state)
{
    uint8_t oldc_irq = m_chancpurx[chan].irq;
    chan &= 3;
    ASSERT(chan < 2);

    if (state & 0100) //irq
        m_chancpurx[chan].irq = 1;
    else
    {
        m_chancpurx[chan].irq = 0;
        if ((chan == 0) || (m_pCPU->GetVIRQ(3))) m_chancpurx[chan].rdwr = 1;
        m_pCPU->InterruptVIRQ(chan ? 3 : 1, 0);
    }
    if ((m_chancpurx[chan].irq) && (m_chancpurx[chan].ready) && (oldc_irq == 0) && (m_chancpurx[chan].rdwr))
    {
        m_chancpurx[chan].rdwr = 0;
        m_pCPU->InterruptVIRQ(chan ? 3 : 1, chan ? 0460 : 060);
    }
}
void CMotherboard::ChanTxStateSetCPU(uint8_t chan, uint8_t state)
{
    uint8_t oldc_irq = m_chancputx[chan].irq;
    chan &= 3;
    ASSERT(chan < 3);

    if (state & 0100) //irq
        m_chancputx[chan].irq = 1;
    else
    {
        m_chancputx[chan].irq = 0;
        if ((chan == 0) || (m_pCPU->GetVIRQ((chan == 2) ? 5 : (chan * 2 + 2)))) m_chancputx[chan].rdwr = 1;
        m_pCPU->InterruptVIRQ((chan == 2) ? 5 : (chan * 2 + 2), 0);
    }

    if ((m_chancputx[chan].irq) && (m_chancputx[chan].ready) && (oldc_irq == 0) && (m_chancputx[chan].rdwr))
    {
        m_chancputx[chan].rdwr = 0;
        switch (chan)
        {
        case 0:
            m_pCPU->InterruptVIRQ(2, 064);
            break;
        case 1:
            m_pCPU->InterruptVIRQ(4, 0464);
            break;
        case 2:
            m_pCPU->InterruptVIRQ(5, 0474);
            break;
        }
    }
}

void CMotherboard::ChanRxStateSetPPU(uint8_t state)
{
    uint8_t oldp_irq0 = m_chanppurx[0].irq;
    uint8_t oldp_irq1 = m_chanppurx[1].irq;
    uint8_t oldp_irq2 = m_chanppurx[2].irq;

    m_chanppurx[0].irq = state & 1;
    m_chanppurx[1].irq = (state >> 1) & 1;
    m_chanppurx[2].irq = (state >> 2) & 1;
    m_irq_cpureset = (state >> 6) & 1;

    if (m_chanppurx[0].irq == 0)
    {
        if (m_pPPU->GetVIRQ(5)) m_chanppurx[0].rdwr = 1;
        m_pPPU->InterruptVIRQ(5, 0);
    }
    if (m_chanppurx[1].irq == 0)
    {
        if (m_pPPU->GetVIRQ(7)) m_chanppurx[1].rdwr = 1;
        m_pPPU->InterruptVIRQ(7, 0);
    }
    if (m_chanppurx[2].irq == 0)
    {
        if (m_pPPU->GetVIRQ(9)) m_chanppurx[2].rdwr = 1;
        m_pPPU->InterruptVIRQ(9, 0);
    }

    if ((m_chanppurx[0].irq) && (m_chanppurx[0].ready) && (oldp_irq0 == 0) && (m_chanppurx[0].rdwr))
    {
        m_chanppurx[0].rdwr = 0;
        m_pPPU->InterruptVIRQ(5, 0320);
    }
    if ((m_chanppurx[1].irq) && (m_chanppurx[1].ready) && (oldp_irq1 == 0) && (m_chanppurx[1].rdwr))
    {
        m_chanppurx[1].rdwr = 0;
        m_pPPU->InterruptVIRQ(7, 0330);
    }
    if ((m_chanppurx[2].irq) && (m_chanppurx[2].ready) && (oldp_irq2 == 0) && (m_chanppurx[2].rdwr))
    {
        m_chanppurx[2].rdwr = 0;
        m_pPPU->InterruptVIRQ(9, 0340);
    }
}
void CMotherboard::ChanTxStateSetPPU(uint8_t state)
{
    uint8_t oldp_irq0 = m_chanpputx[0].irq;
    uint8_t oldp_irq1 = m_chanpputx[1].irq;

    m_chanpputx[0].irq = state & 1;
    m_chanpputx[1].irq = (state >> 1) & 1;
    m_chan0disabled = (state >> 2) & 1;

    if (m_chanpputx[0].irq == 0)
    {
        if (m_pPPU->GetVIRQ(6)) m_chanpputx[0].rdwr = 1;
        m_pPPU->InterruptVIRQ(6, 0);
    }
    if (m_chanpputx[1].irq == 0)
    {
        if (m_pPPU->GetVIRQ(8)) m_chanpputx[1].rdwr = 1;
        m_pPPU->InterruptVIRQ(8, 0);
    }

    if ((m_chanpputx[0].irq) && (m_chanpputx[0].ready) && (oldp_irq0 == 0) && (m_chanpputx[0].rdwr))
    {
        m_chanpputx[0].rdwr = 0;
        m_pPPU->InterruptVIRQ(6, 0324);
    }
    if ((m_chanpputx[1].irq) && (m_chanpputx[1].ready) && (oldp_irq1 == 0) && (m_chanpputx[1].rdwr))
    {
        m_chanpputx[1].rdwr = 0;
        m_pPPU->InterruptVIRQ(8, 0334);
    }
}

void CMotherboard::ChanResetByCPU()
{
    m_chancpurx[0].ready = 0;
    m_chancpurx[0].irq = 0;
    m_chancpurx[0].rdwr = 1;
    m_pCPU->InterruptVIRQ(1, 0);
    m_chanpputx[0].ready = 1;
    if (m_chanpputx[0].irq)
    {
        m_chanpputx[0].rdwr = 0;
        m_pPPU->InterruptVIRQ(6, 0324);
    }

    m_chancputx[0].ready = 1;
    m_chancputx[0].irq = 0;
    m_chancputx[0].rdwr = 1;
    m_pCPU->InterruptVIRQ(2, 0);
    m_chanppurx[0].ready = 0;
    m_chanppurx[0].rdwr = 1;
    m_pPPU->InterruptVIRQ(5, 0);

    m_chancpurx[1].ready = 0;
    m_chancpurx[1].irq = 0;
    m_chancpurx[1].rdwr = 1;
    m_pCPU->InterruptVIRQ(3, 0);
    m_chanpputx[1].ready = 1;
    if (m_chanpputx[1].irq)
    {
        m_chanpputx[1].rdwr = 0;
        m_pPPU->InterruptVIRQ(8, 0334);
    }

    m_chancputx[1].ready = 1;
    m_chancputx[1].irq = 0;
    m_chancputx[1].rdwr = 1;
    m_pCPU->InterruptVIRQ(4, 0);
    m_chanppurx[1].ready = 0;
    m_chanppurx[1].rdwr = 1;
    m_pPPU->InterruptVIRQ(7, 0);

    m_chancputx[2].ready = 1;
    m_chancputx[2].irq = 0;
    m_chancputx[2].rdwr = 1;
    m_pCPU->InterruptVIRQ(5, 0);
    m_chanppurx[2].ready = 0;
    m_chanppurx[2].rdwr = 1;
    m_pPPU->InterruptVIRQ(9, 0);

    if (m_irq_cpureset)
        m_pPPU->InterruptVIRQ(4, 0314);
}

void CMotherboard::ChanResetByPPU()
{
    m_chanppurx[0].ready = 0;
    m_chanppurx[0].irq = 0;
    m_chanppurx[0].rdwr = 1;
    m_pPPU->InterruptVIRQ(5, 0);
    m_chancputx[0].ready = 1;
    if (m_chancputx[0].irq)
    {
        m_chancputx[0].rdwr = 0;
        m_pCPU->InterruptVIRQ(2, 064);
    }

    m_chanpputx[0].ready = 1;
    m_chanpputx[0].irq = 0;
    m_chanpputx[0].rdwr = 1;
    m_pPPU->InterruptVIRQ(6, 0);
    m_chancpurx[0].ready = 0;
    m_chancpurx[0].rdwr = 1;
    m_pCPU->InterruptVIRQ(1, 0);

    m_chanppurx[1].ready = 0;
    m_chanppurx[1].irq = 0;
    m_chanppurx[1].rdwr = 1;
    m_pPPU->InterruptVIRQ(7, 0);
    m_chancputx[1].ready = 1;
    if (m_chancputx[1].irq)
    {
        m_chancputx[1].rdwr = 0;
        m_pCPU->InterruptVIRQ(4, 0464);
    }

    m_chanpputx[1].ready = 1;
    m_chanpputx[1].irq = 0;
    m_chanpputx[1].rdwr = 1;
    m_pPPU->InterruptVIRQ(8, 0);
    m_chancpurx[1].ready = 0;
    m_chancpurx[1].rdwr = 1;
    m_pCPU->InterruptVIRQ(3, 0);

    m_chanppurx[2].ready = 0;
    m_chanppurx[2].irq = 0;
    m_chanppurx[2].rdwr = 1;
    m_pPPU->InterruptVIRQ(9, 0);
    m_chancputx[2].ready = 1;
    if (m_chancputx[2].irq)
    {
        m_chancputx[2].rdwr = 0;
        m_pCPU->InterruptVIRQ(5, 0474);
    }

    m_irq_cpureset = 0;
    m_pPPU->InterruptVIRQ(4, 0);
}

uint16_t CMotherboard::GetFloppyState()
{
    return m_pFloppyCtl->GetState();
}
uint16_t CMotherboard::GetFloppyData()
{
    return m_pFloppyCtl->GetData();
}
void CMotherboard::SetFloppyState(uint16_t val)
{
    m_pFloppyCtl->SetCommand(val);
}
void CMotherboard::SetFloppyData(uint16_t val)
{
    m_pFloppyCtl->WriteData(val);
}


//////////////////////////////////////////////////////////////////////

uint16_t CMotherboard::GetKeyboardRegister()
{
    uint16_t w7214 = GetRAMWord(0, 07214);
    uint8_t b22556 = GetRAMByte(0, 022556);

    uint16_t res = 0;
    switch (w7214)
    {
    case 010534: //fix
    case 07234:  //main
        res = (b22556 & 0200) ? KEYB_RUS : KEYB_LAT;
        break;
    case 07514: //lower register
        res = (b22556 & 0200) ? (KEYB_RUS | KEYB_LOWERREG) : (KEYB_LAT | KEYB_LOWERREG);
        break;
    case 07774: //graph
        res = KEYB_RUS;
        break;
    case 010254: //control
        res = KEYB_LAT;
        break;
    default:
        res = KEYB_LAT;
        break;
    }
    return res;
}

void CMotherboard::DoSound()
{
    freq_out[0] = (m_timer >> 3) & 1; //8000
    if (m_multiply >= 4)
        freq_out[0] = 0;

    freq_out[1] = (m_timer >> 6) & 1; //1000
    freq_out[2] = (m_timer >> 7) & 1; //500
    freq_out[3] = (m_timer >> 8) & 1; //250
    freq_out[4] = (m_timer >> 10) & 1; //60

    int global = !(freq_out[0] & freq_enable[0]) & ! (freq_out[1] & freq_enable[1]) & !(freq_out[2] & freq_enable[2]) & !(freq_out[3] & freq_enable[3]) & !(freq_out[4] & freq_enable[4]);
    if (freq_enable[5] == 0)
        global = 0;
    else
    {
        if ( (!freq_enable[0]) && (!freq_enable[1]) && (!freq_enable[2]) && (!freq_enable[3]) && (!freq_enable[4]))
            global = 1;
    }

    if (m_SoundPrevValue == 0 && global != 0)
        m_SoundChanges++;

    uint16_t value = global ? 0x7fff : 0;
    if (m_okSoundAY)
    {
        uint8_t bufferay[2];
        uint8_t valueay = 0;
        m_pSoundAY[0]->Callback(bufferay);
        valueay |= bufferay[1];
        m_pSoundAY[1]->Callback(bufferay);
        valueay |= bufferay[1];
        m_pSoundAY[2]->Callback(bufferay);
        valueay |= bufferay[1];
        value ^= valueay << 7;
    }

    if (m_okSoundCovox)
    {
        // Get byte from printer port output register, inverted, and merge it with the channel data
        CSecondMemoryController* pSecondMemCtl = static_cast<CSecondMemoryController*>(m_pSecondMemCtl.get());
        uint8_t valuecovox = pSecondMemCtl->m_Port177100 ^ 0xff;
        value ^= valuecovox << 7;
    }

    if (m_SoundGenCallback != nullptr)
        (*m_SoundGenCallback)(value, value);
}

void CMotherboard::SetSound(uint16_t val)
{
    if (val & (1 << 7))
        freq_enable[5] = 1;
    else
        freq_enable[5] = 0;
//12 11 10 9 8

    if (val & (1 << 12))
        freq_enable[0] = 1;
    else
        freq_enable[0] = 0;

    if (val & (1 << 11))
        freq_enable[1] = 1;
    else
        freq_enable[1] = 0;

    if (val & (1 << 10))
        freq_enable[2] = 1;
    else
        freq_enable[2] = 0;

    if (val & (1 << 9))
        freq_enable[3] = 1;
    else
        freq_enable[3] = 0;

    if (val & (1 << 8))
        freq_enable[4] = 1;
    else
        freq_enable[4] = 0;
}

void CMotherboard::SetTapeReadCallback(TAPEREADCALLBACK callback, int sampleRate)
{
    m_tapeSamplePhase = 0;
    if (callback == nullptr || sampleRate <= 0)  // Reset callback
    {
        m_TapeReadCallback = nullptr;
        m_nTapeSampleRate = 0;
    }
    else
    {
        m_TapeReadCallback = callback;
        m_nTapeSampleRate = sampleRate;
        m_TapeWriteCallback = nullptr;
    }
}

void CMotherboard::SetTapeWriteCallback(TAPEWRITECALLBACK callback, int sampleRate)
{
    m_tapeSamplePhase = 0;
    if (callback == nullptr || sampleRate <= 0)  // Reset callback
    {
        m_TapeWriteCallback = nullptr;
        m_nTapeSampleRate = 0;
    }
    else
    {
        m_TapeWriteCallback = callback;
        m_nTapeSampleRate = sampleRate;
        m_TapeReadCallback = nullptr;
    }
}

void CMotherboard::SetSoundGenCallback(SOUNDGENCALLBACK callback)
{
    if (callback == nullptr)  // Reset callback
    {
        m_SoundGenCallback = nullptr;
    }
    else
    {
        m_SoundGenCallback = callback;
    }
}

void CMotherboard::SetSerialCallbacks(SERIALINCALLBACK incallback, SERIALOUTCALLBACK outcallback)
{
    if (incallback == nullptr || outcallback == nullptr)  // Reset callbacks
    {
        m_SerialInCallback = nullptr;
        m_SerialOutCallback = nullptr;
        //TODO: Set port value to indicate we are not ready to translate
    }
    else
    {
        m_SerialInCallback = incallback;
        m_SerialOutCallback = outcallback;
        //TODO: Set port value to indicate we are ready to translate
    }
}

void CMotherboard::SetParallelOutCallback(PARALLELOUTCALLBACK outcallback)
{
    CSecondMemoryController* pMemCtl = static_cast<CSecondMemoryController*>(m_pSecondMemCtl.get());
    if (outcallback == nullptr)  // Reset callback
    {
        pMemCtl->m_Port177101 &= ~2;  // Reset OnLine flag
        m_ParallelOutCallback = nullptr;
    }
    else
    {
        pMemCtl->m_Port177101 |= 2;  // Set OnLine flag
        m_ParallelOutCallback = outcallback;
    }
}

void CMotherboard::SetNetworkCallbacks(NETWORKINCALLBACK incallback, NETWORKOUTCALLBACK outcallback)
{
    if (incallback == nullptr || outcallback == nullptr)  // Reset callbacks
    {
        m_NetworkInCallback = nullptr;
        m_NetworkOutCallback = nullptr;
        //TODO: Set port value to indicate we are not ready to translate
    }
    else
    {
        m_NetworkInCallback = incallback;
        m_NetworkOutCallback = outcallback;
        //TODO: Set port value to indicate we are ready to translate
    }
}


//////////////////////////////////////////////////////////////////////
