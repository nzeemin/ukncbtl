/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

/// \file Hard.cpp
/// \brief Hard disk drive emulation.
/// \details See defines in header file Emubase.h

#include "stdafx.h"
#include <sys/stat.h>
#include "Emubase.h"


//////////////////////////////////////////////////////////////////////
// Constants

#define TIME_PER_SECTOR                 (IDE_DISK_SECTOR_SIZE / 2)

#define IDE_PORT_DATA                   0x1f0
#define IDE_PORT_ERROR                  0x1f1
#define IDE_PORT_SECTOR_COUNT           0x1f2
#define IDE_PORT_SECTOR_NUMBER          0x1f3
#define IDE_PORT_CYLINDER_LSB           0x1f4
#define IDE_PORT_CYLINDER_MSB           0x1f5
#define IDE_PORT_HEAD_NUMBER            0x1f6
#define IDE_PORT_STATUS_COMMAND         0x1f7

#define IDE_STATUS_ERROR                0x01
#define IDE_STATUS_HIT_INDEX            0x02
#define IDE_STATUS_BUFFER_READY         0x08
#define IDE_STATUS_SEEK_COMPLETE        0x10
#define IDE_STATUS_DRIVE_READY          0x40
#define IDE_STATUS_BUSY                 0x80

#define IDE_COMMAND_READ_MULTIPLE       0x20
#define IDE_COMMAND_READ_MULTIPLE1      0x21
#define IDE_COMMAND_SET_CONFIG          0x91
#define IDE_COMMAND_WRITE_MULTIPLE      0x30
#define IDE_COMMAND_WRITE_MULTIPLE1     0x31
#define IDE_COMMAND_IDENTIFY            0xec

#define IDE_ERROR_NONE                  0x00
#define IDE_ERROR_DEFAULT               0x01
#define IDE_ERROR_UNKNOWN_COMMAND       0x04
#define IDE_ERROR_BAD_LOCATION          0x10
#define IDE_ERROR_BAD_SECTOR            0x80

enum TimeoutEvent
{
    TIMEEVT_NONE = 0,
    TIMEEVT_RESET_DONE = 1,
    TIMEEVT_READ_SECTOR_DONE = 2,
    TIMEEVT_WRITE_SECTOR_DONE = 3,
};

//////////////////////////////////////////////////////////////////////

// Inverts 512 bytes in the buffer
static void InvertBuffer(void* buffer)
{
    uint32_t* p = (uint32_t*) buffer;
    for (int i = 0; i < 128; i++)
    {
        *p = ~(*p);
        p++;
    }
}

//////////////////////////////////////////////////////////////////////


CHardDrive::CHardDrive()
{
    m_fpFile = nullptr;

    m_status = IDE_STATUS_BUSY;
    m_error = IDE_ERROR_NONE;
    m_command = 0;
    m_timeoutcount = m_timeoutevent = 0;
    m_sectorcount = 0;

    m_numsectors = m_numheads = m_numcylinders = 0;
    m_curcylinder = m_curhead = m_curheadreg = m_cursector = m_bufferoffset = 0;
    memset(m_buffer, 0, IDE_DISK_SECTOR_SIZE);

    m_okInverted = false;
    m_okReadOnly = false;
}

CHardDrive::~CHardDrive()
{
    DetachImage();
}

void CHardDrive::Reset()
{
    //DebugLog(_T("HDD Reset\r\n"));

    m_status = IDE_STATUS_BUSY;
    m_error = IDE_ERROR_NONE;
    m_command = 0;
    m_timeoutcount = 2;
    m_timeoutevent = TIMEEVT_RESET_DONE;
}

