/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

/// \file Processor.cpp  KM1801VM2 processor class implementation

#include "stdafx.h"
#include "Processor.h"
#include "Emubase.h"


// Timings ///////////////////////////////////////////////////////////

uint16_t MOV_TIMING[8][8] =
{
    {0x000B, 0x0022, 0x0022, 0x0033, 0x0022, 0x0037, 0x0033, 0x0043},
    {0x0020, 0x0031, 0x0037, 0x0043, 0x003B, 0x0047, 0x0043, 0x0054},
    {0x0020, 0x0037, 0x0037, 0x0043, 0x003B, 0x0047, 0x0043, 0x0053},
    {0x0025, 0x0043, 0x0043, 0x004F, 0x0047, 0x0054, 0x004F, 0x0060},
    {0x0020, 0x0037, 0x0037, 0x0043, 0x003B, 0x0047, 0x0043, 0x0053},
    {0x0025, 0x0043, 0x0043, 0x004F, 0x0047, 0x0054, 0x004F, 0x0060},
    {0x0029, 0x0039, 0x003F, 0x004C, 0x003F, 0x004C, 0x004B, 0x005C},
    {0x0035, 0x0045, 0x004C, 0x0057, 0x004C, 0x0057, 0x0057, 0x0068}
};

uint16_t MOVB_TIMING[8][8] =
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

uint16_t CMP_TIMING[8][8] =
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

uint16_t CLR_TIMING[8] =
{
    0x000B, 0x001C, 0x0023, 0x002F, 0x0023, 0x002F, 0x002F, 0x003F
};

uint16_t CLRB_TIMING[8] =
{
    0x000B, 0x0021, 0x0027, 0x0033, 0x0027, 0x0033, 0x0037, 0x0043
};

uint16_t TST_TIMING[8] =
{
    0x000B, 0x0018, 0x0019, 0x0025, 0x0019, 0x0025, 0x0031, 0x003D
};

uint16_t MTPS_TIMING[8] =
{
    0x0018, 0x0029, 0x0029, 0x0035, 0x0029, 0x0035, 0x0041, 0x004D
};

uint16_t XOR_TIMING[8] =
{
    0x000B, 0x0025, 0x002B, 0x0037, 0x002F, 0x003B, 0x003B, 0x0047
};

uint16_t ASH_TIMING[8] =
{
    0x0029, 0x003D, 0x003D, 0x0049, 0x0041, 0x004D, 0x0055, 0x0062
};
uint16_t ASH_S_TIMING = 0x0008;

uint16_t ASHC_TIMING[8] =
{
    0x0039, 0x004E, 0x004D, 0x005A, 0x0051, 0x005D, 0x0066, 0x0072
};
uint16_t ASHC_S_TIMING = 0x0008;

uint16_t MUL_TIMING[8] =
{
    0x0060, 0x00C7, 0x00C7, 0x00D4, 0x00CA, 0x00D8, 0x00E1, 0x00EC
};

uint16_t DIV_TIMING[8] =
{
    0x0080, 0x00E8, 0x00E7, 0x00F4, 0x00EB, 0x00F8, 0x0100, 0x010D
};

uint16_t JMP_TIMING[7] =
{
    0x002D, 0x002D, 0x003D, 0x002D, 0x003D, 0x0031, 0x0041
};
uint16_t JSR_TIMING[7] =
{
    0x0045, 0x0045, 0x0056, 0x0045, 0x0056, 0x0049, 0x0059
};

uint16_t BRANCH_TRUE_TIMING = 0x0025;
uint16_t BRANCH_FALSE_TIMING = 0x0010;
uint16_t BPT_TIMING = 0x0094;
uint16_t EMT_TIMING = 0x009C;
uint16_t RTI_TIMING = 0x0059;
uint16_t RTS_TIMING = 0x0031;
uint16_t NOP_TIMING = 0x0010;
uint16_t SOB_TIMING = 0x002D;
uint16_t SOB_LAST_TIMING = 0x0019; //last iteration of SOB
uint16_t BR_TIMING = 0x0025;
uint16_t MARK_TIMING = 0x0041;
uint16_t RESET_TIMING = 105 + 968;  // ТО КМ1801ВМ2 стр. 134


//////////////////////////////////////////////////////////////////////


CProcessor::ExecuteMethodRef* CProcessor::m_pExecuteMethodMap = nullptr;

#define RegisterMethodOpc(/*uint16_t*/ opcode, /*CProcessor::ExecuteMethodRef*/ methodref) \
    { m_pExecuteMethodMap[opcode] = (methodref); }

#define RegisterMethodRef(/*uint16_t*/ opstart, /*uint16_t*/ opend, /*CProcessor::ExecuteMethodRef*/ methodref) \
    { \
        for (uint32_t opcode = (opstart); opcode <= (opend); opcode++) \
            m_pExecuteMethodMap[opcode] = (methodref); \
    }

