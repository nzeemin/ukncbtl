/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Processor.cpp
// KM1801VM2 processor

#include "stdafx.h"
#include "Processor.h"


// Timings ///////////////////////////////////////////////////////////

//MOV -- 64 
WORD MOV_TIMING[8][8]=
{	
        {0x000B, 0x0021, 0x0027, 0x0033, 0x002B, 0x0037, 0x0033, 0x0043}, 
        {0x0018, 0x0031, 0x0037, 0x0043, 0x003B, 0x0047, 0x0043, 0x0054}, 
        {0x0019, 0x0037, 0x0037, 0x0043, 0x003B, 0x0047, 0x0043, 0x0053}, 
        {0x0025, 0x0043, 0x0043, 0x004F, 0x0047, 0x0054, 0x004F, 0x0060}, 
        {0x0019, 0x0037, 0x0037, 0x0043, 0x003B, 0x0047, 0x0043, 0x0053}, 
        {0x0025, 0x0043, 0x0043, 0x004F, 0x0047, 0x0054, 0x004F, 0x0060}, 
        {0x0029, 0x0039, 0x003F, 0x004C, 0x003F, 0x004C, 0x004B, 0x005C}, 
        {0x0035, 0x0045, 0x004C, 0x0057, 0x004C, 0x0057, 0x0057, 0x0068}
};

WORD MOVB_TIMING[8][8]=
{
        {0x000B, 0x0025, 0x002B, 0x0037, 0x002F, 0x003B, 0x003B, 0x0047}, 
        {0x0018, 0x0035, 0x003B, 0x0047, 0x003F, 0x004C, 0x004B, 0x0057}, 
        {0x0019, 0x003B, 0x003B, 0x0047, 0x0040, 0x004B, 0x004C, 0x0057}, 
        {0x0025, 0x0047, 0x0047, 0x0054, 0x004B, 0x0057, 0x0057, 0x0063}, 
        {0x0019, 0x003B, 0x003B, 0x0047, 0x0040, 0x004B, 0x004C, 0x0057}, 
        {0x0025, 0x0047, 0x0047, 0x0054, 0x004B, 0x0057, 0x0057, 0x0063}, 
        {0x0029, 0x003D, 0x0043, 0x004F, 0x0043, 0x004F, 0x0054, 0x005F}, 
        {0x0035, 0x0049, 0x004F, 0x005B, 0x004F, 0x005B, 0x005F, 0x006C}
};

WORD CMP_TIMING[8][8]=
{
    
        {0x000B, 0x001C, 0x001D, 0x0029, 0x0021, 0x002D, 0x0035, 0x0041}, 
        {0x0018, 0x002D, 0x002D, 0x0039, 0x0031, 0x003D, 0x0045, 0x0051}, 
        {0x0019, 0x002D, 0x002D, 0x0039, 0x0031, 0x003D, 0x0045, 0x0051}, 
        {0x0025, 0x0039, 0x0039, 0x0045, 0x003D, 0x0049, 0x0051, 0x005E}, 
        {0x0019, 0x002D, 0x002D, 0x0039, 0x0031, 0x003D, 0x0045, 0x0051}, 
        {0x0025, 0x0039, 0x0039, 0x0045, 0x003D, 0x0049, 0x0051, 0x005E}, 
        {0x0029, 0x0035, 0x0035, 0x0041, 0x0035, 0x0041, 0x004D, 0x005A}, 
        {0x0035, 0x0041, 0x0041, 0x004E, 0x0041, 0x004E, 0x005A, 0x0065}
};

WORD CLR_TIMING[8]=
{
        0x000B, 0x001C, 0x0023, 0x002F, 0x0023, 0x002F, 0x002F, 0x003F
};

WORD CRLB_TIMING[8]=
{
        0x000B, 0x0021, 0x0027, 0x0033, 0x0027, 0x0033, 0x0037, 0x0043 
};

WORD TST_TIMING[8]=
{
        0x000B, 0x0018, 0x0019, 0x0025, 0x0019, 0x0025, 0x0031, 0x003D
};

WORD MTPS_TIMING[8]=
{
        0x0018, 0x0029, 0x0029, 0x0035, 0x0029, 0x0035, 0x0041, 0x004D
};

WORD XOR_TIMING[8]=
{
        0x000B, 0x0025, 0x002B, 0x0037, 0x002F, 0x003B, 0x003B, 0x0047
};

WORD ASH_TIMING[8]=
{
        0x0029,	0x003D, 0x003D, 0x0049, 0x0041, 0x004D, 0x0055, 0x0062
};
WORD ASH_S_TIMING=0x0008; 

WORD ASHC_TIMING[8]=
{
        0x0039, 0x004E, 0x004D, 0x005A, 0x0051, 0x005D, 0x0066,	0x0072
};
WORD ASHC_S_TIMING=0x0008;

WORD MUL_TIMING[8]=
{
        0x0060, 0x00C7, 0x00C7, 0x00D4, 0x00CA, 0x00D8, 0x00E1, 0x00EC
};

WORD DIV_TIMING[8]=
{
        0x0080, 0x00E8, 0x00E7, 0x00F4, 0x00EB, 0x00F8,	0x0100,	0x010D 
};

WORD JMP_TIMING[7]=
{
        0x002D, 0x002D, 0x003D, 0x002D, 0x003D, 0x0031,	0x0041
};
WORD JSR_TIMING[7]=
{
        0x0045, 0x0045, 0x0056, 0x0045, 0x0056, 0x0049, 0x0059
};

WORD BRANCH_TRUE_TIMING=0x0025;
WORD BRANCH_FALSE_TIMING=0x0010;
WORD BPT_TIMING=0x0094;
WORD EMT_TIMING=0x009C;
WORD RTI_TIMING=0x0059;
WORD RTS_TIMING=0x0031;
WORD NOP_TIMING=0x0010;
WORD SOB_TIMING=0x002D;
WORD SOB_LAST_TIMING=0x0019; //last iteration of SOB
WORD BR_TIMING=0x0025;
WORD MARK_TIMING=0x0041;
WORD RESET_TIMING = 105 + 968;  // ТО КМ1801ВМ2 стр. 134


//////////////////////////////////////////////////////////////////////


CProcessor::ExecuteMethodRef* CProcessor::m_pExecuteMethodMap = NULL;

