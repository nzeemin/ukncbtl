/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

/// \file Floppy.cpp
/// \brief Floppy controller and drives emulation
/// \details See defines in header file Emubase.h

#include "stdafx.h"
#include <algorithm>
#include <array>
#include <sys/stat.h>
#include <share.h>
#include "Emubase.h"


//////////////////////////////////////////////////////////////////////

// Mask of all flags stored in m_flags
const uint16_t FLOPPY_CMD_MASKSTORED =
    FLOPPY_CMD_CORRECTION250 | FLOPPY_CMD_CORRECTION500 | FLOPPY_CMD_SIDEUP | FLOPPY_CMD_DIR | FLOPPY_CMD_SKIPSYNC |
    FLOPPY_CMD_ENGINESTART;

static void EncodeTrackData(std::span<const uint8_t, 5120> source,
    std::span<uint8_t, FLOPPY_RAWTRACKSIZE> data,
    std::span<uint8_t, FLOPPY_RAWMARKERSIZE> marker, uint16_t track, uint16_t side);
static bool DecodeTrackData(std::span<const uint8_t, FLOPPY_RAWTRACKSIZE> raw,
    std::span<uint8_t, 5120> destination);

//////////////////////////////////////////////////////////////////////


CFloppyDrive::CFloppyDrive()
{
    fpFile = nullptr;
    okNetRT11Image = okReadOnly = false;
    datatrack = dataside = 0;
    dataptr = 0;
    track = side = 0;
    data.fill(0);
    marker.fill(0);
}

void CFloppyDrive::Reset()
{
    datatrack = dataside = 0;
    dataptr = 0;
}


//////////////////////////////////////////////////////////////////////


CFloppyController::CFloppyController()
{
    m_drive = m_side = m_track = 0;
    m_pDrive = m_drivedata.data();
    m_datareg = m_writereg = m_shiftreg = 0;
    m_writing = m_searchsync = m_writemarker = m_crccalculus = false;
    m_writeflag = m_shiftflag = m_shiftmarker = false;
    m_trackchanged = false;
    m_status = FLOPPY_STATUS_TRACK0 | FLOPPY_STATUS_WRITEPROTECT;
    m_flags = FLOPPY_CMD_CORRECTION500 | FLOPPY_CMD_SIDEUP | FLOPPY_CMD_DIR | FLOPPY_CMD_SKIPSYNC;
    m_okTrace = 0;
}

CFloppyController::~CFloppyController()
{
    for (int drive = 0; drive < 4; drive++)
    {
        DetachImage(drive);
    }
}

void CFloppyController::Reset()
{
    if (m_okTrace) DebugLog(_T("Floppy RESET\r\n"));

    if (!FlushChanges()) return;

    m_drive = m_side = m_track = 0;
    m_pDrive = m_drivedata.data();
    m_datareg = m_writereg = m_shiftreg = 0;
    m_writing = m_searchsync = m_writemarker = m_crccalculus = false;
    m_writeflag = m_shiftflag = false;
    m_trackchanged = false;
    m_status = (m_pDrive->okReadOnly) ? FLOPPY_STATUS_TRACK0 | FLOPPY_STATUS_WRITEPROTECT : FLOPPY_STATUS_TRACK0;
    m_flags = FLOPPY_CMD_CORRECTION500 | FLOPPY_CMD_SIDEUP | FLOPPY_CMD_DIR | FLOPPY_CMD_SKIPSYNC;

    PrepareTrack();
}