void CProcessor::Init()
{
    ASSERT(m_pExecuteMethodMap == nullptr);
    m_pExecuteMethodMap = static_cast<CProcessor::ExecuteMethodRef*>(::calloc(65536, sizeof(CProcessor::ExecuteMethodRef)));

    // Сначала заполняем таблицу ссылками на метод ExecuteUNKNOWN, выполняющий TRAP 10
    RegisterMethodRef( 0000000, 0177777, &CProcessor::ExecuteUNKNOWN )

    RegisterMethodOpc( 0000000, &CProcessor::ExecuteHALT )
    RegisterMethodOpc( 0000001, &CProcessor::ExecuteWAIT )
    RegisterMethodOpc( 0000002, &CProcessor::ExecuteRTI )
    RegisterMethodOpc( 0000003, &CProcessor::ExecuteBPT )
    RegisterMethodOpc( 0000004, &CProcessor::ExecuteIOT )
    RegisterMethodOpc( 0000005, &CProcessor::ExecuteRESET )
    RegisterMethodOpc( 0000006, &CProcessor::ExecuteRTT )

    RegisterMethodRef( 0000010, 0000013, &CProcessor::ExecuteRUN )
    RegisterMethodRef( 0000014, 0000017, &CProcessor::ExecuteSTEP )
    RegisterMethodOpc( 0000020,          &CProcessor::ExecuteRSEL)
    RegisterMethodOpc( 0000021,          &CProcessor::ExecuteMFUS)
    RegisterMethodRef( 0000022, 0000023, &CProcessor::ExecuteRCPC)
    RegisterMethodRef( 0000024, 0000027, &CProcessor::ExecuteRCPS)
    RegisterMethodOpc( 0000030,          &CProcessor::Execute000030)
    RegisterMethodOpc( 0000031,          &CProcessor::ExecuteMTUS)
    RegisterMethodRef( 0000032, 0000033, &CProcessor::ExecuteWCPC)
    RegisterMethodRef( 0000034, 0000037, &CProcessor::ExecuteWCPS)

    RegisterMethodRef( 0000100, 0000177, &CProcessor::ExecuteJMP)
    RegisterMethodRef( 0000200, 0000207, &CProcessor::ExecuteRTS)  // RTS / RETURN

    RegisterMethodRef( 0000240, 0000257, &CProcessor::ExecuteCCC)

    RegisterMethodRef( 0000260, 0000277, &CProcessor::ExecuteSCC)

    RegisterMethodRef( 0000300, 0000377, &CProcessor::ExecuteSWAB)

    RegisterMethodRef( 0000400, 0000777, &CProcessor::ExecuteBR)
    RegisterMethodRef( 0001000, 0001377, &CProcessor::ExecuteBNE)
    RegisterMethodRef( 0001400, 0001777, &CProcessor::ExecuteBEQ)
    RegisterMethodRef( 0002000, 0002377, &CProcessor::ExecuteBGE)
    RegisterMethodRef( 0002400, 0002777, &CProcessor::ExecuteBLT)
    RegisterMethodRef( 0003000, 0003377, &CProcessor::ExecuteBGT)
    RegisterMethodRef( 0003400, 0003777, &CProcessor::ExecuteBLE)

    RegisterMethodRef( 0004000, 0004777, &CProcessor::ExecuteJSR)  // JSR / CALL

    RegisterMethodRef( 0005000, 0005077, &CProcessor::ExecuteCLR)
    RegisterMethodRef( 0005100, 0005177, &CProcessor::ExecuteCOM)
    RegisterMethodRef( 0005200, 0005277, &CProcessor::ExecuteINC)
    RegisterMethodRef( 0005300, 0005377, &CProcessor::ExecuteDEC)
    RegisterMethodRef( 0005400, 0005477, &CProcessor::ExecuteNEG)
    RegisterMethodRef( 0005500, 0005577, &CProcessor::ExecuteADC)
    RegisterMethodRef( 0005600, 0005677, &CProcessor::ExecuteSBC)
    RegisterMethodRef( 0005700, 0005777, &CProcessor::ExecuteTST)
    RegisterMethodRef( 0006000, 0006077, &CProcessor::ExecuteROR)
    RegisterMethodRef( 0006100, 0006177, &CProcessor::ExecuteROL)
    RegisterMethodRef( 0006200, 0006277, &CProcessor::ExecuteASR)
    RegisterMethodRef( 0006300, 0006377, &CProcessor::ExecuteASL)

    RegisterMethodRef( 0006400, 0006477, &CProcessor::ExecuteMARK)
    RegisterMethodRef( 0006700, 0006777, &CProcessor::ExecuteSXT)

    RegisterMethodRef( 0010000, 0017777, &CProcessor::ExecuteMOV)
    RegisterMethodRef( 0020000, 0027777, &CProcessor::ExecuteCMP)
    RegisterMethodRef( 0030000, 0037777, &CProcessor::ExecuteBIT)
    RegisterMethodRef( 0040000, 0047777, &CProcessor::ExecuteBIC)
    RegisterMethodRef( 0050000, 0057777, &CProcessor::ExecuteBIS)
    RegisterMethodRef( 0060000, 0067777, &CProcessor::ExecuteADD)

    RegisterMethodRef( 0070000, 0070777, &CProcessor::ExecuteMUL)
    RegisterMethodRef( 0071000, 0071777, &CProcessor::ExecuteDIV)
    RegisterMethodRef( 0072000, 0072777, &CProcessor::ExecuteASH)
    RegisterMethodRef( 0073000, 0073777, &CProcessor::ExecuteASHC)
    RegisterMethodRef( 0074000, 0074777, &CProcessor::ExecuteXOR)
    RegisterMethodRef( 0075000, 0075037, &CProcessor::ExecuteFIS)
    RegisterMethodRef( 0077000, 0077777, &CProcessor::ExecuteSOB)

    RegisterMethodRef( 0100000, 0100377, &CProcessor::ExecuteBPL)
    RegisterMethodRef( 0100400, 0100777, &CProcessor::ExecuteBMI)
    RegisterMethodRef( 0101000, 0101377, &CProcessor::ExecuteBHI)
    RegisterMethodRef( 0101400, 0101777, &CProcessor::ExecuteBLOS)
    RegisterMethodRef( 0102000, 0102377, &CProcessor::ExecuteBVC)
    RegisterMethodRef( 0102400, 0102777, &CProcessor::ExecuteBVS)
    RegisterMethodRef( 0103000, 0103377, &CProcessor::ExecuteBHIS)  // BCC
    RegisterMethodRef( 0103400, 0103777, &CProcessor::ExecuteBLO)   // BCS

    RegisterMethodRef( 0104000, 0104377, &CProcessor::ExecuteEMT)
    RegisterMethodRef( 0104400, 0104777, &CProcessor::ExecuteTRAP)

    RegisterMethodRef( 0105000, 0105077, &CProcessor::ExecuteCLRB)
    RegisterMethodRef( 0105100, 0105177, &CProcessor::ExecuteCOMB)
    RegisterMethodRef( 0105200, 0105277, &CProcessor::ExecuteINCB)
    RegisterMethodRef( 0105300, 0105377, &CProcessor::ExecuteDECB)
    RegisterMethodRef( 0105400, 0105477, &CProcessor::ExecuteNEGB)
    RegisterMethodRef( 0105500, 0105577, &CProcessor::ExecuteADCB)
    RegisterMethodRef( 0105600, 0105677, &CProcessor::ExecuteSBCB)
    RegisterMethodRef( 0105700, 0105777, &CProcessor::ExecuteTSTB)
    RegisterMethodRef( 0106000, 0106077, &CProcessor::ExecuteRORB)
    RegisterMethodRef( 0106100, 0106177, &CProcessor::ExecuteROLB)
    RegisterMethodRef( 0106200, 0106277, &CProcessor::ExecuteASRB)
    RegisterMethodRef( 0106300, 0106377, &CProcessor::ExecuteASLB)

    RegisterMethodRef( 0106400, 0106477, &CProcessor::ExecuteMTPS)
    RegisterMethodRef( 0106700, 0106777, &CProcessor::ExecuteMFPS)

    RegisterMethodRef( 0110000, 0117777, &CProcessor::ExecuteMOVB)
    RegisterMethodRef( 0120000, 0127777, &CProcessor::ExecuteCMPB)
    RegisterMethodRef( 0130000, 0137777, &CProcessor::ExecuteBITB)
    RegisterMethodRef( 0140000, 0147777, &CProcessor::ExecuteBICB)
    RegisterMethodRef( 0150000, 0157777, &CProcessor::ExecuteBISB)
    RegisterMethodRef( 0160000, 0167777, &CProcessor::ExecuteSUB)
}

void CProcessor::Done()
{
    ::free(m_pExecuteMethodMap);  m_pExecuteMethodMap = nullptr;
}

//////////////////////////////////////////////////////////////////////


CProcessor::CProcessor (LPCTSTR name)
{
    _tcscpy(m_name, name);
    memset(m_R, 0, sizeof(m_R));
    m_psw = m_savepsw = 0777;
    m_savepc = 0177777;
    m_okStopped = true;
    m_internalTick = 0;
    m_pMemoryController = nullptr;
    m_okTrace = false;
    m_waitmode = false;
    m_stepmode = false;
    m_buserror = false;
    m_STRTrq = m_RPLYrq = m_RSVDrq = m_TBITrq = m_ACLOrq = m_HALTrq = m_EVNTrq = false;
    m_ILLGrq = m_FIS_rq = m_BPT_rq = m_IOT_rq = m_EMT_rq = m_TRAPrq = false;
    m_ACLOreset = m_EVNTreset = false; m_VIRQreset = 0;
    m_DCLOpin = m_ACLOpin = true;
    m_haltpin = false;

    m_instruction = m_instructionpc = 0;
    m_regsrc = m_methsrc = 0;
    m_regdest = m_methdest = 0;
    m_addrsrc = m_addrdest = 0;
    memset(m_virq, 0, sizeof(m_virq));
}

void CProcessor::Execute()
{
    if (m_okStopped) return;  // Processor is stopped - nothing to do

    if (m_internalTick > 0)
    {
        m_internalTick--;
        return;
    }

    if (!InterruptProcessing())
        CommandExecution();
}

