/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Board.cpp
//

#include "stdafx.h"
#include "Emubase.h"
#include "Board.h"


//////////////////////////////////////////////////////////////////////

CMotherboard::CMotherboard ()
{
    memset(freq_per, 0, sizeof(freq_per));
    memset(freq_out, 0, sizeof(freq_out));
    memset(freq_enable, 0, sizeof(freq_enable));
    m_multiply = 1;
    memset(m_chancputx, 0, sizeof(m_chancputx));
    memset(m_chancpurx, 0, sizeof(m_chancpurx));
    memset(m_chanpputx, 0, sizeof(m_chanpputx));
    memset(m_chanppurx, 0, sizeof(m_chanppurx));

    m_TapeReadCallback = NULL;
    m_TapeWriteCallback = NULL;
    m_nTapeSampleRate = 0;
    m_SoundGenCallback = NULL;
    m_SerialInCallback = NULL;
    m_SerialOutCallback = NULL;
    m_ParallelOutCallback = NULL;

    // Create devices
    m_pCPU = new CProcessor(_T("CPU"));
    m_pPPU = new CProcessor(_T("PPU"));
    m_pFirstMemCtl = new CFirstMemoryController();
    m_pSecondMemCtl = new CSecondMemoryController();
    m_pFloppyCtl = new CFloppyController();

    // Connect devices
    m_pCPU->AttachMemoryController(m_pFirstMemCtl);
    m_pFirstMemCtl->Attach(this, m_pCPU);
    m_pPPU->AttachMemoryController(m_pSecondMemCtl);
    m_pSecondMemCtl->Attach(this, m_pPPU);

    // Allocate memory for RAM and ROM
    m_pRAM[0] = (BYTE*) malloc(65536);  memset(m_pRAM[0], 0, 65536);
    m_pRAM[1] = (BYTE*) malloc(65536);  memset(m_pRAM[1], 0, 65536);
    m_pRAM[2] = (BYTE*) malloc(65536);  memset(m_pRAM[2], 0, 65536);
    m_pROM    = (BYTE*) malloc(32768);  memset(m_pROM, 0, 32768);
    m_pROMCart[0] = NULL;
    m_pROMCart[1] = NULL;
    m_pHardDrives[0] = NULL;
    m_pHardDrives[1] = NULL;
}

CMotherboard::~CMotherboard ()
{
    // Delete devices
    delete m_pCPU;
    delete m_pPPU;
    delete m_pFirstMemCtl;
    delete m_pSecondMemCtl;
    delete m_pFloppyCtl;

    // Free memory
    free(m_pRAM[0]);
    free(m_pRAM[1]);
    free(m_pRAM[2]);
    free(m_pROM);
    if (m_pROMCart[0] != NULL) free(m_pROMCart[0]);
    if (m_pROMCart[1] != NULL) free(m_pROMCart[1]);
    if (m_pHardDrives[0] != NULL) delete m_pHardDrives[0];
    if (m_pHardDrives[1] != NULL) delete m_pHardDrives[1];
}

void CMotherboard::Reset () 
{
    m_pPPU->SetDCLOPin(TRUE);
    m_pPPU->SetACLOPin(TRUE);

    ResetFloppy();

    if (m_pHardDrives[0] != NULL)
        m_pHardDrives[0]->Reset();
    if (m_pHardDrives[1] != NULL)
        m_pHardDrives[1]->Reset();

    m_cputicks = 0;
    m_pputicks = 0;
    m_lineticks = 0;
    m_timer = 0;
    m_timerreload = 0;
    m_timerflags = 0;
    m_timerdivider = 0;

    m_chan0disabled = 0;

    m_scanned_key = 0;
    memset(m_kbd_matrix, 0, sizeof(m_kbd_matrix));
    m_kbd_matrix[3].row_Y = 0xFF;

    //ChanResetByCPU();
    //ChanResetByPPU();

    // We always start with PPU
    m_pPPU->SetDCLOPin(FALSE);
    m_pPPU->SetACLOPin(FALSE);
}

void CMotherboard::LoadROM(const BYTE* pBuffer)  // Load 32 KB ROM image from the buffer
{
    memcpy(m_pROM, pBuffer, 32768);
}

void CMotherboard::LoadROMCartridge(int cartno, const BYTE* pBuffer)  // Load 24 KB ROM cartridge image
{
    ASSERT(cartno == 1 || cartno == 2);  // Only two cartridges, #1 and #2
    ASSERT(pBuffer != NULL);

    int cartindex = cartno - 1;
    if (m_pROMCart[cartindex] == NULL)
        m_pROMCart[cartindex] = (BYTE*) malloc(24 * 1024);

    memcpy(m_pROMCart[cartindex], pBuffer, 24 * 1024);
}

void CMotherboard::LoadRAM(int plan, const BYTE* pBuffer)  // Load 32 KB RAM image from the buffer
{
    ASSERT(plan >= 0 && plan <= 2);
    memcpy(m_pRAM[plan], pBuffer, 32768);
}


// Floppy ////////////////////////////////////////////////////////////

BOOL CMotherboard::IsFloppyImageAttached(int slot) const
{
    ASSERT(slot >= 0 && slot < 4);
    return m_pFloppyCtl->IsAttached(slot);
}

BOOL CMotherboard::IsFloppyReadOnly(int slot) const
{
    ASSERT(slot >= 0 && slot < 4);
    return m_pFloppyCtl->IsReadOnly(slot);
}

BOOL CMotherboard::IsFloppyEngineOn() const
{
    return m_pFloppyCtl->IsEngineOn();
}

BOOL CMotherboard::AttachFloppyImage(int slot, LPCTSTR sFileName)
{
    ASSERT(slot >= 0 && slot < 4);
    return m_pFloppyCtl->AttachImage(slot, sFileName);
}

void CMotherboard::DetachFloppyImage(int slot)
{
    ASSERT(slot >= 0 && slot < 4);
    m_pFloppyCtl->DetachImage(slot);
}


// ROM cartridge /////////////////////////////////////////////////////

BOOL CMotherboard::IsROMCartridgeLoaded(int cartno) const
{
    ASSERT(cartno == 1 || cartno == 2);  // Only two cartridges, #1 and #2
    int cartindex = cartno - 1;
    return (m_pROMCart[cartindex] != NULL);
}

void CMotherboard::UnloadROMCartridge(int cartno)
{
    ASSERT(cartno == 1 || cartno == 2);  // Only two cartridges, #1 and #2
    int cartindex = cartno - 1;
    if (m_pROMCart[cartindex] != NULL)
    {
        free(m_pROMCart[cartindex]);
        m_pROMCart[cartindex] = NULL;
    }
}


