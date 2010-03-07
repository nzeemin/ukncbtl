// Hard.cpp
//

#include "StdAfx.h"
#include "Emubase.h"


//////////////////////////////////////////////////////////////////////
// Constants

#define IDE_DISK_SECTOR_SIZE			512

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


//////////////////////////////////////////////////////////////////////


CHardDrive::CHardDrive()
{
    m_hFile = INVALID_HANDLE_VALUE;
}

void CHardDrive::Reset()
{
    //TODO
}

BOOL CHardDrive::AttachImage(LPCTSTR sFileName)
{
    ASSERT(sFileName != NULL);

    m_hFile = CreateFile(sFileName,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    //TODO: Prepare

    return TRUE;
}

void CHardDrive::DetachImage()
{
    if (m_hFile == INVALID_HANDLE_VALUE) return;

    //FlushChanges();

    ::CloseHandle(m_hFile);
    m_hFile = INVALID_HANDLE_VALUE;
}


//////////////////////////////////////////////////////////////////////
