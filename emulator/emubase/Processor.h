/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

/// \file Processor.h  KM1801VM2 processor class

#pragma once

#include "Defines.h"
#include "Memory.h"


class CMemoryController;

//////////////////////////////////////////////////////////////////////

/// \brief KM1801VM2 processor
class CProcessor
{
public:  // Constructor / initialization
    CProcessor(LPCTSTR name);
    /// \brief Link the processor and memory controller
    void        AttachMemoryController(CMemoryController* ctl) { m_pMemoryController = ctl; }
    void        SetHALTPin(bool value) { m_haltpin = value; }
    void        SetDCLOPin(bool value);
    void        SetACLOPin(bool value);
    void        MemoryError();
    /// \brief Get the processor name, assigned in the constructor
    LPCTSTR     GetName() const { return m_name; }

public:
    static void Init();  ///< Initialize static tables
    static void Done();  ///< Release memory used for static tables
protected:  // Statics
    typedef void ( CProcessor::*ExecuteMethodRef )();
    static ExecuteMethodRef* m_pExecuteMethodMap;

protected:  // Processor state
    TCHAR       m_name[5];          ///< Processor name (DO NOT use it inside the processor code!!!)
    uint16_t    m_internalTick;     ///< How many ticks waiting to the end of current instruction
    uint16_t    m_psw;              ///< Processor Status Word (PSW)
    uint16_t    m_R[8];             ///< Registers (R0..R5, R6=SP, R7=PC)
    uint16_t    m_savepc;           ///< CPC register
    uint16_t    m_savepsw;          ///< CPSW register
    bool        m_okStopped;        ///< "Processor stopped" flag
    bool        m_stepmode;         ///< Read true if it's step mode
    bool        m_buserror;         ///< Read true if occured bus error for implementing double bus error if needed
    bool        m_haltpin;          ///< HALT pin
    bool        m_DCLOpin;          ///< DCLO pin
    bool        m_ACLOpin;          ///< ACLO pin
    bool        m_waitmode;         ///< WAIT

protected:  // Current instruction processing
    uint16_t    m_instruction;      ///< Curent instruction
    uint16_t    m_instructionpc;    ///< Address of the current instruction
    uint8_t     m_regsrc;           ///< Source register number
    uint8_t     m_methsrc;          ///< Source address mode
    uint16_t    m_addrsrc;          ///< Source address
    uint8_t     m_regdest;          ///< Destination register number
    uint8_t     m_methdest;         ///< Destination address mode
    uint16_t    m_addrdest;         ///< Destination address
protected:  // Interrupt processing
    bool        m_STRTrq;           ///< Start interrupt pending
    bool        m_RPLYrq;           ///< Hangup interrupt pending
    bool        m_ILLGrq;           ///< Illegal instruction interrupt pending
    bool        m_RSVDrq;           ///< Reserved instruction interrupt pending
    bool        m_TBITrq;           ///< T-bit interrupt pending
    bool        m_ACLOrq;           ///< Power down interrupt pending
    bool        m_HALTrq;           ///< HALT command or HALT signal
    bool        m_EVNTrq;           ///< Timer event interrupt pending
    bool        m_FIS_rq;           ///< FIS command interrupt pending
    bool        m_BPT_rq;           ///< BPT command interrupt pending
    bool        m_IOT_rq;           ///< IOT command interrupt pending
    bool        m_EMT_rq;           ///< EMT command interrupt pending
    bool        m_TRAPrq;           ///< TRAP command interrupt pending
    uint16_t    m_virq[16];         ///< VIRQ vector
    bool        m_ACLOreset;        ///< Power fail interrupt request reset
    bool        m_EVNTreset;        ///< EVNT interrupt request reset;
    uint8_t     m_VIRQreset;        ///< VIRQ request reset for given device
protected:
    CMemoryController* m_pMemoryController;
    bool m_okTrace;                 ///< Trace mode on/off

public:
    CMemoryController* GetMemoryController() { return m_pMemoryController; }
    const CMemoryController* GetMemoryControllerConst() const { return m_pMemoryController; }

public:  // Register control
    uint16_t    GetPSW() const { return m_psw; }  ///< Get the processor status word register value
    uint16_t    GetCPSW() const { return m_savepsw; }
    uint8_t     GetLPSW() const { return (uint8_t)(m_psw & 0xff); }  ///< Get PSW lower byte
    void        SetPSW(uint16_t word);  ///< Set the processor status word register value
    void        SetCPSW(uint16_t word) { m_savepsw = word; }
    void        SetLPSW(uint8_t byte);
    uint16_t    GetReg(int regno) const { return m_R[regno]; }  ///< Get register value, regno=0..7
    void        SetReg(int regno, uint16_t word);  ///< Set register value
    uint8_t     GetLReg(int regno) const { return (uint8_t)(m_R[regno] & 0xff); }
    void        SetLReg(int regno, uint8_t byte);
    uint16_t    GetSP() const { return m_R[6]; }
    void        SetSP(uint16_t word) { m_R[6] = word; }
    uint16_t    GetPC() const { return m_R[7]; }
    uint16_t    GetCPC() const { return m_savepc; }
    void        SetPC(uint16_t word);
    void        SetCPC(uint16_t word) { m_savepc = word; }

public:  // PSW bits control
    void        SetC(bool bFlag);
    uint16_t    GetC() const { return (m_psw & PSW_C) != 0; }
    void        SetV(bool bFlag);
    uint16_t    GetV() const { return (m_psw & PSW_V) != 0; }
    void        SetN(bool bFlag);
    uint16_t    GetN() const { return (m_psw & PSW_N) != 0; }
    void        SetZ(bool bFlag);
    uint16_t    GetZ() const { return (m_psw & PSW_Z) != 0; }
    void        SetHALT(bool bFlag);
    uint16_t    GetHALT() const { return (m_psw & PSW_HALT) != 0; }

public:  // Processor state
    /// \brief "Processor stopped" flag
    bool        IsStopped() const { return m_okStopped; }
    /// \brief HALT flag (true - HALT mode, false - USER mode)
    bool        IsHaltMode() const { return ((m_psw & 0400) != 0); }
public:  // Processor control
    void        TickEVNT();  ///< EVNT signal
    /// \brief External interrupt via VIRQ signal
    void        InterruptVIRQ(int que, uint16_t interrupt);
    uint16_t    GetVIRQ(int que) { return m_virq[que]; }
    /// \brief Execute one processor tick
    void        Execute();
    /// \brief Process pending interrupt requests
    bool        InterruptProcessing();
    /// \brief Execute next command and process interrupts
    void        CommandExecution();
    int         GetInternalTick() const { return m_internalTick; }
    void        ClearInternalTick() { m_internalTick = 0; }
    void        SetTrace(bool okTrace) { m_okTrace = okTrace; }  ///< Set trace mode on/off

public:  // Saving/loading emulator status (pImage addresses up to 32 bytes)
    void        SaveToImage(uint8_t* pImage) const;
    void        LoadFromImage(const uint8_t* pImage);

protected:  // Implementation
    void        FetchInstruction();      ///< Read next instruction
    void        TranslateInstruction();  ///< Execute the instruction
protected:  // Implementation - memory access
    /// \brief Read word from the bus for execution
    uint16_t    GetWordExec(uint16_t address) { return m_pMemoryController->GetWord(address, IsHaltMode(), true); }
    /// \brief Read word from the bus
    uint16_t    GetWord(uint16_t address) { return m_pMemoryController->GetWord(address, IsHaltMode(), false); }
    void        SetWord(uint16_t address, uint16_t word) { m_pMemoryController->SetWord(address, IsHaltMode(), word); }
    uint8_t     GetByte(uint16_t address) { return m_pMemoryController->GetByte(address, IsHaltMode()); }
    void        SetByte(uint16_t address, uint8_t byte) { m_pMemoryController->SetByte(address, IsHaltMode(), byte); }

protected:  // PSW bits calculations
    bool static CheckForNegative(uint8_t byte) { return (byte & 0200) != 0; }
    bool static CheckForNegative(uint16_t word) { return (word & 0100000) != 0; }
    bool static CheckForZero(uint8_t byte) { return byte == 0; }
    bool static CheckForZero(uint16_t word) { return word == 0; }
    bool static CheckAddForOverflow(uint8_t a, uint8_t b);
    bool static CheckAddForOverflow(uint16_t a, uint16_t b);
    bool static CheckSubForOverflow(uint8_t a, uint8_t b);
    bool static CheckSubForOverflow(uint16_t a, uint16_t b);
    bool static CheckAddForCarry(uint8_t a, uint8_t b);
    bool static CheckAddForCarry(uint16_t a, uint16_t b);
    bool static CheckSubForCarry(uint8_t a, uint8_t b);
    bool static CheckSubForCarry(uint16_t a, uint16_t b);

protected:  // Implementation - instruction execution
    // No fields
    uint16_t    GetWordAddr (uint8_t meth, uint8_t reg);
    uint16_t    GetByteAddr (uint8_t meth, uint8_t reg);

