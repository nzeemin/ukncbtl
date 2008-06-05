// Memory.cpp
//

#include "StdAfx.h"
#include "Memory.h"
#include "Board.h"
#include "Processor.h"
#include "..\Views.h"


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
    int addrtype = TranslateAddress(address, okHaltMode, okExec, &offset);

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
        return m_pBoard->GetRAMWord((addrtype & ADDRTYPE_MASK_RAM), offset & 0177776);
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
	
	if((address==0177777)&&(word==0177777))
	{
		if(m_pBoard->ChanRxStateGetPPU()&0x40)
			m_pBoard->GetPPU()->InterruptVIRQ(4, 0314);
		return;
	}

    int addrtype = TranslateAddress(address, okHaltMode, FALSE, &offset);

	//ASSERT( (address!=0157552) || (word!=0157272));
    switch (addrtype)
    {
    case ADDRTYPE_RAM0:
    case ADDRTYPE_RAM1:
    case ADDRTYPE_RAM2:
        m_pBoard->SetRAMWord((addrtype & ADDRTYPE_MASK_RAM), offset & 0177776, word);
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
// 000000-157777 ÎÇÓ - USER - read/write/execute
//
// 174000-177777 I/O - HALT - read/write
// 174000-177777 ÎÇÓ - HALT - execute
// 160000-173777 ÎÇÓ - HALT - read/write/execute
// 000000-157777 ÎÇÓ - HALT - read/write/execute
//
// For RAM access, bytes at even addresses (low byte of word) belongs to plane 1,
// and bytes at odd addresses (high byte of word) - belongs to plane 2.

CFirstMemoryController::CFirstMemoryController() : CMemoryController()
{
    Reset();
}

void CFirstMemoryController::Reset()
{
    m_Port176640 = 0;
    m_Port176642 = 0;
    m_Port177560 = 0;
    m_Port177562 = 0;
    m_Port177564 = 0;
    m_Port176660 = 0;
    m_Port176662 = 0;
    m_Port176664 = 0;
    m_Port176674 = 0;
    m_Port177570 = 0;
}

int CFirstMemoryController::TranslateAddress(WORD address, BOOL okHaltMode, BOOL okExec, WORD* pOffset)
{
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
		{  // USER mode
            /* if (address <= 0173777) 
			{  // 160000-173777 - access denied
                *pOffset = 0;
				m_pProcessor->MemoryError();
                return ADDRTYPE_DENY;
            }
            else 
			{  // 174000-177777 in USER mode
            */    
				*pOffset = address;
                return ADDRTYPE_IO;
            // }
        }
    }

    ASSERT(FALSE);  // If we are here - then if isn't cover all addresses
    return 0;
}

BYTE CFirstMemoryController::GetPortByte(WORD address)
{
	if(address&1)
		return GetPortWord(address&0xfffe)>>8;

	return (BYTE) GetPortWord(address);
}

WORD CFirstMemoryController::GetPortWord(WORD address)
{
    switch (address) {
        case 0176640:  			
			return m_Port176640;  // Plane address register
        case 0176642:  
				m_Port176642 = MAKEWORD(m_pBoard->GetRAMByte(1, m_Port176640), m_pBoard->GetRAMByte(2, m_Port176640));
			return m_Port176642;  // Plane 1 & 2 data register

		case 0177564:
			return m_pBoard->ChanTxStateGetCPU(0);
		case 0177560:
			//m_Port177570=
			return m_pBoard->ChanRxStateGetCPU(0);
		case 0177570:
			return m_Port177570;
		case 0177562:
			return m_pBoard->ChanReadByCPU(0);
		case 0176664:
			return m_pBoard->ChanTxStateGetCPU(1);
		case 0176660:
			return m_pBoard->ChanRxStateGetCPU(1);
		case 0176662:
			return m_pBoard->ChanReadByCPU(1);
		case 0176674:
			return m_pBoard->ChanTxStateGetCPU(2);
		case 0177566: 
		case 0176666:
		case 0176676:
			return 0;

		case 0176560: //network 
		case 0176562:
		
		case 0176566:
			return 0;
		case 0176570: //rs232
		case 0176572:
		
		case 0176576:
			return 0;
		
		case 0176564:
		case 0176574:
			return 0x80; //ready for tx

		default: 
			m_pProcessor->MemoryError();
			return 0x0;
        //TODO
    }
	//ASSERT(0);
    return 0; 
}

