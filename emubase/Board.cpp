// Board.cpp
//

#include "StdAfx.h"
#include "Emubase.h"
#include "..\SoundGen.h"
#include "Board.h"



//////////////////////////////////////////////////////////////////////

CMotherboard::CMotherboard ()
{
    // Create devices
	freq_per[0]=0;
	freq_per[1]=0;
	freq_per[2]=0;
	freq_per[3]=0;
	freq_per[4]=0;

	freq_out[0]=0;
	freq_out[1]=0;
	freq_out[2]=0;
	freq_out[3]=0;
	freq_out[4]=0;

	freq_enable[0]=0;
	freq_enable[1]=0;
	freq_enable[2]=0;
	freq_enable[3]=0;
	freq_enable[4]=0;
	freq_enable[5]=0;

	m_multiply=1;


	m_Sound= new CSoundGen();
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

	//m_floppyaddr=0;
	//m_floppystate=FLOPPY_FSM_WAITFORLSB;

    // Allocate memory for RAM and ROM
    m_pRAM[0] = (BYTE*) ::LocalAlloc(LPTR, 65536);
    m_pRAM[1] = (BYTE*) ::LocalAlloc(LPTR, 65536);
    m_pRAM[2] = (BYTE*) ::LocalAlloc(LPTR, 65536);
    m_pROM    = (BYTE*) ::LocalAlloc(LPTR, 32768);
    m_pROMCart[0] = NULL;
    m_pROMCart[1] = NULL;
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
    ::LocalFree(m_pRAM[0]);
    ::LocalFree(m_pRAM[1]);
    ::LocalFree(m_pRAM[2]);
    ::LocalFree(m_pROM);
    if (m_pROMCart[0] != NULL) ::LocalFree(m_pROMCart[0]);
    if (m_pROMCart[1] != NULL) ::LocalFree(m_pROMCart[1]);
}

void CMotherboard::Reset () 
{
    m_pCPU->Stop();
    m_pPPU->Stop();

    m_pFirstMemCtl->Reset();
    m_pSecondMemCtl->Reset();
    m_pFloppyCtl->Reset();

    m_cputicks = 0;
    m_pputicks = 0;
    m_lineticks = 0;
    m_timer = 0;
    m_timerreload = 0;
    m_timerflags = 0;
    m_timerdivider = 0;

	//m_floppyaddr=0;
	//m_floppystate=FLOPPY_FSM_WAITFORLSB;

    ::ZeroMemory(m_chancpurx, sizeof(m_chancpurx));
    ::ZeroMemory(m_chanppurx, sizeof(m_chanppurx));
    ::ZeroMemory(m_chancputx, sizeof(m_chancputx));
    ::ZeroMemory(m_chanpputx, sizeof(m_chanpputx));
	m_chancputx[0].ready=1;
	m_chancputx[1].ready=1;
	m_chancputx[2].ready=1;
	m_chanpputx[0].ready=1;
	m_chanpputx[1].ready=1;

    m_chan0disabled = 0;
    //m_currentdrive = 0;

    //m_CPUbp = 0177777;
    //m_PPUbp = 0177777;

    // We always start with PPU
    m_pPPU->Start();
}

void CMotherboard::LoadROM(const BYTE* pBuffer)  // Load 32 KB ROM image from the buffer
{
    ::CopyMemory(m_pROM, pBuffer, 32768);
}

void CMotherboard::LoadROMCartridge(int cartno, const BYTE* pBuffer)  // Load 24 KB ROM cartridge image
{
    ASSERT(cartno == 1 || cartno == 2);  // Only two cartridges, #1 and #2
    ASSERT(pBuffer != NULL);

    int cartindex = cartno - 1;
    if (m_pROMCart[cartindex] == NULL)
        m_pROMCart[cartindex] = (BYTE*) ::LocalAlloc(LPTR, 24 * 1024);

    ::CopyMemory(m_pROMCart[cartindex], pBuffer, 24 * 1024);
}

void CMotherboard::LoadRAM(int plan, const BYTE* pBuffer)  // Load 32 KB RAM image from the buffer
{
    ASSERT(plan >= 0 && plan <= 2);
    ::CopyMemory(m_pRAM[plan], pBuffer, 32768);
}

// Floppy ////////////////////////////////////////////////////////////

