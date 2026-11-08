/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

/// \file Emubase.h  Header for use of all emubase classes

#pragma once

#include "Board.h"
#include "Processor.h"
#include "Memory.h"
#include <optional>
#include <span>
#include <string>


//////////////////////////////////////////////////////////////////////
// Disassembler

/// \brief Disassemble one instruction of KM1801VM2 processor
/// \param[in]  memory Instruction words; missing extension words are treated as zero
/// \param[in]  addr    Address of the instruction
struct DisassembledInstruction
{
    std::basic_string<TCHAR> mnemonic;
    std::basic_string<TCHAR> argument;
    uint16_t length = 0;
};

/// \return The decoded instruction, or no value when memory is empty
[[nodiscard]] std::optional<DisassembledInstruction> DisassembleInstruction(
    std::span<const uint16_t> memory, uint16_t addr);

/// \brief Check if the instruction is a jump with a known target, for the jump arrow
/// \param[in]  memory   Memory image: the instruction and the word after it (the offset or the address of the target)
/// \param[in]  address  Address of the instruction
/// \return Distance in words to the known target, or no value
[[nodiscard]] std::optional<int> Disasm_CheckForJump(std::span<const uint16_t> memory, uint16_t address);

// Hints of the current instruction: up to three lines of text and the jump prediction
inline constexpr auto DISASM_HINT_LINES = 3;

struct DisasmHint
{
    std::array<TcharString, DISASM_HINT_LINES> lines{};
    int count = 0;
    bool isJump = false;
    bool jumpPredict = false;
};

/// \brief Prepare the hints of the instruction at PC
/// \param[in]  memory   Memory image: the instruction and the two words after it
/// \param[in]  processor Processor, the instruction is at its PC
/// \param[in]  memoryController Memory controller of the processor
/// \return The hint text and jump prediction
[[nodiscard]] DisasmHint Disasm_GetHint(std::span<const uint16_t> memory, const CProcessor& processor,
                                        const CMemoryController& memoryController);

/// Prepare the regular instruction hint independently of the jump hint.
[[nodiscard]] DisasmHint Disasm_GetInstructionHint(std::span<const uint16_t> memory,
    const CProcessor& processor, const CMemoryController& memoryController);

struct DisasmJumpHint
{
    TcharString text;
    bool predict = false;
};
/// Prepare the jump condition and its prediction.
[[nodiscard]] DisasmJumpHint Disasm_GetJumpConditionHint(std::span<const uint16_t> memory,
    const CProcessor& processor, const CMemoryController& memoryController);

/// \brief Convert two words of a FIS floating point number (DEC single precision) to a string
/// \param[in]  high        High word: sign, 8-bit exponent (bias 128), high 7 bits of the mantissa
/// \param[in]  low         Low word: low 16 bits of the mantissa
/// \return Shortest representation (up to nine digits) preserving the 24-bit mantissa
[[nodiscard]] TcharString Disasm_FloatToString(uint16_t high, uint16_t low);


//////////////////////////////////////////////////////////////////////
// CFloppy

inline constexpr auto SAMPLERATE = 22050;

inline constexpr auto FLOPPY_FSM_IDLE = 0;

inline constexpr auto FLOPPY_CMD_CORRECTION250 = 04;
inline constexpr auto FLOPPY_CMD_CORRECTION500 = 010;
inline constexpr auto FLOPPY_CMD_ENGINESTART = 020;  ///< Engine on/off
inline constexpr auto FLOPPY_CMD_SIDEUP = 040;  ///< Side: 1 -- upper head, 0 -- lower head
inline constexpr auto FLOPPY_CMD_DIR = 0100;  ///< Direction: 0 -- to center (towards trk0), 1 -- from center (towards trk80)
inline constexpr auto FLOPPY_CMD_STEP = 0200;  ///< Step / ready
inline constexpr auto FLOPPY_CMD_SEARCHSYNC = 0400;  ///< Search sync
inline constexpr auto FLOPPY_CMD_SKIPSYNC = 01000;  ///< Skip sync

