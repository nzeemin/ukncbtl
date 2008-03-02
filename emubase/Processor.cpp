// Processor.cpp
//

#include "StdAfx.h"
#include "..\Emulator.h"
#include "Processor.h"


// Timings ///////////////////////////////////////////////////////////

//#define PI_TIME_BASE      40
//#define PI_TIME_HALT     280
//#define PI_TIME_RTI      134
//#define PI_TIME_RESET   3800
//#define PI_TIME_BR_BASE   54
//#define PI_TIME_RTS      108
//#define PI_TIME_MARK     118
//#define PI_TIME_EMT      228
//#define PI_TIME_SOB       68
//#define PI_TIME_INT      200
//
//WORD timing_A_A1[8]     ={0,  40,  40,  68,  40,  68,  68, 94}; 
//WORD timing_AB[8]       ={0,  54,  54,  80,  54,  80,  80, 108};
//WORD timing_B[8]        ={0,  68,  68, 108,  68, 108, 108, 134};
//WORD timing_A2_Nj[8]    ={0,  68,  68,  94,  68,  94,  94, 120};
//WORD timing_Ns[8]       ={0, 108, 108, 134, 108, 134, 134, 160};


//////////////////////////////////////////////////////////////////////

CProcessor::CProcessor (TCHAR * name)

{
    lstrcpy(NAME, name);
    m_psw = 0400;  // Start value of PSW is 340
    ZeroMemory(m_R, sizeof(m_R));
	m_psw = 0400;  // Start value of PSW is 340
    m_savepc = m_savepsw = 0;
    m_okStopped = TRUE;
    m_internalTick = 0;
    m_pMemoryController = NULL;
	m_eqreadptr=0;
	m_eqwriteptr=0;
	m_eqcount=0;
	m_waitmode=0;
	m_traprq=0;
	m_trap=0;
	m_userspace=0;
	m_stepmode=0;
	m_virqrq=0;
	memset(m_virq, 0, sizeof(m_virq));
	m_evntrq=0;
	m_ACLOrq = 0;
	if(NAME[0]==_T('C'))
		m_haltpin=0;
	else
		m_haltpin=0;
}

void CProcessor::Start ()
{
    m_okStopped = FALSE;
    m_internalTick = 0;

	m_eqreadptr=0;
	m_eqwriteptr=0;
	m_eqcount=0;
	m_userspace=0;
	m_stepmode=0;
	m_waitmode=0;
	m_virqrq=0;
	memset(m_virq, 0, sizeof(m_virq));
	m_evntrq=0;
	m_ACLOrq=0;

    // Calculate start vector value
    WORD startvec = m_pMemoryController->GetSelRegister() & 0177400;
    // "Turn On" interrupt processing
    WORD pc = GetWord(startvec);
    SetPC( pc );
    WORD psw = GetWord(startvec + 2);
    SetPSW( psw );
	if(m_haltpin)
		m_psw |= 0400;
    //TODO: Make sure we implemented start process correctly
}
void CProcessor::Stop ()
{
    m_okStopped = TRUE;

	m_eqreadptr=0;
	m_eqwriteptr=0;
	m_eqcount=0;
	m_userspace=0;
	m_stepmode=0;
	m_psw = 0400;  // Start value of PSW is 340
    m_savepc = m_savepsw = 0;
    m_okStopped = TRUE;
    m_internalTick = 0;
	m_eqreadptr=0;
	m_eqwriteptr=0;
	m_eqcount=0;
	m_traprq=0;
	m_evntrq=0;
	m_ACLOrq=0;
	m_trap=0;
	m_virqrq=0;
	memset(m_virq, 0, sizeof(m_virq));
	m_waitmode=0;
	m_userspace=0;
	m_stepmode=0;
	if(NAME[0]==_T('C'))
		m_haltpin=0;
	else
		m_haltpin=0;

}


void CProcessor::Execute()
{
	WORD	intr;

    if (m_okStopped) return;  // Processor is stopped - nothing to do

    if (m_internalTick > 0)
    {
        m_internalTick--;
        return;
    }
    m_internalTick = 24;  //TODO: Implement real timings
	
	if(m_waitmode==0)
		TranslateInstruction();  // Execute next instruction

	ASSERT(m_psw<0777);
	
	if(m_stepmode==0)
	{
		intr=0;
		if(m_traprq)
		{
			if ((m_trap == INTERRUPT_4) && ((m_psw & 0400) != 0))
				intr=0160004;
			else
				intr=m_trap;
			m_traprq=0;
		}
		else
		if(m_psw&020)
		{
			intr=014;
		}
		else
		if ((m_ACLOrq) && ((m_psw&0600)!=0600))
		{
			m_ACLOrq=0;
			intr=024;
		}
		else
		if((m_haltpin)&&((m_psw&0400)==0))
		{
			intr=0160170;
		}
		else
		if((m_evntrq)&&((m_psw&0200)==0))
		{
			m_evntrq=0;
			intr=0100;
		}
		else
		if((m_virqrq != 0)&&((m_psw&0200)==0))
		{
			// intr=m_virq;
			// m_virqrq=0;
		 int irq;
		 for (irq = 0; irq<=15; irq++)
		  {
		   if (m_virq[irq] != 0)
		    {
		     intr = m_virq[irq];
			 m_virq[irq] = 0;
			 m_virqrq--;
			 break;
		    }
		  }
         if (intr == 0) m_virqrq = 0;
		}
		
		if(intr)
		{
			m_waitmode=0;
			if(intr>=0160000)
			{
				m_savepc=GetPC();
				m_savepsw=GetPSW();
                m_psw |= 0400;
            }
			else
			{
				SetSP(GetSP() - 2);
				SetWord(GetSP(), m_psw);
				SetSP(GetSP() - 2);
				SetWord(GetSP(), GetPC());
			}
			SetPC(GetWord(intr));
			if (intr >= 0160000)
				m_psw = GetWord(intr + 2) & 0777;
			else
				m_psw = GetWord(intr + 2) & 0377;
			/*	
			switch(m_trap&0777)
			{
				case INTERRUPT_20: //IOT
				case INTERRUPT_30: //EMT
				case INTERRUPT_34: //TRAP
					m_psw = GetWord(intr + 2) & 0177377; // Drop the HALT mode
				break;
				case INTERRUPT_4:
					m_psw = GetWord(intr + 2);
				break;
				default:
					m_psw = GetWord(intr + 2)&0777;
				break;
			}
        */
		}
	}
	else
		m_stepmode=0;

	//	ASSERT((GetPC()!=0147002));
}
void CProcessor::TickEVNT()
{
    if (m_okStopped) return;  // Processor is stopped - nothing to do

	m_evntrq=1;

}

void CProcessor::PowerFail()
{
    if (m_okStopped) return;  // Processor is stopped - nothing to do

	m_ACLOrq=1;

}

void CProcessor::InterruptVIRQ(int que, WORD interrupt)
{
    if (m_okStopped) return;  // Processor is stopped - nothing to do
	// if (m_virqrq == 1)
	// {
	//  DebugPrintFormat(_T("Lost VIRQ %d %d\r\n"), m_virq, interrupt);
	// }
	m_virqrq += 1;
	m_virq[que] = interrupt;
}
void CProcessor::AssertHALT()
{
	m_haltpin=1;
}

void CProcessor::DeassertHALT()
{
	m_haltpin=0;
}

void CProcessor::MemoryError()
{
			m_traprq=1;
			m_trap=4; //normal HALT
}

//void CProcessor::QueueInterrupt(WORD interrupt, int priority)
//{
//	ASSERT(m_eqcount<MAXEVTQUEUE);
//	if(m_eqcount<MAXEVTQUEUE)
//	{
//        m_eventqueue[m_eqwriteptr]=interrupt;
//	    m_eqwriteptr++;
//	    if(m_eqwriteptr>=MAXEVTQUEUE)
//			m_eqwriteptr=0;
//	    m_eqcount++;
//	}
//}

void CProcessor::MakeInterrupt(WORD interrupt)
{
	m_traprq=1;
	m_trap=interrupt;
	//m_internalTick += PI_TIME_INT;
}


//////////////////////////////////////////////////////////////////////


