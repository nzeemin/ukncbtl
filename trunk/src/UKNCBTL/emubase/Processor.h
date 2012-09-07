/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Processor.h
// KM1801VM2 processor

#pragma once

#include "Defines.h"
#include "Memory.h"


class CMemoryController;

//////////////////////////////////////////////////////////////////////


class CProcessor  // KM1801VM2 processor
{

public:  // Constructor / initialization
                CProcessor(LPCTSTR name);
    void        AttachMemoryController(CMemoryController* ctl) { m_pMemoryController = ctl; }
    void        SetHALTPin(BOOL value);
    void        SetDCLOPin(BOOL value);
    void        SetACLOPin(BOOL value);
    void        MemoryError();
    LPCTSTR     GetName() const { return m_name; }

public:
    static void Init();  // Initialize static tables
    static void Done();  // Release memory used for static tables
protected:  // Statics
    typedef void ( CProcessor::*ExecuteMethodRef )();
    static ExecuteMethodRef* m_pExecuteMethodMap;
    static void RegisterMethodRef(WORD start, WORD end, CProcessor::ExecuteMethodRef methodref);

protected:  // Processor state
    TCHAR       m_name[5];          // Processor name (DO NOT use it inside the processor code!!!)
    WORD        m_internalTick;     // How many ticks waiting to the end of current instruction
    WORD        m_psw;              // Processor Status Word (PSW)
    WORD        m_R[8];             // Registers (R0..R5, R6=SP, R7=PC)
    BOOL        m_okStopped;        // "Processor stopped" flag
    WORD        m_savepc;           // CPC register
    WORD        m_savepsw;          // CPSW register
    BOOL        m_stepmode;         // Read TRUE if it's step mode
    BOOL        m_buserror;         // Read TRUE if occured bus error for implementing double bus error if needed
    BOOL        m_haltpin;          // HALT pin
    BOOL        m_DCLOpin;          // DCLO pin
    BOOL        m_ACLOpin;          // ACLO pin
    BOOL        m_waitmode;         // WAIT

protected:  // Current instruction processing
    WORD        m_instruction;      // Curent instruction
    int         m_regsrc;           // Source register number
    int         m_methsrc;          // Source address mode
    WORD        m_addrsrc;          // Source address
    int         m_regdest;          // Destination register number
    int         m_methdest;         // Destination address mode
    WORD        m_addrdest;         // Destination address
protected:  // Interrupt processing
    BOOL        m_STRTrq;           // Start interrupt pending
    BOOL        m_RPLYrq;           // Hangup interrupt pending
    BOOL        m_ILLGrq;           // Illegal instruction interrupt pending
    BOOL        m_RSVDrq;           // Reserved instruction interrupt pending
    BOOL        m_TBITrq;           // T-bit interrupt pending
    BOOL        m_ACLOrq;           // Power down interrupt pending
    BOOL        m_HALTrq;           // HALT command or HALT signal
    BOOL        m_EVNTrq;           // Timer event interrupt pending
    BOOL        m_FIS_rq;           // FIS command interrupt pending
    BOOL        m_BPT_rq;           // BPT command interrupt pending
    BOOL        m_IOT_rq;           // IOT command interrupt pending
    BOOL        m_EMT_rq;           // EMT command interrupt pending
    BOOL        m_TRAPrq;           // TRAP command interrupt pending
    WORD        m_virq[16];         // VIRQ vector
    BOOL        m_ACLOreset;        // Power fail interrupt request reset
    BOOL        m_EVNTreset;        // EVNT interrupt request reset;
    int         m_VIRQreset;        // VIRQ request reset for given device
protected:
    CMemoryController* m_pMemoryController;

public:
    CMemoryController* GetMemoryController() { return m_pMemoryController; }

public:  // Register control
    WORD        GetPSW() const { return m_psw; }
    WORD        GetCPSW() const { return m_savepsw; }
    BYTE        GetLPSW() const { return LOBYTE(m_psw); }
    void        SetPSW(WORD word)
    {
        m_psw = word & 0777;
        if ((m_psw & 0600) != 0600) m_savepsw = m_psw;
    }
    void        SetCPSW(WORD word) {m_savepsw = word; }
    void        SetLPSW(BYTE byte)
    {
        m_psw = (m_psw & 0xFF00) | (WORD)byte;
        if ((m_psw & 0600) != 0600) m_savepsw = m_psw;
    }
    WORD        GetReg(int regno) const { return m_R[regno]; }
    void        SetReg(int regno, WORD word)
    {
        m_R[regno] = word;
        if ((regno == 7) && ((m_psw & 0600)!=0600))	m_savepc = word;
    }
    BYTE        GetLReg(int regno) const { return LOBYTE(m_R[regno]); }
    void        SetLReg(int regno, BYTE byte)
    {
        m_R[regno] = (m_R[regno] & 0xFF00) | (WORD)byte;
        if ((regno == 7) && ((m_psw & 0600)!=0600))	m_savepc = m_R[7];
    }
    WORD        GetSP() const { return m_R[6]; }
    void        SetSP(WORD word) { m_R[6] = word; }
    WORD        GetPC() const { return m_R[7]; }
    WORD        GetCPC() const { return m_savepc; }
    void        SetPC(WORD word)
    { 
        m_R[7] = word;
        if ((m_psw & 0600) != 0600) m_savepc = word;
    }
    void        SetCPC(WORD word) {m_savepc = word; }

public:  // PSW bits control
    void        SetC(BOOL bFlag);
    WORD        GetC() const { return (m_psw & PSW_C) != 0; }
    void        SetV(BOOL bFlag);
    WORD        GetV() const { return (m_psw & PSW_V) != 0; }
    void        SetN(BOOL bFlag);
    WORD        GetN() const { return (m_psw & PSW_N) != 0; }
    void        SetZ(BOOL bFlag);
    WORD        GetZ() const { return (m_psw & PSW_Z) != 0; }
    void        SetHALT(BOOL bFlag);
    WORD        GetHALT() const { return (m_psw & PSW_HALT) != 0; }

public:  // Processor state
    // "Processor stopped" flag
    BOOL        IsStopped() const { return m_okStopped; }
    // HALT flag (TRUE - HALT mode, FALSE - USER mode)
    BOOL        IsHaltMode() const
    { 
            return ((m_psw & 0400) != 0);
    }
public:  // Processor control
    void        TickEVNT();  // EVNT signal
    void        InterruptVIRQ(int que, WORD interrupt);  // External interrupt via VIRQ signal
    WORD        GetVIRQ(int que);
    void        Execute();   // Execute one instruction - for debugger only
    BOOL        InterruptProcessing();
    void        CommandExecution();
    
public:  // Saving/loading emulator status (pImage addresses up to 32 bytes)
    void        SaveToImage(BYTE* pImage) const;
    void        LoadFromImage(const BYTE* pImage);

protected:  // Implementation
    void        FetchInstruction();      // Read next instruction
    void        TranslateInstruction();  // Execute the instruction
protected:  // Implementation - memory access
    WORD        GetWordExec(WORD address) { return m_pMemoryController->GetWordExec(address, IsHaltMode()); }
    WORD        GetWord(WORD address) { return m_pMemoryController->GetWord(address, IsHaltMode()); }
    void        SetWord(WORD address, WORD word) { m_pMemoryController->SetWord(address, IsHaltMode(), word); }
    BYTE        GetByte(WORD address) { return m_pMemoryController->GetByte(address, IsHaltMode()); }
    void        SetByte(WORD address, BYTE byte) { m_pMemoryController->SetByte(address, IsHaltMode(), byte); }

protected:  // PSW bits calculations
    BOOL static CheckForNegative(BYTE byte) { return (byte & 0200) != 0; }
    BOOL static CheckForNegative(WORD word) { return (word & 0100000) != 0; }
    BOOL static CheckForZero(BYTE byte) { return byte == 0; }
    BOOL static CheckForZero(WORD word) { return word == 0; }
    BOOL static CheckAddForOverflow(BYTE a, BYTE b);
    BOOL static CheckAddForOverflow(WORD a, WORD b);
    BOOL static CheckSubForOverflow(BYTE a, BYTE b);
    BOOL static CheckSubForOverflow(WORD a, WORD b);
    BOOL static CheckAddForCarry(BYTE a, BYTE b);
    BOOL static CheckAddForCarry(WORD a, WORD b);
    BOOL static CheckSubForCarry(BYTE a, BYTE b);
    BOOL static CheckSubForCarry(WORD a, WORD b);

protected:  // Implementation - instruction execution
    // No fields
    WORD        GetWordAddr (BYTE meth, BYTE reg);
    WORD        GetByteAddr (BYTE meth, BYTE reg);

