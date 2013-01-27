/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Board.h
//

#pragma once

#include "Defines.h"

class CProcessor;
class CMemoryController;

// Floppy debug constants
#define FLOPPY_FSM_WAITFORLSB	0
#define FLOPPY_FSM_WAITFORMSB	1
#define FLOPPY_FSM_WAITFORTERM1	2
#define FLOPPY_FSM_WAITFORTERM2	3

// Emulator image constants
#define UKNCIMAGE_HEADER_SIZE 512
#define UKNCIMAGE_SIZE (UKNCIMAGE_HEADER_SIZE + (32 + 64 * 3) * 1024)
#define UKNCIMAGE_HEADER1 0x434E4B55  // "UKNC"
#define UKNCIMAGE_HEADER2 0x214C5442  // "BTL!"
#define UKNCIMAGE_VERSION 0x00010001  // 1.1

#define KEYB_RUS		0x01
#define KEYB_LAT		0x02
#define KEYB_LOWERREG	0x10

typedef struct chan_tag
{
    BYTE	data;
    BYTE	ready;
    BYTE	irq;
    BYTE	rdwr;
} chan_stc;

typedef struct kbd_row_tag
{
	BYTE	processed;
	BYTE	row_Y;
} kbd_row;

// Tape emulator callback used to read a tape recorded data.
// Input:
//   samples    Number of samples to play.
// Output:
//   result     Bit to put in tape input port.
typedef BOOL (CALLBACK* TAPEREADCALLBACK)(unsigned int samples);

// Tape emulator callback used to write a data to tape.
// Input:
//   value      Sample value to write.
typedef void (CALLBACK* TAPEWRITECALLBACK)(int value, unsigned int samples);

// Sound generator callback function type
typedef void (CALLBACK* SOUNDGENCALLBACK)(unsigned short L, unsigned short R);

// Network port callback for receiving
// Output:
//   pbyte      Byte received
//   result     TRUE means we have a new byte, FALSE means not ready yet
typedef BOOL (CALLBACK* NETWORKINCALLBACK)(BYTE* pbyte);

// Network port callback for translating
// Input:
//   byte       A byte to translate
// Output:
//   result     TRUE means we translated the byte successfully, FALSE means we have an error
typedef BOOL (CALLBACK* NETWORKOUTCALLBACK)(BYTE byte);

// Serial port callback for receiving
// Output:
//   pbyte      Byte received
//   result     TRUE means we have a new byte, FALSE means not ready yet
typedef BOOL (CALLBACK* SERIALINCALLBACK)(BYTE* pbyte);

// Serial port callback for translating
// Input:
//   byte       A byte to translate
// Output:
//   result     TRUE means we translated the byte successfully, FALSE means we have an error
typedef BOOL (CALLBACK* SERIALOUTCALLBACK)(BYTE byte);

// Parallel port output callback
// Input:
//   byte       An output byte
// Output:
//   result     TRUE means OK, FALSE means we have an error
typedef BOOL (CALLBACK* PARALLELOUTCALLBACK)(BYTE byte);


class CFloppyController;
class CHardDrive;

//////////////////////////////////////////////////////////////////////


// \brief Bus device
class CBusDevice
{
public:
    // Name of the device
    virtual LPCTSTR GetName() const = 0;
    // Device address ranges: [address, length] pairs, last pair is [0,0]
    virtual const WORD* GetAddressRanges() const = 0;
};