// Вычисление адреса операнда, в зависимости от метода адресации
//   meth - метод адресации
//   reg  - номер регистра
WORD CProcessor::CalculateOperAddrSrc (int meth, int reg)
{
	WORD arg;

	
		switch (meth) {
			case 0:  // R0,     PC 
				return GetReg(reg);
			case 1:  // (R0),   (PC)
				return GetReg(reg);
			case 2:  // (R0)+,  #012345
				//if(reg==7) // is it immediate?
				//	arg = GetWord(GetReg(reg));
				//else
					arg = GetReg(reg);
				if ((m_instruction & 0100000)&&(reg<6))
					SetReg(reg, GetReg(reg) + 1);
				else
					SetReg(reg, GetReg(reg) + 2);
				return arg;
			case 3:  // @(R0)+, @#012345
				//if(reg==7) //abs index
				//	arg =  GetWord(GetWord(GetReg(reg))) ;
				//else
					arg =  GetWord(GetReg(reg)) ;
				//if ((m_instruction & 0100000)&&(reg!=7))
			//		SetReg(reg, GetReg(reg) + 1);
		//		else
					SetReg(reg, GetReg(reg) + 2);
				return arg;
			case 4:  // -(R0),  -(PC)
				if ((m_instruction & 0100000)&&(reg<6))
					SetReg(reg, GetReg(reg) - 1);
				else
					SetReg(reg, GetReg(reg) - 2);
				return GetReg(reg);
			case 5:  // @-(R0), @-(PC)
		//		if (m_instruction & 0100000)
		//			SetReg(reg, GetReg(reg) - 1);
		//		else
					SetReg(reg, GetReg(reg) - 2);
				return  GetWord(GetReg(reg));
			case 6: {  // 345(R0),  345
				WORD pc=0;
				//if(reg==7) //relative direct
				//	pc = GetWord(GetWordExec( GetPC() ));
				//else
				    pc = (GetWordExec( GetPC() ));

				SetPC( GetPC() + 2 );
				arg=(WORD)(pc + GetReg(reg));
				return arg;
			}
			case 7: {  // @345(R0),@345
				WORD pc;
				//if(reg==7) //relative direct
				//	pc = GetWord(GetWordExec( GetPC() ));
				//else
				    pc = GetWordExec( GetPC() );
				SetPC( GetPC() + 2 );
				arg=( GetWord(pc + GetReg(reg)) );
				return arg;
			}
		}
	
    return 0;

}

WORD CProcessor::CalculateOperAddr (int meth, int reg)
{


	WORD arg;
		switch (meth) {
			case 0:  // R0,     PC 
				return reg;
			case 1:  // (R0),   (PC)
				return GetReg(reg);
			case 2:  // (R0)+,  #012345
				//if(reg==7) // is it immediate?
				//	arg = GetWord(GetReg(reg));
				//else
					arg = GetReg(reg);
				if ((m_instruction & 0100000)&&(reg<6))
					SetReg(reg, GetReg(reg) + 1);
				else
					SetReg(reg, GetReg(reg) + 2);
				return arg;
			case 3:  // @(R0)+, @#012345
				//if(reg==7) //abs index
					//arg =  GetWord(GetWord(GetReg(reg))) ;
				//else
					arg =  GetWord(GetReg(reg)) ;
				//if ((m_instruction & 0100000)&&(reg!=7))
				//	SetReg(reg, GetReg(reg) + 1);
				//else
					SetReg(reg, GetReg(reg) + 2);
				return arg;
			case 4:  // -(R0),  -(PC)
				if ((m_instruction & 0100000)&&(reg<6))
					SetReg(reg, GetReg(reg) - 1);
				else
					SetReg(reg, GetReg(reg) - 2);
				return GetReg(reg);
			case 5:  // @-(R0), @-(PC)
				//if (m_instruction & 0100000)
				//	SetReg(reg, GetReg(reg) - 1);
				//else
					SetReg(reg, GetReg(reg) - 2);
				return  GetWord(GetReg(reg));
			case 6: {  // 345(R0),  345
				WORD pc=0;
				//if(reg==7) //relative direct
				// pc = GetWord(GetWordExec( GetPC() ));
				//else
				 pc = (GetWordExec( GetPC() ));

				SetPC( GetPC() + 2 );
				arg=(WORD)(pc + GetReg(reg));
				return arg;
			}
			case 7: {  // @345(R0),@345
				WORD pc=0;
				//if(reg==7)
				//	pc = GetWord(GetWordExec( GetPC() ));
				//else
					pc = GetWordExec( GetPC() );
				SetPC( GetPC() + 2 );
				arg=( GetWord(pc + GetReg(reg)) );
				return arg;
			}
		}
    return 0;
}


BYTE CProcessor::GetByteSrc ()
{
    if (m_methsrc == 0)
        return (BYTE) GetReg(m_regsrc)&0377;
    else
        return GetByte( m_addrsrc );
}
BYTE CProcessor::GetByteDest ()
{
    if (m_methdest == 0)
        return (BYTE) GetReg(m_regdest);
    else
        return GetByte( m_addrdest );
}

void CProcessor::SetByteDest (BYTE byte)
{
    if (m_methdest == 0)
	{
		if(byte&0200)
			SetReg(m_regdest, 0xff00|byte);
		else
			SetReg(m_regdest, (GetReg(m_regdest)&0xff00)|byte);
	}
    else
        SetByte( m_addrdest, byte );
}

WORD CProcessor::GetWordSrc ()
{
    if (m_methsrc == 0)
	{
        return GetReg(m_regsrc);
	}
    else
        return GetWord( m_addrsrc );
}
WORD CProcessor::GetWordDest ()
{
    if (m_methdest == 0)
        return GetReg(m_regdest);
    else
        return GetWord( m_addrdest );
}

void CProcessor::SetWordDest (WORD word)
{
    if (m_methdest == 0)
        SetReg(m_regdest, word);
    else
        SetWord( (m_addrdest), word );
}

WORD CProcessor::GetDstWordArgAsBranch ()
{
	int reg = GetDigit(m_instruction, 0);
	int meth = GetDigit(m_instruction, 1);
	WORD arg;

	    switch (meth) {
        case 0:  // R0,     PC 
			ASSERT(0);
            return 0;
        case 1:  // (R0),   (PC)
            return GetReg(reg);
        case 2:  // (R0)+,  #012345
            arg = GetReg(reg);
			SetReg(reg, GetReg(reg) + 2);
            return arg;
        case 3:  // @(R0)+, @#012345
            arg = GetWord( GetReg(reg) );
				SetReg(reg, GetReg(reg) + 2);
            return arg;
        case 4:  // -(R0),  -(PC)
				SetReg(reg, GetReg(reg) - 2);
            return GetReg(reg);
        case 5:  // @-(R0), @-(PC)
				SetReg(reg, GetReg(reg) - 2);
            return GetWord( GetReg(reg) );
        case 6: {  // 345(R0),  345
            WORD pc = GetWordExec( GetPC() );
            SetPC( GetPC() + 2 );
            return (WORD)(pc + GetReg(reg));
        }
        case 7: {  // @345(R0),@345
            WORD pc = GetWordExec( GetPC() );
            SetPC( GetPC() + 2 );
            return GetWord( (WORD)(pc + GetReg(reg)) );
        }
    }


	return 0;
}


//////////////////////////////////////////////////////////////////////