bool CProcessor::InterruptProcessing ()
{
    uint16_t intrVector = 0xFFFF;
    bool currMode = ((m_psw & 0400) != 0);  // Current processor mode: true = HALT mode, false = USER mode
    bool intrMode = false;  // true = HALT mode interrupt, false = USER mode interrupt

    if (m_stepmode)
        m_stepmode = false;
    else
    {
        m_ACLOreset = m_EVNTreset = false; m_VIRQreset = 0;
        m_TBITrq = (m_psw & 020) != 0;  // T-bit

        if (m_STRTrq)
        {
            intrVector = 0; intrMode = true;
            m_STRTrq = false;
        }
        else if (m_HALTrq)  // HALT command
        {
            intrVector = 0170;  intrMode = true;
            m_HALTrq = false;
        }
        else if (m_BPT_rq)  // BPT command
        {
            intrVector = 0000014;  intrMode = false;
            m_BPT_rq = false;
        }
        else if (m_IOT_rq)  // IOT command
        {
            intrVector = 0000020;  intrMode = false;
            m_IOT_rq = false;
        }
        else if (m_EMT_rq)  // EMT command
        {
            intrVector = 0000030;  intrMode = false;
            m_EMT_rq = false;
        }
        else if (m_TRAPrq)  // TRAP command
        {
            intrVector = 0000034;  intrMode = false;
            m_TRAPrq = false;
        }
        else if (m_FIS_rq)  // FIS commands -- Floating point Instruction Set
        {
            intrVector = 0010;  intrMode = true;
            m_FIS_rq = false;
        }
        else if (m_RPLYrq)  // Зависание, priority 1
        {
            if (m_buserror)
            {
                intrVector = 0174; intrMode = true;
            }
            else if (currMode)
            {
                intrVector = 0004;  intrMode = true;
            }
            else
            {
                intrVector = 0000004; intrMode = false;
            }
            m_buserror = true;
            m_RPLYrq = false;
        }
        else if (m_ILLGrq)
        {
            intrVector = 000004;  intrMode = false;
            m_ILLGrq = false;
        }
        else if (m_RSVDrq)  // Reserved command, priority 2
        {
            intrVector = 000010;  intrMode = false;
            m_RSVDrq = false;
        }
        else if (m_TBITrq && (!m_waitmode))  // T-bit, priority 3
        {
            intrVector = 000014;  intrMode = false;
            m_TBITrq = false;
        }
        else if (m_ACLOrq && (m_psw & 0600) != 0600)  // ACLO, priority 4
        {
            intrVector = 000024;  intrMode = false;
            m_ACLOreset = true;
        }
        else if (m_haltpin && (m_psw & 0400) != 0400)  // HALT signal in USER mode, priority 5
        {
            intrVector = 0170;  intrMode = true;
        }
        else if (m_EVNTrq && (m_psw & 0200) != 0200)  // EVNT signal, priority 6
        {
            intrVector = 0000100;  intrMode = false;
            m_EVNTreset = true;
        }
        else if ((m_psw & 0200) != 0200)  // VIRQ, priority 7
        {
            intrMode = false;
            for (uint8_t irq = 1; irq <= 15; irq++)
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

            m_waitmode = false;

            if (intrMode)  // HALT mode interrupt
            {
                uint16_t selVector = GetMemoryController()->GetSelRegister() & 0x0ff00;
                uint16_t new_pc, new_psw;
                intrVector |= selVector;
                // Save PC/PSW to CPC/CPSW
                //m_savepc = GetPC();
                //m_savepsw = GetPSW();
                //m_psw |= 0400;
                SetHALT(true);
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
                uint16_t new_pc, new_psw;

                SetHALT(false);
                // Save PC/PSW to stack
                SetSP(GetSP() - 2);
                SetWord(GetSP(), GetCPSW());
                SetSP(GetSP() - 2);
                if (!m_RPLYrq)
                {
                    SetWord(GetSP(), GetCPC());
                    if (!m_RPLYrq)
                    {
                        if (m_ACLOreset) m_ACLOrq = false;
                        if (m_EVNTreset) m_EVNTrq = false;
                        if (m_VIRQreset) m_virq[m_VIRQreset] = 0;
                        new_pc = GetWord(intrVector);
                        new_psw = GetWord(intrVector + 2);
                        if (!m_RPLYrq)
                        {
                            SetLPSW((uint8_t)(new_psw & 0xff));
                            SetPC(new_pc);
                        }
                    }
                }
            }

            return true;
        }
    }
    return false;
}

void CProcessor::CommandExecution()
{
    if (!m_waitmode)
    {
        m_instructionpc = m_R[7];  // Store address of the current instruction
        FetchInstruction();  // Read next instruction from memory
        if (!m_RPLYrq)
        {
            m_buserror = false;
            TranslateInstruction();  // Execute next instruction
        }
    }
    if (m_HALTrq || m_BPT_rq || m_IOT_rq || m_EMT_rq || m_TRAPrq || m_FIS_rq)
        InterruptProcessing();
}

void CProcessor::TickEVNT()
{
    if (m_okStopped) return;  // Processor is stopped - nothing to do

    m_EVNTrq = true;
}

void CProcessor::SetDCLOPin(bool value)
{
    m_DCLOpin = value;
    if (m_DCLOpin)
    {
        m_okStopped = true;

        m_stepmode = false;
        m_buserror = false;
        m_waitmode = false;
        m_internalTick = 0;
        m_RPLYrq = m_RSVDrq = m_TBITrq = m_ACLOrq = m_HALTrq = m_EVNTrq = false;
        m_ILLGrq = m_FIS_rq = m_BPT_rq = m_IOT_rq = m_EMT_rq = m_TRAPrq = false;
        memset(m_virq, 0, sizeof(m_virq));
        m_ACLOreset = m_EVNTreset = false; m_VIRQreset = 0;
        m_pMemoryController->DCLO_Signal();
        m_pMemoryController->ResetDevices();
    }
}

void CProcessor::SetACLOPin(bool value)
{
    if (m_okStopped && !m_DCLOpin && m_ACLOpin && !value)
    {
        m_okStopped = false;
        m_internalTick = 0;

        m_stepmode = false;
        m_waitmode = false;
        m_buserror = false;
        m_RPLYrq = m_RSVDrq = m_TBITrq = m_ACLOrq = m_HALTrq = m_EVNTrq = false;
        m_ILLGrq = m_FIS_rq = m_BPT_rq = m_IOT_rq = m_EMT_rq = m_TRAPrq = false;
        memset(m_virq, 0, sizeof(m_virq));
        m_ACLOreset = m_EVNTreset = false; m_VIRQreset = 0;

        // "Turn On" interrupt processing
        m_STRTrq = true;
    }
    if (!m_okStopped && !m_DCLOpin && !m_ACLOpin && value)
    {
        m_ACLOrq = true;
    }
    m_ACLOpin = value;
}

void CProcessor::MemoryError()
{
    m_RPLYrq = true;
}


//////////////////////////////////////////////////////////////////////

#if defined(PRODUCT)
static void TraceInstruction(CProcessor* /*pProc*/, uint16_t /*address*/) {}
#else
static void TraceInstruction(CProcessor* pProc, uint16_t address)
{
    CMemoryController* pMemCtl = pProc->GetMemoryController();
    bool okHaltMode = pProc->IsHaltMode();
    uint16_t memory[4];
    int addrtype = 0;
    memory[0] = pMemCtl->GetWordView(address + 0 * 2, okHaltMode, true, &addrtype);
    memory[1] = pMemCtl->GetWordView(address + 1 * 2, okHaltMode, true, &addrtype);
    memory[2] = pMemCtl->GetWordView(address + 2 * 2, okHaltMode, true, &addrtype);
    memory[3] = pMemCtl->GetWordView(address + 3 * 2, okHaltMode, true, &addrtype);

    TCHAR bufaddr[7];
    PrintOctalValue(bufaddr, address);

    TCHAR instr[8];
    TCHAR args[32];
    DisassembleInstruction(memory, address, instr, args);
    TCHAR buffer[64];
    _sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR) - 1, _T("%s\t%s\t%s\t%s\r\n"), pProc->GetName(), bufaddr, instr, args);

    DebugLog(buffer);
}
#endif

void CProcessor::FetchInstruction()
{
    // Считываем очередную инструкцию
    uint16_t pc = GetPC();
    ASSERT((pc & 1) == 0); // it have to be word aligned

    m_instruction = GetWordExec(pc);
    SetPC(GetPC() + 2);
}

void CProcessor::TranslateInstruction()
{
    if (m_okTrace)
        TraceInstruction(this, m_instructionpc);

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
//    DebugPrintFormat(_T(">>Invalid OPCODE = %06o @ %06o\r\n"), m_instruction, GetPC()-2);

    m_RSVDrq = true;
}


// Instruction execution /////////////////////////////////////////////

void CProcessor::ExecuteWAIT ()  // WAIT - Wait for an interrupt
{
    m_waitmode = true;
}

void CProcessor::ExecuteSTEP()  // ШАГ
{
    if ((m_psw & PSW_HALT) == 0)  // Эта команда выполняется только в режиме HALT
        m_RSVDrq = true;
    else
    {
        SetPC(m_savepc);        // СК <- КРСК
        SetPSW(m_savepsw);      // РСП(8:0) <- КРСП(8:0)
        m_stepmode = true;
    }
}

void CProcessor::ExecuteRSEL()  // RSEL / ЧПТ - Чтение безадресного регистра
{
    if ((m_psw & PSW_HALT) == 0)  // Эта команда выполняется только в режиме HALT
        m_RSVDrq = true;
    else
    {
        SetReg(0, GetMemoryController()->GetSelRegister());  // R0 <- (SEL)
    }
}

void CProcessor::Execute000030()  // Unknown command
{
    if ((m_psw & PSW_HALT) == 0)  // Эта команда выполняется только в режиме HALT
    {
        m_RSVDrq = true;
        return;
    }

    // Описание: По этой команде сперва очищается регистр R0. Далее исполняется цикл, окончанием которого
    //           является установка в разряде 07 R0 или R2 единицы. В цикле над регистрами проводятся
    //           следующие действия: регистры с R1 по R3 сдвигаются влево, при этом в R1 в младший разряд
    //           вдвигается ноль, а в R2 и R3 – содержимое разряда C, при этом старшая часть R2 расширяется
    //           знаковым разрядом младшей части, R0 инкрементируется. Так как останов исполнения команды
    //           производится при наличии единицы в разряде 7 в R0 или R2, то после исполнения команды R0
    //           может принимать значения от 0 до 108 или 2008. Значение 2008 получается в том случае,
    //           если до исполнения операции младшая часть R2 была равна нулю и был сброшен бит С.
    // Признаки: N – очищается,
    //           Z – устанавливается, если значение в R0 равно нулю, в противном случае очищается,
    //           V – очищается,
    //           C – очищается.

    SetReg(0, 0);
    while ((GetReg(0) & 0200) == 0 && (GetReg(2) & 0200) == 0)
    {
        SetReg(1, GetReg(1) << 1);
        SetReg(2, (((uint16_t)GetLReg(2)) << 1) | (GetC() ? 1 : 0));
        SetReg(2, ((GetReg(2) & 0200) ? 0xff00 : 0) | GetLReg(2));
        SetReg(3, (GetReg(3) << 1) | (GetC() ? 1 : 0));
        SetReg(0, GetReg(0) + 1);
    }
    SetN(0);
    SetZ(GetReg(0) == 0);
    SetV(0);
    SetC(0);
}

