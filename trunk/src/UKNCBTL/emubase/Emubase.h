// Emubase.h  Header for use of all emubase classes
//

#pragma once

#include "Board.h"
#include "Processor.h"
#include "Memory.h"


//////////////////////////////////////////////////////////////////////
// Disassembler

// Disassemble one instruction of KM1801VM2 processor
//   pMemory - memory image (we read only words of the instruction)
//   sInstr  - instruction mnemonics buffer - at least 8 characters
//   sArg    - instruction arguments buffer - at least 32 characters
//   Return value: number of words in the instruction
int DisassembleInstruction(WORD* pMemory, WORD addr, TCHAR* sInstr, TCHAR* sArg);


//////////////////////////////////////////////////////////////////////
// CFloppy

#define SAMPLERATE			64000


#define FLOPPY_FSM_IDLE			0

#define FLOPPY_CMD_CORRECTION250		04
#define FLOPPY_CMD_ENGINESTART			020
#define FLOPPY_CMD_CORRECTION500		010
#define FLOPPY_CMD_SIDEUP				040
#define FLOPPY_CMD_DIR					0100
#define FLOPPY_CMD_STEP					0200
#define FLOPPY_CMD_SEARCHSYNC			0400
#define FLOPPY_CMD_SKIPSYNC				01000
//dir == 0 to center (towards trk0)
//dir == 1 from center (towards trk80)

#define FLOPPY_STATUS_TRACK0			01
#define FLOPPY_STATUS_RDY				02
#define FLOPPY_STATUS_WRITEPROTECT		04
#define FLOPPY_STATUS_MOREDATA			0200
#define FLOPPY_STATUS_CHECKSUMOK		040000
#define FLOPPY_STATUS_INDEXMARK			0100000

#define FLOPPY_RAWTRACKSIZE             6250
#define FLOPPY_RAWMARKERSIZE            (FLOPPY_RAWTRACKSIZE / 2)
#define FLOPPY_INDEXLENGTH              150

struct CFloppyDrive
{
    HANDLE hFile;
    BOOL okNetRT11Image;  // TRUE - .rtd image, FALSE - .dsk image
    BOOL okReadOnly;    // Write protection flag
	WORD track;         // Track number: from 0 to 79
	WORD side;          // Disk side: 0 or 1
	WORD dataptr;       // Data offset within m_data - "head" position
	BYTE data[FLOPPY_RAWTRACKSIZE];  // Raw track image for the current track
    BYTE marker[FLOPPY_RAWMARKERSIZE];  // Marker positions
    WORD datatrack;     // Track number of data in m_data array
    WORD dataside;      // Disk side of data in m_data array

public:
    CFloppyDrive();
    void Reset();
};

class CFloppyController
{
protected:
    CFloppyDrive m_drivedata[4];
    WORD m_drive;       // Drive number: from 0 to 3
    CFloppyDrive* m_pDrive;  // Current drive
	WORD m_track;       // Track number: from 0 to 79
	WORD m_side;        // Disk side: 0 or 1
	WORD m_status;      // See FLOPPY_STATUS_XXX defines
	WORD m_flags;       // See FLOPPY_CMD_XXX defines
	WORD m_datareg;     // Read mode data register
    WORD m_writereg;    // Write mode data register
    BOOL m_writeflag;   // Write mode data register has data
    BOOL m_writemarker; // Write marker in m_marker
    WORD m_shiftreg;    // Write mode shift register
    BOOL m_shiftflag;   // Write mode shift register has data
    BOOL m_shiftmarker; // Write marker in m_marker
    BOOL m_writing;     // TRUE = write mode, FALSE = read mode
    BOOL m_searchsync;  // Read sub-mode: TRUE = search for sync, FALSE = just read
    BOOL m_crccalculus; // TRUE = CRC is calculated now
    BOOL m_trackchanged;  // TRUE = m_data was changed - need to save it into the file

public:
    CFloppyController();
    ~CFloppyController();
    void Reset();

public:
    BOOL AttachImage(int drive, LPCTSTR sFileName);
    void DetachImage(int drive);
    BOOL IsAttached(int drive) { return (m_drivedata[drive].hFile != INVALID_HANDLE_VALUE); }
    BOOL IsReadOnly(int drive) { return m_drivedata[drive].okReadOnly; } // return (m_status & FLOPPY_STATUS_WRITEPROTECT) != 0; }
    BOOL IsEngineOn() { return (m_flags & FLOPPY_CMD_ENGINESTART) != 0; }
	WORD GetData(void);         // Reading port 177132 - data
	WORD GetState(void);        // Reading port 177130 - device status
	void SetCommand(WORD cmd);  // Writing to port 177130 - commands
	void WriteData(WORD Data);  // Writing to port 177132 - data
	void Periodic();            // Rotate disk; call it each 64 us - 15625 times per second

private:
	void PrepareTrack();
    void FlushChanges();  // If current track was changed - save it

};


//////////////////////////////////////////////////////////////////////