void CProcessor::TranslateInstruction ()
{
//#ifdef _DEBUG
    TCHAR buffer[20];
	WORD pc;
//    PrintOctalValue(buffer, GetPC());
//    DebugLog(buffer);
//#endif // _DEBUG
	//if(GetPC()==0116632)
	//{
	//	SetWord(022550,4);
	//}
	/*if(GetPC()==0104156)
	{
		DebugPrint(_T("key return ="));
		PrintOctalValue(buffer,GetReg(0));
		DebugPrint(buffer);
		DebugPrint(_T("\r\n"));
	}
	else
	if(GetPC()==0133306)
	{
		DebugPrint(_T(">>>FDD RQ sect ="));
		PrintOctalValue(buffer,GetByte(023341));
		DebugPrint(buffer);
		DebugPrint(_T(" AQ sect ="));
		PrintOctalValue(buffer,GetReg(0)&0xff);
		DebugPrint(buffer);
		DebugPrint(_T("\r\n"));	
	}*/
		
	
    // Считываем очередную инструкцию
	 pc = GetPC();
	
	ASSERT((pc&1)==0); // it have to be word alined
//	ASSERT(pc > 0140000 && pc < 0160000); //stop here
    m_instruction = GetWordExec(pc);
    SetPC(GetPC() + 2);
	
	m_regdest=GetDigit(m_instruction,0);
	m_methdest=GetDigit(m_instruction,1);
	m_regsrc=GetDigit(m_instruction,2);
	m_methsrc=GetDigit(m_instruction,3);


    // No fields
    switch (m_instruction)
    {
    case PI_HALT:
        ExecuteHALT (); //HALT timing
        //m_internalTick = PI_TIME_HALT;
		return;
    // case PI_HALT10:
    // case PI_HALT11:
    // case PI_HALT13:
    // case PI_HALT14:
    // case PI_HALT15:
    // case PI_HALT17:
	//	ASSERT(0);
    //    ExecuteHALT (); //HALT timing
        //m_internalTick = PI_TIME_HALT;
    //    return;
    case PI_WAIT:
        ExecuteWAIT ();
        return;

    case PI_RTI:        // RTI timing
		//wsprintf(buffer,_T("RTI: "));
		//DebugLog(buffer);
        ExecuteRTI ();
        //m_internalTick = PI_TIME_RTI;
        return;
    case PI_BPT:
        ExecuteBPT ();  // EMT timing
        //m_internalTick = PI_TIME_EMT;
        return;
    case PI_IOT:
        ExecuteIOT ();  // EMT timing
        //m_internalTick = PI_TIME_EMT;
        return;
    case PI_RESET:      // Reset timing
        ExecuteRESET ();
        //m_internalTick = PI_TIME_RESET;
        return;
    case PI_RTT:        // RTI timing
		//wsprintf(buffer,_T("RTT: "));
		//DebugLog(buffer);

        ExecuteRTT ();
        //m_internalTick = PI_TIME_RTI;
        return;
    case PI_NOP:        
        ExecuteNOP ();  // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_CLC:
        ExecuteCLC ();  // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_CLV:
        ExecuteCLV ();  // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_CLVC:
        ExecuteCLVC (); // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_CLZ:
        ExecuteCLZ ();  // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_CLZC:
        ExecuteCLZC (); // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_CLZV:
        ExecuteCLZV (); // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_CLZVC:
        ExecuteCLZVC ();// Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_CLN:
        ExecuteCLN ();  // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_CLNC:
        ExecuteCLNC (); // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_CLNV:
        ExecuteCLNV (); // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_CLNVC:
        ExecuteCLNVC ();// Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_CLNZ:
        ExecuteCLNZ (); // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_CLNZC:
        ExecuteCLNZC ();// Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_CLNZV:
        ExecuteCLNZV ();// Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_CCC:
        ExecuteCCC ();  // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_NOP260:
        ExecuteNOP ();  // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_SEC:
        ExecuteSEC ();  // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_SEV:
        ExecuteSEV ();  // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_SEVC:
        ExecuteSEVC (); // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_SEZ:
        ExecuteSEZ ();  // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_SEZC:
        ExecuteSEZC (); // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_SEZV:
        ExecuteSEZV (); // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_SEZVC:
        ExecuteSEZVC ();// Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_SEN:
        ExecuteSEN ();  // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_SENC:
        ExecuteSENC (); // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_SENV:
        ExecuteSENV (); // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_SENVC:
        ExecuteSENVC ();// Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_SENZ:
        ExecuteSENZ (); // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_SENZC:
        ExecuteSENZC ();// Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_SENZV:
        ExecuteSENZV ();// Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
    case PI_SCC:
        ExecuteSCC ();  // Base timing
        //m_internalTick = PI_TIME_BASE;
        return;
       // Спецкоманды режима HALT ВМ2
	case 010:
	case 011:
	case PI_GO:
	case 013:
		ExecuteRUN();
		//m_internalTick = PI_TIME_BASE;     
		return;
	case 014:
	case 015:
	case PI_STEP:   
	case 017:
		ExecuteSTEP();
		//m_internalTick = PI_TIME_BASE;   
		return;
    case PI_RSEL: 
		ASSERT(0);
		//m_internalTick = PI_TIME_BASE;
		return;
    case PI_MFUS:
		ExecuteMFUS();
		//m_internalTick = PI_TIME_BASE;
		return;
    case PI_RCPC: 
	case 023:
		ExecuteRCPC();
		//m_internalTick = PI_TIME_BASE;
		return;
    case PI_RCPS: 
	case 025:
	case 026:
	case 027:
		ExecuteRCPS();
		//m_internalTick = PI_TIME_BASE;
		return;
    case PI_MTUS:   
		ExecuteMTUS();
		//m_internalTick = PI_TIME_BASE;
		return;
    case PI_WCPC: 
	case 033:
		ExecuteWCPC();
		//m_internalTick = PI_TIME_BASE;
		return;
    case PI_WCPS:
	case 035:
	case 036:
	case 037:
		ExecuteWCPS();
		//m_internalTick = PI_TIME_BASE;   
		return;
	}

    // Branchs & interrupts
    switch (m_instruction & ~(WORD)0377)
    {
    case PI_BR:
        ExecuteBR ();   // Base Branch timing
        //m_internalTick = PI_TIME_BR_BASE;
        return;
    case PI_BNE:
        ExecuteBNE ();  // Base Branch timing
        //m_internalTick = PI_TIME_BR_BASE;
        return;
    case PI_BEQ:
        ExecuteBEQ ();  // Base Branch timing
        //m_internalTick = PI_TIME_BR_BASE;
        return;
    case PI_BGE:
        ExecuteBGE ();  // Base Branch timing
        //m_internalTick = PI_TIME_BR_BASE;
        return;
    case PI_BLT:
        ExecuteBLT ();  // Base Branch timing
        //m_internalTick = PI_TIME_BR_BASE;
        return;
    case PI_BGT:
        ExecuteBGT ();  // Base Branch timing
        //m_internalTick = PI_TIME_BR_BASE;
        return;
    case PI_BLE:
        ExecuteBLE ();  // Base Branch timing
        //m_internalTick = PI_TIME_BR_BASE;
        return;
    case PI_BPL:
        ExecuteBPL ();  // Base Branch timing
        //m_internalTick = PI_TIME_BR_BASE;
        return;
    case PI_BMI:
        ExecuteBMI ();  // Base Branch timing
        //m_internalTick = PI_TIME_BR_BASE;
        return;
    case PI_BHI:
        ExecuteBHI ();  // Base Branch timing
        //m_internalTick = PI_TIME_BR_BASE;
        return;
    case PI_BLOS:
        ExecuteBLOS (); // Base Branch timing
        //m_internalTick = PI_TIME_BR_BASE;
        return;
    case PI_BVC:
        ExecuteBVC ();  // Base Branch timing
        //m_internalTick = PI_TIME_BR_BASE;
        return;
    case PI_BVS:
        ExecuteBVS ();  // Base Branch timing
        //m_internalTick = PI_TIME_BR_BASE;
        return;
    case PI_BHIS:
        ExecuteBHIS (); // Base Branch timing
        //m_internalTick = PI_TIME_BR_BASE;
        return;
    case PI_BLO:
        ExecuteBLO ();  // Base Branch timing
        //m_internalTick = PI_TIME_BR_BASE;
        return;

    case PI_EMT:
        ExecuteEMT();   // EMT timing
        //m_internalTick = PI_TIME_EMT;
        return;
    case PI_TRAP:
        ExecuteTRAP (); // EMT timing
        //m_internalTick = PI_TIME_EMT;
        return;
    }

    // One field
    if ((m_instruction & ~(WORD)7) == PI_RTS)
    {
		wsprintf(buffer,_T("RTS: "));
		//DebugLog(buffer);

        ExecuteRTS (); // RTS timing
        //m_internalTick = PI_TIME_RTS;
        return;
    }
	switch (m_instruction & ~(WORD)7)
	{
		case PI_FADD: 
		case PI_FSUB:         
		case PI_FMUL:         
		case PI_FDIV:         
				m_traprq=1;
				m_trap=0160010;
				//m_psw|=0400;
				return;
		break;
	}


    int meth = GetDigit (m_instruction, 1);

    // Two fields
    switch (m_instruction & ~(WORD)077)
    {
    case PI_JMP:    // Nj timing
        ExecuteJMP ();
        //m_internalTick = timing_A2_Nj[meth];
        return;
    case PI_SWAB:   // AB timing
        ExecuteSWAB ();
        //m_internalTick = PI_TIME_BASE + timing_AB[meth];
        return;
    case PI_MARK:   // MARK timing
        ExecuteMARK ();
        //m_internalTick = PI_TIME_MARK;
        return;
    case PI_SXT:    // AB timing
        ExecuteSXT ();
        //m_internalTick = PI_TIME_BASE + timing_AB[meth];
        return;
    case PI_MTPS:   // AB timing
        ExecuteMTPS ();
        //m_internalTick = PI_TIME_BASE + timing_AB[meth];
        return;
    case PI_MFPS:   // AB timing
        ExecuteMFPS ();
        //m_internalTick = PI_TIME_BASE + timing_AB[meth];
        return;
    }
    
    switch (m_instruction & ~(WORD)0100077)
    {
    case PI_CLR:    // AB timing
        ExecuteCLR ();
        //m_internalTick = PI_TIME_BASE + timing_AB[meth];
        return;
    case PI_COM:    // AB timing
        ExecuteCOM ();
        //m_internalTick = PI_TIME_BASE + timing_AB[meth];
        return;
    case PI_INC:    // AB timing
        ExecuteINC ();
        //m_internalTick = PI_TIME_BASE + timing_AB[meth];
        return;
    case PI_DEC:    // AB timing
        ExecuteDEC ();
        //m_internalTick = PI_TIME_BASE + timing_AB[meth];
        return;
    case PI_NEG:    // AB timing
        ExecuteNEG ();
        //m_internalTick = PI_TIME_BASE + timing_AB[meth];
        return;
    case PI_ADC:    // AB timing
        ExecuteADC ();
        //m_internalTick = PI_TIME_BASE + timing_AB[meth];
        return;
    case PI_SBC:    // AB timing
        ExecuteSBC ();
        //m_internalTick = PI_TIME_BASE + timing_AB[meth];
        return;
    case PI_TST:    // A1 timing
        ExecuteTST ();
        //m_internalTick = PI_TIME_BASE + timing_A_A1[meth];
        return;
    case PI_ROR:    // AB timing
        ExecuteROR ();
        //m_internalTick = PI_TIME_BASE + timing_AB[meth];
        return;
    case PI_ROL:    // AB timing
        ExecuteROL ();
        //m_internalTick = PI_TIME_BASE + timing_AB[meth];
        return;
    case PI_ASR:    // AB timing
        ExecuteASR ();
        //m_internalTick = PI_TIME_BASE + timing_AB[meth];
        return;
    case PI_ASL:    // AB timing
        ExecuteASL ();
        //m_internalTick = PI_TIME_BASE + timing_AB[meth];
        return;
    }
    
    // Three fields
    switch(m_instruction & ~(WORD)0777)
    {
    case PI_JSR:    // Ns timing
        ExecuteJSR ();
        //m_internalTick = timing_A2_Nj[meth];
        return;
	case PI_MUL:
		ExecuteMUL ();
		//m_internalTick = PI_TIME_BASE + timing_A2_Nj[meth]; // TODO: исправить время!!!
		return;
	case PI_DIV:
		ExecuteDIV ();
		//m_internalTick = PI_TIME_BASE + timing_A2_Nj[meth]; // TODO: исправить время!!!
		return;
	case PI_ASH:
		ExecuteASH ();
		//m_internalTick = PI_TIME_BASE + timing_A2_Nj[meth]; // TODO: исправить время!!!
		return;
	case PI_ASHC:
		ExecuteASHC ();
		//m_internalTick = PI_TIME_BASE + timing_A2_Nj[meth]; // TODO: исправить время!!!
		return;
    case PI_XOR:    // A2 timing
        ExecuteXOR ();
        //m_internalTick = PI_TIME_BASE + timing_A2_Nj[meth];
        return;
    case PI_SOB:    // SOB timing
        ExecuteSOB ();
        //m_internalTick = PI_TIME_SOB;
        return;
    }

    int methA = GetDigit (m_instruction, 3);
    int methB = GetDigit (m_instruction, 1);

    // Four fields
    switch(m_instruction & ~(WORD)0107777)
    {
    case PI_MOV:    // A + B timing
        ExecuteMOV ();
        //m_internalTick = PI_TIME_BASE + timing_A_A1[methA] + timing_B[methB];
        return;
    case PI_CMP:    // A1 + A2 timing
        ExecuteCMP ();
        //m_internalTick = PI_TIME_BASE + timing_A_A1[methA] + timing_A2_Nj[methB];
        return;
    case PI_BIT:    // A1 + A2 timing
        ExecuteBIT ();
        //m_internalTick = PI_TIME_BASE + timing_A_A1[methA] + timing_A2_Nj[methB];
        return;
    case PI_BIC:    // A + B timing
        ExecuteBIC ();
        //m_internalTick = PI_TIME_BASE + timing_A_A1[methA] + timing_B[methB];
        return;
    case PI_BIS:    // A + B timing
        ExecuteBIS ();
        //m_internalTick = PI_TIME_BASE + timing_A_A1[methA] + timing_B[methB];
        return;
    }

    switch (m_instruction & ~(WORD)0007777)
    {
    case PI_ADD:    // A + B timing
        ExecuteADD ();
        //m_internalTick = PI_TIME_BASE + timing_A_A1[methA] + timing_B[methB];
        return;
    case PI_SUB:    // A + B timing
        ExecuteSUB ();
        //m_internalTick = PI_TIME_BASE + timing_A_A1[methA] + timing_B[methB];
        return;
    }

#if !defined(PRODUCT)
	DebugPrint(_T(">>Invalid OPCODE ="));
	PrintOctalValue(buffer,m_instruction);
	DebugPrint(buffer);
	DebugPrint(_T("@ "));
	PrintOctalValue(buffer,GetPC()-2);
	DebugPrint(buffer);
	DebugPrint(_T("\r\n"));
#endif

	m_traprq=1;
	m_trap=010;
}