void CProcessor::Init()
{
    ASSERT(m_pExecuteMethodMap == NULL);
    m_pExecuteMethodMap = (CProcessor::ExecuteMethodRef*) ::malloc(sizeof(CProcessor::ExecuteMethodRef) * 65536);

    // Сначала заполняем таблицу ссылками на метод ExecuteUNKNOWN, выполняющий TRAP 10
    RegisterMethodRef( 0000000, 0177777, &CProcessor::ExecuteUNKNOWN );

    RegisterMethodRef( 0000000, 0000000, &CProcessor::ExecuteHALT );
    RegisterMethodRef( 0000001, 0000001, &CProcessor::ExecuteWAIT );
    RegisterMethodRef( 0000002, 0000002, &CProcessor::ExecuteRTI );
    RegisterMethodRef( 0000003, 0000003, &CProcessor::ExecuteBPT );
    RegisterMethodRef( 0000004, 0000004, &CProcessor::ExecuteIOT );
    RegisterMethodRef( 0000005, 0000005, &CProcessor::ExecuteRESET );
    RegisterMethodRef( 0000006, 0000006, &CProcessor::ExecuteRTT );

    RegisterMethodRef( 0000010, 0000013, &CProcessor::ExecuteRUN );
    RegisterMethodRef( 0000014, 0000017, &CProcessor::ExecuteSTEP );
    RegisterMethodRef( 0000020, 0000020, &CProcessor::ExecuteRSEL );
    RegisterMethodRef( 0000021, 0000021, &CProcessor::ExecuteMFUS );
    RegisterMethodRef( 0000022, 0000023, &CProcessor::ExecuteRCPC );
    RegisterMethodRef( 0000024, 0000027, &CProcessor::ExecuteRCPS );
    RegisterMethodRef( 0000030, 0000030, &CProcessor::ExecuteRSEL );
    RegisterMethodRef( 0000031, 0000031, &CProcessor::ExecuteMTUS );
    RegisterMethodRef( 0000032, 0000033, &CProcessor::ExecuteWCPC );
    RegisterMethodRef( 0000034, 0000037, &CProcessor::ExecuteWCPS );

    RegisterMethodRef( 0000100, 0000177, &CProcessor::ExecuteJMP );
    RegisterMethodRef( 0000200, 0000207, &CProcessor::ExecuteRTS );  // RTS / RETURN

    RegisterMethodRef( 0000240, 0000257, &CProcessor::ExecuteCCC );

    RegisterMethodRef( 0000260, 0000277, &CProcessor::ExecuteSCC );

    RegisterMethodRef( 0000300, 0000377, &CProcessor::ExecuteSWAB );

    RegisterMethodRef( 0000400, 0000777, &CProcessor::ExecuteBR );
    RegisterMethodRef( 0001000, 0001377, &CProcessor::ExecuteBNE );
    RegisterMethodRef( 0001400, 0001777, &CProcessor::ExecuteBEQ );
    RegisterMethodRef( 0002000, 0002377, &CProcessor::ExecuteBGE );
    RegisterMethodRef( 0002400, 0002777, &CProcessor::ExecuteBLT );
    RegisterMethodRef( 0003000, 0003377, &CProcessor::ExecuteBGT );
    RegisterMethodRef( 0003400, 0003777, &CProcessor::ExecuteBLE );
    
    RegisterMethodRef( 0004000, 0004777, &CProcessor::ExecuteJSR );  // JSR / CALL

    RegisterMethodRef( 0005000, 0005077, &CProcessor::ExecuteCLR );
    RegisterMethodRef( 0005100, 0005177, &CProcessor::ExecuteCOM );
    RegisterMethodRef( 0005200, 0005277, &CProcessor::ExecuteINC );
    RegisterMethodRef( 0005300, 0005377, &CProcessor::ExecuteDEC );
    RegisterMethodRef( 0005400, 0005477, &CProcessor::ExecuteNEG );
    RegisterMethodRef( 0005500, 0005577, &CProcessor::ExecuteADC );
    RegisterMethodRef( 0005600, 0005677, &CProcessor::ExecuteSBC );
    RegisterMethodRef( 0005700, 0005777, &CProcessor::ExecuteTST );
    RegisterMethodRef( 0006000, 0006077, &CProcessor::ExecuteROR );
    RegisterMethodRef( 0006100, 0006177, &CProcessor::ExecuteROL );
    RegisterMethodRef( 0006200, 0006277, &CProcessor::ExecuteASR );
    RegisterMethodRef( 0006300, 0006377, &CProcessor::ExecuteASL );
    
    RegisterMethodRef( 0006400, 0006477, &CProcessor::ExecuteMARK );
    RegisterMethodRef( 0006700, 0006777, &CProcessor::ExecuteSXT );

    RegisterMethodRef( 0010000, 0017777, &CProcessor::ExecuteMOV );
    RegisterMethodRef( 0020000, 0027777, &CProcessor::ExecuteCMP );
    RegisterMethodRef( 0030000, 0037777, &CProcessor::ExecuteBIT );
    RegisterMethodRef( 0040000, 0047777, &CProcessor::ExecuteBIC );
    RegisterMethodRef( 0050000, 0057777, &CProcessor::ExecuteBIS );
    RegisterMethodRef( 0060000, 0067777, &CProcessor::ExecuteADD );
    
    RegisterMethodRef( 0070000, 0070777, &CProcessor::ExecuteMUL );
    RegisterMethodRef( 0071000, 0071777, &CProcessor::ExecuteDIV );
    RegisterMethodRef( 0072000, 0072777, &CProcessor::ExecuteASH );
    RegisterMethodRef( 0073000, 0073777, &CProcessor::ExecuteASHC );
    RegisterMethodRef( 0074000, 0074777, &CProcessor::ExecuteXOR );
    RegisterMethodRef( 0075000, 0075037, &CProcessor::ExecuteFIS );
    RegisterMethodRef( 0077000, 0077777, &CProcessor::ExecuteSOB );

    RegisterMethodRef( 0100000, 0100377, &CProcessor::ExecuteBPL );
    RegisterMethodRef( 0100400, 0100777, &CProcessor::ExecuteBMI );
    RegisterMethodRef( 0101000, 0101377, &CProcessor::ExecuteBHI );
    RegisterMethodRef( 0101400, 0101777, &CProcessor::ExecuteBLOS );
    RegisterMethodRef( 0102000, 0102377, &CProcessor::ExecuteBVC );
    RegisterMethodRef( 0102400, 0102777, &CProcessor::ExecuteBVS );
    RegisterMethodRef( 0103000, 0103377, &CProcessor::ExecuteBHIS );  // BCC
    RegisterMethodRef( 0103400, 0103777, &CProcessor::ExecuteBLO );   // BCS
    
    RegisterMethodRef( 0104000, 0104377, &CProcessor::ExecuteEMT );
    RegisterMethodRef( 0104400, 0104777, &CProcessor::ExecuteTRAP );
    
    RegisterMethodRef( 0105000, 0105077, &CProcessor::ExecuteCLR );  // CLRB
    RegisterMethodRef( 0105100, 0105177, &CProcessor::ExecuteCOM );  // COMB
    RegisterMethodRef( 0105200, 0105277, &CProcessor::ExecuteINC );  // INCB
    RegisterMethodRef( 0105300, 0105377, &CProcessor::ExecuteDEC );  // DECB
    RegisterMethodRef( 0105400, 0105477, &CProcessor::ExecuteNEG );  // NEGB
    RegisterMethodRef( 0105500, 0105577, &CProcessor::ExecuteADC );  // ADCB
    RegisterMethodRef( 0105600, 0105677, &CProcessor::ExecuteSBC );  // SBCB
    RegisterMethodRef( 0105700, 0105777, &CProcessor::ExecuteTST );  // TSTB
    RegisterMethodRef( 0106000, 0106077, &CProcessor::ExecuteROR );  // RORB
    RegisterMethodRef( 0106100, 0106177, &CProcessor::ExecuteROL );  // ROLB
    RegisterMethodRef( 0106200, 0106277, &CProcessor::ExecuteASR );  // ASRB
    RegisterMethodRef( 0106300, 0106377, &CProcessor::ExecuteASL );  // ASLB
    
    RegisterMethodRef( 0106400, 0106477, &CProcessor::ExecuteMTPS );
    RegisterMethodRef( 0106700, 0106777, &CProcessor::ExecuteMFPS );

    RegisterMethodRef( 0110000, 0117777, &CProcessor::ExecuteMOV );  // MOVB
    RegisterMethodRef( 0120000, 0127777, &CProcessor::ExecuteCMP );  // CMPB
    RegisterMethodRef( 0130000, 0137777, &CProcessor::ExecuteBIT );  // BITB
    RegisterMethodRef( 0140000, 0147777, &CProcessor::ExecuteBIC );  // BICB
    RegisterMethodRef( 0150000, 0157777, &CProcessor::ExecuteBIS );  // BISB
    RegisterMethodRef( 0160000, 0167777, &CProcessor::ExecuteSUB );
}

void CProcessor::Done()
{
    ::free(m_pExecuteMethodMap);  m_pExecuteMethodMap = NULL;
}

void CProcessor::RegisterMethodRef(WORD start, WORD end, CProcessor::ExecuteMethodRef methodref)
{
    for (int opcode = start; opcode <= end; opcode++ )
        m_pExecuteMethodMap[opcode] = methodref;
}

//////////////////////////////////////////////////////////////////////


CProcessor::CProcessor (LPCTSTR name)
{
    _tcscpy(m_name, name);
    memset(m_R, 0, sizeof(m_R));
    m_psw = m_savepsw = 0777; 
    m_savepc = 0177777;
    m_okStopped = TRUE;
    m_internalTick = 0;
    m_pMemoryController = NULL;
    m_waitmode = FALSE;
    m_stepmode = FALSE;
    m_buserror = FALSE;
    m_STRTrq = m_RPLYrq = m_RSVDrq = m_TBITrq = m_ACLOrq = m_HALTrq = m_EVNTrq = FALSE;
    m_ILLGrq = m_FIS_rq = m_BPT_rq = m_IOT_rq = m_EMT_rq = m_TRAPrq = FALSE;
    m_ACLOreset = m_EVNTreset = FALSE; m_VIRQreset = 0;
    m_haltpin = FALSE;
}


void CProcessor::Execute()
{
    if (m_okStopped) return;  // Processor is stopped - nothing to do

    if (m_internalTick > 0)
    {
        m_internalTick--;
        return;
    }

    m_internalTick = 0;  //ANYTHING UNKNOWN WILL CAUSE EXCEPTION (EMT)

    if (!CProcessor::InterruptProcessing())
        CProcessor::CommandExecution();
}