    void        ExecuteUNKNOWN ();  ///< There is no such instruction -- just call TRAP 10
    void        ExecuteHALT ();
    void        ExecuteWAIT ();
    void        ExecuteRCPC();
    void        ExecuteRCPS ();
    void        ExecuteWCPC();
    void        ExecuteWCPS();
    void        ExecuteMFUS ();
    void        ExecuteMTUS ();
    void        ExecuteRTI ();
    void        ExecuteBPT ();
    void        ExecuteIOT ();
    void        ExecuteRESET ();
    void        ExecuteSTEP();
    void        ExecuteRSEL ();
    void        Execute000030 ();
    void        ExecuteFIS ();
    void        ExecuteRUN();
    void        ExecuteRTT ();
    void        ExecuteCCC ();
    void        ExecuteSCC ();

    // One fiels
    void        ExecuteRTS ();

    // Two fields
    void        ExecuteJMP ();
    void        ExecuteSWAB ();
    void        ExecuteCLR ();
    void        ExecuteCLRB ();
    void        ExecuteCOM ();
    void        ExecuteCOMB ();
    void        ExecuteINC ();
    void        ExecuteINCB ();
    void        ExecuteDEC ();
    void        ExecuteDECB ();
    void        ExecuteNEG ();
    void        ExecuteNEGB ();
    void        ExecuteADC ();
    void        ExecuteADCB ();
    void        ExecuteSBC ();
    void        ExecuteSBCB ();
    void        ExecuteTST ();
    void        ExecuteTSTB ();
    void        ExecuteROR ();
    void        ExecuteRORB ();
    void        ExecuteROL ();
    void        ExecuteROLB ();
    void        ExecuteASR ();
    void        ExecuteASRB ();
    void        ExecuteASL ();
    void        ExecuteASLB ();
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
    void        ExecuteCMPB ();
    void        ExecuteBIT ();
    void        ExecuteBITB ();
    void        ExecuteBIC ();
    void        ExecuteBICB ();
    void        ExecuteBIS ();
    void        ExecuteBISB ();

