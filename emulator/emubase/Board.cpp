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
#include "Emubase.h"
#include "Board.h"


//////////////////////////////////////////////////////////////////////
// Bus devices

static const uint16_t ProcessorTimerAddressRanges[] =
{
    0177710, 6,  // 177710-177715
    0, 0
};
class CBusDeviceProcessorTimer : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("Processor timer"); }
    virtual const uint16_t* GetAddressRanges() const { return ProcessorTimerAddressRanges; }
};

static const uint16_t CpuChannelsAddressRanges[] =
{
    0176660, 16,
    0177560, 8,
    0, 0
};
class CBusDeviceCpuChannels : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("CPU-PPU channels"); }
    virtual const uint16_t* GetAddressRanges() const { return CpuChannelsAddressRanges; }
};

static const uint16_t PpuChannelsAddressRanges[] =
{
    0177060, 16,
    0177100, 4,
    0, 0
};
class CBusDevicePpuChannels : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("CPU-PPU channels"); }
    virtual const uint16_t* GetAddressRanges() const { return PpuChannelsAddressRanges; }
};

static const uint16_t NetworkAdapterAddressRanges[] =
{
    0176560, 8,
    0, 0
};
class CBusDeviceNetworkAdapter : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("Network adapter"); }
    virtual const uint16_t* GetAddressRanges() const { return NetworkAdapterAddressRanges; }
};

static const uint16_t SerialPortAddressRanges[] =
{
    0176570, 8,
    0, 0
};
class CBusDeviceSerialPort : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("Serial port"); }
    virtual const uint16_t* GetAddressRanges() const { return SerialPortAddressRanges; }
};

static const uint16_t CpuMemoryAccessAddressRanges[] =
{
    0176640, 8,
    0176564, 4,  // Регистры пультового терминала
    0, 0
};
class CBusDeviceCpuMemoryAccess : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("Memory access"); }
    virtual const uint16_t* GetAddressRanges() const { return CpuMemoryAccessAddressRanges; }
};

static const uint16_t PpuMemoryAccessAddressRanges[] =
{
    0177010, 16,
    0177054, 2,
    0, 0
};
class CBusDevicePpuMemoryAccess : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("Memory access"); }
    virtual const uint16_t* GetAddressRanges() const { return PpuMemoryAccessAddressRanges; }
};

static const uint16_t ProgrammablePortAddressRanges[] =
{
    0177100, 4,  // i8255 ports
    0, 0
};
class CBusDeviceProgrammablePort : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("Programmable port"); }
    virtual const uint16_t* GetAddressRanges() const { return ProgrammablePortAddressRanges; }
};

static const uint16_t KeyboardAddressRanges[] =
{
    0177700, 6,
    0, 0
};
class CBusDeviceKeyboard : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("Keyboard"); }
    virtual const uint16_t* GetAddressRanges() const { return KeyboardAddressRanges; }
};

static const uint16_t FloppyControllerAddressRanges[] =
{
    0177130, 4,
    0, 0
};
class CBusDeviceFloppyController : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("Floppy controller"); }
    virtual const uint16_t* GetAddressRanges() const { return FloppyControllerAddressRanges; }
};

static const uint16_t HardDriveAddressRanges[] =
{
    0110000, 010000,
    0, 0
};
class CBusDeviceHardDrive : public CBusDevice
{
public:
    virtual LPCTSTR GetName() const { return _T("Hard drive"); }
    virtual const uint16_t* GetAddressRanges() const { return HardDriveAddressRanges; }
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
    m_pCPU = new CProcessor(_T("CPU"));
    m_pPPU = new CProcessor(_T("PPU"));
    m_pFirstMemCtl = new CFirstMemoryController();
    m_pSecondMemCtl = new CSecondMemoryController();
    m_pFloppyCtl = new CFloppyController();
    m_pHardDrives[0] = nullptr;
    m_pHardDrives[1] = nullptr;
    m_pSoundAY[0] = new CSoundAY();
    m_pSoundAY[1] = new CSoundAY();
    m_pSoundAY[2] = new CSoundAY();