void CFirstMemoryController::ResetAll()
{

}

// Read word from port for debugger
WORD CFirstMemoryController::GetPortView(WORD address)
{
    switch (address) {
        case 0176640:  return m_Port176640;  // Plane address register
        case 0176642:  return m_Port176642;  // Plane 1 & 2 data register

        case 0177560:  return m_Port177560;  // Channel 0 RX status
        case 0177562:  return m_Port177562;  // Channel 0 RX data
        case 0177564:  return m_Port177564;  // Channel 0 TX status
        case 0176660:  return m_Port176660;  // Channel 1 RX status
        case 0176662:  return m_Port176662;  // Channel 1 RX data
        case 0176664:  return m_Port176664;  // Channel 1 TX status
        case 0176674:  return m_Port176674;  // Channel 2 TX status

        //TODO

        default:
            return 0;
    }
}

void CFirstMemoryController::SetPortByte(WORD address, BYTE byte)
{
	if(address&1)
	{
		WORD word;
		word=GetPortWord(address&0xfffe);
		word&=0xff;
		word|=byte<<8;
		SetPortWord(address&0xfffe,word);
	}
	else
	{
		WORD word;
		word=GetPortWord(address);
		word&=0xff00;
		SetPortWord(address,word|byte);
	}
}

void CFirstMemoryController::SetPortWord(WORD address, WORD word)
{
    switch (address) {
        case 0176640:  // Plane address register
            m_Port176640 = word;
            m_Port176642 = MAKEWORD(
                    m_pBoard->GetRAMByte(1, m_Port176640), m_pBoard->GetRAMByte(2, m_Port176640));
            break;
        case 0176642:  // Plane 1 & 2 data register
            m_Port176642 = word;
            m_pBoard->SetRAMByte(1, m_Port176640, LOBYTE(word));
            m_pBoard->SetRAMByte(2, m_Port176640, HIBYTE(word));
            break;
		case 0177560:
			m_pBoard->ChanRxStateSetCPU(0, (BYTE) word);
			break;
		case 0177564:
			m_pBoard->ChanTxStateSetCPU(0, (BYTE) word);
			break;
		case 0177566:  // TX data, channel 0
			m_pBoard->ChanWriteByCPU(0, (BYTE) word);
			break;
		case 0176660:
			m_pBoard->ChanRxStateSetCPU(1, (BYTE) word);
			break;
		case 0176664:
			m_pBoard->ChanTxStateSetCPU(1, (BYTE) word);
			break;
		case 0176666:  // TX data, channel 1
			m_pBoard->ChanWriteByCPU(1, (BYTE) word);
			break;
		case 0176674:
			m_pBoard->ChanTxStateSetCPU(2, (BYTE) word);
			break;
		case 0176676:  // TX data, channel 2
			m_pBoard->ChanWriteByCPU(2, (BYTE) word);
			break;

		case 0176560: //network 
		case 0176562:
		case 0176564:
		case 0176566:
			return ;
		case 0176570: //rs232
		case 0176572:
		case 0176574:
		case 0176576:
			return ;

		case 0177562:
			return ;
		case 0176662:
			return ;


		default:
			m_pProcessor->MemoryError();
//			ASSERT(0);
			break;
        //TODO
    }
}