    void        ExecuteADD ();
    void        ExecuteSUB ();
};

inline void CProcessor::SetPSW(uint16_t word)
{
    m_psw = word & 0777;
    if ((m_psw & 0600) != 0600) m_savepsw = m_psw;
}
inline void CProcessor::SetLPSW(uint8_t byte)
{
    m_psw = (m_psw & 0xFF00) | (uint16_t)byte;
    if ((m_psw & 0600) != 0600) m_savepsw = m_psw;
}
inline void CProcessor::SetReg(int regno, uint16_t word)
{
    m_R[regno] = word;
    if ((regno == 7) && ((m_psw & 0600) != 0600)) m_savepc = word;
}
inline void CProcessor::SetLReg(int regno, uint8_t byte)
{
    m_R[regno] = (m_R[regno] & 0xFF00) | (uint16_t)byte;
    if ((regno == 7) && ((m_psw & 0600) != 0600)) m_savepc = m_R[7];
}
inline void CProcessor::SetPC(uint16_t word)
{
    m_R[7] = word;
    if ((m_psw & 0600) != 0600) m_savepc = word;
}

// PSW bits control - implementation
inline void CProcessor::SetC (bool bFlag)
{
    if (bFlag) m_psw |= PSW_C; else m_psw &= ~PSW_C;
    if ((m_psw & 0600) != 0600) m_savepsw = m_psw;
}
inline void CProcessor::SetV (bool bFlag)
{
    if (bFlag) m_psw |= PSW_V; else m_psw &= ~PSW_V;
    if ((m_psw & 0600) != 0600) m_savepsw = m_psw;
}
inline void CProcessor::SetN (bool bFlag)
{
    if (bFlag) m_psw |= PSW_N; else m_psw &= ~PSW_N;
    if ((m_psw & 0600) != 0600) m_savepsw = m_psw;
}
inline void CProcessor::SetZ (bool bFlag)
{
    if (bFlag) m_psw |= PSW_Z; else m_psw &= ~PSW_Z;
    if ((m_psw & 0600) != 0600) m_savepsw = m_psw;
}

inline void CProcessor::SetHALT (bool bFlag)
{
    if (bFlag) m_psw |= PSW_HALT; else m_psw &= ~PSW_HALT;
}