// Instruction execution /////////////////////////////////////////////

void CProcessor::ExecuteWAIT ()  // WAIT - Wait for an interrupt
{
	m_waitmode=1;
}
void CProcessor::ExecuteSTEP()
{
	m_stepmode=1;
	SetPC(m_savepc);
	SetPSW(m_savepsw);
}

void CProcessor::ExecuteRUN()
{
	SetPC(m_savepc);
	SetPSW(m_savepsw);
}

void CProcessor::ExecuteHALT ()  // HALT - Останов
{
    //TODO
	// m_psw|=0400;
    m_traprq=1;
	m_trap=0160170;
}
void CProcessor::ExecuteRCPC	()
{
	SetReg(0,m_savepc);
}
void CProcessor::ExecuteRCPS	()
{
	SetReg(0,m_savepsw);
}
void CProcessor::ExecuteWCPC	()
{
	m_savepc=GetReg(0);
}
void CProcessor::ExecuteWCPS	()
{
	m_savepsw=GetReg(0);
}

void CProcessor::ExecuteMFUS () //move from user space
{
	//r0 = (r5)+
	m_userspace=1;
	SetReg(0,GetWord(GetReg(5)));
	m_userspace=0;
	SetReg(5,GetReg(5)+2);
}

void CProcessor::ExecuteMTUS () //move to user space
{
	//-(r5)=r0
	SetReg(5,GetReg(5)-2);
	m_userspace=1;
	SetWord(GetReg(5),GetReg(0));
	m_userspace=0;
}