bool CFloppyController::AttachImage(int drive, LPCTSTR sFileName)
{
    ASSERT(sFileName != nullptr);
    if (sFileName == nullptr || !FlushChanges())
        return false;

    // If image attached - detach one first
    if (m_drivedata[drive].fpFile != nullptr)
    {
        DetachImage(drive);
        if (m_drivedata[drive].fpFile != nullptr)
            return false;
    }

    // Detect if this is a .dsk image or .rtd image, using the file extension
    m_drivedata[drive].okNetRT11Image = false;
    LPCTSTR sFileNameExt = _tcsrchr(sFileName, _T('.'));
    if (sFileNameExt != nullptr && _tcsicmp(sFileNameExt, _T(".rtd")) == 0)
        m_drivedata[drive].okNetRT11Image = true;

    // Open file
    m_drivedata[drive].okReadOnly = false;
    m_drivedata[drive].fpFile.reset(::_tfsopen(sFileName, _T("r+b"), _SH_DENYNO));
    if (m_drivedata[drive].fpFile == nullptr)
    {
        m_drivedata[drive].okReadOnly = true;
        m_drivedata[drive].fpFile.reset(::_tfsopen(sFileName, _T("rb"), _SH_DENYNO));
    }
    if (m_drivedata[drive].fpFile == nullptr)
        return false;

    m_side = m_track = m_drivedata[drive].datatrack = m_drivedata[drive].dataside = 0;
    m_drivedata[drive].dataptr = 0;
    m_datareg = m_writereg = m_shiftreg = 0;
    m_writing = m_searchsync = m_writemarker = m_crccalculus = false;
    m_writeflag = m_shiftflag = false;
    m_trackchanged = false;
    m_status = (m_pDrive->okReadOnly) ? FLOPPY_STATUS_TRACK0 | FLOPPY_STATUS_WRITEPROTECT : FLOPPY_STATUS_TRACK0;
    m_flags = FLOPPY_CMD_CORRECTION500 | FLOPPY_CMD_SIDEUP | FLOPPY_CMD_DIR | FLOPPY_CMD_SKIPSYNC;

    PrepareTrack();

    return true;
}

void CFloppyController::DetachImage(int drive)
{
    if (m_drivedata[drive].fpFile == nullptr) return;

    if (drive == m_drive && !FlushChanges()) return;

    m_drivedata[drive].fpFile.reset();
    m_drivedata[drive].okNetRT11Image = m_drivedata[drive].okReadOnly = false;
    m_drivedata[drive].Reset();
}

//////////////////////////////////////////////////////////////////////


uint16_t CFloppyController::GetState()
{
    if (m_track == 0)
        m_status |= FLOPPY_STATUS_TRACK0;
    else
        m_status &= ~FLOPPY_STATUS_TRACK0;
    if (m_pDrive->dataptr < FLOPPY_INDEXLENGTH)
        m_status |= FLOPPY_STATUS_INDEXMARK;
    else
        m_status &= ~FLOPPY_STATUS_INDEXMARK;

    uint16_t res = m_status;

    if (m_drivedata[m_drive].fpFile == nullptr)
        res |= FLOPPY_STATUS_MOREDATA;

//    if (res & FLOPPY_STATUS_MOREDATA)
//    {
//        TCHAR oct2[7];  PrintOctalValue(oct2, res);
//        DebugLogFormat(_T("Floppy GET STATE {}\r\n"), oct2);
//    }

    return res;
}

