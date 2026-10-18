/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// BoardTests.cpp : tests for CMotherboard: creation, reset, memory loading, CPU-PPU channels,
// the timer, the 50 Hz event and the emulator image
//
// Expected values come from the UKNC documentation (vectors, port bits), not from the emulator
// output. Tests of the known bugs are ignored, see the list in docs/unit-tests.md.

#include "stdafx.h"
#include <CppUnitTest.h>
#include <vector>
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace
{

const uint16_t STACK = 0400;

// Run the timer for the given number of 2 microsecond ticks
void TimerTicks(CMotherboard* pBoard, int count)
{
    for (int i = 0; i < count; i++)
        pBoard->TimerTick();
}

}

TEST_CLASS(BoardTests)
{
public:
    // Creation and reset

    TEST_METHOD(Board_CreateAndReset)
    {
        CMotherboard board;
        board.Reset();

        Assert::IsNotNull(board.GetCPU());
        Assert::IsNotNull(board.GetPPU());
        Assert::IsNotNull(board.GetCPUMemoryController());
        Assert::IsNotNull(board.GetPPUMemoryController());
    }

    TEST_METHOD(Board_RamWordRoundTrip)
    {
        CMotherboard board;
        board.Reset();

        for (int plan = 0; plan < 3; plan++)
        {
            board.SetRAMWord(plan, 01000, 0123456);
            Assert::AreEqual(0123456u, (unsigned int) board.GetRAMWord(plan, 01000));
        }
    }

    TEST_METHOD(Board_ResetStartsPpuButNotCpu)
    {
        CMotherboard board;
        board.Reset();
        Assert::IsFalse(board.GetPPU()->IsStopped(), _T("PPU runs"));
        Assert::IsTrue(board.GetCPU()->IsStopped(), _T("CPU waits for the PPU"));
    }

    TEST_METHOD(Board_RamIsCleared)
    {
        CMotherboard board;
        for (int plan = 0; plan < 3; plan++)
        {
            Eq(0, board.GetRAMWord(plan, 0), _T("first word"));
            Eq(0, board.GetRAMWord(plan, 0xFFFE), _T("last word"));
        }
    }

    TEST_METHOD(Board_LoadRamLoads32Kilobytes)
    {
        std::vector<uint8_t> buffer(32768);
        for (size_t i = 0; i < buffer.size(); i++)
            buffer[i] = (uint8_t) (i * 5 + 1);
        for (int plan = 0; plan < 3; plan++)
        {
            CMotherboard board;
            board.LoadRAM(plan, &buffer[0]);
            Eq(buffer[0] | (buffer[1] << 8), board.GetRAMWord(plan, 0), _T("first word"));
            Eq(buffer[32766] | (buffer[32767] << 8), board.GetRAMWord(plan, 32766), _T("last loaded word"));
            Eq(0, board.GetRAMWord(plan, 32768), _T("the second half is not loaded"));
        }
    }

    TEST_METHOD(Board_LoadRomAndCartridge)
    {
        CMotherboard board;
        std::vector<uint8_t> rom(32768);
        for (size_t i = 0; i < rom.size(); i++)
            rom[i] = (uint8_t) (i + 9);
        board.LoadROM(&rom[0]);
        Eq(rom[0] | (rom[1] << 8), board.GetROMWord(0), _T("ROM first word"));
        Eq(rom[100] | (rom[101] << 8), board.GetROMWord(100), _T("ROM word 100"));
        Eq(rom[100], board.GetROMByte(100), _T("ROM byte 100"));
        Eq(rom[101], board.GetROMByte(101), _T("ROM byte 101"));

        Assert::IsFalse(board.IsROMCartridgeLoaded(1), _T("cartridge 1 is absent"));
        Eq(0xFFFF, board.GetROMCartWord(1, 0), _T("an absent cartridge reads as ones"));
        std::vector<uint8_t> cart(24 * 1024, 0x6C);
        board.LoadROMCartridge(1, &cart[0]);
        Assert::IsTrue(board.IsROMCartridgeLoaded(1), _T("cartridge 1 is loaded"));
        Assert::IsFalse(board.IsROMCartridgeLoaded(2), _T("cartridge 2 is still absent"));
        Eq(0x6C6C, board.GetROMCartWord(1, 0), _T("cartridge word"));
        board.UnloadROMCartridge(1);
        Assert::IsFalse(board.IsROMCartridgeLoaded(1), _T("cartridge 1 is unloaded"));
    }

    // CPU-PPU channels

    TEST_METHOD(Board_ChannelsAfterReset)
    {
        TestBoard tb;
        tb.Board()->ChanResetByCPU();
        tb.Board()->ChanResetByPPU();

        for (int ch = 0; ch < 3; ch++)
            Eq(0200, tb.Board()->ChanTxStateGetCPU(ch), _T("CPU transmitter is ready"));
        for (int ch = 0; ch < 2; ch++)
            Eq(0, tb.Board()->ChanRxStateGetCPU(ch), _T("CPU receiver is empty"));
        Eq(0, tb.Board()->ChanRxStateGetPPU(), _T("PPU receivers are empty"));
        Eq(030, tb.Board()->ChanTxStateGetPPU(), _T("PPU transmitters 0 and 1 are ready"));
    }

    TEST_METHOD(Board_ChannelsCpuToPpu)
    {
        for (uint8_t ch = 0; ch < 3; ch++)
        {
            TestBoard tb;
            CMotherboard* pBoard = tb.Board();
            pBoard->ChanResetByCPU();
            pBoard->ChanResetByPPU();

            pBoard->ChanWriteByCPU(ch, 0xA0 + ch);
            Eq(0, pBoard->ChanTxStateGetCPU(ch), _T("CPU transmitter is busy"));
            Eq(1 << (3 + ch), pBoard->ChanRxStateGetPPU() & (1 << (3 + ch)), _T("PPU receiver has data"));

            Eq(0xA0 + ch, pBoard->ChanReadByPPU(ch), _T("data"));
            Eq(0, pBoard->ChanRxStateGetPPU() & (1 << (3 + ch)), _T("PPU receiver is empty"));
            Eq(0200, pBoard->ChanTxStateGetCPU(ch), _T("CPU transmitter is ready again"));
        }
    }

    TEST_METHOD(Board_ChannelsPpuToCpu)
    {
        for (uint8_t ch = 0; ch < 2; ch++)
        {
            TestBoard tb;
            CMotherboard* pBoard = tb.Board();
            pBoard->ChanResetByCPU();
            pBoard->ChanResetByPPU();

            pBoard->ChanWriteByPPU(ch, 0xB0 + ch);
            Eq(0, pBoard->ChanTxStateGetPPU() & (1 << (3 + ch)), _T("PPU transmitter is busy"));
            Eq(0200, pBoard->ChanRxStateGetCPU(ch), _T("CPU receiver has data"));

            Eq(0xB0 + ch, pBoard->ChanReadByCPU(ch), _T("data"));
            Eq(0, pBoard->ChanRxStateGetCPU(ch), _T("CPU receiver is empty"));
            Eq(1 << (3 + ch), pBoard->ChanTxStateGetPPU() & (1 << (3 + ch)), _T("PPU transmitter is ready again"));
        }
    }

    TEST_METHOD(Board_ChannelsThroughThePorts)
    {
        // The CPU writes the channel 0 data register 177566, the PPU reads it from 177060
        TestBoard tb;
        tb.Board()->ChanResetByCPU();
        tb.Board()->ChanResetByPPU();

        tb.Board()->GetCPUMemoryController()->SetByte(0177566, false, 0x42);
        Eq(0x42, tb.Board()->GetPPUMemoryController()->GetByte(0177060, false), _T("PPU reads the byte"));

        // The PPU writes the channel 1 data register 177072, the CPU reads it from 176662
        tb.Board()->GetPPUMemoryController()->SetByte(0177072, false, 0x43);
        Eq(0x43, tb.Board()->GetCPUMemoryController()->GetByte(0176662, false), _T("CPU reads the byte"));
    }

    TEST_METHOD(Board_ChannelCpuReceiverInterrupts)
    {
        // Vectors: channel 0 receiver 060, channel 1 receiver 460
        TestBoard tb;
        tb.StartCPU();
        tb.Board()->ChanResetByCPU();
        tb.Board()->ChanResetByPPU();

        tb.Board()->ChanRxStateSetCPU(0, 0100);  // interrupt enable
        tb.Board()->ChanRxStateSetCPU(1, 0100);
        tb.Board()->ChanWriteByPPU(0, 1);
        Eq(060, tb.CPU()->GetVIRQ(1), _T("channel 0 vector"));
        tb.Board()->ChanWriteByPPU(1, 2);
        Eq(0460, tb.CPU()->GetVIRQ(3), _T("channel 1 vector"));

        tb.Board()->ChanReadByCPU(0);
        Eq(0, tb.CPU()->GetVIRQ(1), _T("reading the data cancels the request, channel 0"));
        tb.Board()->ChanReadByCPU(1);
        Eq(0, tb.CPU()->GetVIRQ(3), _T("reading the data cancels the request, channel 1"));
    }

    TEST_METHOD(Board_ChannelCpuTransmitterInterrupts)
    {
        // Vectors: channel 0 transmitter 064, channel 1 transmitter 464, channel 2 transmitter 474.
        // The transmitters are ready after the reset, so enabling the interrupt requests it at once.
        TestBoard tb;
        tb.StartCPU();
        tb.Board()->ChanResetByCPU();
        tb.Board()->ChanResetByPPU();

        tb.Board()->ChanTxStateSetCPU(0, 0100);
        Eq(064, tb.CPU()->GetVIRQ(2), _T("channel 0 vector"));
        tb.Board()->ChanTxStateSetCPU(1, 0100);
        Eq(0464, tb.CPU()->GetVIRQ(4), _T("channel 1 vector"));
        tb.Board()->ChanTxStateSetCPU(2, 0100);
        Eq(0474, tb.CPU()->GetVIRQ(5), _T("channel 2 vector"));

        tb.Board()->ChanTxStateSetCPU(0, 0);
        Eq(0, tb.CPU()->GetVIRQ(2), _T("disabling the interrupt cancels the request"));
    }

    TEST_METHOD(Board_ChannelPpuReceiverInterrupts)
    {
        // Vectors: channel 0 receiver 320, channel 1 receiver 330, channel 2 receiver 340
        TestBoard tb;
        tb.Board()->ChanResetByCPU();
        tb.Board()->ChanResetByPPU();

        tb.Board()->ChanRxStateSetPPU(07);  // interrupt enable for the three channels
        tb.Board()->ChanWriteByCPU(0, 1);
        Eq(0320, tb.PPU()->GetVIRQ(5), _T("channel 0 vector"));
        tb.Board()->ChanWriteByCPU(1, 2);
        Eq(0330, tb.PPU()->GetVIRQ(7), _T("channel 1 vector"));
        tb.Board()->ChanWriteByCPU(2, 3);
        Eq(0340, tb.PPU()->GetVIRQ(9), _T("channel 2 vector"));
    }

    TEST_METHOD(Board_ChannelPpuTransmitterInterrupts)
    {
        // Vectors: channel 0 transmitter 324, channel 1 transmitter 334
        TestBoard tb;
        tb.Board()->ChanResetByCPU();
        tb.Board()->ChanResetByPPU();

        tb.Board()->ChanTxStateSetPPU(03);
        Eq(0324, tb.PPU()->GetVIRQ(6), _T("channel 0 vector"));
        Eq(0334, tb.PPU()->GetVIRQ(8), _T("channel 1 vector"));
    }

    TEST_METHOD(Board_ChannelInterruptIsTakenByTheCpu)
    {
        TestBoard tb;
        tb.StartCPU();
        tb.Board()->ChanResetByCPU();
        tb.Board()->ChanResetByPPU();
        tb.SetWord(060, 02000);  // handler address
        tb.SetWord(062, 0);      // handler PSW
        tb.Board()->ChanRxStateSetCPU(0, 0100);
        tb.Board()->ChanWriteByPPU(0, 1);
        tb.CPU()->SetSP(STACK);

        Assert::IsTrue(tb.CPU()->InterruptProcessing(), _T("interrupt is taken"));

        Eq(02000, tb.CPU()->GetPC(), _T("PC"));
        Eq(STACK - 4, tb.CPU()->GetSP(), _T("SP"));
        Eq(0, tb.CPU()->GetVIRQ(1), _T("the request is gone"));
    }

    // Timer

    TEST_METHOD(Board_TimerCountsDown)
    {
        TestBoard tb;
        CMotherboard* pBoard = tb.Board();
        pBoard->SetTimerReload(5);
        Eq(5, pBoard->GetTimerValueView(), _T("a stopped timer holds the reload value"));

        TimerTicks(pBoard, 3);
        Eq(5, pBoard->GetTimerValueView(), _T("a stopped timer does not count"));

        pBoard->SetTimerState(1);  // start
        TimerTicks(pBoard, 2);
        Eq(3, pBoard->GetTimerValueView(), _T("2 ticks after the start"));
    }

    TEST_METHOD(Board_TimerReloadsAtZero)
    {
        TestBoard tb;
        CMotherboard* pBoard = tb.Board();
        pBoard->SetTimerReload(3);
        pBoard->SetTimerState(1);

        TimerTicks(pBoard, 2);
        Eq(1, pBoard->GetTimerValueView(), _T("2 ticks"));
        Eq(0, pBoard->GetTimerStateView() & 0200, _T("no zero flag yet"));

        TimerTicks(pBoard, 1);
        Eq(3, pBoard->GetTimerValueView(), _T("reloaded after 0"));
        Eq(0200, pBoard->GetTimerStateView() & 0200, _T("zero flag"));
    }

    TEST_METHOD(Board_TimerPrescaler)
    {
        // Bits 2, 1 of the state: 00 = 2 us, 01 = 4 us, 10 = 8 us, 11 = 16 us per count; the tick is 2 us
        static const struct { uint16_t state; int ticksPerCount; } cases[] =
        {
            { 01, 1 }, { 03, 2 }, { 05, 4 }, { 07, 8 },
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        {
            TestBoard tb;
            CMotherboard* pBoard = tb.Board();
            pBoard->SetTimerReload(100);
            pBoard->SetTimerState(cases[i].state);

            TimerTicks(pBoard, cases[i].ticksPerCount * 3);

            TCHAR msg[40];
            _stprintf_s(msg, _T("state %03o"), (unsigned int) cases[i].state);
            Eq(97, pBoard->GetTimerValueView(), msg);
        }
    }

    TEST_METHOD(Board_TimerOverflowFlag)
    {
        // The zero flag is set at 0; reaching 0 again before the flag is read sets the overflow flag too
        TestBoard tb;
        CMotherboard* pBoard = tb.Board();
        pBoard->SetTimerReload(2);
        pBoard->SetTimerState(1);

        TimerTicks(pBoard, 4);

        Eq(0211, pBoard->GetTimerState(), _T("running, overflow, zero"));
        Eq(0201, pBoard->GetTimerState(), _T("reading the state clears the overflow flag"));
    }

    TEST_METHOD(Board_TimerReloadIs12Bit)
    {
        TestBoard tb;
        tb.Board()->SetTimerReload(0177777);
        Eq(07777, tb.Board()->GetTimerReload(), _T("reload"));
        Eq(07777, tb.Board()->GetTimerValueView(), _T("value"));
    }

    TEST_METHOD(Board_TimerInterrupt)
    {
        // The PPU interrupt vector of the timer is 304
        TestBoard tb;
        tb.Board()->SetTimerReload(1);
        tb.Board()->SetTimerState(0101);  // run, interrupt enable
        Eq(0, tb.PPU()->GetVIRQ(2), _T("no request before"));

        TimerTicks(tb.Board(), 1);

        Eq(0304, tb.PPU()->GetVIRQ(2), _T("vector"));
    }

    TEST_METHOD(Board_TimerNoInterruptWhenDisabled)
    {
        TestBoard tb;
        tb.Board()->SetTimerReload(1);
        tb.Board()->SetTimerState(1);
        TimerTicks(tb.Board(), 3);
        Eq(0, tb.PPU()->GetVIRQ(2), _T("no request"));
    }

    TEST_METHOD(Board_TimerPorts)
    {
        // The PPU ports: 177710 state, 177712 reload, 177714 counter
        TestBoard tb;
        CMemoryController* pCtl = tb.Board()->GetPPUMemoryController();
        pCtl->SetWord(0177712, false, 4);
        pCtl->SetWord(0177710, false, 1);
        TimerTicks(tb.Board(), 3);

        Eq(1, pCtl->GetWord(0177714, false, false), _T("counter"));
        Eq(1, pCtl->GetWord(0177710, false, false) & 1, _T("state: running"));
    }

    // 50 Hz event

    TEST_METHOD(Board_Tick50InterruptsCpuAndPpu)
    {
        // The event vector is 100. The bits 8 and 9 of the port 177054 mask the PPU and CPU events,
        // they are set after the power on.
        TestBoard tb;
        tb.StartCPU();
        tb.SetPpuWord(0177054, 01);  // unmask both
        tb.SetWord(0100, 02000);
        tb.SetWord(0102, 0);
        tb.SetPpuWord(0100, 03000);
        tb.SetPpuWord(0102, 0);

        tb.Board()->Tick50();
        tb.CPU()->SetSP(STACK);
        tb.PPU()->SetSP(STACK);

        Assert::IsTrue(tb.CPU()->InterruptProcessing(), _T("CPU takes the event"));
        Eq(02000, tb.CPU()->GetPC(), _T("CPU PC"));
        Assert::IsTrue(tb.PPU()->InterruptProcessing(), _T("PPU takes the event"));
        Eq(03000, tb.PPU()->GetPC(), _T("PPU PC"));
    }

    TEST_METHOD(Board_Tick50IsMaskedAfterPowerOn)
    {
        TestBoard tb;
        tb.StartCPU();
        tb.Board()->Tick50();
        tb.CPU()->SetSP(STACK);
        tb.PPU()->SetSP(STACK);
        Assert::IsFalse(tb.CPU()->InterruptProcessing(), _T("CPU event is masked"));
        Assert::IsFalse(tb.PPU()->InterruptProcessing(), _T("PPU event is masked"));
    }

    TEST_METHOD(Board_Tick50IsIgnoredWhenInterruptsAreDisabled)
    {
        // Bit 7 of the PSW (priority) blocks the event
        TestBoard tb;
        tb.StartCPU();
        tb.SetPpuWord(0177054, 01);
        tb.CPU()->SetPSW(0200);
        tb.Board()->Tick50();
        tb.CPU()->SetSP(STACK);
        Assert::IsFalse(tb.CPU()->InterruptProcessing());
    }

    // Emulator image

    TEST_METHOD(Board_ImageSize)
    {
        // 512 bytes of the header, 32 KB of ROM, three 64 KB RAM planes
        Eq(512 + 32 * 1024 + 3 * 64 * 1024, UKNCIMAGE_SIZE, _T("image size"));
        Eq(512, UKNCIMAGE_HEADER_SIZE, _T("header size"));
    }

    TEST_METHOD(Board_SaveAndLoadImage)
    {
        TestBoard a;
        CMotherboard* pa = a.Board();

        // Memory
        std::vector<uint8_t> rom(32768);
        for (size_t i = 0; i < rom.size(); i++)
            rom[i] = (uint8_t) (i * 3 + 7);
        pa->LoadROM(&rom[0]);
        pa->SetRAMByte(0, 0x1234, 0x11);
        pa->SetRAMByte(1, 0x2345, 0x22);
        pa->SetRAMByte(2, 0xFFFF, 0x33);
        // Processors
        a.CPU()->SetReg(0, 0x1111);
        a.CPU()->SetReg(5, 0x5555);
        a.CPU()->SetSP(0x2222);
        a.CPU()->SetPSW(012);
        a.PPU()->SetReg(1, 0x7777);
        // Timer
        pa->SetTimerReload(0123);
        pa->SetTimerState(0103);
        TimerTicks(pa, 6);
        // Channels
        pa->ChanResetByCPU();
        pa->ChanResetByPPU();
        pa->ChanWriteByCPU(1, 0x5A);
        // Ports
        a.CpuMem()->SetWord(0176640, false, 0123);
        a.CpuMem()->SerialInput(0x41);
        a.SetPpuWord(0177010, 0456);
        a.SetPpuWord(0177054, 0360);
        a.PpuMem()->KeyboardEvent(0x2A, true);

        std::vector<uint8_t> image(UKNCIMAGE_SIZE, 0);
        pa->SaveToImage(&image[0]);

        TestBoard b;
        CMotherboard* pb = b.Board();
        pb->LoadFromImage(&image[0]);

        Eq(rom[0] | (rom[1] << 8), pb->GetROMWord(0), _T("ROM"));
        Eq(rom[32766] | (rom[32767] << 8), pb->GetROMWord(32766), _T("ROM end"));
        Eq(0x11, pb->GetRAMByte(0, 0x1234), _T("RAM plane 0"));
        Eq(0x22, pb->GetRAMByte(1, 0x2345), _T("RAM plane 1"));
        Eq(0x33, pb->GetRAMByte(2, 0xFFFF), _T("RAM plane 2"));

        Eq(0x1111, b.CPU()->GetReg(0), _T("CPU R0"));
        Eq(0x5555, b.CPU()->GetReg(5), _T("CPU R5"));
        Eq(0x2222, b.CPU()->GetSP(), _T("CPU SP"));
        Eq(012, b.CPU()->GetPSW(), _T("CPU PSW"));
        Eq(0x7777, b.PPU()->GetReg(1), _T("PPU R1"));

        Eq(pa->GetTimerValueView(), pb->GetTimerValueView(), _T("timer value"));
        Eq(pa->GetTimerReloadView(), pb->GetTimerReloadView(), _T("timer reload"));
        Eq(pa->GetTimerStateView(), pb->GetTimerStateView(), _T("timer state"));

        Eq(0x5A, pb->ChanReadByPPU(1), _T("channel data"));

        Eq(0123, b.Board()->GetCPUMemoryController()->GetWord(0176640, false, false), _T("CPU port 176640"));
        Eq(0200, b.Board()->GetCPUMemoryController()->GetWord(0176570, false, false), _T("serial receiver is ready"));
        Eq(0x41, b.Board()->GetCPUMemoryController()->GetWord(0176572, false, false), _T("serial data"));
        Eq(0456, b.Board()->GetPPUMemoryController()->GetWord(0177010, false, false), _T("PPU port 177010"));
        Eq(0360, b.Board()->GetPPUMemoryController()->GetWord(0177054, false, false), _T("PPU port 177054"));
        Eq(0200, b.Board()->GetPPUMemoryController()->GetWord(0177700, false, false) & 0200, _T("keyboard is ready"));
        Eq(0x2A, b.Board()->GetPPUMemoryController()->GetWord(0177702, false, false), _T("key code"));
    }

    TEST_METHOD(Board_LoadImageRestoresTheMemoryMap)
    {
        TestBoard a;
        a.SetPpuWord(0177054, 0360);  // RAM in all the windows
        std::vector<uint8_t> image(UKNCIMAGE_SIZE, 0);
        a.Board()->SaveToImage(&image[0]);

        TestBoard b;
        uint16_t offset;
        Eq(ADDRTYPE_ROM, b.PpuMem()->TranslateAddress(0100000, false, false, &offset, true), _T("ROM before the load"));
        b.Board()->LoadFromImage(&image[0]);
        Eq(ADDRTYPE_RAM0, b.PpuMem()->TranslateAddress(0100000, false, false, &offset, true), _T("RAM after the load"));
    }

    TEST_METHOD(Board_ImageKeepsTheHeaderUntouched)
    {
        // The board writes only its own part of the header; the first 32 bytes belong to the application
        TestBoard a;
        std::vector<uint8_t> image(UKNCIMAGE_SIZE, 0xEE);
        a.Board()->SaveToImage(&image[0]);
        for (int i = 0; i < 32; i++)
            Eq(0xEE, image[i], _T("header byte"));
    }
};