bool CHardDrive::AttachImage(LPCTSTR sFileName)
{
    ASSERT(sFileName != nullptr);

    // Open file
    m_okReadOnly = false;
    m_fpFile = ::_tfopen(sFileName, _T("r+b"));
    if (m_fpFile == nullptr)
    {
        m_okReadOnly = true;
        m_fpFile = ::_tfopen(sFileName, _T("rb"));
    }
    if (m_fpFile == nullptr)
        return false;

    // Check file size
    ::fseek(m_fpFile, 0, SEEK_END);
    uint32_t dwFileSize = ::ftell(m_fpFile);
    ::fseek(m_fpFile, 0, SEEK_SET);
    if (dwFileSize % 512 != 0)
        return false;

    // Read first sector
    size_t dwBytesRead = ::fread(m_buffer, 1, 512, m_fpFile);
    if (dwBytesRead != 512)
        return false;

    // Detect hard disk type
    const uint16_t * pwHardBuffer = (const uint16_t*)m_buffer;
    if (pwHardBuffer[0] == 0x54A9 && pwHardBuffer[1] == 0xFFEF && pwHardBuffer[2] == 0xFEFF ||
        pwHardBuffer[0] == 0xAB56 && pwHardBuffer[1] == 0x0010 && pwHardBuffer[2] == 0x0100)  // HD type
    {
        // Autodetect inverted image
        m_okInverted = (pwHardBuffer[0] == 0xAB56);
        // Invert the buffer if needed
        if (m_okInverted)
            InvertBuffer(m_buffer);

        // Calculate geometry
        m_numsectors = pwHardBuffer[4];
        if (m_numsectors == 0)
            return false;  // Geometry params are not defined
        m_numheads = (uint8_t)(pwHardBuffer[5] / m_numsectors);
        if (m_numheads == 0)
            return false;  // Geometry params are not defined
        m_numcylinders = dwFileSize / 512 / m_numsectors / m_numheads;
        if (m_numcylinders == 0)
            return false;
    }
    else  // ID or WD type
    {
        // Autodetect inverted image
        uint8_t test = 0xff;
        for (int i = 0x1f0; i <= 0x1fb; i++)
            test &= m_buffer[i];
        m_okInverted = (test == 0xff);
        // Invert the buffer if needed
        if (m_okInverted)
            InvertBuffer(m_buffer);

        // Calculate geometry
        m_numsectors = *(m_buffer + 0);
        m_numheads = *(m_buffer + 1);
        if (m_numsectors == 0 || m_numheads == 0)
            return false;  // Geometry params are not defined
        m_numcylinders = dwFileSize / 512 / m_numsectors / m_numheads;
        if (m_numcylinders == 0 || m_numcylinders > 1024)
            return false;
    }

    m_curcylinder = m_curhead = m_curheadreg = m_cursector = m_bufferoffset = 0;

    m_status = IDE_STATUS_BUSY;
    m_error = IDE_ERROR_NONE;

    return true;
}

void CHardDrive::DetachImage()
{
    if (m_fpFile == nullptr) return;

    //FlushChanges();

    ::fclose(m_fpFile);
    m_fpFile = nullptr;
}

uint16_t CHardDrive::ReadPort(uint16_t port)
{
    ASSERT(port >= 0x1F0 && port <= 0x1F7);

    uint16_t data = 0;
    switch (port)
    {
    case IDE_PORT_DATA:
        if (m_status & IDE_STATUS_BUFFER_READY)
        {
            data = *((uint16_t*)(m_buffer + m_bufferoffset));
            if (!m_okInverted)
                data = ~data;  // Image stored non-inverted, but QBUS inverts the bits
            m_bufferoffset += 2;

            //if (m_bufferoffset == 2)
            //    DebugLogFormat(_T("HDD Read sector start %04x\r\n"), data);

            if (m_bufferoffset >= IDE_DISK_SECTOR_SIZE)
            {
                //DebugLog(_T("HDD Read sector complete\r\n"));

                ContinueRead();
            }
        }
        break;
    case IDE_PORT_ERROR:
        data = 0xff00 | m_error;
        break;
    case IDE_PORT_SECTOR_COUNT:
        data = 0xff00 | (uint16_t)m_sectorcount;
        break;
    case IDE_PORT_SECTOR_NUMBER:
        data = 0xff00 | (uint16_t)m_cursector;
        break;
    case IDE_PORT_CYLINDER_LSB:
        data = 0xff00 | (uint16_t)(m_curcylinder & 0xff);
        break;
    case IDE_PORT_CYLINDER_MSB:
        data = 0xff00 | (uint16_t)((m_curcylinder >> 8) & 0xff);
        break;
    case IDE_PORT_HEAD_NUMBER:
        data = 0xff00 | (uint16_t)m_curheadreg;
        break;
    case IDE_PORT_STATUS_COMMAND:
        data = 0xff00 | m_status;
        break;
    }

    //DebugPrintFormat(_T("HDD Read  %x     0x%04x\r\n"), port, data);
    return data;
}