void CFloppyController::SetCommand(uint16_t cmd)
{
    if (m_okTrace) DebugLogFormat(_T("Floppy COMMAND {:06o}\r\n"), cmd);

    bool okPrepareTrack = false;  // Is it needed to load the track into the buffer

    // Check if the current drive was changed or not
    uint16_t newdrive = (cmd & 3) ^ 3;
    const bool driveChanges = (cmd & 02000) != 0 && m_drive != newdrive;
    const bool sideChanges = ((cmd & FLOPPY_CMD_SIDEUP) != 0) != (m_side != 0);
    const bool trackChanges = (cmd & FLOPPY_CMD_STEP) != 0 &&
        (((cmd & FLOPPY_CMD_DIR) != 0 && m_track < 79) ||
         ((cmd & FLOPPY_CMD_DIR) == 0 && m_track >= 1));
    if ((driveChanges || sideChanges || trackChanges) && !FlushChanges())
        return;
    if ((cmd & 02000) != 0 && m_drive != newdrive)
    {
        DebugLogFormat(_T("Floppy DRIVE {}\r\n"), newdrive);

        m_drive = newdrive;
        m_pDrive = &m_drivedata[m_drive];
        if (m_pDrive->fpFile == nullptr || m_pDrive->okReadOnly)
            m_status |= FLOPPY_STATUS_WRITEPROTECT;
        else
            m_status &= ~FLOPPY_STATUS_WRITEPROTECT;
        okPrepareTrack = true;
    }
    cmd &= ~3;  // Remove the info about the current drive

    // Copy new flags to m_flags
    m_flags &= ~FLOPPY_CMD_MASKSTORED;
    m_flags |= cmd & FLOPPY_CMD_MASKSTORED;

    // Check if the side was changed
    if (m_flags & FLOPPY_CMD_SIDEUP)  // Side selection: 0 - down, 1 - up
    {
        if (m_side == 0) { m_side = 1;  okPrepareTrack = true; }
    }
    else
    {
        if (m_side == 1) { m_side = 0;  okPrepareTrack = true; }
    }

    if (cmd & FLOPPY_CMD_STEP)  // Move head for one track to center or from center
    {
        if (m_okTrace) DebugLog(_T("Floppy STEP\r\n"));

        m_side = (m_flags & FLOPPY_CMD_SIDEUP) ? 1 : 0; // DO WE NEED IT HERE?

        if (m_flags & FLOPPY_CMD_DIR)
        {
            if (m_track < 79) { m_track++;  okPrepareTrack = true; }
        }
        else
        {
            if (m_track >= 1) { m_track--;  okPrepareTrack = true; }
        }
    }
    if (okPrepareTrack)
    {
        PrepareTrack();

//    	DebugLogFormat(_T("Floppy DRIVE {} TR {} SD {}\r\n"), m_drive, m_track, m_side);
    }

    if (cmd & FLOPPY_CMD_SEARCHSYNC) // Search for marker
    {
//        DebugLog(_T("Floppy SEARCHSYNC\r\n"));

        m_flags &= ~FLOPPY_CMD_SEARCHSYNC;
        m_searchsync = true;
        m_crccalculus = true;
        m_status &= ~(FLOPPY_STATUS_CHECKSUMOK | FLOPPY_STATUS_MOREDATA); // a pending word must not block the search
    }

    if (m_writing && (cmd & FLOPPY_CMD_SKIPSYNC))  // Writing marker
    {
//        DebugLog(_T("Floppy MARKER\r\n"));

        m_writemarker = true;
        m_status &= ~FLOPPY_STATUS_CHECKSUMOK;
    }
}

uint16_t CFloppyController::GetData()
{
    if (m_okTrace) DebugLogFormat(_T("Floppy READ\t\t{:04x}\r\n"), m_datareg);

    m_status &= ~FLOPPY_STATUS_MOREDATA;
    m_writing = m_searchsync = false;
    m_writeflag = m_shiftflag = false;

    return m_datareg;
}

void CFloppyController::WriteData(uint16_t data)
{
//        DebugLogFormat(_T("Floppy WRITE\t\t{:04x}\r\n"), data);

    m_writing = true;  // Switch to write mode if not yet
    m_searchsync = false;

    if (!m_writeflag && !m_shiftflag)  // Both registers are empty
    {
        m_shiftreg = data;
        m_shiftflag = true;
        m_status |= FLOPPY_STATUS_MOREDATA;
    }
    else if (!m_writeflag && m_shiftflag)  // Write register is empty
    {
        m_writereg = data;
        m_writeflag = true;
        m_status &= ~FLOPPY_STATUS_MOREDATA;
    }
    else if (m_writeflag && !m_shiftflag)  // Shift register is empty
    {
        m_shiftreg = m_writereg;
        m_shiftflag = true;
        m_writereg = data;
        m_status &= ~FLOPPY_STATUS_MOREDATA;
    }
    else  // Both registers are not empty
    {
        m_writereg = data;
    }
}

