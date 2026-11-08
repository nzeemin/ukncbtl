/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

/// \file Board.h  Motherboard class

#pragma once

#include "Defines.h"
#include <memory>
#include <array>
#include <span>

class CProcessor;
class CMemoryController;
class CSoundAY;

inline constexpr auto FRAMERATE = 50;  // Number of frames per second

// Floppy debug constants
inline constexpr auto FLOPPY_FSM_WAITFORLSB = 0;
inline constexpr auto FLOPPY_FSM_WAITFORMSB = 1;
inline constexpr auto FLOPPY_FSM_WAITFORTERM1 = 2;
inline constexpr auto FLOPPY_FSM_WAITFORTERM2 = 3;

// Trace flags
inline constexpr auto TRACE_NONE = 0;  // Turn off all tracing
inline constexpr auto TRACE_FLOPPY = 0100;  // Trace floppies
inline constexpr auto TRACE_PPU = 0200;  // Trace PPU instructions
inline constexpr auto TRACE_CPU = 0400;  // Trace CPU instructions
inline constexpr auto TRACE_PPUINT = 01000;  // Trace PPU interrupts
inline constexpr auto TRACE_CPUINT = 02000;  // Trace CPU interrupts
inline constexpr auto TRACE_ALL = 0177777;  // Trace all

// Emulator image constants
inline constexpr auto UKNCIMAGE_HEADER_SIZE = 512;
inline constexpr auto UKNCIMAGE_SIZE = UKNCIMAGE_HEADER_SIZE + (32 + 64 * 3) * 1024;
inline constexpr auto UKNCIMAGE_HEADER1 = 0x434E4B55;  // "UKNC"
inline constexpr auto UKNCIMAGE_HEADER2 = 0x214C5442;  // "BTL!"
inline constexpr auto UKNCIMAGE_VERSION = 0x00010001;  // 1.1

inline constexpr auto KEYB_RUS = 0x01;
inline constexpr auto KEYB_LAT = 0x02;
inline constexpr auto KEYB_LOWERREG = 0x10;

struct chan_stc
{
    uint8_t data;
    uint8_t ready;
    uint8_t irq;
    uint8_t rdwr;
};

struct kbd_row
{
    uint8_t processed;
    uint8_t row_Y;
};

// Tape emulator callback used to read a tape recorded data.
// Input:
//   samples    Number of samples to play.
// Output:
//   result     Bit to put in tape input port.
using TAPEREADCALLBACK = bool (CALLBACK*)(unsigned int samples);

// Tape emulator callback used to write a data to tape.
// Input:
//   value      Sample value to write.
using TAPEWRITECALLBACK = void (CALLBACK*)(int value, unsigned int samples);

// Sound generator callback function type
using SOUNDGENCALLBACK = void (CALLBACK*)(unsigned short L, unsigned short R);

// Network port callback for receiving
// Output:
//   pbyte      Byte received
//   result     true means we have a new byte, false means not ready yet
using NETWORKINCALLBACK = bool (CALLBACK*)(uint8_t* pbyte);

// Network port callback for translating
// Input:
//   byte       A byte to translate
// Output:
//   result     true means we translated the byte successfully, false means we have an error
using NETWORKOUTCALLBACK = bool (CALLBACK*)(uint8_t byte);

// Serial port callback for receiving
// Output:
//   pbyte      Byte received
//   result     true means we have a new byte, false means not ready yet
using SERIALINCALLBACK = bool (CALLBACK*)(uint8_t* pbyte);

// Serial port callback for translating
// Input:
//   byte       A byte to translate
// Output:
//   result     true means we translated the byte successfully, false means we have an error
using SERIALOUTCALLBACK = bool (CALLBACK*)(uint8_t byte);

// Parallel port output callback
// Input:
//   byte       An output byte
// Output:
//   result     true means OK, false means we have an error
using PARALLELOUTCALLBACK = bool (CALLBACK*)(uint8_t byte);

