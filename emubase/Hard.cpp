// Hard.cpp
//

#include "StdAfx.h"
#include "Emubase.h"


//////////////////////////////////////////////////////////////////////
// Constants

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

#define IDE_COMMAND_READ_MULTIPLE		0x20
#define IDE_COMMAND_READ_MULTIPLE_ONCE	0x21
#define IDE_COMMAND_WRITE_MULTIPLE		0x30
#define IDE_COMMAND_SET_CONFIG			0x91
#define IDE_COMMAND_READ_MULTIPLE_BLOCK	0xc4
#define IDE_COMMAND_WRITE_MULTIPLE_BLOCK 0xc5
#define IDE_COMMAND_SET_BLOCK_COUNT		0xc6
#define IDE_COMMAND_READ_DMA			0xc8
#define IDE_COMMAND_WRITE_DMA			0xca
#define IDE_COMMAND_GET_INFO			0xec
#define IDE_COMMAND_SET_FEATURES		0xef
#define IDE_COMMAND_SECURITY_UNLOCK		0xf2
#define IDE_COMMAND_UNKNOWN_F9			0xf9
#define IDE_COMMAND_VERIFY_MULTIPLE		0x40
#define IDE_COMMAND_ATAPI_IDENTIFY		0xa1
#define IDE_COMMAND_RECALIBRATE			0x10
#define IDE_COMMAND_IDLE_IMMEDIATE		0xe1

#define IDE_ERROR_NONE					0x00
#define IDE_ERROR_DEFAULT				0x01
#define IDE_ERROR_UNKNOWN_COMMAND		0x04
#define IDE_ERROR_BAD_LOCATION			0x10
#define IDE_ERROR_BAD_SECTOR			0x80


//////////////////////////////////////////////////////////////////////


CHardDrive::CHardDrive()
{
    m_hFile = INVALID_HANDLE_VALUE;

    m_status = m_error = 0;
}

void CHardDrive::Reset()
{
    m_status = IDE_STATUS_DRIVE_READY | IDE_STATUS_SEEK_COMPLETE;
    m_error = IDE_ERROR_DEFAULT;

    //TODO
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

    m_curcylinder = m_curhead = m_curheadreg = m_cursector = m_curoffset = 0;
    //TODO

    return TRUE;
}

void CHardDrive::DetachImage()
{
    if (m_hFile == INVALID_HANDLE_VALUE) return;

    //FlushChanges();

    ::CloseHandle(m_hFile);
    m_hFile = INVALID_HANDLE_VALUE;
}

// Called from CMotherboard::SystemFrame() every tick
void CHardDrive::Periodic()
{
    // Rotate the disks
    m_curoffset += 2;
    if (m_curoffset > 128)
        m_status &= ~IDE_STATUS_HIT_INDEX;

    if (m_curoffset >= 512)  // Next sector
    {
        m_curoffset = 0;
        m_cursector++;
        if (m_cursector > m_numsectors)  // First sector (sectors are 1-based)
        {
            m_cursector = 1;
            m_status |= IDE_STATUS_HIT_INDEX;
        }
    }

    //TODO
}

WORD CHardDrive::ReadPort(WORD port)
{
    ASSERT(port >= 0x1F0 && port <= 0x1F7);

    WORD data = 0;
    switch (port)
    {
    case IDE_PORT_DATA:
        //TODO
        break;
    case IDE_PORT_ERROR:
        data = m_error;
        break;
    case IDE_PORT_SECTOR_COUNT:
        //TODO
        break;
    case IDE_PORT_SECTOR_NUMBER:
        data = m_cursector;
        break;
    case IDE_PORT_CYLINDER_LSB:
        data = m_curcylinder & 0xff;
        break;
    case IDE_PORT_CYLINDER_MSB:
        data = m_curcylinder >> 8;
        break;
    case IDE_PORT_HEAD_NUMBER:
        data = m_curheadreg;
        break;
    case IDE_PORT_STATUS_COMMAND:
        data = m_status;
        break;
    }

    DebugPrintFormat(_T("HDD ReadPort %x %06o\r\n"), port, data);
    return data;
}

void CHardDrive::WritePort(WORD port, WORD data)
{
    ASSERT(port >= 0x1F0 && port <= 0x1F7);

    DebugPrintFormat(_T("HDD WritePort %x %06o\r\n"), port, data);

    switch (port)
    {
    case IDE_PORT_DATA:
        //TODO
        break;
    case IDE_PORT_ERROR:
        // Writing precompensation value -- ignore
        break;
    case IDE_PORT_SECTOR_COUNT:
        m_sectorcount = (data == 0) ? 256 : data;
        break;
    case IDE_PORT_SECTOR_NUMBER:
        //TODO
        break;
    case IDE_PORT_CYLINDER_LSB:
        m_curcylinder = (m_curcylinder & 0xff00) | (data & 0xff);
        break;
    case IDE_PORT_CYLINDER_MSB:
        m_curcylinder = (m_curcylinder & 0x00ff) | ((data & 0xff) << 8);
        break;
    case IDE_PORT_HEAD_NUMBER:
		m_curhead = data & 0x0f;
		m_curheadreg = data;
        break;
    case IDE_PORT_STATUS_COMMAND:
        HandleCommand((BYTE)data);
        break;
    }
}

void CHardDrive::HandleCommand(BYTE command)
{
    m_command = command;
    switch (command)
    {
        case IDE_COMMAND_READ_MULTIPLE:
        case IDE_COMMAND_READ_MULTIPLE_ONCE:
            //TODO
            break;

        case IDE_COMMAND_READ_MULTIPLE_BLOCK:
            //TODO
            break;

        //TODO
    }
}


//////////////////////////////////////////////////////////////////////
