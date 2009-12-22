// Processor.h
//

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
    void        AssertHALT();
	void		DeassertHALT();
	void		MemoryError();
    LPCTSTR     GetName() const { return m_name; }
	void        SetInternalTick (WORD tick) { m_internalTick = tick; }

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
    BOOL        m_userspace;        // Read TRUE if user space is used -- CPU is accessing I/O from HALT mode using user space
    BOOL        m_stepmode;         // Read TRUE if it's step mode
    BOOL        m_haltpin;			// HALT 
    BOOL        m_waitmode;			// WAIT

protected:  // Current instruction processing
    WORD        m_instruction;      // Curent instruction
    int         m_regsrc;           // Source register number
    int         m_methsrc;          // Source address mode
    WORD        m_addrsrc;          // Source address
    int         m_regdest;          // Destination register number
    int         m_methdest;         // Destination address mode
    WORD        m_addrdest;         // Destination address
protected:  // Interrupt processing
    BOOL        m_RPLYrq;           // Hangup interrupt pending
    BOOL        m_RSVDrq;           // Reserved instruction interrupt pending
    BOOL        m_TBITrq;           // T-bit interrupt pending
	BOOL		m_ACLOrq;           // Power down interrupt pending
    BOOL        m_HALTrq;           // HALT command or HALT signal
    BOOL        m_RPL2rq;           // Double hangup interrupt pending
	BOOL		m_EVNTrq;           // Timer event interrupt pending
    BOOL        m_FIS_rq;           // FIS command interrupt pending
    BOOL        m_BPT_rq;           // BPT command interrupt pending
    BOOL        m_IOT_rq;           // IOT command interrupt pending
    BOOL        m_EMT_rq;           // EMT command interrupt pending
    BOOL        m_TRAPrq;           // TRAP command interrupt pending
    //BOOL        m_VIRQrq;           // VIRQ vector interrupt pending
    //WORD        m_VIRQvector;       // VIRQ interrupt vector
    int         m_virqrq;           // VIRQ pending
    WORD        m_virq[16];         // VIRQ vector
protected:
    CMemoryController* m_pMemoryController;

public:
    CMemoryController* GetMemoryController() { return m_pMemoryController; }

public:  // Register control
    WORD        GetPSW() { return m_psw; }
    WORD        GetCPSW() { return m_savepsw; }
    void        SetPSW(WORD word) { m_psw = word; }
    WORD        GetReg(int regno) { return m_R[regno]; }
    void        SetReg(int regno, WORD word) { m_R[regno] = word; }
    WORD        GetSP() const { return m_R[6]; }
    void        SetSP(WORD word) { m_R[6] = word; }
    WORD        GetPC() const { return m_R[7]; }
    WORD        GetCPC() const { return m_savepc; }
    void        SetPC(WORD word) { m_R[7] = word; }

public:  // PSW bits control
    void        SetC(BOOL bFlag);
    WORD        GetC() const { return (m_psw & PSW_C) != 0; }
    void        SetV(BOOL bFlag);
    WORD        GetV() const { return (m_psw & PSW_V) != 0; }
    void        SetN(BOOL bFlag);
    WORD        GetN() const { return (m_psw & PSW_N) != 0; }
    void        SetZ(BOOL bFlag);
    WORD        GetZ() const { return (m_psw & PSW_Z) != 0; }
    WORD        GetHALT() const { return (m_psw & PSW_HALT) != 0; }

public:  // Processor state
    // "Processor stopped" flag
    BOOL        IsStopped() const { return m_okStopped; }
    // HALT flag (TRUE - HALT mode, FALSE - USER mode)
    BOOL        IsHaltMode() 
	{ 
			BOOL mode = ((m_psw & 0x100) != 0); 
			if (mode)
				if(m_userspace)
					return 0;
			return mode;
	}
public:  // Processor control
    void        Start();     // Start processor
    void        Stop();      // Stop processor
    void        TickEVNT();  // EVNT signal
	void		PowerFail();
    void        InterruptVIRQ(int que, WORD interrupt);  // External interrupt via VIRQ signal
    void        Execute();   // Execute one instruction - for debugger only
    
public:  // Saving/loading emulator status (pImage addresses up to 32 bytes)
    void        SaveToImage(BYTE* pImage);
    void        LoadFromImage(const BYTE* pImage);

protected:  // Implementation
    void        FetchInstruction();      // Read next instruction
    void        TranslateInstruction();  // Execute the instruction
