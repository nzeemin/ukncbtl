/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Memory.h
//

#pragma once

#include "Defines.h"
#include "Board.h"

class CProcessor;


//////////////////////////////////////////////////////////////////////

// TranslateAddress result code
#define ADDRTYPE_RAM0  0    // RAM plane 0
#define ADDRTYPE_RAM1  1    // RAM plane 1
#define ADDRTYPE_RAM2  2    // RAM plane 2
#define ADDRTYPE_RAM12 4    // RAM plane 1 & 2 - a special case for CPU memory
#define ADDRTYPE_ROM   32   // ROM
#define ADDRTYPE_ROMCART1 40  // ADDRTYPE_ROM + 8  -- ROM cartridge #1
#define ADDRTYPE_ROMCART2 48  // ADDRTYPE_ROM + 16 -- ROM cartridge #2
#define ADDRTYPE_IO    64   // I/O port; bits 5..0 -- device number
#define ADDRTYPE_NONE  128  // No data
#define ADDRTYPE_DENY  255  // Access denied
#define ADDRTYPE_MASK_RAM  7  // Mask to get memory plane number


//////////////////////////////////////////////////////////////////////


// Memory control device for CPU or PPU (abstract class)
class CMemoryController
{
protected:
    CMotherboard*   m_pBoard;
    CProcessor*     m_pProcessor;
    BYTE*           m_pMapping;  // Memory mapping
    CBusDevice**    m_pDevices;  // Attached bus devices
    int             m_nDeviceCount;
public:
    CMemoryController();
    ~CMemoryController();
    void        Attach(CMotherboard* board, CProcessor* processor)
                    { m_pBoard = board;  m_pProcessor = processor; }
    // Attach/reattach bus devices
    void        AttachDevices(const CBusDevice** pDevices);
    virtual void UpdateMemoryMap();
    // Reset to initial state
    virtual void DCLO_Signal() = 0;  // DCLO signal
    virtual void ResetDevices() = 0;  // INIT signal
public:  // Access to memory
    // Read command for execution
    WORD GetWordExec(WORD address, BOOL okHaltMode) { return GetWord(address, okHaltMode, TRUE); }
    // Read word from memory
    WORD GetWord(WORD address, BOOL okHaltMode) { return GetWord(address, okHaltMode, FALSE); }
    // Read word
    WORD GetWord(WORD address, BOOL okHaltMode, BOOL okExec);
    // Write word
    void SetWord(WORD address, BOOL okHaltMode, WORD word);
    // Read byte
    BYTE GetByte(WORD address, BOOL okHaltMode);
    // Write byte
    void SetByte(WORD address, BOOL okHaltMode, BYTE byte);
    // Read word from memory for debugger
    WORD GetWordView(WORD address, BOOL okHaltMode, BOOL okExec, BOOL* pValid);
    // Read word from port for debugger
    virtual WORD GetPortView(WORD address) = 0;
    // Read SEL register
    virtual WORD GetSelRegister() = 0;
public:  // Saving/loading emulator status (64 bytes)
    virtual void SaveToImage(BYTE* pImage) = 0;
    virtual void LoadFromImage(const BYTE* pImage) = 0;
protected:
    // Determite memory type for given address - see ADDRTYPE_Xxx constants
    //   okHaltMode - processor mode (USER/HALT)
    //   okExec - TRUE: read instruction for execution; FALSE: read memory
    //   pOffset - result - offset in memory plane
    virtual int TranslateAddress(WORD address, BOOL okHaltMode, BOOL okExec, WORD* pOffset, BOOL okView = FALSE) = 0;
protected:  // Access to I/O ports
    virtual WORD GetPortWord(WORD address) = 0;
    virtual void SetPortWord(WORD address, WORD word) = 0;
    virtual BYTE GetPortByte(WORD address) = 0;
    virtual void SetPortByte(WORD address, BYTE byte) = 0;
};

class CFirstMemoryController : public CMemoryController  // CPU memory control device
{
    friend class CMotherboard;
public:
    CFirstMemoryController();
    virtual void DCLO_Signal();  // DCLO signal
    virtual void ResetDevices();  // INIT signal
public:
    virtual int TranslateAddress(WORD address, BOOL okHaltMode, BOOL okExec, WORD* pOffset, BOOL okView);
    virtual WORD GetSelRegister() { return 0160000; }
    virtual WORD GetPortView(WORD address);
protected:  // Access to I/O ports
    virtual WORD GetPortWord(WORD address);
    virtual void SetPortWord(WORD address, WORD word);
    virtual BYTE GetPortByte(WORD address);
    virtual void SetPortByte(WORD address, BYTE byte);
public:  // Saving/loading emulator status (64 bytes)
    virtual void SaveToImage(BYTE* pImage);
    virtual void LoadFromImage(const BYTE* pImage);
public:  // CPU specific
    BOOL SerialInput(BYTE inputByte);
    
protected:  // Implementation
    int         m_NetStation;  // Network station number
    WORD        m_Port176640;  // Plane address register
    WORD        m_Port176642;  // Plane 1 & 2 data register
    WORD        m_Port176644;
    WORD        m_Port176646;
    WORD        m_Port176570;  // RS-232 receiver state
    WORD        m_Port176572;  // RS-232 receiver data (bits 0-7)
    WORD        m_Port176574;  // RS-232 translator state
    WORD        m_Port176576;  // RS-232 translator data (bits 0-7)
};

class CSecondMemoryController : public CMemoryController  // PPU memory control device
{
    friend class CMotherboard;
public:
    CSecondMemoryController();
    virtual void UpdateMemoryMap();
    virtual void DCLO_Signal();  // DCLO signal
    virtual void ResetDevices();  // INIT signal
    virtual void DCLO_177716();
    virtual void Init_177716();
public:
    virtual int TranslateAddress(WORD address, BOOL okHaltMode, BOOL okExec, WORD* pOffset, BOOL okView);
    virtual WORD GetSelRegister() { return 0160000; }
    virtual WORD GetPortView(WORD address);
protected:  // Access to I/O ports
    virtual WORD GetPortWord(WORD address);
    virtual void SetPortWord(WORD address, WORD word);
    virtual BYTE GetPortByte(WORD address);
    virtual void SetPortByte(WORD address, BYTE byte);  //TODO
public:  // Saving/loading emulator status (64 bytes)
    virtual void SaveToImage(BYTE* pImage);
    virtual void LoadFromImage(const BYTE* pImage);
public:  // PPU specifics
    void KeyboardEvent(BYTE scancode, BOOL okPressed);  // Keyboard key pressed or released
    BOOL TapeInput(BOOL inputBit);
    BOOL TapeOutput();
protected:  // Implementation
    WORD        m_Port177010;  // Plane address register
    WORD        m_Port177012;  // Plane 0 data register
    WORD        m_Port177014;  // Plane 1 & 2 data register

    WORD        m_Port177026;  // Plane mask
    WORD        m_Port177024;  // SpriteByte
    WORD        m_Port177020;  // Background color 1
    WORD        m_Port177022;  // Background color 2
    WORD        m_Port177016;  // Pixel Color

    WORD        m_Port177700;  // Keyboard status
    WORD        m_Port177702;  // Keyboard data
    WORD        m_Port177716;  // System control register

    WORD        m_Port177054;  // address space control

    BYTE        m_Port177100;  // i8255 port A -- Parallel port output data
    BYTE        m_Port177101;  // i8255 port B
    BYTE        m_Port177102;  // i8255 port C
};


//////////////////////////////////////////////////////////////////////