// Terminal out callback -- CPU sends character by channel 0
using TERMINALOUTCALLBACK = void (CALLBACK*)(uint8_t byte);

// Instruction execution callback -- called by CPU/PPU just before an instruction is executed,
// memory and registers are still in pre-execution state
// Input:
//   pProc      Processor executing the instruction (CPU or PPU), see CProcessor::GetName()
//   address    Address of the instruction, PC value before the fetch
//   opcode     First word of the instruction as fetched
using INSTRUCTIONCALLBACK = void (CALLBACK*)(const CProcessor* pProc, uint16_t address, uint16_t opcode);

// Interrupt callback -- called by CPU/PPU when an interrupt or trap has been taken: the old PC/PSW
// are saved and the new PC/PSW are loaded from the vector
// Input:
//   pProc      Processor that took the interrupt (CPU or PPU), see CProcessor::GetName();
//              pProc->GetPC() / GetPSW() are the handler address and PSW, IsHaltMode() is the handler mode
//   oldPc      PC where the interrupt appeared: the address of the code that was interrupted
//   vector     Interrupt vector address; for HALT mode interrupts it includes the SEL register high byte
using INTERRUPTCALLBACK = void (CALLBACK*)(const CProcessor* pProc, uint16_t oldPc, uint16_t vector);

class CFloppyController;
class CHardDrive;

//////////////////////////////////////////////////////////////////////


struct BusAddressRange
{
    uint16_t start;
    uint16_t length;
};

/// \brief Bus device
class CBusDevice
{
public:
    /// \brief Name of the device
    virtual LPCTSTR GetName() const = 0;
    /// \brief Device address ranges
    virtual std::span<const BusAddressRange> GetAddressRanges() const = 0;

    virtual ~CBusDevice() = default;
};