// Hard Drives ///////////////////////////////////////////////////////

BOOL CMotherboard::IsHardImageAttached(int slot) const
{
    ASSERT(slot >= 1 && slot <= 2);
    return (m_pHardDrives[slot - 1] != NULL);
}

BOOL CMotherboard::IsHardImageReadOnly(int slot) const
{
    ASSERT(slot >= 1 && slot <= 2);
    CHardDrive* pHardDrive = m_pHardDrives[slot - 1];
    if (pHardDrive == NULL) return FALSE;
    return pHardDrive->IsReadOnly();
}

BOOL CMotherboard::AttachHardImage(int slot, LPCTSTR sFileName)
{
    ASSERT(slot >= 1 && slot <= 2);

    m_pHardDrives[slot - 1] = new CHardDrive();
    BOOL success = m_pHardDrives[slot - 1]->AttachImage(sFileName);
    if (success)
        m_pHardDrives[slot - 1]->Reset();

    return success;
}
void CMotherboard::DetachHardImage(int slot)
{
    ASSERT(slot >= 1 && slot <= 2);

    delete m_pHardDrives[slot - 1];
    m_pHardDrives[slot - 1] = NULL;
}

WORD CMotherboard::GetHardPortWord(int slot, WORD port)
{
    ASSERT(slot >= 1 && slot <= 2);

    if (m_pHardDrives[slot - 1] == NULL) return 0;
    port = (WORD) (~(port >> 1) & 7) | 0x1f0;
    WORD data = m_pHardDrives[slot - 1]->ReadPort(port);
    return ~data;  // QBUS inverts the bits
}
void CMotherboard::SetHardPortWord(int slot, WORD port, WORD data)
{
    ASSERT(slot >= 1 && slot <= 2);

    if (m_pHardDrives[slot - 1] == NULL) return;
    port = (WORD) (~(port >> 1) & 7) | 0x1f0;
    data = ~data;  // QBUS inverts the bits
    m_pHardDrives[slot - 1]->WritePort(port, data);
}


// Memory control ////////////////////////////////////////////////////

//NOTE: GetRAMWord() and GetRAMByte() are inline, see Processor.h

void CMotherboard::SetRAMWord(int plan, WORD offset, WORD word) 
{
    ASSERT(plan >= 0 && plan <= 2);
    *((WORD*)(m_pRAM[plan] + (offset & 0xFFFE))) = word;
}
void CMotherboard::SetRAMByte(int plan, WORD offset, BYTE byte) 
{
    ASSERT(plan >= 0 && plan <= 2);
    m_pRAM[plan][offset] = byte; 
}

WORD CMotherboard::GetROMWord(WORD offset)
{
    ASSERT(offset < 32768);
    return *((WORD*)(m_pROM + (offset & 0xFFFE))); 
}
BYTE CMotherboard::GetROMByte(WORD offset) 
{ 
    ASSERT(offset < 32768);
    return m_pROM[offset]; 
}

WORD CMotherboard::GetROMCartWord(int cartno, WORD offset)
{
    ASSERT(cartno == 1 || cartno == 2);
    ASSERT(offset < 24 * 1024 - 1);
    int cartindex = cartno - 1;
    if (m_pROMCart[cartindex] == NULL)
        return 0177777;
    WORD* p = (WORD*) (m_pROMCart[cartindex] + (offset & 0xFFFE));
    return *p;
}
BYTE CMotherboard::GetROMCartByte(int cartno, WORD offset)
{
    ASSERT(cartno == 1 || cartno == 2);
    ASSERT(offset < 24 * 1024);
    int cartindex = cartno - 1;
    if (m_pROMCart[cartindex] == NULL)
        return 0377;
    BYTE* p = m_pROMCart[cartindex] + offset;
    return *p;
}


//////////////////////////////////////////////////////////////////////


void CMotherboard::ResetFloppy()
{
    m_pFloppyCtl->Reset();
}

void CMotherboard::Tick8000 ()
{
    m_pCPU->Execute();
}
void CMotherboard::Tick6250 ()
{
    m_pPPU->Execute();
}
void CMotherboard::Tick50 ()
{
    if((m_pPPU->GetMemoryController()->GetPortView(0177054)&0400)==0)
        m_pPPU->TickEVNT();
    if((m_pPPU->GetMemoryController()->GetPortView(0177054)&01000)==0)
        m_pCPU->TickEVNT();
}

void CMotherboard::ExecuteCPU ()
{
    m_pCPU->Execute();
}

void CMotherboard::ExecutePPU ()
{
    m_pPPU->Execute();
}

void CMotherboard::TimerTick() // Timer Tick, 2uS -- dividers are within timer routine
{
    if ((m_timerflags & 1) == 0)  // Timer is off
        return;
    if ((m_timerflags & 040) != 0)  // External event is ready
        return;

    m_timerdivider++;

    int flag = 0;
    switch((m_timerflags >> 1) & 3)
    {
        case 0: //2uS
            flag = 1;
            m_timerdivider = 0;
            break;
        case 1: //4uS
            if (m_timerdivider >= 2)
            {
                flag = 1;
                m_timerdivider = 0;
            }
            break;
        case 2: //8uS
            if (m_timerdivider >= 4)
            {
                flag = 1;
                m_timerdivider = 0;
            }
            break;
        case 3:
            if (m_timerdivider >= 8)
            {
                flag = 1;
                m_timerdivider = 0;
            }
            break;
    }

    if (flag == 0)  // Nothing happened
        return; 

    m_timer--;
    m_timer &= 07777;  // 12 bit only

    if (m_timer == 0)
    {
        if(m_timerflags & 0200)
            m_timerflags |= 010;  // Overflow
        m_timerflags |= 0200;  // 0

        if ((m_timerflags & 0100) && (m_timerflags & 0200))
        {
            m_pPPU->InterruptVIRQ(2, 0304); 
        }

        m_timer = m_timerreload & 07777; // Reload it
    }
}
WORD CMotherboard::GetTimerValue()  // Returns current timer value
{
    if ((m_timerflags & 0240) == 0)
        return m_timer;
    
    m_timerflags &= ~0240;  // Clear flags
    WORD res = m_timer;
    m_timer = m_timerreload & 07777; // Reload it
    return res;
}
WORD CMotherboard::GetTimerReload()  // Returns timer reload value
{
    return m_timerreload;
}
WORD CMotherboard::GetTimerState() // Returns timer state
{
    WORD res = m_timerflags;
    m_timerflags &= ~010;  // Clear overflow
    return res;
}