BOOL CProcessor::InterruptProcessing ()
{
    WORD intrVector = 0xFFFF;
    BOOL currMode = ((m_psw & 0400) != 0);  // Current processor mode: TRUE = HALT mode, FALSE = USER mode
    BOOL intrMode;  // TRUE = HALT mode interrupt, FALSE = USER mode interrupt

    if (m_stepmode)
        m_stepmode = FALSE;
    else
    {
        m_ACLOreset = m_EVNTreset = FALSE; m_VIRQreset = 0;
        m_TBITrq = (m_psw & 020);  // T-bit

        if (m_STRTrq)
        {
            intrVector = 0; intrMode = TRUE;
            m_STRTrq = FALSE;
        }
        else if (m_HALTrq)  // HALT command
        {
            intrVector = 0170;  intrMode = TRUE;
            m_HALTrq = FALSE;
        }
        else if (m_BPT_rq)  // BPT command
        {
            intrVector = 0000014;  intrMode = FALSE;
            m_BPT_rq = FALSE;
        }
        else if (m_IOT_rq)  // IOT command
        {
            intrVector = 0000020;  intrMode = FALSE;
            m_IOT_rq = FALSE;
        }
        else if (m_EMT_rq)  // EMT command
        {
            intrVector = 0000030;  intrMode = FALSE;
            m_EMT_rq = FALSE;
        }
        else if (m_TRAPrq)  // TRAP command
        {
            intrVector = 0000034;  intrMode = FALSE;
            m_TRAPrq = FALSE;
        }
        else if (m_FIS_rq)  // FIS commands -- Floating point Instruction Set
        {
            intrVector = 0010;  intrMode = TRUE;
            m_FIS_rq = FALSE;
        }
        else if (m_RPLYrq)  // Зависание, priority 1
        {
            if (m_buserror)
            {
                intrVector = 0174; intrMode = TRUE;
            }
            else
                if (currMode)
                {
                    intrVector = 0004;  intrMode = TRUE;
                }
                else
                {
                    intrVector = 0000004; intrMode = FALSE;
                }
            m_buserror = TRUE;
            m_RPLYrq = FALSE;
        }
        else if (m_ILLGrq)
        {
            intrVector = 000004;  intrMode = FALSE;
            m_ILLGrq = FALSE;
        }
        else if (m_RSVDrq)  // Reserved command, priority 2
        {
            intrVector = 000010;  intrMode = FALSE;
            m_RSVDrq = FALSE;
        }
        else if (m_TBITrq && (!m_waitmode))  // T-bit, priority 3
        {
            intrVector = 000014;  intrMode = FALSE;
            m_TBITrq = FALSE;
        }
        else if (m_ACLOrq && (m_psw & 0600) != 0600)  // ACLO, priority 4
        {
            intrVector = 000024;  intrMode = FALSE;
            m_ACLOreset = TRUE;
        }
        else if (m_haltpin && (m_psw & 0400) != 0400)  // HALT signal in USER mode, priority 5
        {
            intrVector = 0170;  intrMode = TRUE;
        }
        else if (m_EVNTrq && (m_psw & 0200) != 0200)  // EVNT signal, priority 6
        {
            intrVector = 0000100;  intrMode = FALSE;
            m_EVNTreset = TRUE;
        }
        else if ((m_psw & 0200) != 0200)  // VIRQ, priority 7
        {
            intrMode = FALSE;
            for (int irq = 1; irq <= 15; irq++)
            {
                if (m_virq[irq] != 0)
                {
                    intrVector = m_virq[irq];
                    m_VIRQreset = irq;
                    break;
                }
            }
        }
        if (intrVector != 0xFFFF)
        {
            if (m_internalTick == 0) m_internalTick = EMT_TIMING;  //ANYTHING UNKNOWN WILL CAUSE EXCEPTION (EMT)

            m_waitmode = FALSE;

            if (intrMode)  // HALT mode interrupt
            {
                WORD selVector = GetMemoryController()->GetSelRegister() & 0x0ff00;
                WORD new_pc,new_psw;
                intrVector |= selVector;
                    // Save PC/PSW to CPC/CPSW
                //m_savepc = GetPC();
                //m_savepsw = GetPSW();
                //m_psw |= 0400;
                SetHALT(TRUE);
                new_pc = GetWord(intrVector);
                new_psw = GetWord(intrVector + 2);
                if (!m_RPLYrq)
                {
                    SetPSW(new_psw);
                    SetPC(new_pc);
                }
            }
            else  // USER mode interrupt
            {
                WORD new_pc,new_psw;

                SetHALT(FALSE);
                // Save PC/PSW to stack
                SetSP(GetSP() - 2);
                SetWord(GetSP(), GetCPSW());
                SetSP(GetSP() - 2);
                if (!m_RPLYrq)
                {
                    SetWord(GetSP(), GetCPC());
                    if (!m_RPLYrq)
                    {
                        if (m_ACLOreset) m_ACLOrq = FALSE;
                        if (m_EVNTreset) m_EVNTrq = FALSE;
                        if (m_VIRQreset) m_virq[m_VIRQreset] = 0;
                        new_pc = GetWord(intrVector);
                        new_psw = GetWord(intrVector + 2);
                        if (!m_RPLYrq)
                        {
                            SetLPSW(LOBYTE(new_psw));
                            SetPC(new_pc);
                        }
                    }
                }
            }
        
            return TRUE;
        }
    }
    return FALSE;
}

void CProcessor::CommandExecution()
{
    if (!m_waitmode)
    {
        FetchInstruction();  // Read next instruction from memory
        if (!m_RPLYrq)
        {
            m_buserror = FALSE;
            TranslateInstruction();  // Execute next instruction
        }
    }
    if (m_HALTrq || m_BPT_rq || m_IOT_rq || m_EMT_rq || m_TRAPrq || m_FIS_rq)
        CProcessor::InterruptProcessing ();
}

void CProcessor::TickEVNT()
{
    if (m_okStopped) return;  // Processor is stopped - nothing to do

    m_EVNTrq = TRUE;
}

void CProcessor::InterruptVIRQ(int que, WORD interrupt)
{
    if (m_okStopped) return;  // Processor is stopped - nothing to do
    m_virq[que] = interrupt;
}
WORD CProcessor::GetVIRQ(int que)
{
    return m_virq[que];
}

void CProcessor::SetHALTPin(BOOL value)
{
    m_haltpin = value;
}

void CProcessor::SetDCLOPin(BOOL value)
{
    m_DCLOpin = value;
    if (m_DCLOpin)
    {
        m_okStopped = TRUE;

        m_stepmode = FALSE;
        m_buserror = FALSE;
        m_waitmode = FALSE;
        m_internalTick = 0;
        m_RPLYrq = m_RSVDrq = m_TBITrq = m_ACLOrq = m_HALTrq = m_EVNTrq = FALSE;
        m_ILLGrq = m_FIS_rq = m_BPT_rq = m_IOT_rq = m_EMT_rq = m_TRAPrq = FALSE;
        memset(m_virq, 0, sizeof(m_virq));
        m_ACLOreset = m_EVNTreset = FALSE; m_VIRQreset = 0;
        m_pMemoryController->DCLO_Signal();
        m_pMemoryController->ResetDevices();
    }
}

void CProcessor::SetACLOPin(BOOL value)
{
    if (m_okStopped && !m_DCLOpin && m_ACLOpin && !value)
    {
        m_okStopped = FALSE;
        m_internalTick = 0;

        m_stepmode = FALSE;
        m_waitmode = FALSE;
        m_buserror = FALSE;
        m_RPLYrq = m_RSVDrq = m_TBITrq = m_ACLOrq = m_HALTrq = m_EVNTrq = FALSE;
        m_ILLGrq = m_FIS_rq = m_BPT_rq = m_IOT_rq = m_EMT_rq = m_TRAPrq = FALSE;
        memset(m_virq, 0, sizeof(m_virq));
        m_ACLOreset = m_EVNTreset = FALSE; m_VIRQreset = 0;

        // "Turn On" interrupt processing
        m_STRTrq = TRUE;
    }
    if (!m_okStopped && !m_DCLOpin && !m_ACLOpin && value)
    {
        m_ACLOrq = TRUE;
    }
    m_ACLOpin = value;
}

void CProcessor::MemoryError()
{
    m_RPLYrq = TRUE;
}


//////////////////////////////////////////////////////////////////////

void CProcessor::FetchInstruction()
{
    // Считываем очередную инструкцию
    WORD pc = GetPC();
    ASSERT((pc & 1) == 0); // it have to be word aligned

    m_instruction = GetWordExec(pc);
    SetPC(GetPC() + 2);
}

void CProcessor::TranslateInstruction ()
{
    // Prepare values to help decode the command
    m_regdest  = GetDigit(m_instruction, 0);
    m_methdest = GetDigit(m_instruction, 1);
    m_regsrc   = GetDigit(m_instruction, 2);
    m_methsrc  = GetDigit(m_instruction, 3);

    // Find command implementation using the command map
    ExecuteMethodRef methodref = m_pExecuteMethodMap[m_instruction];
    (this->*methodref)();  // Call command implementation method
}

void CProcessor::ExecuteUNKNOWN ()  // Нет такой инструкции - просто вызывается TRAP 10
{
#if !defined(PRODUCT)
    DebugPrintFormat(_T(">>Invalid OPCODE = %06o @ %06o\r\n"), m_instruction, GetPC()-2);
#endif

    m_RSVDrq = TRUE;
}


// Instruction execution /////////////////////////////////////////////

void CProcessor::ExecuteWAIT ()  // WAIT - Wait for an interrupt
{
    m_waitmode = TRUE;
}

void CProcessor::ExecuteSTEP()  // ШАГ
{
    if ((m_psw & PSW_HALT) == 0)  // Эта команда выполняется только в режиме HALT
    {
        m_RSVDrq = TRUE;
        return;
    }

    SetPC(m_savepc);        // СК <- КРСК
    SetPSW(m_savepsw);      // РСП(8:0) <- КРСП(8:0)
    m_stepmode = TRUE;
}

void CProcessor::ExecuteRSEL()  // ЧПТ
{
    if ((m_psw & PSW_HALT) == 0)  // Эта команда выполняется только в режиме HALT
    {
        m_RSVDrq = TRUE;
        return;
    }

    SetReg(0, GetMemoryController()->GetSelRegister());  // R0 <- (SEL)
}

void CProcessor::ExecuteFIS()  // Floating point instruction set
{
    if (GetMemoryController()->GetSelRegister() & 0200)
        m_RSVDrq = TRUE;
    else
        m_FIS_rq = TRUE;
}

void CProcessor::ExecuteRUN()  // ПУСК
{
    if ((m_psw & PSW_HALT) == 0)  // Эта команда выполняется только в режиме HALT
    {
        m_RSVDrq = TRUE;
        return;
    }

    SetPC(m_savepc);        // СК <- КРСК
    SetPSW(m_savepsw);      // РСП(8:0) <- КРСП(8:0)
}