void CProcessor::ExecuteFIS()  // Floating point instruction set: FADD, FSUB, FMUL, FDIV
{
    if (GetMemoryController()->GetSelRegister() & 0200)  // bit 7 set?
        m_RSVDrq = true;  // Программа эмуляции FIS отсутствует, прерывание по резервному коду
    else
        m_FIS_rq = true;  // Прерывание обработки FIS
}

void CProcessor::ExecuteRUN()  // ПУСК / START
{
    if ((m_psw & PSW_HALT) == 0)  // Эта команда выполняется только в режиме HALT
        m_RSVDrq = true;
    else
    {
        SetPC(m_savepc);        // СК <- КРСК
        SetPSW(m_savepsw);      // РСП(8:0) <- КРСП(8:0)
    }
}

void CProcessor::ExecuteHALT ()  // HALT - Останов
{
    m_HALTrq = true;
}

void CProcessor::ExecuteRCPC()  // ЧКСК - Чтение регистра копии счётчика команд
{
    if ((m_psw & PSW_HALT) == 0)  // Эта команда выполняется только в режиме HALT
        m_RSVDrq = true;
    else
    {
        SetReg(0, m_savepc);        // R0 <- КРСК
        m_internalTick = NOP_TIMING;
    }
}
void CProcessor::ExecuteRCPS()  // ЧКСП - Чтение регистра копии слова состояния процессора
{
    if ((m_psw & PSW_HALT) == 0)  // Эта команда выполняется только в режиме HALT
        m_RSVDrq = true;
    else
    {
        SetReg(0, m_savepsw);       // R0 <- КРСП
        m_internalTick = NOP_TIMING;
    }
}
void CProcessor::ExecuteWCPC()  // ЗКСК - Запись регистра копии счётчика команд
{
    if ((m_psw & PSW_HALT) == 0)  // Эта команда выполняется только в режиме HALT
        m_RSVDrq = true;
    else
    {
        m_savepc = GetReg(0);       // КРСК <- R0
        m_internalTick = NOP_TIMING;
    }
}
void CProcessor::ExecuteWCPS()  // ЗКСП - Запись регистра копии слова состояния процессора
{
    if ((m_psw & PSW_HALT) == 0)  // Эта команда выполняется только в режиме HALT
        m_RSVDrq = true;
    else
    {
        m_savepsw = GetReg(0);      // КРСП <- R0
        m_internalTick = NOP_TIMING;
    }
}

void CProcessor::ExecuteMFUS ()  // ЧЧП, move from user space - Чтение памяти адресного пространства USER
{
    if ((m_psw & PSW_HALT) == 0)  // Эта команда выполняется только в режиме HALT
    {
        m_RSVDrq = true;
        return;
    }

    //r0 = (r5)+
    SetHALT(false);
    uint16_t addr = GetReg(5);
    uint16_t word = GetWord(addr);  // Read in USER mode
    SetHALT(true);
    SetReg(5, addr + 2);
    if (!m_RPLYrq)  SetReg(0, word);

    m_internalTick = MOV_TIMING[0][2];
}

void CProcessor::ExecuteMTUS()  // ЗЧП, move to user space - Запись в память адресного пространства USER
{
    if ((m_psw & PSW_HALT) == 0)  // Эта команда выполняется только в режиме HALT
    {
        m_RSVDrq = true;
        return;
    }

    // -(r5) = r0
    SetReg(5, GetReg(5) - 2);
    SetHALT(false);
    SetWord(GetReg(5), GetReg(0));  // Write in USER mode
    SetHALT(true);

    m_internalTick = MOV_TIMING[0][2];
}

void CProcessor::ExecuteRTI()  // RTI - Return from Interrupt - Возврат из прерывания
{
    uint16_t word;
    word = GetWord(GetSP());
    SetSP( GetSP() + 2 );
    if (m_RPLYrq) return;
    SetPC(word);  // Pop PC
    word = GetWord ( GetSP() );  // Pop PSW --- saving HALT
    SetSP( GetSP() + 2 );
    if (m_RPLYrq) return;
    if (GetPC() < 0160000)
        SetLPSW((uint8_t)(word & 0xff));
    else
        SetPSW(word); //load new mode
    m_internalTick = RTI_TIMING;
}

void CProcessor::ExecuteRTT ()  // RTT - Return from Trace Trap -- Возврат из прерывания
{
    uint16_t word;
    word = GetWord(GetSP());
    SetSP( GetSP() + 2 );
    if (m_RPLYrq) return;
    SetPC(word);  // Pop PC
    word = GetWord ( GetSP() );  // Pop PSW --- saving HALT
    SetSP( GetSP() + 2 );
    if (m_RPLYrq) return;
    if (GetPC() < 0160000)
        SetLPSW((uint8_t)(word & 0xff));
    else
        SetPSW(word); //load new mode

    m_stepmode = (word & PSW_T) ? true : false;

    m_internalTick = RTI_TIMING;
}

void CProcessor::ExecuteBPT ()  // BPT - Breakpoint
{
    m_BPT_rq = true;
    m_internalTick = BPT_TIMING;
}

void CProcessor::ExecuteIOT ()  // IOT - I/O trap
{
    m_IOT_rq = true;
    m_internalTick = EMT_TIMING;
}

void CProcessor::ExecuteRESET ()  // Reset input/output devices -- Сброс внешних устройств
{
    m_EVNTrq = false;
    m_pMemoryController->ResetDevices();  // INIT signal

    m_internalTick = RESET_TIMING;
}

void CProcessor::ExecuteRTS ()  // RTS - return from subroutine - Возврат из процедуры
{
    uint16_t word;
    SetPC(GetReg(m_regdest));
    word = GetWord(GetSP());
    SetSP(GetSP() + 2);
    if (m_RPLYrq) return;
    SetReg(m_regdest, word);
    m_internalTick = RTS_TIMING;
}

void CProcessor::ExecuteCCC ()
{
    SetLPSW(GetLPSW() &  ~((uint8_t)(m_instruction & 0xff) & 017));
    m_internalTick = NOP_TIMING;
}
void CProcessor::ExecuteSCC ()
{
    SetLPSW(GetLPSW() |  ((uint8_t)(m_instruction & 0xff) & 017));
    m_internalTick = NOP_TIMING;
}

void CProcessor::ExecuteJMP ()  // JMP - jump: PC = &d (a-mode > 0)
{
    if (m_methdest == 0)  // Неправильный метод адресации
    {
        m_ILLGrq = true;
        m_internalTick = EMT_TIMING;
    }
    else
    {
        uint16_t word;
        word = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        SetPC(word);
        m_internalTick = JMP_TIMING[m_methdest - 1];
    }
}

void CProcessor::ExecuteSWAB ()
{
    uint16_t ea = 0;
    uint16_t dst;
    uint8_t new_psw = GetLPSW() & 0xF0;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regdest);

    dst = ((dst >> 8) & 0377) | (dst << 8);

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);

    if (m_RPLYrq) return;

    if ((dst & 0200) != 0) new_psw |= PSW_N;
    if ((uint8_t)(dst & 0xff) == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
    m_internalTick = MOV_TIMING[m_methdest][m_methdest];
}

