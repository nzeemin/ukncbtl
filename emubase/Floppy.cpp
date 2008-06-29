// Floppy.cpp
// Floppy controller and drives emulation
// See defines in header file Emubase.h

#include "StdAfx.h"
#include "Emubase.h"

//////////////////////////////////////////////////////////////////////

// Маска флагов, сохраняемых в m_flags
const WORD FLOPPY_CMD_MASKSTORED =
    FLOPPY_CMD_CORRECTION250 | FLOPPY_CMD_CORRECTION500 | FLOPPY_CMD_SIDEUP | FLOPPY_CMD_DIR | FLOPPY_CMD_SKIPSYNC |
    FLOPPY_CMD_ENGINESTART;

static void EncodeTrackData(BYTE* pSrc, BYTE* data, BYTE* marker, WORD track, WORD side);
static BOOL DecodeTrackData(BYTE* pRaw, BYTE* pDest);

//////////////////////////////////////////////////////////////////////


CFloppyDrive::CFloppyDrive()
{
    hFile = INVALID_HANDLE_VALUE;
    okNetRT11Image = okReadOnly = FALSE;
    datatrack = dataside = 0;
    dataptr = 0;
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
    m_pDrive = m_drivedata;
    m_datareg = m_writereg = m_shiftreg = 0;
    m_writing = m_searchsync = m_writemarker = m_crccalculus = FALSE;
    m_writeflag = m_shiftflag = FALSE;
    m_trackchanged = FALSE;
    m_status = FLOPPY_STATUS_TRACK0|FLOPPY_STATUS_WRITEPROTECT;
    m_flags = FLOPPY_CMD_CORRECTION500|FLOPPY_CMD_SIDEUP|FLOPPY_CMD_DIR|FLOPPY_CMD_SKIPSYNC;
}

CFloppyController::~CFloppyController()
{
    for (int drive = 0; drive < 4; drive++)
        DetachImage(drive);
}

void CFloppyController::Reset()
{
    FlushChanges();

    m_drive = m_side = m_track = 0;
    m_pDrive = m_drivedata;
    m_datareg = m_writereg = m_shiftreg = 0;
    m_writing = m_searchsync = m_writemarker = m_crccalculus = FALSE;
    m_writeflag = m_shiftflag = FALSE;
    m_trackchanged = FALSE;
    m_status = (m_pDrive->okReadOnly) ? FLOPPY_STATUS_TRACK0|FLOPPY_STATUS_WRITEPROTECT : FLOPPY_STATUS_TRACK0;
    m_flags = FLOPPY_CMD_CORRECTION500|FLOPPY_CMD_SIDEUP|FLOPPY_CMD_DIR|FLOPPY_CMD_SKIPSYNC;

    PrepareTrack();
}