    // Connect devices
    m_pCPU->AttachMemoryController(m_pFirstMemCtl);
    m_pFirstMemCtl->Attach(this, m_pCPU);
    m_pPPU->AttachMemoryController(m_pSecondMemCtl);
    m_pSecondMemCtl->Attach(this, m_pPPU);

    // Allocate memory for RAM and ROM
    m_pRAM[0] = static_cast<uint8_t*>(calloc(65536, 1));
    m_pRAM[1] = static_cast<uint8_t*>(calloc(65536, 1));
    m_pRAM[2] = static_cast<uint8_t*>(calloc(65536, 1));
    m_pROM    = static_cast<uint8_t*>(calloc(32768, 1));
    m_pROMCart[0] = nullptr;
    m_pROMCart[1] = nullptr;

    // Prepare bus devices
    m_pCpuDevices = static_cast<CBusDevice**>(calloc(6, sizeof(CBusDevice*)));
    m_pCpuDevices[0] = new CBusDeviceProcessorTimer();
    m_pCpuDevices[1] = new CBusDeviceCpuChannels();
    m_pCpuDevices[2] = new CBusDeviceNetworkAdapter();
    m_pCpuDevices[3] = new CBusDeviceSerialPort();
    m_pCpuDevices[4] = new CBusDeviceCpuMemoryAccess();
    m_pCpuDevices[5] = nullptr;
    m_pPpuDevices = static_cast<CBusDevice**>(calloc(8, sizeof(CBusDevice*)));
    m_pPpuDevices[0] = new CBusDeviceProcessorTimer();
    m_pPpuDevices[1] = new CBusDevicePpuChannels();
    m_pPpuDevices[2] = new CBusDevicePpuMemoryAccess();
    m_pPpuDevices[3] = new CBusDeviceProgrammablePort();
    m_pPpuDevices[4] = new CBusDeviceKeyboard();
    m_pPpuDevices[5] = new CBusDeviceFloppyController();
    m_pPpuDevices[6] = new CBusDeviceHardDrive();
    m_pPpuDevices[7] = nullptr;

    m_pFirstMemCtl->AttachDevices((const CBusDevice**)m_pCpuDevices);
    m_pSecondMemCtl->AttachDevices((const CBusDevice**)m_pPpuDevices);
}

CMotherboard::~CMotherboard ()
{
    // Delete bus devices
    CBusDevice** ppDevice = m_pCpuDevices;
    while (*ppDevice != nullptr)
    {
        delete *ppDevice;
        *ppDevice = nullptr;
        ppDevice++;
    }
    free(m_pCpuDevices);
    ppDevice = m_pPpuDevices;
    while (*ppDevice != nullptr)
    {
        delete *ppDevice;
        *ppDevice = nullptr;
        ppDevice++;
    }
    free(m_pPpuDevices);

    // Delete devices
    delete m_pCPU;
    delete m_pPPU;
    delete m_pFirstMemCtl;
    delete m_pSecondMemCtl;
    delete m_pFloppyCtl;
    delete m_pSoundAY[0];
    delete m_pSoundAY[1];
    delete m_pSoundAY[2];

    // Free memory
    free(m_pRAM[0]);
    free(m_pRAM[1]);
    free(m_pRAM[2]);
    free(m_pROM);
    if (m_pROMCart[0] != nullptr) free(m_pROMCart[0]);
    if (m_pROMCart[1] != nullptr) free(m_pROMCart[1]);
    if (m_pHardDrives[0] != nullptr) delete m_pHardDrives[0];
    if (m_pHardDrives[1] != nullptr) delete m_pHardDrives[1];
}