void CProcessor::ExecuteCLR ()  // CLR
{
    uint16_t dst_addr;

    if (m_methdest)
    {
        dst_addr = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        SetWord(dst_addr, 0);
        if (m_RPLYrq) return;
    }
    else
        SetReg(m_regdest, 0);

    SetLPSW((GetLPSW() & 0xF0) | PSW_Z);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteCLRB ()  // CLRB
{
    uint16_t dst_addr;

    if (m_methdest)
    {
        dst_addr = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        GetByte(dst_addr);
        if (m_RPLYrq) return;
        SetByte(dst_addr, 0);
        if (m_RPLYrq) return;
    }
    else
        SetLReg(m_regdest, 0);

    SetLPSW((GetLPSW() & 0xF0) | PSW_Z);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteCOM()  // COM
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint16_t dst;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regdest);

    dst = ~dst;

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    new_psw |= PSW_C;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}
void CProcessor::ExecuteCOMB()  // COM
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint8_t dst;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetLReg(m_regdest);

    dst = ~dst;

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetLReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0200) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    new_psw |= PSW_C;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteINC()  // INC - Инкремент
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF1;
    uint16_t dst;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regdest);

    dst = dst + 1;

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if (dst == 0100000) new_psw |= PSW_V;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}
void CProcessor::ExecuteINCB()  // INCB - Инкремент
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF1;
    uint8_t dst;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetLReg(m_regdest);

    dst = dst + 1;

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetLReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0200) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if (dst == 0200) new_psw |= PSW_V;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteDEC()  // DEC - Декремент
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF1;
    uint16_t dst;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regdest);

    dst = dst - 1;

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if (dst == 077777) new_psw |= PSW_V;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteDECB ()  // DECB - Декремент
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF1;
    uint8_t dst;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetLReg(m_regdest);

    dst = dst - 1;

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetLReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0200) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if (dst == 0177) new_psw |= PSW_V;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteNEG()
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint16_t dst;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regdest);

    dst = 0 - dst;

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if (dst == 0100000) new_psw |= PSW_V;
    if (dst != 0) new_psw |= PSW_C;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteNEGB ()
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint8_t dst;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetLReg(m_regdest);

    dst = 0 - dst ;

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetLReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0200) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if (dst == 0200) new_psw |= PSW_V;
    if (dst != 0) new_psw |= PSW_C;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteADC()
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint16_t dst;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regdest);

    dst = dst + (GetC() ? 1 : 0);

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if ((dst == 0100000) && GetC()) new_psw |= PSW_V;
    if ((dst == 0) && GetC()) new_psw |= PSW_C;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteADCB()  // ADCB
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint8_t dst;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetLReg(m_regdest);

    dst = dst + (GetC() ? 1 : 0);

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetLReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0200) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if ((dst == 0200) && GetC()) new_psw |= PSW_V;
    if ((dst == 0) && GetC()) new_psw |= PSW_C;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteSBC()
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint16_t dst;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regdest);

    dst = dst - (GetC() ? 1 : 0);

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if ((dst == 077777) && GetC()) new_psw |= PSW_V;
    if ((dst == 0177777) && GetC()) new_psw |= PSW_C;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteSBCB()
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint8_t dst;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetLReg(m_regdest);

    dst = dst - (GetC() ? 1 : 0);

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetLReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0200) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if ((dst == 0177) && GetC()) new_psw |= PSW_V;
    if ((dst == 0377) && GetC()) new_psw |= PSW_C;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteTST()  // TST
{
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint16_t dst;

    if (m_methdest)
    {
        uint16_t ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regdest);

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
    m_internalTick = TST_TIMING[m_methdest];
}

void CProcessor::ExecuteTSTB()  // TSTB
{
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint8_t dst;

    if (m_methdest)
    {
        uint16_t ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetLReg(m_regdest);

    if (dst & 0200) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
    m_internalTick = TST_TIMING[m_methdest];
}

void CProcessor::ExecuteROR()  // ROR
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint16_t src;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        src = GetReg(m_regdest);

    uint16_t dst = (src >> 1) | (GetC() ? 0100000 : 0);

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if (src & 1) new_psw |= PSW_C;
    if (((new_psw & PSW_N) != 0) != ((new_psw & PSW_C) != 0)) new_psw |= PSW_V;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteRORB()  // RORB
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint8_t src;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        src = GetLReg(m_regdest);

    uint8_t dst = (src >> 1) | (GetC() ? 0200 : 0);

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetLReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0200) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if (src & 1) new_psw |= PSW_C;
    if (((new_psw & PSW_N) != 0) != ((new_psw & PSW_C) != 0)) new_psw |= PSW_V;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteROL()  // ROL
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint16_t src, dst;

    if (m_methdest)
    {
        ea = GetWordAddr((uint8_t)m_methdest, (uint8_t)m_regdest);
        if (m_RPLYrq) return;
        src = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        src = GetReg(m_regdest);

    dst = (src << 1) | (GetC() ? 1 : 0);

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if (src & 0100000) new_psw |= PSW_C;
    if (((new_psw & PSW_N) != 0) != ((new_psw & PSW_C) != 0)) new_psw |= PSW_V;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteROLB()  // ROLB
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint8_t src, dst;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        src = GetLReg(m_regdest);

    dst = (src << 1) | (GetC() ? 1 : 0);

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetLReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0200) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if (src & 0200) new_psw |= PSW_C;
    if (((new_psw & PSW_N) != 0) != ((new_psw & PSW_C) != 0)) new_psw |= PSW_V;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteASR()  // ASR
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint16_t src, dst;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        src = GetReg(m_regdest);

    dst = (src >> 1) | (src & 0100000);

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if (src & 1) new_psw |= PSW_C;
    if (((new_psw & PSW_N) != 0) != ((new_psw & PSW_C) != 0)) new_psw |= PSW_V;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteASRB()  // ASRB
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint8_t src, dst;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        src = GetLReg(m_regdest);

    dst = (src >> 1) | (src & 0200);

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetLReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0200) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if (src & 1) new_psw |= PSW_C;
    if (((new_psw & PSW_N) != 0) != ((new_psw & PSW_C) != 0)) new_psw |= PSW_V;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteASL()  // ASL
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint16_t src, dst;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        src = GetReg(m_regdest);

    dst = src << 1;

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if (src & 0100000) new_psw |= PSW_C;
    if (((new_psw & PSW_N) != 0) != ((new_psw & PSW_C) != 0)) new_psw |= PSW_V;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteASLB()  // ASLB
{
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint8_t src, dst;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        src = GetLReg(m_regdest);

    dst = src << 1;

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetLReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0200) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if (src & 0200) new_psw |= PSW_C;
    if (((new_psw & PSW_N) != 0) != ((new_psw & PSW_C) != 0)) new_psw |= PSW_V;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteSXT()  // SXT - sign-extend
{
    uint8_t new_psw = GetLPSW() & 0xF9;
    if (m_methdest)
    {
        uint16_t ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        SetWord(ea, GetN() ? 0177777 : 0);
        if (m_RPLYrq) return;
    }
    else
        SetReg(m_regdest, GetN() ? 0177777 : 0); //sign extend

    if (!GetN()) new_psw |= PSW_Z;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteMTPS()  // MTPS - move to PS
{
    uint8_t dst;
    if (m_methdest)
    {
        uint16_t ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetLReg(m_regdest);

    SetLPSW((GetLPSW() & 0x10) | (dst & 0xEF));
    SetPC(GetPC());
    m_internalTick = MTPS_TIMING[m_methdest];
}

void CProcessor::ExecuteMFPS()  // MFPS - move from PS
{
    uint8_t psw = GetLPSW();
    uint8_t new_psw = psw & 0xF1;

    if (m_methdest)
    {
        uint16_t ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        GetByte(ea);
        if (m_RPLYrq) return;
        SetByte(ea, psw);
        if (m_RPLYrq) return;
    }
    else
        SetReg(m_regdest, (uint16_t)(signed short)(char)psw); //sign extend

    if (psw & 0200) new_psw |= PSW_N;
    if (psw == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
    m_internalTick = CLR_TIMING[m_methdest];
}

void CProcessor::ExecuteBR()
{
    SetPC(GetPC() + ((short)(char)(uint8_t)(m_instruction & 0xff)) * 2 );
    m_internalTick = BR_TIMING;
}

void CProcessor::ExecuteBNE()
{
    if (GetZ())
        m_internalTick = BRANCH_FALSE_TIMING;
    else
    {
        SetPC(GetPC() + ((short)(char)(uint8_t)(m_instruction & 0xff)) * 2 );
        m_internalTick = BRANCH_TRUE_TIMING;
    }
}

void CProcessor::ExecuteBEQ()
{
    if (!GetZ())
        m_internalTick = BRANCH_FALSE_TIMING;
    else
    {
        m_internalTick = BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char)(uint8_t)(m_instruction & 0xff)) * 2 );
    }
}

void CProcessor::ExecuteBGE()
{
    if (GetN() != GetV())
        m_internalTick = BRANCH_FALSE_TIMING;
    else
    {
        m_internalTick = BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char)(uint8_t)(m_instruction & 0xff)) * 2 );
    }
}