void CHardDrive::WritePort(uint16_t port, uint16_t data)
{
    ASSERT(port >= 0x1F0 && port <= 0x1F7);

//    DebugPrintFormat(_T("HDD Write %x <-- 0x%04x\r\n"), port, data);

    switch (port)
    {
    case IDE_PORT_DATA:
        if (m_status & IDE_STATUS_BUFFER_READY)
        {
            if (!m_okInverted)
                data = ~data;  // Image stored non-inverted, but QBUS inverts the bits
            *((uint16_t*)(m_buffer + m_bufferoffset)) = data;
            m_bufferoffset += 2;

            //if (m_bufferoffset == 2)
            //    DebugLogFormat(_T("HDD Write sector start %04x\r\n"), data);

            if (m_bufferoffset >= IDE_DISK_SECTOR_SIZE)
            {
                m_status &= ~IDE_STATUS_BUFFER_READY;

                ContinueWrite();
            }
        }
        break;
    case IDE_PORT_ERROR:
        // Writing precompensation value -- ignore
        break;
    case IDE_PORT_SECTOR_COUNT:
        data &= 0x0ff;
        m_sectorcount = (data == 0) ? 256 : data;
        break;
    case IDE_PORT_SECTOR_NUMBER:
        data &= 0x0ff;
        m_cursector = data;
        break;
    case IDE_PORT_CYLINDER_LSB:
        data &= 0x0ff;
        m_curcylinder = (m_curcylinder & 0xff00) | (data & 0xff);
        break;
    case IDE_PORT_CYLINDER_MSB:
        data &= 0x0ff;
        m_curcylinder = (m_curcylinder & 0x00ff) | ((data & 0xff) << 8);
        break;
    case IDE_PORT_HEAD_NUMBER:
        data &= 0x0ff;
        m_curhead = data & 0x0f;
        m_curheadreg = data;
        break;
    case IDE_PORT_STATUS_COMMAND:
        data &= 0x0ff;
        HandleCommand((uint8_t)data);
        break;
    }
}

// Called from CMotherboard::SystemFrame() every tick
void CHardDrive::Periodic()
{
    if (m_timeoutcount > 0)
    {
        m_timeoutcount--;
        if (m_timeoutcount == 0)
        {
            int evt = m_timeoutevent;
            m_timeoutevent = TIMEEVT_NONE;
            switch (evt)
            {
            case TIMEEVT_RESET_DONE:
                m_status &= ~IDE_STATUS_BUSY;
                m_status |= IDE_STATUS_DRIVE_READY | IDE_STATUS_SEEK_COMPLETE;
                break;
            case TIMEEVT_READ_SECTOR_DONE:
                ReadSectorDone();
                break;
            case TIMEEVT_WRITE_SECTOR_DONE:
                WriteSectorDone();
                break;
            }
        }
    }
}