void CMotherboard::SetTimerReload(WORD val)	 // Sets timer reload value
{
    m_timerreload = val & 07777;
    if ((m_timerflags & 1) == 0)
        m_timer = m_timerreload;
}

void CMotherboard::SetTimerState(WORD val) // Sets timer state
{
    // 753   200 40 10
    if ((val & 1) && ((m_timerflags & 1) == 0))
        m_timer = m_timerreload & 07777;

    m_timerflags &= 0250;  // Clear everything but bits 7,5,3
    m_timerflags |= (val & (~0250));  // Preserve bits 753

    switch((m_timerflags >> 1) & 3)
    {
        case 0: //2uS
            m_multiply=8;
            break;
        case 1: //4uS
            m_multiply=4;
            break;
        case 2: //8uS
            m_multiply=2;
            break;
        case 3:
            m_multiply=1;
            break;
    }
}

void CMotherboard::DebugTicks()
{
    if (!m_pPPU->IsStopped())
    {
        while (m_pPPU->InterruptProcessing()) {}
        m_pPPU->CommandExecution();
        while (m_pPPU->InterruptProcessing()) {}
    }
    if (!m_pCPU->IsStopped())
    {
        while (m_pCPU->InterruptProcessing()) {}
        m_pCPU->CommandExecution();
        while (m_pCPU->InterruptProcessing()) {}
    }
    if (!m_pPPU->IsStopped()) while (m_pPPU->InterruptProcessing()) {}
}

/*
Каждый фрейм равен 1/25 секунды = 40 мс = 20000 тиков, 1 тик = 2 мкс.

* 20000 тиков системного таймера - на каждый 1-й тик
* 2 сигнала EVNT, в 0-й и 10000-й тик фрейма
* 320000 тиков ЦП - 16 раз за один тик
* 250000 тиков ПП - 12.5 раз за один тик
* Отрисовка 288 видимых строк, по 32 тика на строку (только в первой половине фрейма)
** Первая невидимая строка (#0) начинает рисоваться на 96-ой тик
** Первая видимая строка (#18) начинает рисоваться на 672-й тик
* 625 тиков FDD - каждый 32-й тик
* 48 тиков обмена с COM-портом - каждый 416 тик
*/
#define SYSTEMFRAME_EXECUTE_CPU     { if (m_pCPU->GetPC() == m_CPUbp) return FALSE;  m_pCPU->Execute(); }
#define SYSTEMFRAME_EXECUTE_PPU     { if (m_pPPU->GetPC() == m_PPUbp) return FALSE;  m_pPPU->Execute(); }
BOOL CMotherboard::SystemFrame()
{
    int frameticks = 0;  // 20000 ticks
    const int audioticks = 20286 / (SAMPLERATE / 25);
    const int serialOutTicks = 20000 / (9600 / 25);
    int serialTxCount = 0;

    int tapeSamplesPerFrame = 1, tapeBrasErr = 0;
    if (m_TapeReadCallback != NULL || m_TapeWriteCallback != NULL)
    {
        tapeSamplesPerFrame = m_nTapeSampleRate / 25;
        tapeBrasErr = 0;
    }

    do
    {
        TimerTick();  // System timer tick

        if (frameticks % 10000 == 0)
            Tick50();  // 1/50 timer event

        // CPU - 16 times, PPU - 12.5 times
        /*  0 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;  SYSTEMFRAME_EXECUTE_CPU;
        /*  1 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;
        /*  2 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;
        /*  3 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;  SYSTEMFRAME_EXECUTE_CPU;
        /*  4 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;
        /*  5 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;
        /*  6 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;  SYSTEMFRAME_EXECUTE_CPU;
        /*  7 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;
        /*  8 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;
        /*  9 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;  SYSTEMFRAME_EXECUTE_CPU;
        /* 10 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;
        /* 11 */  SYSTEMFRAME_EXECUTE_CPU;  SYSTEMFRAME_EXECUTE_PPU;
        if ((frameticks & 1) == 0)  // (frameticks % 2 == 0) PPU extra ticks
            SYSTEMFRAME_EXECUTE_PPU;

        if ((frameticks & 31) == 0)  // (frameticks % 32 == 0)
        {
            m_pFloppyCtl->Periodic();  // Each 32nd tick -- FDD tick

            // Keyboard processing
            CSecondMemoryController* pMemCtl = (CSecondMemoryController*) m_pSecondMemCtl;
            if ((pMemCtl->m_Port177700 & 0200) == 0)
            {
                BYTE row_Y = m_scanned_key & 0xF;
                BYTE col_X = (m_scanned_key & 0x70) >> 4;
                BYTE bit_X = 1 << col_X;
                pMemCtl->m_Port177702 = m_scanned_key;
                if ((m_scanned_key & 0200) == 0)
                {
                    if ((m_kbd_matrix[row_Y].processed == FALSE) && ((m_kbd_matrix[row_Y].row_Y & bit_X) != 0))
                    {
                        pMemCtl->m_Port177700 |= 0200;
                        m_kbd_matrix[row_Y].processed = TRUE;
                        if (pMemCtl->m_Port177700 & 0100)
                            m_pPPU->InterruptVIRQ(3, 0300);
                    }
                }
                else
                {
                    if ((m_kbd_matrix[row_Y].processed == TRUE) && (m_kbd_matrix[row_Y].row_Y == 0))
                    {
                        pMemCtl->m_Port177700 |= 0200;
                        m_kbd_matrix[row_Y].processed = FALSE;
                        if (pMemCtl->m_Port177700 & 0100)
                            m_pPPU->InterruptVIRQ(3, 0300);
                        pMemCtl->m_Port177702 = m_scanned_key & 0x8F;
                    }
                }
                m_scanned_key++;
            }
        }

        if (m_pHardDrives[0] != NULL)
            m_pHardDrives[0]->Periodic();
        if (m_pHardDrives[1] != NULL)
            m_pHardDrives[1]->Periodic();

        if (frameticks % audioticks == 0) //AUDIO tick
            DoSound();

        if (m_TapeReadCallback != NULL || m_TapeWriteCallback != NULL)
        {
            tapeBrasErr += tapeSamplesPerFrame;
            if (2 * tapeBrasErr >= 20000)
            {
                tapeBrasErr -= 20000;

                if (m_TapeReadCallback != NULL)  // Tape reading
                {
                    BOOL tapeBit = (*m_TapeReadCallback)(1);
                    CSecondMemoryController* pMemCtl = (CSecondMemoryController*) m_pSecondMemCtl;
                    if (pMemCtl->TapeInput(tapeBit))
                    {
                        m_timerflags |= 040;  // Set bit 5 of timer state: external event ready to read
                    }
                }
                else if (m_TapeWriteCallback != NULL)  // Tape writing
                {
                    CSecondMemoryController* pMemCtl = (CSecondMemoryController*) m_pSecondMemCtl;
                    unsigned int value = pMemCtl->TapeOutput() ? 0xffffffff : 0;
                    (*m_TapeWriteCallback)(value, 1);
                }
            }
        }

        if (m_SerialInCallback != NULL && frameticks % 416 == 0)
        {
            CFirstMemoryController* pMemCtl = (CFirstMemoryController*) m_pFirstMemCtl;
            if ((pMemCtl->m_Port176574 & 004) == 0)  // Not loopback?
            {
                BYTE b;
                if (m_SerialInCallback(&b))
                {
                    if (pMemCtl->SerialInput(b) && (pMemCtl->m_Port176570 & 0100))
                        m_pCPU->InterruptVIRQ(3, 0370);
                }
            }
        }
        if (m_SerialOutCallback != NULL && frameticks % serialOutTicks)
        {
            CFirstMemoryController* pMemCtl = (CFirstMemoryController*) m_pFirstMemCtl;
            if (serialTxCount > 0)
            {
                serialTxCount--;
                if (serialTxCount == 0)  // Translation countdown finished - the byte translated
                {
                    if ((pMemCtl->m_Port176574 & 004) == 0)  // Not loopback?
                        (*m_SerialOutCallback)((BYTE)(pMemCtl->m_Port176576 & 0xff));
                    else  // Loopback
                    {
                        if (pMemCtl->SerialInput((BYTE)(pMemCtl->m_Port176576 & 0xff)) && (pMemCtl->m_Port176570 & 0100))
                            m_pCPU->InterruptVIRQ(3, 0370);
                    }
                    pMemCtl->m_Port176574 |= 0200;  // Set Ready flag
                    if (pMemCtl->m_Port176574 & 0100)  // Interrupt?
                         m_pCPU->InterruptVIRQ(3, 0374);
                }
            }
            else if ((pMemCtl->m_Port176574 & 0200) == 0)  // Ready is 0?
            {
                serialTxCount = 8;  // Start translation countdown
            }
        }

        if (m_ParallelOutCallback != NULL)
        {
            CSecondMemoryController* pMemCtl = (CSecondMemoryController*) m_pSecondMemCtl;
            if ((pMemCtl->m_Port177102 & 0x80) == 0x80 && (pMemCtl->m_Port177101 & 0x80) == 0x80)
            {  // Strobe set, Printer Ack set => reset Printer Ack
                pMemCtl->m_Port177101 &= ~0x80;
                // Now printer waits for a next byte
            }
            else if ((pMemCtl->m_Port177102 & 0x80) == 0 && (pMemCtl->m_Port177101 & 0x80) == 0)
            {  // Strobe reset, Printer Ack reset => byte is ready, print it
                (*m_ParallelOutCallback)(pMemCtl->m_Port177100);
                pMemCtl->m_Port177101 |= 0x80;  // Set Printer Acknowledge
                // Now the printer waits for Strobe
            }
        }

        frameticks++;
    }
    while (frameticks < 20000);

    return TRUE;
}