void CProcessor::ExecuteBLT()
{
    if (GetN() == GetV())
        m_internalTick = BRANCH_FALSE_TIMING;
    else
    {
        m_internalTick = BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char)(uint8_t)(m_instruction & 0xff)) * 2 );
    }
}

void CProcessor::ExecuteBGT()
{
    if ((GetN() != GetV()) || GetZ())
        m_internalTick = BRANCH_FALSE_TIMING;
    else
    {
        m_internalTick = BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char)(uint8_t)(m_instruction & 0xff)) * 2 );
    }
}

void CProcessor::ExecuteBLE()
{
    if (! ((GetN() != GetV()) || GetZ()))
        m_internalTick = BRANCH_FALSE_TIMING;
    else
    {
        m_internalTick = BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char)(uint8_t)(m_instruction & 0xff)) * 2 );
    }
}

void CProcessor::ExecuteBPL()
{
    if (GetN())
        m_internalTick = BRANCH_FALSE_TIMING;
    else
    {
        m_internalTick = BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char)(uint8_t)(m_instruction & 0xff)) * 2 );
    }
}

void CProcessor::ExecuteBMI()
{
    if (!GetN())
        m_internalTick = BRANCH_FALSE_TIMING;
    else
    {
        m_internalTick = BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char)(uint8_t)(m_instruction & 0xff)) * 2 );
    }
}

void CProcessor::ExecuteBHI()
{
    if (GetZ() || GetC())
        m_internalTick = BRANCH_FALSE_TIMING;
    else
    {
        m_internalTick = BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char)(uint8_t)(m_instruction & 0xff)) * 2 );
    }
}

void CProcessor::ExecuteBLOS()
{
    if (!(GetZ() || GetC()))
        m_internalTick = BRANCH_FALSE_TIMING;
    else
    {
        m_internalTick = BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char)(uint8_t)(m_instruction & 0xff)) * 2 );
    }
}

void CProcessor::ExecuteBVC()
{
    if (GetV())
        m_internalTick = BRANCH_FALSE_TIMING;
    else
    {
        m_internalTick = BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char)(uint8_t)(m_instruction & 0xff)) * 2 );
    }
}

void CProcessor::ExecuteBVS()
{
    if (!GetV())
        m_internalTick = BRANCH_FALSE_TIMING;
    else
    {
        m_internalTick = BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char)(uint8_t)(m_instruction & 0xff)) * 2 );
    }
}

void CProcessor::ExecuteBHIS()
{
    if (GetC())
        m_internalTick = BRANCH_FALSE_TIMING;
    else
    {
        m_internalTick = BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char)(uint8_t)(m_instruction & 0xff)) * 2 );
    }
}

void CProcessor::ExecuteBLO()
{
    if (!GetC())
        m_internalTick = BRANCH_FALSE_TIMING;
    else
    {
        m_internalTick = BRANCH_TRUE_TIMING;
        SetPC(GetPC() + ((short)(char)(uint8_t)(m_instruction & 0xff)) * 2 );
    }
}

void CProcessor::ExecuteXOR()  // XOR
{
    uint16_t dst;
    uint16_t ea = 0;
    uint8_t new_psw = GetLPSW() & 0xF1;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regdest);

    dst = dst ^ GetReg(m_regsrc);

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
    m_internalTick = XOR_TIMING[m_methdest];
}

void CProcessor::ExecuteMUL()  // MUL - multiply
{
    uint16_t dst = GetReg(m_regsrc);
    uint16_t src, ea = 0;
    int res;
    uint8_t new_psw = GetLPSW() & 0xF0;

    if (m_methdest) ea = GetWordAddr(m_methdest, m_regdest);
    if (m_RPLYrq) return;
    src = m_methdest ? GetWord(ea) : GetReg(m_regdest);
    if (m_RPLYrq) return;

    res = (signed short)dst * (signed short)src;

    SetReg(m_regsrc, (uint16_t)((res >> 16) & 0xffff));
    SetReg(m_regsrc | 1, (uint16_t)(res & 0xffff));

    if (res < 0) new_psw |= PSW_N;
    if (res == 0) new_psw |= PSW_Z;
    if ((res > 32767) || (res < -32768)) new_psw |= PSW_C;
    SetLPSW(new_psw);
    m_internalTick = MUL_TIMING[m_methdest];
}

