/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// ProcessorSystemTests.cpp : T-bit trace trap, RTI / RTT, WAIT and RESET of the KM1801VM2 (CPU, USER mode)
//
// Expected values come from the PDP-11 / KM1801VM2 descriptions of the instructions, not from the
// emulator output:
//   - the T-bit (bit 4 of the PSW) causes the trap through the vector 014 after the instruction is executed,
//     the pushed PC is the address of the next instruction;
//   - RTI that restores the PSW with the T-bit set traps at once, before the next instruction;
//     RTT does not: the next instruction is executed first;
//   - WAIT stops the processor until an interrupt; the trace trap does not wake it up;
//   - RESET initializes the I/O devices and cancels the pending line clock request.

#include "stdafx.h"
#include <CppUnitTest.h>
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace
{

const uint16_t PROG = 01000;
const uint16_t HANDLER = 02000;
const uint16_t STACK = 0400;

const uint16_t TBIT = 020;         // the trace bit
const uint16_t PRIORITY_BIT = 0200; // interrupts of the lower priority are blocked

const uint16_t OP_INC_R0 = 0005200;
const uint16_t OP_RTI = 0000002;
const uint16_t OP_RTT = 0000006;
const uint16_t OP_WAIT = 0000001;
const uint16_t OP_RESET = 0000005;

// The program at PROG, SP = STACK, registers zero
void Prepare(TestBoard& tb, const uint16_t* program, size_t count)
{
    uint16_t regs[8] = { 0, 0, 0, 0, 0, 0, STACK, 0 };
    tb.SetRegs(regs);
    tb.LoadProgram(PROG, program, count);
}

// The trace trap vector 014 leads to the HANDLER with the PSW 0
void SetTraceVector(TestBoard& tb)
{
    tb.SetWord(014, HANDLER);
    tb.SetWord(016, 0);
}

}