// Key pressed or released
void CMotherboard::KeyboardEvent(BYTE scancode, BOOL okPressed)
{
    BYTE row_Y = scancode & 0xF;
    BYTE col_X = (scancode & 0x70) >> 4;
    BYTE bit_X = 1 << col_X;
    if (okPressed)
        m_kbd_matrix[row_Y].row_Y |= bit_X;
    else
        m_kbd_matrix[row_Y].row_Y &= ~bit_X;
}


//////////////////////////////////////////////////////////////////////
//
// Emulator image format:
//  Offset Size
//     0    32 bytes  - Header
//    32   128 bytes  - Board status
//   160    64 bytes  - CPU status
//   224    64 bytes  - PPU status
//   288    64 bytes  - CPU memory/IO
//   352    64 bytes  - PPU memory/IO
//   416    96 bytes  - reserved
//   512    --        - end of the header
//   512   32 Kbytes  - ROM image
//         64 Kbytes * 3  - RAM planes 0, 1, 2
//TODO: 256 bytes * 2 - Cartridge 1..2 path
//TODO: 256 bytes * 4 - Floppy 1..4 path
//TODO: 256 bytes * 2 - Hard 1..2 path
//TODO: Floppy drive state
//TODO: Hard drive state

void CMotherboard::SaveToImage(BYTE* pImage)
{
    // Board data                                       // Offset Size
    WORD* pwImage = (WORD*) (pImage + 32);              //   32    --
    *pwImage++ = m_timer;                               //   32     2
    *pwImage++ = m_timerreload;                         //   34     2
    *pwImage++ = m_timerflags;                          //   36     2
    *pwImage++ = m_timerdivider;                        //   38     2
    DWORD* pdwImage = (DWORD*)pwImage;                  //   40    --
    memcpy(pdwImage, m_chancputx, 3 * 4); pdwImage += 3;//   40    12
    memcpy(pdwImage, m_chancpurx, 2 * 4); pdwImage += 2;//   52     8
    memcpy(pdwImage, m_chanpputx, 2 * 4); pdwImage += 2;//   60     8
    memcpy(pdwImage, m_chanppurx, 3 * 4); pdwImage += 3;//   68    12
    BYTE* pbImage = (BYTE*) pdwImage;                   //   80    --
    *pbImage++ = m_chan0disabled;                       //   80     1
    *pbImage++ = m_irq_cpureset;                        //   81     1
    *pbImage++ = 0;                                     //   82     1  // not used
    *pbImage++ = m_scanned_key;                         //   83     1
    memcpy(pbImage, m_kbd_matrix, 2*16); pbImage += 32; //   84    32
    pwImage = (WORD*) pbImage;                          //  116    --
    *pwImage++ = m_multiply;                            //  116     2
    memcpy(pwImage, freq_per, 12); pwImage += 6;        //  118    12
    memcpy(pwImage, freq_out, 12); pwImage += 6;        //  130    12
    memcpy(pwImage, freq_enable, 12); pwImage += 6;     //  142    12

    // CPU status
    BYTE* pImageCPU = pImage + 160;
    m_pCPU->SaveToImage(pImageCPU);
    // PPU status
    BYTE* pImagePPU = pImageCPU + 64;
    m_pPPU->SaveToImage(pImagePPU);
    // CPU memory/IO controller status
    BYTE* pImageCpuMem = pImagePPU + 64;
    m_pFirstMemCtl->SaveToImage(pImageCpuMem);
    // PPU memory/IO controller status
    BYTE* pImagePpuMem = pImageCpuMem + 64;
    m_pSecondMemCtl->SaveToImage(pImagePpuMem);

    // ROM
    BYTE* pImageRom = pImage + UKNCIMAGE_HEADER_SIZE;
    memcpy(pImageRom, m_pROM, 32 * 1024);
    // RAM planes 0, 1, 2
    BYTE* pImageRam = pImageRom + 32 * 1024;
    memcpy(pImageRam, m_pRAM[0], 64 * 1024);
    pImageRam += 64 * 1024;
    memcpy(pImageRam, m_pRAM[1], 64 * 1024);
    pImageRam += 64 * 1024;
    memcpy(pImageRam, m_pRAM[2], 64 * 1024);
    pImageRam += 64 * 1024;
    ASSERT((pImageRam - pImage) == UKNCIMAGE_SIZE);
}
void CMotherboard::LoadFromImage(const BYTE* pImage)
{
    // Board data                                       // Offset Size
    const WORD* pwImage = (const WORD*) (pImage + 32);  //   32    --
    m_timer = *pwImage++;                               //   32     2
    m_timerreload = *pwImage++;                         //   34     2
    m_timerflags = *pwImage++;                          //   36     2
    m_timerdivider = *pwImage++;                        //   38     2
    DWORD* pdwImage = (DWORD*)pwImage;                  //   40    --
    memcpy(m_chancputx, pdwImage, 3 * 4); pdwImage += 3;//   40    12
    memcpy(m_chancpurx, pdwImage, 2 * 4); pdwImage += 2;//   52     8
    memcpy(m_chanpputx, pdwImage, 2 * 4); pdwImage += 2;//   60     8
    memcpy(m_chanppurx, pdwImage, 3 * 4); pdwImage += 3;//   68    12
    const BYTE* pbImage = (const BYTE*) pdwImage;       //   80    --
    m_chan0disabled = *pbImage++;                       //   80     1
    m_irq_cpureset = *pbImage++;                        //   81     1
    pbImage++;                                          //   82     1  // not used
    m_scanned_key = *pbImage++;                         //   83     1
    memcpy(m_kbd_matrix, pbImage, 2*16); pbImage += 32; //   84    32
    pwImage = (const WORD*) pbImage;                    //  116    --
    m_multiply = *pwImage++;                            //  116     2
    memcpy(freq_per, pwImage, 12); pwImage += 6;        //  118    12
    memcpy(freq_out, pwImage, 12); pwImage += 6;        //  130    12
    memcpy(freq_enable, pwImage, 12); pwImage += 6;     //  142    12

    // CPU status
    const BYTE* pImageCPU = pImage + 160;               //  160    32
    m_pCPU->LoadFromImage(pImageCPU);
    // PPU status
    const BYTE* pImagePPU = pImageCPU + 64;             //  224    32
    m_pPPU->LoadFromImage(pImagePPU);
    // CPU memory/IO controller status
    const BYTE* pImageCpuMem = pImagePPU + 64;          //  288    64
    m_pFirstMemCtl->LoadFromImage(pImageCpuMem);
    // PPU memory/IO controller status
    const BYTE* pImagePpuMem = pImageCpuMem + 64;       //  352    64
    m_pSecondMemCtl->LoadFromImage(pImagePpuMem);

    // ROM
    const BYTE* pImageRom = pImage + UKNCIMAGE_HEADER_SIZE; // 512
    memcpy(m_pROM, pImageRom, 32 * 1024);
    // RAM planes 0, 1, 2
    const BYTE* pImageRam = pImageRom + 32 * 1024;
    memcpy(m_pRAM[0], pImageRam, 64 * 1024);
    pImageRam += 64 * 1024;
    memcpy(m_pRAM[1], pImageRam, 64 * 1024);
    pImageRam += 64 * 1024;
    memcpy(m_pRAM[2], pImageRam, 64 * 1024);
    pImageRam += 64 * 1024;
    ASSERT((pImageRam - pImage) == UKNCIMAGE_SIZE);
}

