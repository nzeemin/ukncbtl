// Floppy.cpp
// Floppy controller and drives emulation
// See defines in header file Emubase.h

#include "StdAfx.h"
#include "Emubase.h"


static BOOL DecodeTrackData(BYTE* pRaw, BYTE* pDest);

//////////////////////////////////////////////////////////////////////

    
CFloppy::CFloppy()
{
    m_hFile = INVALID_HANDLE_VALUE;
    m_okNetRT11Image = FALSE;
    m_okReadOnly = TRUE;

    m_side = m_track = m_datatrack = m_dataside = 0;
    m_dataptr = 0;
    m_datareg = m_writereg = m_shiftreg = 0;
    m_writing = m_searchsync = m_writemarker = m_crccalculus = FALSE;
    m_writeflag = m_shiftflag = FALSE;
    m_trackchanged = FALSE;
    m_status = FLOPPY_STATUS_TRACK0|FLOPPY_STATUS_WRITEPROTECT;
    m_flags = FLOPPY_CMD_CORRECTION500|FLOPPY_CMD_SIDEUP|FLOPPY_CMD_DIR|FLOPPY_CMD_SKIPSYNC;
}

CFloppy::~CFloppy()
{
    DetachImage();
}

void CFloppy::Reset()
{
    FlushChanges();

    m_side = m_track = m_datatrack = m_dataside = 0;
    m_dataptr = 0;
    m_datareg = m_writereg = m_shiftreg = 0;
    m_writing = m_searchsync = m_writemarker = m_crccalculus = FALSE;
    m_writeflag = m_shiftflag = FALSE;
    m_trackchanged = FALSE;
    m_status = (m_okReadOnly) ? FLOPPY_STATUS_TRACK0|FLOPPY_STATUS_WRITEPROTECT : FLOPPY_STATUS_TRACK0;
    m_flags = FLOPPY_CMD_CORRECTION500|FLOPPY_CMD_SIDEUP|FLOPPY_CMD_DIR|FLOPPY_CMD_SKIPSYNC;

    PrepareTrack();
}