/// \brief UKNC computer
class CMotherboard
{

public:  // Construct / destruct
    CMotherboard();
    ~CMotherboard();

protected:  // Devices
    CProcessor*     m_pCPU;  ///< CPU device
    CProcessor*     m_pPPU;  ///< PPU device
    CMemoryController*  m_pFirstMemCtl;  ///< CPU memory control
    CMemoryController*  m_pSecondMemCtl;  ///< PPU memory control
    CFloppyController*  m_pFloppyCtl;  ///< FDD control
    CHardDrive* m_pHardDrives[2];  ///< HDD control

public:  // Getting devices
    CProcessor*     GetCPU() { return m_pCPU; }  ///< Getter for m_pCPU
    CProcessor*     GetPPU() { return m_pPPU; }  ///< Getter for m_pPPU
    CMemoryController*  GetCPUMemoryController() { return m_pFirstMemCtl; }  ///< Get CPU memory controller
    CMemoryController*  GetPPUMemoryController() { return m_pSecondMemCtl; }  ///< Get PPU memory controller

protected:
    CBusDevice** m_pCpuDevices;
    CBusDevice** m_pPpuDevices;
public:
    const CBusDevice** GetCPUBusDevices() { return (const CBusDevice**) m_pCpuDevices; }
    const CBusDevice** GetPPUBusDevices() { return (const CBusDevice**) m_pPpuDevices; }

protected:  // Memory
    BYTE*           m_pRAM[3];  ///< RAM, three planes, 64 KB each
    BYTE*           m_pROM;     ///< System ROM, 32 KB
    BYTE*           m_pROMCart[2];  ///< ROM cartridges #1 and #2, 24 KB each
public:  // Memory access
    WORD        GetRAMWord(int plan, WORD offset);
    BYTE        GetRAMByte(int plan, WORD offset);
    void        SetRAMWord(int plan, WORD offset, WORD word);
    void        SetRAMByte(int plan, WORD offset, BYTE byte);
    WORD        GetROMWord(WORD offset);
    BYTE        GetROMByte(WORD offset);
    WORD        GetROMCartWord(int cartno, WORD offset);
    BYTE        GetROMCartByte(int cartno, WORD offset);
public:  // Debug
    void        DebugTicks();  ///< One Debug PPU tick -- use for debug step or debug breakpoint
    void        SetCPUBreakpoint(WORD bp) { m_CPUbp = bp; } ///< Set CPU breakpoint
    void        SetPPUBreakpoint(WORD bp) { m_PPUbp = bp; } ///< Set PPU breakpoint
    chan_stc	GetChannelStruct(unsigned char cpu,unsigned char chan, unsigned char tx)
    {//cpu==1 ,ppu==0; tx==1, rx==0
        if(cpu)
        {
            if(tx)
                return m_chancputx[chan];
            else
                return m_chancpurx[chan];
        }
        else
        {
            if(tx)
                return m_chanpputx[chan];
            else
                return m_chanppurx[chan];
        }
    }

public:  // System control
    void        Reset();  ///< Reset computer
    void        LoadROM(const BYTE* pBuffer);  ///< Load 32 KB ROM image from the biffer
    void        LoadROMCartridge(int cartno, const BYTE* pBuffer);  ///< Load 24 KB ROM cartridge image
    void        LoadRAM(int plan, const BYTE* pBuffer);  ///< Load 32 KB RAM image from the biffer
    void        SetNetStation(int station);  // Network station number
    void        Tick8000();  ///< Tick 8.00 MHz
    void        Tick6250();  ///< Tick 6.25 MHz
    void        Tick50();    ///< Tick 50 Hz - goes to CPU/PPU EVNT line
    void		TimerTick(); ///< Timer Tick, 2uS -- dividers are within timer routine
    void        ResetFloppy();     ///< INIT signal for FDD
    WORD		GetTimerValue();	///< Returns current timer value
    WORD		GetTimerValueView() { return m_timer; }	///< Returns current timer value for debugger
    WORD		GetTimerReload();	///< Returns timer reload value
    WORD		GetTimerReloadView() { return m_timerreload; }	///< Returns timer reload value for debugger
    WORD		GetTimerState();	///< Returns timer state
    WORD		GetTimerStateView() { return m_timerflags; } ///< Returns timer state for debugger

    void		ChanWriteByCPU(BYTE chan, BYTE data);
    void		ChanWriteByPPU(BYTE chan, BYTE data);
    BYTE		ChanReadByCPU(BYTE chan);
    BYTE		ChanReadByPPU(BYTE chan);
    
    BYTE		ChanRxStateGetCPU(BYTE chan);
    BYTE		ChanTxStateGetCPU(BYTE chan);
    BYTE		ChanRxStateGetPPU();
    BYTE		ChanTxStateGetPPU();
    void		ChanRxStateSetCPU(BYTE chan, BYTE state);
    void		ChanTxStateSetCPU(BYTE chan, BYTE state);
    void		ChanRxStateSetPPU(BYTE state);
    void		ChanTxStateSetPPU(BYTE state);

    void		ChanResetByCPU();
    void		ChanResetByPPU();

    //void		FloppyDebug(BYTE val);

