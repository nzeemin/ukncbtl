/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// TestHelpers.cpp : fixtures shared by the tests

#include "stdafx.h"
#include <CppUnitTest.h>
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The processor builds its instruction table once per process
TEST_MODULE_INITIALIZE(TestModuleInitialize)
{
    CProcessor::Init();
}

TEST_MODULE_CLEANUP(TestModuleCleanup)
{
    CProcessor::Done();
}

void Eq(unsigned int expected, unsigned int actual, const TCHAR* msg)
{
    Assert::AreEqual(expected, actual, msg);
}

void FlagsText(uint16_t flags, TCHAR* buf)
{
    buf[0] = (flags & TF_N) ? 'N' : 'n';
    buf[1] = (flags & TF_Z) ? 'Z' : 'z';
    buf[2] = (flags & TF_V) ? 'V' : 'v';
    buf[3] = (flags & TF_C) ? 'C' : 'c';
    buf[4] = 0;
}

TestBoard::TestBoard()
{
    m_pBoard = new CMotherboard();
    m_pBoard->Reset();

    // The CPU stays in the state we set: USER mode, no flags, all registers zero
    CProcessor* pCPU = CPU();
    pCPU->SetPSW(0);
    for (int r = 0; r < 8; r++)
        pCPU->SetReg(r, 0);

    // Reset() starts the PPU: take its start interrupt, so that later interrupt checks see only new requests
    PPU()->InterruptProcessing();
    PPU()->SetPSW(0);
    for (int r = 0; r < 8; r++)
        PPU()->SetReg(r, 0);
}

TestBoard::~TestBoard()
{
    delete m_pBoard;
}

void TestBoard::SetWord(uint16_t address, uint16_t word)
{
    m_pBoard->GetCPUMemoryController()->SetWord(address, false, word);
}

void TestBoard::SetByte(uint16_t address, uint8_t byte)
{
    m_pBoard->GetCPUMemoryController()->SetByte(address, false, byte);
}

uint16_t TestBoard::GetWord(uint16_t address)
{
    int addrtype;
    return m_pBoard->GetCPUMemoryController()->GetWordView(address & 0xFFFE, false, false, &addrtype);
}

uint8_t TestBoard::GetByte(uint16_t address)
{
    uint16_t word = GetWord(address);
    return (address & 1) ? (uint8_t) (word >> 8) : (uint8_t) (word & 0xFF);
}

void TestBoard::SetPpuWord(uint16_t address, uint16_t word)
{
    m_pBoard->GetPPUMemoryController()->SetWord(address, false, word);
}

uint16_t TestBoard::GetPpuWord(uint16_t address)
{
    int addrtype;
    return m_pBoard->GetPPUMemoryController()->GetWordView(address & 0xFFFE, false, false, &addrtype);
}

void TestBoard::StartCPU()
{
    SetPpuWord(0177716, 0100000);
    CPU()->InterruptProcessing();  // the start interrupt
    CPU()->SetPSW(0);
    for (int r = 0; r < 8; r++)
        CPU()->SetReg(r, 0);
}

void TestBoard::LoadProgram(uint16_t address, const uint16_t* words, size_t count)
{
    for (size_t i = 0; i < count; i++)
        SetWord((uint16_t) (address + i * 2), words[i]);
    CPU()->SetPC(address);
}

void TestBoard::SetRegs(const uint16_t* regs)
{
    for (int r = 0; r < 8; r++)
        CPU()->SetReg(r, regs[r]);
}

void TestBoard::SetFlags(uint16_t flags)
{
    CPU()->SetPSW(flags & TF_ALL);
}

uint16_t TestBoard::GetFlags()
{
    CProcessor* pCPU = CPU();
    return (uint16_t) ((pCPU->GetN() ? TF_N : 0) | (pCPU->GetZ() ? TF_Z : 0) |
                       (pCPU->GetV() ? TF_V : 0) | (pCPU->GetC() ? TF_C : 0));
}

void TestBoard::Run(int instructions)
{
    for (int i = 0; i < instructions; i++)
        CPU()->CommandExecution();
}

bool TestBoard::TakeInterrupt()
{
    return CPU()->InterruptProcessing();
}