void CProcessor::ExecuteHALT ()  // HALT - Останов
{
    m_HALTrq = TRUE;
}

void CProcessor::ExecuteRCPC()  // ЧКСК
{
    if ((m_psw & PSW_HALT) == 0)  // Эта команда выполняется только в режиме HALT
    {
        m_RSVDrq = TRUE;
        return;
    }

    SetReg(0, m_savepc);        // R0 <- КРСК
    m_internalTick = NOP_TIMING;
}
void CProcessor::ExecuteRCPS()  // ЧКСП
{
    if ((m_psw & PSW_HALT) == 0)  // Эта команда выполняется только в режиме HALT
    {
        m_RSVDrq = TRUE;
        return;
    }

    SetReg(0, m_savepsw);       // R0 <- КРСП
    m_internalTick = NOP_TIMING;
}
void CProcessor::ExecuteWCPC()  // ЗКСК
{
    if ((m_psw & PSW_HALT) == 0)  // Эта команда выполняется только в режиме HALT
    {
        m_RSVDrq = TRUE;
        return;
    }

    m_savepc = GetReg(0);       // КРСК <- R0
    m_internalTick = NOP_TIMING;
}
void CProcessor::ExecuteWCPS()  // ЗКСП
{
    if ((m_psw & PSW_HALT) == 0)  // Эта команда выполняется только в режиме HALT
    {
        m_RSVDrq = TRUE;
        return;
    }

    m_savepsw = GetReg(0);      // КРСП <- R0
    m_internalTick = NOP_TIMING;
}

void CProcessor::ExecuteMFUS ()  // ЧЧП, move from user space
{
    WORD word;

    if ((m_psw & PSW_HALT) == 0)  // Эта команда выполняется только в режиме HALT
    {
        m_RSVDrq = TRUE;
        return;
    }

    //r0 = (r5)+
    SetHALT(FALSE);
    word = GetWord(GetReg(5));
    SetHALT(TRUE);
    SetReg(5,GetReg(5)+2);
    if (!m_RPLYrq) 	SetReg(0, word);

    m_internalTick = MOV_TIMING[0][2];
}

void CProcessor::ExecuteMTUS()  // ЗЧП, move to user space
{
    if ((m_psw & PSW_HALT) == 0)  // Эта команда выполняется только в режиме HALT
    {
        m_RSVDrq = TRUE;
        return;
    }

    // -(r5) = r0
    SetReg(5, GetReg(5) - 2);
    SetHALT(FALSE);
    SetWord(GetReg(5), GetReg(0));
    SetHALT(TRUE);
    m_internalTick = MOV_TIMING[0][2];
}

void CProcessor::ExecuteRTI ()  // RTI - Возврат из прерывания
{
    WORD word;
    word = GetWord(GetSP());
    SetSP( GetSP() + 2 );
    if (m_RPLYrq) return;
    SetPC(word);  // Pop PC
    word = GetWord ( GetSP() );  // Pop PSW --- saving HALT
    SetSP( GetSP() + 2 );
    if (m_RPLYrq) return;
    if(GetPC() < 0160000)
        SetLPSW(LOBYTE(word)); 
    else
        SetPSW(word); //load new mode
    m_internalTick=RTI_TIMING;
}

void CProcessor::ExecuteRTT ()  // RTT - return from trace trap
{
    WORD word;
    word = GetWord(GetSP());
    SetSP( GetSP() + 2 );
    if (m_RPLYrq) return;
    SetPC(word);  // Pop PC
    word = GetWord ( GetSP() );  // Pop PSW --- saving HALT
    SetSP( GetSP() + 2 );
    if (m_RPLYrq) return;
    if(GetPC() < 0160000)
        SetLPSW(LOBYTE(word)); 
    else
        SetPSW(word); //load new mode

    m_stepmode = (word & PSW_T)?TRUE:FALSE;

    m_internalTick = RTI_TIMING;
}

void CProcessor::ExecuteBPT ()  // BPT - Breakpoint
{
    m_BPT_rq = TRUE;
    m_internalTick = BPT_TIMING;
}

void CProcessor::ExecuteIOT ()  // IOT - I/O trap
{
    m_IOT_rq = TRUE;
    m_internalTick=EMT_TIMING;
}

void CProcessor::ExecuteRESET ()  // Reset input/output devices
{
    m_EVNTrq = FALSE;
    m_pMemoryController->ResetDevices();  // INIT signal

    m_internalTick = RESET_TIMING;
}

void CProcessor::ExecuteRTS ()  // RTS - return from subroutine - Возврат из процедуры
{
    WORD word;
    SetPC(GetReg(m_regdest));
    word = GetWord(GetSP());
    SetSP(GetSP()+2);
    if (m_RPLYrq) return;
    SetReg(m_regdest, word);
    m_internalTick=RTS_TIMING;
}

void CProcessor::ExecuteCCC ()
{
    SetLPSW(GetLPSW() &  ~(LOBYTE(m_instruction) & 017));
    m_internalTick=NOP_TIMING;
}
void CProcessor::ExecuteSCC ()
{
    SetLPSW(GetLPSW() |  (LOBYTE(m_instruction) & 017));
    m_internalTick=NOP_TIMING;
}

void CProcessor::ExecuteJMP ()  // JMP - jump: PC = &d (a-mode > 0)
{
    if (m_methdest == 0)  // Неправильный метод адресации
    {
        m_ILLGrq = TRUE;
        m_internalTick = EMT_TIMING;
    }
    else 
    {
        WORD word;
        word = GetWordAddr(m_methdest,m_regdest);
        if (m_RPLYrq) return;
        SetPC(word);
        m_internalTick=JMP_TIMING[m_methdest-1];
    }
}

void CProcessor::ExecuteSWAB ()
{
    WORD ea;
    WORD dst;
    BYTE new_psw = GetLPSW() & 0xF0;

    if (m_methdest)
    {	
        ea = GetWordAddr(m_methdest,m_regdest);
        if (m_RPLYrq) return;
        dst=GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regdest);
    
    dst=((dst>>8)&0377) | (dst<<8);

    if(m_methdest)
        SetWord(ea,dst);
    else
        SetReg(m_regdest,dst);
    
    if (m_RPLYrq) return;
    
    if ((dst&0200)!=0) new_psw |= PSW_N;
    if (LOBYTE(dst) == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
    m_internalTick=MOV_TIMING[m_methdest][m_methdest];
}

void CProcessor::ExecuteCLR ()  // CLR
{
    WORD dst_addr;
    if(m_instruction&0100000)
    {
        if(m_methdest)
        {
            dst_addr = GetByteAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            GetByte(dst_addr);
            if (m_RPLYrq) return;
            SetByte(dst_addr, 0);
            if (m_RPLYrq) return;
        }
        else
            SetLReg(m_regdest, 0);
        
        SetLPSW((GetLPSW() & 0xF0) | PSW_Z);
        m_internalTick=CLR_TIMING[m_methdest];
    }
    else
    {
        if(m_methdest)
        {
            dst_addr = GetWordAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            SetWord(dst_addr, 0);
            if (m_RPLYrq) return;
        }
        else
            SetReg(m_regdest,0);

        SetLPSW((GetLPSW() & 0xF0) | PSW_Z);
        m_internalTick=CLR_TIMING[m_methdest];
    }
}

void CProcessor::ExecuteCOM ()  // COM
{
    WORD ea;
    BYTE new_psw = GetLPSW() & 0xF0;
    if(m_instruction&0100000)
    {
        BYTE dst;

        if (m_methdest)
        {
            ea = GetByteAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            dst= GetByte(ea);
            if (m_RPLYrq) return;
        }
        else
            dst= GetLReg(m_regdest);

        dst = ~dst;

        if(m_methdest)
            SetByte(ea,dst);
        else
            SetLReg(m_regdest,dst);
        if (m_RPLYrq) return;
        
        if (dst & 0200) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        new_psw |= PSW_C;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
        
    }
    else
    {
        WORD dst;

        if (m_methdest)
        {
            ea = GetWordAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            dst= GetWord(ea);
            if (m_RPLYrq) return;
        }
        else
            dst= GetReg(m_regdest);

        dst = ~dst;

        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0100000) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        new_psw |= PSW_C;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
    }
}

void CProcessor::ExecuteINC ()  // INC - Инкремент
{
    WORD ea;
    BYTE new_psw = GetLPSW() & 0xF1;
    if(m_instruction&0100000)
    {
        BYTE dst;

        if (m_methdest)
        {
            ea = GetByteAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            dst= GetByte(ea);
            if (m_RPLYrq) return;
        }
        else
            dst= GetLReg(m_regdest);

        dst =dst + 1;
        
        if(m_methdest)
            SetByte(ea,dst);
        else
            SetLReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0200) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if (dst == 0200) new_psw |= PSW_V;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
    }
    else
    {
        WORD dst;

        if (m_methdest)
        {
            ea = GetWordAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            dst= GetWord(ea);
            if (m_RPLYrq) return;
        }
        else
            dst= GetReg(m_regdest);

        dst = dst + 1;

        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0100000) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if (dst == 0100000) new_psw |= PSW_V;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
    }
}