inline constexpr auto FLOPPY_STATUS_TRACK0 = 01;  ///< Track 0 flag
inline constexpr auto FLOPPY_STATUS_RDY = 02;  ///< Ready status
inline constexpr auto FLOPPY_STATUS_WRITEPROTECT = 04;  ///< Write protect
inline constexpr auto FLOPPY_STATUS_MOREDATA = 0200;  ///< Need more data flag
inline constexpr auto FLOPPY_STATUS_CHECKSUMOK = 040000;  ///< Checksum verified OK
inline constexpr auto FLOPPY_STATUS_INDEXMARK = 0100000;  ///< Index flag, indicates the beginning of track

inline constexpr auto FLOPPY_RAWTRACKSIZE = 6250;     ///< Raw track size, bytes
#define FLOPPY_RAWMARKERSIZE            (FLOPPY_RAWTRACKSIZE / 2)
inline constexpr auto FLOPPY_INDEXLENGTH = 150;      ///< Length of index hole, in bytes of raw track image

/// \brief Floppy drive (one of four drives in the floppy controller)
/// \sa CFloppyController
struct CFloppyDrive
{
    FILE* fpFile;       ///< File pointer of the disk image file
    bool okNetRT11Image;  ///< true - .rtd image, false - .dsk image
    bool okReadOnly;    ///< Write protection flag
    uint16_t track;         ///< Track number: from 0 to 79
    uint16_t side;          ///< Disk side: 0 or 1
    uint16_t dataptr;       ///< Data offset within m_data - "head" position
    uint16_t datatrack;     ///< Track number of data in m_data array
    uint16_t dataside;      ///< Disk side of data in m_data array
    uint8_t data[FLOPPY_RAWTRACKSIZE];  ///< Raw track image for the current track
    uint8_t marker[FLOPPY_RAWMARKERSIZE];  ///< Marker positions

public:
    CFloppyDrive();
    void Reset();       ///< Reset the device
};

/// \brief UKNC floppy controller (MZ standard)
/// \sa CFloppyDrive
class CFloppyController
{
protected:
    CFloppyDrive m_drivedata[4];  ///< Floppy drives
    CFloppyDrive* m_pDrive;  ///< Current drive
    uint16_t m_drive;       ///< Current drive number: from 0 to 3
    uint16_t m_track;       ///< Track number: from 0 to 79
    uint16_t m_side;        ///< Disk side: 0 or 1
    uint16_t m_status;      ///< See FLOPPY_STATUS_XXX defines
    uint16_t m_flags;       ///< See FLOPPY_CMD_XXX defines
    uint16_t m_datareg;     ///< Read mode data register
    uint16_t m_writereg;    ///< Write mode data register
    bool m_writeflag;   ///< Write mode data register has data
    bool m_writemarker; ///< Write marker in m_marker
    uint16_t m_shiftreg;    ///< Write mode shift register
    bool m_shiftflag;   ///< Write mode shift register has data
    bool m_shiftmarker; ///< Write marker in m_marker
    bool m_writing;     ///< true = write mode, false = read mode
    bool m_searchsync;  ///< Read sub-mode: true = search for sync, false = just read
    bool m_crccalculus; ///< true = CRC is calculated now
    bool m_trackchanged;  ///< true = m_data was changed - need to save it into the file
    bool m_okTrace;         ///< Trace mode on/off

public:
    CFloppyController();
    ~CFloppyController();
    void Reset();       ///< Reset the device

    /// \brief Attach the image to the drive -- insert disk
    bool AttachImage(int drive, LPCTSTR sFileName);
    /// \brief Detach image from the drive -- remove disk
    void DetachImage(int drive);
    /// \brief Check if the drive has an image attached
    bool IsAttached(int drive) const { return (m_drivedata[drive].fpFile != nullptr); }
    /// \brief Check if the drive's attached image is read-only
    bool IsReadOnly(int drive) const { return m_drivedata[drive].okReadOnly; }
    /// \brief Check if floppy engine now rotates
    bool IsEngineOn() const { return (m_flags & FLOPPY_CMD_ENGINESTART) != 0; }
    uint16_t GetData();             ///< Reading port 177132 -- data
    uint16_t GetState();            ///< Reading port 177130 -- device status
    void SetCommand(uint16_t cmd);  ///< Writing to port 177130 -- commands
    void WriteData(uint16_t data);  ///< Writing to port 177132 -- data
    void Periodic();                ///< Rotate disk; call it each 64 us -- 15625 times per second
    void SetTrace(bool okTrace) { m_okTrace = okTrace; }  // Set trace mode on/off

private:
    void PrepareTrack();
    bool FlushChanges();  ///< If current track was changed - save it
};