void CMotherboard::ChanWriteByCPU(BYTE chan, BYTE data)
{
    BYTE oldp_ready = m_chanppurx[chan].ready;
    chan &= 3;
    ASSERT(chan<3);

//	if((chan==0)&&(m_chan0disabled))
//		return;

    m_chanppurx[chan].data = data;
    m_chanppurx[chan].ready = 1;
    m_chancputx[chan].ready = 0;
    m_chancputx[chan].rdwr = 1;
    m_pCPU->InterruptVIRQ((chan==2)?5:(2+chan*2),0);
    if((m_chanppurx[chan].irq) && (oldp_ready==0))
    {
        m_chanppurx[chan].rdwr = 0;
        m_pPPU->InterruptVIRQ(5+chan*2, 0320+(010*chan));
    }
}
void CMotherboard::ChanWriteByPPU(BYTE chan, BYTE data)
{
    BYTE oldc_ready = m_chancpurx[chan].ready;
    chan &= 3;
    ASSERT(chan<2); 

//	if((chan==0)&&(m_chan0disabled))
//		return;

    m_chancpurx[chan].data = data;
    m_chancpurx[chan].ready = 1;
    m_chanpputx[chan].ready = 0;
    m_chanpputx[chan].rdwr = 1;
    m_pPPU->InterruptVIRQ((chan==0)?6:8,0);
    if((m_chancpurx[chan].irq) && (oldc_ready==0))
    {
        m_chancpurx[chan].rdwr = 0;
        m_pCPU->InterruptVIRQ(chan?3:1, chan?0460:060);
    }
}
BYTE CMotherboard::ChanReadByCPU(BYTE chan)
{
    BYTE res,oldp_ready = m_chanpputx[chan].ready;

    chan &= 3;
    ASSERT(chan<2); 

//	if((chan==0)&&(m_chan0disabled))
//		return 0;

    res = m_chancpurx[chan].data;
    m_chancpurx[chan].ready = 0;
    m_chancpurx[chan].rdwr = 1;
    m_chanpputx[chan].ready = 1;
    m_pCPU->InterruptVIRQ(chan*2+1,0);
    if((m_chanpputx[chan].irq) && (oldp_ready==0))
    {
        m_chanpputx[chan].rdwr = 0;
        m_pPPU->InterruptVIRQ(chan?8:6, chan?0334:0324);
    }
    return res;
}
BYTE CMotherboard::ChanReadByPPU(BYTE chan)
{
    BYTE res,oldc_ready = m_chancputx[chan].ready;

    chan &= 3;
    ASSERT(chan<3); 

    //if((chan==0)&&(m_chan0disabled))
    //	return 0;

    res = m_chanppurx[chan].data;
    m_chanppurx[chan].ready = 0;
    m_chanppurx[chan].rdwr = 1;
    m_chancputx[chan].ready = 1;
    m_pPPU->InterruptVIRQ(chan*2+5,0);
    if((m_chancputx[chan].irq) && (oldc_ready==0))
    {
        m_chancputx[chan].rdwr = 0;
        switch(chan)
        {
        case 0:
            m_pCPU->InterruptVIRQ(2, 064);
            break;
        case 1:
            m_pCPU->InterruptVIRQ(4, 0464);
            break;
        case 2:
            m_pCPU->InterruptVIRQ(5, 0474);
            break;
        }
    }

    return res;
}

