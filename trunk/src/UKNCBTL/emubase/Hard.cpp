// Hard.cpp
//

#include "StdAfx.h"
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
#define IDE_COMMAND_SET_CONFIG          0x91
#define IDE_COMMAND_WRITE_MULTIPLE      0x30

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


CHardDrive::CHardDrive()
{
    m_hFile = INVALID_HANDLE_VALUE;

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

    m_hFile = ::CreateFile(sFileName,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    // Read first sector
    DWORD dwFileSize = ::GetFileSize(m_hFile, NULL);
    if (dwFileSize % 512 != 0)
        return FALSE;
    DWORD dwBytesRead;
    if (!::ReadFile(m_hFile, m_buffer, 512, &dwBytesRead, NULL))
        return FALSE;
    
    // Calculate geometry
    m_numsectors = *(m_buffer + 0);
    m_numheads   = *(m_buffer + 1);
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
    if (m_hFile == INVALID_HANDLE_VALUE) return;

    //FlushChanges();

    ::CloseHandle(m_hFile);
    m_hFile = INVALID_HANDLE_VALUE;
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
        data = 0xff00 | m_curcylinder & 0xff;
        break;
    case IDE_PORT_CYLINDER_MSB:
        data = 0xff00 | (m_curcylinder >> 8) & 0xff;
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
#if !defined(PRODUCT)
            DebugPrintFormat(_T("HDD COMMAND %02x (WRITE MULT): C=%d, H=%d, SN=%d, SC=%d\r\n"),
                    command, m_curcylinder, m_curhead, m_cursector, m_sectorcount);
#endif
            m_bufferoffset = 0;
            m_status |= IDE_STATUS_BUFFER_READY;
            break;

        default:
#if !defined(PRODUCT)
            DebugPrintFormat(_T("HDD COMMAND %02x (UNKNOWN): C=%d, H=%d, SN=%d, SC=%d\r\n"),
                    command, m_curcylinder, m_curhead, m_cursector, m_sectorcount);
            DebugBreak();  // Implement this IDE command!
#endif
            break;
    }
}

DWORD CHardDrive::CalculateOffset()
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
    ::SetFilePointer(m_hFile, fileOffset, NULL, FILE_BEGIN);
    DWORD dwBytesRead;
    if (!::ReadFile(m_hFile, m_buffer, IDE_DISK_SECTOR_SIZE, &dwBytesRead, NULL))
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
    ::SetFilePointer(m_hFile, fileOffset, NULL, FILE_BEGIN);
    DWORD dwBytesWritten;
    if (!::WriteFile(m_hFile, m_buffer, IDE_DISK_SECTOR_SIZE, &dwBytesWritten, NULL))
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