/// \brief UKNC computer
class CMotherboard
{
public:  // Construct / destruct
    CMotherboard();
    ~CMotherboard();

protected:  // Devices
    std::unique_ptr<CProcessor> m_pCPU;  ///< CPU device
    std::unique_ptr<CProcessor> m_pPPU;  ///< PPU device
    std::unique_ptr<CMemoryController> m_pFirstMemCtl;  ///< CPU memory control
    std::unique_ptr<CMemoryController> m_pSecondMemCtl;  ///< PPU memory control
    std::unique_ptr<CFloppyController> m_pFloppyCtl;  ///< FDD control
    std::unique_ptr<CHardDrive> m_pHardDrives[2];  ///< HDD control
    std::unique_ptr<CSoundAY> m_pSoundAY[3];

public:  // Getting devices
    CProcessor*     GetCPU() { return m_pCPU.get(); }  ///< Getter for m_pCPU
    CProcessor*     GetPPU() { return m_pPPU.get(); }  ///< Getter for m_pPPU
    CMemoryController*  GetCPUMemoryController() { return m_pFirstMemCtl.get(); }  ///< Get CPU memory controller
    CMemoryController*  GetPPUMemoryController() { return m_pSecondMemCtl.get(); }  ///< Get PPU memory controller

protected:
    std::array<std::unique_ptr<CBusDevice>, 5> m_cpuDeviceOwners;
    std::array<std::unique_ptr<CBusDevice>, 7> m_ppuDeviceOwners;
    std::array<const CBusDevice*, 5> m_pCpuDevices{};
    std::array<const CBusDevice*, 7> m_pPpuDevices{};
public:
    std::span<const CBusDevice* const> GetCPUBusDevices() const { return m_pCpuDevices; }
    std::span<const CBusDevice* const> GetPPUBusDevices() const { return m_pPpuDevices; }

protected:  // Memory
    std::unique_ptr<uint8_t[]> m_pRAM[3];  ///< RAM, three planes, 64 KB each
    std::unique_ptr<uint8_t[]> m_pROM;     ///< System ROM, 32 KB
    std::unique_ptr<uint8_t[]> m_pROMCart[2];  ///< ROM cartridges #1 and #2, 24 KB each
public:  // Memory access
    uint16_t    GetRAMWord(int plan, uint16_t offset) const;
    uint8_t     GetRAMByte(int plan, uint16_t offset) const;
    void        SetRAMWord(int plan, uint16_t offset, uint16_t word);
    void        SetRAMByte(int plan, uint16_t offset, uint8_t byte);
    uint16_t    GetROMWord(uint16_t offset) const;
    uint8_t     GetROMByte(uint16_t offset) const;
    uint16_t    GetROMCartWord(int cartno, uint16_t offset) const;
    uint8_t     GetROMCartByte(int cartno, uint16_t offset) const;
public:  // Debug
    void        DebugTicks();  ///< One Debug PPU tick -- use for debug step or debug breakpoint
    void        SetCPUBreakpoints(const uint16_t* bps) { m_CPUbps = bps; } ///< Set CPU breakpoint list
    void        SetPPUBreakpoints(const uint16_t* bps) { m_PPUbps = bps; } ///< Set PPU breakpoint list
    uint32_t    GetTrace() const { return m_dwTrace; }
    void        SetTrace(uint32_t dwTrace);
    chan_stc    GetChannelStruct(unsigned char cpu, unsigned char chan, unsigned char tx)
    {
        //cpu==1 ,ppu==0; tx==1, rx==0
        if (cpu)
        {
            if (tx)
                return m_chancputx[chan];
            else
                return m_chancpurx[chan];
        }
        else
        {
            if (tx)
                return m_chanpputx[chan];
            else
                return m_chanppurx[chan];
        }
    }

public:  // System control
    void        Reset();  ///< Reset computer
    void        LoadROM(const uint8_t* pBuffer);  ///< Load 32 KB ROM image from the biffer
    void        LoadROMCartridge(int cartno, const uint8_t* pBuffer);  ///< Load 24 KB ROM cartridge image
    void        LoadRAM(int plan, const uint8_t* pBuffer);  ///< Load 32 KB RAM image from the biffer
    void        SetNetStation(uint16_t station);  // Network station number
    void        Tick8000();  ///< Tick 8.00 MHz
    void        Tick6250();  ///< Tick 6.25 MHz
    void        Tick50();    ///< Tick 50 Hz - goes to CPU/PPU EVNT line
    void        TimerTick(); ///< Timer Tick, 2uS -- dividers are within timer routine
    void        ResetFloppy();     ///< INIT signal for FDD
    uint16_t    GetTimerValue();    ///< Returns current timer value
    uint16_t    GetTimerValueView() { return m_timer; } ///< Returns current timer value for debugger
    uint16_t    GetTimerReload();   ///< Returns timer reload value
    uint16_t    GetTimerReloadView() { return m_timerreload; }  ///< Returns timer reload value for debugger
    uint16_t    GetTimerState();    ///< Returns timer state
    uint16_t    GetTimerStateView() { return m_timerflags; } ///< Returns timer state for debugger

    void        ChanWriteByCPU(uint8_t chan, uint8_t data);
    void        ChanWriteByPPU(uint8_t chan, uint8_t data);
    uint8_t     ChanReadByCPU(uint8_t chan);
    uint8_t     ChanReadByPPU(uint8_t chan);

    uint8_t     ChanRxStateGetCPU(uint8_t chan);
    uint8_t     ChanTxStateGetCPU(uint8_t chan);
    uint8_t     ChanRxStateGetPPU();
    uint8_t     ChanTxStateGetPPU();
    void        ChanRxStateSetCPU(uint8_t chan, uint8_t state);
    void        ChanTxStateSetCPU(uint8_t chan, uint8_t state);
    void        ChanRxStateSetPPU(uint8_t state);
    void        ChanTxStateSetPPU(uint8_t state);

