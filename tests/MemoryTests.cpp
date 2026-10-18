/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// MemoryTests.cpp : tests for the CPU and PPU memory controllers: address decoding,
// RAM planes, ROM, I/O ports and bus errors
//
// Expected values come from the memory maps in the comments of Memory.cpp and from the
// UKNC documentation, not from the emulator output. Tests of the known bugs are ignored,
// see the list in docs/unit-tests.md.

#include "stdafx.h"
#include <CppUnitTest.h>
#include <vector>
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace
{

const uint16_t STACK = 0400;

// Take the pending bus error, if any, of the CPU
bool CpuBusError(TestBoard& tb)
{
    tb.CPU()->SetSP(STACK);
    return tb.CPU()->InterruptProcessing();
}

// Take the pending bus error, if any, of the PPU
bool PpuBusError(TestBoard& tb)
{
    tb.PPU()->SetSP(STACK);
    return tb.PPU()->InterruptProcessing();
}

int CpuAddrType(TestBoard& tb, uint16_t address, bool haltMode, bool exec, uint16_t* pOffset)
{
    return tb.CpuMem()->TranslateAddress(address, haltMode, exec, pOffset, true);
}

int PpuAddrType(TestBoard& tb, uint16_t address, bool exec, uint16_t* pOffset)
{
    return tb.PpuMem()->TranslateAddress(address, false, exec, pOffset, true);
}

}