void CProcessor::ExecuteRTI ()  // RTI - Возврат из прерывания
{
	WORD new_psw;
    SetReg(7, GetWord( GetSP() ) );  // Pop PC
    SetSP( GetSP() + 2 );
    
	m_psw &= 0400;  // Store HALT
    new_psw = GetWord ( GetSP() );  // Pop PSW --- saving HALT
	if(GetPC() < 0160000)
		SetPSW((new_psw & 0377)|m_psw);  // Preserve HALT mode
	else
		SetPSW(new_psw&0777); //load new mode

    SetSP( GetSP() + 2 );
}

void CProcessor::ExecuteBPT ()  // BPT - Breakpoint
{
	m_traprq=1;
	m_trap=014;
}

void CProcessor::ExecuteIOT ()  // IOT - I/O trap
{
	m_traprq=1;
	m_trap=020;
}

void CProcessor::ExecuteRESET ()  // Reset input/output devices
{
}

void CProcessor::ExecuteRTT ()  // RTT - return from trace trap
{
	WORD new_psw;
    SetReg(7, GetWord( GetSP() ) );  // Pop PC
    SetSP( GetSP() + 2 );
    
	m_psw &= 0400;  // Store HALT
    new_psw = GetWord ( GetSP() );  // Pop PSW --- saving HALT
	if(GetPC() < 0160000)
		SetPSW((new_psw & 0377)|m_psw);  // Preserve HALT mode
	else
		SetPSW(new_psw&0777); //load new mode

    SetSP( GetSP() + 2 );

	m_psw|=PSW_T; // set the trap flag ???
}

void CProcessor::ExecuteRTS ()  // RTS - return from subroutine - Возврат из процедуры
{
	SetPC(GetReg(m_regdest));
	SetReg(m_regdest,GetWord(GetSP()));
	SetSP(GetSP()+2);
}

void CProcessor::ExecuteNOP ()  // NOP - Нет операции
{
}

void CProcessor::ExecuteCLC ()  // CLC - Очистка C
{
    SetC(FALSE);
}
void CProcessor::ExecuteCLV ()
{
    SetV(FALSE);
}
void CProcessor::ExecuteCLVC ()
{
    SetV(FALSE);
    SetC(FALSE);
}
void CProcessor::ExecuteCLZ ()
{
    SetZ(FALSE);
}
void CProcessor::ExecuteCLZC ()
{
    SetZ(FALSE);
    SetC(FALSE);
}
void CProcessor::ExecuteCLZV ()
{
    SetZ(FALSE);
    SetV(FALSE);
}
void CProcessor::ExecuteCLZVC ()
{
    SetZ(FALSE);
    SetV(FALSE);
    SetC(FALSE);
}
void CProcessor::ExecuteCLN ()
{
    SetN(FALSE);
}
void CProcessor::ExecuteCLNC ()
{
    SetN(FALSE);
    SetC(FALSE);
}
void CProcessor::ExecuteCLNV ()
{
    SetN(FALSE);
    SetV(FALSE);
}
void CProcessor::ExecuteCLNVC ()
{
    SetN(FALSE);
    SetV(FALSE);
    SetZ(FALSE);
}
void CProcessor::ExecuteCLNZ ()
{
    SetN(FALSE);
    SetZ(FALSE);
}
void CProcessor::ExecuteCLNZC ()
{
    SetN(FALSE);
    SetZ(FALSE);
    SetC(FALSE);
}
void CProcessor::ExecuteCLNZV ()
{
    SetN(FALSE);
    SetZ(FALSE);
    SetV(FALSE);
}
void CProcessor::ExecuteCCC ()
{
    SetC(FALSE);
    SetV(FALSE);
    SetZ(FALSE);
    SetN(FALSE);
}
void CProcessor::ExecuteSEC ()
{
    SetC(TRUE);
}
void CProcessor::ExecuteSEV ()
{
    SetV(TRUE);
}
void CProcessor::ExecuteSEVC ()
{
    SetV(TRUE);
    SetC(TRUE);
}
void CProcessor::ExecuteSEZ ()
{
    SetZ(TRUE);
}
void CProcessor::ExecuteSEZC ()
{
    SetZ(TRUE);
    SetC(TRUE);
}
void CProcessor::ExecuteSEZV ()
{
    SetZ(TRUE);
    SetV(TRUE);
}
void CProcessor::ExecuteSEZVC ()
{
    SetZ(TRUE);
    SetV(TRUE);
    SetC(TRUE);
}
void CProcessor::ExecuteSEN ()
{
    SetN(TRUE);
}
void CProcessor::ExecuteSENC ()
{
    SetN(TRUE);
    SetZ(TRUE);
}
void CProcessor::ExecuteSENV ()
{
    SetN(TRUE);
    SetV(TRUE);
}
void CProcessor::ExecuteSENVC ()
{
    SetN(TRUE);
    SetV(TRUE);
    SetC(TRUE);
}
void CProcessor::ExecuteSENZ ()
{
    SetN(TRUE);
    SetZ(TRUE);
}
void CProcessor::ExecuteSENZC ()
{
    SetN(TRUE);
    SetZ(TRUE);
    SetC(TRUE);
}
void CProcessor::ExecuteSENZV ()
{
    SetN(TRUE);
    SetZ(TRUE);
    SetV(TRUE);
}
void CProcessor::ExecuteSCC ()
{
    SetC(TRUE);
    SetV(TRUE);
    SetZ(TRUE);
    SetN(TRUE);
}

void CProcessor::ExecuteJMP ()  // JMP - jump: PC = &d (a-mode > 0)
{
	//ASSERT(m_R[7]!=0174222);
	
    if (m_methdest == 0) {  // Неправильный метод адресации
        m_traprq=1;
		m_trap=010;
    }
    else 
	{
      
        SetPC(GetWordAddr(m_methdest,m_regdest));
    }
}

void CProcessor::ExecuteSWAB ()
{
	WORD ea;
	WORD dst;

	dst=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);
	dst=((dst>>8)&0377) | (dst<<8);

	if(m_methdest)
		SetWord(ea,dst);
	else
		SetReg(m_regdest,dst);
	
	SetN((dst&0200)!=0);
	SetZ(!(dst&0377));
	SetV(0);
	SetC(0);
}

void CProcessor::ExecuteCLR ()  // CLR
{
	
	if(m_instruction&0100000)
	{

		SetN(0);
		SetZ(1);
		SetV(0);
		SetC(0);

		if(m_methdest)
			SetByte(GetByteAddr(m_methdest,m_regdest),0);
		else
			SetReg(m_regdest,(GetReg(m_regdest)&0177400));
	}
	else
	{
		
		SetN(0);
		SetZ(1);
		SetV(0);
		SetC(0);

		if(m_methdest)
			SetWord(GetWordAddr(m_methdest,m_regdest),0);
		else
			SetReg(m_regdest,0);
	}
}

void CProcessor::ExecuteCOM ()  // COM
{
	WORD ea;
	if(m_instruction&0100000)
	{
		BYTE dst;

		dst= m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);

		dst = dst ^ 0377;

		SetN(dst>>7);
		SetZ(!dst);
		SetV(0);
		SetC(1);

		if(m_methdest)
			SetByte(ea,dst);
		else
			SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);
		
	}
	else
	{
		WORD dst;

		dst= m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);
		dst = dst ^ 0177777;
		SetN(dst>>15);
		SetZ(!dst);
		SetV(0);
		SetC(1);
		if(m_methdest)
			SetWord(ea,dst);
		else
			SetReg(m_regdest,dst);
	}
}

void CProcessor::ExecuteINC ()  // INC - Инкремент
{
	WORD ea;
	if(m_instruction&0100000)
	{
		BYTE dst;

		dst=m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);

		dst =dst + 1;
		
		SetN(dst>>7);
		SetZ(!dst);
		SetV(dst == 0200);

		if(m_methdest)
			SetByte(ea,dst);
		else
			SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);
	
	}
	else
	{
		WORD dst;

		dst= m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

		dst = dst + 1;
		SetN(dst>>15);
		SetZ(!dst);
		SetV(dst == 0100000);

		if(m_methdest)
			SetWord(ea,dst);
		else
			SetReg(m_regdest,dst);
	}
}