BOOL CFloppyController::AttachImage(int drive, LPCTSTR sFileName)
{
    ASSERT(sFileName != NULL);

    // If image attached - detach one first
    if (m_drivedata[drive].hFile != INVALID_HANDLE_VALUE)
        DetachImage(drive);

    // Определяем, это .dsk-образ или .rtd-образ - по расширению файла
    m_drivedata[drive].okNetRT11Image = FALSE;
    LPCTSTR sFileNameExt = wcsrchr(sFileName, _T('.'));
    if (sFileNameExt != NULL && _wcsicmp(sFileNameExt, _T(".rtd")) == 0)
        m_drivedata[drive].okNetRT11Image = TRUE;

    // Check read-only file attribute
    DWORD dwFileAttrs = ::GetFileAttributes(sFileName);
    m_drivedata[drive].okReadOnly = (dwFileAttrs & FILE_ATTRIBUTE_READONLY) != 0;

    // Open file
    if (m_drivedata[drive].okReadOnly)
        m_drivedata[drive].hFile = CreateFile(sFileName,
                GENERIC_READ, FILE_SHARE_READ, NULL,
                OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
    else
        m_drivedata[drive].hFile = CreateFile(sFileName,
                GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_drivedata[drive].hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    m_side = m_track = m_drivedata[drive].datatrack = m_drivedata[drive].dataside = 0;
    m_drivedata[drive].dataptr = 0;
    m_datareg = m_writereg = m_shiftreg = 0;
    m_writing = m_searchsync = m_writemarker = m_crccalculus = FALSE;
    m_writeflag = m_shiftflag = FALSE;
    m_trackchanged = FALSE;
    m_status = (m_pDrive->okReadOnly) ? FLOPPY_STATUS_TRACK0|FLOPPY_STATUS_WRITEPROTECT : FLOPPY_STATUS_TRACK0;
    m_flags = FLOPPY_CMD_CORRECTION500|FLOPPY_CMD_SIDEUP|FLOPPY_CMD_DIR|FLOPPY_CMD_SKIPSYNC;

    PrepareTrack();

    return TRUE;
}

void CFloppyController::DetachImage(int drive)
{
    if (m_drivedata[drive].hFile == INVALID_HANDLE_VALUE) return;

    FlushChanges();

    ::CloseHandle(m_drivedata[drive].hFile);
    m_drivedata[drive].hFile = INVALID_HANDLE_VALUE;
    m_drivedata[drive].okNetRT11Image = m_drivedata[drive].okReadOnly = FALSE;
    m_drivedata[drive].Reset();
}

//////////////////////////////////////////////////////////////////////


WORD CFloppyController::GetState(void)
{
    if (m_track == 0)
        m_status |= FLOPPY_STATUS_TRACK0;
    else
        m_status &= ~FLOPPY_STATUS_TRACK0;
    if (m_pDrive->dataptr < FLOPPY_INDEXLENGTH)
        m_status |= FLOPPY_STATUS_INDEXMARK;
    else
        m_status &= ~FLOPPY_STATUS_INDEXMARK;

    WORD res = m_status;

//#if !defined(PRODUCT)
//    if (res & FLOPPY_STATUS_MOREDATA)
//    {
//        TCHAR oct2[7];  PrintOctalValue(oct2, res);
//        DebugLogFormat(_T("Floppy GET STATE %s\r\n"), oct2);
//    }
//#endif

    return res;
}

void CFloppyController::SetCommand(WORD cmd)
{
//#if !defined(PRODUCT)
//	TCHAR oct2[7];
//    PrintOctalValue(oct2, cmd);
//	DebugLogFormat(_T("Floppy COMMAND %s\r\n"), oct2);
//#endif

    BOOL okPrepareTrack = FALSE;  // Нужно ли считывать дорожку в буфер

    // Проверить, не сменился ли текущий привод
    WORD newdrive = (cmd & 3) ^ 3;
    if (m_drive != newdrive)
    {
        FlushChanges();

        m_drive = newdrive;
        m_pDrive = m_drivedata + m_drive;
        okPrepareTrack = TRUE;
    }
    cmd &= ~3;  // Убираем из команды информацию о текущем приводе

    // Copy new flags to m_flags
    m_flags &= ~FLOPPY_CMD_MASKSTORED;
    m_flags |= cmd & FLOPPY_CMD_MASKSTORED;
    
    // Проверяем, не сменилась ли сторона
    if (m_flags & FLOPPY_CMD_SIDEUP)  // Side selection: 0 - down, 1 - up
    {
        if (m_side == 0) { m_side = 1;  okPrepareTrack = TRUE; }
    }
    else
    {
        if (m_side == 1) { m_side = 0;  okPrepareTrack = TRUE; }	
    }

    if (cmd & FLOPPY_CMD_STEP)  // Move head for one track to center or from center
    {
//#if !defined(PRODUCT)
//        DebugLog(_T("Floppy STEP\r\n"));  //DEBUG
//#endif
        m_side = (m_flags & FLOPPY_CMD_SIDEUP) ? 1 : 0;

        if (m_flags & FLOPPY_CMD_DIR)
        {
            if (m_track < 79) { m_track++;  okPrepareTrack = TRUE; }
        }
        else
        {
            if (m_track >= 1) { m_track--;  okPrepareTrack = TRUE; }
        }
    }
    if (okPrepareTrack)
        PrepareTrack();

    if(cmd & FLOPPY_CMD_SEARCHSYNC)  // Search for marker
    {
//#if !defined(PRODUCT)
//        DebugLog(_T("Floppy SEARCHSYNC\r\n"));  //DEBUG
//#endif
        m_flags &= ~FLOPPY_CMD_SEARCHSYNC;
        m_searchsync = TRUE;
        m_crccalculus = TRUE;
        m_status &= ~FLOPPY_STATUS_CHECKSUMOK;
    }

    if (m_writing && (cmd & FLOPPY_CMD_SKIPSYNC))  // Запись маркера
    {
//#if !defined(PRODUCT)
//        DebugLog(_T("Floppy MARKER\r\n"));  //DEBUG
//#endif
        m_writemarker = TRUE;
        m_status &= ~FLOPPY_STATUS_CHECKSUMOK;
    }
    
}

WORD CFloppyController::GetData(void)
{
//#if !defined(PRODUCT)
//    DebugLogFormat(_T("Floppy READ\t\t%04x\r\n"), m_datareg);  //DEBUG
//#endif
    
    m_status &= ~FLOPPY_STATUS_MOREDATA;
    m_writing = m_searchsync = FALSE;
    m_writeflag = m_shiftflag = FALSE;

    return m_datareg;
}

void CFloppyController::WriteData(WORD data)
{
//#if !defined(PRODUCT)
//	DebugLogFormat(_T("Floppy WRITE\t\t%04x\r\n"), data);  //DEBUG
//#endif

    m_writing = TRUE;  // Switch to write mode if not yet
    m_searchsync = FALSE;

    if (!m_writeflag && !m_shiftflag)  // Both registers are empty
    {
        m_shiftreg = data;
        m_shiftflag = TRUE;
        m_status |= FLOPPY_STATUS_MOREDATA;
    }
    else if (!m_writeflag && m_shiftflag)  // Write register is empty
    {
        m_writereg = data;
        m_writeflag = TRUE;
        m_status &= ~FLOPPY_STATUS_MOREDATA;
    }
    else if (m_writeflag && !m_shiftflag)  // Shift register is empty
    {
        m_shiftreg = m_writereg;
        m_shiftflag = m_writeflag;
        m_writereg = data;
        m_writeflag = TRUE;
        m_status &= ~FLOPPY_STATUS_MOREDATA;
    }
    else  // Both registers are not empty
    {
        m_writereg = data;
    }
}

void CFloppyController::Periodic()
{
    if (!IsEngineOn()) return;  // Вращаем дискеты только если включен мотор

    // Вращаем дискеты во всех драйвах сразу
    for (int drive = 0; drive < 4; drive++)
    {
        m_drivedata[drive].dataptr += 2;
        if (m_drivedata[drive].dataptr >= FLOPPY_RAWTRACKSIZE)
            m_drivedata[drive].dataptr = 0;
    }

    // Далее обрабатываем чтение/запись на текущем драйве
    if (!IsAttached(m_drive)) return;

    if (!m_writing)  // Read mode
    {
        m_datareg = (m_pDrive->data[m_pDrive->dataptr] << 8) | m_pDrive->data[m_pDrive->dataptr + 1];
        if (m_status & FLOPPY_STATUS_MOREDATA)
        {
            if (m_crccalculus)  // Stop CRC calculation
            {
                m_crccalculus = FALSE;
                //TODO: Compare calculated CRC to m_datareg
                m_status |= FLOPPY_STATUS_CHECKSUMOK;
            }
        }
        else
        {
            if (m_searchsync)  // Search for marker
            {
                if (m_pDrive->marker[m_pDrive->dataptr / 2])  // Marker found
                {
                    m_status |= FLOPPY_STATUS_MOREDATA;
                    m_searchsync = FALSE;
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
            m_pDrive->data[m_pDrive->dataptr] = LOBYTE(m_shiftreg);
            m_pDrive->data[m_pDrive->dataptr + 1] = HIBYTE(m_shiftreg);
            m_shiftflag = FALSE;
            m_trackchanged = TRUE;

            if (m_shiftmarker)
            {
//#if !defined(PRODUCT)
//            DebugLogFormat(_T("Floppy WRITING %04x MARKER\r\n"), m_shiftreg);  //DEBUG
//#endif
                m_pDrive->marker[m_pDrive->dataptr / 2] = TRUE;
                m_shiftmarker = FALSE;
                m_crccalculus = TRUE;  // Start CRC calculation
            }
            else
            {
//#if !defined(PRODUCT)
//            DebugLogFormat(_T("Floppy WRITING %04x\r\n"), m_shiftreg);  //DEBUG
//#endif
                m_pDrive->marker[m_pDrive->dataptr / 2] = FALSE;
            }

            if (m_writeflag)
            {
                m_shiftreg = m_writereg;
                m_shiftflag = m_writeflag;  m_writeflag = FALSE;
                m_shiftmarker = m_writemarker;  m_writemarker = FALSE;
                m_status |= FLOPPY_STATUS_MOREDATA;
            }
            else
            {
                if (m_crccalculus)  // Stop CRC calclation
                {
                    m_shiftreg = 0x4444;  //STUB
                    m_shiftflag = TRUE;
                    m_shiftmarker = FALSE;
                    m_crccalculus = FALSE;
                    m_status |= FLOPPY_STATUS_CHECKSUMOK;
                }
            }

        }
    }

}

// Read track data from file and fill m_data
void CFloppyController::PrepareTrack()
{
    FlushChanges();

    //TCHAR buffer[512];
    DWORD count;

    m_trackchanged = FALSE;
    m_status |= FLOPPY_STATUS_MOREDATA;
    m_pDrive->dataptr = 0;
    m_pDrive->datatrack = m_track;
    m_pDrive->dataside = m_side;

    // Track has 10 sectors, 512 bytes each; offset of the file is === ((Track<<1)+SIDE)*5120
    long foffset = ((m_track * 2) + (m_side)) * 5120;
    if (m_pDrive->okNetRT11Image) foffset += 256;  // Skip .RTD image header
    //wsprintf(buffer,_T("floppy file offset %d  for trk %d side %d\r\n"),foffset,m_track,m_side);
    //DebugPrint(buffer);

    BYTE data[5120];
    memset(data, 0, 5120);

    if (m_pDrive->hFile != INVALID_HANDLE_VALUE)
    {
        SetFilePointer(m_pDrive->hFile, foffset, NULL, FILE_BEGIN);
        ReadFile(m_pDrive->hFile, &data, 5120, &count, NULL);
        //TODO: Контроль ошибок чтения
    }

    // Fill m_data array and m_marker array with marked data
    EncodeTrackData(data, m_pDrive->data, m_pDrive->marker, m_track, m_side);

    ////DEBUG: Test DecodeTrackData()
    //BYTE data2[5120];
    //BOOL parsed = DecodeTrackData(m_pDrive->data, data2);
    //ASSERT(parsed);
    //BOOL tested = TRUE;
    //for (int i = 0; i < 5120; i++)
    //    if (data[i] != data2[i])
    //    {
    //        tested = FALSE;
    //        break;
    //    }
    //ASSERT(tested);
}

void CFloppyController::FlushChanges()
{
    if (!IsAttached(m_drive)) return;
    if (!m_trackchanged) return;

//#if !defined(PRODUCT)
//    DebugLog(_T("Floppy FLUSH\r\n"));  //DEBUG
//#endif

    BYTE data[5120];
    memset(data, 0, 5120);

    // Decode track data from m_data
    BOOL decoded = DecodeTrackData(m_pDrive->data, data);

    //TODO: Check for errors
    if (decoded)  // Write to the file only if the track was correctly decoded from raw data
    {
        // Track has 10 sectors, 512 bytes each; offset of the file is === ((Track<<1)+SIDE)*5120
        long foffset = ((m_pDrive->datatrack * 2) + (m_pDrive->dataside)) * 5120;
        if (m_pDrive->okNetRT11Image) foffset += 256;  // Skip .RTD image header

        // Check file length
        DWORD currentFileSize = ::GetFileSize(m_pDrive->hFile, NULL);
        if (currentFileSize < (DWORD)(foffset + 5120))
        {
            ::SetFilePointer(m_pDrive->hFile, foffset + 5120, NULL, FILE_BEGIN);
            ::SetEndOfFile(m_pDrive->hFile);
        }
        // Save data into the file
        ::SetFilePointer(m_pDrive->hFile, foffset, NULL, FILE_BEGIN);
        DWORD dwBytesWritten;
        ::WriteFile(m_pDrive->hFile, &data, 5120, &dwBytesWritten, NULL);
        //TODO: Проверка на ошибки записи
    }
    else {
//#if !defined(PRODUCT)
//    DebugLog(_T("Floppy FLUSH FAILED\r\n"));  //DEBUG
//#endif
    }

    m_trackchanged = FALSE;

    ////DEBUG: Save raw m_data/m_marker into rawdata.bin
    //HANDLE hRawFile = CreateFile(_T("rawdata.bin"),
    //            GENERIC_WRITE, FILE_SHARE_READ, NULL,
    //            CREATE_ALWAYS, 0, NULL);

}


//////////////////////////////////////////////////////////////////////


// Fill data array and marker array with marked data
static void EncodeTrackData(BYTE* pSrc, BYTE* data, BYTE* marker, WORD track, WORD side)
{
    memset(data, 0, FLOPPY_RAWTRACKSIZE);
    memset(marker, 0, FLOPPY_RAWMARKERSIZE);
    DWORD count;
    int ptr = 0;
    int gap = 34;
    for (int sect = 0; sect < 10; sect++)
    {
        // GAP
        for (count = 0; count < (DWORD) gap; count++) data[ptr++] = 0x4e;
        // sector header
        for (count = 0; count < 12; count++) data[ptr++] = 0x00;
        // marker
        marker[ptr / 2] = TRUE;  // ID marker; start CRC calculus
        data[ptr++] = 0xa1;  data[ptr++] = 0xa1;  data[ptr++] = 0xa1;
        data[ptr++] = 0xfe;
        
        data[ptr++] = (BYTE) track;  data[ptr++] = (side != 0);
        data[ptr++] = sect + 1;  data[ptr++] = 2; // Assume 512 bytes per sector;
        // crc
        //TODO: Calculate CRC
        data[ptr++] = 0x12;  data[ptr++] = 0x34;  // CRC stub

        // sync
        for (count = 0; count < 24; count++) data[ptr++] = 0x4e;
        // data header
        for (count = 0; count < 12; count++) data[ptr++] = 0x00;
        // marker
        marker[ptr / 2] = TRUE;  // Data marker; start CRC calculus
        data[ptr++] = 0xa1;  data[ptr++] = 0xa1;  data[ptr++]=0xa1;
        data[ptr++] = 0xfb;
        // data
        for (count = 0; count < 512; count++)
            data[ptr++] = pSrc[(sect * 512) + count];
        // crc
        //TODO: Calculate CRC
        data[ptr++] = 0x43;  data[ptr++] = 0x21;  // CRC stub

        gap = 38;
    }
    // fill GAP4B to the end of the track
    while (ptr < FLOPPY_RAWTRACKSIZE) data[ptr++] = 0x4e;
}

// Decode track data from raw data
// pRaw is array of FLOPPY_RAWTRACKSIZE bytes
// pDest is array of 5120 bytes
// Returns: TRUE - decoded, FALSE - parse error
static BOOL DecodeTrackData(BYTE* pRaw, BYTE* pDest)
{
    WORD dataptr = 0;  // Offset in m_data array
    WORD destptr = 0;  // Offset in data array
    for (;;)
    {
        while (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0x4e) dataptr++;  // Skip GAP1 or GAP3
        if (dataptr >= FLOPPY_RAWTRACKSIZE) break;  // End of track or error
        while (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0x00) dataptr++;  // Skip sync
        if (dataptr >= FLOPPY_RAWTRACKSIZE) return FALSE;  // Something wrong

        if (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0xa1) dataptr++;
        if (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0xa1) dataptr++;
        if (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0xa1) dataptr++;
        if (dataptr >= FLOPPY_RAWTRACKSIZE) return FALSE;  // Something wrong
        if (pRaw[dataptr++] != 0xfe) return FALSE;  // Marker not found

        BYTE sectcyl, secthd, sectsec, sectno;
        if (dataptr < FLOPPY_RAWTRACKSIZE) sectcyl = pRaw[dataptr++];
        if (dataptr < FLOPPY_RAWTRACKSIZE) secthd  = pRaw[dataptr++];
        if (dataptr < FLOPPY_RAWTRACKSIZE) sectsec = pRaw[dataptr++];
        if (dataptr < FLOPPY_RAWTRACKSIZE) sectno  = pRaw[dataptr++];
        if (dataptr >= FLOPPY_RAWTRACKSIZE) return FALSE;  // Something wrong

        int sectorsize;
        if (sectno == 1) sectorsize = 256;
        else if (sectno == 2) sectorsize = 512;
        else if (sectno == 3) sectorsize = 1024;
        else return FALSE;  // Something wrong: unknown sector size
        // crc
        if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;
        if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;

        while (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0x4e) dataptr++;  // Skip GAP2
        if (dataptr >= FLOPPY_RAWTRACKSIZE) return FALSE;  // Something wrong
        while (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0x00) dataptr++;  // Skip sync
        if (dataptr >= FLOPPY_RAWTRACKSIZE) return FALSE;  // Something wrong

        if (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0xa1) dataptr++;
        if (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0xa1) dataptr++;
        if (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0xa1) dataptr++;
        if (dataptr >= FLOPPY_RAWTRACKSIZE) return FALSE;  // Something wrong
        if (pRaw[dataptr++] != 0xfb) return FALSE;  // Marker not found

        for (int count = 0; count < sectorsize; count++)  // Copy sector data
        {
            if (destptr >= 5120) break;  // End of track or error
            pDest[destptr++] = pRaw[dataptr++];
            if (dataptr >= FLOPPY_RAWTRACKSIZE) return FALSE;  // Something wrong
        }
        if (dataptr >= FLOPPY_RAWTRACKSIZE) return FALSE;  // Something wrong
        // crc
        if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;
        if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;
    }

    return TRUE;
}


//////////////////////////////////////////////////////////////////////