void CFloppyController::Periodic()
{
    //if (!IsEngineOn()) return;  // Rotate diskettes only if the motor is on

    // Rotating all the disks at once
    for (int drive = 0; drive < 4; drive++)
    {
        m_drivedata[drive].dataptr += 2;
        if (m_drivedata[drive].dataptr >= FLOPPY_RAWTRACKSIZE)
            m_drivedata[drive].dataptr = 0;
    }

    // Then process reading/writing on the current drive
    if (!IsAttached(m_drive)) return;

    if (!m_writing)  // Read mode
    {
        m_datareg = (m_pDrive->data[m_pDrive->dataptr] << 8) | m_pDrive->data[m_pDrive->dataptr + 1];
        if (m_status & FLOPPY_STATUS_MOREDATA)
        {
            if (m_crccalculus)  // Stop CRC calculation
            {
                m_crccalculus = false;
                //TODO: Compare calculated CRC to m_datareg
                m_status |= FLOPPY_STATUS_CHECKSUMOK;
            }
        }
        else
        {
            if (m_searchsync)  // Search for marker
            {
                if (m_pDrive->marker[m_pDrive->dataptr / 2] != 0)  // Marker found
                {
                    m_status |= FLOPPY_STATUS_MOREDATA;
                    m_searchsync = false;
                }
            }
            else  // Just read
                m_status |= FLOPPY_STATUS_MOREDATA;
        }
    }
    else  // Write mode
    {
        if (m_shiftflag)
        {
            if (!(m_status & FLOPPY_STATUS_WRITEPROTECT))
            {
                m_pDrive->data[m_pDrive->dataptr] = static_cast<uint8_t>(m_shiftreg);
                m_pDrive->data[m_pDrive->dataptr + 1] = static_cast<uint8_t>(m_shiftreg >> 8);
                m_trackchanged = true;
                m_pDrive->marker[m_pDrive->dataptr / 2] = m_shiftmarker ? 1 : 0;
                if (m_shiftmarker)
                    m_crccalculus = true;  // Start CRC calculation
            }
            m_shiftflag = false;
            m_shiftmarker = false;

            if (m_writeflag)
            {
                m_shiftreg = m_writereg;
                m_shiftflag = true;
                m_writeflag = false;
                m_shiftmarker = m_writemarker;
                m_writemarker = false;
                m_status |= FLOPPY_STATUS_MOREDATA;
            }
            else
            {
                if (m_crccalculus)  // Stop CRC calclation
                {
                    m_shiftreg = 0x4444;  //STUB
                    m_shiftflag = false; //Should be true, but temporarily disabled
                    m_shiftmarker = false;
                    m_crccalculus = false;
                    m_status |= FLOPPY_STATUS_CHECKSUMOK;
                }
            }
        }
    }
}

// Read track data from file and fill m_data
void CFloppyController::PrepareTrack()
{
    if (!FlushChanges()) return;

    if (m_okTrace) DebugLogFormat(_T("Floppy PREP  {} TR {} SD {}\r\n"), m_drive, m_track, m_side);

    //TCHAR buffer[512];

    m_trackchanged = false;
    m_status |= FLOPPY_STATUS_MOREDATA;
    //NOTE: Not changing m_pDrive->dataptr
    m_pDrive->datatrack = m_track;
    m_pDrive->dataside = m_side;

    // Track has 10 sectors, 512 bytes each; offset of the file is === ((Track<<1)+SIDE)*5120
    long foffset = ((m_track * 2) + (m_side)) * 5120;
    if (m_pDrive->okNetRT11Image) foffset += 256;  // Skip .RTD image header
    //DebugPrintFormat(_T("floppy file offset {}  for trk {} side {}\r\n"), foffset, m_track, m_side);

    std::array<uint8_t, 5120> data{};

    if (m_pDrive->fpFile != nullptr)
    {
        bool readFailed = ::fseek(m_pDrive->fpFile.get(), foffset, SEEK_SET) != 0;
        if (!readFailed)
        {
            (void)::fread(data.data(), 1, data.size(), m_pDrive->fpFile.get());
            readFailed = ::ferror(m_pDrive->fpFile.get()) != 0;
        }
        if (readFailed)
        {
            ::clearerr(m_pDrive->fpFile.get());
            m_pDrive->okReadOnly = true;
            m_status |= FLOPPY_STATUS_WRITEPROTECT;
            m_status &= ~FLOPPY_STATUS_CHECKSUMOK;
            DebugLog(_T("Floppy image read failed; the drive is write-protected.\r\n"));
        }
    }

    // Fill m_data array and m_marker array with marked data
    EncodeTrackData(data, m_pDrive->data, m_pDrive->marker, m_track, m_side);

}