void CProcessor::ExecuteDEC ()  // DEC - Декремент
{
	WORD ea;
	if(m_instruction&0100000)
	{
		BYTE dst;

		dst=m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);

		dst =dst - 1;
		
		SetN(dst>>7);
		SetZ(!dst);
		SetV(dst == 0177);

		if(m_methdest)
			SetByte(ea,dst);
		else
			SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);
	
	}
	else
	{
		WORD dst;

		dst= m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

		dst = dst - 1;
		SetN(dst>>15);
		SetZ(!dst);
		SetV(dst == 077777);
		if(m_methdest)
			SetWord(ea,dst);
		else
			SetReg(m_regdest,dst);
	}
}

void CProcessor::ExecuteNEG ()
{
	WORD ea;

	if(m_instruction&0100000)
	{
		BYTE dst;
		
		dst=m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);

		dst= 0-dst;

		SetN(dst>>7);
		SetZ(!dst);
		SetV(dst==0200);
		SetC(!GetZ());

		if(m_methdest)
			SetByte(ea,dst);
		else
			SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);

	}
	else
	{
		WORD dst;
		
		dst=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

		dst= 0-dst;

		SetN(dst>>15);
		SetZ(!dst);
		SetV(dst==0100000);
		SetC(!GetZ());

		if(m_methdest)
			SetWord(ea,dst);
		else
			SetReg(m_regdest,dst);
	}
}

void CProcessor::ExecuteADC ()  // ADC{B}
{
	WORD ea;
	if(m_instruction&0100000)
	{
		BYTE dst;

		dst=m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);
		dst=dst+(GetC()?1:0);

		if(m_methdest)
			SetByte(ea,dst);
		else
			SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);


		SetN(dst>>7);
		SetZ(!dst);
		SetV(GetC() && (dst==0200));
		SetC(GetC() && GetZ());
	}
	else
	{
		WORD dst;

		dst=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);
		dst=dst+(GetC()?1:0);

		if(m_methdest)
			SetWord(ea,dst);
		else
			SetReg(m_regdest,dst);


		SetN(dst>>15);
		SetZ(!dst);
		SetV(GetC() && (dst==0100000));
		SetC(GetC() && GetZ());
	}
}

void CProcessor::ExecuteSBC ()  // SBC{B}
{
	WORD ea;

	if(m_instruction&0100000)
	{
		BYTE dst;
		dst=m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);

		dst=dst-(GetC()?1:0);

		SetN(dst>>7);
		SetZ(!dst);
		SetV(GetC() && (dst==0177));
		SetC(GetC() && (dst==0377));
	
		if(m_methdest)
			SetByte(ea,dst);
		else
			SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);
	}
	else
	{
		WORD dst;
		dst=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

		dst=dst-(GetC()?1:0);

		SetN(dst>>15);
		SetZ(!dst);
		SetV(GetC() && (dst==077777));
		SetC(GetC() && (dst==0177777));
	
		if(m_methdest)
			SetWord(ea,dst);
		else
			SetReg(m_regdest,dst);
	
	}
}

void CProcessor::ExecuteTST ()  // TST{B} - test
{
	if(m_instruction&0100000)
	{
		BYTE dst;

		dst=m_methdest?GetByte(GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);
		SetN(dst>>7);
		SetZ(!dst);
		SetV(0);
		SetC(0);
	}
	else
	{
		WORD dst;

		dst=m_methdest?GetWord(GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);
		SetN(dst>>15);
		SetZ(!dst);
		SetV(0);
		SetC(0);	
	}
}

void CProcessor::ExecuteROR ()  // ROR{B}
{
	WORD ea;

	if(m_instruction&0100000)
	{
		BYTE src;
		BYTE dst;

		src=m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);

		dst=(src>>1)|(GetC()?0200:0);
		
		SetN(dst>>7);
		SetZ(!dst);
		SetC(src&1);
		SetV(GetN()!=GetC());
		
		if(m_methdest)
			SetByte(ea,dst);
		else
			SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);
	}
	else
	{
		WORD src;
		WORD dst;

		src=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

		dst=(src>>1)|(GetC()?0100000:0);
		
		SetN(dst>>15);
		SetZ(!dst);
		SetC(src&1);
		SetV(GetN()!=GetC());
		
		if(m_methdest)
			SetWord(ea,dst);
		else
			SetReg(m_regdest,dst);
	}
}

void CProcessor::ExecuteROL ()  // ROL{B}
{
	WORD ea;

	if(m_instruction&0100000)
	{
		BYTE src;
		BYTE dst;

		src=m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);

		dst=(src<<1)|(GetC()?1:0);
		
		SetN(dst>>7);
		SetZ(!dst);
		SetC(src>>7);
		SetV(GetN()!=GetC());
		
		if(m_methdest)
			SetByte(ea,dst);
		else
			SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);
	}
	else
	{
		WORD src;
		WORD dst;

		src=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

		dst=(src<<1)|(GetC()?1:0);
		
		SetN(dst>>15);
		SetZ(!dst);
		SetC(src>>15);
		SetV(GetN()!=GetC());
		
		if(m_methdest)
			SetWord(ea,dst);
		else
			SetReg(m_regdest,dst);
	}
}

void CProcessor::ExecuteASR ()  // ASR{B}
{
	WORD ea;
	if(m_instruction&0100000)
	{
		BYTE src;
		BYTE dst;

		src =m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);
		dst = (src >> 1) | (src & 0200);
		SetN(dst>>7);
		SetZ(!dst);
		SetC(src & 1);
		SetV(GetN() != GetC());
		
		if(m_methdest)
			SetByte(ea,dst);
		else
			SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);

	}
	else
	{
		WORD src;
		WORD dst;

		src =m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);
		dst = (src >> 1) | (src & 0100000);
		SetN(dst>>15);
		SetZ(!dst);
		SetC(src & 1);
		SetV(GetN() != GetC());
		
		if(m_methdest)
			SetWord(ea,dst);
		else
			SetReg(m_regdest,dst);
	
	}
}

void CProcessor::ExecuteASL ()  // ASL{B}
{
	WORD ea;
	if(m_instruction&0100000)
	{
		BYTE src;
		BYTE dst;

		src = m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);
		dst = (src << 1) & 0377;
		SetN(dst>>7);
		SetZ(!dst);
		SetC(src>>7);
		SetV(GetN()!=GetC());
		
		if(m_methdest)
			SetByte(ea,dst);
		else
			SetReg(m_regdest,(GetReg(m_regdest)& 0177400)|dst);
	}
	else
	{
		WORD src;
		WORD dst;
		src = m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);
		dst = src << 1;
		SetN(dst>>15);
		SetZ(!dst);
		SetC(src>>15);
		SetV(GetN()!=GetC());
		
		if(m_methdest)
			SetWord(ea,dst);
		else
			SetReg(m_regdest,dst);	
	}
}

void CProcessor::ExecuteSXT ()  // SXT - sign-extend
{
	if(m_methdest)
		SetWord(GetWordAddr(m_methdest,m_regdest),GetN()?0177777:0);
	else
		SetReg(m_regdest,GetN()?0177777:0); //sign extend	

	SetZ(!GetN());
	SetV(0);
}

void CProcessor::ExecuteMTPS ()  // MTPS - move to PS
{
		BYTE dst;

		dst=m_methdest?GetByte(GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);
		
		if(GetPSW()&0400)//in halt?
		{ //allow everything
			SetPSW(GetPSW()&0400|dst);
		}
		else
		{
			SetPSW((GetPSW()&0420)|(dst&0357));//preserve T			
		}
}

void CProcessor::ExecuteMFPS ()  // MFPS - move from PS
{
	if(m_methdest)
		SetByte(GetByteAddr(m_methdest,m_regdest), (BYTE) GetPSW());
	else
		SetReg(m_regdest,(char)(GetPSW()&0377)); //sign extend

}

void CProcessor::ExecuteBR ()
{
    SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
}

void CProcessor::ExecuteBNE ()
{
    if (! GetZ())
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
}

void CProcessor::ExecuteBEQ ()
{
    if (GetZ())
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
}

void CProcessor::ExecuteBGE ()
{
    if (GetN() == GetV())
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
}

