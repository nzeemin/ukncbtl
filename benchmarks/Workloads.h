/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once
#include "emubase/Emubase.h"
#include <array>

namespace workloads {
constexpr uint16_t ProgramAddress = 01000;
constexpr uint16_t DataAddress = 02000;
// MOV #1,R0; ADD R0,R1; MOV R1,@#02000; BR 01000.
// Includes immediate, register, memory and branch work without ROM or files.
constexpr std::array<uint16_t, 6> Program{0012700, 1, 0060001, 0010137, DataAddress, 0000772};

inline void Prepare(CMotherboard& board)
{
    board.Reset();
    // Release the CPU from reset using the PPU's control port, as the ROM does.
    board.GetPPUMemoryController()->SetWord(0177716, false, 0100000);
    board.GetCPU()->InterruptProcessing();
    board.GetPPU()->InterruptProcessing();
    for (auto* processor : {board.GetCPU(), board.GetPPU()})
    {
        processor->SetPSW(0340); // Mask periodic interrupts for a stable workload.
        for (int reg = 0; reg < 7; ++reg) processor->SetReg(reg, 0);
        processor->SetSP(04000);
        processor->SetPC(ProgramAddress);
        processor->ClearInternalTick();
        auto* memory = processor->GetMemoryController();
        for (size_t i = 0; i < Program.size(); ++i)
            memory->SetWord(static_cast<uint16_t>(ProgramAddress + 2 * i), false, Program[i]);
        memory->SetWord(DataAddress, false, 0);
    }
}
}