void CHardDrive::HandleCommand(uint8_t command)
{
    m_command = command;
    switch (command)
    {
    case IDE_COMMAND_READ_MULTIPLE:
    case IDE_COMMAND_READ_MULTIPLE1:
        //DebugLogFormat(_T("HDD COMMAND %02x (READ MULT): C=%d, H=%d, SN=%d, SC=%d\r\n"),
        //        command, m_curcylinder, m_curhead, m_cursector, m_sectorcount);

        m_status |= IDE_STATUS_BUSY;
        m_status &= ~IDE_STATUS_BUFFER_READY;

        m_timeoutcount = TIME_PER_SECTOR * 3;  // Timeout while seek for track
        m_timeoutevent = TIMEEVT_READ_SECTOR_DONE;
        break;

    case IDE_COMMAND_SET_CONFIG:
        //DebugLogFormat(_T("HDD COMMAND %02x (SET CONFIG): H=%d, SC=%d\r\n"),
        //        command, m_curhead, m_sectorcount);

        m_numsectors = m_sectorcount;
        m_numheads = m_curhead + 1;
        break;

    case IDE_COMMAND_WRITE_MULTIPLE:
    case IDE_COMMAND_WRITE_MULTIPLE1:
        //DebugLogFormat(_T("HDD COMMAND %02x (WRITE MULT): C=%d, H=%d, SN=%d, SC=%d\r\n"),
        //        command, m_curcylinder, m_curhead, m_cursector, m_sectorcount);

        m_bufferoffset = 0;
        m_status |= IDE_STATUS_BUFFER_READY;
        break;

    case IDE_COMMAND_IDENTIFY:
        //DebugLogFormat(_T("HDD COMMAND %02x (IDENTIFY)\r\n"), command);

        IdentifyDrive();  // Prepare the buffer
        m_bufferoffset = 0;
        m_sectorcount = 1;
        m_status |= IDE_STATUS_BUFFER_READY | IDE_STATUS_SEEK_COMPLETE | IDE_STATUS_DRIVE_READY;
        m_status &= ~IDE_STATUS_BUSY;
        m_status &= ~IDE_STATUS_ERROR;
        break;

    default:
        //DebugLogFormat(_T("HDD COMMAND %02x (UNKNOWN): C=%d, H=%d, SN=%d, SC=%d\r\n"),
        //        command, m_curcylinder, m_curhead, m_cursector, m_sectorcount);
        break;
    }
}

// Copy the string to the destination, swapping bytes in every word
// For use in CHardDrive::IdentifyDrive() method.
static void swap_strncpy(uint8_t* dst, const char* src, int words)
{
    int i;
    for (i = 0; src[i] != 0; i++)
        dst[i ^ 1] = src[i];
    for ( ; i < words * 2; i++)
        dst[i ^ 1] = ' ';
}

void CHardDrive::IdentifyDrive()
{
    uint32_t totalsectors = (uint32_t)m_numcylinders * (uint32_t)m_numheads * (uint32_t)m_numsectors;

    memset(m_buffer, 0, IDE_DISK_SECTOR_SIZE);

    uint16_t* pwBuffer = (uint16_t*)m_buffer;
    pwBuffer[0]  = 0x045a;  // Configuration: fixed disk
    pwBuffer[1]  = (uint16_t)m_numcylinders;
    pwBuffer[3]  = (uint16_t)m_numheads;
    pwBuffer[6]  = (uint16_t)m_numsectors;
    swap_strncpy((uint8_t*)(pwBuffer + 10), "0000000000", 10);  // Serial number
    swap_strncpy((uint8_t*)(pwBuffer + 23), "1.0", 4);  // Firmware version
    swap_strncpy((uint8_t*)(pwBuffer + 27), "UKNCBTL Hard Disk", 18);  // Model
    pwBuffer[47] = 0x8001;  // Read/write multiple support
    pwBuffer[49] = 0x2f00;  // Capabilities: bit9 = LBA
    pwBuffer[53] = 1;  // Words 54-58 are valid
    pwBuffer[54] = (uint16_t)m_numcylinders;
    pwBuffer[55] = (uint16_t)m_numheads;
    pwBuffer[56] = (uint16_t)m_numsectors;
    *(uint32_t*)(pwBuffer + 57) = (uint32_t)m_numheads * (uint32_t)m_numsectors;
    *(uint32_t*)(pwBuffer + 60) = totalsectors;
    *(uint32_t*)(pwBuffer + 100) = totalsectors;

    InvertBuffer(m_buffer);
}

