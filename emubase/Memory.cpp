/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Memory.cpp
//

#include "stdafx.h"
#include "Memory.h"
#include "Board.h"
#include "Processor.h"


//////////////////////////////////////////////////////////////////////

CMemoryController::CMemoryController ()
{
    m_pProcessor = NULL;
    m_pBoard = NULL;
}

// Read word from memory for debugger
WORD CMemoryController::GetWordView(WORD address, BOOL okHaltMode, BOOL okExec, BOOL* pValid)
{
    WORD offset;
    int addrtype = TranslateAddress(address, okHaltMode, okExec, &offset, TRUE);

    switch (addrtype)
    {
    case ADDRTYPE_RAM0:
    case ADDRTYPE_RAM1:
    case ADDRTYPE_RAM2:
        *pValid = TRUE;
        return m_pBoard->GetRAMWord((addrtype & ADDRTYPE_MASK_RAM), offset);
    case ADDRTYPE_RAM12:
        *pValid = TRUE;
        return MAKEWORD(
            m_pBoard->GetRAMByte(1, offset / 2),
            m_pBoard->GetRAMByte(2, offset / 2));
    case ADDRTYPE_ROMCART1:
        return m_pBoard->GetROMCartWord(1, offset);
    case ADDRTYPE_ROMCART2:
        return m_pBoard->GetROMCartWord(2, offset);
    case ADDRTYPE_ROM:
        *pValid = TRUE;
        return m_pBoard->GetROMWord(offset);
    case ADDRTYPE_IO:
        *pValid = FALSE;  // I/O port, not memory
        return 0;
    case ADDRTYPE_DENY:
        *pValid = TRUE;  // This memory is inaccessible for reading
        return 0;
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
    return 0;
}

WORD CMemoryController::GetWord(WORD address, BOOL okHaltMode, BOOL okExec)
{
//#if !defined(PRODUCT)
//	TCHAR buf[40];
//#endif

    WORD offset;
    int addrtype = TranslateAddress(address, okHaltMode, okExec, &offset);

    switch (addrtype)
    {
    case ADDRTYPE_RAM0:
    case ADDRTYPE_RAM1:
    case ADDRTYPE_RAM2:
        return m_pBoard->GetRAMWord((addrtype & ADDRTYPE_MASK_RAM), offset);
    case ADDRTYPE_RAM12:
        return MAKEWORD(
            m_pBoard->GetRAMByte(1, offset / 2),
            m_pBoard->GetRAMByte(2, offset / 2));
    case ADDRTYPE_ROMCART1:
        return m_pBoard->GetROMCartWord(1, offset);
    case ADDRTYPE_ROMCART2:
        return m_pBoard->GetROMCartWord(2, offset);
    case ADDRTYPE_ROM:
//#if !defined(PRODUCT)
//		//136064 =< 136274
//		//ASSERT( (address<0136064) || (address>0136273));
//		if((address>=0136064)&&(address<0136274))
//		{
//			PrintOctalValue(buf,m_pBoard->GetPPU()->GetPC()-2);
//			DebugLog(_T("R ACC PC: "));
//			DebugLog(buf);
//			DebugLog(_T("\r\n"));
//		}
//#endif
        return m_pBoard->GetROMWord(offset);
    case ADDRTYPE_IO:
        //TODO: What to do if okExec == TRUE ?
        return GetPortWord(address);
    case ADDRTYPE_DENY:
        //TODO: Exception processing
        return 0;
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
    return 0;
}

BYTE CMemoryController::GetByte(WORD address, BOOL okHaltMode)
{
    WORD offset;
    int addrtype = TranslateAddress(address, okHaltMode, FALSE, &offset);

    switch (addrtype)
    {
    case ADDRTYPE_RAM0:
    case ADDRTYPE_RAM1:
    case ADDRTYPE_RAM2:
        return m_pBoard->GetRAMByte((addrtype & ADDRTYPE_MASK_RAM), offset);
    case ADDRTYPE_RAM12:
        if ((offset & 1) == 0)
            return m_pBoard->GetRAMByte(1, offset / 2);
        else
            return m_pBoard->GetRAMByte(2, offset / 2);

    case ADDRTYPE_ROMCART1:
        return m_pBoard->GetROMCartByte(1, offset);
    case ADDRTYPE_ROMCART2:
        return m_pBoard->GetROMCartByte(2, offset);

    case ADDRTYPE_ROM:
        return m_pBoard->GetROMByte(offset);
    case ADDRTYPE_IO:
        //TODO: What to do if okExec == TRUE ?
        return GetPortByte(address);
    case ADDRTYPE_DENY:
        //TODO: Exception processing
        return 0;
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
    return 0;
}

void CMemoryController::SetWord(WORD address, BOOL okHaltMode, WORD word)
{
    WORD offset;
    int addrtype = TranslateAddress(address, okHaltMode, FALSE, &offset);

    //ASSERT( (address!=0157552) || (word!=0157272));
    switch (addrtype)
    {
    case ADDRTYPE_RAM0:
    case ADDRTYPE_RAM1:
    case ADDRTYPE_RAM2:
        m_pBoard->SetRAMWord((addrtype & ADDRTYPE_MASK_RAM), offset, word);
        return;
    case ADDRTYPE_RAM12:
        m_pBoard->SetRAMByte(1, offset / 2, LOBYTE(word));
        m_pBoard->SetRAMByte(2, offset / 2, HIBYTE(word));
        return;

    case ADDRTYPE_ROMCART1:
    case ADDRTYPE_ROMCART2:
    case ADDRTYPE_ROM:
        // Nothing to do: writing to ROM
        return;

    case ADDRTYPE_IO:
        SetPortWord(address, word);
        return;
    
    case ADDRTYPE_DENY:
        //TODO: Exception processing
        return;
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
}

void CMemoryController::SetByte(WORD address, BOOL okHaltMode, BYTE byte)
{
    WORD offset;
    int addrtype = TranslateAddress(address, okHaltMode, FALSE, &offset);

    switch (addrtype)
    {
    case ADDRTYPE_RAM0:
    case ADDRTYPE_RAM1:
    case ADDRTYPE_RAM2:
        m_pBoard->SetRAMByte((addrtype & ADDRTYPE_MASK_RAM), offset, byte);
        return;
    case ADDRTYPE_RAM12:
        if ((offset & 1) == 0)
            m_pBoard->SetRAMByte(1, offset / 2, byte);
        else
            m_pBoard->SetRAMByte(2, offset / 2, byte);
    
    case ADDRTYPE_ROMCART1:
    case ADDRTYPE_ROMCART2:
    case ADDRTYPE_ROM:
        // Nothing to do: writing to ROM
        return;
    case ADDRTYPE_IO:
        SetPortByte(address, byte);
        return;
    case ADDRTYPE_DENY:
        //TODO: Exception processing
        return;
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
}


//////////////////////////////////////////////////////////////////////
//
// CPU memory controller
// is connected to RAM plane 1 & 2.
//
// 174000-177777 I/O - USER - read/write
// 160000-173777     - USER - access denied
// 000000-157777 ОЗУ - USER - read/write/execute
//
// 174000-177777 I/O - HALT - read/write
// 174000-177777 ОЗУ - HALT - execute
// 160000-173777 ОЗУ - HALT - read/write/execute
// 000000-157777 ОЗУ - HALT - read/write/execute
//
// For RAM access, bytes at even addresses (low byte of word) belongs to plane 1,
// and bytes at odd addresses (high byte of word) - belongs to plane 2.

CFirstMemoryController::CFirstMemoryController() : CMemoryController()
{
    m_Port176640 = 0;
    m_Port176642 = 0;
    m_Port176644 = 0;
    m_Port176646 = 0;
    m_Port176570 = m_Port176572 = m_Port176576 = 0;  // RS-232 ports
    m_Port176574 = 0200;
}

void CFirstMemoryController::DCLO_Signal()
{
}

void CFirstMemoryController::ResetDevices()
{
    m_pBoard->ChanResetByCPU();
	m_Port176644 = 0;
	m_pProcessor->InterruptVIRQ(6,0);
	//TODO
}

int CFirstMemoryController::TranslateAddress(WORD address, BOOL okHaltMode, BOOL okExec, WORD* pOffset, BOOL okView)
{
    if ((!okView) && ((m_Port176644 & 0x101) == 0x101) && (address == m_Port176646) && (((m_Port176644 & 2) == 2) == okHaltMode))
	{
		m_pProcessor->InterruptVIRQ(6,m_Port176644 & 0xFC);
	}
	if (address < 0160000) {  // CPU RAM (plane 1 & 2)
        *pOffset = address;
        return ADDRTYPE_RAM12;
    }
    else 
    {
        if (okHaltMode) 
        {  // HALT mode
                *pOffset = address;
                return ADDRTYPE_RAM12;
        }
        else 
        {      
                *pOffset = address;
                return ADDRTYPE_IO;
        }
    }

    ASSERT(FALSE);  // If we are here - then if isn't cover all addresses
    return 0;
}

BYTE CFirstMemoryController::GetPortByte(WORD address)
{
    WORD word = GetPortWord(address);
    return (BYTE) (address&1)?HIBYTE(word):LOBYTE(word);
}

WORD CFirstMemoryController::GetPortWord(WORD address)
{
    switch (address) {
        case 0176640:
        case 0176641:
            return m_Port176640;  // Plane address register
        case 0176642:
        case 0176643:
            m_Port176642 = MAKEWORD(m_pBoard->GetRAMByte(1, m_Port176640), m_pBoard->GetRAMByte(2, m_Port176640));
            return m_Port176642;  // Plane 1 & 2 data register

        case 0176644: case 0176645:
            return (m_Port176644 & 0x200);
        case 0176646: case 0176647:
            return 0;

		case 0177560:
        case 0177561:
            return m_pBoard->ChanRxStateGetCPU(0);
        case 0177562:
        case 0177563:
            return m_pBoard->ChanReadByCPU(0);
        case 0177564:
        case 0177565:
            return m_pBoard->ChanTxStateGetCPU(0);

        case 0176660:
        case 0176661:
            return m_pBoard->ChanRxStateGetCPU(1);
        case 0176662:
        case 0176663:
            return m_pBoard->ChanReadByCPU(1);
        case 0176664:
        case 0177665:
            return m_pBoard->ChanTxStateGetCPU(1);
        case 0176674:
        case 0176675:
            return m_pBoard->ChanTxStateGetCPU(2);
        case 0177566:
        case 0177567:
        case 0176666:
        case 0176667:
        case 0176676:
        case 0176677:
        case 0176670:
        case 0176671:
        case 0176672:
        case 0176673:
            return 0;

        case 0176560: //network 
        case 0176561:
        case 0176562:
        case 0176563:
            return 0;
        
        case 0176564:
        case 0176565:
            return 0x80; //ready for tx
        
        case 0176566:
        case 0176567:
            return 0;
        
        case 0176570:  // Стык С2: Регистр состояния приемника
        case 0176571:
            return m_Port176570;
        case 0176572:  // Стык С2: Регистр данных приемника
        case 0176573:  // нижние 8 бит доступны по чтению
            m_Port176570 &= ~010200;  // Reset bit 12 and bit 7
            return m_Port176572;
        case 0176574:  // Стык С2: Регистр состояния источника
        case 0176575:
            return m_Port176574;
        case 0176576:  // Стык С2: Регистр данных источника
        case 0176577:
            return 0370;

        default: 
            if (!(((m_Port176644 & 0x103) == 0x100) && m_Port176646 == address))
				m_pProcessor->MemoryError();
            return 0x0;
    }
    //ASSERT(0);
    return 0; 
}


// Read word from port for debugger
WORD CFirstMemoryController::GetPortView(WORD address)
{
    switch (address) {
        case 0176640:  return m_Port176640;  // Plane address register
        case 0176642:  return m_Port176642;  // Plane 1 & 2 data register

        //TODO

        default:
            return 0;
    }
}

void CFirstMemoryController::SetPortByte(WORD address, BYTE byte)
{
    WORD word = (address&1)?((WORD)byte) << 8:(WORD)byte;
    switch (address) {
        case 0176640:  // Plane address register
        case 0176641:
            SetPortWord(address,word);
            break;
        case 0176642:  // Plane 1 & 2 data register
            m_Port176642 &= 0xFF00;
            m_Port176642 |= word;
            m_pBoard->SetRAMByte(1, m_Port176640, LOBYTE(word));
            break;
        case 0176643:
            m_Port176642 &= 0xFF;
            m_Port176642 |= word;
            m_pBoard->SetRAMByte(2, m_Port176640, HIBYTE(word));
            break;

        case 0176644: case 0176645:
            SetPortWord(address,word);
			break;
        case 0176646: case 0176647:
            SetPortWord(address,word);
			break;

        case 0177560:
            m_pBoard->ChanRxStateSetCPU(0, (BYTE) word);
            break;
        case 0177561:
            m_pBoard->ChanRxStateSetCPU(0, 0);
            break;
        case 0177562:
        case 0177563:
            break;
        case 0177564:
            m_pBoard->ChanTxStateSetCPU(0, (BYTE) word);
            break;
        case 0177565:
            m_pBoard->ChanTxStateSetCPU(0, 0);
            break;
        case 0177566:  // TX data, channel 0
            m_pBoard->ChanWriteByCPU(0, (BYTE) word);
            break;
        case 0177567:
            m_pBoard->ChanWriteByCPU(0, 0);
            break;
        case 0176660:
            m_pBoard->ChanRxStateSetCPU(1, (BYTE) word);
            break;
        case 0176661:
            m_pBoard->ChanRxStateSetCPU(1, 0);
            break;
        case 0176662:
        case 0176663:
            break ;
        case 0176664:
            m_pBoard->ChanTxStateSetCPU(1, (BYTE) word);
            break;
        case 0176665:
            m_pBoard->ChanTxStateSetCPU(1, 0);
            break;
        case 0176666:  // TX data, channel 1
            m_pBoard->ChanWriteByCPU(1, (BYTE) word);
            break;
        case 0176667:
            m_pBoard->ChanWriteByCPU(1, 0);
            break;
        case 0176674:
            m_pBoard->ChanTxStateSetCPU(2, (BYTE) word);
            break;
        case 0176675:
            m_pBoard->ChanTxStateSetCPU(2, 0);
            break;
        case 0176676:  // TX data, channel 2
            m_pBoard->ChanWriteByCPU(2, (BYTE) word);
            break;
        case 0176677:
            m_pBoard->ChanWriteByCPU(2, 0);
            break;
        case 0176670:
        case 0176671:
        case 0176672:
        case 0176673:
            break;

        case 0176560: //network 
        case 0176561:
        case 0176562:
        case 0176563:
        case 0176564:
        case 0176565:
        case 0176566:
        case 0176567:
            return ;
        case 0176570:  // Стык С2: Регистр состояния приемника
        case 0176571:
        case 0176572:  // Стык С2: Регистр данных приемника
        case 0176573:
        case 0176574:  // Стык С2: Регистр состояния источника
        case 0176575:
        case 0176576:  // Стык С2: Регистр данных источника
        case 0176577:
            return ;

        default:
            if (!(((m_Port176644 & 0x103) == 0x100) && m_Port176646 == address))
	            m_pProcessor->MemoryError();
//			ASSERT(0);
            break;
    }
}

void CFirstMemoryController::SetPortWord(WORD address, WORD word)
{
    switch (address) {
        case 0176640:  // Plane address register
        case 0176641:
            m_Port176640 = word;
            m_Port176642 = MAKEWORD(
                    m_pBoard->GetRAMByte(1, m_Port176640), m_pBoard->GetRAMByte(2, m_Port176640));
            break;
        case 0176642:  // Plane 1 & 2 data register
        case 0176643:
            m_Port176642 = word;
            m_pBoard->SetRAMByte(1, m_Port176640, LOBYTE(word));
            m_pBoard->SetRAMByte(2, m_Port176640, HIBYTE(word));
            break;

        case 0176644: case 0176645:
            word &= 0x3FF;
			m_Port176644 = word;
			if ((word & 0x101) == 0x101)
			{
				if ((m_pProcessor->GetVIRQ(6)) != 0)
					m_pProcessor->InterruptVIRQ(6, word & 0xFC);
			}
			else
			{
				m_pProcessor->InterruptVIRQ(6,0);
			}
			break;
        case 0176646: case 0176647:
            m_Port176646 = word;
			break;

        case 0177560:
        case 0177561:
            m_pBoard->ChanRxStateSetCPU(0, (BYTE) word);
            break;
        case 0177562:
        case 0177563:
            break;

        case 0177564:
        case 0177565:
            m_pBoard->ChanTxStateSetCPU(0, (BYTE) word);
            break;
        case 0177566:  // TX data, channel 0
        case 0177567:
            m_pBoard->ChanWriteByCPU(0, (BYTE) word);
            break;
        case 0176660:
        case 0176661:
            m_pBoard->ChanRxStateSetCPU(1, (BYTE) word);
            break;
        case 0176662:
        case 0176663:
            break ;
        case 0176664:
        case 0176665:
            m_pBoard->ChanTxStateSetCPU(1, (BYTE) word);
            break;
        case 0176666:  // TX data, channel 1
        case 0176667:
            m_pBoard->ChanWriteByCPU(1, (BYTE) word);
            break;
        case 0176674:
        case 0176675:
            m_pBoard->ChanTxStateSetCPU(2, (BYTE) word);
            break;
        case 0176676:  // TX data, channel 2
        case 0176677:
            m_pBoard->ChanWriteByCPU(2, (BYTE) word);
            break;
        case 0176670:
        case 0176671:
        case 0176672:
        case 0176673:
            break;

        case 0176560: //network 
        case 0176561:
        case 0176562:
        case 0176563:
        case 0176564:
        case 0176565:
        case 0176566:
        case 0176567:
            return ;
        case 0176570:  // Стык С2: Регистр состояния приемника
        case 0176571:
            m_Port176570 = (m_Port176570 & ~0100) | (word & 0100);  // Bit 6 only
            break;
        case 0176572:  // Стык С2: Регистр данных приемника
        case 0176573:  // недоступен по записи
            return ;
        case 0176574:  // Стык С2: Регистр состояния источника
        case 0176575:
            m_Port176574 = (m_Port176574 & ~0105) | (word & 0105);  // Bits 0,2,6
            break;
        case 0176576:  // Стык С2: Регистр данных источника
        case 0176577:  // нижние 8 бит доступны по записи
            m_Port176576 = word & 0xff;
            m_Port176574 &= ~128;  // Reset bit 7 (Ready)
            break;

        default:
            if (!(((m_Port176644 & 0x103) == 0x100) && m_Port176646 == address))
	            m_pProcessor->MemoryError();
//			ASSERT(0);
            break;
    }
}

BOOL CFirstMemoryController::SerialInput(BYTE inputByte)
{
    this->m_Port176572 = (WORD)inputByte;
    if (this->m_Port176570 & 0200)  // Ready?
        this->m_Port176570 |= 010000;  // Set Overflow flag
    else
    {
        this->m_Port176570 |= 0200;  // Set Ready flag
        if (this->m_Port176570 & 0200)  // Interrupt?
            return TRUE;
    }

    return FALSE;
}

//////////////////////////////////////////////////////////////////////
//
// CPU memory/IO controller image format (64 bytes):
//   2*10 bytes     10 port registers
//   46 bytes       Not used

void CFirstMemoryController::SaveToImage(BYTE* pImage)
{
    WORD* pwImage = (WORD*) pImage;
    *pwImage++ = m_Port176640;
    *pwImage++ = m_Port176642;
}
void CFirstMemoryController::LoadFromImage(const BYTE* pImage)
{
    WORD* pwImage = (WORD*) pImage;
    m_Port176640 = *pwImage++;
    m_Port176642 = *pwImage++;
}


//////////////////////////////////////////////////////////////////////
//
// PPU memory controller
// is connected to RAM plane 0 and ROM.
//
// 177000-177777 I/O - only read/write
// 100000-176777 ROM - full access - read/write/execute
// 000000-077777 RAM - full access - read/write/execute

CSecondMemoryController::CSecondMemoryController() : CMemoryController()
{
    m_Port177010 = m_Port177012 = m_Port177014 = 0;

    m_Port177026 = m_Port177024 = 0;
    m_Port177020 = m_Port177022 = 0;
    m_Port177016 = 0;

    m_Port177700 = m_Port177702 = 0;
    m_Port177716 = 0;

    m_Port177054 = 01401;

    m_Port177100 = m_Port177101 = m_Port177102 = 0377;
}

void CSecondMemoryController::DCLO_Signal()
{
    DCLO_177716();
}

void CSecondMemoryController::ResetDevices()
{
    Init_177716();
    m_pBoard->ChanResetByPPU();
    //TODO
}

int CSecondMemoryController::TranslateAddress(WORD address, BOOL okHaltMode, BOOL okExec, WORD* pOffset, BOOL okView)
{
    if (address < 0100000)  // 000000-077777 - PPU RAM
    {
        *pOffset = address;
        return ADDRTYPE_RAM0;
    }
    else if (address < 0177000)  // 100000-176777 - Window
    {
        if (address < 0120000)  // 100000-117777 - Window block 0
        {
            if ((m_Port177054 & 16) != 0)  // Port 177054 bit 4 set => RAM selected
            {
                *pOffset = address;
                return ADDRTYPE_RAM0;
            }
            else if ((m_Port177054 & 1) != 0)  // ROM selected
            {
                *pOffset = address - 0100000;
                return ADDRTYPE_ROM;
            }
            else if ((m_Port177054 & 14) != 0)  // ROM cartridge selected
            {
                int slot = ((m_Port177054 & 8) == 0) ? 1 : 2;
                if (m_pBoard->IsHardImageAttached(slot) && address >= 0110000)
                {
                    *pOffset = address;
                    return ADDRTYPE_IO;  // 110000-117777 - HDD ports
                }
                else
                {
                    int bank = (m_Port177054 & 6) >> 1;
                    *pOffset = address - 0100000 + ((bank - 1) << 13);
                    return (slot == 1) ? ADDRTYPE_ROMCART1 : ADDRTYPE_ROMCART2;
                }
            }
            return ADDRTYPE_NONE;
        }
        else if (address < 0140000)  // 120000-137777 - Window block 1
        {
            if ((m_Port177054 & 32) != 0)  // Port 177054 bit 5 set => RAM selected
            {
                *pOffset = address;
                return ADDRTYPE_RAM0;
            }
            *pOffset = address - 0100000;
            return ADDRTYPE_ROM;
        }
        else if (address < 0160000)  // 140000-157777 - Window block 2
        {
            if ((m_Port177054 & 64) != 0)  // Port 177054 bit 6 set => RAM selected
            {
                *pOffset = address;
                return ADDRTYPE_RAM0;
            }
            *pOffset = address - 0100000;
            return ADDRTYPE_ROM;
        }
        else if (address < 0177000)  // 160000-176777 - Window block 3
        {
            if ((m_Port177054 & 128) != 0)  // Port 177054 bit 7 set => RAM selected
            {
                *pOffset = address;
                return ADDRTYPE_RAM0;
            }
            *pOffset = address - 0100000;
            return ADDRTYPE_ROM;
        }
    }
    else  // 177000-177777 - I/O addresses
    {
        if (okExec) {  // Execution on this address is denied
            *pOffset = 0;
            return ADDRTYPE_DENY;
        }
        else {
            *pOffset = address;
            return ADDRTYPE_IO;
        }
    }

    ASSERT(FALSE);  // If we are here - then if isn't cover all addresses
    return ADDRTYPE_NONE;
}

WORD CSecondMemoryController::GetPortWord(WORD address)
{
    WORD value;
 #if !defined(PRODUCT)
    TCHAR oct1[7];
    TCHAR oct2[7];
    PrintOctalValue(oct1, address);
    PrintOctalValue(oct2, m_pBoard->GetPPU()->GetPC());
#endif

    switch (address) {
        case 0177010:
        case 0177011:
            return m_Port177010;  // Plane address register
        case 0177012:
        case 0177013:
            return m_Port177012;  // Plane 0 data register
        case 0177014:
        case 0177015:
            return m_Port177014;  // Plane 1 & 2 data register
        case 0177016:
        case 0177017:
            return m_Port177016;  // Sprite Color
        case 0177020:
        case 0177021:
            return m_Port177020;  // Plane 0,1,2 bits 0-3
        case 0177022:
        case 0177023:
            return m_Port177022;  // Plane 0,1,2 bits 4-7
        case 0177024:  // Load background registers
        case 0177025:
            {
                BYTE planes[3];
                planes[0] = m_pBoard->GetRAMByte(0,m_Port177010);
                planes[1] = m_pBoard->GetRAMByte(1,m_Port177010);
                planes[2] = m_pBoard->GetRAMByte(2,m_Port177010);

                m_Port177020 = 0;
                m_Port177022 = 0;

                m_Port177020 |= ((planes[0]&(1<<0))?1:0)<<0;
                m_Port177020 |= ((planes[0]&(1<<1))?1:0)<<4;
                m_Port177020 |= ((planes[0]&(1<<2))?1:0)<<8;
                m_Port177020 |= ((planes[0]&(1<<3))?1:0)<<12;
                m_Port177022 |= ((planes[0]&(1<<4))?1:0)<<0;
                m_Port177022 |= ((planes[0]&(1<<5))?1:0)<<4;
                m_Port177022 |= ((planes[0]&(1<<6))?1:0)<<8;
                m_Port177022 |= ((planes[0]&(1<<7))?1:0)<<12;

                m_Port177020 |= ((planes[1]&(1<<0))?1:0)<<1;
                m_Port177020 |= ((planes[1]&(1<<1))?1:0)<<5;
                m_Port177020 |= ((planes[1]&(1<<2))?1:0)<<9;
                m_Port177020 |= ((planes[1]&(1<<3))?1:0)<<13;
                m_Port177022 |= ((planes[1]&(1<<4))?1:0)<<1;
                m_Port177022 |= ((planes[1]&(1<<5))?1:0)<<5;
                m_Port177022 |= ((planes[1]&(1<<6))?1:0)<<9;
                m_Port177022 |= ((planes[1]&(1<<7))?1:0)<<13;

                m_Port177020 |= ((planes[2]&(1<<0))?1:0)<<2;
                m_Port177020 |= ((planes[2]&(1<<1))?1:0)<<6;
                m_Port177020 |= ((planes[2]&(1<<2))?1:0)<<10;
                m_Port177020 |= ((planes[2]&(1<<3))?1:0)<<14;
                m_Port177022 |= ((planes[2]&(1<<4))?1:0)<<2;
                m_Port177022 |= ((planes[2]&(1<<5))?1:0)<<6;
                m_Port177022 |= ((planes[2]&(1<<6))?1:0)<<10;
                m_Port177022 |= ((planes[2]&(1<<7))?1:0)<<14;
            }
            return 0;
        case 0177026:
        case 0177027:
            return m_Port177026;  // Plane Mask

        case 0177054:
        case 0177055:
            return m_Port177054;

        case 0177060:
        case 0177061:
            return m_pBoard->ChanReadByPPU(0);
        case 0177062:
        case 0177063:
            return m_pBoard->ChanReadByPPU(1);
        case 0177064:
        case 0177065:
            return m_pBoard->ChanReadByPPU(2);
        case 0177066:
        case 0177067:
            return m_pBoard->ChanRxStateGetPPU();
        case 0177070:
        case 0177071:
        case 0177072:
        case 0177073:
        case 0177074:
        case 0177075:
            return 0;
        case 0177076:
        case 0177077:
            return m_pBoard->ChanTxStateGetPPU();

        case 0177100:  // i8255 port A -- Parallel port output data
            return m_Port177100;
        case 0177101:  // i8255 port B
            return m_Port177101;
        case 0177102:  // i8255 port C
            return m_Port177102 & 0x0f;
        case 0177103:  // i8255 control
            return 0;

        case 0177700:
        case 0177701:
            return m_Port177700;  // Keyboard status
        case 0177702:  // Keyboard data
        case 0177703:
            m_Port177700 &= ~0200;  // Reset bit 7 - "data ready" flag
			m_pProcessor->InterruptVIRQ(3, 0);
			return m_Port177702;
        case 0177704:
        case 0177705:
            return 010000; //!!!

        case 0177710:
        case 0177711:
            return m_pBoard->GetTimerState();
        case 0177712:
        case 0177713:
            return m_pBoard->GetTimerReload();
        case 0177714:
        case 0177715:
            return m_pBoard->GetTimerValue();
            
        case 0177716:
        case 0177717:
            return m_Port177716;  // System control register
    
        case 0177130:  // FDD status
        case 0177131:
            value = m_pBoard->GetFloppyState();
            //PrintOctalValue(oct2, value);
            //wsprintf(str, _T("FDD STATE R %s, %s\r\n"), oct1, oct2);
            //DebugLog(str);
            return value;
        case 0177132: //fdd data
        case 0177133:
            value = m_pBoard->GetFloppyData();
            //wsprintf(str,_T("FDD DATA  R %s, %04x\r\n"), oct1, value);
            //DebugLog(str);
            return value;

        // HDD ports
        case 0110016:
        case 0110014:
        case 0110012:
        case 0110010:
        case 0110006:
        case 0110004:
        case 0110002:
        case 0110000:
            return m_pBoard->GetHardPortWord(((m_Port177054 & 8) == 0) ? 1 : 2, address);

        default:
            m_pProcessor->MemoryError();
            break;
    }

    return 0; 
}

BYTE CSecondMemoryController::GetPortByte(WORD address)
{
    WORD word = GetPortWord(address);
    return (BYTE) (address&1)?HIBYTE(word):LOBYTE(word); 
}

void CSecondMemoryController::SetPortByte(WORD address, BYTE byte)
{
    WORD word = (address&1)?((WORD)byte) << 8:(WORD)byte;
    if ((address>=0110000) && (address<0120000))
        address &= 0110016;
    switch (address) {
        case 0177010:
        case 0177011:
            SetPortWord(address, word);
            break;
        case 0177012:
        case 0177013:
            SetPortWord(address, word);
            break;
        case 0177014:
            m_Port177014 &= 0xFF00;
            m_Port177014 |= word;
            m_pBoard->SetRAMByte(1, m_Port177010, LOBYTE(word));
            break;
        case 0177015:
            m_Port177014 &= 0xFF;
            m_Port177014 |= word;
            m_pBoard->SetRAMByte(2, m_Port177010, HIBYTE(word));
            break;
        case 0177016:
        case 0177017:
            SetPortWord(address, word);
            break;
        case 0177020:
        case 0177021:
            SetPortWord(address, word);
            break;
        case 0177022:
        case 0177023:
            SetPortWord(address, word);
            break;
        case 0177024:
        case 0177025:
            SetPortWord(address, word);
            break;
        case 0177026:
        case 0177027:
            SetPortWord(address, word);
            break;
        case 0177054:
        case 0177055:
            SetPortWord(address, word);
            break;

        case 0177060:
        case 0177061:
        case 0177062:
        case 0177063:
        case 0177064:
        case 0177065:
            break;
        case 0177066:  // RX status, channels 0,1,2
            m_pBoard->ChanRxStateSetPPU((BYTE) word);
            break;
        case 0177067:
            m_pBoard->ChanRxStateSetPPU(0);
            break;
        case 0177070:  // TX data, channel 0
            m_pBoard->ChanWriteByPPU(0, (BYTE) word);
            break;
        case 0177071:
            m_pBoard->ChanWriteByPPU(0, 0);
            break;
        case 0177072:  // TX data, channel 1
            m_pBoard->ChanWriteByPPU(1, (BYTE) word);
            break;
        case 0177073:
            m_pBoard->ChanWriteByPPU(1, 0);
            break;
        case 0177074:
        case 0177075:
            break;
        case 0177076:  // TX status, channels 0,1
            m_pBoard->ChanTxStateSetPPU((BYTE) word);
            break;
        case 0177077:
            m_pBoard->ChanTxStateSetPPU(0);
            break;

        case 0177100:  // i8255 port A -- Parallel port output data
            m_Port177100 = word & 0xff;
            break;
        case 0177101:  // i8255 port B
            break;
        case 0177102:  // i8255 port C
            m_Port177102 = (m_Port177102 & 0x0f) | (word & 0xf0);
            break;
        case 0177103:  // i8255 control byte
            break;

        case 0177130:  // FDD status
        case 0177131:
            m_pBoard->SetFloppyState(word);
            break;
        case 0177132:  // FDD data
        case 0177133:
            m_pBoard->SetFloppyData(word);
            break;

        case 0177700:  // Keyboard status
        case 0177701:
            SetPortWord(address, word);
            break;
        case 0177704: // fdd params:
        case 0177705:
            break;

        case 0177710: //timer status
        case 0177711:
            m_pBoard->SetTimerState(word);
            break;
        case 0177712: //timer latch
        case 0177713:
            m_pBoard->SetTimerReload(word);
            break;
        case 0177714: //timer counter
        case 0177715:
            //m_pBoard->sett
            break;
        case 0177716:  // System control register
        case 0177717:
            SetPortWord(address, word);
            break;

            // HDD ports
        case 0110016:
        case 0110014:
        case 0110012:
        case 0110010:
        case 0110006:
        case 0110004:
        case 0110002:
        case 0110000:
            m_pBoard->SetHardPortWord(((m_Port177054 & 8) == 0) ? 1 : 2, address, word);
            break;

        default:
            m_pProcessor->MemoryError();
            //ASSERT(0);
            break;
    } 
}

void CSecondMemoryController::SetPortWord(WORD address, WORD word)
{
#if !defined(PRODUCT)
    TCHAR oct[7];
    TCHAR oct1[7];
    TCHAR oct2[7];
    PrintOctalValue(oct, word);
    PrintOctalValue(oct1, address);
    PrintOctalValue(oct2, m_pBoard->GetPPU()->GetPC());
//    TCHAR str[1024];
#endif

    if ((address>=0110000) && (address<0120000))
        address &= 0110016;
    switch (address) {
        case 0177010:  // Plane address register
        case 0177011:
            m_Port177010 = word;
            m_Port177012 = m_pBoard->GetRAMByte(0, word);
            m_Port177014 = MAKEWORD(
                    m_pBoard->GetRAMByte(1, word), m_pBoard->GetRAMByte(2, word));
            break;
        case 0177012:  // Plane 0 data register
        case 0177013:
            m_Port177012 = word & 0xFF;
            m_pBoard->SetRAMByte(0, m_Port177010, LOBYTE(word));
            break;
        case 0177014:  // Plane 1 & 2 data register
        case 0177015:
            m_Port177014 = word;
            m_pBoard->SetRAMByte(1, m_Port177010, LOBYTE(word));
            m_pBoard->SetRAMByte(2, m_Port177010, HIBYTE(word));
            break;

        case 0177016:  // Sprite Color
        case 0177017:
            m_Port177016 = word & 7;
            break;
        case 0177020:  // Background color code, plane 0,1,2 bits 0-3
        case 0177021:
            m_Port177020 = word;
            break;
        case 0177022:  // Background color code, plane 0,1,2 bits 4-7
        case 0177023:
            m_Port177022 = word;
            break;
        case 0177024:  // Pixel byte
        case 0177025:
            {
                m_Port177024 = word & 0xFF;
                // Convert background into planes... it could've been modified by user
                BYTE planebyte[3];
                planebyte[0]  = ((m_Port177020&(1<< 0))?1:0)<<0;
                planebyte[0] |= ((m_Port177020&(1<< 4))?1:0)<<1;
                planebyte[0] |= ((m_Port177020&(1<< 8))?1:0)<<2;
                planebyte[0] |= ((m_Port177020&(1<<12))?1:0)<<3;
                planebyte[0] |= ((m_Port177022&(1<< 0))?1:0)<<4;
                planebyte[0] |= ((m_Port177022&(1<< 4))?1:0)<<5;
                planebyte[0] |= ((m_Port177022&(1<< 8))?1:0)<<6;
                planebyte[0] |= ((m_Port177022&(1<<12))?1:0)<<7;

                planebyte[1]  = ((m_Port177020&(1<< 1))?1:0)<<0;
                planebyte[1] |= ((m_Port177020&(1<< 5))?1:0)<<1;
                planebyte[1] |= ((m_Port177020&(1<< 9))?1:0)<<2;
                planebyte[1] |= ((m_Port177020&(1<<13))?1:0)<<3;
                planebyte[1] |= ((m_Port177022&(1<< 1))?1:0)<<4;
                planebyte[1] |= ((m_Port177022&(1<< 5))?1:0)<<5;
                planebyte[1] |= ((m_Port177022&(1<< 9))?1:0)<<6;
                planebyte[1] |= ((m_Port177022&(1<<13))?1:0)<<7;

                planebyte[2]  = ((m_Port177020&(1<< 2))?1:0)<<0;
                planebyte[2] |= ((m_Port177020&(1<< 6))?1:0)<<1;
                planebyte[2] |= ((m_Port177020&(1<<10))?1:0)<<2;
                planebyte[2] |= ((m_Port177020&(1<<14))?1:0)<<3;
                planebyte[2] |= ((m_Port177022&(1<< 2))?1:0)<<4;
                planebyte[2] |= ((m_Port177022&(1<< 6))?1:0)<<5;
                planebyte[2] |= ((m_Port177022&(1<<10))?1:0)<<6;
                planebyte[2] |= ((m_Port177022&(1<<14))?1:0)<<7;
                // Draw spryte
                planebyte[0] &= ~m_Port177024;
                if (m_Port177016 & 1)
                    planebyte[0] |= m_Port177024;
                planebyte[1] &= ~m_Port177024;
                if (m_Port177016 & 2)
                    planebyte[1] |= m_Port177024;
                planebyte[2] &= ~m_Port177024;
                if (m_Port177016 & 4)
                    planebyte[2] |= m_Port177024;

                if ((m_Port177026 & 1) == 0)
                    m_pBoard->SetRAMByte(0, m_Port177010, planebyte[0]);
                if ((m_Port177026 & 2) == 0)
                    m_pBoard->SetRAMByte(1, m_Port177010, planebyte[1]);
                if ((m_Port177026 & 4) == 0)
                    m_pBoard->SetRAMByte(2, m_Port177010, planebyte[2]);
            }
            break;
        case 0177026:  // Pixel mask
        case 0177027:
            m_Port177026 = word & 7;
            break;
        case 0177054:  // Address space control
        case 0177055:
            //wsprintf(str,_T("W %s, %s\r\n"),oct1,oct);
            //DebugPrint(str);
            m_Port177054 = word & 01777;
            break;

        case 0177060:
        case 0177061:
        case 0177062:
        case 0177063:
        case 0177064:
        case 0177065:
            break;
        case 0177066:  // RX status, channels 0,1,2
        case 0177067:
            m_pBoard->ChanRxStateSetPPU((BYTE) word);
            break;
        case 0177070:  // TX data, channel 0
        case 0177071:
            m_pBoard->ChanWriteByPPU(0, (BYTE) word);
            break;
        case 0177072:  // TX data, channel 1
        case 0177073:
            m_pBoard->ChanWriteByPPU(1, (BYTE) word);
            break;
        case 0177074:
        case 0177075:
            break;
        case 0177076:  // TX status, channels 0,1
        case 0177077:
            m_pBoard->ChanTxStateSetPPU((BYTE) word);
            break;

        case 0177100:  // i8255 port A -- Parallel port output data
            m_Port177100 = word & 0xff;
            break;
        case 0177101:  // i8255 port B
            break;
        case 0177102:  // i8255 port C
            m_Port177102 = (m_Port177102 & 0x0f) | (word & 0xf0);
            break;
        case 0177103:  // i8255 control byte
            m_Port177100 = 0377;  // Writing to control register resets port A
            break;

        case 0177130:  // FDD status
        case 0177131:
            //ASSERT(word==0);
            //wsprintf(str,_T("FDD CMD   W %s, %s\r\n"), oct1,oct);
            //DebugLog(str);
            m_pBoard->SetFloppyState(word);
            break;
        case 0177132:  // FDD data
        case 0177133:
            //ASSERT(word==0);
            //wsprintf(str,_T("%s: FDD DATA W %s, %s\r\n"),oct2,oct1,oct);
            //wsprintf(str,_T("FDD DATA  W %04x\r\n"), word);
            //DebugLog(str);
            m_pBoard->SetFloppyData(word);
            break;

        case 0177700:  // Keyboard status
        case 0177701:
            if (((m_Port177700 & 0100) == 0) && (word & 0100) && (m_Port177700 & 0200))
				m_pProcessor->InterruptVIRQ(3, 0300);
			if ((word & 0100) == 0)
				m_pProcessor->InterruptVIRQ(3, 0);
			m_Port177700 = (m_Port177700 & 0177677) | (word & 0100);
            break;
        case 0177704: // fdd params:
        case 0177705:
#if !defined(PRODUCT)
            DebugLogFormat(_T("FDD 177704 W %s, %s, %s\r\n"), oct2, oct1, oct);
#endif
            break;

        case 0177710: //timer status
        case 0177711:
            m_pBoard->SetTimerState(word);
            break;
        case 0177712: //timer latch
        case 0177713:
            m_pBoard->SetTimerReload(word);
            break;
        case 0177714: //timer counter
        case 0177715:
            //m_pBoard->sett
            break;
        case 0177716:  // System control register
        case 0177717:
            word &= 0137676;
            m_pBoard->GetCPU()->SetHALTPin((word&020)?TRUE:FALSE);

            m_pBoard->GetCPU()->SetDCLOPin((word&040)?TRUE:FALSE);
        
            m_pBoard->GetCPU()->SetACLOPin((word&0100000)?FALSE:TRUE);

            m_Port177716 &= 1;
            m_Port177716 |= word;
            m_pBoard->SetSound(word);
            break;

        // HDD ports
        case 0110016:
        case 0110014:
        case 0110012:
        case 0110010:
        case 0110006:
        case 0110004:
        case 0110002:
        case 0110000:
            m_pBoard->SetHardPortWord(((m_Port177054 & 8) == 0) ? 1 : 2, address, word);
            break;

        default:
            m_pProcessor->MemoryError();
            //ASSERT(0);
            break;
    }
}

// Read word from port for debugger
WORD CSecondMemoryController::GetPortView(WORD address)
{
    switch (address) {
        case 0177010:  return m_Port177010;
        case 0177012:  return m_Port177012;
        case 0177014:  return m_Port177014;
        case 0177016:  return m_Port177016;
        case 0177020:  return m_Port177020;
        case 0177022:  return m_Port177022;
        case 0177024:  return m_Port177024;
        case 0177026:  return m_Port177026;
        case 0177054:  return m_Port177054;
        case 0177700:  return m_Port177700;
        case 0177716:  return m_Port177716;

        //TODO

        default:
            return 0;
    }
}

// Keyboard key pressed or released
void CSecondMemoryController::KeyboardEvent(BYTE scancode, BOOL okPressed)
{
    if (okPressed)
        m_Port177702 = (scancode & 0177);
    else
        m_Port177702 = (scancode & 017) | 0200;
    
    m_Port177700 |= 0200;  // Set bit 7 - "data ready" flag

    if ((m_Port177700 & 0100) != 0)  // Keyboard interrupt enabled
    {
        m_pProcessor->InterruptVIRQ(3, 0300);
    }
}

// A new bit from the tape input received
BOOL CSecondMemoryController::TapeInput(BOOL inputBit)
{
    BOOL res = FALSE;
    // Check port 177716 bit 2
    if ((m_Port177716 & 4) != 0)
    {
        // Check port 177716 bit 0 old state
        WORD tapeBitOld = (m_Port177716 & 1);
        WORD tapeBitNew = inputBit ? 0 : 1;
        if (tapeBitNew != tapeBitOld)
        {
            res = TRUE;
            m_Port177716 = (m_Port177716 & 0177776) | tapeBitNew;
            //if ((m_Port177716 & 8) == 0)
            {
                m_pProcessor->InterruptVIRQ(3, 0310);
            }
        }
    }
    return res;
}

BOOL CSecondMemoryController::TapeOutput()
{
    return (BOOL)(m_Port177716 & 2);
}

void CSecondMemoryController::DCLO_177716()
{
    m_Port177716 &= 0077717;
    m_pBoard->GetCPU()->SetHALTPin(FALSE);
    m_pBoard->GetCPU()->SetDCLOPin(FALSE);
    m_pBoard->GetCPU()->SetACLOPin(TRUE);
}

void CSecondMemoryController::Init_177716()
{
    m_Port177716 &= 0117461;
}


//////////////////////////////////////////////////////////////////////
//
// PPU memory/IO controller image format (64 bytes):
//   2*17 bytes     17 port registers
//   42 bytes       Not used

void CSecondMemoryController::SaveToImage(BYTE* pImage)
{
    WORD* pwImage = (WORD*) pImage;
    *pwImage++ = m_Port177010;
    *pwImage++ = m_Port177012;
    *pwImage++ = m_Port177014;

    *pwImage++ = m_Port177016;
    *pwImage++ = m_Port177020;
    *pwImage++ = m_Port177022;
    *pwImage++ = m_Port177024;
    *pwImage++ = m_Port177026;

    *pwImage++ = m_Port177700;
    *pwImage++ = m_Port177702;
    *pwImage++ = m_Port177716;

    *pwImage++ = m_Port177054;
}
void CSecondMemoryController::LoadFromImage(const BYTE* pImage)
{
    WORD* pwImage = (WORD*) pImage;
    m_Port177010 = *pwImage++;
    m_Port177012 = *pwImage++;
    m_Port177014 = *pwImage++;

    m_Port177016 = *pwImage++;
    m_Port177020 = *pwImage++;
    m_Port177022 = *pwImage++;
    m_Port177024 = *pwImage++;
    m_Port177026 = *pwImage++;

    m_Port177700 = *pwImage++;
    m_Port177702 = *pwImage++;
    m_Port177716 = *pwImage++;

    m_Port177054 = *pwImage++;
}


//////////////////////////////////////////////////////////////////////