    void        ChanResetByCPU();
    void        ChanResetByPPU();

    void        SetTimerReload(uint16_t val);   ///< Sets timer reload value
    void        SetTimerState(uint16_t val);    ///< Sets timer state
    void        ExecuteCPU();  ///< Execute one CPU instruction
    void        ExecutePPU();  ///< Execute one PPU instruction
    bool        SystemFrame();  ///< Do one frame -- use for normal run
    void        KeyboardEvent(uint8_t scancode, bool okPressed);  ///< Key pressed or released
    uint16_t    GetKeyboardRegister();
    uint16_t    GetScannedKey() const { return m_scanned_key; }
    int         GetSoundChanges() const { return m_SoundChanges; }  ///< Sound signal 0 to 1 changes since the beginning of the frame
    void        MouseMove(int16_t dx, int16_t dy, bool btnLeft, bool btnRight);

    /// \brief Attach floppy image to the slot -- insert the disk.
    bool        AttachFloppyImage(int slot, LPCTSTR sFileName);
    /// \brief Empty the floppy slot -- remove the disk.
    void        DetachFloppyImage(int slot);
    /// \brief Check if the floppy attached.
    bool        IsFloppyImageAttached(int slot) const;
    /// \brief Check if the attached floppy image is read-only.
    bool        IsFloppyReadOnly(int slot) const;
    /// \brief Check if the floppy drive engine rotates the disks.
    bool        IsFloppyEngineOn() const;
    uint16_t    GetFloppyState();
    uint16_t    GetFloppyData();
    void        SetSoundAYReg(int chip, uint8_t reg);
    void        SetSoundAYVal(int chip, uint8_t val);
    void        SetFloppyState(uint16_t val);
    void        SetFloppyData(uint16_t val);

    /// \brief Check if ROM cartridge image assigned to the cartridge slot.
    bool        IsROMCartridgeLoaded(int cartno) const;
    /// \brief Empty the ROM cartridge slot.
    void        UnloadROMCartridge(int cartno);

    /// \brief Attach hard drive image to the slot.
    bool        AttachHardImage(int slot, LPCTSTR sFileName);
    /// \brief Empty hard drive slot.
    void        DetachHardImage(int slot);
    /// \brief Check if the hard drive attached.
    bool        IsHardImageAttached(int slot) const;
    /// \brief Check if the attached hard drive image is read-only.
    bool        IsHardImageReadOnly(int slot) const;
    uint16_t    GetHardPortWord(int slot, uint16_t port);  ///< To use from CSecondMemoryController only
    void        SetHardPortWord(int slot, uint16_t port, uint16_t data);  ///< To use from CSecondMemoryController only

    /// \brief Assign tape read callback function.
    void        SetTapeReadCallback(TAPEREADCALLBACK callback, int sampleRate);
    /// \brief Assign write read callback function.
    void        SetTapeWriteCallback(TAPEWRITECALLBACK callback, int sampleRate);
    /// \brief Assign sound output callback function.
    void        SetSoundGenCallback(SOUNDGENCALLBACK callback);
    /// \brief Assign serial port input/output callback functions.
    void        SetSerialCallbacks(SERIALINCALLBACK incallback, SERIALOUTCALLBACK outcallback);
    /// \brief Assign parallel port output callback function.
    void        SetParallelOutCallback(PARALLELOUTCALLBACK outcallback);
    /// \brief Assign network port input/output callback functions.
    void        SetNetworkCallbacks(NETWORKINCALLBACK incallback, NETWORKOUTCALLBACK outcallback);
    /// \brief Assign terminal callback functions.
    void        SetTerminalCallback(TERMINALOUTCALLBACK callback) { m_TerminalOutCallback = callback; }
    /// \brief Assign callback for CPU/PPU instruction execution, nullptr to remove
    void        SetInstructionCallback(INSTRUCTIONCALLBACK callback);
    /// \brief Assign callback for CPU/PPU interrupts, nullptr to remove
    void        SetInterruptCallback(INTERRUPTCALLBACK callback);
public:  // Saving/loading emulator status
    void        SaveToImage(std::span<uint8_t, UKNCIMAGE_SIZE> image);
    void        LoadFromImage(std::span<const uint8_t, UKNCIMAGE_SIZE> image);