void CMotherboard::SetTrace(uint32_t dwTrace)
{
    m_dwTrace = dwTrace;
    if (m_pPPU != nullptr)
        m_pPPU->SetTrace((dwTrace & TRACE_PPU) != 0);
    if (m_pCPU != nullptr)
        m_pCPU->SetTrace((dwTrace & TRACE_CPU) != 0);
    if (m_pFloppyCtl != nullptr)
        m_pFloppyCtl->SetTrace((dwTrace & TRACE_FLOPPY) != 0);
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
    memcpy(m_pROM, pBuffer, 32768);
}

void CMotherboard::LoadROMCartridge(int cartno, const uint8_t* pBuffer)  // Load 24 KB ROM cartridge image
{
    ASSERT(cartno == 1 || cartno == 2);  // Only two cartridges, #1 and #2
    ASSERT(pBuffer != nullptr);

    int cartindex = cartno - 1;
    if (m_pROMCart[cartindex] == nullptr)
        m_pROMCart[cartindex] = static_cast<uint8_t*>(calloc(24 * 1024, 1));

    memcpy(m_pROMCart[cartindex], pBuffer, 24 * 1024);
}

void CMotherboard::LoadRAM(int plan, const uint8_t* pBuffer)  // Load 32 KB RAM image from the buffer
{
    ASSERT(plan >= 0 && plan <= 2);
    memcpy(m_pRAM[plan], pBuffer, 32768);
}

void CMotherboard::SetNetStation(uint16_t station)
{
    CFirstMemoryController* pMemCtl = static_cast<CFirstMemoryController*>(m_pFirstMemCtl);
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
    if (m_pROMCart[cartindex] != nullptr)
    {
        free(m_pROMCart[cartindex]);
        m_pROMCart[cartindex] = nullptr;
    }
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
    CHardDrive* pHardDrive = m_pHardDrives[slot - 1];
    if (pHardDrive == nullptr) return false;
    return pHardDrive->IsReadOnly();
}

bool CMotherboard::AttachHardImage(int slot, LPCTSTR sFileName)
{
    ASSERT(slot >= 1 && slot <= 2);

    m_pHardDrives[slot - 1] = new CHardDrive();
    bool success = m_pHardDrives[slot - 1]->AttachImage(sFileName);
    if (success)
    {
        m_pHardDrives[slot - 1]->Reset();
        m_pSecondMemCtl->UpdateMemoryMap();
    }

    return success;
}
void CMotherboard::DetachHardImage(int slot)
{
    ASSERT(slot >= 1 && slot <= 2);

    delete m_pHardDrives[slot - 1];
    m_pHardDrives[slot - 1] = nullptr;
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
    return *((uint16_t*)(m_pROM + (offset & 0xFFFE)));
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
    uint16_t* p = (uint16_t*) (m_pROMCart[cartindex] + (offset & 0xFFFE));
    return *p;
}
uint8_t CMotherboard::GetROMCartByte(int cartno, uint16_t offset) const
{
    ASSERT(cartno == 1 || cartno == 2);
    ASSERT(offset < 24 * 1024);
    int cartindex = cartno - 1;
    if (m_pROMCart[cartindex] == nullptr)
        return 0377;
    uint8_t* p = m_pROMCart[cartindex] + offset;
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
    ((CSecondMemoryController*)m_pSecondMemCtl)->SetMouse(onoff);
}