void CProcessor::ExecuteDIV()  // DIV - divide
{
    uint16_t ea = 0;
    int32_t longsrc;
    int res, res1, src2;
    uint8_t new_psw = GetLPSW() & 0xF0;

    if (m_methdest) ea = GetWordAddr(m_methdest, m_regdest);
    if (m_RPLYrq) return;
    src2 = (int)(signed short)(m_methdest ? GetWord(ea) : GetReg(m_regdest));
    if (m_RPLYrq) return;

    longsrc = (int32_t)(((uint32_t)GetReg(m_regsrc | 1)) | ((uint32_t)GetReg(m_regsrc) << 16));

    m_internalTick = DIV_TIMING[m_methdest];

    if (src2 == 0)
    {
        new_psw |= (PSW_V | PSW_C); //если делят на 0 -- то устанавливаем V и C
        SetLPSW(new_psw);
        return;
    }
    if ((longsrc == (int32_t)020000000000) && (src2 == -1))
    {
        new_psw |= PSW_V; // переполняемся, товарищи
        SetLPSW(new_psw);
        return;
    }

    res = longsrc / src2;
    res1 = longsrc % src2;

    if ((res > 32767) || (res < -32768))
    {
        new_psw |= PSW_V; // переполняемся, товарищи
        SetLPSW(new_psw);
        return;
    }

    SetReg(m_regsrc | 1, res1 & 0177777);
    SetReg(m_regsrc, res & 0177777);

    if (res < 0) new_psw |= PSW_N;
    if (res == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
}

void CProcessor::ExecuteASH()  // ASH - arithmetic shift
{
    uint16_t ea = 0;
    short src;
    short dst;
    uint8_t new_psw = GetLPSW() & 0xF0;

    if (m_methdest) ea = GetWordAddr(m_methdest, m_regdest);
    if (m_RPLYrq) return;
    src = (short)(m_methdest ? GetWord(ea) : GetReg(m_regdest));
    if (m_RPLYrq) return;
    src &= 0x3F;
    src |= (src & 040) ? 0177700 : 0;
    dst = (short)GetReg(m_regsrc);

    m_internalTick = ASH_TIMING[m_methdest];

    if (src >= 0)
    {
        while (src--)
        {
            if (dst & 0100000) new_psw |= PSW_C; else new_psw &= ~PSW_C;
            dst <<= 1;
            if ((dst < 0) != ((new_psw & PSW_C) != 0)) new_psw |= PSW_V;
            m_internalTick = m_internalTick + ASH_S_TIMING;
        }
    }
    else
    {
        while (src++)
        {
            if (dst & 1) new_psw |= PSW_C; else new_psw &= ~PSW_C;
            dst >>= 1;
            m_internalTick = m_internalTick + ASH_S_TIMING;
        }
    }

    SetReg(m_regsrc, dst);

    if (dst < 0) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
}

void CProcessor::ExecuteASHC()  // ASHC - arithmetic shift combined
{
    uint16_t ea = 0;
    int16_t src;
    int32_t dst;
    uint8_t new_psw = GetLPSW() & 0xF0;

    if (m_methdest) ea = GetWordAddr(m_methdest, m_regdest);
    if (m_RPLYrq) return;
    src = (int16_t)(m_methdest ? GetWord(ea) : GetReg(m_regdest));
    if (m_RPLYrq) return;
    src &= 0x3F;
    src |= (src & 040) ? 0177700 : 0;
    dst = ((uint32_t)GetReg(m_regsrc | 1)) | ((uint32_t)GetReg(m_regsrc) << 16);
    m_internalTick = ASHC_TIMING[m_methdest];
    if (src >= 0)
    {
        while (src--)
        {
            if (dst & (int32_t)0x80000000L) new_psw |= PSW_C; else new_psw &= ~PSW_C;
            dst <<= 1;
            if ((dst < 0) != ((new_psw & PSW_C) != 0)) new_psw |= PSW_V;
            m_internalTick = m_internalTick + ASHC_S_TIMING;
        }
    }
    else
    {
        while (src++)
        {
            if (dst & 1) new_psw |= PSW_C; else new_psw &= ~PSW_C;
            dst >>= 1;
            m_internalTick = m_internalTick + ASHC_S_TIMING;
        }
    }

    SetReg(m_regsrc, (uint16_t)((dst >> 16) & 0xffff));
    SetReg(m_regsrc | 1, (uint16_t)(dst & 0xffff));

    SetN(dst < 0);
    SetZ(dst == 0);
    if (dst < 0) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
}

void CProcessor::ExecuteSOB()  // SOB - subtract one: R = R - 1 ; if R != 0 : PC = PC - 2*nn
{
    uint16_t dst = GetReg(m_regsrc);

    m_internalTick = SOB_LAST_TIMING;
    --dst;
    SetReg(m_regsrc, dst);

    if (dst)
    {
        m_internalTick = SOB_TIMING;
        SetPC(GetPC() - (m_instruction & (uint16_t)077) * 2 );
    }
}

void CProcessor::ExecuteMOV()  // MOV - move
{
    uint16_t src_addr, dst_addr;
    uint8_t new_psw = GetLPSW() & 0xF1;
    uint16_t dst;

    if (m_methsrc)
    {
        src_addr = GetWordAddr(m_methsrc, m_regsrc);
        if (m_RPLYrq) return;
        dst = GetWord(src_addr);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regsrc);

    if (m_methdest)
    {
        dst_addr = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        SetWord(dst_addr, dst);
        if (m_RPLYrq) return;
    }
    else
        SetReg(m_regdest, dst);

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
    m_internalTick = MOV_TIMING[m_methsrc][m_methdest];
}

void CProcessor::ExecuteMOVB()  // MOVB - move byte
{
    uint16_t src_addr, dst_addr;
    uint8_t new_psw = GetLPSW() & 0xF1;
    uint8_t dst;

    if (m_methsrc)
    {
        src_addr = GetByteAddr(m_methsrc, m_regsrc);
        if (m_RPLYrq) return;
        dst = GetByte(src_addr);
        if (m_RPLYrq) return;
    }
    else
        dst = GetLReg(m_regsrc);

    if (m_methdest)
    {
        dst_addr = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        GetByte(dst_addr);
        if (m_RPLYrq) return;
        SetByte(dst_addr, dst);
        if (m_RPLYrq) return;
    }
    else
        SetReg(m_regdest, (uint16_t)(signed short)(char)dst);

    if (dst & 0200) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
    m_internalTick = MOVB_TIMING[m_methsrc][m_methdest];
}

void CProcessor::ExecuteCMP()  // CMP - compare
{
    uint16_t src_addr, dst_addr;
    uint8_t new_psw = GetLPSW() & 0xF0;

    uint16_t src;
    uint16_t src2;
    uint16_t dst;

    if (m_methsrc)
    {
        src_addr = GetWordAddr(m_methsrc, m_regsrc);
        if (m_RPLYrq) return;
        src = GetWord(src_addr);
        if (m_RPLYrq) return;
    }
    else
        src = GetReg(m_regsrc);

    if (m_methdest)
    {
        dst_addr = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src2 = GetWord(dst_addr);
        if (m_RPLYrq) return;
    }
    else
        src2 = GetReg(m_regdest);

    dst = src - src2;

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if (((src ^ src2) & ~(dst ^ src2)) & 0100000) new_psw |= PSW_V;
    if (((~src & src2) | (~(src ^ src2) & dst)) & 0100000) new_psw |= PSW_C;
    SetLPSW(new_psw);
    m_internalTick = CMP_TIMING[m_methsrc][m_methdest];
}

void CProcessor::ExecuteCMPB()  // CMPB - compare byte
{
    uint16_t src_addr, dst_addr;
    uint8_t new_psw = GetLPSW() & 0xF0;

    uint8_t src;
    uint8_t src2;
    uint8_t dst;

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

    dst = src - src2;

    if (dst & 0200) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if (((src ^ src2) & ~(dst ^ src2)) & 0200) new_psw |= PSW_V;
    if (((~src & src2) | (~(src ^ src2) & dst)) & 0200) new_psw |= PSW_C;
    SetLPSW(new_psw);
    m_internalTick = CMP_TIMING[m_methsrc][m_methdest];
}

void CProcessor::ExecuteBIT()  // BIT - bit test
{
    uint16_t src_addr, dst_addr;
    uint8_t new_psw = GetLPSW() & 0xF1;
    uint16_t src;
    uint16_t src2;
    uint16_t dst;

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

void CProcessor::ExecuteBITB()  // BITB - bit test on byte
{
    uint16_t src_addr, dst_addr;
    uint8_t new_psw = GetLPSW() & 0xF1;
    uint8_t src;
    uint8_t src2;
    uint8_t dst;

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

void CProcessor::ExecuteBIC()  // BIC - bit clear
{
    uint16_t src_addr, dst_addr = 0;
    uint8_t new_psw = GetLPSW() & 0xF1;
    uint16_t src;
    uint16_t src2;
    uint16_t dst;

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

    dst = src2 & (~src);

    if (m_methdest)
        SetWord(dst_addr, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
    m_internalTick = MOV_TIMING[m_methsrc][m_methdest];
}

void CProcessor::ExecuteBICB()  // BICB - bit clear
{
    uint16_t src_addr, dst_addr = 0;
    uint8_t new_psw = GetLPSW() & 0xF1;
    uint8_t src;
    uint8_t src2;
    uint8_t dst;

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

    dst = src2 & (~src);


    if (m_methdest)
        SetByte(dst_addr, dst);
    else
        SetLReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0200) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
    m_internalTick = MOVB_TIMING[m_methsrc][m_methdest];
}

void CProcessor::ExecuteBIS()  // BIS - bit set
{
    uint16_t src_addr, dst_addr = 0;
    uint8_t new_psw = GetLPSW() & 0xF1;
    uint16_t src;
    uint16_t src2;
    uint16_t dst;

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

    dst = src2 | src;

    if (m_methdest)
        SetWord(dst_addr, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
    m_internalTick = MOV_TIMING[m_methsrc][m_methdest];
}

void CProcessor::ExecuteBISB()  // BISB - bit set on byte
{
    uint16_t src_addr, dst_addr = 0;
    uint8_t new_psw = GetLPSW() & 0xF1;
    uint8_t src;
    uint8_t src2;
    uint8_t dst;

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

    dst = src2 | src;

    if (m_methdest)
        SetByte(dst_addr, dst);
    else
        SetLReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0200) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
    m_internalTick = MOVB_TIMING[m_methsrc][m_methdest];
}

void CProcessor::ExecuteADD()  // ADD
{
    uint16_t src_addr, dst_addr = 0;
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint16_t src, src2, dst;

    if (m_methsrc)
    {
        src_addr = GetWordAddr(m_methsrc, m_regsrc);
        if (m_RPLYrq) return;
        src = GetWord(src_addr);
        if (m_RPLYrq) return;
    }
    else
        src = GetReg(m_regsrc);

    if (m_methdest)
    {
        dst_addr = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src2 = GetWord(dst_addr);
        if (m_RPLYrq) return;
    }
    else
        src2 = GetReg(m_regdest);

    dst = src2 + src;

    if (m_methdest)
        SetWord(dst_addr, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if ((~(src ^ src2) & (dst ^ src2)) & 0100000) new_psw |= PSW_V;
    if (((src & src2) | ((src ^ src2) & ~dst)) & 0100000) new_psw |= PSW_C;
    SetLPSW(new_psw);
    m_internalTick = MOVB_TIMING[m_methsrc][m_methdest];
}

void CProcessor::ExecuteSUB()  // SUB
{
    uint16_t src_addr, dst_addr = 0;
    uint8_t new_psw = GetLPSW() & 0xF0;
    uint16_t src, src2, dst;

    if (m_methsrc)
    {
        src_addr = GetWordAddr(m_methsrc, m_regsrc);
        if (m_RPLYrq) return;
        src = GetWord(src_addr);
        if (m_RPLYrq) return;
    }
    else
        src = GetReg(m_regsrc);

    if (m_methdest)
    {
        dst_addr = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src2 = GetWord(dst_addr);
        if (m_RPLYrq) return;
    }
    else
        src2 = GetReg(m_regdest);

    dst = src2 - src;

    if (m_methdest)
        SetWord(dst_addr, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    if (dst & 0100000) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    if (((src ^ src2) & ~(dst ^ src)) & 0100000) new_psw |= PSW_V;
    if (((src & ~src2) | (~(src ^ src2) & dst)) & 0100000) new_psw |= PSW_C;
    SetLPSW(new_psw);
    m_internalTick = MOVB_TIMING[m_methsrc][m_methdest];
}

void CProcessor::ExecuteEMT()  // EMT - emulator trap
{
    m_EMT_rq = true;
    m_internalTick = EMT_TIMING;
}

void CProcessor::ExecuteTRAP()
{
    m_TRAPrq = true;
    m_internalTick = EMT_TIMING;
}

void CProcessor::ExecuteJSR()  // JSR - jump subroutine: *--SP = R; R = PC; PC = &d (a-mode > 0)
{
    if (m_methdest == 0)
    {
        // Неправильный метод адресации
        m_ILLGrq = true;
        m_internalTick = EMT_TIMING;
    }
    else
    {
        uint16_t dst;
        dst = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;

        SetSP( GetSP() - 2 );
        SetWord( GetSP(), GetReg(m_regsrc) );
        SetReg(m_regsrc, GetPC());
        SetPC(dst);
        if (m_RPLYrq) return;

        m_internalTick = JSR_TIMING[m_methdest - 1];
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
// CPU image format (64 bytes):
//   2   bytes      PSW
//   2*8 bytes      Registers R0..R7
//   2*2 bytes      Saved PC and PSW
//   2   byte       Stopped flag: 1 - stopped, 0 - not stopped
//   2   bytes      Internal tick count
//   3   bytes      Flags
//   1   byte       VIRQ reset request
//   2   bytes      Reserved
//  32   bytes      VIRQ vectors

void CProcessor::SaveToImage(uint8_t* pImage) const
{
    // Processor data                               // Offset Size
    uint16_t* pwImage = (uint16_t*) pImage;         //    0    --
    *pwImage++ = m_psw;                             //    0     2   PSW
    memcpy(pwImage, m_R, 2 * 8);  pwImage += 8;     //    2    16   Registers R0-R7
    *pwImage++ = m_savepc;                          //   18     2   PC'
    *pwImage++ = m_savepsw;                         //   20     2   PSW'
    *pwImage++ = (m_okStopped ? 1 : 0);             //   22     2   Stopped
    *pwImage++ = m_internalTick;                    //   24     2   Internal tick count
    uint8_t* pbImage = (uint8_t*) pwImage;
    uint8_t flags0 = 0;
    flags0 |= (m_stepmode ?   1 : 0);
    flags0 |= (m_buserror ?   2 : 0);
    flags0 |= (m_haltpin  ?   4 : 0);
    flags0 |= (m_DCLOpin  ?   8 : 0);
    flags0 |= (m_ACLOpin  ?  16 : 0);
    flags0 |= (m_waitmode ?  32 : 0);
    *pbImage++ = flags0;                            //   26     1   Flags
    uint8_t flags1 = 0;
    flags1 |= (m_STRTrq ?   1 : 0);
    flags1 |= (m_RPLYrq ?   2 : 0);
    flags1 |= (m_ILLGrq ?   4 : 0);
    flags1 |= (m_RSVDrq ?   8 : 0);
    flags1 |= (m_TBITrq ?  16 : 0);
    flags1 |= (m_ACLOrq ?  32 : 0);
    flags1 |= (m_HALTrq ?  64 : 0);
    flags1 |= (m_EVNTrq ? 128 : 0);
    *pbImage++ = flags1;                            //   27     1   Flags
    uint8_t flags2 = 0;
    flags2 |= (m_FIS_rq ?   1 : 0);
    flags2 |= (m_BPT_rq ?   2 : 0);
    flags2 |= (m_IOT_rq ?   4 : 0);
    flags2 |= (m_EMT_rq ?   8 : 0);
    flags2 |= (m_TRAPrq ?  16 : 0);
    flags2 |= (m_ACLOreset ? 32 : 0);
    flags2 |= (m_EVNTreset ? 64 : 0);
    *pbImage++ = flags2;                            //   28     1   Flags
    *pbImage++ = m_VIRQreset;                       //   29     1   VIRQ reset request
    //                                              //   30     2   Reserved
    memcpy(pImage + 32, m_virq, 2 * 16);            //   32    32   VIRQ vectors
}

void CProcessor::LoadFromImage(const uint8_t* pImage)
{
    const uint16_t* pwImage = (const uint16_t*) pImage;  //    0    --
    m_psw = *pwImage++;                             //    0     2   PSW
    memcpy(m_R, pwImage, 2 * 8);  pwImage += 8;     //    2    16   Registers R0-R7
    m_savepc    = *pwImage++;                       //   18     2   PC'
    m_savepsw   = *pwImage++;                       //   20     2   PSW'
    m_okStopped = (*pwImage++ != 0);                //   22     2   Stopped
    m_internalTick = *pwImage++;                    //   24     2   Internal tick count
    const uint8_t* pbImage = (const uint8_t*) pwImage;
    uint8_t flags0 = *pbImage++;                    //   26     1   Flags
    m_stepmode  = ((flags0 &  1) != 0);
    m_buserror  = ((flags0 &  2) != 0);
    m_haltpin   = ((flags0 &  4) != 0);
    m_DCLOpin   = ((flags0 &  8) != 0);
    m_ACLOpin   = ((flags0 & 16) != 0);
    m_waitmode  = ((flags0 & 32) != 0);
    uint8_t flags1 = *pbImage++;                    //   27     1   Flags
    m_STRTrq    = ((flags1 &   1) != 0);
    m_RPLYrq    = ((flags1 &   2) != 0);
    m_ILLGrq    = ((flags1 &   4) != 0);
    m_RSVDrq    = ((flags1 &   8) != 0);
    m_TBITrq    = ((flags1 &  16) != 0);
    m_ACLOrq    = ((flags1 &  32) != 0);
    m_HALTrq    = ((flags1 &  64) != 0);
    m_EVNTrq    = ((flags1 & 128) != 0);
    uint8_t flags2 = *pbImage++;                    //   28     1   Flags
    m_FIS_rq    = ((flags2 &  1) != 0);
    m_BPT_rq    = ((flags2 &  2) != 0);
    m_IOT_rq    = ((flags2 &  4) != 0);
    m_EMT_rq    = ((flags2 &  8) != 0);
    m_TRAPrq    = ((flags2 & 16) != 0);
    m_ACLOreset = ((flags2 & 32) != 0);
    m_EVNTreset = ((flags2 & 64) != 0);
    m_VIRQreset = *pbImage++;                       //   29     1   VIRQ reset request
    //                                              //   30     2   Reserved
    memcpy(m_virq, pImage + 32, 2 * 16);            //   32    32   VIRQ vectors
}

uint16_t CProcessor::GetWordAddr (uint8_t meth, uint8_t reg)
{
    switch (meth)
    {
    case 1:   //(R)
        return GetReg(reg);
    case 2:   //(R)+
        {
            uint16_t addr = GetReg(reg);
            SetReg(reg, addr + 2);
            return addr;
        }
    case 3:  //@(R)+
        {
            uint16_t addr = GetReg(reg);
            SetReg(reg, addr + 2);
            return GetWord(addr);
        }
    case 4: //-(R)
        SetReg(reg, GetReg(reg) - 2);
        return GetReg(reg);
    case 5: //@-(R)
        {
            SetReg(reg, GetReg(reg) - 2);
            uint16_t addr = GetReg(reg);
            return GetWord(addr);
        }
    case 6: //d(R)
        {
            uint16_t addr = GetWord(GetPC());
            SetPC(GetPC() + 2);
            return GetReg(reg) + addr;
        }
    case 7: //@d(r)
        {
            uint16_t addr = GetWord(GetPC());
            SetPC(GetPC() + 2);
            addr = GetReg(reg) + addr;
            if (!m_RPLYrq)
                return GetWord(addr);
            return addr;
        }
    }
    return 0;
}

uint16_t CProcessor::GetByteAddr (uint8_t meth, uint8_t reg)
{
    uint16_t addr;

    addr = 0;
    switch (meth)
    {
    case 1:
        addr = GetReg(reg);
        break;
    case 2:
        addr = GetReg(reg);
        SetReg(reg, addr + (reg < 6 ? 1 : 2));
        break;
    case 3:
        addr = GetReg(reg);
        SetReg(reg, addr + 2);
        addr = GetWord(addr);
        break;
    case 4:
        SetReg(reg, GetReg(reg) - (reg < 6 ? 1 : 2));
        addr = GetReg(reg);
        break;
    case 5:
        SetReg(reg, GetReg(reg) - 2);
        addr = GetReg(reg);
        addr = GetWord(addr);
        break;
    case 6: //d(R)
        addr = GetWord(GetPC());
        SetPC(GetPC() + 2);
        addr = GetReg(reg) + addr;
        break;
    case 7: //@d(r)
        addr = GetWord(GetPC());
        SetPC(GetPC() + 2);
        addr = GetReg(reg) + addr;
        if (!m_RPLYrq) addr = GetWord(addr);
        break;
    }

    return addr;
}


//////////////////////////////////////////////////////////////////////