TEST_CLASS(MemoryTests)
{
public:
    // CPU memory map

    TEST_METHOD(Memory_CpuRamIsSplitBetweenPlanes1And2)
    {
        // A CPU word at the address A is the byte A/2 of the plane 1 (low) and plane 2 (high)
        TestBoard tb;
        tb.SetWord(0200, 0x1234);
        Eq(0x34, tb.Board()->GetRAMByte(1, 0100), _T("plane 1"));
        Eq(0x12, tb.Board()->GetRAMByte(2, 0100), _T("plane 2"));
        Eq(0x00, tb.Board()->GetRAMByte(0, 0100), _T("plane 0 is not touched"));
        Eq(0x00, tb.Board()->GetRAMByte(0, 0200), _T("plane 0 is not touched at the same address"));

        tb.SetByte(0203, 0xAB);  // odd address: the high byte of the word 0202
        Eq(0xAB, tb.Board()->GetRAMByte(2, 0101), _T("odd byte goes to the plane 2"));
        tb.SetByte(0202, 0xCD);
        Eq(0xCD, tb.Board()->GetRAMByte(1, 0101), _T("even byte goes to the plane 1"));
        Eq(0xABCD, tb.GetWord(0202), _T("the word"));
    }

    TEST_METHOD(Memory_CpuRamLastWord)
    {
        TestBoard tb;
        tb.SetWord(0157776, 0xBEEF);
        Eq(0xBEEF, tb.GetWord(0157776), _T("last RAM word in USER mode"));
        Eq(0xEF, tb.Board()->GetRAMByte(1, 0157776 / 2), _T("plane 1"));
        Eq(0xBE, tb.Board()->GetRAMByte(2, 0157776 / 2), _T("plane 2"));
    }

    TEST_METHOD(Memory_CpuAddressTypes)
    {
        TestBoard tb;
        uint16_t offset;

        Eq(ADDRTYPE_RAM12, CpuAddrType(tb, 0, false, false, &offset), _T("0, USER"));
        Eq(0, offset, _T("offset of 0"));
        Eq(ADDRTYPE_RAM12, CpuAddrType(tb, 0157776, false, true, &offset), _T("157776, USER, exec"));
        Eq(0157776, offset, _T("offset of 157776"));
        Eq(ADDRTYPE_IO, CpuAddrType(tb, 0160000, false, false, &offset), _T("160000, USER"));
        Eq(ADDRTYPE_IO, CpuAddrType(tb, 0177776, false, false, &offset), _T("177776, USER"));

        Eq(ADDRTYPE_RAM12, CpuAddrType(tb, 0157776, true, false, &offset), _T("157776, HALT"));
        Eq(ADDRTYPE_RAM12, CpuAddrType(tb, 0160000, true, false, &offset), _T("160000, HALT"));
        Eq(ADDRTYPE_RAM12, CpuAddrType(tb, 0177776, true, false, &offset), _T("177776, HALT"));
        Eq(0177776, offset, _T("offset of 177776, HALT"));
    }

    TEST_METHOD(Memory_CpuHaltModeSeesRamAtTheTopOfTheAddressSpace)
    {
        TestBoard tb;
        tb.Board()->GetCPUMemoryController()->SetWord(0170000, true, 0x5AA5);
        Eq(0xA5, tb.Board()->GetRAMByte(1, 0170000 / 2), _T("plane 1"));
        Eq(0x5A, tb.Board()->GetRAMByte(2, 0170000 / 2), _T("plane 2"));
        Eq(0x5AA5, tb.Board()->GetCPUMemoryController()->GetWord(0170000, true, false), _T("read back"));
        Assert::IsFalse(CpuBusError(tb), _T("no bus error in HALT mode"));
    }

    TEST_METHOD(Memory_CpuBusErrorOnUnknownPort)
    {
        TestBoard tb;
        Assert::IsFalse(CpuBusError(tb), _T("nothing pending at the start"));
        tb.Board()->GetCPUMemoryController()->GetWord(0170000, false, false);
        Assert::IsTrue(CpuBusError(tb), _T("read of the address 170000 in USER mode"));
    }

    TEST_METHOD(Memory_CpuBusErrorOnUnknownPortWrite)
    {
        TestBoard tb;
        tb.Board()->GetCPUMemoryController()->SetWord(0176000, false, 0);
        Assert::IsTrue(CpuBusError(tb), _T("write to the address 176000 in USER mode"));
    }

    TEST_METHOD(Memory_CpuNoBusErrorInRam)
    {
        TestBoard tb;
        tb.SetWord(0200, 1);
        tb.Board()->GetCPUMemoryController()->GetWord(0200, false, false);
        Assert::IsFalse(CpuBusError(tb));
    }

    // CPU ports

    TEST_METHOD(Memory_CpuPlaneRegisters)
    {
        // 176640: plane address, 176642: data of the planes 1 (low byte) and 2 (high byte)
        TestBoard tb;
        tb.Board()->SetRAMByte(1, 0100, 0x34);
        tb.Board()->SetRAMByte(2, 0100, 0x12);
        CMemoryController* pCtl = tb.Board()->GetCPUMemoryController();

        pCtl->SetWord(0176640, false, 0100);
        Eq(0100, pCtl->GetWord(0176640, false, false), _T("address register"));
        Eq(0x1234, pCtl->GetWord(0176642, false, false), _T("data register"));

        pCtl->SetWord(0176642, false, 0xABCD);
        Eq(0xCD, tb.Board()->GetRAMByte(1, 0100), _T("plane 1 after the word write"));
        Eq(0xAB, tb.Board()->GetRAMByte(2, 0100), _T("plane 2 after the word write"));

        pCtl->SetByte(0176642, false, 0x11);
        pCtl->SetByte(0176643, false, 0x22);
        Eq(0x11, tb.Board()->GetRAMByte(1, 0100), _T("plane 1 after the byte write"));
        Eq(0x22, tb.Board()->GetRAMByte(2, 0100), _T("plane 2 after the byte write"));
        Assert::IsFalse(CpuBusError(tb), _T("no bus error"));
    }

    TEST_METHOD(Memory_CpuSerialReceiver)
    {
        TestBoard tb;
        CMemoryController* pCtl = tb.Board()->GetCPUMemoryController();
        Eq(0, pCtl->GetWord(0176570, false, false), _T("status at the start"));

        bool irq = tb.CpuMem()->SerialInput(0x41);
        Assert::IsFalse(irq, _T("no interrupt when disabled"));
        Eq(0200, pCtl->GetWord(0176570, false, false), _T("ready"));
        Eq(0x41, pCtl->GetWord(0176572, false, false), _T("data"));
        Eq(0, pCtl->GetWord(0176570, false, false), _T("reading the data resets ready"));
    }

    TEST_METHOD(Memory_CpuSerialReceiverOverflowAndInterrupt)
    {
        TestBoard tb;
        CMemoryController* pCtl = tb.Board()->GetCPUMemoryController();

        tb.CpuMem()->SerialInput(0x41);
        tb.CpuMem()->SerialInput(0x42);  // the first byte was not read
        Eq(0010200, pCtl->GetWord(0176570, false, false) & 0010200, _T("ready and overflow"));
        Eq(0x41, pCtl->GetWord(0176572, false, false), _T("the first byte is kept"));

        pCtl->SetWord(0176570, false, 0100);  // interrupt enable
        Assert::IsTrue(tb.CpuMem()->SerialInput(0x43), _T("interrupt is requested"));
    }

    TEST_METHOD(Memory_CpuSerialTransmitter)
    {
        TestBoard tb;
        CMemoryController* pCtl = tb.Board()->GetCPUMemoryController();
        Eq(0200, pCtl->GetWord(0176574, false, false), _T("ready at the start"));
        pCtl->SetWord(0176576, false, 'A');
        Eq(0, pCtl->GetWord(0176574, false, false) & 0200, _T("a byte is written: not ready"));
    }

    TEST_METHOD(Memory_CpuNetworkReceiver)
    {
        TestBoard tb;
        CMemoryController* pCtl = tb.Board()->GetCPUMemoryController();
        Eq(0200, pCtl->GetWord(0176564, false, false), _T("transmitter is ready at the start"));

        tb.CpuMem()->NetworkInput(0x77);
        Eq(0200, pCtl->GetWord(0176560, false, false), _T("ready"));
        Eq(0x77, pCtl->GetWord(0176562, false, false), _T("data"));
        Eq(0, pCtl->GetWord(0176560, false, false), _T("reading the data resets ready"));
    }

    TEST_METHOD(Memory_CpuNetworkStationNumberIsAddedToTheStatus)
    {
        TestBoard tb;
        tb.Board()->SetNetStation(5);
        Eq(5, tb.Board()->GetCPUMemoryController()->GetWord(0176560, false, false) & 0377, _T("receiver status"));
        Eq(0200 + 5, tb.Board()->GetCPUMemoryController()->GetWord(0176564, false, false) & 0377, _T("transmitter status"));
    }

    // Known bug: the status register of the channel 1 transmitter is 176664, its odd byte 176665 must work
    // like the odd byte 177565 of the channel 0 (the case label in GetPortWord has a typo, 177665)
    BEGIN_TEST_METHOD_ATTRIBUTE(Memory_CpuChannelStatusOddBytes_KnownBug)
        TEST_IGNORE()
    END_TEST_METHOD_ATTRIBUTE()
    TEST_METHOD(Memory_CpuChannelStatusOddBytes_KnownBug)
    {
        static const uint16_t addresses[] = { 0177561, 0177565, 0176661, 0176665, 0176675 };
        for (size_t i = 0; i < sizeof(addresses) / sizeof(addresses[0]); i++)
        {
            TestBoard tb;
            tb.Board()->GetCPUMemoryController()->GetByte(addresses[i], false);
            TCHAR msg[40];
            _stprintf_s(msg, _T("read of the byte %06o"), (unsigned int) addresses[i]);
            Assert::IsFalse(CpuBusError(tb), msg);
        }
    }

    // PPU memory map

    TEST_METHOD(Memory_PpuRamIsPlane0)
    {
        TestBoard tb;
        tb.SetPpuWord(0200, 0x1234);
        Eq(0x1234, tb.Board()->GetRAMWord(0, 0200), _T("plane 0"));
        Eq(0x00, tb.Board()->GetRAMByte(1, 0200), _T("plane 1 is not touched"));
        Eq(0x00, tb.Board()->GetRAMByte(2, 0200), _T("plane 2 is not touched"));
        Eq(0x1234, tb.GetPpuWord(0200), _T("read back"));
    }

    TEST_METHOD(Memory_PpuAddressTypesAtReset)
    {
        // The window ports select the ROM at 100000-176777 after the reset
        TestBoard tb;
        uint16_t offset;

        Eq(ADDRTYPE_RAM0, PpuAddrType(tb, 0, false, &offset), _T("0"));
        Eq(ADDRTYPE_RAM0, PpuAddrType(tb, 077776, false, &offset), _T("77776"));
        Eq(077776, offset, _T("offset of 77776"));
        Eq(ADDRTYPE_ROM, PpuAddrType(tb, 0100000, false, &offset), _T("100000"));
        Eq(0, offset, _T("offset of 100000"));
        Eq(ADDRTYPE_ROM, PpuAddrType(tb, 0120000, false, &offset), _T("120000"));
        Eq(020000, offset, _T("offset of 120000"));
        Eq(ADDRTYPE_ROM, PpuAddrType(tb, 0176776, false, &offset), _T("176776"));
        Eq(076776, offset, _T("offset of 176776"));
        Eq(ADDRTYPE_IO, PpuAddrType(tb, 0177000, false, &offset), _T("177000"));
        Eq(ADDRTYPE_IO, PpuAddrType(tb, 0177776, false, &offset), _T("177776"));
        Eq(ADDRTYPE_DENY, PpuAddrType(tb, 0177000, true, &offset), _T("177000, exec is denied"));
    }

    TEST_METHOD(Memory_PpuReadsRom)
    {
        TestBoard tb;
        std::vector<uint8_t> rom(32768);
        for (size_t i = 0; i < rom.size(); i++)
            rom[i] = (uint8_t) (i * 7 + 3);
        tb.Board()->LoadROM(&rom[0]);

        Eq(rom[0] | (rom[1] << 8), tb.GetPpuWord(0100000), _T("first ROM word"));
        Eq(rom[0x1234] | (rom[0x1235] << 8), tb.GetPpuWord(0100000 + 0x1234), _T("ROM word 1234"));
        Eq(rom[076776] | (rom[076777] << 8), tb.GetPpuWord(0176776), _T("last ROM word below the ports"));
        Eq(rom[0] | (rom[1] << 8), tb.Board()->GetROMWord(0), _T("GetROMWord"));
    }

    TEST_METHOD(Memory_PpuRomIsReadOnly)
    {
        TestBoard tb;
        std::vector<uint8_t> rom(32768, 0x5A);
        tb.Board()->LoadROM(&rom[0]);

        tb.SetPpuWord(0100000, 0x1234);

        Eq(0x5A5A, tb.GetPpuWord(0100000), _T("the word is not changed"));
        Assert::IsFalse(PpuBusError(tb), _T("write to ROM is not an error"));
    }

    TEST_METHOD(Memory_PpuWindowsCanSelectRam)
    {
        // Port 177054: bit 4 = RAM in the window 100000-117777, bits 5, 6, 7 = RAM in the next three windows
        TestBoard tb;
        uint16_t offset;
        tb.SetPpuWord(0177054, 0360);

        Eq(ADDRTYPE_RAM0, PpuAddrType(tb, 0100000, false, &offset), _T("window 0"));
        Eq(0100000, offset, _T("offset in window 0"));
        Eq(ADDRTYPE_RAM0, PpuAddrType(tb, 0120000, false, &offset), _T("window 1"));
        Eq(ADDRTYPE_RAM0, PpuAddrType(tb, 0140000, false, &offset), _T("window 2"));
        Eq(ADDRTYPE_RAM0, PpuAddrType(tb, 0160000, false, &offset), _T("window 3"));
        Eq(0160000, offset, _T("offset in window 3"));
        Eq(ADDRTYPE_IO, PpuAddrType(tb, 0177000, false, &offset), _T("the ports stay"));

        tb.SetPpuWord(0100000, 0xCAFE);
        tb.SetPpuWord(0176776, 0xBABE);
        Eq(0xCAFE, tb.Board()->GetRAMWord(0, 0100000), _T("RAM at 100000"));
        Eq(0xBABE, tb.Board()->GetRAMWord(0, 0176776), _T("RAM at 176776"));
    }

    TEST_METHOD(Memory_PpuWindowsAreSelectedOneByOne)
    {
        TestBoard tb;
        uint16_t offset;
        tb.SetPpuWord(0177054, 0101);  // window 0 is the ROM (bit 0); only the window 2 = 140000-157777 is RAM (bit 6)

        Eq(ADDRTYPE_ROM, PpuAddrType(tb, 0100000, false, &offset), _T("window 0"));
        Eq(ADDRTYPE_ROM, PpuAddrType(tb, 0120000, false, &offset), _T("window 1"));
        Eq(ADDRTYPE_RAM0, PpuAddrType(tb, 0140000, false, &offset), _T("window 2"));
        Eq(ADDRTYPE_ROM, PpuAddrType(tb, 0160000, false, &offset), _T("window 3"));
    }

    TEST_METHOD(Memory_PpuRomCartridgeWindow)
    {
        // Bits 1, 2 of the port 177054 select the bank 1..3 of the cartridge in the slot 1
        TestBoard tb;
        uint16_t offset;
        tb.SetPpuWord(0177054, 02);

        Eq(ADDRTYPE_DENY, PpuAddrType(tb, 0100000, false, &offset), _T("no cartridge: access denied"));

        std::vector<uint8_t> cart(24 * 1024);
        for (size_t i = 0; i < cart.size(); i++)
            cart[i] = (uint8_t) (i ^ (i >> 8));
        tb.Board()->LoadROMCartridge(1, &cart[0]);

        Eq(ADDRTYPE_ROMCART1, PpuAddrType(tb, 0100000, false, &offset), _T("bank 1"));
        Eq(0, offset, _T("bank 1 starts at the cartridge start"));
        Eq(cart[0] | (cart[1] << 8), tb.GetPpuWord(0100000), _T("bank 1 word"));

        tb.SetPpuWord(0177054, 04);
        Eq(ADDRTYPE_ROMCART1, PpuAddrType(tb, 0100000, false, &offset), _T("bank 2"));
        Eq(020000, offset, _T("bank 2 is 8 KB further"));
        Eq(cart[020000] | (cart[020001] << 8), tb.GetPpuWord(0100000), _T("bank 2 word"));

        tb.SetPpuWord(0177054, 06);
        Eq(ADDRTYPE_ROMCART1, PpuAddrType(tb, 0100000, false, &offset), _T("bank 3"));
        Eq(040000, offset, _T("bank 3 is 16 KB further"));
    }

    TEST_METHOD(Memory_PpuRomCartridgeInSlot2)
    {
        TestBoard tb;
        uint16_t offset;
        std::vector<uint8_t> cart(24 * 1024, 0x11);
        tb.Board()->LoadROMCartridge(2, &cart[0]);
        tb.SetPpuWord(0177054, 012);  // bit 3 = slot 2, bank 1

        Eq(ADDRTYPE_ROMCART2, PpuAddrType(tb, 0100000, false, &offset), _T("slot 2"));
        Eq(0x1111, tb.GetPpuWord(0100000), _T("word"));
    }

    // PPU ports

    TEST_METHOD(Memory_PpuPlaneRegisters)
    {
        // 177010: plane address, 177012: plane 0 data, 177014: planes 1 (low) and 2 (high) data
        TestBoard tb;
        CMemoryController* pCtl = tb.Board()->GetPPUMemoryController();
        tb.Board()->SetRAMByte(0, 0100, 0x55);
        tb.Board()->SetRAMByte(1, 0100, 0x34);
        tb.Board()->SetRAMByte(2, 0100, 0x12);

        pCtl->SetWord(0177010, false, 0100);
        Eq(0100, pCtl->GetWord(0177010, false, false), _T("address register"));
        Eq(0x55, pCtl->GetWord(0177012, false, false), _T("plane 0 data"));
        Eq(0x1234, pCtl->GetWord(0177014, false, false), _T("planes 1 and 2 data"));

        pCtl->SetWord(0177012, false, 0x00AA);
        pCtl->SetWord(0177014, false, 0xBBCC);
        Eq(0xAA, tb.Board()->GetRAMByte(0, 0100), _T("plane 0"));
        Eq(0xCC, tb.Board()->GetRAMByte(1, 0100), _T("plane 1"));
        Eq(0xBB, tb.Board()->GetRAMByte(2, 0100), _T("plane 2"));
        Assert::IsFalse(PpuBusError(tb), _T("no bus error"));
    }

    TEST_METHOD(Memory_PpuBusErrorOnUnknownPort)
    {
        TestBoard tb;
        Assert::IsFalse(PpuBusError(tb), _T("nothing pending at the start"));
        tb.Board()->GetPPUMemoryController()->GetWord(0177040, false, false);
        Assert::IsTrue(PpuBusError(tb), _T("read of the port 177040"));
    }

    TEST_METHOD(Memory_PpuBusErrorOnMouseWithoutMouse)
    {
        TestBoard tb;
        tb.Board()->GetPPUMemoryController()->GetWord(0177400, false, false);
        Assert::IsTrue(PpuBusError(tb), _T("the mouse port is absent when the mouse is off"));
    }

    TEST_METHOD(Memory_PpuMousePort)
    {
        TestBoard tb;
        tb.PpuMem()->SetMouse(true);
        tb.PpuMem()->MouseMove(5, 0, true, false);
        uint16_t value = tb.Board()->GetPPUMemoryController()->GetWord(0177400, false, false);
        Eq(5, (value >> 1) & 0177, _T("dx"));
        Eq(0x0100, value & 0x0101, _T("left button"));
        Assert::IsFalse(PpuBusError(tb), _T("no bus error"));
    }

    // Keyboard

    TEST_METHOD(Memory_PpuKeyboardPressAndRelease)
    {
        TestBoard tb;
        CMemoryController* pCtl = tb.Board()->GetPPUMemoryController();
        Eq(0, pCtl->GetWord(0177700, false, false), _T("status at the start"));

        tb.PpuMem()->KeyboardEvent(0x2A, true);
        Eq(0200, pCtl->GetWord(0177700, false, false) & 0200, _T("ready after a press"));
        Eq(0x2A, pCtl->GetWord(0177702, false, false), _T("pressed key code"));
        Eq(0, pCtl->GetWord(0177700, false, false) & 0200, _T("reading the data resets ready"));

        tb.PpuMem()->KeyboardEvent(0x2A, false);
        Eq(0200, pCtl->GetWord(0177700, false, false) & 0200, _T("ready after a release"));
        Eq(0200 | 0x0A, pCtl->GetWord(0177702, false, false), _T("released key: bit 7 and the low 4 bits"));
    }

    TEST_METHOD(Memory_PpuKeyboardInterrupt)
    {
        TestBoard tb;
        CMemoryController* pCtl = tb.Board()->GetPPUMemoryController();

        tb.PpuMem()->KeyboardEvent(0x10, true);
        Eq(0, tb.PPU()->GetVIRQ(3), _T("no interrupt when disabled"));
        pCtl->GetWord(0177702, false, false);

        pCtl->SetWord(0177700, false, 0100);  // interrupt enable
        tb.PpuMem()->KeyboardEvent(0x10, true);
        Eq(0300, tb.PPU()->GetVIRQ(3), _T("interrupt vector 300"));

        pCtl->GetWord(0177702, false, false);
        Eq(0, tb.PPU()->GetVIRQ(3), _T("reading the data cancels the request"));
    }

    // System control register

    TEST_METHOD(Memory_PpuStartsAndStopsCpu)
    {
        // 177716: bit 15 = ACLO released (the CPU starts), bit 5 = DCLO (the CPU stops), bit 4 = HALT
        TestBoard tb;
        Assert::IsTrue(tb.CPU()->IsStopped(), _T("the CPU is stopped after the reset"));

        tb.SetPpuWord(0177716, 0100000);
        Assert::IsFalse(tb.CPU()->IsStopped(), _T("the CPU runs"));

        tb.SetPpuWord(0177716, 040);
        Assert::IsTrue(tb.CPU()->IsStopped(), _T("the CPU is stopped by DCLO"));
    }

    // Bus devices

    TEST_METHOD(Memory_BusDevicesHaveNamesAndRanges)
    {
        TestBoard tb;
        const CBusDevice** lists[2] = { tb.Board()->GetCPUBusDevices(), tb.Board()->GetPPUBusDevices() };
        for (int list = 0; list < 2; list++)
        {
            int deviceCount = 0;
            for (const CBusDevice** pp = lists[list]; *pp != nullptr; pp++, deviceCount++)
            {
                const CBusDevice* pDevice = *pp;
                Assert::IsTrue(pDevice->GetName() != nullptr && pDevice->GetName()[0] != 0, _T("device name"));
                const uint16_t* r = pDevice->GetAddressRanges();
                Assert::IsTrue(r[0] != 0, _T("device has at least one range"));
            }
            Assert::IsTrue(deviceCount > 0, _T("there are devices"));
        }
    }

    // Known bug: the address ranges of the CPU devices "Network adapter" (176560-176567) and "Memory access"
    // (176564-176567) overlap, and so do the PPU devices "CPU-PPU channels" (177100-177103) and
    // "Programmable port" (177100-177103). The device that comes later in the list wins in the memory map.
    BEGIN_TEST_METHOD_ATTRIBUTE(Memory_BusDevicesDoNotOverlap_KnownBug)
        TEST_IGNORE()
    END_TEST_METHOD_ATTRIBUTE()
    TEST_METHOD(Memory_BusDevicesDoNotOverlap_KnownBug)
    {
        TestBoard tb;
        const CBusDevice** lists[2] = { tb.Board()->GetCPUBusDevices(), tb.Board()->GetPPUBusDevices() };
        for (int list = 0; list < 2; list++)
        {
            std::vector<int> owner(65536, -1);
            int deviceCount = 0;
            for (const CBusDevice** pp = lists[list]; *pp != nullptr; pp++, deviceCount++)
            {
                const CBusDevice* pDevice = *pp;
                for (const uint16_t* r = pDevice->GetAddressRanges(); r[0] != 0; r += 2)
                {
                    for (uint32_t a = r[0]; a < (uint32_t) r[0] + r[1]; a++)
                    {
                        TCHAR msg[80];
                        _stprintf_s(msg, _T("address %06o of the device %s"), a, pDevice->GetName());
                        Assert::AreEqual(-1, owner[a], msg);
                        owner[a] = deviceCount;
                    }
                }
            }
        }
    }
};