BOOL CMotherboard::IsFloppyImageAttached(int slot)
{
    ASSERT(slot >= 0 && slot < 4);
    return m_pFloppyCtl->IsAttached(slot);
}

BOOL CMotherboard::IsFloppyReadOnly(int slot)
{
    ASSERT(slot >= 0 && slot < 4);
    return m_pFloppyCtl->IsReadOnly(slot);
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

BOOL CMotherboard::IsROMCartridgeLoaded(int cartno)
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
        ::LocalFree(m_pROMCart[cartindex]);
        m_pROMCart[cartindex] = NULL;
    }
}


// Работа с памятью //////////////////////////////////////////////////

WORD CMotherboard::GetRAMWord(int plan, WORD offset)
{
    ASSERT(plan >= 0 && plan <= 2);
    return *((WORD*)(m_pRAM[plan] + offset)); 
}
BYTE CMotherboard::GetRAMByte(int plan, WORD offset) 
{ 
    ASSERT(plan >= 0 && plan <= 2);
    return m_pRAM[plan][offset]; 
}
void CMotherboard::SetRAMWord(int plan, WORD offset, WORD word) 
{
    ASSERT(plan >= 0 && plan <= 2);
	*((WORD*)(m_pRAM[plan] + offset)) = word;
}
void CMotherboard::SetRAMByte(int plan, WORD offset, BYTE byte) 
{
    ASSERT(plan >= 0 && plan <= 2);
    m_pRAM[plan][offset] = byte; 
}

WORD CMotherboard::GetROMWord(WORD offset)
{
    ASSERT(offset < 32768);
    return *((WORD*)(m_pROM + offset)); 
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
    WORD* p = (WORD*) (m_pROMCart[cartindex] + offset);
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
    int flag;
	
	
	
    if ((m_timerflags & 1) == 0)  // Nothing to do
        return;

	flag=0;
    m_timerdivider++;
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
        m_timer = m_timerreload & 07777; // Reload it

        if((m_timerflags & 0100) && (m_timerflags & 0200))
        {
            m_pPPU->InterruptVIRQ(2, 0304); 
        }
    }
    
}
WORD CMotherboard::GetTimerValue()  // Returns current timer value
{
    if(m_timerflags & 0200)
    {
        m_timerflags &= ~0200;  // Clear it
        return m_timer;
    }
    return m_timer;
}
WORD CMotherboard::GetTimerReload()  // Returns timer reload value
{
    return m_timerreload;
}

WORD CMotherboard::GetTimerState() // Returns timer state
{
    WORD res = m_timerflags;
    m_timerflags &= ~010;  // Clear overflow
    m_timerflags &= ~040;  // Clear external int
    
    return res;
}