uint32_t CHardDrive::CalculateOffset() const
{
    int sector = (m_curcylinder * m_numheads + m_curhead) * m_numsectors + (m_cursector - 1);
    return sector * IDE_DISK_SECTOR_SIZE;
}

void CHardDrive::ReadNextSector()
{
    m_status |= IDE_STATUS_BUSY;

    m_timeoutcount = TIME_PER_SECTOR * 2;  // Timeout while seek for next sector
    m_timeoutevent = TIMEEVT_READ_SECTOR_DONE;
}

void CHardDrive::ReadSectorDone()
{
    m_status &= ~IDE_STATUS_BUSY;
    m_status &= ~IDE_STATUS_ERROR;
    m_status |= IDE_STATUS_BUFFER_READY;
    m_status |= IDE_STATUS_SEEK_COMPLETE;

    // Read sector from HDD image to the buffer
    uint32_t fileOffset = CalculateOffset();
    ::fseek(m_fpFile, fileOffset, SEEK_SET);
    size_t dwBytesRead = ::fread(m_buffer, 1, IDE_DISK_SECTOR_SIZE, m_fpFile);
    if (dwBytesRead != IDE_DISK_SECTOR_SIZE)
    {
        m_status |= IDE_STATUS_ERROR;
        m_error = IDE_ERROR_BAD_SECTOR;
        return;
    }

    if (m_sectorcount > 0)
        m_sectorcount--;

    if (m_sectorcount > 0)
    {
        NextSector();
    }

    m_error = IDE_ERROR_NONE;
    m_bufferoffset = 0;
}

void CHardDrive::WriteSectorDone()
{
    m_status &= ~IDE_STATUS_BUSY;
    m_status &= ~IDE_STATUS_ERROR;
    m_status |= IDE_STATUS_BUFFER_READY;
    m_status |= IDE_STATUS_SEEK_COMPLETE;

    // Write buffer to the HDD image
    uint32_t fileOffset = CalculateOffset();

    //DebugLogFormat(_T("WriteSector %lx\r\n"), fileOffset);

    if (m_okReadOnly)
    {
        m_status |= IDE_STATUS_ERROR;
        m_error = IDE_ERROR_BAD_SECTOR;
        return;
    }

    ::fseek(m_fpFile, fileOffset, SEEK_SET);
    size_t dwBytesWritten = ::fwrite(m_buffer, 1, IDE_DISK_SECTOR_SIZE, m_fpFile);
    if (dwBytesWritten != IDE_DISK_SECTOR_SIZE)
    {
        m_status |= IDE_STATUS_ERROR;
        m_error = IDE_ERROR_BAD_SECTOR;
        return;
    }

    if (m_sectorcount > 0)
        m_sectorcount--;

    if (m_sectorcount > 0)
    {
        NextSector();
    }

    m_error = IDE_ERROR_NONE;
    m_bufferoffset = 0;
}

void CHardDrive::NextSector()
{
    // Advance to the next sector, CHS-based
    m_cursector++;
    if (m_cursector > m_numsectors)  // Sectors are 1-based
    {
        m_cursector = 1;
        m_curhead++;
        if (m_curhead >= m_numheads)
        {
            m_curhead = 0;
            m_curcylinder++;
        }
    }
}

void CHardDrive::ContinueRead()
{
    m_bufferoffset = 0;

    m_status &= ~IDE_STATUS_BUFFER_READY;
    m_status &= ~IDE_STATUS_BUSY;

    if (m_sectorcount > 0)
        ReadNextSector();
}

void CHardDrive::ContinueWrite()
{
    m_bufferoffset = 0;

    m_status &= ~IDE_STATUS_BUFFER_READY;
    m_status |= IDE_STATUS_BUSY;

    m_timeoutcount = TIME_PER_SECTOR;
    m_timeoutevent = TIMEEVT_WRITE_SECTOR_DONE;
}


//////////////////////////////////////////////////////////////////////