// Receive byte sent via inter-processor channel
void CFirstMemoryController::ReceiveChannelByte(int channel, BYTE data)
{
    switch (channel)
    {
    case 0:
        m_Port177562 = (WORD) data;
        m_Port177560 |= 0100;  // "Data ready" flag
        if ((m_Port177560 & 0200) != 0)  // Interrupt enabled
            m_pProcessor->InterruptVIRQ(1, 060);
        break;
    case 1:
        m_Port176662 = (WORD) data;
        m_Port176660 |= 0100;  // "Data ready" flag
        if ((m_Port176660 & 0200) != 0)  // Interrupt enabled
            m_pProcessor->InterruptVIRQ(3, 0460);
        break;
    default:
        ASSERT(FALSE);  // Wrong channel number
    }
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
    *pwImage++ = m_Port177560;
    *pwImage++ = m_Port177562;
    *pwImage++ = m_Port177564;
    *pwImage++ = m_Port176660;
    *pwImage++ = m_Port176662;
    *pwImage++ = m_Port176664;
    *pwImage++ = m_Port176674;
    *pwImage++ = m_Port177570;
}
void CFirstMemoryController::LoadFromImage(const BYTE* pImage)
{
    WORD* pwImage = (WORD*) pImage;
    m_Port176640 = *pwImage++;
    m_Port176642 = *pwImage++;
    m_Port177560 = *pwImage++;
    m_Port177562 = *pwImage++;
    m_Port177564 = *pwImage++;
    m_Port176660 = *pwImage++;
    m_Port176662 = *pwImage++;
    m_Port176664 = *pwImage++;
    m_Port176674 = *pwImage++;
    m_Port177570 = *pwImage++;
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
    Reset();
}

void CSecondMemoryController::Reset()
{
    m_Port177010 = m_Port177012 = m_Port177014 = 0;

	m_Port177026 = m_Port177024 = 0;
    m_Port177020 = m_Port177022 = 0;
    m_Port177016 = 0;

    m_Port177700 = m_Port177702 = 0;
    m_Port177716 = 0;
    m_Port177076 = m_Port177066 = 0;
    m_Port177060 = m_Port177062 = m_Port177064 = 0;

	m_Port177054 = 01401;
}

int CSecondMemoryController::TranslateAddress(WORD address, BOOL okHaltMode, BOOL okExec, WORD* pOffset)
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
				int bank = (m_Port177054 & 6) >> 1;
				*pOffset = address - 0100000 + ((bank - 1) << 13);
				if ((m_Port177054 & 8) == 0)
					return ADDRTYPE_ROMCART1;
				else
					return ADDRTYPE_ROMCART2;
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
        case 0177010:  return m_Port177010;  // Plane address register
        case 0177012:  

           //m_Port177012 = m_pBoard->GetRAMByte(0, m_Port177010);
			return m_Port177012;  // Plane 0 data register
        case 0177700:  return m_Port177700;  // Keyboard status
        case 0177702:  // Keyboard data
            m_Port177700 &= ~0200;  // Reset bit 7 - "data ready" flag
            return m_Port177702;
        case 0177014:  
           // m_Port177014 = MAKEWORD(m_pBoard->GetRAMByte(1, m_Port177010), m_pBoard->GetRAMByte(2, m_Port177010));

			
			return m_Port177014;  // Plane 1 & 2 data register
        case 0177716:  return m_Port177716;  // System control register

        case 0177026:  return m_Port177026;  // Plane Mask
        case 0177020:  return m_Port177020;  // Plane 0,1,2 bits 0-3
        case 0177022:  return m_Port177022;  // Plane 0,1,2 bits 4-7
        case 0177016:  return m_Port177016;  // Sprite Color
        case 0177024:  // Load background registers
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

        case 0177710:
            return m_pBoard->GetTimerState();
        case 0177712:
            return m_pBoard->GetTimerReload();
        case 0177714:
            return m_pBoard->GetTimerValue();

		case 0177054:
			return m_Port177054;
	
		case 0177060:
			return m_pBoard->ChanReadByPPU(0);
		case 0177062:
			return m_pBoard->ChanReadByPPU(1);
		case 0177064:
			return m_pBoard->ChanReadByPPU(2);
		case 0177066:
			return m_pBoard->ChanRxStateGetPPU();
		case 0177076:
			return m_pBoard->ChanTxStateGetPPU();
		case 0177130: //fdd status
            value = m_pBoard->GetFloppyState();
            //PrintOctalValue(oct2, value);
			//wsprintf(str, _T("FDD STATE R %s, %s\r\n"), oct1, oct2);
			//DebugLog(str);
			return value;
		case 0177132: //fdd data
            value = m_pBoard->GetFloppyData();
			//wsprintf(str,_T("FDD DATA  R %s, %04x\r\n"), oct1, value);
			//DebugLog(str);
			return value;
		case 0177704:
			//fdd related