    void        ExecuteUNKNOWN ();  // Нет такой инструкции - просто вызывается TRAP 10
    void        ExecuteHALT ();
    void        ExecuteWAIT ();
    void        ExecuteRCPC	();
    void        ExecuteRCPS ();
    void        ExecuteWCPC	();
    void        ExecuteWCPS	();
    void        ExecuteMFUS ();
    void        ExecuteMTUS ();
    void        ExecuteRTI ();
    void        ExecuteBPT ();
    void        ExecuteIOT ();
    void        ExecuteRESET ();
    void        ExecuteSTEP	();
    void        ExecuteRSEL ();
    void        Execute000030 ();
    void        ExecuteFIS ();
    void        ExecuteRUN	();
    void        ExecuteRTT ();
    void        ExecuteCCC ();
    void        ExecuteSCC ();

    // One fiels
    void        ExecuteRTS ();

    // Two fields
    void        ExecuteJMP ();
    void        ExecuteSWAB ();
    void        ExecuteCLR ();
    void        ExecuteCOM ();
    void        ExecuteINC ();
    void        ExecuteDEC ();
    void        ExecuteNEG ();
    void        ExecuteADC ();
    void        ExecuteSBC ();
    void        ExecuteTST ();
    void        ExecuteTSTB ();
    void        ExecuteROR ();
    void        ExecuteROL ();
    void        ExecuteASR ();
    void        ExecuteASL ();
    void        ExecuteMARK ();
    void        ExecuteSXT ();
    void        ExecuteMTPS ();
    void        ExecuteMFPS ();
    
