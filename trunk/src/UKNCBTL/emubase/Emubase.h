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


//////////////////////////////////////////////////////////////////////
// Disassembler

/// \brief Disassemble one instruction of KM1801VM2 processor
/// \param[in]  pMemory Memory image (we read only words of the instruction)
/// \param[in]  addr    Address of the instruction
/// \param[out] sInstr  Instruction mnemonics buffer - at least 8 characters
/// \param[out] sArg    Instruction arguments buffer - at least 32 characters
/// \return  Number of words in the instruction
int DisassembleInstruction(WORD* pMemory, WORD addr, TCHAR* sInstr, TCHAR* sArg);


//////////////////////////////////////////////////////////////////////
// CFloppy

#define SAMPLERATE                      22050

#define FLOPPY_FSM_IDLE                 0

#define FLOPPY_CMD_CORRECTION250             04
#define FLOPPY_CMD_CORRECTION500            010
#define FLOPPY_CMD_ENGINESTART              020  ///< Engine on/off
#define FLOPPY_CMD_SIDEUP                   040  ///< Side: 1 -- upper head, 0 -- lower head
#define FLOPPY_CMD_DIR                     0100  ///< Direction: 0 -- to center (towards trk0), 1 -- from center (towards trk80)
#define FLOPPY_CMD_STEP                    0200  ///< Step / ready
#define FLOPPY_CMD_SEARCHSYNC              0400  ///< Search sync
#define FLOPPY_CMD_SKIPSYNC               01000  ///< Skip sync

#define FLOPPY_STATUS_TRACK0                 01  ///< Track 0 flag
#define FLOPPY_STATUS_RDY                    02  ///< Ready status
#define FLOPPY_STATUS_WRITEPROTECT           04  ///< Write protect
#define FLOPPY_STATUS_MOREDATA             0200  ///< Need more data flag
#define FLOPPY_STATUS_CHECKSUMOK         040000  ///< Checksum verified OK
#define FLOPPY_STATUS_INDEXMARK         0100000  ///< Index flag, indicates the beginning of track

#define FLOPPY_RAWTRACKSIZE             6250     ///< Raw track size, bytes
#define FLOPPY_RAWMARKERSIZE            (FLOPPY_RAWTRACKSIZE / 2)
#define FLOPPY_INDEXLENGTH              150      ///< Length of index hole, in bytes of raw track image

/// \brief Floppy drive (one of four drives in the floppy controller)
/// \sa CFloppyController
struct CFloppyDrive
{
    FILE* fpFile;       ///< File pointer of the disk image file
    BOOL okNetRT11Image;  ///< TRUE - .rtd image, FALSE - .dsk image
    BOOL okReadOnly;    ///< Write protection flag
    WORD track;         ///< Track number: from 0 to 79
    WORD side;          ///< Disk side: 0 or 1
    WORD dataptr;       ///< Data offset within m_data - "head" position
    BYTE data[FLOPPY_RAWTRACKSIZE];  ///< Raw track image for the current track
    BYTE marker[FLOPPY_RAWMARKERSIZE];  ///< Marker positions
    WORD datatrack;     ///< Track number of data in m_data array
    WORD dataside;      ///< Disk side of data in m_data array

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
    WORD m_drive;       ///< Current drive number: from 0 to 3
    CFloppyDrive* m_pDrive;  ///< Current drive
    WORD m_track;       ///< Track number: from 0 to 79
    WORD m_side;        ///< Disk side: 0 or 1
    WORD m_status;      ///< See FLOPPY_STATUS_XXX defines
    WORD m_flags;       ///< See FLOPPY_CMD_XXX defines
    WORD m_datareg;     ///< Read mode data register
    WORD m_writereg;    ///< Write mode data register
    BOOL m_writeflag;   ///< Write mode data register has data
    BOOL m_writemarker; ///< Write marker in m_marker
    WORD m_shiftreg;    ///< Write mode shift register
    BOOL m_shiftflag;   ///< Write mode shift register has data
    BOOL m_shiftmarker; ///< Write marker in m_marker
    BOOL m_writing;     ///< TRUE = write mode, FALSE = read mode
    BOOL m_searchsync;  ///< Read sub-mode: TRUE = search for sync, FALSE = just read
    BOOL m_crccalculus; ///< TRUE = CRC is calculated now
    BOOL m_trackchanged;  ///< TRUE = m_data was changed - need to save it into the file

public:
    CFloppyController();
    ~CFloppyController();
    void Reset();       ///< Reset the device

public:
    /// \brief Attach the image to the drive -- insert disk
    BOOL AttachImage(int drive, LPCTSTR sFileName);
    /// \brief Detach image from the drive -- remove disk
    void DetachImage(int drive);
    /// \brief Check if the drive has an image attached
    BOOL IsAttached(int drive) const { return (m_drivedata[drive].fpFile != NULL); }
    /// \brief Check if the drive's attached image is read-only
    BOOL IsReadOnly(int drive) const { return m_drivedata[drive].okReadOnly; }
    /// \brief Check if floppy engine now rotates
    BOOL IsEngineOn() const { return (m_flags & FLOPPY_CMD_ENGINESTART) != 0; }
    WORD GetData(void);         ///< Reading port 177132 -- data
    WORD GetState(void);        ///< Reading port 177130 -- device status
    void SetCommand(WORD cmd);  ///< Writing to port 177130 -- commands
    void WriteData(WORD Data);  ///< Writing to port 177132 -- data
    void Periodic();            ///< Rotate disk; call it each 64 us -- 15625 times per second

private:
    void PrepareTrack();
    void FlushChanges();  ///< If current track was changed - save it

};