    void        SetTimerReload(WORD val);	///< Sets timer reload value
    void        SetTimerState(WORD val);	///< Sets timer state
    void        ExecuteCPU();  ///< Execute one CPU instruction
    void        ExecutePPU();  ///< Execute one PPU instruction
    BOOL        SystemFrame();  ///< Do one frame -- use for normal run
    void        KeyboardEvent(BYTE scancode, BOOL okPressed);  ///< Key pressed or released
    WORD        GetKeyboardRegister(void);
    WORD        GetScannedKey() {return m_scanned_key; }

    /// \brief Attach floppy image to the slot -- insert the disk.
    BOOL        AttachFloppyImage(int slot, LPCTSTR sFileName);
    /// \brief Empty the floppy slot -- remove the disk.
    void        DetachFloppyImage(int slot);
    /// \brief Check if the floppy attached.
    BOOL        IsFloppyImageAttached(int slot) const;
    /// \brief Check if the attached floppy image is read-only.
    BOOL        IsFloppyReadOnly(int slot) const;
    /// \brief Check if the floppy drive engine rotates the disks.
    BOOL        IsFloppyEngineOn() const;
    WORD        GetFloppyState();
    WORD        GetFloppyData();
    void        SetFloppyState(WORD val);
    void        SetFloppyData(WORD val);

    /// \brief Check if ROM cartridge image assigned to the cartridge slot.
    BOOL        IsROMCartridgeLoaded(int cartno) const;
    /// \brief Empty the ROM cartridge slot.
    void        UnloadROMCartridge(int cartno);

    /// \brief Attach hard drive image to the slot.
    BOOL        AttachHardImage(int slot, LPCTSTR sFileName);
    /// \brief Empty hard drive slot.
    void        DetachHardImage(int slot);
    /// \brief Check if the hard drive attached.
    BOOL        IsHardImageAttached(int slot) const;
    /// \brief Check if the attached hard drive image is read-only.
    BOOL        IsHardImageReadOnly(int slot) const;
    WORD        GetHardPortWord(int slot, WORD port);  ///< To use from CSecondMemoryController only
    void        SetHardPortWord(int slot, WORD port, WORD data);  ///< To use from CSecondMemoryController only

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
public:  // Saving/loading emulator status
    void        SaveToImage(BYTE* pImage);
    void        LoadFromImage(const BYTE* pImage);
    void        SetSound(WORD val);
private: // Timing
    WORD        m_multiply;
    WORD        freq_per[6];
    WORD        freq_out[6];
    WORD        freq_enable[6];
    int         m_pputicks;
    int         m_cputicks;
    unsigned int m_lineticks;
private:
    WORD        m_CPUbp;  ///< CPU breakpoint, 177777 if not set
    WORD        m_PPUbp;  ///< PPU breakpoint, 177777 if not set

    WORD		m_timer;
    WORD		m_timerreload;
    WORD		m_timerflags;
    WORD		m_timerdivider;
    
    chan_stc	m_chancputx[3];
    chan_stc	m_chancpurx[2];
    chan_stc	m_chanpputx[2];
    chan_stc	m_chanppurx[3];

    BYTE		m_chan0disabled;
    BYTE		m_irq_cpureset;

	BYTE		m_scanned_key;
	kbd_row		m_kbd_matrix[16];

private:
    TAPEREADCALLBACK m_TapeReadCallback;
    TAPEWRITECALLBACK m_TapeWriteCallback;
    int			m_nTapeSampleRate;
    SOUNDGENCALLBACK m_SoundGenCallback;
    SERIALINCALLBACK    m_SerialInCallback;
    SERIALOUTCALLBACK   m_SerialOutCallback;
    PARALLELOUTCALLBACK m_ParallelOutCallback;
    NETWORKINCALLBACK   m_NetworkInCallback;
    NETWORKOUTCALLBACK  m_NetworkOutCallback;

    void DoSound(void);
    
};

inline WORD CMotherboard::GetRAMWord(int plan, WORD offset)
{
    ASSERT(plan >= 0 && plan <= 2);
    return *((WORD*)(m_pRAM[plan] + (offset & 0xFFFE))); 
}
inline BYTE CMotherboard::GetRAMByte(int plan, WORD offset) 
{ 
    ASSERT(plan >= 0 && plan <= 2);
    return m_pRAM[plan][offset]; 
}

//////////////////////////////////////////////////////////////////////