    // Branchs & interrupts
    void        ExecuteBR ();
    void        ExecuteBNE ();
    void        ExecuteBEQ ();
    void        ExecuteBGE ();
    void        ExecuteBLT ();
    void        ExecuteBGT ();
    void        ExecuteBLE ();
    void        ExecuteBPL ();
    void        ExecuteBMI ();
    void        ExecuteBHI ();
    void        ExecuteBLOS ();
    void        ExecuteBVC ();
    void        ExecuteBVS ();
    void        ExecuteBHIS ();
    void        ExecuteBLO ();

    void        ExecuteEMT ();
    void        ExecuteTRAP ();

    // Three fields
    void        ExecuteJSR ();
    void        ExecuteXOR ();
    void        ExecuteSOB ();
    void        ExecuteMUL ();
    void        ExecuteDIV ();
    void        ExecuteASH ();
    void        ExecuteASHC ();

    // Four fields
    void        ExecuteMOV ();
    void        ExecuteMOVB ();
    void        ExecuteCMP ();
    void        ExecuteBIT ();
    void        ExecuteBIC ();
    void        ExecuteBIS ();

    void        ExecuteADD ();
    void        ExecuteSUB ();

};

// PSW bits control - implementation
inline void CProcessor::SetC (BOOL bFlag)
{
    if (bFlag) m_psw |= PSW_C; else m_psw &= ~PSW_C;
    if ((m_psw & 0600) != 0600) m_savepsw = m_psw;
}
inline void CProcessor::SetV (BOOL bFlag)
{
    if (bFlag) m_psw |= PSW_V; else m_psw &= ~PSW_V;
    if ((m_psw & 0600) != 0600) m_savepsw = m_psw;
}
inline void CProcessor::SetN (BOOL bFlag)
{
    if (bFlag) m_psw |= PSW_N; else m_psw &= ~PSW_N;
    if ((m_psw & 0600) != 0600) m_savepsw = m_psw;
}
inline void CProcessor::SetZ (BOOL bFlag)
{
    if (bFlag) m_psw |= PSW_Z; else m_psw &= ~PSW_Z;
    if ((m_psw & 0600) != 0600) m_savepsw = m_psw;
}