inline void CProcessor::InterruptVIRQ(int que, uint16_t interrupt)
{
    if (m_okStopped) return;  // Processor is stopped - nothing to do
    m_virq[que] = interrupt;
}

// PSW bits calculations - implementation
inline bool CProcessor::CheckAddForOverflow (uint8_t a, uint8_t b)
{
#if defined(_M_IX86) && defined(_MSC_VER) && !defined(_MANAGED)
    bool bOverflow = false;
    _asm
    {
        pushf
        push cx
        mov cl, byte ptr [a]
        add cl, byte ptr [b]
        jno end
        mov dword ptr [bOverflow], 1
        end:
        pop cx
        popf
    }
    return bOverflow;
#else
    //uint16_t sum = a < 0200 ? (uint16_t)a + (uint16_t)b + 0200 : (uint16_t)a + (uint16_t)b - 0200;
    //return HIBYTE (sum) != 0;
    uint8_t sum = a + b;
    return ((~a ^ b) & (a ^ sum)) & 0200;
#endif
}
inline bool CProcessor::CheckAddForOverflow (uint16_t a, uint16_t b)
{
#if defined(_M_IX86) && defined(_MSC_VER) && !defined(_MANAGED)
    bool bOverflow = false;
    _asm
    {
        pushf
        push cx
        mov cx, word ptr [a]
        add cx, word ptr [b]
        jno end
        mov dword ptr [bOverflow], 1
        end:
        pop cx
        popf
    }
    return bOverflow;
#else
    //uint32_t sum =  a < 0100000 ? (uint32_t)a + (uint32_t)b + 0100000 : (uint32_t)a + (uint32_t)b - 0100000;
    //return HIWORD (sum) != 0;
    uint16_t sum = a + b;
    return ((~a ^ b) & (a ^ sum)) & 0100000;
#endif
}

inline bool CProcessor::CheckSubForOverflow (uint8_t a, uint8_t b)
{
#if defined(_M_IX86) && defined(_MSC_VER) && !defined(_MANAGED)
    bool bOverflow = false;
    _asm
    {
        pushf
        push cx
        mov cl, byte ptr [a]
        sub cl, byte ptr [b]
        jno end
        mov dword ptr [bOverflow], 1
        end:
        pop cx
        popf
    }
    return bOverflow;
#else
    //uint16_t sum = a < 0200 ? (uint16_t)a - (uint16_t)b + 0200 : (uint16_t)a - (uint16_t)b - 0200;
    //return HIBYTE (sum) != 0;
    uint8_t sum = a - b;
    return ((a ^ b) & (~b ^ sum)) & 0200;
#endif
}
inline bool CProcessor::CheckSubForOverflow (uint16_t a, uint16_t b)
{
#if defined(_M_IX86) && defined(_MSC_VER) && !defined(_MANAGED)
    bool bOverflow = false;
    _asm
    {
        pushf
        push cx
        mov cx, word ptr [a]
        sub cx, word ptr [b]
        jno end
        mov dword ptr [bOverflow], 1
        end:
        pop cx
        popf
    }
    return bOverflow;
#else
    //uint32_t sum =  a < 0100000 ? (uint32_t)a - (uint32_t)b + 0100000 : (uint32_t)a - (uint32_t)b - 0100000;
    //return HIWORD (sum) != 0;
    uint16_t sum = a - b;
    return ((a ^ b) & (~b ^ sum)) & 0100000;
#endif
}
inline bool CProcessor::CheckAddForCarry (uint8_t a, uint8_t b)
{
    uint16_t sum = (uint16_t)a + (uint16_t)b;
    return (uint8_t)((sum >> 8) & 0xff) != 0;
}
inline bool CProcessor::CheckAddForCarry (uint16_t a, uint16_t b)
{
    uint32_t sum = (uint32_t)a + (uint32_t)b;
    return (uint16_t)((sum >> 16) & 0xffff) != 0;
}
inline bool CProcessor::CheckSubForCarry (uint8_t a, uint8_t b)
{
    uint16_t sum = (uint16_t)a - (uint16_t)b;
    return (uint8_t)((sum >> 8) & 0xff) != 0;
}
inline bool CProcessor::CheckSubForCarry (uint16_t a, uint16_t b)
{
    uint32_t sum = (uint32_t)a - (uint32_t)b;
    return (uint16_t)((sum >> 16) & 0xffff) != 0;
}


//////////////////////////////////////////////////////////////////////