void CMotherboard::MouseMove(int16_t dx, int16_t dy, bool btnLeft, bool btnRight)
{
    ((CSecondMemoryController*)m_pSecondMemCtl)->MouseMove(dx, dy, btnLeft, btnRight);
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
Каждый фрейм равен 1/25 секунды = 40 мс = 20000 тиков, 1 тик = 2 мкс.

* 20000 тиков системного таймера - на каждый 1-й тик
* 2 сигнала EVNT, в 0-й и 10000-й тик фрейма
* 320000 тиков ЦП - 16 раз за один тик
* 250000 тиков ПП - 12.5 раз за один тик
* Отрисовка 288 видимых строк, по 32 тика на строку (только в первой половине фрейма)
** Первая невидимая строка (#0) начинает рисоваться на 96-ой тик
** Первая видимая строка (#18) начинает рисоваться на 672-й тик
* 625 тиков FDD - каждый 32-й тик
* 48 тиков обмена с COM-портом - каждый 416 тик
* 8?? тиков обмена с NET-портом - каждый 64 тик ???
*/
#define SYSTEMFRAME_EXECUTE_CPU     { m_pCPU->Execute(); }
#define SYSTEMFRAME_EXECUTE_PPU     { m_pPPU->Execute(); }
#define SYSTEMFRAME_EXECUTE_BP_CPU  { m_pCPU->Execute(); if (m_CPUbps != nullptr) \
    { const uint16_t* pbps = m_CPUbps; while(*pbps != 0177777) { if (m_pCPU->GetPC() == *pbps++) return false; } } }
#define SYSTEMFRAME_EXECUTE_BP_PPU  { m_pPPU->Execute(); if (m_PPUbps != nullptr) \
    { const uint16_t* pbps = m_PPUbps; while(*pbps != 0177777) { if (m_pPPU->GetPC() == *pbps++) return false; } } }
bool CMotherboard::SystemFrame()
{
    int frameticks = 0;  // count 20000 ticks

    m_SoundChanges = 0;
    int soundSamplesPerFrame = SAMPLERATE / 25, soundBrasErr = 0;

    const int serialOutTicks = 20000 / (9600 / 25);
    int serialTxCount = 0;
    const int networkOutTicks = 7; //20000 / (57600 / 25);
    int networkTxCount = 0;

    int tapeSamplesPerFrame = 1, tapeBrasErr = 0;
    if (m_TapeReadCallback != nullptr || m_TapeWriteCallback != nullptr)
        tapeSamplesPerFrame = m_nTapeSampleRate / 25;

    do
    {
        TimerTick();  // System timer tick

        if (frameticks % 10000 == 0)
            Tick50();  // 1/50 timer event

        // CPU - 16 times, PPU - 12.5 times
        if (m_CPUbps == nullptr && m_PPUbps == nullptr)  // No breakpoints, no need to check
        {
            /*  0 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;
            /*  1 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;
            /*  2 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;  SYSTEMFRAME_EXECUTE_CPU;
            /*  3 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;
            /*  4 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;
            /*  5 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;  SYSTEMFRAME_EXECUTE_CPU;
            /*  6 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;
            /*  7 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;
            /*  8 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;  SYSTEMFRAME_EXECUTE_CPU;
            /*  9 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;
            /* 10 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;
            /* 11 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;  SYSTEMFRAME_EXECUTE_CPU;
            if ((frameticks & 1) == 0)  // (frameticks % 2 == 0) PPU extra ticks
                SYSTEMFRAME_EXECUTE_PPU;
        }
        else  // Have breakpoint, need to check
        {
            /*  0 */  SYSTEMFRAME_EXECUTE_BP_CPU;  SYSTEMFRAME_EXECUTE_BP_PPU;
            /*  1 */  SYSTEMFRAME_EXECUTE_BP_CPU;  SYSTEMFRAME_EXECUTE_BP_PPU;
            /*  2 */  SYSTEMFRAME_EXECUTE_BP_CPU;  SYSTEMFRAME_EXECUTE_BP_PPU;  SYSTEMFRAME_EXECUTE_BP_CPU;
            /*  3 */  SYSTEMFRAME_EXECUTE_BP_CPU;  SYSTEMFRAME_EXECUTE_BP_PPU;
            /*  4 */  SYSTEMFRAME_EXECUTE_BP_CPU;  SYSTEMFRAME_EXECUTE_BP_PPU;
            /*  5 */  SYSTEMFRAME_EXECUTE_BP_CPU;  SYSTEMFRAME_EXECUTE_BP_PPU;  SYSTEMFRAME_EXECUTE_BP_CPU;
            /*  6 */  SYSTEMFRAME_EXECUTE_BP_CPU;  SYSTEMFRAME_EXECUTE_BP_PPU;
            /*  7 */  SYSTEMFRAME_EXECUTE_BP_CPU;  SYSTEMFRAME_EXECUTE_BP_PPU;
            /*  8 */  SYSTEMFRAME_EXECUTE_BP_CPU;  SYSTEMFRAME_EXECUTE_BP_PPU;  SYSTEMFRAME_EXECUTE_BP_CPU;
            /*  9 */  SYSTEMFRAME_EXECUTE_BP_CPU;  SYSTEMFRAME_EXECUTE_BP_PPU;
            /* 10 */  SYSTEMFRAME_EXECUTE_BP_CPU;  SYSTEMFRAME_EXECUTE_BP_PPU;
            /* 11 */  SYSTEMFRAME_EXECUTE_BP_CPU;  SYSTEMFRAME_EXECUTE_BP_PPU;  SYSTEMFRAME_EXECUTE_BP_CPU;
            if ((frameticks & 1) == 0)  // (frameticks % 2 == 0) PPU extra ticks
                SYSTEMFRAME_EXECUTE_BP_PPU;
        }

        if ((frameticks & 31) == 0)  // (frameticks % 32 == 0)
        {
            m_pFloppyCtl->Periodic();  // Each 32nd tick -- FDD tick

            // Keyboard processing
            CSecondMemoryController* pMemCtl = static_cast<CSecondMemoryController*>(m_pSecondMemCtl);
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
        if (2 * soundBrasErr >= 20000)
        {
            soundBrasErr -= 20000;
            DoSound();
        }

        if (m_TapeReadCallback != nullptr || m_TapeWriteCallback != nullptr)
        {
            tapeBrasErr += tapeSamplesPerFrame;
            if (2 * tapeBrasErr >= 20000)
            {
                tapeBrasErr -= 20000;

                if (m_TapeReadCallback != nullptr)  // Tape reading
                {
                    bool tapeBit = (*m_TapeReadCallback)(1);
                    CSecondMemoryController* pMemCtl = dynamic_cast<CSecondMemoryController*>(m_pSecondMemCtl);
                    if (pMemCtl->TapeInput(tapeBit))
                    {
                        m_timerflags |= 040;  // Set bit 5 of timer state: external event ready to read
                    }
                }
                else if (m_TapeWriteCallback != nullptr)  // Tape writing
                {
                    CSecondMemoryController* pMemCtl = dynamic_cast<CSecondMemoryController*>(m_pSecondMemCtl);
                    unsigned int value = pMemCtl->TapeOutput() ? 0xffffffff : 0;
                    (*m_TapeWriteCallback)(value, 1);
                }
            }
        }

        if (m_SerialInCallback != nullptr && frameticks % 416 == 0)
        {
            CFirstMemoryController* pMemCtl = dynamic_cast<CFirstMemoryController*>(m_pFirstMemCtl);
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
            CFirstMemoryController* pMemCtl = dynamic_cast<CFirstMemoryController*>(m_pFirstMemCtl);
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
            CFirstMemoryController* pMemCtl = dynamic_cast<CFirstMemoryController*>(m_pFirstMemCtl);
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
            CFirstMemoryController* pMemCtl = dynamic_cast<CFirstMemoryController*>(m_pFirstMemCtl);
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
            CSecondMemoryController* pMemCtl = dynamic_cast<CSecondMemoryController*>(m_pSecondMemCtl);
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
    while (frameticks < 20000);

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

void CMotherboard::SaveToImage(uint8_t* pImage)
{
    // Board data                                       // Offset Size
    uint16_t* pwImage = (uint16_t*) (pImage + 32);      //   32    --
    *pwImage++ = m_timer;                               //   32     2
    *pwImage++ = m_timerreload;                         //   34     2
    *pwImage++ = m_timerflags;                          //   36     2
    *pwImage++ = m_timerdivider;                        //   38     2
    uint32_t* pdwImage = (uint32_t*)pwImage;            //   40    --
    memcpy(pdwImage, m_chancputx, 3 * 4); pdwImage += 3;//   40    12
    memcpy(pdwImage, m_chancpurx, 2 * 4); pdwImage += 2;//   52     8
    memcpy(pdwImage, m_chanpputx, 2 * 4); pdwImage += 2;//   60     8
    memcpy(pdwImage, m_chanppurx, 3 * 4); pdwImage += 3;//   68    12
    uint8_t* pbImage = (uint8_t*) pdwImage;             //   80    --
    *pbImage++ = m_chan0disabled;                       //   80     1
    *pbImage++ = m_irq_cpureset;                        //   81     1
    *pbImage++ = 0;                                     //   82     1  // not used
    *pbImage++ = m_scanned_key;                         //   83     1
    memcpy(pbImage, m_kbd_matrix, 2 * 16); pbImage += 32;//  84    32
    pwImage = (uint16_t*) pbImage;                      //  116    --
    *pwImage++ = m_multiply;                            //  116     2
    memcpy(pwImage, freq_per, 12); pwImage += 6;        //  118    12
    memcpy(pwImage, freq_out, 12); pwImage += 6;        //  130    12
    memcpy(pwImage, freq_enable, 12); /*pwImage += 6;*/ //  142    12

    // CPU status
    uint8_t* pImageCPU = pImage + 160;
    m_pCPU->SaveToImage(pImageCPU);
    // PPU status
    uint8_t* pImagePPU = pImageCPU + 64;
    m_pPPU->SaveToImage(pImagePPU);
    // CPU memory/IO controller status
    uint8_t* pImageCpuMem = pImagePPU + 64;
    m_pFirstMemCtl->SaveToImage(pImageCpuMem);
    // PPU memory/IO controller status
    uint8_t* pImagePpuMem = pImageCpuMem + 64;
    m_pSecondMemCtl->SaveToImage(pImagePpuMem);

    // ROM
    uint8_t* pImageRom = pImage + UKNCIMAGE_HEADER_SIZE;
    memcpy(pImageRom, m_pROM, 32 * 1024);
    // RAM planes 0, 1, 2
    uint8_t* pImageRam = pImageRom + 32 * 1024;
    memcpy(pImageRam, m_pRAM[0], 64 * 1024);
    pImageRam += 64 * 1024;
    memcpy(pImageRam, m_pRAM[1], 64 * 1024);
    pImageRam += 64 * 1024;
    memcpy(pImageRam, m_pRAM[2], 64 * 1024);
    pImageRam += 64 * 1024;
    ASSERT((pImageRam - pImage) == UKNCIMAGE_SIZE);
}
void CMotherboard::LoadFromImage(const uint8_t* pImage)
{
    // Board data                                       // Offset Size
    const uint16_t* pwImage = (const uint16_t*) (pImage + 32);//   32    --
    m_timer = *pwImage++;                               //   32     2
    m_timerreload = *pwImage++;                         //   34     2
    m_timerflags = *pwImage++;                          //   36     2
    m_timerdivider = *pwImage++;                        //   38     2
    uint32_t* pdwImage = (uint32_t*)pwImage;            //   40    --
    memcpy(m_chancputx, pdwImage, 3 * 4); pdwImage += 3;//   40    12
    memcpy(m_chancpurx, pdwImage, 2 * 4); pdwImage += 2;//   52     8
    memcpy(m_chanpputx, pdwImage, 2 * 4); pdwImage += 2;//   60     8
    memcpy(m_chanppurx, pdwImage, 3 * 4); pdwImage += 3;//   68    12
    const uint8_t* pbImage = (const uint8_t*) pdwImage; //   80    --
    m_chan0disabled = *pbImage++;                       //   80     1
    m_irq_cpureset = *pbImage++;                        //   81     1
    pbImage++;                                          //   82     1  // not used
    m_scanned_key = *pbImage++;                         //   83     1
    memcpy(m_kbd_matrix, pbImage, 2 * 16); pbImage += 32;//  84    32
    pwImage = (const uint16_t*) pbImage;                //  116    --
    m_multiply = *pwImage++;                            //  116     2
    memcpy(freq_per, pwImage, 12); pwImage += 6;        //  118    12
    memcpy(freq_out, pwImage, 12); pwImage += 6;        //  130    12
    memcpy(freq_enable, pwImage, 12); pwImage += 6;     //  142    12

    // CPU status
    const uint8_t* pImageCPU = pImage + 160;            //  160    32
    m_pCPU->LoadFromImage(pImageCPU);
    // PPU status
    const uint8_t* pImagePPU = pImageCPU + 64;          //  224    32
    m_pPPU->LoadFromImage(pImagePPU);
    // CPU memory/IO controller status
    const uint8_t* pImageCpuMem = pImagePPU + 64;       //  288    64
    m_pFirstMemCtl->LoadFromImage(pImageCpuMem);
    // PPU memory/IO controller status
    const uint8_t* pImagePpuMem = pImageCpuMem + 64;    //  352    64
    m_pSecondMemCtl->LoadFromImage(pImagePpuMem);

    // ROM
    const uint8_t* pImageRom = pImage + UKNCIMAGE_HEADER_SIZE; // 512
    memcpy(m_pROM, pImageRom, 32 * 1024);
    // RAM planes 0, 1, 2
    const uint8_t* pImageRam = pImageRom + 32 * 1024;
    memcpy(m_pRAM[0], pImageRam, 64 * 1024);
    pImageRam += 64 * 1024;
    memcpy(m_pRAM[1], pImageRam, 64 * 1024);
    pImageRam += 64 * 1024;
    memcpy(m_pRAM[2], pImageRam, 64 * 1024);
    pImageRam += 64 * 1024;
    ASSERT((pImageRam - pImage) == UKNCIMAGE_SIZE);
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
    uint8_t res;

    res = (m_irq_cpureset << 6) | (m_chanppurx[2].ready << 5) | (m_chanppurx[1].ready << 4) | (m_chanppurx[0].ready << 3) |
          (m_chanppurx[2].irq << 2)   | (m_chanppurx[1].irq << 1)   | (m_chanppurx[0].irq);


    return res;
}
uint8_t CMotherboard::ChanTxStateGetPPU()
{
    uint8_t res;
    res = (m_chanpputx[1].ready << 4) | (m_chanpputx[0].ready << 3) | (m_chan0disabled << 2) |
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

uint16_t CMotherboard::GetKeyboardRegister(void)
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

void CMotherboard::DoSound(void)
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
        m_pSoundAY[0]->Callback(bufferay, sizeof(bufferay));
        valueay |= bufferay[1];
        m_pSoundAY[1]->Callback(bufferay, sizeof(bufferay));
        valueay |= bufferay[1];
        m_pSoundAY[2]->Callback(bufferay, sizeof(bufferay));
        valueay |= bufferay[1];
        value ^= valueay << 7;
    }

    if (m_okSoundCovox)
    {
        // Get byte from printer port output register, inverted, and merge it with the channel data
        CSecondMemoryController* pSecondMemCtl = dynamic_cast<CSecondMemoryController*>(m_pSecondMemCtl);
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
    if (callback == nullptr)  // Reset callback
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
    if (callback == nullptr)  // Reset callback
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
    CSecondMemoryController* pMemCtl = static_cast<CSecondMemoryController*>(m_pSecondMemCtl);
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