TEST_CLASS(ProcessorSystemTests)
{
public:
    // T-bit

    TEST_METHOD(Processor_Tbit_TrapsAfterTheInstruction)
    {
        TestBoard tb;
        const uint16_t program[] = { OP_INC_R0, OP_INC_R0 };
        Prepare(tb, program, 2);
        SetTraceVector(tb);
        tb.CPU()->SetPSW(TBIT);

        tb.Run();
        Eq(1, tb.CPU()->GetReg(0), _T("the traced instruction is executed"));
        Assert::IsTrue(tb.TakeInterrupt(), _T("trace trap"));

        Eq(HANDLER, tb.CPU()->GetPC(), _T("PC = handler"));
        Eq(0, tb.CPU()->GetPSW(), _T("PSW from the vector: T-bit is cleared"));
        Eq(STACK - 4, tb.CPU()->GetSP(), _T("SP"));
        Eq(PROG + 2, tb.GetWord(STACK - 4), _T("the pushed PC is the next instruction"));
        Eq(TBIT, tb.GetWord(STACK - 2), _T("the pushed PSW has the T-bit"));
        Eq(1, tb.CPU()->GetReg(0), _T("the next instruction is not executed yet"));
    }

    TEST_METHOD(Processor_Tbit_NoTrapWithoutTheBit)
    {
        TestBoard tb;
        const uint16_t program[] = { OP_INC_R0 };
        Prepare(tb, program, 1);
        SetTraceVector(tb);

        tb.Run();
        Assert::IsFalse(tb.TakeInterrupt(), _T("no trap"));
        Eq(PROG + 2, tb.CPU()->GetPC(), _T("PC"));
    }

    TEST_METHOD(Processor_Tbit_TrapsAfterEveryInstruction)
    {
        // The handler returns with RTT, which lets one instruction run: INC, trap, RTT, INC, trap, ...
        TestBoard tb;
        const uint16_t program[] = { OP_INC_R0, OP_INC_R0, OP_INC_R0 };
        Prepare(tb, program, 3);
        SetTraceVector(tb);
        tb.SetWord(HANDLER, OP_RTT);
        tb.CPU()->SetPSW(TBIT);

        for (int n = 1; n <= 3; n++)
        {
            tb.Run();  // the traced instruction
            Eq(n, tb.CPU()->GetReg(0), _T("R0"));
            Assert::IsTrue(tb.TakeInterrupt(), _T("trace trap after the instruction"));
            Eq(PROG + n * 2, tb.GetWord(STACK - 4), _T("the pushed PC"));
            Eq(HANDLER, tb.CPU()->GetPC(), _T("PC = handler"));

            tb.Run();  // RTT
            Eq(PROG + n * 2, tb.CPU()->GetPC(), _T("back to the program"));
            Eq(TBIT, tb.CPU()->GetPSW(), _T("the T-bit is restored"));
            Eq(STACK, tb.CPU()->GetSP(), _T("SP"));
            Assert::IsFalse(tb.TakeInterrupt(), _T("RTT inhibits the trap before the next instruction"));
        }
    }

    TEST_METHOD(Processor_Tbit_RtiTrapsBeforeTheNextInstruction)
    {
        // RTI restores the PSW with the T-bit set and the trace trap occurs at once
        TestBoard tb;
        const uint16_t program[] = { OP_INC_R0, OP_INC_R0 };
        Prepare(tb, program, 2);
        SetTraceVector(tb);
        tb.SetWord(HANDLER, OP_RTI);
        tb.CPU()->SetSP(STACK - 4);
        tb.SetWord(STACK - 4, PROG);   // the return address
        tb.SetWord(STACK - 2, TBIT);  // the PSW to restore
        tb.CPU()->SetPC(HANDLER);

        tb.Run();  // RTI
        Eq(PROG, tb.CPU()->GetPC(), _T("PC after RTI"));
        Eq(TBIT, tb.CPU()->GetPSW(), _T("the T-bit is restored"));
        Assert::IsTrue(tb.TakeInterrupt(), _T("trace trap before the instruction at PROG"));

        Eq(0, tb.CPU()->GetReg(0), _T("the instruction at PROG is not executed"));
        Eq(PROG, tb.GetWord(STACK - 4), _T("the pushed PC is the return address"));
        Eq(HANDLER, tb.CPU()->GetPC(), _T("PC = handler"));
    }

    TEST_METHOD(Processor_Tbit_RttRunsOneInstructionFirst)
    {
        // RTT restores the same PSW, but the trap waits until the next instruction is executed
        TestBoard tb;
        const uint16_t program[] = { OP_INC_R0, OP_INC_R0 };
        Prepare(tb, program, 2);
        SetTraceVector(tb);
        tb.SetWord(HANDLER, OP_RTT);
        tb.CPU()->SetSP(STACK - 4);
        tb.SetWord(STACK - 4, PROG);
        tb.SetWord(STACK - 2, TBIT);
        tb.CPU()->SetPC(HANDLER);

        tb.Run();  // RTT
        Eq(PROG, tb.CPU()->GetPC(), _T("PC after RTT"));
        Eq(TBIT, tb.CPU()->GetPSW(), _T("the T-bit is restored"));
        Assert::IsFalse(tb.TakeInterrupt(), _T("no trap right after RTT"));

        tb.Run();  // the instruction at PROG
        Eq(1, tb.CPU()->GetReg(0), _T("the instruction at PROG is executed"));
        Assert::IsTrue(tb.TakeInterrupt(), _T("trace trap after it"));
        Eq(PROG + 2, tb.GetWord(STACK - 4), _T("the pushed PC is the next instruction"));
    }

    TEST_METHOD(Processor_Tbit_ReturnWithoutTheBitDoesNotTrap)
    {
        // RTI and RTT with the PSW without the T-bit: no trap after them
        static const uint16_t opcodes[] = { OP_RTI, OP_RTT };
        for (size_t i = 0; i < 2; i++)
        {
            TestBoard tb;
            const uint16_t program[] = { OP_INC_R0 };
            Prepare(tb, program, 1);
            SetTraceVector(tb);
            tb.SetWord(HANDLER, opcodes[i]);
            tb.CPU()->SetSP(STACK - 4);
            tb.SetWord(STACK - 4, PROG);
            tb.SetWord(STACK - 2, 0);
            tb.CPU()->SetPC(HANDLER);

            tb.Run();
            Eq(PROG, tb.CPU()->GetPC(), _T("PC after the return"));
            Assert::IsFalse(tb.TakeInterrupt(), _T("no trap"));
        }
    }

    TEST_METHOD(Processor_Tbit_FlagsAreKeptInThePushedPsw)
    {
        TestBoard tb;
        const uint16_t program[] = { OP_INC_R0 };
        Prepare(tb, program, 1);
        SetTraceVector(tb);
        tb.CPU()->SetPSW(TBIT | TF_C);  // C is not changed by INC

        tb.Run();
        Assert::IsTrue(tb.TakeInterrupt());
        Eq(TBIT | TF_C, tb.GetWord(STACK - 2), _T("T and C in the pushed PSW"));
    }

    TEST_METHOD(Processor_Tbit_TrapInstructionGoesThroughItsOwnVector)
    {
        // EMT with the T-bit: the EMT trap has the priority, vector 030; the pushed PSW has the T-bit
        TestBoard tb;
        const uint16_t program[] = { 0104001 };  // EMT 1
        Prepare(tb, program, 1);
        SetTraceVector(tb);
        tb.SetWord(030, 03000);
        tb.SetWord(032, 0);
        tb.CPU()->SetPSW(TBIT);

        tb.Run();
        Eq(03000, tb.CPU()->GetPC(), _T("PC = EMT handler"));
        Eq(TBIT, tb.GetWord(STACK - 2), _T("the pushed PSW"));
    }

    // WAIT

    TEST_METHOD(Processor_Wait_StopsTheInstructionFlow)
    {
        TestBoard tb;
        const uint16_t program[] = { OP_WAIT, OP_INC_R0 };
        Prepare(tb, program, 2);

        tb.Run();
        Eq(PROG + 2, tb.CPU()->GetPC(), _T("PC after WAIT"));
        tb.Run(5);
        Eq(PROG + 2, tb.CPU()->GetPC(), _T("PC does not move"));
        Eq(0, tb.CPU()->GetReg(0), _T("the next instruction is not executed"));
        Assert::IsFalse(tb.TakeInterrupt(), _T("nothing wakes the processor up"));
    }

    TEST_METHOD(Processor_Wait_InterruptWakesTheProcessorUp)
    {
        TestBoard tb;
        tb.StartCPU();
        const uint16_t program[] = { OP_WAIT, OP_INC_R0 };
        Prepare(tb, program, 2);
        tb.SetWord(060, HANDLER);
        tb.SetWord(062, 0);
        tb.SetWord(HANDLER, OP_RTI);

        tb.Run();
        tb.Run(3);
        Eq(0, tb.CPU()->GetReg(0), _T("waiting"));

        tb.CPU()->InterruptVIRQ(1, 060);
        Assert::IsTrue(tb.TakeInterrupt(), _T("the interrupt is taken"));
        Eq(HANDLER, tb.CPU()->GetPC(), _T("PC = handler"));
        Eq(PROG + 2, tb.GetWord(STACK - 4), _T("the pushed PC is the address after WAIT"));

        tb.Run();  // RTI of the handler
        Eq(PROG + 2, tb.CPU()->GetPC(), _T("back after WAIT"));
        tb.Run();  // INC R0: the processor runs again
        Eq(1, tb.CPU()->GetReg(0), _T("the instruction after WAIT is executed"));
    }

    TEST_METHOD(Processor_Wait_BlockedInterruptDoesNotWakeTheProcessorUp)
    {
        // The bit 7 of the PSW blocks the interrupts of the devices
        TestBoard tb;
        tb.StartCPU();
        const uint16_t program[] = { OP_WAIT, OP_INC_R0 };
        Prepare(tb, program, 2);
        tb.SetWord(060, HANDLER);
        tb.SetWord(062, 0);
        tb.CPU()->SetPSW(PRIORITY_BIT);

        tb.Run();
        tb.CPU()->InterruptVIRQ(1, 060);
        Assert::IsFalse(tb.TakeInterrupt(), _T("the interrupt is blocked"));
        tb.Run(3);
        Eq(PROG + 2, tb.CPU()->GetPC(), _T("still waiting"));
        Eq(0, tb.CPU()->GetReg(0), _T("the next instruction is not executed"));
    }

    TEST_METHOD(Processor_Wait_TraceTrapDoesNotWakeTheProcessorUp)
    {
        TestBoard tb;
        const uint16_t program[] = { OP_WAIT, OP_INC_R0 };
        Prepare(tb, program, 2);
        SetTraceVector(tb);
        tb.CPU()->SetPSW(TBIT);

        tb.Run();
        Assert::IsFalse(tb.TakeInterrupt(), _T("no trace trap while waiting"));
        tb.Run(3);
        Assert::IsFalse(tb.TakeInterrupt(), _T("and after more cycles"));
        Eq(0, tb.CPU()->GetReg(0), _T("the next instruction is not executed"));
    }

    // RESET

    TEST_METHOD(Processor_Reset_TakesItsTime)
    {
        // The KM1801VM2 technical description: RESET takes 105 + 968 ticks
        TestBoard tb;
        const uint16_t program[] = { OP_RESET };
        Prepare(tb, program, 1);
        tb.Run();
        Eq(PROG + 2, tb.CPU()->GetPC(), _T("PC"));
        Eq(105 + 968, tb.CPU()->GetInternalTick(), _T("execution time"));
    }

    TEST_METHOD(Processor_Reset_InitializesTheChannels)
    {
        TestBoard tb;
        CMotherboard* pBoard = tb.Board();
        const uint16_t program[] = { OP_RESET };
        Prepare(tb, program, 1);

        // a byte for the PPU is in the channel, the CPU transmitter is busy
        pBoard->ChanResetByCPU();
        pBoard->ChanResetByPPU();
        pBoard->ChanWriteByCPU(0, 0x55);
        Eq(0, pBoard->ChanTxStateGetCPU(0), _T("CPU transmitter is busy before RESET"));

        tb.Run();

        for (int ch = 0; ch < 3; ch++)
            Eq(0200, pBoard->ChanTxStateGetCPU(ch), _T("CPU transmitter is ready"));
        for (int ch = 0; ch < 2; ch++)
            Eq(0, pBoard->ChanRxStateGetCPU(ch), _T("CPU receiver is empty"));
    }

    TEST_METHOD(Processor_Reset_InitializesTheSerialPorts)
    {
        TestBoard tb;
        CMemoryController* pCtl = tb.Board()->GetCPUMemoryController();
        const uint16_t program[] = { OP_RESET };
        Prepare(tb, program, 1);

        pCtl->SetWord(0176570, false, 0100);     // receiver interrupt enable
        tb.CpuMem()->SerialInput(0x41);          // a byte is received
        pCtl->SetWord(0176576, false, 'A');      // a byte is sent, the transmitter is busy
        Assert::IsTrue((pCtl->GetWord(0176570, false, false) & 0200) != 0, _T("receiver is ready before RESET"));
        Eq(0, pCtl->GetWord(0176574, false, false) & 0200, _T("transmitter is busy before RESET"));

        tb.Run();

        Eq(0, pCtl->GetWord(0176570, false, false), _T("receiver status is cleared"));
        Eq(0200, pCtl->GetWord(0176574, false, false), _T("transmitter is ready"));
    }

    TEST_METHOD(Processor_Reset_CancelsThePendingLineClockRequest)
    {
        TestBoard tb;
        tb.StartCPU();
        tb.SetPpuWord(0177054, 01);  // unmask the 50 Hz event
        const uint16_t program[] = { OP_RESET };
        Prepare(tb, program, 1);
        tb.SetWord(0100, HANDLER);
        tb.SetWord(0102, 0);
        tb.CPU()->SetPSW(PRIORITY_BIT);  // the request waits, the priority blocks it

        tb.Board()->Tick50();
        tb.Run();  // RESET
        tb.CPU()->SetPSW(0);

        Assert::IsFalse(tb.TakeInterrupt(), _T("the line clock request is gone"));
    }

    TEST_METHOD(Processor_Reset_LineClockRequestStaysPendingWithoutReset)
    {
        // The control for the previous test: without RESET the request is taken as soon as the priority allows
        TestBoard tb;
        tb.StartCPU();
        tb.SetPpuWord(0177054, 01);
        const uint16_t program[] = { OP_INC_R0 };
        Prepare(tb, program, 1);
        tb.SetWord(0100, HANDLER);
        tb.SetWord(0102, 0);
        tb.CPU()->SetPSW(PRIORITY_BIT);

        tb.Board()->Tick50();
        tb.Run();
        tb.CPU()->SetPSW(0);

        Assert::IsTrue(tb.TakeInterrupt(), _T("the line clock request is taken"));
        Eq(HANDLER, tb.CPU()->GetPC(), _T("PC = handler"));
    }
};