BYTE CMotherboard::ChanRxStateGetCPU(BYTE chan)
{
    chan &= 3;
    ASSERT(chan<2);
    
    return (m_chancpurx[chan].ready<<7)|(m_chancpurx[chan].irq<<6);
}

BYTE CMotherboard::ChanTxStateGetCPU(BYTE chan)
{
    chan &= 3;
    ASSERT(chan<3);
    return (m_chancputx[chan].ready<<7)|(m_chancputx[chan].irq<<6);
}

BYTE CMotherboard::ChanRxStateGetPPU()
{
    BYTE res;

    res = (m_irq_cpureset<<6) | (m_chanppurx[2].ready<<5) | (m_chanppurx[1].ready<<4) | (m_chanppurx[0].ready<<3) | 
           (m_chanppurx[2].irq<<2)   | (m_chanppurx[1].irq<<1)   | (m_chanppurx[0].irq);


    return res;
}
BYTE CMotherboard::ChanTxStateGetPPU()
{
    BYTE res;
    res = (m_chanpputx[1].ready<<4) | (m_chanpputx[0].ready<<3) | (m_chan0disabled<<2) |
            (m_chanpputx[1].irq<<1)   | (m_chanpputx[0].irq);


    return res;
}
void CMotherboard::ChanRxStateSetCPU(BYTE chan, BYTE state)
{
    BYTE oldc_irq = m_chancpurx[chan].irq;
    chan &= 3;
    ASSERT(chan<2);

    if(state&0100) //irq
        m_chancpurx[chan].irq = 1;
    else
    {
        m_chancpurx[chan].irq = 0;
        if ((chan==0) || (m_pCPU->GetVIRQ(chan?3:1))) m_chancpurx[chan].rdwr = 1;
        m_pCPU->InterruptVIRQ(chan?3:1, 0);
    }
    if((m_chancpurx[chan].irq) && (m_chancpurx[chan].ready) && (oldc_irq==0) && (m_chancpurx[chan].rdwr))
    {
        m_chancpurx[chan].rdwr = 0;
        m_pCPU->InterruptVIRQ(chan?3:1, chan?0460:060);
    }
}
void CMotherboard::ChanTxStateSetCPU(BYTE chan, BYTE state)
{
    BYTE oldc_irq = m_chancputx[chan].irq;
    chan &= 3;
    ASSERT(chan<3);

    if(state&0100) //irq
        m_chancputx[chan].irq = 1;
    else
    {
        m_chancputx[chan].irq = 0;
        if ((chan==0) || (m_pCPU->GetVIRQ((chan==2)?5:(chan*2+2)))) m_chancputx[chan].rdwr = 1;
        m_pCPU->InterruptVIRQ((chan==2)?5:(chan*2+2),0);
    }

    if((m_chancputx[chan].irq) && (m_chancputx[chan].ready) && (oldc_irq==0) && (m_chancputx[chan].rdwr))
    {
        m_chancputx[chan].rdwr = 0;
        switch(chan)
        {
        case 0:
            m_pCPU->InterruptVIRQ(2, 064);
            break;
        case 1:
            m_pCPU->InterruptVIRQ(4, 0464);
            break;
        case 2:
            m_pCPU->InterruptVIRQ(5, 0474);
            break;
        }
    }
}

void CMotherboard::ChanRxStateSetPPU(BYTE state)
{
    BYTE oldp_irq0 = m_chanppurx[0].irq;
    BYTE oldp_irq1 = m_chanppurx[1].irq;
    BYTE oldp_irq2 = m_chanppurx[2].irq;

    m_chanppurx[0].irq = state&1;
    m_chanppurx[1].irq = (state>>1)&1;
    m_chanppurx[2].irq = (state>>2)&1;
    m_irq_cpureset = (state>>6)&1;

    if (m_chanppurx[0].irq==0)
    {
        if (m_pPPU->GetVIRQ(5)) m_chanppurx[0].rdwr = 1;
        m_pPPU->InterruptVIRQ(5, 0);
    }
    if (m_chanppurx[1].irq==0)
    {
        if (m_pPPU->GetVIRQ(7)) m_chanppurx[1].rdwr = 1;
        m_pPPU->InterruptVIRQ(7, 0);
    }
    if (m_chanppurx[2].irq==0)
    {
        if (m_pPPU->GetVIRQ(9)) m_chanppurx[2].rdwr = 1;
        m_pPPU->InterruptVIRQ(9, 0);
    }
    
    if((m_chanppurx[0].irq) && (m_chanppurx[0].ready) && (oldp_irq0==0) && (m_chanppurx[0].rdwr))
    {
        m_chanppurx[0].rdwr = 0;
        m_pPPU->InterruptVIRQ(5, 0320);
    }
    if((m_chanppurx[1].irq) && (m_chanppurx[1].ready) && (oldp_irq1==0) && (m_chanppurx[1].rdwr))
    {
        m_chanppurx[1].rdwr = 0;
        m_pPPU->InterruptVIRQ(7, 0330);
    }
    if((m_chanppurx[2].irq) && (m_chanppurx[2].ready) && (oldp_irq2==0) && (m_chanppurx[2].rdwr))
    {
        m_chanppurx[2].rdwr = 0;
        m_pPPU->InterruptVIRQ(9, 0340);
    }

}
void CMotherboard::ChanTxStateSetPPU(BYTE state)
{
    BYTE oldp_irq0 = m_chanpputx[0].irq;
    BYTE oldp_irq1 = m_chanpputx[1].irq;

    m_chanpputx[0].irq = state&1;
    m_chanpputx[1].irq = (state>>1)&1;
    m_chan0disabled = (state>>2)&1;

    if (m_chanpputx[0].irq==0)
    {
        if (m_pPPU->GetVIRQ(6)) m_chanpputx[0].rdwr = 1;
        m_pPPU->InterruptVIRQ(6, 0);
    }
    if (m_chanpputx[1].irq==0)
    {
        if (m_pPPU->GetVIRQ(8)) m_chanpputx[1].rdwr = 1;
        m_pPPU->InterruptVIRQ(8, 0);
    }

    if((m_chanpputx[0].irq) && (m_chanpputx[0].ready) && (oldp_irq0==0) && (m_chanpputx[0].rdwr))
    {
        m_chanpputx[0].rdwr = 0;
        m_pPPU->InterruptVIRQ(6, 0324);
    }
    if((m_chanpputx[1].irq) && (m_chanpputx[1].ready) && (oldp_irq1==0) && (m_chanpputx[1].rdwr))
    {
        m_chanpputx[1].rdwr = 0;
        m_pPPU->InterruptVIRQ(8, 0334);
    }
}