//////////////////////////////////////////////////////////////////////
// CHardDrive

inline constexpr auto IDE_DISK_SECTOR_SIZE = 512;

/// \brief UKNC IDE hard drive
class CHardDrive
{
protected:
    FILE*   m_fpFile;           ///< File pointer for the attached HDD image
    bool    m_okReadOnly;       ///< Flag indicating that the HDD image file is read-only
    bool    m_okInverted;       ///< Flag indicating that the HDD image has inverted bits
    uint8_t m_status;           ///< IDE status register, see IDE_STATUS_XXX constants
    uint8_t m_error;            ///< IDE error register, see IDE_ERROR_XXX constants
    uint8_t m_command;          ///< Current IDE command, see IDE_COMMAND_XXX constants
    int     m_numcylinders;     ///< Cylinder count
    int     m_numheads;         ///< Head count
    int     m_numsectors;       ///< Sectors per track
    int     m_curcylinder;      ///< Current cylinder number
    int     m_curhead;          ///< Current head number
    int     m_cursector;        ///< Current sector number
    int     m_curheadreg;       ///< Current head number
    int     m_sectorcount;      ///< Sector counter for read/write operations
    uint8_t m_buffer[IDE_DISK_SECTOR_SIZE];  ///< Sector data buffer
    int     m_bufferoffset;     ///< Current offset within sector: 0..511
    int     m_timeoutcount;     ///< Timeout counter to wait for the next event
    int     m_timeoutevent;     ///< Current stage of operation, see TimeoutEvent enum

public:
    CHardDrive();
    ~CHardDrive();
    /// \brief Reset the device.
    void Reset();
    /// \brief Attach HDD image file to the device
    bool AttachImage(LPCTSTR sFileName);
    /// \brief Detach HDD image file from the device
    void DetachImage();
    /// \brief Check if the attached hard drive image is read-only
    bool IsReadOnly() const { return m_okReadOnly; }

public:
    /// \brief Read word from the device port
    uint16_t ReadPort(uint16_t port);
    /// \brief Write word th the device port
    void WritePort(uint16_t port, uint16_t data);
    /// \brief Rotate disk
    void Periodic();

private:
    bool CalculateOffset(long& offset) const;  ///< Validate CHS and calculate the image offset
    void HandleCommand(uint8_t command);  ///< Handle the IDE command
    void ReadNextSector();
    void ReadSectorDone();
    void WriteSectorDone();
    void NextSector();          ///< Advance to the next sector, CHS-based
    void ContinueRead();
    void ContinueWrite();
    void IdentifyDrive();       ///< Prepare m_buffer for the IDENTIFY DRIVE command
};


//////////////////////////////////////////////////////////////////////

class CSoundAY
{
protected:
    int index;
    int ready;
    unsigned Regs[16];
    int32_t lastEnable;
    int32_t PeriodA, PeriodB, PeriodC, PeriodN, PeriodE;
    int32_t CountA, CountB, CountC, CountN, CountE;
    uint32_t VolA, VolB, VolC, VolE;
    uint8_t EnvelopeA, EnvelopeB, EnvelopeC;
    uint8_t OutputA, OutputB, OutputC, OutputN;
    int8_t CountEnv;
    uint8_t Hold, Alternate, Attack, Holding;
    int32_t RNG;
protected:
    static unsigned int VolTable[32];

public:
    CSoundAY();
    void Reset();

public:
    void SetReg(int r, int v);
    void Callback(std::span<uint8_t> stream);

protected:
    static void BuildMixerTable();
    void RenderChunk(std::span<uint8_t> stream);
};


//////////////////////////////////////////////////////////////////////
