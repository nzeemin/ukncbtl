/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

/// \file Memory.h  Memory controller class

#pragma once

#include "Defines.h"
#include "Board.h"
#include <array>
#include <vector>

class CProcessor;


//////////////////////////////////////////////////////////////////////

// TranslateAddress result code
inline constexpr auto ADDRTYPE_RAM0 = 0;    ///< RAM plane 0
inline constexpr auto ADDRTYPE_RAM1 = 1;    ///< RAM plane 1
inline constexpr auto ADDRTYPE_RAM2 = 2;    ///< RAM plane 2
inline constexpr auto ADDRTYPE_RAM12 = 4;    ///< RAM plane 1 & 2 - a special case for CPU memory
inline constexpr auto ADDRTYPE_ROM = 32;   ///< ROM
inline constexpr auto ADDRTYPE_ROMCART1 = 40;  ///< ADDRTYPE_ROM + 8  -- ROM cartridge #1
inline constexpr auto ADDRTYPE_ROMCART2 = 48;  ///< ADDRTYPE_ROM + 16 -- ROM cartridge #2
inline constexpr auto ADDRTYPE_IO = 64;   ///< I/O port; bits 5..0 -- device number
inline constexpr auto ADDRTYPE_NONE = 128;  ///< No data
inline constexpr auto ADDRTYPE_DENY = 255;  ///< Access denied
inline constexpr auto ADDRTYPE_MASK_RAM = 7;  ///< Mask to get memory plane number

struct MemoryWordView
{
    uint16_t value;
    int addressType;
};


//////////////////////////////////////////////////////////////////////


/// \brief Memory control device for CPU or PPU (abstract class)
class CMemoryController
{
protected:
    CMotherboard*   m_pBoard;       ///< Motherboard attached
    CProcessor*     m_pProcessor;   ///< Processor attached
    std::array<uint8_t, 65536> m_pMapping{}; ///< Memory mapping
    std::vector<const CBusDevice*> m_pDevices; ///< Non-owning bus devices
public:
    CMemoryController();
    virtual ~CMemoryController();
    void        Attach(CMotherboard* board, CProcessor* processor)
    { m_pBoard = board;  m_pProcessor = processor; }
    /// \brief Attach/reattach bus devices
    void        AttachDevices(std::span<const CBusDevice* const> devices);
    virtual void UpdateMemoryMap();
    // Reset to initial state
    virtual void DCLO_Signal() = 0;  ///< DCLO signal
    virtual void ResetDevices() = 0;  ///< INIT signal
public:  // Access to memory
    /// \brief Read word
    uint16_t GetWord(uint16_t address, bool okHaltMode, bool okExec);
    /// \brief Write word
    void SetWord(uint16_t address, bool okHaltMode, uint16_t word);
    /// \brief Read byte
    uint8_t GetByte(uint16_t address, bool okHaltMode);
    /// \brief Write byte
    void SetByte(uint16_t address, bool okHaltMode, uint8_t byte);
    /// \brief Read word from memory for debugger
    [[nodiscard]] MemoryWordView GetWordView(uint16_t address, bool okHaltMode, bool okExec) const;
    /// \brief Read word from port for debugger
    virtual uint16_t GetPortView(uint16_t address) const = 0;
    /// \brief Read SEL register
    virtual uint16_t GetSelRegister() const = 0;
public:  // Saving/loading emulator status (64 bytes)
    virtual void SaveToImage(std::span<uint8_t, 64> image) = 0;
    virtual void LoadFromImage(std::span<const uint8_t, 64> image) = 0;
protected:
    /// \brief Determine memory type for given address - see ADDRTYPE_Xxx constants
    /// \param okHaltMode  processor mode (USER/HALT)
    /// \param okExec  true: read instruction for execution; false: read memory
    /// \param pOffset  result -- offset in memory plane
    virtual int TranslateAddress(uint16_t address, bool okHaltMode, bool okExec, uint16_t* pOffset, bool okView = false) const = 0;
protected:  // Access to I/O ports
    virtual uint16_t GetPortWord(uint16_t address) = 0;
    virtual void SetPortWord(uint16_t address, uint16_t word) = 0;
    virtual uint8_t GetPortByte(uint16_t address) = 0;
    virtual void SetPortByte(uint16_t address, uint8_t byte) = 0;
};