void CMotherboard::ChanResetByCPU()
{
    m_chancpurx[0].ready = 0;
    m_chancpurx[0].irq = 0;
    m_chancpurx[0].rdwr = 1;
    m_pCPU->InterruptVIRQ(1, 0);
    m_chanpputx[0].ready = 1;
    if (m_chanpputx[0].irq)
    {
        m_chanpputx[0].rdwr = 0;
        m_pPPU->InterruptVIRQ(6, 0324);
    }

    m_chancputx[0].ready = 1;
    m_chancputx[0].irq = 0;
    m_chancputx[0].rdwr = 1;
    m_pCPU->InterruptVIRQ(2, 0);
    m_chanppurx[0].ready = 0;
    m_chanppurx[0].rdwr = 1;
    m_pPPU->InterruptVIRQ(5, 0);

    m_chancpurx[1].ready = 0;
    m_chancpurx[1].irq = 0;
    m_chancpurx[1].rdwr = 1;
    m_pCPU->InterruptVIRQ(3, 0);
    m_chanpputx[1].ready = 1;
    if (m_chanpputx[1].irq)
    {
        m_chanpputx[1].rdwr = 0;
        m_pPPU->InterruptVIRQ(8, 0334);
    }

    m_chancputx[1].ready = 1;
    m_chancputx[1].irq = 0;
    m_chancputx[1].rdwr = 1;
    m_pCPU->InterruptVIRQ(4, 0);
    m_chanppurx[1].ready = 0;
    m_chanppurx[1].rdwr = 1;
    m_pPPU->InterruptVIRQ(7, 0);

    m_chancputx[2].ready = 1;
    m_chancputx[2].irq = 0;
    m_chancputx[2].rdwr = 1;
    m_pCPU->InterruptVIRQ(5, 0);
    m_chanppurx[2].ready = 0;
    m_chanppurx[2].rdwr = 1;
    m_pPPU->InterruptVIRQ(9, 0);

    if (m_irq_cpureset)
        m_pPPU->InterruptVIRQ(4, 0314);
}

void CMotherboard::ChanResetByPPU()
{
    m_chanppurx[0].ready = 0;
    m_chanppurx[0].irq = 0;
    m_chanppurx[0].rdwr = 1;
    m_pPPU->InterruptVIRQ(5, 0);
    m_chancputx[0].ready = 1;
    if (m_chancputx[0].irq)
    {
        m_chancputx[0].rdwr = 0;
        m_pCPU->InterruptVIRQ(2, 064);
    }

    m_chanpputx[0].ready = 1;
    m_chanpputx[0].irq = 0;
    m_chanpputx[0].rdwr = 1;
    m_pPPU->InterruptVIRQ(6, 0);
    m_chancpurx[0].ready = 0;
    m_chancpurx[0].rdwr = 1;
    m_pCPU->InterruptVIRQ(1, 0);

    m_chanppurx[1].ready = 0;
    m_chanppurx[1].irq = 0;
    m_chanppurx[1].rdwr = 1;
    m_pPPU->InterruptVIRQ(7, 0);
    m_chancputx[1].ready = 1;
    if (m_chancputx[1].irq)
    {
        m_chancputx[1].rdwr = 0;
        m_pCPU->InterruptVIRQ(4, 0464);
    }

    m_chanpputx[1].ready = 1;
    m_chanpputx[1].irq = 0;
    m_chanpputx[1].rdwr = 1;
    m_pPPU->InterruptVIRQ(8, 0);
    m_chancpurx[1].ready = 0;
    m_chancpurx[1].rdwr = 1;
    m_pCPU->InterruptVIRQ(3, 0);

    m_chanppurx[2].ready = 0;
    m_chanppurx[2].irq = 0;
    m_chanppurx[2].rdwr = 1;
    m_pPPU->InterruptVIRQ(9, 0);
    m_chancputx[2].ready = 1;
    if (m_chancputx[2].irq)
    {
        m_chancputx[2].rdwr = 0;
        m_pCPU->InterruptVIRQ(5, 0474);
    }

    m_irq_cpureset = 0;
    m_pPPU->InterruptVIRQ(4, 0);
}

//void CMotherboard::FloppyDebug(BYTE val)
//{
////#if !defined(PRODUCT)
////    TCHAR buffer[512];
////#endif
///*
//m_floppyaddr=0;
//m_floppystate=FLOPPY_FSM_WAITFORLSB;
//#define FLOPPY_FSM_WAITFORLSB	0
//#define FLOPPY_FSM_WAITFORMSB	1
//#define FLOPPY_FSM_WAITFORTERM1	2
//#define FLOPPY_FSM_WAITFORTERM2	3
//
//*/
//	switch(m_floppystate)
//	{
//		case FLOPPY_FSM_WAITFORLSB:
//			if(val!=0xff)
//			{
//				m_floppyaddr=val;
//				m_floppystate=FLOPPY_FSM_WAITFORMSB;
//			}
//			break;
//		case FLOPPY_FSM_WAITFORMSB:
//			if(val!=0xff)
//			{
//				m_floppyaddr|=val<<8;
//				m_floppystate=FLOPPY_FSM_WAITFORTERM1;
//			}
//			else
//			{
//				m_floppystate=FLOPPY_FSM_WAITFORLSB;
//			}
//			break;
//		case FLOPPY_FSM_WAITFORTERM1:
//			if(val==0xff)
//			{ //done
//				WORD par;
//				BYTE trk,sector,side;
//
//				par=m_pFirstMemCtl->GetWord(m_floppyaddr,0);
//
////#if !defined(PRODUCT)
////				wsprintf(buffer,_T(">>>>FDD Cmd %d "),(par>>8)&0xff);
////				DebugPrint(buffer);
////#endif
//                par=m_pFirstMemCtl->GetWord(m_floppyaddr+2,0);
//				side=par&0x8000?1:0;
////#if !defined(PRODUCT)
////				wsprintf(buffer,_T("Side %d Drv %d, Type %d "),par&0x8000?1:0,(par>>8)&0x7f,par&0xff);
////				DebugPrint(buffer);
////#endif
//				par=m_pFirstMemCtl->GetWord(m_floppyaddr+4,0);
//				sector=(par>>8)&0xff;
//				trk=par&0xff;
////#if !defined(PRODUCT)
////				wsprintf(buffer,_T("Sect %d, Trk %d "),(par>>8)&0xff,par&0xff);
////				DebugPrint(buffer);
////				PrintOctalValue(buffer,m_pFirstMemCtl->GetWord(m_floppyaddr+6,0));
////				DebugPrint(_T("Addr "));
////				DebugPrint(buffer);
////#endif
//				par=m_pFirstMemCtl->GetWord(m_floppyaddr+8,0);
////#if !defined(PRODUCT)
////				wsprintf(buffer,_T(" Block %d Len %d\n"),trk*20+side*10+sector-1,par);
////				DebugPrint(buffer);
////#endif
//				
//				m_floppystate=FLOPPY_FSM_WAITFORLSB;
//			}
//			break;
//	
//	}
//}


