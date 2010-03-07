// Hard.cpp
//

#include "StdAfx.h"
#include "Emubase.h"


//////////////////////////////////////////////////////////////////////
// Constants

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
    m_numcilynders = dwFileSize / 512 / m_numsectors / m_numheads;
    if (m_numcilynders == 0 || m_numcilynders > 1024)
        return FALSE;

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

WORD CHardDrive::ReadPort(WORD port)
{
    ASSERT(port >= 0x1F0 && port <= 0x1F7);

    WORD data = 0;
    switch (port)
    {
    case 0x1f1:
        data = m_error;
        break;
    case 0x1f7:
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

    //TODO
}

//////////////////////////////////////////////////////////////////////
