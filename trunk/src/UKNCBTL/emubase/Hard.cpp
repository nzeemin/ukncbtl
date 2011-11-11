/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Hard.cpp
// Hard disk drive emulation
// See defines in header file Emubase.h

#include "stdafx.h"
#include <sys/stat.h>
#include "Emubase.h"


//////////////////////////////////////////////////////////////////////
// Constants

#define TIME_PER_SECTOR					(IDE_DISK_SECTOR_SIZE / 2)

#define IDE_PORT_DATA                   0x1f0
#define IDE_PORT_ERROR                  0x1f1
#define IDE_PORT_SECTOR_COUNT           0x1f2
#define IDE_PORT_SECTOR_NUMBER          0x1f3
#define IDE_PORT_CYLINDER_LSB           0x1f4
#define IDE_PORT_CYLINDER_MSB           0x1f5
#define IDE_PORT_HEAD_NUMBER            0x1f6
#define IDE_PORT_STATUS_COMMAND         0x1f7

#define IDE_STATUS_ERROR				0x01
#define IDE_STATUS_HIT_INDEX			0x02
#define IDE_STATUS_BUFFER_READY			0x08
#define IDE_STATUS_SEEK_COMPLETE		0x10
#define IDE_STATUS_DRIVE_READY			0x40
#define IDE_STATUS_BUSY					0x80

#define IDE_COMMAND_READ_MULTIPLE       0x20
#define IDE_COMMAND_READ_MULTIPLE1      0x21
#define IDE_COMMAND_SET_CONFIG          0x91
#define IDE_COMMAND_WRITE_MULTIPLE      0x30
#define IDE_COMMAND_WRITE_MULTIPLE1     0x31
#define IDE_COMMAND_IDENTIFY            0xec

#define IDE_ERROR_NONE					0x00
#define IDE_ERROR_DEFAULT				0x01
#define IDE_ERROR_UNKNOWN_COMMAND		0x04
#define IDE_ERROR_BAD_LOCATION			0x10
#define IDE_ERROR_BAD_SECTOR			0x80

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
    DWORD* p = (DWORD*) buffer;
    for (int i = 0; i < 128; i++)
    {
        *p = ~(*p);
        p++;
    }
}

//////////////////////////////////////////////////////////////////////


CHardDrive::CHardDrive()
{
    m_fpFile = NULL;

    m_status = IDE_STATUS_BUSY;
    m_error = IDE_ERROR_NONE;
    m_command = 0;
    m_timeoutcount = m_timeoutevent = 0;
    m_sectorcount = 0;
}

CHardDrive::~CHardDrive()
{
    DetachImage();
}

void CHardDrive::Reset()
{
#if !defined(PRODUCT)
    DebugPrint(_T("HDD Reset\r\n"));
#endif

    m_status = IDE_STATUS_BUSY;
    m_error = IDE_ERROR_NONE;
    m_command = 0;
    m_timeoutcount = 2;
    m_timeoutevent = TIMEEVT_RESET_DONE;
}

BOOL CHardDrive::AttachImage(LPCTSTR sFileName)
{
    ASSERT(sFileName != NULL);

    // Open file
    m_okReadOnly = FALSE;
    m_fpFile = ::_tfopen(sFileName, _T("r+b"));
    if (m_fpFile == NULL)
    {
        m_okReadOnly = TRUE;
        m_fpFile = ::_tfopen(sFileName, _T("rb"));
    }
    if (m_fpFile == NULL)
        return FALSE;

    // Check file size
    ::fseek(m_fpFile, 0, SEEK_END);
    DWORD dwFileSize = ::ftell(m_fpFile);
    ::fseek(m_fpFile, 0, SEEK_SET);
    if (dwFileSize % 512 != 0)
        return FALSE;

    // Read first sector
    size_t dwBytesRead = ::fread(m_buffer, 1, 512, m_fpFile);
    if (dwBytesRead != 512)
        return FALSE;

    // Autodetect inverted image
    m_okInverted = FALSE;
    BYTE test = 0xff;
    for (int i = 0x1f0; i <= 0x1fb; i++)
        test &= m_buffer[i];
    m_okInverted = (test == 0xff);
    // Invert the buffer if needed
    if (m_okInverted)
        InvertBuffer(m_buffer);
    
    // Calculate geometry
    m_numsectors = *(m_buffer + 0);
    m_numheads   = *(m_buffer + 1);
    if (m_numsectors == 0 || m_numheads == 0)
        return FALSE;  // Geometry params are not defined
    m_numcylinders = dwFileSize / 512 / m_numsectors / m_numheads;
    if (m_numcylinders == 0 || m_numcylinders > 1024)
        return FALSE;

    m_curcylinder = m_curhead = m_curheadreg = m_cursector = m_bufferoffset = 0;

    m_status = IDE_STATUS_BUSY;
    m_error = IDE_ERROR_NONE;

    return TRUE;
}