WORD CMotherboard::GetFloppyState()
{
    return m_pFloppyCtl->GetState();
}
WORD CMotherboard::GetFloppyData()
{
    return m_pFloppyCtl->GetData();
}
void CMotherboard::SetFloppyState(WORD val)
{
    m_pFloppyCtl->SetCommand(val);
}
void CMotherboard::SetFloppyData(WORD val)
{
    m_pFloppyCtl->WriteData(val);
}


//////////////////////////////////////////////////////////////////////

WORD CMotherboard::GetKeyboardRegister(void)
{
    WORD w7214 = GetRAMWord(0,07214);
    BYTE b22556 = GetRAMByte(0,022556);

    WORD res = 0;
    switch (w7214)
    {
        case 010534: //fix
        case 07234:  //main
            res=(b22556&0200)?KEYB_RUS:KEYB_LAT;
            break;
        case 07514: //lower register
            res=(b22556&0200)?(KEYB_RUS|KEYB_LOWERREG):(KEYB_LAT|KEYB_LOWERREG);
            break;
        case 07774: //graph
            res=KEYB_RUS;
            break;
        case 010254: //control
            res=KEYB_LAT;
            break;
        default:
            res=KEYB_LAT;
            break;
    }
    return res;
}

void CMotherboard::DoSound(void)
{
    int global;

    freq_out[0]=(m_timer>>3)&1; //8000
    if(m_multiply>=4)
        freq_out[0]=0;

    freq_out[1]=(m_timer>>6)&1;//1000

    freq_out[2]=(m_timer>>7)&1;//500
    freq_out[3]=(m_timer>>8)&1;//250
    freq_out[4]=(m_timer>>10)&1;//60

    global=0;
    global= !(freq_out[0]&freq_enable[0]) & ! (freq_out[1]&freq_enable[1]) & !(freq_out[2]&freq_enable[2]) & !(freq_out[3]&freq_enable[3]) & !(freq_out[4]&freq_enable[4]);
    if(freq_enable[5]==0)
        global=0;
    else
    {
        if( (!freq_enable[0]) && (!freq_enable[1]) && (!freq_enable[2]) && (!freq_enable[3]) && (!freq_enable[4]))
            global=1;
    }

    if (m_SoundGenCallback != NULL)
    {
        if (global)
            (*m_SoundGenCallback)(0x7fff,0x7fff);
        else
            (*m_SoundGenCallback)(0x0000,0x0000);
    }
}

void CMotherboard::SetSound(WORD val)
{
    if(val&(1<<7))
        freq_enable[5]=1;
    else
        freq_enable[5]=0;
//12 11 10 9 8
    
    if(val&(1<<12))
        freq_enable[0]=1;
    else
        freq_enable[0]=0;

    if(val&(1<<11))
        freq_enable[1]=1;
    else
        freq_enable[1]=0;

    if(val&(1<<10))
        freq_enable[2]=1;
    else
        freq_enable[2]=0;

    if(val&(1<<9))
        freq_enable[3]=1;
    else
        freq_enable[3]=0;

    if(val&(1<<8))
        freq_enable[4]=1;
    else
        freq_enable[4]=0;
}

void CMotherboard::SetTapeReadCallback(TAPEREADCALLBACK callback, int sampleRate)
{
    if (callback == NULL)  // Reset callback
    {
        m_TapeReadCallback = NULL;
        m_nTapeSampleRate = 0;
    }
    else
    {
        m_TapeReadCallback = callback;
        m_nTapeSampleRate = sampleRate;
        m_TapeWriteCallback = NULL;
    }
}

void CMotherboard::SetTapeWriteCallback(TAPEWRITECALLBACK callback, int sampleRate)
{
    if (callback == NULL)  // Reset callback
    {
        m_TapeWriteCallback = NULL;
        m_nTapeSampleRate = 0;
    }
    else
    {
        m_TapeWriteCallback = callback;
        m_nTapeSampleRate = sampleRate;
        m_TapeReadCallback = NULL;
    }
}

void CMotherboard::SetSoundGenCallback(SOUNDGENCALLBACK callback)
{
    if (callback == NULL)  // Reset callback
    {
        m_SoundGenCallback = NULL;
    }
    else
    {
        m_SoundGenCallback = callback;
    }
}

void CMotherboard::SetSerialCallbacks(SERIALINCALLBACK incallback, SERIALOUTCALLBACK outcallback)
{
    if (incallback == NULL || outcallback == NULL)  // Reset callbacks
    {
        m_SerialInCallback = NULL;
        m_SerialOutCallback = NULL;
        //TODO: Set port value to indicate we are not ready to translate
    }
    else
    {
        m_SerialInCallback = incallback;
        m_SerialOutCallback = outcallback;
        //TODO: Set port value to indicate we are ready to translate
    }
}

void CMotherboard::SetParallelOutCallback(PARALLELOUTCALLBACK outcallback)
{
    CSecondMemoryController* pMemCtl = (CSecondMemoryController*) m_pSecondMemCtl;
    if (outcallback == NULL)  // Reset callback
    {
        pMemCtl->m_Port177101 &= ~2;  // Reset OnLine flag
        m_ParallelOutCallback = NULL;
    }
    else
    {
        pMemCtl->m_Port177101 |= 2;  // Set OnLine flag
        m_ParallelOutCallback = outcallback;
    }
}


//////////////////////////////////////////////////////////////////////