    void        SetSound(uint16_t val);
    void        SetSoundAY(bool onoff) { m_okSoundAY = onoff; }
    void        SetSoundCovox(bool onoff) { m_okSoundCovox = onoff; }
    void        SetMouse(bool onoff);
private: // Timing
    uint16_t    m_multiply;
    uint16_t    freq_per[6];
    uint16_t    freq_out[6];
    uint16_t    freq_enable[6];
    int         m_pputicks;
    int         m_cputicks;
    unsigned int m_lineticks;
private:
    const uint16_t* m_CPUbps;  ///< CPU breakpoint list, ends with 177777 value
    const uint16_t* m_PPUbps;  ///< PPU breakpoint list, ends with 177777 value
    uint32_t    m_dwTrace;  ///< Trace flags

    uint16_t    m_timer;
    uint16_t    m_timerreload;
    uint16_t    m_timerflags;
    uint16_t    m_timerdivider;

    chan_stc    m_chancputx[3];
    chan_stc    m_chancpurx[2];
    chan_stc    m_chanpputx[2];
    chan_stc    m_chanppurx[3];
    uint8_t     m_chan0disabled;
    uint8_t     m_irq_cpureset;

    uint8_t     m_scanned_key;
    kbd_row     m_kbd_matrix[16];

    bool        m_okSoundAY;
    uint8_t     m_nSoundAYReg[3];  ///< AY current register
    bool        m_okSoundCovox;

private:
    TAPEREADCALLBACK    m_TapeReadCallback;
    TAPEWRITECALLBACK   m_TapeWriteCallback;
    int                 m_nTapeSampleRate;
    uint64_t            m_tapeSamplePhase = 0;
    SOUNDGENCALLBACK    m_SoundGenCallback;
    int                 m_SoundPrevValue;  ///< Previous value of the sound signal
    int                 m_SoundChanges;  ///< Sound signal 0 to 1 changes since the beginning of the frame
    SERIALINCALLBACK    m_SerialInCallback;
    SERIALOUTCALLBACK   m_SerialOutCallback;
    PARALLELOUTCALLBACK m_ParallelOutCallback;
    NETWORKINCALLBACK   m_NetworkInCallback;
    NETWORKOUTCALLBACK  m_NetworkOutCallback;
    TERMINALOUTCALLBACK m_TerminalOutCallback;

    void DoSound();
};

inline uint16_t CMotherboard::GetRAMWord(int plan, uint16_t offset) const
{
    ASSERT(plan >= 0 && plan <= 2);
    offset &= 0xFFFE;
    return static_cast<uint16_t>(m_pRAM[plan][offset] | (m_pRAM[plan][offset + 1] << 8));
}
inline uint8_t CMotherboard::GetRAMByte(int plan, uint16_t offset) const
{
    ASSERT(plan >= 0 && plan <= 2);
    return m_pRAM[plan][offset];
}
inline void CMotherboard::SetRAMWord(int plan, uint16_t offset, uint16_t word)
{
    ASSERT(plan >= 0 && plan <= 2);
    offset &= 0xFFFE;
    m_pRAM[plan][offset] = static_cast<uint8_t>(word);
    m_pRAM[plan][offset + 1] = static_cast<uint8_t>(word >> 8);
}
inline void CMotherboard::SetRAMByte(int plan, uint16_t offset, uint8_t byte)
{
    ASSERT(plan >= 0 && plan <= 2);
    m_pRAM[plan][offset] = byte;
}


//////////////////////////////////////////////////////////////////////