void CProcessor::ExecuteBLT ()
{
    if (GetN() != GetV())
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
}

void CProcessor::ExecuteBGT ()
{
    if (! ((GetN() != GetV()) || GetZ()))
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
}

void CProcessor::ExecuteBLE ()
{
    if ((GetN() != GetV()) || GetZ())
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
}

void CProcessor::ExecuteBPL ()
{
    if (! GetN())
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
}

void CProcessor::ExecuteBMI ()
{
    if (GetN())
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
}

void CProcessor::ExecuteBHI ()
{
    if (! (GetZ() || GetC()))
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
}

void CProcessor::ExecuteBLOS ()
{
    if (GetZ() || GetC())
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
}

void CProcessor::ExecuteBVC ()
{
    if (! GetV())
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
}

void CProcessor::ExecuteBVS ()
{
    if (GetV())
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
}

void CProcessor::ExecuteBHIS ()
{
    if (! GetC())
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
}

void CProcessor::ExecuteBLO ()
{
    if (GetC())
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
}

void CProcessor::ExecuteXOR ()  // XOR
{
	WORD dst;
	WORD ea;

	dst=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);
	dst=dst^GetReg(m_regsrc);
	
	SetN(dst>>15);
	SetZ(!dst);
	SetV(0);

	if(m_methdest)
		SetWord(ea,dst);
	else
		SetReg(m_regdest,dst);

}

void CProcessor::ExecuteMUL ()  // MUL
{
    WORD dst = GetReg(m_regsrc);
	WORD src = m_methdest?GetWord(GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);
	int res;
	
	
	if(dst>>15)
		dst|=~077777; 
	if(src>>15)
		src|=~077777;	
	
	res=(signed short)dst*(signed short)src;
    
    SetReg(m_regsrc,(res>>16));
	SetReg(m_regsrc|1,res&0177777);

    SetN( res>>31 );
    SetZ( !res );
    SetV( FALSE );
	SetC( (res > 32767) || (res < -32768) );
}
void CProcessor::ExecuteDIV ()  // DIV
{
	//время надо считать тут

    int longsrc;
	int res;

	int src2=m_methdest?(short)GetWord(GetWordAddr(m_methdest,m_regdest)):(short)GetReg(m_regdest);

	longsrc=(GetReg(m_regsrc)<<16)|GetReg(m_regsrc|1);
	
    if(src2==0)
	{
		SetV(TRUE);
		SetC(TRUE); //если делят на 0 -- то устанавливаем V и C
		return; 
	}	
	if ((longsrc == 020000000000) && (src2 == 0177777))
	{
		SetV(TRUE);
		SetC(FALSE); // переполняемся, товарищи
		return;
	}
    
	if(src2>>15)
		src2|=~077777;
	if(GetReg(m_regsrc)>>15)
		longsrc|=~017777777777;

	res=(signed)longsrc/(signed)src2;

	if ((res >= 32767) || (res < -32768)) 
	{
		SetV(TRUE);
		SetC(FALSE); // переполняемся, товарищи
		return;
	}


    SetReg(m_regsrc,res&0177777);
	SetReg(m_regsrc|1,(longsrc-(src2*res))&0177777);

    SetN( res&0100000 );
    SetZ( res==0 );
    SetV( FALSE );
	SetC( FALSE );
}
void CProcessor::ExecuteASH ()  // ASH
{
		WORD src2;
		WORD src;
		WORD dst;
		int sign;
		int i;

		src2 = m_methdest?GetWord(GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);
		src2 = src2 & 077;
		sign = GetReg(m_regsrc)>>15;
		src = sign? GetReg(m_regsrc) | ~077777: GetReg(m_regsrc);
		if (src2 == 0) 
		{			/* [0] */
			dst = src;
			SetV(0);
			SetC(0);  
		}
		else if (src2 <= 15) 
		{			/* [1,15] */
			dst = src << src2;
			i = (src >> (16 - src2)) & 0177777;
			SetV(i != ((dst & 0100000)? 0177777: 0));
			SetC(i & 1);  
		}
		else if (src2 <= 31) 
		{			/* [16,31] */
			dst = 0;
			SetV(src != 0);
			SetC((src << (src2 - 16)) & 1);  
		}
		else 
		{					/* [-32,-1] */
			dst = (src >> (64 - src2)) | (-sign << (src2 - 32));
			SetV(0);
			SetC((src >> (63 - src2)) & 1);  
		}
		
		SetReg(m_regsrc,dst);
		//dst&=0177777;
		
		SetN(dst>>15);
		SetZ(!dst);

}
void CProcessor::ExecuteASHC ()  // ASHC
{
		WORD src2;
		int src;
		int dst;
		int sign;
		int i;

		src2 = m_methdest?GetWord(GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);
		src2 = src2 & 077;
		sign = GetReg(m_regsrc)>>15;//GET_SIGN_W (R[srcspec]);
		src = (GetReg(m_regsrc) << 16) | GetReg(m_regsrc|1);
		if (src2 == 0) 
		{ 			/* [0] */
			dst = src;
			SetV(0); 
			SetC(0);  
		}
		else if (src2 <= 31) 
		{			/* [1,31] */
			dst = src << src2;
			i = (src >> (32 - src2)) | (-sign << src2);
			SetV(i != ((dst & 020000000000)? -1: 0));
			SetC(i & 1);  
		}
		else 
		{					/* [-32,-1] */
			dst = (src >> (64 - src2)) | (-sign << (src2 - 32));
			SetV(0);
			SetC((src >> (63 - src2)) & 1);  
		}
		i = (dst >> 16) & 0177777;
		dst = dst & 0177777;
		
		SetReg(m_regsrc,i);
		SetReg(m_regsrc|1,dst);

		
		SetN(i>>15);
		SetZ(!dst & !i);

}


void CProcessor::ExecuteSOB ()  // SOB - subtract one: R = R - 1 ; if R != 0 : PC = PC - 2*nn
{
	
    WORD dst = GetReg(m_regsrc);

    --dst;
    SetReg(m_regsrc, dst);
    
    if (dst)
        SetPC(GetPC() - (m_instruction & (WORD)077) * 2 );

}

void CProcessor::ExecuteMOV ()
{
	if(m_instruction&0100000)
	{
		BYTE dst;

		dst=m_methsrc?GetByte(GetByteAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);

		SetN(dst>>7);
		SetZ(!dst);
		SetV(0);

		if(m_methdest)
			SetByte(GetByteAddr(m_methdest,m_regdest),dst);
		else
			SetReg(m_regdest,(dst&0200)?(0177400|dst):dst);
	}
	else
	{
		WORD dst;

		dst=m_methsrc?GetWord(GetWordAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);

		SetN(dst>>15);
		SetZ(!dst);
		SetV(0);

		if(m_methdest)
			SetWord(GetWordAddr(m_methdest,m_regdest),dst);
		else
			SetReg(m_regdest,dst);
	}
}

void CProcessor::ExecuteCMP ()
{
	if(m_instruction&0100000)
	{
		BYTE src;
		BYTE src2;
		BYTE dst;

		src = m_methsrc?GetByte(GetByteAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);
		src2 = m_methdest?GetByte(GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);
		
		dst = src - src2;
        SetN( CheckForNegative((BYTE)(src - src2)) );
        SetZ( CheckForZero((BYTE)(src - src2)) );
        SetV( CheckSubForOverflow (src, src2) );
        SetC( CheckSubForCarry (src, src2) );
	}
	else
	{
		WORD src;
		WORD src2;
		WORD dst;

		src = m_methsrc?GetWord(GetWordAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);
		src2 = m_methdest?GetWord(GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);
		
		dst = src - src2;
		
        SetN( CheckForNegative ((WORD)(src - src2)) );
        SetZ( CheckForZero ((WORD)(src - src2)) );
        SetV( CheckSubForOverflow (src, src2) );
        SetC( CheckSubForCarry (src, src2) );
	
	}
}