/// \brief CPU memory control device
class CFirstMemoryController : public CMemoryController
{
    friend class CMotherboard;
public:
    CFirstMemoryController();
    void DCLO_Signal() override;  ///< DCLO signal
    void ResetDevices() override;  ///< INIT signal
public:
    int TranslateAddress(uint16_t address, bool okHaltMode, bool okExec, uint16_t* pOffset, bool okView) const override;
    uint16_t GetSelRegister() const override { return 0160000; }
    uint16_t GetPortView(uint16_t address) const override;
protected:  // Access to I/O ports
    uint16_t GetPortWord(uint16_t address) override;
    void SetPortWord(uint16_t address, uint16_t word) override;
    uint8_t GetPortByte(uint16_t address) override;
    void SetPortByte(uint16_t address, uint8_t byte) override;
public:  // Saving/loading emulator status (64 bytes)
    void SaveToImage(std::span<uint8_t, 64> image) override;
    void LoadFromImage(std::span<const uint8_t, 64> image) override;
public:  // CPU specific
    bool SerialInput(uint8_t inputByte);
    bool NetworkInput(uint8_t inputByte);
protected:  // Implementation
    uint16_t    m_NetStation;  ///< Network station number
    uint16_t    m_Port176560;  ///< Network receiver state
    uint16_t    m_Port176562;  ///< Network receiver data (bits 0-7)
    uint16_t    m_Port176564;  ///< Network translator state
    uint16_t    m_Port176566;  ///< Network translator data (bits 0-7)
    uint16_t    m_Port176640;  ///< Plane address register
    uint16_t    m_Port176642;  ///< Plane 1 & 2 data register
    uint16_t    m_Port176644;
    uint16_t    m_Port176646;
    uint16_t    m_Port176570;  ///< RS-232 receiver state
    uint16_t    m_Port176572;  ///< RS-232 receiver data (bits 0-7)
    uint16_t    m_Port176574;  ///< RS-232 translator state
    uint16_t    m_Port176576;  ///< RS-232 translator data (bits 0-7)
};

/// \brief PPU memory control device
class CSecondMemoryController : public CMemoryController
{
    friend class CMotherboard;
public:
    CSecondMemoryController();
    void UpdateMemoryMap() override;
    void DCLO_Signal() override;  ///< DCLO signal
    void ResetDevices() override;  ///< INIT signal
    virtual void DCLO_177716();
    virtual void Init_177716();
public:
    int TranslateAddress(uint16_t address, bool okHaltMode, bool okExec, uint16_t* pOffset, bool okView) const override;
    uint16_t GetSelRegister() const override { return 0160000; }
    uint16_t GetPortView(uint16_t address) const override;
protected:  // Access to I/O ports
    uint16_t GetPortWord(uint16_t address) override;
    void SetPortWord(uint16_t address, uint16_t word) override;
    uint8_t GetPortByte(uint16_t address) override;
    void SetPortByte(uint16_t address, uint8_t byte) override;  //TODO
public:  // Saving/loading emulator status (64 bytes)
    void SaveToImage(std::span<uint8_t, 64> image) override;
    void LoadFromImage(std::span<const uint8_t, 64> image) override;
public:  // PPU specifics
    void KeyboardEvent(uint8_t scancode, bool okPressed);  ///< Keyboard key pressed or released
    bool TapeInput(bool inputBit);
    bool TapeOutput();
    void SetMouse(bool onoff) { m_okMouse = onoff; }
    void MouseMove(int16_t dx, int16_t dy, bool btnLeft, bool btnRight);
protected:  // Implementation
    uint16_t    m_Port177010;  ///< Plane address register
    uint16_t    m_Port177012;  ///< Plane 0 data register
    uint16_t    m_Port177014;  ///< Plane 1 & 2 data register

    uint16_t    m_Port177026;  ///< Plane mask
    uint16_t    m_Port177024;  ///< SpriteByte
    uint16_t    m_Port177020;  ///< Background color 1
    uint16_t    m_Port177022;  ///< Background color 2
    uint16_t    m_Port177016;  ///< Pixel Color

    uint16_t    m_Port177700;  ///< Keyboard status
    uint16_t    m_Port177702;  ///< Keyboard data
    uint16_t    m_Port177716;  ///< System control register

    uint16_t    m_Port177054;  ///< address space control

    uint8_t     m_Port177100;  ///< i8255 port A -- Parallel port output data
    uint8_t     m_Port177101;  ///< i8255 port B
    uint8_t     m_Port177102;  ///< i8255 port C

    int16_t     m_mousedx, m_mousedy; // Mouse delta X, Y
    uint16_t    m_mouseflags;
    bool        m_okMouse;
};


//////////////////////////////////////////////////////////////////////