//////////////////////////////////////////////////////////////////////
// CHardDrive

#define IDE_DISK_SECTOR_SIZE      512

/// \brief UKNC IDE hard drive
class CHardDrive
{
protected:
    FILE*   m_fpFile;           ///< File pointer for the attached HDD image
    BOOL    m_okReadOnly;       ///< Flag indicating that the HDD image file is read-only
    BOOL    m_okInverted;       ///< Flag indicating that the HDD image has inverted bits
    BYTE    m_status;           ///< IDE status register, see IDE_STATUS_XXX constants
    BYTE    m_error;            ///< IDE error register, see IDE_ERROR_XXX constants
    BYTE    m_command;          ///< Current IDE command, see IDE_COMMAND_XXX constants
    int     m_numcylinders;     ///< Cylinder count
    int     m_numheads;         ///< Head count
    int     m_numsectors;       ///< Sectors per track
    int     m_curcylinder;      ///< Current cylinder number
    int     m_curhead;          ///< Current head number
    int     m_cursector;        ///< Current sector number
    int     m_curheadreg;       ///< Current head number
    int     m_sectorcount;      ///< Sector counter for read/write operations
    BYTE    m_buffer[IDE_DISK_SECTOR_SIZE];  ///< Sector data buffer
    int     m_bufferoffset;     ///< Current offset within sector: 0..511
    int     m_timeoutcount;     ///< Timeout counter to wait for the next event
    int     m_timeoutevent;     ///< Current stage of operation, see TimeoutEvent enum

public:
    CHardDrive();
    ~CHardDrive();
    /// \brief Reset the device.
    void Reset();
    /// \brief Attach HDD image file to the device
    BOOL AttachImage(LPCTSTR sFileName);
    /// \brief Detach HDD image file from the device
    void DetachImage();
    /// \brief Check if the attached hard drive image is read-only
    BOOL IsReadOnly() const { return m_okReadOnly; }

public:
    /// \brief Read word from the device port
    WORD ReadPort(WORD port);
    /// \brief Write word th the device port
    void WritePort(WORD port, WORD data);
    /// \brief Rotate disk
    void Periodic();

private:
    DWORD CalculateOffset() const;  ///< Calculate sector offset in the HDD image
    void HandleCommand(BYTE command);  ///< Handle the IDE command
    void ReadNextSector();
    void ReadSectorDone();
    void WriteSectorDone();
    void NextSector();          ///< Advance to the next sector, CHS-based
    void ContinueRead();
    void ContinueWrite();
    void IdentifyDrive();       ///< Prepare m_buffer for the IDENTIFY DRIVE command
};


//////////////////////////////////////////////////////////////////////