inline void CProcessor::SetHALT (BOOL bFlag)
{
    if (bFlag) m_psw |= PSW_HALT; else m_psw &= ~PSW_HALT;
}

// PSW bits calculations - implementation
inline BOOL CProcessor::CheckAddForOverflow (BYTE a, BYTE b)
{
#ifdef _M_IX86
    BOOL bOverflow = FALSE;
    _asm
    {
        pushf
        push cx
        mov cl,byte ptr [a]
        add cl,byte ptr [b]
        jno end
        mov dword ptr [bOverflow],1
    end:
        pop cx
        popf
    }
    return bOverflow;
#else
    //WORD sum = a < 0200 ? (WORD)a + (WORD)b + 0200 : (WORD)a + (WORD)b - 0200;
    //return HIBYTE (sum) != 0;
    BYTE sum = a + b;
    return ((~a ^ b) & (a ^ sum)) & 0200;
#endif
}
inline BOOL CProcessor::CheckAddForOverflow (WORD a, WORD b)
{
#ifdef _M_IX86
    BOOL bOverflow = FALSE;
    _asm
    {
        pushf
        push cx
        mov cx,word ptr [a]
        add cx,word ptr [b]
        jno end
        mov dword ptr [bOverflow],1
    end:
        pop cx
        popf
    }
    return bOverflow;
#else
    //DWORD sum =  a < 0100000 ? (DWORD)a + (DWORD)b + 0100000 : (DWORD)a + (DWORD)b - 0100000;
    //return HIWORD (sum) != 0;
    WORD sum = a + b;
    return ((~a ^ b) & (a ^ sum)) & 0100000;
#endif
}

inline BOOL CProcessor::CheckSubForOverflow (BYTE a, BYTE b)
{
#ifdef _M_IX86
    BOOL bOverflow = FALSE;
    _asm
    {
        pushf
        push cx
        mov cl,byte ptr [a]
        sub cl,byte ptr [b]
        jno end
        mov dword ptr [bOverflow],1
    end:
        pop cx
        popf
    }
    return bOverflow;
#else
    //WORD sum = a < 0200 ? (WORD)a - (WORD)b + 0200 : (WORD)a - (WORD)b - 0200;
    //return HIBYTE (sum) != 0;
    BYTE sum = a - b;
    return ((a ^ b) & (~b ^ sum)) & 0200;
#endif
}
inline BOOL CProcessor::CheckSubForOverflow (WORD a, WORD b)
{
#ifdef _M_IX86
    BOOL bOverflow = FALSE;
    _asm
    {
        pushf
        push cx
        mov cx,word ptr [a]
        sub cx,word ptr [b]
        jno end
        mov dword ptr [bOverflow],1
    end:
        pop cx
        popf
    }
    return bOverflow;
#else
    //DWORD sum =  a < 0100000 ? (DWORD)a - (DWORD)b + 0100000 : (DWORD)a - (DWORD)b - 0100000;
    //return HIWORD (sum) != 0;
    WORD sum = a - b;
    return ((a ^ b) & (~b ^ sum)) & 0100000;
#endif
}
inline BOOL CProcessor::CheckAddForCarry (BYTE a, BYTE b)
{
    WORD sum = (WORD)a + (WORD)b;
    return HIBYTE (sum) != 0;
}
inline BOOL CProcessor::CheckAddForCarry (WORD a, WORD b)
{
    DWORD sum = (DWORD)a + (DWORD)b;
    return HIWORD (sum) != 0;
}
inline BOOL CProcessor::CheckSubForCarry (BYTE a, BYTE b)
{
    WORD sum = (WORD)a - (WORD)b;
    return HIBYTE (sum) != 0;
}
inline BOOL CProcessor::CheckSubForCarry (WORD a, WORD b)
{
    DWORD sum = (DWORD)a - (DWORD)b;
    return HIWORD (sum) != 0;
}


//////////////////////////////////////////////////////////////////////
