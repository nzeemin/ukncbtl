// Board.h
//

#pragma once

#include "Defines.h"

class CProcessor;
class CMemoryController;

//floppy debug
#define FLOPPY_FSM_WAITFORLSB	0
#define FLOPPY_FSM_WAITFORMSB	1
#define FLOPPY_FSM_WAITFORTERM1	2
#define FLOPPY_FSM_WAITFORTERM2	3

// Emulator image constants
#define UKNCIMAGE_HEADER_SIZE 256
#define UKNCIMAGE_SIZE (UKNCIMAGE_HEADER_SIZE + (32 + 64 * 3) * 1024)
#define UKNCIMAGE_HEADER1 0x434E4B55  // "UKNC"
#define UKNCIMAGE_HEADER2 0x214C5442  // "BTL!"
#define UKNCIMAGE_VERSION 0x00010000  // 1.0


#define KEYB_RUS		0x01
#define KEYB_LAT		0x02
#define KEYB_LOWERREG	0x10


typedef struct chan_tag{
	BYTE	data;
	BYTE	ready;
	BYTE	irq;
}chan_stc;

// Tape emulator callback used to read a tape recorded data.
// Input:
//   samples    Number of samples to play.
// Output:
//   result     Bit to put in tape input port.
typedef BOOL (CALLBACK* TAPEREADCALLBACK)(UINT samples);

// Sound generator callback function type
typedef void (CALLBACK* SOUNDGENCALLBACK)(unsigned short L, unsigned short R);

class CFloppyController;

//////////////////////////////////////////////////////////////////////

class CMotherboard  // UKNC computer
{

public:  // Construct / destruct
    CMotherboard();
    ~CMotherboard();

protected:  // Devices
    CProcessor*     m_pCPU;  // CPU device
    CProcessor*     m_pPPU;  // PPU device
    CMemoryController*  m_pFirstMemCtl;  // CPU memory control
    CMemoryController*  m_pSecondMemCtl;  // PPU memory control
    CFloppyController*  m_pFloppyCtl;  // FDD control
public:  // Getting devices
    CProcessor*     GetCPU() { return m_pCPU; }
    CProcessor*     GetPPU() { return m_pPPU; }
    CMemoryController*  GetCPUMemoryController() { return m_pFirstMemCtl; }
    CMemoryController*  GetPPUMemoryController() { return m_pSecondMemCtl; }

protected:  // Memory
    BYTE*           m_pRAM[3];  // RAM, three planes, 64 KB each
    BYTE*           m_pROM;     // System ROM, 32 KB
    BYTE*           m_pROMCart[2];  // ROM cartridges #1 and #2, 24 KB each
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
    void        DebugTicks();  // One Debug PPU tick -- use for debug step or debug breakpoint
    void        SetCPUBreakpoint(WORD bp) { m_CPUbp = bp; } // Set CPU breakpoint
    void        SetPPUBreakpoint(WORD bp) { m_PPUbp = bp; } // Set PPU breakpoint
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
    void        Reset();  // Reset computer
    void        LoadROM(const BYTE* pBuffer);  // Load 32 KB ROM image from the biffer
    void        LoadROMCartridge(int cartno, const BYTE* pBuffer);  // Load 24 KB ROM cartridge image
    void        LoadRAM(int plan, const BYTE* pBuffer);  // Load 32 KB RAM image from the biffer
    void        Tick8000();  // Tick 8.00 MHz
    void        Tick6250();  // Tick 6.25 MHz
    void        Tick50();    // Tick 50 Hz - goes to CPU/PPU EVNT line
	void		TimerTick();		// Timer Tick, 2uS -- dividers are within timer routine
	WORD		GetTimerValue();	// returns current timer value
	WORD		GetTimerValueView() { return m_timer; }	// Returns current timer value for debugger
	WORD		GetTimerReload();	// returns timer reload value
	WORD		GetTimerReloadView() { return m_timerreload; }	// Returns timer reload value for debugger
	WORD		GetTimerState();	// returns timer state
	WORD		GetTimerStateView() { return m_timerflags; } // Returns timer state for debugger

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

	//void		FloppyDebug(BYTE val);

	void		SetTimerReload(WORD val);	//sets timer reload value
	void		SetTimerState(WORD val);	//sets timer state
    void        ExecuteCPU();  // Execute one CPU instruction
    void        ExecutePPU();  // Execute one PPU instruction
    BOOL        SystemFrame();  // Do one frame -- use for normal run
    void        KeyboardEvent(BYTE scancode, BOOL okPressed);  // Key pressed or released
	WORD        GetKeyboardRegister(void);
    
    BOOL        AttachFloppyImage(int slot, LPCTSTR sFileName);
    void        DetachFloppyImage(int slot);
    BOOL        IsFloppyImageAttached(int slot);
    BOOL        IsFloppyReadOnly(int slot);
	WORD		GetFloppyState();
	WORD		GetFloppyData();
	void		SetFloppyState(WORD val);
	void		SetFloppyData(WORD val);

    BOOL        IsROMCartridgeLoaded(int cartno);
    void        UnloadROMCartridge(int cartno);

	void		SetTapeReadCallback(TAPEREADCALLBACK callback, int sampleRate);
	void		SetSoundGenCallback(SOUNDGENCALLBACK callback);

public:  // Saving/loading emulator status
    void        SaveToImage(BYTE* pImage);
    void        LoadFromImage(const BYTE* pImage);
	void		SetSound(WORD val);
private: // Timing
	int m_multiply;
	int freq_per[6];
	int freq_out[6];
	int freq_enable[6];
    int m_pputicks;
    int m_cputicks;
    unsigned int m_lineticks;
private:
    WORD        m_CPUbp;
    WORD        m_PPUbp;
	WORD		m_timer;
	WORD		m_timerreload;
	WORD		m_timerflags;
	WORD		m_timerdivider;
	
	chan_stc	m_chancputx[3];
	chan_stc	m_chancpurx[2];
	chan_stc	m_chanpputx[2];
	chan_stc	m_chanppurx[3];

	BYTE		m_chan0disabled;

    TAPEREADCALLBACK m_TapeReadCallback;
	int			m_nTapeReadSampleRate;
    SOUNDGENCALLBACK m_SoundGenCallback;

	void DoSound(void);
	
};


//////////////////////////////////////////////////////////////////////