void CProcessor::ExecuteDEC ()  // DEC - Декремент
{
    WORD ea;
    BYTE new_psw = GetLPSW() & 0xF1;
    if(m_instruction&0100000)
    {
        BYTE dst;

        if (m_methdest)
        {
            ea = GetByteAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            dst= GetByte(ea);
            if (m_RPLYrq) return;
        }
        else
            dst= GetLReg(m_regdest);

        dst =dst - 1;
        
        if(m_methdest)
            SetByte(ea,dst);
        else
            SetLReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0200) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if (dst == 0177) new_psw |= PSW_V;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
    }
    else
    {
        WORD dst;

        if (m_methdest)
        {
            ea = GetWordAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            dst= GetWord(ea);
            if (m_RPLYrq) return;
        }
        else
            dst= GetReg(m_regdest);

        dst = dst - 1;

        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0100000) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if (dst == 077777) new_psw |= PSW_V;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
    }
}

void CProcessor::ExecuteNEG ()
{
    WORD ea;
    BYTE new_psw = GetLPSW() & 0xF0;
    if(m_instruction&0100000)
    {
        BYTE dst;

        if (m_methdest)
        {
            ea = GetByteAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            dst= GetByte(ea);
            if (m_RPLYrq) return;
        }
        else
            dst= GetLReg(m_regdest);

        dst = 0 - dst ;
        
        if(m_methdest)
            SetByte(ea,dst);
        else
            SetLReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0200) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if (dst == 0200) new_psw |= PSW_V;
        if (dst != 0) new_psw |= PSW_C;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
    }
    else
    {
        WORD dst;

        if (m_methdest)
        {
            ea = GetWordAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            dst= GetWord(ea);
            if (m_RPLYrq) return;
        }
        else
            dst= GetReg(m_regdest);

        dst = 0 - dst;

        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0100000) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if (dst == 0100000) new_psw |= PSW_V;
        if (dst != 0) new_psw |= PSW_C;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
    }
}

void CProcessor::ExecuteADC ()  // ADC{B}
{
    WORD ea;
    BYTE new_psw = GetLPSW() & 0xF0;
    if(m_instruction&0100000)
    {
        BYTE dst;

        if (m_methdest)
        {
            ea = GetByteAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            dst= GetByte(ea);
            if (m_RPLYrq) return;
        }
        else
            dst= GetLReg(m_regdest);

        dst = dst + (GetC()?1:0);
        
        if(m_methdest)
            SetByte(ea,dst);
        else
            SetLReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0200) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if ((dst == 0200) && GetC()) new_psw |= PSW_V;
        if ((dst == 0) && GetC()) new_psw |= PSW_C;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
    }
    else
    {
        WORD dst;

        if (m_methdest)
        {
            ea = GetWordAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            dst= GetWord(ea);
            if (m_RPLYrq) return;
        }
        else
            dst= GetReg(m_regdest);

        dst = dst + (GetC()?1:0);

        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0100000) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if ((dst == 0100000) && GetC()) new_psw |= PSW_V;
        if ((dst == 0) && GetC()) new_psw |= PSW_C;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
    }
}

void CProcessor::ExecuteSBC ()  // SBC{B}
{
    WORD ea;
    BYTE new_psw = GetLPSW() & 0xF0;
    if(m_instruction&0100000)
    {
        BYTE dst;

        if (m_methdest)
        {
            ea = GetByteAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            dst= GetByte(ea);
            if (m_RPLYrq) return;
        }
        else
            dst= GetLReg(m_regdest);

        dst = dst - (GetC()?1:0);
        
        if(m_methdest)
            SetByte(ea,dst);
        else
            SetLReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0200) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if ((dst == 0177) && GetC()) new_psw |= PSW_V;
        if ((dst == 0377) && GetC()) new_psw |= PSW_C;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
    }
    else
    {
        WORD dst;

        if (m_methdest)
        {
            ea = GetWordAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            dst= GetWord(ea);
            if (m_RPLYrq) return;
        }
        else
            dst= GetReg(m_regdest);

        dst = dst - (GetC()?1:0);

        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0100000) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if ((dst == 077777) && GetC()) new_psw |= PSW_V;
        if ((dst == 0177777) && GetC()) new_psw |= PSW_C;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
    }
}

void CProcessor::ExecuteTST ()  // TST{B} - test
{
    WORD ea;
    BYTE new_psw = GetLPSW() & 0xF0;
    if(m_instruction&0100000)
    {
        BYTE dst;

        if (m_methdest)
        {
            ea = GetByteAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            dst= GetByte(ea);
            if (m_RPLYrq) return;
        }
        else
            dst= GetLReg(m_regdest);

        if (dst & 0200) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        SetLPSW(new_psw);
        m_internalTick=TST_TIMING[m_methdest];
    }
    else
    {
        WORD dst;

        if (m_methdest)
        {
            ea = GetWordAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            dst= GetWord(ea);
            if (m_RPLYrq) return;
        }
        else
            dst= GetReg(m_regdest);

        if (dst & 0100000) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        SetLPSW(new_psw);
        m_internalTick=TST_TIMING[m_methdest];
    }
}

void CProcessor::ExecuteROR ()  // ROR{B}
{
    WORD ea;
    BYTE new_psw = GetLPSW() & 0xF0;
    if(m_instruction&0100000)
    {
        BYTE src, dst;

        if (m_methdest)
        {
            ea = GetByteAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            src = GetByte(ea);
            if (m_RPLYrq) return;
        }
        else
            src = GetLReg(m_regdest);

        dst = (src>>1)|(GetC()?0200:0);
        
        if(m_methdest)
            SetByte(ea,dst);
        else
            SetLReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0200) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if (src & 1) new_psw |= PSW_C;
        if (((new_psw & PSW_N)!=0) != ((new_psw & PSW_C)!=0)) new_psw |= PSW_V;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
    }
    else
    {
        WORD src, dst;

        if (m_methdest)
        {
            ea = GetWordAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            src = GetWord(ea);
            if (m_RPLYrq) return;
        }
        else
            src = GetReg(m_regdest);

        dst = (src>>1)|(GetC()?0100000:0);

        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0100000) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if (src & 1) new_psw |= PSW_C;
        if (((new_psw & PSW_N)!=0) != ((new_psw & PSW_C)!=0)) new_psw |= PSW_V;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
    }
}

void CProcessor::ExecuteROL ()  // ROL{B}
{
    WORD ea;
    BYTE new_psw = GetLPSW() & 0xF0;
    if(m_instruction&0100000)
    {
        BYTE src, dst;

        if (m_methdest)
        {
            ea = GetByteAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            src = GetByte(ea);
            if (m_RPLYrq) return;
        }
        else
            src = GetLReg(m_regdest);

        dst = (src<<1)|(GetC()?1:0);
        
        if(m_methdest)
            SetByte(ea,dst);
        else
            SetLReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0200) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if (src & 0200) new_psw |= PSW_C;
        if (((new_psw & PSW_N)!=0) != ((new_psw & PSW_C)!=0)) new_psw |= PSW_V;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
    }
    else
    {
        WORD src, dst;

        if (m_methdest)
        {
            ea = GetWordAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            src = GetWord(ea);
            if (m_RPLYrq) return;
        }
        else
            src = GetReg(m_regdest);

        dst = (src<<1)|(GetC()?1:0);

        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0100000) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if (src & 0100000) new_psw |= PSW_C;
        if (((new_psw & PSW_N)!=0) != ((new_psw & PSW_C)!=0)) new_psw |= PSW_V;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
    }
}

void CProcessor::ExecuteASR ()  // ASR{B}
{
    WORD ea;
    BYTE new_psw = GetLPSW() & 0xF0;
    if(m_instruction&0100000)
    {
        BYTE src, dst;

        if (m_methdest)
        {
            ea = GetByteAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            src = GetByte(ea);
            if (m_RPLYrq) return;
        }
        else
            src = GetLReg(m_regdest);

        dst = (src>>1)|(src & 0200);
        
        if(m_methdest)
            SetByte(ea,dst);
        else
            SetLReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0200) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if (src & 1) new_psw |= PSW_C;
        if (((new_psw & PSW_N)!=0) != ((new_psw & PSW_C)!=0)) new_psw |= PSW_V;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
    }
    else
    {
        WORD src, dst;

        if (m_methdest)
        {
            ea = GetWordAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            src = GetWord(ea);
            if (m_RPLYrq) return;
        }
        else
            src = GetReg(m_regdest);

        dst = (src>>1)|(src & 0100000);

        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0100000) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if (src & 1) new_psw |= PSW_C;
        if (((new_psw & PSW_N)!=0) != ((new_psw & PSW_C)!=0)) new_psw |= PSW_V;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
    }
}

void CProcessor::ExecuteASL ()  // ASL{B}
{
    WORD ea;
    BYTE new_psw = GetLPSW() & 0xF0;
    if(m_instruction&0100000)
    {
        BYTE src, dst;

        if (m_methdest)
        {
            ea = GetByteAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            src = GetByte(ea);
            if (m_RPLYrq) return;
        }
        else
            src = GetLReg(m_regdest);

        dst = src<<1;
        
        if(m_methdest)
            SetByte(ea,dst);
        else
            SetLReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0200) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if (src & 0200) new_psw |= PSW_C;
        if (((new_psw & PSW_N)!=0) != ((new_psw & PSW_C)!=0)) new_psw |= PSW_V;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
    }
    else
    {
        WORD src, dst;

        if (m_methdest)
        {
            ea = GetWordAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            src = GetWord(ea);
            if (m_RPLYrq) return;
        }
        else
            src = GetReg(m_regdest);

        dst = src<<1;

        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0100000) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if (src & 0100000) new_psw |= PSW_C;
        if (((new_psw & PSW_N)!=0) != ((new_psw & PSW_C)!=0)) new_psw |= PSW_V;
        SetLPSW(new_psw);
        m_internalTick=CLR_TIMING[m_methdest];
    }
}