bool CFloppyController::FlushChanges()
{
    if (!IsAttached(m_drive) || !m_trackchanged) return true;

    if (m_okTrace) DebugLogFormat(_T("Floppy FLUSH {} TR {} SD {}\r\n"), m_drive, m_pDrive->datatrack, m_pDrive->dataside);

    // Decode track data from m_data
    std::array<uint8_t, 5120> data{};
    bool decoded = DecodeTrackData(m_pDrive->data, data);

    if (decoded)  // Write to the file only if the track was correctly decoded from raw data
    {
        // Track has 10 sectors, 512 bytes each; offset of the file is === ((Track<<1)+SIDE)*5120
        long foffset = ((m_pDrive->datatrack * 2) + (m_pDrive->dataside)) * 5120;
        if (m_pDrive->okNetRT11Image) foffset += 256;  // Skip .RTD image header

        // Check file length
        if (::fseek(m_pDrive->fpFile.get(), 0, SEEK_END) != 0)
            goto WriteError;
        long fileSize = ::ftell(m_pDrive->fpFile.get());
        if (fileSize < 0)
            goto WriteError;
        uint32_t currentFileSize = static_cast<uint32_t>(fileSize);
        while (currentFileSize < (uint32_t)(foffset + 5120))
        {
            std::array<uint8_t, 512> datafill{};
            uint32_t bytesToWrite = ((uint32_t)(foffset + 5120) - currentFileSize) % 512;
            if (bytesToWrite == 0) bytesToWrite = 512;
            if (::fwrite(datafill.data(), 1, bytesToWrite, m_pDrive->fpFile.get()) != bytesToWrite)
                goto WriteError;
            currentFileSize += bytesToWrite;
        }

        // Save data into the file
        if (::fseek(m_pDrive->fpFile.get(), foffset, SEEK_SET) != 0 ||
            ::fwrite(data.data(), 1, data.size(), m_pDrive->fpFile.get()) != data.size() ||
            ::fflush(m_pDrive->fpFile.get()) != 0)
            goto WriteError;
    }
    else
    {
        m_status &= ~FLOPPY_STATUS_CHECKSUMOK;
        DebugLog(_T("Floppy track cannot be represented in the disk image; changes retained.\r\n"));
        return false;
    }

    m_trackchanged = false;
    if (!m_pDrive->okReadOnly)
        m_status &= ~FLOPPY_STATUS_WRITEPROTECT;
    return true;

WriteError:
    ::clearerr(m_pDrive->fpFile.get());
    m_status |= FLOPPY_STATUS_WRITEPROTECT;
    m_status &= ~FLOPPY_STATUS_CHECKSUMOK;
    DebugLog(_T("Floppy image write failed; track changes retained.\r\n"));
    return false;

    ////DEBUG: Save raw m_data/m_marker into rawdata.bin
    //HANDLE hRawFile = CreateFile(_T("rawdata.bin"),
    //            GENERIC_WRITE, FILE_SHARE_READ, nullptr,
    //            CREATE_ALWAYS, 0, nullptr);
}


//////////////////////////////////////////////////////////////////////