void CHardDrive::DetachImage()
{
    if (m_fpFile == NULL) return;

    //FlushChanges();

    ::fclose(m_fpFile);
    m_fpFile = NULL;
}

WORD CHardDrive::ReadPort(WORD port)
{
    ASSERT(port >= 0x1F0 && port <= 0x1F7);

    WORD data = 0;
    switch (port)
    {
    case IDE_PORT_DATA:
        if (m_status & IDE_STATUS_BUFFER_READY)
        {
            data = *((WORD*)(m_buffer + m_bufferoffset));
            if (!m_okInverted)
                data = ~data;  // Image stored non-inverted, but QBUS inverts the bits
            m_bufferoffset += 2;

            if (m_bufferoffset >= IDE_DISK_SECTOR_SIZE)
            {
#if !defined(PRODUCT)
                DebugPrint(_T("HDD Read sector complete\r\n"));
#endif
                ContinueRead();
            }
        }
        break;
    case IDE_PORT_ERROR:
        data = 0xff00 | m_error;
        break;
    case IDE_PORT_SECTOR_COUNT:
        data = 0xff00 | m_sectorcount;
        break;
    case IDE_PORT_SECTOR_NUMBER:
        data = 0xff00 | m_cursector;
        break;
    case IDE_PORT_CYLINDER_LSB:
        data = 0xff00 | (m_curcylinder & 0xff);
        break;
    case IDE_PORT_CYLINDER_MSB:
        data = 0xff00 | ((m_curcylinder >> 8) & 0xff);
        break;
    case IDE_PORT_HEAD_NUMBER:
        data = 0xff00 | m_curheadreg;
        break;
    case IDE_PORT_STATUS_COMMAND:
        data = 0xff00 | m_status;
        break;
    }

    //DebugPrintFormat(_T("HDD Read  %x     0x%04x\r\n"), port, data);
    return data;
}