void CProcessor::ExecuteSXT ()  // SXT - sign-extend
{
    WORD ea;
    BYTE new_psw = GetLPSW() & 0xF9;
    if(m_methdest)
    {
        ea = GetWordAddr(m_methdest,m_regdest);
        if (m_RPLYrq) return;
        SetWord(ea,GetN()?0177777:0);
        if (m_RPLYrq) return;
    }
    else
        SetReg(m_regdest,GetN()?0177777:0); //sign extend	

    if (!GetN()) new_psw |= PSW_Z;
    SetLPSW(new_psw);
    m_internalTick=CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteMTPS ()  // MTPS - move to PS
{
    WORD ea;
    BYTE dst;

    if(m_methdest)
    {
        ea = GetByteAddr(m_methdest,m_regdest);
        if (m_RPLYrq) return;
        dst = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetLReg(m_regdest);
    
    SetLPSW((GetLPSW() & 0x10) | (dst & 0xEF));
    SetPC(GetPC());
    m_internalTick=MTPS_TIMING[m_methdest];
}

void CProcessor::ExecuteMFPS ()  // MFPS - move from PS
{
    WORD ea;
    BYTE psw = GetLPSW();
    BYTE new_psw = psw & 0xF1;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest,m_regdest);
        if (m_RPLYrq) return;
        GetByte(ea);
        if (m_RPLYrq) return;
        SetByte(ea, psw);
        if (m_RPLYrq) return;
    }
    else
        SetReg(m_regdest, (WORD)(signed short)(char)psw); //sign extend
    
    if (psw & 0200) new_psw |= PSW_N;
    if (psw == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
    m_internalTick=CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteBR ()
{
    SetPC(GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    m_internalTick=BR_TIMING;
}

void CProcessor::ExecuteBNE ()
{
    m_internalTick=BRANCH_FALSE_TIMING;
    if (! GetZ())
    {
        SetPC(GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
        m_internalTick=BRANCH_TRUE_TIMING;
    }
}

void CProcessor::ExecuteBEQ ()
{
    m_internalTick = BRANCH_FALSE_TIMING;
    if (GetZ())
    {
        m_internalTick = BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }
}

void CProcessor::ExecuteBGE ()
{
    m_internalTick=BRANCH_FALSE_TIMING;
    if (GetN() == GetV())
    {
        m_internalTick=BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }
}

void CProcessor::ExecuteBLT ()
{
    m_internalTick=BRANCH_FALSE_TIMING;
    if (GetN() != GetV())
    {
        m_internalTick=BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }
}

void CProcessor::ExecuteBGT ()
{
    m_internalTick=BRANCH_FALSE_TIMING;
    if (! ((GetN() != GetV()) || GetZ()))
    {
        m_internalTick=BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }
}

void CProcessor::ExecuteBLE ()
{
    m_internalTick=BRANCH_FALSE_TIMING;
    if ((GetN() != GetV()) || GetZ())
    {
        m_internalTick=BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }
}

void CProcessor::ExecuteBPL ()
{
    m_internalTick=BRANCH_FALSE_TIMING;
    if (! GetN())
    {
        m_internalTick=BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }
}

void CProcessor::ExecuteBMI ()
{
    m_internalTick=BRANCH_FALSE_TIMING;
    if (GetN())
    {
        m_internalTick=BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }
}

void CProcessor::ExecuteBHI ()
{
    m_internalTick=BRANCH_FALSE_TIMING;
    if (! (GetZ() || GetC()))
    {
        m_internalTick=BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }
}

void CProcessor::ExecuteBLOS ()
{
    m_internalTick=BRANCH_FALSE_TIMING;
    if (GetZ() || GetC())
    {
        m_internalTick=BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }
}

void CProcessor::ExecuteBVC ()
{
    m_internalTick=BRANCH_FALSE_TIMING;
    if (! GetV())
    {
        m_internalTick=BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }
}

void CProcessor::ExecuteBVS ()
{
    m_internalTick=BRANCH_FALSE_TIMING;
    if (GetV())
    {
        m_internalTick=BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }
}

void CProcessor::ExecuteBHIS ()
{
    m_internalTick=BRANCH_FALSE_TIMING;
    if (! GetC())
    {
        m_internalTick=BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }
}

void CProcessor::ExecuteBLO ()
{
    m_internalTick=BRANCH_FALSE_TIMING;
    if (GetC())
    {
        m_internalTick=BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }
}

void CProcessor::ExecuteXOR ()  // XOR
{
    WORD dst;
    WORD ea;
    BYTE new_psw = GetLPSW() & 0xF1;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest,m_regdest);
        if (m_RPLYrq) return;
        dst = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst=GetReg(m_regdest);
    
    dst = dst ^ GetReg(m_regsrc);
    
    if(m_methdest)
        SetWord(ea,dst);
    else
        SetReg(m_regdest,dst);
    if (m_RPLYrq) return;

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
    m_internalTick=XOR_TIMING[m_methdest];
}

void CProcessor::ExecuteMUL ()  // MUL
{
    WORD dst = GetReg(m_regsrc);
    WORD src, ea;
    int res;
    BYTE new_psw = GetLPSW() & 0xF0;

    if (m_methdest) ea = GetWordAddr(m_methdest,m_regdest);
    if (m_RPLYrq) return;
    src = m_methdest?GetWord(ea):GetReg(m_regdest);
    if (m_RPLYrq) return;
    
    res=(signed short)dst*(signed short)src;
    
    SetReg(m_regsrc,HIWORD(res));
    SetReg(m_regsrc|1,LOWORD(res));

    if (res<0) new_psw |= PSW_N;
    if (res==0) new_psw |= PSW_Z;
    if ((res > 32767) || (res < -32768)) new_psw |= PSW_C;
    SetLPSW(new_psw);
    m_internalTick=MUL_TIMING[m_methdest];
}
void CProcessor::ExecuteDIV ()  // DIV
{
    //время надо считать тут
    WORD ea;
    int longsrc;
    int res,res1,src2;
    BYTE new_psw = GetLPSW() & 0xF0;

    if (m_methdest) ea = GetWordAddr(m_methdest,m_regdest);
    if (m_RPLYrq) return;
    src2=(int)(signed short)(m_methdest?GetWord(ea):GetReg(m_regdest));
    if (m_RPLYrq) return;

    longsrc=(int)MAKELONG(GetReg(m_regsrc|1),GetReg(m_regsrc));

    m_internalTick=DIV_TIMING[m_methdest];

    if(src2==0)
    {
        new_psw |= (PSW_V | PSW_C); //если делят на 0 -- то устанавливаем V и C
        SetLPSW(new_psw);
        return; 
    }	
    if ((longsrc == 020000000000) && (src2 == -1))
    {
        new_psw |= PSW_V; // переполняемся, товарищи
        SetLPSW(new_psw);
        return;
    }

    res=longsrc/src2;
    res1=longsrc%src2;

    if ((res > 32767) || (res < -32768)) 
    {
        new_psw |= PSW_V; // переполняемся, товарищи
        SetLPSW(new_psw);
        return;
    }

    SetReg(m_regsrc|1,res1&0177777);
    SetReg(m_regsrc,res&0177777);

    if (res<0) new_psw |= PSW_N;
    if (res==0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
}
void CProcessor::ExecuteASH ()  // ASH
{
    WORD ea;
    short src;
    short dst;
    BYTE new_psw = GetLPSW() & 0xF0;

    if (m_methdest) ea = GetWordAddr(m_methdest,m_regdest);
    if (m_RPLYrq) return;
    src = (short)(m_methdest?GetWord(ea):GetReg(m_regdest));
    if (m_RPLYrq) return;
    src &= 0x3F;
	src |= (src & 040)?0177700:0;
    dst = (short)GetReg(m_regsrc);

    m_internalTick=ASH_TIMING[m_methdest];

    if (src >= 0)
    {
     while (src--)
     {
      if (dst & 0100000) new_psw |= PSW_C; else new_psw &= ~PSW_C;
      dst <<= 1;
      if ((dst<0) != ((new_psw & PSW_C)!=0)) new_psw |= PSW_V;
      m_internalTick+=ASH_S_TIMING;
     }
    }
    else
    {
     while (src++)
     {
      if (dst & 1) new_psw |= PSW_C; else new_psw &= ~PSW_C;
      dst >>= 1;
      m_internalTick+=ASH_S_TIMING;
     }
    }
    
    SetReg(m_regsrc,dst);
        
    if (dst<0) new_psw |= PSW_N;
    if (dst==0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
}
void CProcessor::ExecuteASHC ()  // ASHC
{
    WORD ea;
    short src;
    long dst;
    BYTE new_psw = GetLPSW() & 0xF0;

    if (m_methdest) ea = GetWordAddr(m_methdest,m_regdest);
    if (m_RPLYrq) return;
    src = (short)(m_methdest?GetWord(ea):GetReg(m_regdest));
    if (m_RPLYrq) return;
    src &= 0x3F;
	src |= (src & 040)?0177700:0;
    dst = MAKELONG(GetReg(m_regsrc|1),GetReg(m_regsrc));
    m_internalTick=ASHC_TIMING[m_methdest];
    if (src >= 0)
    {
     while (src--)
     {
      if (dst & 0x80000000L) new_psw |= PSW_C; else new_psw &= ~PSW_C;
      dst <<= 1;
      if ((dst<0) != ((new_psw & PSW_C)!=0)) new_psw |= PSW_V;
      m_internalTick+=ASHC_S_TIMING;
     }
    }
    else
    {
     while (src++)
     {
      if (dst & 1) new_psw |= PSW_C; else new_psw &= ~PSW_C;
      dst >>= 1;
      m_internalTick+=ASHC_S_TIMING;
     }
    }
        
    SetReg(m_regsrc,HIWORD(dst));
    SetReg(m_regsrc|1,LOWORD(dst));
        
    SetN(dst<0);
    SetZ(dst==0);
    if (dst<0) new_psw |= PSW_N;
    if (dst==0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
}

void CProcessor::ExecuteSOB ()  // SOB - subtract one: R = R - 1 ; if R != 0 : PC = PC - 2*nn
{
    WORD dst = GetReg(m_regsrc);
    
    m_internalTick=SOB_LAST_TIMING;
    --dst;
    SetReg(m_regsrc, dst);
    
    if (dst)
    {
        m_internalTick=SOB_TIMING;
        SetPC(GetPC() - (m_instruction & (WORD)077) * 2 );
    }
}

void CProcessor::ExecuteMOV ()
{
    WORD src_addr, dst_addr;
    BYTE new_psw = GetLPSW() & 0xF1;

    if(m_instruction&0100000)
    {
        BYTE dst;

        if (m_methsrc)
        {
            src_addr = GetByteAddr(m_methsrc,m_regsrc);
            if (m_RPLYrq) return;
            dst = GetByte(src_addr);
            if (m_RPLYrq) return;
        }
        else
            dst=GetLReg(m_regsrc);

        if (m_methdest)
        {
            dst_addr = GetByteAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            GetByte(dst_addr);
            if (m_RPLYrq) return;
            SetByte(dst_addr,dst);
            if (m_RPLYrq) return;
        }
        else
            SetReg(m_regdest,(WORD)(signed short)(char)dst);

        if (dst & 0200) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        SetLPSW(new_psw);
        m_internalTick=MOVB_TIMING[m_methsrc][m_methdest];
    }
    else
    {
        WORD dst;

        if (m_methsrc)
        {
            src_addr = GetWordAddr(m_methsrc,m_regsrc);
            if (m_RPLYrq) return;
            dst = GetWord(src_addr);
            if (m_RPLYrq) return;
        }
        else
            dst=GetReg(m_regsrc);

        if (m_methdest)
        {
            dst_addr = GetWordAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            SetWord(dst_addr,dst);
            if (m_RPLYrq) return;
        }
        else
            SetReg(m_regdest,dst);

        if (dst & 0100000) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        SetLPSW(new_psw);
        m_internalTick=MOV_TIMING[m_methsrc][m_methdest];
    }
}

void CProcessor::ExecuteCMP ()
{
    WORD src_addr, dst_addr;
    BYTE new_psw = GetLPSW() & 0xF0;

    if(m_instruction&0100000)
    {
        BYTE src;
        BYTE src2;
        BYTE dst;

        if (m_methsrc)
        {
            src_addr = GetByteAddr(m_methsrc,m_regsrc);
            if (m_RPLYrq) return;
            src = GetByte(src_addr);
            if (m_RPLYrq) return;
        }
        else
            src = GetLReg(m_regsrc);

        if (m_methdest)
        {
            dst_addr = GetByteAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            src2 = GetByte(dst_addr);
            if (m_RPLYrq) return;
        }
        else
            src2 = GetLReg(m_regdest);
        
        dst = src - src2;
        //SetN( CheckForNegative((BYTE)(src - src2)) );
        //SetZ( CheckForZero((BYTE)(src - src2)) );
        //SetV( CheckSubForOverflow (src, src2) );
        //SetC( CheckSubForCarry (src, src2) );
        if (dst & 0200) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if (((src ^ src2) & ~(dst ^ src2)) & 0200) new_psw |= PSW_V;
        if (((~src & src2) | (~(src ^ src2) & dst)) & 0200) new_psw |= PSW_C;
        SetLPSW(new_psw);
        m_internalTick=CMP_TIMING[m_methsrc][m_methdest];
    }
    else
    {
        WORD src;
        WORD src2;
        WORD dst;

        if (m_methsrc)
        {
            src_addr = GetWordAddr(m_methsrc,m_regsrc);
            if (m_RPLYrq) return;
            src = GetWord(src_addr);
            if (m_RPLYrq) return;
        }
        else
            src = GetReg(m_regsrc);

        if (m_methdest)
        {
            dst_addr = GetWordAddr(m_methdest,m_regdest);
            if (m_RPLYrq) return;
            src2 = GetWord(dst_addr);
            if (m_RPLYrq) return;
        }
        else
            src2 = GetReg(m_regdest);
        
        dst = src - src2;
        
        //SetN( CheckForNegative ((WORD)(src - src2)) );
        //SetZ( CheckForZero ((WORD)(src - src2)) );
        //SetV( CheckSubForOverflow (src, src2) );
        //SetC( CheckSubForCarry (src, src2) );
        if (dst & 0100000) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        if (((src ^ src2) & ~(dst ^ src2)) & 0100000) new_psw |= PSW_V;
        if (((~src & src2) | (~(src ^ src2) & dst)) & 0100000) new_psw |= PSW_C;
        SetLPSW(new_psw);
        m_internalTick=CMP_TIMING[m_methsrc][m_methdest];
    }
}

void CProcessor::ExecuteBIT ()  // BIT{B} - bit test
{
    WORD src_addr, dst_addr;
    BYTE new_psw = GetLPSW() & 0xF1;

    if(m_instruction&0100000)
    {
        BYTE src;
        BYTE src2;
        BYTE dst;
        
        if (m_methsrc)
        {
            src_addr = GetByteAddr(m_methsrc, m_regsrc);
            if (m_RPLYrq) return;
            src = GetByte(src_addr);
            if (m_RPLYrq) return;
        }
        else
            src = GetLReg(m_regsrc);

        if (m_methdest)
        {
            dst_addr = GetByteAddr(m_methdest, m_regdest);
            if (m_RPLYrq) return;
            src2 = GetByte(dst_addr);
            if (m_RPLYrq) return;
        }
        else
            src2 = GetLReg(m_regdest);

        dst = src2 & src;

        if (dst & 0200) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        SetLPSW(new_psw);
        m_internalTick = CMP_TIMING[m_methsrc][m_methdest];
    }
    else
    {
        WORD src;
        WORD src2;
        WORD dst;
        
        if (m_methsrc)
        {
            src_addr = GetWordAddr(m_methsrc, m_regsrc);
            if (m_RPLYrq) return;
            src = GetWord(src_addr);
            if (m_RPLYrq) return;
        }
        else
            src  = GetReg(m_regsrc);

        if (m_methdest)
        {
            dst_addr = GetWordAddr(m_methdest, m_regdest);
            if (m_RPLYrq) return;
            src2 = GetWord(dst_addr);
            if (m_RPLYrq) return;
        }
        else
            src2 = GetReg(m_regdest);

        dst = src2 & src;

        if (dst & 0100000) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        SetLPSW(new_psw);
        m_internalTick = CMP_TIMING[m_methsrc][m_methdest];
    }
}

void CProcessor::ExecuteBIC ()  // BIC{B} - bit clear
{
    WORD src_addr, dst_addr;
    BYTE new_psw = GetLPSW() & 0xF1;

    if(m_instruction&0100000)
    {
        BYTE src;
        BYTE src2;
        BYTE dst;
        
        if (m_methsrc)
        {
            src_addr = GetByteAddr(m_methsrc, m_regsrc);
            if (m_RPLYrq) return;
            src = GetByte(src_addr);
            if (m_RPLYrq) return;
        }
        else
            src = GetLReg(m_regsrc);

        if (m_methdest)
        {
            dst_addr = GetByteAddr(m_methdest, m_regdest);
            if (m_RPLYrq) return;
            src2 = GetByte(dst_addr);
            if (m_RPLYrq) return;
        }
        else
            src2 = GetLReg(m_regdest);

        dst=src2 & (~src);


        if(m_methdest)
            SetByte(dst_addr,dst);
        else
            SetLReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0200) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        SetLPSW(new_psw);
        m_internalTick=MOVB_TIMING[m_methsrc][m_methdest];
    }
    else
    {
        WORD src;
        WORD src2;
        WORD dst;
        
        if (m_methsrc)
        {
            src_addr = GetWordAddr(m_methsrc, m_regsrc);
            if (m_RPLYrq) return;
            src = GetWord(src_addr);
            if (m_RPLYrq) return;
        }
        else
            src  = GetReg(m_regsrc);

        if (m_methdest)
        {
            dst_addr = GetWordAddr(m_methdest, m_regdest);
            if (m_RPLYrq) return;
            src2 = GetWord(dst_addr);
            if (m_RPLYrq) return;
        }
        else
            src2 = GetReg(m_regdest);

        dst=src2 & (~src);

        if(m_methdest)
            SetWord(dst_addr,dst);
        else
            SetReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0100000) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        SetLPSW(new_psw);
        m_internalTick=MOV_TIMING[m_methsrc][m_methdest];
    }
}

void CProcessor::ExecuteBIS ()  // BIS{B} - bit set
{
    WORD src_addr, dst_addr;
    BYTE new_psw = GetLPSW() & 0xF1;

    if(m_instruction&0100000)
    {
        BYTE src;
        BYTE src2;
        BYTE dst;
        
        if (m_methsrc)
        {
            src_addr = GetByteAddr(m_methsrc, m_regsrc);
            if (m_RPLYrq) return;
            src = GetByte(src_addr);
            if (m_RPLYrq) return;
        }
        else
            src = GetLReg(m_regsrc);

        if (m_methdest)
        {
            dst_addr = GetByteAddr(m_methdest, m_regdest);
            if (m_RPLYrq) return;
            src2 = GetByte(dst_addr);
            if (m_RPLYrq) return;
        }
        else
            src2 = GetLReg(m_regdest);

        dst=src2 | src;


        if(m_methdest)
            SetByte(dst_addr,dst);
        else
            SetLReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0200) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        SetLPSW(new_psw);
        m_internalTick=MOVB_TIMING[m_methsrc][m_methdest];
    }
    else
    {
        WORD src;
        WORD src2;
        WORD dst;
        
        if (m_methsrc)
        {
            src_addr = GetWordAddr(m_methsrc, m_regsrc);
            if (m_RPLYrq) return;
            src = GetWord(src_addr);
            if (m_RPLYrq) return;
        }
        else
            src  = GetReg(m_regsrc);

        if (m_methdest)
        {
            dst_addr = GetWordAddr(m_methdest, m_regdest);
            if (m_RPLYrq) return;
            src2 = GetWord(dst_addr);
            if (m_RPLYrq) return;
        }
        else
            src2 = GetReg(m_regdest);

        dst=src2 | src;

        if(m_methdest)
            SetWord(dst_addr,dst);
        else
            SetReg(m_regdest,dst);
        if (m_RPLYrq) return;

        if (dst & 0100000) new_psw |= PSW_N;
        if (dst == 0) new_psw |= PSW_Z;
        SetLPSW(new_psw);
        m_internalTick=MOV_TIMING[m_methsrc][m_methdest];
    }
}

void CProcessor::ExecuteADD ()  // ADD
{
    WORD src_addr, dst_addr;
    BYTE new_psw = GetLPSW() & 0xF0;
    WORD src, src2, dst;
    
    if (m_methsrc)
    {
        src_addr = GetWordAddr(m_methsrc,m_regsrc);
        if (m_RPLYrq) return;
        src = GetWord(src_addr);
        if (m_RPLYrq) return;
    }
    else
     src = GetReg(m_regsrc);

    if (m_methdest)
    {
        dst_addr = GetWordAddr(m_methdest,m_regdest);
        if (m_RPLYrq) return;
        src2 = GetWord(dst_addr);
        if (m_RPLYrq) return;
    }
    else
        src2 = GetReg(m_regdest);

    dst = src2 + src;

    if(m_methdest)
        SetWord(dst_addr, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if ((~(src ^ src2) & (dst ^ src2)) & 0100000) new_psw |= PSW_V;
    if (((src & src2) | ((src ^ src2) & ~dst)) & 0100000) new_psw |= PSW_C;
    SetLPSW(new_psw);
    m_internalTick=MOVB_TIMING[m_methsrc][m_methdest];
}

void CProcessor::ExecuteSUB ()
{
    WORD src_addr, dst_addr;
    BYTE new_psw = GetLPSW() & 0xF0;
    WORD src, src2, dst;
    
    if (m_methsrc)
    {
        src_addr = GetWordAddr(m_methsrc,m_regsrc);
        if (m_RPLYrq) return;
        src = GetWord(src_addr);
        if (m_RPLYrq) return;
    }
    else
     src = GetReg(m_regsrc);

    if (m_methdest)
    {
        dst_addr = GetWordAddr(m_methdest,m_regdest);
        if (m_RPLYrq) return;
        src2 = GetWord(dst_addr);
        if (m_RPLYrq) return;
    }
    else
        src2 = GetReg(m_regdest);

    dst = src2 - src;

    if(m_methdest)
        SetWord(dst_addr, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if (((src ^ src2) & ~(dst ^ src)) & 0100000) new_psw |= PSW_V;
    if (((src & ~src2) | (~(src ^ src2) & dst)) & 0100000) new_psw |= PSW_C;
    SetLPSW(new_psw);
    m_internalTick=MOVB_TIMING[m_methsrc][m_methdest];
}

void CProcessor::ExecuteEMT ()  // EMT - emulator trap
{
    m_EMT_rq = TRUE;
    m_internalTick=EMT_TIMING;
}

void CProcessor::ExecuteTRAP ()
{
    m_TRAPrq = TRUE;
    m_internalTick=EMT_TIMING;
}

void CProcessor::ExecuteJSR ()  // JSR - Jump subroutine: *--SP = R; R = PC; PC = &d (a-mode > 0)
{
    if (m_methdest == 0) 
    {  // Неправильный метод адресации
        m_ILLGrq = TRUE;
        m_internalTick=EMT_TIMING;
    }
    else 
    {
        WORD dst;
        dst= GetWordAddr(m_methdest,m_regdest);
        if (m_RPLYrq) return;

        SetSP( GetSP() - 2 );
        SetWord( GetSP(), GetReg(m_regsrc) );
        SetReg(m_regsrc, GetPC());
        SetPC(dst);
        if (m_RPLYrq) return;

        m_internalTick=JSR_TIMING[m_methdest-1];
    }
}

void CProcessor::ExecuteMARK ()  // MARK
{
    SetSP( GetPC() + (m_instruction & 0x003F) * 2 );
    SetPC( GetReg(5) );
    SetReg(5, GetWord( GetSP() ));
    SetSP( GetSP() + 2 );
    if (m_RPLYrq) return;

    m_internalTick = MARK_TIMING;
}

//////////////////////////////////////////////////////////////////////
//
// CPU image format (32 bytes):
//   2   bytes      PSW
//   2*8 bytes      Registers R0..R7
//   2*2 bytes      Saved PC and PSW
//   2   byte       Stopped flag: 1 - stopped, 0 - not stopped
//   2   bytes      Internal tick count
//   1*6 bytes      Step mode, Bus error, HALT, DCLO, ACLO, WAIT

void CProcessor::SaveToImage(BYTE* pImage) const
{
    WORD* pwImage = (WORD*) pImage;
    *pwImage++ = m_psw;  // PSW
    memcpy(pwImage, m_R, 2 * 8);  // Registers R0..R7
    pwImage += 2 * 8;
    *pwImage++ = m_savepc;
    *pwImage++ = m_savepsw;
    *pwImage++ = (m_okStopped ? 1 : 0);
    *pwImage++ = m_internalTick;
    BYTE* pbImage = (BYTE*) pwImage;
    *pbImage++ = (m_stepmode  ? 1 : 0);
    *pbImage++ = (m_buserror  ? 1 : 0);
    *pbImage++ = (m_haltpin   ? 1 : 0);
    *pbImage++ = (m_DCLOpin   ? 1 : 0);
    *pbImage++ = (m_ACLOpin   ? 1 : 0);
    *pbImage++ = (m_waitmode  ? 1 : 0);
}

void CProcessor::LoadFromImage(const BYTE* pImage)
{
    const WORD* pwImage = (const WORD*) pImage;
    m_psw = *pwImage++;  // PSW
    memcpy(m_R, pwImage, 2 * 8);  // Registers R0..R7
    m_savepc    = *pwImage++;
    m_savepsw   = *pwImage++;
    m_okStopped = (*pwImage++ != 0);
    m_internalTick = *pwImage++;
    const BYTE* pbImage = (const BYTE*) pwImage;
    m_stepmode  = (*pbImage++ != 0);
    m_buserror  = (*pbImage++ != 0);
    m_haltpin   = (*pbImage++ != 0);
    m_DCLOpin   = (*pbImage++ != 0);
    m_ACLOpin   = (*pbImage++ != 0);
    m_waitmode  = (*pbImage++ != 0);
}

WORD CProcessor::GetWordAddr (BYTE meth, BYTE reg)
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
            if (!m_RPLYrq) addr=GetWord(addr);
            break;
    }
    return addr;
}

WORD CProcessor::GetByteAddr (BYTE meth, BYTE reg)
{
    WORD addr;

    addr=0;
    switch(meth)
    {
        case 1:
            addr=GetReg(reg);
        break;
        case 2:
            addr=GetReg(reg);
            SetReg(reg,addr+(reg<6?1:2));
            break;
        case 3:
            addr=GetReg(reg);
            SetReg(reg,addr+2);
            addr=GetWord(addr);
            break;
        case 4:
            SetReg(reg,GetReg(reg)-(reg<6?1:2));
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
            if (!m_RPLYrq) addr=GetWord(addr);
            break;
    }

    return addr;
}


//////////////////////////////////////////////////////////////////////