protected:  // Implementation - instruction processing
    WORD        CalculateOperAddr (int meth, int reg);
	WORD        CalculateOperAddrSrc (int meth, int reg);
    BYTE        GetByteSrc();
    BYTE        GetByteDest();
    void        SetByteDest(BYTE);
    WORD        GetWordSrc();
    WORD        GetWordDest();
    void        SetWordDest(WORD);
    WORD        GetDstWordArgAsBranch();
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
	WORD		GetWordAddr (BYTE meth, BYTE reg);
	WORD		GetByteAddr (BYTE meth, BYTE reg);

    void        ExecuteUNKNOWN ();  // Нет такой инструкции - просто вызывается TRAP 10
    void        ExecuteHALT ();
    void        ExecuteWAIT ();
	void		ExecuteRCPC	();
	void		ExecuteRCPS ();
	void		ExecuteWCPC	();
	void		ExecuteWCPS	();
	void		ExecuteMFUS ();
	void		ExecuteMTUS ();
    void        ExecuteRTI ();
    void        ExecuteBPT ();
    void        ExecuteIOT ();
    void        ExecuteRESET ();
	void		ExecuteSTEP	();
    void        ExecuteRSEL ();
    void        Execute000030 ();
    void        ExecuteFIS ();
	void		ExecuteRUN	();
    void        ExecuteRTT ();
    void        ExecuteNOP ();
    void        ExecuteCLC ();
    void        ExecuteCLV ();
    void        ExecuteCLVC ();
    void        ExecuteCLZ ();
    void        ExecuteCLZC ();
    void        ExecuteCLZV ();
    void        ExecuteCLZVC ();
    void        ExecuteCLN ();
    void        ExecuteCLNC ();
    void        ExecuteCLNV ();
    void        ExecuteCLNVC ();
    void        ExecuteCLNZ ();
    void        ExecuteCLNZC ();
    void        ExecuteCLNZV ();
    void        ExecuteCCC ();
    void        ExecuteSEC ();
    void        ExecuteSEV ();
    void        ExecuteSEVC ();
    void        ExecuteSEZ ();
    void        ExecuteSEZC ();
    void        ExecuteSEZV ();
    void        ExecuteSEZVC ();
    void        ExecuteSEN ();
    void        ExecuteSENC ();
    void        ExecuteSENV ();
    void        ExecuteSENVC ();
    void        ExecuteSENZ ();
    void        ExecuteSENZC ();
    void        ExecuteSENZV ();
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
	void		ExecuteMUL ();
	void		ExecuteDIV ();
	void		ExecuteASH ();
	void		ExecuteASHC ();

    // Four fields
    void        ExecuteMOV ();
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
}
inline void CProcessor::SetV (BOOL bFlag)
{
    if (bFlag) m_psw |= PSW_V; else m_psw &= ~PSW_V;
}
inline void CProcessor::SetN (BOOL bFlag)
{
    if (bFlag) m_psw |= PSW_N; else m_psw &= ~PSW_N;
}
inline void CProcessor::SetZ (BOOL bFlag)
{
    if (bFlag) m_psw |= PSW_Z; else m_psw &= ~PSW_Z;
}

// PSW bits calculations - implementation
inline BOOL CProcessor::CheckAddForOverflow (BYTE a, BYTE b)
{
    //WORD sum = a < 0200 ? (WORD)a + (WORD)b + 0200 : (WORD)a + (WORD)b - 0200;
    //return HIBYTE (sum) != 0;

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
}
inline BOOL CProcessor::CheckAddForOverflow (WORD a, WORD b)
{
    //DWORD sum =  a < 0100000 ? (DWORD)a + (DWORD)b + 0100000 : (DWORD)a + (DWORD)b - 0100000;
    //return HIWORD (sum) != 0;

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
}
//void        CProcessor::SetReg(int regno, WORD word) 

inline BOOL CProcessor::CheckSubForOverflow (BYTE a, BYTE b)
{
    //WORD sum = a < 0200 ? (WORD)a - (WORD)b + 0200 : (WORD)a - (WORD)b - 0200;
    //return HIBYTE (sum) != 0;

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
}
inline BOOL CProcessor::CheckSubForOverflow (WORD a, WORD b)
{
    //DWORD sum =  a < 0100000 ? (DWORD)a - (DWORD)b + 0100000 : (DWORD)a - (DWORD)b - 0100000;
    //return HIWORD (sum) != 0;

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