// Fill data array and marker array with marked data
static void EncodeTrackData(std::span<const uint8_t, 5120> source,
    std::span<uint8_t, FLOPPY_RAWTRACKSIZE> data,
    std::span<uint8_t, FLOPPY_RAWMARKERSIZE> marker, uint16_t track, uint16_t side)
{
    std::ranges::fill(data, 0);
    std::ranges::fill(marker, 0);
    uint32_t count;
    int ptr = 0;
    int gap = 34;
    for (int sect = 0; sect < 10; sect++)
    {
        // GAP
        for (count = 0; count < (uint32_t) gap; count++) data[ptr++] = 0x4e;
        // sector header
        for (count = 0; count < 12; count++) data[ptr++] = 0x00;
        // marker
        marker[ptr / 2] = 1;  // ID marker; start CRC calculus
        data[ptr++] = 0xa1;  data[ptr++] = 0xa1;  data[ptr++] = 0xa1;
        data[ptr++] = 0xfe;

        data[ptr++] = (uint8_t) track;  data[ptr++] = (side != 0);
        data[ptr++] = (uint8_t)sect + 1;  data[ptr++] = 2; // Assume 512 bytes per sector;
        // crc
        //TODO: Calculate CRC
        data[ptr++] = 0x12;  data[ptr++] = 0x34;  // CRC stub

        // sync
        for (count = 0; count < 24; count++) data[ptr++] = 0x4e;
        // data header
        for (count = 0; count < 12; count++) data[ptr++] = 0x00;
        // marker
        marker[ptr / 2] = 1;  // Data marker; start CRC calculus
        data[ptr++] = 0xa1;  data[ptr++] = 0xa1;  data[ptr++] = 0xa1;
        data[ptr++] = 0xfb;
        // data
        for (count = 0; count < 512; count++)
            data[ptr++] = source[(sect * 512) + count];
        // crc
        //TODO: Calculate CRC
        data[ptr++] = 0x43;  data[ptr++] = 0x21;  // CRC stub

        gap = 38;
    }
    // fill GAP4B to the end of the track
    while (ptr < FLOPPY_RAWTRACKSIZE) data[ptr++] = 0x4e;
}

// Decode a track into the sector order used by the disk image.
static bool DecodeTrackData(std::span<const uint8_t, FLOPPY_RAWTRACKSIZE> raw,
    std::span<uint8_t, 5120> destination)
{
    constexpr size_t sectorSize = 512;
    constexpr size_t sectorCount = 10;
    std::array<bool, sectorCount> seen{};
    size_t position = 0;
    size_t decoded = 0;

    const auto skip = [&raw, &position](uint8_t byte)
    {
        while (position < raw.size() && raw[position] == byte)
            ++position;
    };
    const auto marker = [&raw, &position](uint8_t value)
    {
        for (int i = 0; i < 3 && position < raw.size() && raw[position] == 0xa1; ++i)
            ++position;
        return position < raw.size() && raw[position++] == value;
    };

    for (;;)
    {
        skip(0x4e);  // GAP1 or GAP3
        if (position == raw.size())
            break;
        skip(0x00);
        if (!marker(0xfe) || raw.size() - position < 6)
            return false;

        const auto sector = raw[position + 2];
        const auto sizeCode = raw[position + 3];
        position += 6;  // ID fields and CRC
        if (sector == 0 || sector > sectorCount || sizeCode != 2 || seen[sector - 1])
            return false;

        skip(0x4e);  // GAP2
        skip(0x00);
        if (!marker(0xfb) || raw.size() - position < sectorSize + 2)
            return false;

        std::ranges::copy_n(raw.begin() + position, sectorSize,
            destination.begin() + (sector - 1) * sectorSize);
        position += sectorSize + 2;  // Data and CRC
        seen[sector - 1] = true;
        ++decoded;
    }

    return decoded == sectorCount;
}


//////////////////////////////////////////////////////////////////////