//#if !defined(PRODUCT)
//			DebugLogFormat(_T("FDD 177704 R %s, %s\r\n"), oct2, oct1);
//#endif
			return 010000; //!!!
		default:
//			ASSERT(0);
			break;
        //TODO

    }
//	ASSERT(0);
    return 0; 
}

BYTE CSecondMemoryController::GetPortByte(WORD address)
{
	if(address&1)
	{
		WORD word;

		word=GetPortWord(address&0xfffe);
		return (word>>8)&0xff;
	}
    return (BYTE) GetPortWord(address); 
}

void CSecondMemoryController::SetPortByte(WORD address, BYTE byte)
{
	WORD word;
	word = GetPortWord(address & 0xfffe);
	if (address & 1)
	{
		word &= 0xff;
		word |= (byte << 8);
		SetPortWord(address & 0xfffe, word);
	}
	else
	{
		word &= 0xff00;
		SetPortWord(address, word | byte);
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

    switch (address) {
        case 0177010:  // Plane address register
            m_Port177010 = word;
            m_Port177012 = m_pBoard->GetRAMByte(0, word);
            m_Port177014 = MAKEWORD(
                    m_pBoard->GetRAMByte(1, word), m_pBoard->GetRAMByte(2, word));
            break;
        case 0177012:  // Plane 0 data register
            m_Port177012 = word;
            m_pBoard->SetRAMByte(0, m_Port177010, LOBYTE(word));
            break;
        case 0177014:  // Plane 1 & 2 data register
			m_Port177014 = word;
            m_pBoard->SetRAMByte(1, m_Port177010, LOBYTE(word));
            m_pBoard->SetRAMByte(2, m_Port177010, HIBYTE(word));
            break;

        case 0177016:  // Sprite Color
            m_Port177016 = word;
            break;
        case 0177020:  // Background color code, plane 0,1,2 bits 0-3
            m_Port177020 = word;
            break;
        case 0177022:  // Background color code, plane 0,1,2 bits 4-7
            m_Port177022 = word;
            break;
        case 0177024:  // Pixel byte
            {
                m_Port177024 = word;
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
            
            m_Port177026 = word;
            break;
        case 0177054:  // Address space control
            //wsprintf(str,_T("W %s, %s\r\n"),oct1,oct);
            //DebugPrint(str);
			m_Port177054 = word;
            break;
        case 0177066:  // RX status, channels 0,1,2
			m_pBoard->ChanRxStateSetPPU((BYTE) word);
            break;
        case 0177070:  // TX data, channel 0
			m_pBoard->ChanWriteByPPU(0, (BYTE) word);
			break;
        case 0177072:  // TX data, channel 1
			m_pBoard->ChanWriteByPPU(1, (BYTE) word);
			break;
        case 0177076:  // TX status, channels 0,1
			m_pBoard->ChanTxStateSetPPU((BYTE) word);
            break;
        case 0177130:  // FDD status
			//ASSERT(word==0);
			//wsprintf(str,_T("FDD CMD   W %s, %s\r\n"), oct1,oct);
			//DebugLog(str);
			m_pBoard->SetFloppyState(word);
            break;
        case 0177132:  // FDD data
			//ASSERT(word==0);
			//wsprintf(str,_T("%s: FDD DATA W %s, %s\r\n"),oct2,oct1,oct);
            //wsprintf(str,_T("FDD DATA  W %04x\r\n"), word);
			//DebugLog(str);
			m_pBoard->SetFloppyData(word);
			break;

		case 0177704: // fdd params:
#if !defined(PRODUCT)
			DebugLogFormat(_T("FDD 177704 W %s, %s, %s\r\n"), oct2, oct1, oct);
#endif
			break;

        case 0177700:  // Keyboard status
            m_Port177700 = (m_Port177700 & 0177677) | (word & 0100);
            break;

        case 0177710: //timer status
			m_pBoard->SetTimerState(word);
            break;
        case 0177712: //timer latch
			m_pBoard->SetTimerReload(word);
            break;
        case 0177714: //timer counter
			//m_pBoard->sett
            break;
        case 0177716:  // System control register
            
			if(word&020)
				m_pBoard->GetCPU()->AssertHALT();
			else
				m_pBoard->GetCPU()->DeassertHALT();

			//else
			//	m_pBoard->GetCPU()->SetPSW(m_pBoard->GetCPU()->GetPSW()&(~0x100));

			if(word&040)
				m_pBoard->GetCPU()->Stop();
			else
			{//prepare == int 340????
				//m_pBoard->GetCPU()->SetPSW(m_pBoard->GetCPU()->GetPSW()|0x100); //logically should be in halt
			}
		
            if ((m_Port177716 & 32768) == 0 && (word & 32768))  // Start CPU
            {
				m_Port177716 = word;
                if (m_pBoard->GetCPU()->IsStopped())
				{
					m_pBoard->GetCPU()->Start();
					if(m_pBoard->ChanRxStateGetPPU()&0x40)
						m_pBoard->GetPPU()->InterruptVIRQ(4, 0314);
				}
            }
			else
			{
				m_Port177716 = word;
				if ((word & 32768) == 0) m_pBoard->GetCPU()->PowerFail();
			}

            m_Port177716 = word;
			m_pBoard->SetSound(word);
            break;
		case 0177102: //par port data
			break;
		case 0177103: //par port ctrl
			break;
		default:
        //TODO
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
        case 0177076:  return m_Port177076;
        case 0177066:  return m_Port177066;
        case 0177060:  return m_Port177060;
        case 0177062:  return m_Port177062;
        case 0177064:  return m_Port177064;

        //TODO

        default:
            return 0;
    }
}

// Receive byte sent via inter-processor channel
void CSecondMemoryController::ReceiveChannelByte(int channel, BYTE data)
{
	ASSERT(0);
    switch (channel)
    {
    case 0:
        m_Port177060 = (WORD) data;
        m_Port177066 |= 010;  // "Data ready" flag
        if ((m_Port177066 & 1) != 0)  // Interrupt enabled
            m_pProcessor->InterruptVIRQ(5, 0320);
        break;
    case 1:
        m_Port177062 = (WORD) data;
        m_Port177066 |= 020;  // "Data ready" flag
        if ((m_Port177066 & 2) != 0)  // Interrupt enabled
            m_pProcessor->InterruptVIRQ(7, 0330);
        break;
    case 2:
        m_Port177064 = (WORD) data;
        m_Port177066 |= 040;  // "Data ready" flag
        if ((m_Port177066 & 4) != 0)  // Interrupt enabled
            m_pProcessor->InterruptVIRQ(9, 0340);
        break;
    default:
        ASSERT(FALSE);  // Wrong channel number
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
    *pwImage++ = m_Port177076;
    *pwImage++ = m_Port177066;
    *pwImage++ = m_Port177060;
    *pwImage++ = m_Port177062;
    *pwImage++ = m_Port177064;

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
    m_Port177076 = *pwImage++;
    m_Port177066 = *pwImage++;
    m_Port177060 = *pwImage++;
    m_Port177062 = *pwImage++;
    m_Port177064 = *pwImage++;

    m_Port177054 = *pwImage++;
}


//////////////////////////////////////////////////////////////////////