void CProcessor::ExecuteBIT ()  // BIT{B} - bit test
{
	WORD ea;
	if(m_instruction&0100000)
	{
		BYTE src;
		BYTE src2;
		BYTE dst;
		
		src=m_methsrc?GetByte(GetByteAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);
		src2=m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);

		dst=src2 & src;

		SetN(dst>>7);
		SetZ(!dst);
		SetV(0);


	}
	else
	{
		WORD src;
		WORD src2;
		WORD dst;
		
		src=m_methsrc?GetWord(GetWordAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);
		src2=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

		dst=src2 & src;

		SetN(dst>>15);
		SetZ(!dst);
		SetV(0);

	}

}

void CProcessor::ExecuteBIC ()  // BIC{B} - bit clear
{
	WORD ea;
	if(m_instruction&0100000)
	{
		BYTE src;
		BYTE src2;
		BYTE dst;
		
		src=m_methsrc?GetByte(GetByteAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);
		src2=m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);

		dst=src2 & (~src);

		SetN(dst>>7);
		SetZ(!dst);
		SetV(0);

		if(m_methdest)
			SetByte(ea,dst);
		else
			SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);

	}
	else
	{
		WORD src;
		WORD src2;
		WORD dst;
		
		src=m_methsrc?GetWord(GetWordAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);
		src2=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

		dst=src2 & (~src);

		SetN(dst>>15);
		SetZ(!dst);
		SetV(0);

		if(m_methdest)
			SetWord(ea,dst);
		else
			SetReg(m_regdest,dst);
	}
}

void CProcessor::ExecuteBIS ()  // BIS{B} - bit set
{
	WORD ea;
	if(m_instruction&0100000)
	{
		BYTE src;
		BYTE src2;
		BYTE dst;
		
		src=m_methsrc?GetByte(GetByteAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);
		src2=m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);

		dst=src2 | src;

		SetN(dst>>7);
		SetZ(!dst);
		SetV(0);

		if(m_methdest)
			SetByte(ea,dst);
		else
			SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);

	}
	else
	{
		WORD src;
		WORD src2;
		WORD dst;
		
		src=m_methsrc?GetWord(GetWordAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);
		src2=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

		dst=src2 | src;

		SetN(dst>>15);
		SetZ(!dst);
		SetV(0);

		if(m_methdest)
			SetWord(ea,dst);
		else
			SetReg(m_regdest,dst);
	}

}

void CProcessor::ExecuteADD ()  // ADD
{
	WORD src;
	WORD src2;
	signed short dst;
	signed long dst2;
	WORD ea;
	
	src=m_methsrc?GetWord(GetWordAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);
	src2=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

    SetN(CheckForNegative ((WORD)(src2 + src)));
    SetZ(CheckForZero ((WORD)(src2 + src)));
    SetV(CheckAddForOverflow (src2, src));
    SetC(CheckAddForCarry (src2, src));


	dst=src2+src;
	dst2=(short)src2+(short)src;


	if(m_methdest)
		SetWord(ea,dst);
	else
		SetReg(m_regdest,dst);

}

void CProcessor::ExecuteSUB ()
{
	WORD src;
	WORD src2;
	WORD dst;
	WORD ea;
	
	src = m_methsrc?GetWord(GetWordAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);
	src2 = m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);


	SetN(CheckForNegative ((WORD)(src2 - src)));
    SetZ(CheckForZero ((WORD)(src2 - src)));
    SetV(CheckSubForOverflow (src2, src));
    SetC(CheckSubForCarry (src2, src));


	dst = src2 - src;
	

	if(m_methdest)
		SetWord(ea,dst);
	else
		SetReg(m_regdest,dst);
}

void CProcessor::ExecuteEMT ()  // EMT - emulator trap
{
	m_traprq=1;
	m_trap=030;
}

void CProcessor::ExecuteTRAP ()
{
	m_traprq=1;
	m_trap=034;
}

void CProcessor::ExecuteJSR ()  // JSR - Jump subroutine: *--SP = R; R = PC; PC = &d (a-mode > 0)
{
	//int meth = GetDigit(m_instruction, DST + 1);
    if (m_methdest == 0) 
	{  // Неправильный метод адресации
        //QueueInterrupt(INTERRUPT_10, 2);
		m_traprq=1;
		m_trap=010;
    }
    else 
	{
        WORD dst;
		//WORD pc = GetDstWordArgAsBranch();
		dst= GetWordAddr(m_methdest,m_regdest);
	    
	    SetSP( GetSP() - 2 );
		
		SetWord( GetSP(), GetReg(m_regsrc) );

        SetReg(m_regsrc, GetPC());

        SetPC(dst);
    }
}

void CProcessor::ExecuteMARK ()  // MARK
{
    SetReg(7, GetReg(5) );
    SetReg(5, GetWord( GetSP() ));
    SetSP( GetSP() + (m_instruction & 0x003F) * 2 + 2 );
}

//////////////////////////////////////////////////////////////////////
//
// CPU image format (32 bytes):
//   2 bytes        PSW
//   2*8 bytes      Registers R0..R7
//   2*2 bytes      Saved PC and PSW
//   2 bytes        Stopped flag: !0 - stopped, 0 - not stopped

void CProcessor::SaveToImage(BYTE* pImage)
{
    WORD* pwImage = (WORD*) pImage;
    // PSW
    *pwImage++ = m_psw;
    // Registers R0..R7
    CopyMemory(pwImage, m_R, 2 * 8);
    pwImage += 2 * 8;
    // Saved PC and PSW
	*pwImage++ = m_savepc;
	*pwImage++ = m_savepsw;
    // Stopped flag
    *pwImage++ = (m_okStopped ? 1 : 0);
}

void CProcessor::LoadFromImage(const BYTE* pImage)
{
    WORD* pwImage = (WORD*) pImage;
    // PSW
    m_psw = *pwImage++;
    // Registers R0..R7
    CopyMemory(m_R, pwImage, 2 * 8);
    // Saved PC and PSW
	m_savepc= *pwImage++;
	m_savepsw= *pwImage++;
    // Stopped flag
    m_okStopped = (*pwImage++ != 0);
}

WORD	CProcessor::GetWordAddr (BYTE meth, BYTE reg)
{
	WORD addr;

	addr=0;

	switch(meth)
	{
		case 1:   //(R)
			addr=GetReg(reg);
			break;
		case 2:   //(R)+
			addr=GetReg(reg);
			SetReg(reg,addr+2);
			break;
		case 3:  //@(R)+
			addr=GetReg(reg);
			SetReg(reg,addr+2);
			addr=GetWord(addr);
		break;
		case 4: //-(R)
			SetReg(reg,GetReg(reg)-2);
			addr=GetReg(reg);
			break;
		case 5: //@-(R)
			SetReg(reg,GetReg(reg)-2);
			addr=GetReg(reg);
			addr=GetWord(addr);
			break;
		case 6: //d(R)
			addr=GetWord(GetPC());
			SetPC(GetPC()+2);
			addr=GetReg(reg)+addr;
			break;
		case 7: //@d(r)
			addr=GetWord(GetPC());
			SetPC(GetPC()+2);
			addr=GetReg(reg)+addr;
			addr=GetWord(addr);
			break;
	}
	return addr;
}

WORD	CProcessor::GetByteAddr (BYTE meth, BYTE reg)
{
	WORD addr,delta;

	addr=0;
	switch(meth)
	{
		case 1:
			addr=GetReg(reg);
		break;
		case 2:
			delta=1+(reg>=6);
			addr=GetReg(reg);
			SetReg(reg,addr+delta);
			break;
		case 3:
			addr=GetReg(reg);
			SetReg(reg,addr+2);
			addr=GetWord(addr);
			break;
		case 4:
			delta=1+(reg>=6);
			SetReg(reg,GetReg(reg)-delta);
			addr=GetReg(reg);
			break;
		case 5:
			SetReg(reg,GetReg(reg)-2);
			addr=GetReg(reg);
			addr=GetWord(addr);
			break;
		case 6: //d(R)
			addr=GetWord(GetPC());
			SetPC(GetPC()+2);
			addr=GetReg(reg)+addr;
			break;
		case 7: //@d(r)
			addr=GetWord(GetPC());
			SetPC(GetPC()+2);
			addr=GetReg(reg)+addr;
			addr=GetWord(addr);
			break;
	}

	return addr;
}



//////////////////////////////////////////////////////////////////////