void CMotherboard::SetTimerReload(WORD val)	 // Sets timer reload value
{
    m_timerreload = val & 07777;
	if ((m_timerflags & 1) == 0)
		m_timer=m_timerreload;
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
 	m_pPPU->SetInternalTick(0);
	m_pPPU->Execute();
	m_pCPU->SetInternalTick(0);
	m_pCPU->Execute();
	m_pFloppyCtl->Periodic();
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

*/
//TODO: DoSound() call
BOOL CMotherboard::SystemFrame()
{
    int frameticks = 0;  // 20000 ticks
	
	int audioticks = 20000/(SAMPLERATE/25);
	

    do
    {
        TimerTick();  // System timer tick

        if (frameticks % 10000 == 0)
        {
            Tick50();  // 1/50 timer event
        }

        // CPU - 16 times, PPU - 12.5 times
        for (int procticks = 0; procticks < 12; procticks++)
        {
            // CPU
            if (m_pCPU->GetPC() == m_CPUbp) return FALSE;  // Breakpoint
            m_pCPU->Execute();
            // PPU
            if (m_pPPU->GetPC() == m_PPUbp) return FALSE;  // Breakpoint
            m_pPPU->Execute();
            // CPU - extra ticks
            if (procticks % 3 == 0)  // CPU
            {
                if (m_pCPU->GetPC() == m_CPUbp) return FALSE;  // Breakpoint
                m_pCPU->Execute();
            }
        }
        if (frameticks % 2 == 0)  // PPU extra ticks
        {
            if (m_pPPU->GetPC() == m_PPUbp) return FALSE;  // Breakpoint
            m_pPPU->Execute();
        }

        if (frameticks % 32 == 0)  // FDD tick
        {
            m_pFloppyCtl->Periodic();
        }

		if (frameticks % audioticks == 0) //AUDIO tick
			DoSound();

        frameticks++;

	
	//	DoSound();
    }
    while (frameticks < 20000);

    return TRUE;
}

// Key pressed or released
void CMotherboard::KeyboardEvent(BYTE scancode, BOOL okPressed)
{
    CSecondMemoryController* pMemCtl = (CSecondMemoryController*) m_pSecondMemCtl;
    pMemCtl->KeyboardEvent(scancode, okPressed);
}


//////////////////////////////////////////////////////////////////////
//
// Emulator image format:
//   32 bytes  - Header
//   32 bytes  - Board status
//   32 bytes  - CPU status
//   32 bytes  - PPU status
//   64 bytes  - CPU memory/IO controller status
//   64 bytes  - PPU memory/IO controller status
//   32 Kbytes - ROM image
//   64 Kbytes * 3  - RAM planes 0, 1, 2

void CMotherboard::SaveToImage(BYTE* pImage)
{
    // Board data
    WORD* pwImage = (WORD*) (pImage + 32);
    *pwImage++ = m_timer;
    *pwImage++ = m_timerreload;
    *pwImage++ = m_timerflags;
    *pwImage++ = m_timerdivider;
    *pwImage++ = (WORD) m_chan0disabled;

    // CPU status
    BYTE* pImageCPU = pImage + 64;
    m_pCPU->SaveToImage(pImageCPU);
    // PPU status
    BYTE* pImagePPU = pImageCPU + 32;
    m_pPPU->SaveToImage(pImagePPU);
    // CPU memory/IO controller status
    BYTE* pImageCpuMem = pImagePPU + 32;
    m_pFirstMemCtl->SaveToImage(pImageCpuMem);
    // PPU memory/IO controller status
    BYTE* pImagePpuMem = pImageCpuMem + 64;
    m_pSecondMemCtl->SaveToImage(pImagePpuMem);

    // ROM
    BYTE* pImageRom = pImage + 256;
    CopyMemory(pImageRom, m_pROM, 32 * 1024);
    // RAM planes 0, 1, 2
    BYTE* pImageRam = pImageRom + 32 * 1024;
    CopyMemory(pImageRam, m_pRAM[0], 64 * 1024);
    pImageRam += 64 * 1024;
    CopyMemory(pImageRam, m_pRAM[1], 64 * 1024);
    pImageRam += 64 * 1024;
    CopyMemory(pImageRam, m_pRAM[2], 64 * 1024);

}
void CMotherboard::LoadFromImage(const BYTE* pImage)
{
    // Board data
    WORD* pwImage = (WORD*) (pImage + 32);
    m_timer = *pwImage++;
    m_timerreload = *pwImage++;
    m_timerflags = *pwImage++;
    m_timerdivider = *pwImage++;
    m_chan0disabled = (BYTE) *pwImage++;

    // CPU status
    const BYTE* pImageCPU = pImage + 64;
    m_pCPU->LoadFromImage(pImageCPU);
    // PPU status
    const BYTE* pImagePPU = pImageCPU + 32;
    m_pPPU->LoadFromImage(pImagePPU);
    // CPU memory/IO controller status
    const BYTE* pImageCpuMem = pImagePPU + 32;
    m_pFirstMemCtl->LoadFromImage(pImageCpuMem);
    // PPU memory/IO controller status
    const BYTE* pImagePpuMem = pImageCpuMem + 64;
    m_pSecondMemCtl->LoadFromImage(pImagePpuMem);

    // ROM
    const BYTE* pImageRom = pImage + 256;
    CopyMemory(m_pROM, pImageRom, 32 * 1024);
    // RAM planes 0, 1, 2
    const BYTE* pImageRam = pImageRom + 32 * 1024;
    CopyMemory(m_pRAM[0], pImageRam, 64 * 1024);
    pImageRam += 64 * 1024;
    CopyMemory(m_pRAM[1], pImageRam, 64 * 1024);
    pImageRam += 64 * 1024;
    CopyMemory(m_pRAM[2], pImageRam, 64 * 1024);
}

void		CMotherboard::ChanWriteByCPU(BYTE chan, BYTE data)
{
//#if !defined(PRODUCT)
//	TCHAR	txt[1024];
//#endif
	chan&=3;
	ASSERT(chan<3);

	if(chan==2)
	{
//#if !defined(PRODUCT)
//		wsprintf(txt,_T("CPU WR Chan %d, %x\r\n"),chan,data);
//		DebugPrint(txt);
//#endif
		//FloppyDebug(data);
	}

	if((chan==0)&&(m_chan0disabled))
		return;
	

	m_chanppurx[chan].data=data;
	m_chanppurx[chan].ready=1;
	m_chancputx[chan].ready=0;

	if(m_chanppurx[chan].irq)
		m_pPPU->InterruptVIRQ(5+chan*2, 0320+(010*chan));
}
void		CMotherboard::ChanWriteByPPU(BYTE chan, BYTE data)
{
	//TCHAR	txt[1024];
	chan&=3;
	ASSERT(chan<2); 

	//wsprintf(txt,_T("PPU WR Chan %d, %x\r\n"),chan,data);
	//DebugPrint(txt);

	if((chan==0)&&(m_chan0disabled))
		return;

	m_chancpurx[chan].data=data;
	m_chancpurx[chan].ready=1;
	m_chanpputx[chan].ready=0;

	if(m_chancpurx[chan].irq)
		m_pCPU->InterruptVIRQ(chan?3:1, chan?0460:060);
}
BYTE		CMotherboard::ChanReadByCPU(BYTE chan)
{
	BYTE res;

	chan&=3;
	ASSERT(chan<2); 

	if((chan==0)&&(m_chan0disabled))
		return 0;


	res=m_chancpurx[chan].data;
	m_chancpurx[chan].ready=0;
	m_chanpputx[chan].ready=1;
	if(m_chanpputx[chan].irq)
		m_pPPU->InterruptVIRQ(chan?8:6, chan?0334:0324);
	return res;
}
BYTE		CMotherboard::ChanReadByPPU(BYTE chan)
{
	BYTE res;

	chan&=3;
	ASSERT(chan<3); 

	//TCHAR txt[1024];

	if((chan==0)&&(m_chan0disabled))
		return 0;

	//wsprintf(txt,_T("PPU Read chan %d dis%d dat%d,rdy%d\r\n"),chan,m_chan0disabled,m_chanppurx[chan].data,m_chanppurx[chan].ready);
	//DebugPrint(txt);

	res=m_chanppurx[chan].data;
	m_chanppurx[chan].ready=0;
	m_chancputx[chan].ready=1;

	if(m_chancputx[chan].irq)
	{
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

BYTE		CMotherboard::ChanRxStateGetCPU(BYTE chan)
{
	//TCHAR txt[1024];
	chan&=3;
	ASSERT(chan<2);


	
	return (m_chancpurx[chan].ready<<7)|(m_chancpurx[chan].irq<<6);
}

BYTE		CMotherboard::ChanTxStateGetCPU(BYTE chan)
{
	chan&=3;
	ASSERT(chan<3);
	return (m_chancputx[chan].ready<<7)|(m_chancputx[chan].irq<<6);
}

BYTE		CMotherboard::ChanRxStateGetPPU()
{
	BYTE res;
	//TCHAR txt[1024];

		res= (m_chanppurx[2].ready<<5) | (m_chanppurx[1].ready<<4) | (m_chanppurx[0].ready<<3) | 
		   (m_chanppurx[2].irq<<2)   | (m_chanppurx[1].irq<<1)   | (m_chanppurx[0].irq);

	//wsprintf(txt,_T("PPU RX Stateget =0x%2.2X\r\n"),res);
	//DebugPrint(txt);

	return res;
}
BYTE		CMotherboard::ChanTxStateGetPPU()
{
	BYTE res;
	//TCHAR txt[1024];
	res= (m_chanpputx[1].ready<<4) | (m_chanpputx[0].ready<<3) | (m_chan0disabled<<2)   |
		    (m_chanpputx[1].irq<<1)   | (m_chanpputx[0].irq);

	//wsprintf(txt,_T("PPU TX Stateget 0x%2.2X\r\n"),res);
	//DebugPrint(txt);

	return res;
}
void		CMotherboard::ChanRxStateSetCPU(BYTE chan, BYTE state)
{
	chan&=3;
	ASSERT(chan<2);

/*	if(state&0200)
	{
		m_chancpurx[chan].ready=1;
	//	m_chanpputx[chan].ready=0;
	}
	else
	{
		m_chancpurx[chan].ready=0;
		//m_chanpputx[chan].ready=1;
	}
*/
	if(state&0100) //irq
		m_chancpurx[chan].irq=1;
	else
		m_chancpurx[chan].irq=0;

	if((m_chancpurx[chan].irq)&&(m_chancpurx[chan].ready))
		m_pCPU->InterruptVIRQ(chan?3:1, chan?0460:060);


}
void		CMotherboard::ChanTxStateSetCPU(BYTE chan, BYTE state)
{
	chan&=3;
	ASSERT(chan<3);

	if(state&0200)
	{
		m_chancputx[chan].ready=1;
		///m_chanppurx[chan].ready=0;
	}
	/*else
	{
		m_chancputx[chan].ready=1;
		//m_chanppurx[chan].ready=1;
	}*/

	if(state&0100) //irq
		m_chancputx[chan].irq=1;
	else
		m_chancputx[chan].irq=0;


	if((m_chancputx[chan].irq)&&(m_chancputx[chan].ready))
	{
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

void		CMotherboard::ChanRxStateSetPPU(BYTE state)
{
	//TCHAR txt[1024];
	//wsprintf(txt,_T("PPU RX Stateset 0x%2.2X \r\n"),state);
	//DebugPrint(txt);

	m_chanppurx[0].irq=state&1;
	m_chanppurx[1].irq=(state>>1)&1;
	m_chanppurx[2].irq=(state>>2)&1;
	
	//if(state&0x40)
		//GetPPU()->InterruptVIRQ(0314);
//what shoud we do with interrupts if interrupts are pending and we've just enabled them?

}
void		CMotherboard::ChanTxStateSetPPU(BYTE state)
{
	//TCHAR txt[1024];
	//wsprintf(txt,_T("PPU TX Stateset 0x%2.2x \r\n"),state);
	//DebugPrint(txt);

	m_chanpputx[0].irq=state&1;
	m_chanpputx[1].irq=(state>>1)&1;
	m_chan0disabled=(state>>2)&1;
	if((state>>3)&1)
		m_chanpputx[0].ready=(state>>3)&1;
	if((state>>4)&1)
		m_chanpputx[1].ready=(state>>4)&1;

	if((m_chanpputx[0].irq)&&(m_chanpputx[0].ready))
		m_pPPU->InterruptVIRQ(6, 0324);
	else
	if((m_chanpputx[1].irq)&&(m_chanpputx[1].ready))
		m_pPPU->InterruptVIRQ(8, 0334);



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


WORD	CMotherboard::GetFloppyState()
{
	return m_pFloppyCtl->GetState();
}
WORD	CMotherboard::GetFloppyData()
{
	return m_pFloppyCtl->GetData();
}
void	CMotherboard::SetFloppyState(WORD val)
{
	//if(val&02000)
	//{
	//	m_currentdrive=(val&3)^3;
	//}
	////m_currentdrive=0;
	//m_pFloppyCtl[m_currentdrive]->SetCommand(val&~3); // it should not get select :)
    m_pFloppyCtl->SetCommand(val);
}
void	CMotherboard::SetFloppyData(WORD val)
{
    m_pFloppyCtl->WriteData(val);
}


//////////////////////////////////////////////////////////////////////

WORD CMotherboard::GetKeyboardRegister(void)
{
	WORD res;
	WORD w7214;
	BYTE b22556;

	w7214=GetRAMWord(0,07214);
	b22556=GetRAMByte(0,022556);

	switch(w7214)
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
/*		int freq_per[6];
	int freq_out[6];
	int freq_enable[6];*/
	int global;


	freq_out[0]=(m_timer>>3)&1; //8000

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

//	global=(freq_out[0]);
//	global=(freq_out[4]);
	//global|=(freq_out[2]&freq_enable[2]);
//	global|=(freq_out[3]&freq_enable[3]);
//	global|=(freq_out[4]&freq_enable[4]);
//	global&=freq_enable[5];


	if(!global)
		m_Sound->FeedDAC(0x7fff,0x7fff);
	else
		m_Sound->FeedDAC(0x8000,0x8000);


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
