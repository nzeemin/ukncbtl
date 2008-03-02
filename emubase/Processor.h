// Processor.h
//

#pragma once

#include "Defines.h"
#include "Memory.h"


#define MAXEVTQUEUE 2000

class CMemoryController;

//////////////////////////////////////////////////////////////////////

class CProcessor  // KM1801VM2 processor
{

public:  // Constructor / initialization
                CProcessor(TCHAR * name);
    void        AttachMemoryController(CMemoryController* ctl) { m_pMemoryController = ctl; }
    void        AssertHALT();
	void		DeassertHALT();
	void		MemoryError();
    LPCTSTR     GetName() const { return NAME; }
	void        SetInternalTick (WORD tick) {m_internalTick = tick; }

protected:  // Processor state
	TCHAR       NAME[5];            // Processor name
    WORD        m_internalTick;     // How many ticks waiting to the end of current instruction
    WORD        m_psw;              // Processor Status Word (PSW)
    WORD        m_R[8];             // Registers (R0..R5, R6=SP, R7=PC)
    BOOL        m_okStopped;        // "Processor stopped" flag
    WORD        m_savepc;
    WORD        m_savepsw;

protected:  // Current instruction processing
    WORD        m_instruction;      // Curent instruction
    int         m_regsrc;           // Source register number
    int         m_methsrc;          // Source address mode
    WORD        m_addrsrc;          // Source address
    int         m_regdest;          // Destination register number
    int         m_methdest;         // Destination address mode
    WORD        m_addrdest;         // Destination address
    WORD        m_eventqueue[MAXEVTQUEUE];  // Event queue
    int         m_eqreadptr;        // Event queue read pointer
    int         m_eqwriteptr;       // Event queue write pointer
    int         m_eqcount;          // Event queue count
    int         m_traprq;           // trap pending
    WORD        m_trap;             // trap vector
	int			m_virqrq;			// VIRQ pending
	int			m_evntrq;
	int			m_ACLOrq;
	WORD		m_virq[16];				// VIRQ vector
    int         m_userspace;        // Read 1 if user space is used -- cpu is accessing I/O from HALT mode using user space
    int         m_stepmode;         // Read 1 if it's step mode
	int			m_haltpin;			// HALT 
	int			m_waitmode;			// WAIT
    
protected:
    CMemoryController* m_pMemoryController;

public:
    CMemoryController* GetMemoryController() { return m_pMemoryController; }

public:  // Register control
    WORD        GetPSW() { return m_psw; }
    WORD        GetCPSW() { return m_savepsw; }
    void        SetPSW(WORD word) { m_psw = word; }
	//{ 
	//	ASSERT(word<0777);
	//	m_psw = word; 
	//
	//}
    WORD        GetReg(int regno) { return m_R[regno]; }
    void        SetReg(int regno, WORD word) { m_R[regno] = word; }
//	{
//		
//		if(regno==7)
//		{
//			TCHAR buffer[40];
//
//			wsprintf(buffer,_T("%s "),NAME);
//			//DebugLog(buffer);
//			PrintOctalValue(buffer, GetPC()-2);
//			//DebugLog(buffer);
//		}
//		ASSERT((regno!=7)||((word&1)==0)); // it have to be word alined
////		ASSERT((regno!=7)||(word>=0400));  // catch PC=0 or something rediculus
////		ASSERT((regno!=7)||(word!=0133526));// catch the bug
//		m_R[regno] = word;
//		if(regno==7)
//		{
//			TCHAR buffer[40];
//			//DebugLog(_T(":="));
//			PrintOctalValue(buffer, GetPC());
//			//DebugLog(buffer);
//			//DebugLog(_T("\r\n"));
//		}
//
//	
//	}
    WORD        GetSP() const { return m_R[6]; }
    void        SetSP(WORD word) { m_R[6] = word; }
    WORD        GetPC() const { return m_R[7]; }
    WORD        GetCPC() const { return m_savepc; }
    void        SetPC(WORD word) { m_R[7] = word; }
	//{ 
	//	ASSERT(!(word&1));
	//	//ASSERT((word!=0163016)||(m_R[4]!=0145512));
	//	//ASSERT(word!=0133526);// catch the bug //corrupting PSW
	//	/*ASSERT(word>0400);*/ m_R[7] = word; 
	//
	//}
public:  // PSW bits control
    void        SetC(BOOL bFlag);
    WORD        GetC() const { return (m_psw & PSW_C) != 0; }
    void        SetV(BOOL bFlag);
    WORD        GetV() const { return (m_psw & PSW_V) != 0; }
    void        SetN(BOOL bFlag);
    WORD        GetN() const { return (m_psw & PSW_N) != 0; }
    void        SetZ(BOOL bFlag);
    WORD        GetZ() const { return (m_psw & PSW_Z) != 0; }

public:  // Processor state
    // "Processor stopped" flag
    BOOL        IsStopped() const { return m_okStopped; }
    // HALT flag (TRUE - HALT mode, FALSE - USER mode)
    BOOL        IsHaltMode() 
	{ 
			BOOL mode;
			mode=((m_psw & 0x100) != 0); 
			if(mode)
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
    void        TranslateInstruction();  // Execute one instruction
    void        QueueInterrupt(WORD interrupt, int priority);  // ѕоместить прерывание в очередь прерываний
    void        MakeInterrupt (WORD interrupt);  // Process the interrupt
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