BOOL CFloppy::AttachImage(LPCTSTR sFileName)
{
    ASSERT(sFileName != NULL);

    // If image attached - detach one first
    if (m_hFile != INVALID_HANDLE_VALUE)
        DetachImage();

    // Определяем, это .dsk-образ или .rtd-образ - по расширению файла
    m_okNetRT11Image = FALSE;
    LPCTSTR sFileNameExt = wcsrchr(sFileName, _T('.'));
    if (sFileNameExt != NULL && _wcsicmp(sFileNameExt, _T(".rtd")) == 0)
        m_okNetRT11Image = TRUE;

    // Check read-only file attribute
    DWORD dwFileAttrs = ::GetFileAttributes(sFileName);
    m_okReadOnly = (dwFileAttrs & FILE_ATTRIBUTE_READONLY) != 0;

    // Open file
    if (m_okReadOnly)
        m_hFile = CreateFile(sFileName,
                GENERIC_READ, FILE_SHARE_READ, NULL,
                OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
    else
        m_hFile = CreateFile(sFileName,
                GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    m_side = m_track = m_datatrack = m_dataside = 0;
    m_dataptr = 0;
    m_datareg = m_writereg = m_shiftreg = 0;
    m_writing = m_searchsync = m_writemarker = m_crccalculus = FALSE;
    m_writeflag = m_shiftflag = FALSE;
    m_trackchanged = FALSE;
    m_status = (m_okReadOnly) ? FLOPPY_STATUS_TRACK0|FLOPPY_STATUS_WRITEPROTECT : FLOPPY_STATUS_TRACK0;
    m_flags = FLOPPY_CMD_CORRECTION500|FLOPPY_CMD_SIDEUP|FLOPPY_CMD_DIR|FLOPPY_CMD_SKIPSYNC;

    PrepareTrack();

    return TRUE;
}

void CFloppy::DetachImage()
{
    if (m_hFile == INVALID_HANDLE_VALUE) return;

    FlushChanges();

    ::CloseHandle(m_hFile);
    m_hFile = INVALID_HANDLE_VALUE;
}

//////////////////////////////////////////////////////////////////////


WORD CFloppy::GetState(void)
{
    if (m_track == 0)
        m_status |= FLOPPY_STATUS_TRACK0;
    else
        m_status &= ~FLOPPY_STATUS_TRACK0;
    if (m_dataptr < FLOPPY_INDEXLENGTH)
        m_status |= FLOPPY_STATUS_INDEXMARK;
    else
        m_status &= ~FLOPPY_STATUS_INDEXMARK;

    WORD res = m_status;

//#if !defined(PRODUCT)
//    if (m_writing && (res & FLOPPY_STATUS_MOREDATA))
//    {
//        TCHAR oct2[7];  PrintOctalValue(oct2, res);
//        DebugLogFormat(_T("Floppy GET STATE %s\r\n"), oct2);
//    }
//#endif

    return res;
}

void CFloppy::SetCommand(WORD cmd)
{
//#if !defined(PRODUCT)
//	TCHAR oct2[7];
//    PrintOctalValue(oct2, cmd);
//	DebugLogFormat(_T("Floppy COMMAND %s\r\n"), oct2);
//#endif

    BOOL okPrepareTrack = FALSE;

    cmd &= ~3; // to ensure no selection data

    // Copy new flags to m_flags
    m_flags &= ~(FLOPPY_CMD_CORRECTION250|FLOPPY_CMD_CORRECTION500|FLOPPY_CMD_SIDEUP|FLOPPY_CMD_DIR|FLOPPY_CMD_SKIPSYNC);
    m_flags |= cmd & (FLOPPY_CMD_CORRECTION250|FLOPPY_CMD_CORRECTION500|FLOPPY_CMD_SIDEUP|FLOPPY_CMD_DIR|FLOPPY_CMD_SKIPSYNC);
    
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

    if (m_writing && (cmd & FLOPPY_CMD_SKIPSYNC))
    {
//#if !defined(PRODUCT)
//        DebugLog(_T("Floppy MARKER\r\n"));  //DEBUG
//#endif
        m_writemarker = TRUE;
        m_status &= ~FLOPPY_STATUS_CHECKSUMOK;
    }
    
}

WORD CFloppy::GetData(void)
{
//#if !defined(PRODUCT)
//    DebugLogFormat(_T("Floppy READ\t\t%04x\r\n"), m_datareg);  //DEBUG
//#endif
    
    m_status &= ~FLOPPY_STATUS_MOREDATA;
    m_writing = m_searchsync = FALSE;
    m_writeflag = m_shiftflag = FALSE;

    return m_datareg;
}

void CFloppy::WriteData(WORD data)
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

void CFloppy::Periodic()
{
    if (!IsAttached()) return;

    // Rotate diskette for the next word
    m_dataptr += 2;
    if (m_dataptr >= FLOPPY_RAWTRACKSIZE)
        m_dataptr = 0;

    if (!m_writing)  // Read mode
    {
        m_datareg = (m_data[m_dataptr] << 8) | m_data[m_dataptr + 1];
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
                if (m_marker[m_dataptr / 2])  // Marker found
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
            m_data[m_dataptr] = LOBYTE(m_shiftreg);
            m_data[m_dataptr + 1] = HIBYTE(m_shiftreg);
            m_shiftflag = FALSE;
            m_trackchanged = TRUE;

            if (m_shiftmarker)
            {
//#if !defined(PRODUCT)
//            DebugLogFormat(_T("Floppy WRITING %04x MARKER\r\n"), m_shiftreg);  //DEBUG
//#endif
                m_marker[m_dataptr / 2] = TRUE;
                m_shiftmarker = FALSE;
                m_crccalculus = TRUE;  // Start CRC calculation
            }
            else
            {
//#if !defined(PRODUCT)
//            DebugLogFormat(_T("Floppy WRITING %04x\r\n"), m_shiftreg);  //DEBUG
//#endif
                m_marker[m_dataptr / 2] = FALSE;
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
void CFloppy::PrepareTrack()
{
    FlushChanges();

    //TCHAR buffer[512];
    DWORD count;

    m_trackchanged = FALSE;
    m_status |= FLOPPY_STATUS_MOREDATA;
    m_dataptr = 0;
    m_datatrack = m_track;
    m_dataside = m_side;

    // Track has 10 sectors, 512 bytes each; offset of the file is === ((Track<<1)+SIDE)*5120
    long foffset = ((m_datatrack * 2) + (m_dataside)) * 5120;
    if (m_okNetRT11Image) foffset += 256;  // Skip .RTD image header
    //wsprintf(buffer,_T("floppy file offset %d  for trk %d side %d\r\n"),foffset,m_track,m_side);
    //DebugPrint(buffer);

    BYTE data[5120];
    memset(data, 0, 5120);

    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        SetFilePointer(m_hFile, foffset, NULL, FILE_BEGIN);
        ReadFile(m_hFile, &data, 5120, &count, NULL);
        //TODO: Контроль ошибок чтения
    }

    // Fill m_data array and m_marker array with marked data
    memset(m_data, 0, FLOPPY_RAWTRACKSIZE);
    memset(m_marker, 0, FLOPPY_RAWMARKERSIZE);
    int ptr = 0;
    int gap = 34;
    for (int sect = 0; sect < 10; sect++)
    {
        // GAP
        for (count = 0; count < (DWORD) gap; count++) m_data[ptr++] = 0x4e;
        // sector header
        for (count = 0; count < 12; count++) m_data[ptr++] = 0x00;
        // marker
        m_marker[ptr / 2] = TRUE;  // ID marker; start CRC calculus
        m_data[ptr++] = 0xa1;  m_data[ptr++] = 0xa1;  m_data[ptr++] = 0xa1;
        m_data[ptr++] = 0xfe;
        
        m_data[ptr++] = (BYTE) m_datatrack;  m_data[ptr++] = (m_dataside != 0);
        m_data[ptr++] = sect + 1;  m_data[ptr++] = 2; // Assume 512 bytes per sector;
        // crc
        //TODO: Calculate CRC
        m_data[ptr++] = 0x12;  m_data[ptr++] = 0x34;  // CRC stub

        // sync
        for (count = 0; count < 24; count++) m_data[ptr++] = 0x4e;
        // data header
        for (count = 0; count < 12; count++) m_data[ptr++] = 0x00;
        // marker
        m_marker[ptr / 2] = TRUE;  // Data marker; start CRC calculus
        m_data[ptr++] = 0xa1;  m_data[ptr++] = 0xa1;  m_data[ptr++]=0xa1;
        m_data[ptr++] = 0xfb;
        // data
        for (count = 0; count < 512; count++)
            m_data[ptr++] = data[(sect * 512) + count];
        // crc
        //TODO: Calculate CRC
        m_data[ptr++] = 0x43;  m_data[ptr++] = 0x21;  // CRC stub

        gap = 38;
    }
    // fill GAP4B to the end of the track
    while (ptr < FLOPPY_RAWTRACKSIZE) m_data[ptr++] = 0x4e;

    ////DEBUG: Test DecodeTrackData()
    //BYTE data2[5120];
    //BOOL parsed = DecodeTrackData(m_data, data2);
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

void CFloppy::FlushChanges()
{
    if (!IsAttached()) return;
    if (!m_trackchanged) return;

//#if !defined(PRODUCT)
//    DebugLog(_T("Floppy FLUSH\r\n"));  //DEBUG
//#endif

    BYTE data[5120];
    memset(data, 0, 5120);

    // Decode track data from m_data
    BOOL decoded = DecodeTrackData(m_data, data);

    //TODO: Check for errors
    if (decoded)  // Write to the file only if the track was correctly decoded from raw data
    {
        // Track has 10 sectors, 512 bytes each; offset of the file is === ((Track<<1)+SIDE)*5120
        long foffset = ((m_datatrack * 2) + (m_dataside)) * 5120;
        if (m_okNetRT11Image) foffset += 256;  // Skip .RTD image header

        // Check file length
        DWORD currentFileSize = ::GetFileSize(m_hFile, NULL);
        if (currentFileSize < (DWORD)(foffset + 5120))
        {
            ::SetFilePointer(m_hFile, foffset + 5120, NULL, FILE_BEGIN);
            ::SetEndOfFile(m_hFile);
        }
        // Save data into the file
        ::SetFilePointer(m_hFile, foffset, NULL, FILE_BEGIN);
        DWORD dwBytesWritten;
        ::WriteFile(m_hFile, &data, 5120, &dwBytesWritten, NULL);
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