void CHardDrive::WritePort(WORD port, WORD data)
{
    ASSERT(port >= 0x1F0 && port <= 0x1F7);

//#if !defined(PRODUCT)
//    DebugPrintFormat(_T("HDD Write %x <-- 0x%04x\r\n"), port, data);
//#endif

    switch (port)
    {
    case IDE_PORT_DATA:
        if (m_status & IDE_STATUS_BUFFER_READY)
        {
            if (!m_okInverted)
                data = ~data;  // Image stored non-inverted, but QBUS inverts the bits
            *((WORD*)(m_buffer + m_bufferoffset)) = data;
            m_bufferoffset += 2;

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
        HandleCommand((BYTE)data);
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

void CHardDrive::HandleCommand(BYTE command)
{
    m_command = command;
    switch (command)
    {
        case IDE_COMMAND_READ_MULTIPLE:
        case IDE_COMMAND_READ_MULTIPLE1:
#if !defined(PRODUCT)
            DebugPrintFormat(_T("HDD COMMAND %02x (READ MULT): C=%d, H=%d, SN=%d, SC=%d\r\n"),
                    command, m_curcylinder, m_curhead, m_cursector, m_sectorcount);
#endif
            m_status |= IDE_STATUS_BUSY;

            m_timeoutcount = TIME_PER_SECTOR * 3;  // Timeout while seek for track
            m_timeoutevent = TIMEEVT_READ_SECTOR_DONE;
            break;

        case IDE_COMMAND_SET_CONFIG:
#if !defined(PRODUCT)
            DebugPrintFormat(_T("HDD COMMAND %02x (SET CONFIG): H=%d, SC=%d\r\n"),
                    command, m_curhead, m_sectorcount);
#endif
            m_numsectors = m_sectorcount;
            m_numheads = m_curhead + 1;
            break;

        case IDE_COMMAND_WRITE_MULTIPLE:
        case IDE_COMMAND_WRITE_MULTIPLE1:
#if !defined(PRODUCT)
            DebugPrintFormat(_T("HDD COMMAND %02x (WRITE MULT): C=%d, H=%d, SN=%d, SC=%d\r\n"),
                    command, m_curcylinder, m_curhead, m_cursector, m_sectorcount);
#endif
            m_bufferoffset = 0;
            m_status |= IDE_STATUS_BUFFER_READY;
            break;

        case IDE_COMMAND_IDENTIFY:
#if !defined(PRODUCT)
            DebugPrintFormat(_T("HDD COMMAND %02x (IDENTIFY)\r\n"), command);
#endif
            IdentifyDrive();  // Prepare the buffer
            m_bufferoffset = 0;
            m_sectorcount = 1;
            m_status |= IDE_STATUS_BUFFER_READY | IDE_STATUS_SEEK_COMPLETE | IDE_STATUS_DRIVE_READY;
            m_status &= ~IDE_STATUS_BUSY;
            m_status &= ~IDE_STATUS_ERROR;
            break;

        default:
#if !defined(PRODUCT)
            DebugPrintFormat(_T("HDD COMMAND %02x (UNKNOWN): C=%d, H=%d, SN=%d, SC=%d\r\n"),
                    command, m_curcylinder, m_curhead, m_cursector, m_sectorcount);
            //DebugBreak();  // Implement this IDE command!
#endif
            break;
    }
}

// Copy the string to the destination, swapping bytes in every word
// For use in CHardDrive::IdentifyDrive() method.
static void swap_strncpy(BYTE* dst, const char* src, int words)
{
	int i;
	for (i = 0; i < (int)strlen(src); i++)
		dst[i ^ 1] = src[i];
	for ( ; i < words * 2; i++)
		dst[i ^ 1] = ' ';
}

void CHardDrive::IdentifyDrive()
{
    DWORD totalsectors = (DWORD)m_numcylinders * (DWORD)m_numheads * (DWORD)m_numsectors;

    memset(m_buffer, 0, IDE_DISK_SECTOR_SIZE);

    WORD* pwBuffer = (WORD*)m_buffer;
    pwBuffer[0]  = 0x045a;  // Configuration: fixed disk
    pwBuffer[1]  = (WORD)m_numcylinders;
    pwBuffer[3]  = (WORD)m_numheads;
    pwBuffer[6]  = (WORD)m_numsectors;
    swap_strncpy((BYTE*)(pwBuffer + 10), "0000000000", 10);  // Serial number
    swap_strncpy((BYTE*)(pwBuffer + 23), "1.0", 4);  // Firmware version
    swap_strncpy((BYTE*)(pwBuffer + 27), "UKNCBTL Hard Disk", 20);  // Model
    pwBuffer[47] = 0x8001;  // Read/write multiple support
    pwBuffer[49] = 0x2f00;  // Capabilities: bit9 = LBA
    pwBuffer[53] = 1;  // Words 54-58 are valid
    pwBuffer[54] = (WORD)m_numcylinders;
    pwBuffer[55] = (WORD)m_numheads;
    pwBuffer[56] = (WORD)m_numsectors;
    *(DWORD*)(pwBuffer + 57) = (DWORD)m_numheads * (DWORD)m_numsectors;
    *(DWORD*)(pwBuffer + 60) = totalsectors;
    *(DWORD*)(pwBuffer + 100) = totalsectors;

    InvertBuffer(m_buffer);
}

DWORD CHardDrive::CalculateOffset() const
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
    DWORD fileOffset = CalculateOffset();
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
    DWORD fileOffset = CalculateOffset();
#if !defined(PRODUCT)
    DebugPrintFormat(_T("WriteSector %lx\r\n"), fileOffset);  //DEBUG
#endif
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
