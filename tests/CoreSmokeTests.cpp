#include "stdafx.h"
#include "emubase/Emubase.h"
#include "emubase/ImageSerialization.h"
#include "util/Octal.h"
#include <cstdio>
#include <cstdlib>
#include <array>
#include <vector>

#ifdef _DEBUG
int AssertFailedLine(const char* file, int line)
{
    fprintf(stderr, "Assertion failed at %s:%d\n", file, line);
    abort();
}
#endif
void DebugLog(LPCTSTR) {}
void DebugLogFormat(LPCTSTR, ...) {}
const TCHAR* REGISTER_NAME[] = { _T("R0"), _T("R1"), _T("R2"), _T("R3"),
                                _T("R4"), _T("R5"), _T("SP"), _T("PC") };

int main()
{
    CProcessor::Init();
    bool ok = true;
    ok &= ParseOctalValue(_T("177777")) == 65535;
    ok &= ParseOctalValue(_T("200000")) == std::unexpected(OctalError::out_of_range);
    ok &= ParseOctalValue(_T("19")) == std::unexpected(OctalError::invalid_digit);
    ok &= ParseOctalValue(_T("")) == std::unexpected(OctalError::empty);
    std::array<uint8_t, 1> tiny{};
    try { image::Writer writer(tiny); writer.word(1); ok = false; }
    catch (const std::out_of_range&) {}
    try { image::Reader reader(tiny); (void)reader.word(); ok = false; }
    catch (const std::out_of_range&) {}
    const std::array<uint16_t, 3> instruction{0012761, 0000001, 0000002};
    const auto decoded = DisassembleInstruction(instruction, 01000);
    ok &= decoded && decoded->length == 3 && decoded->mnemonic == _T("MOV") &&
          decoded->argument == _T("#000001, 000002(R1)");
    ok &= !DisassembleInstruction(std::span<const uint16_t>{}, 01000);
    const auto truncated = DisassembleInstruction(std::span{instruction}.first<1>(), 01000);
    ok &= truncated && truncated->length == 3 && truncated->argument == _T("#000000, 000000(R1)");
    const std::array<uint16_t, 1> jumpWithoutAddress{0000137};
    ok &= !Disasm_CheckForJump(jumpWithoutAddress, 01000);
    const std::array<uint16_t, 2> jumpToSelf{0000137, 01000};
    ok &= Disasm_CheckForJump(jumpToSelf, 01000) == 0;
    {
        CMotherboard board;
        board.Reset();
        board.SetRAMWord(0, 0xffff, 0x1234);
        ok &= board.GetRAMWord(0, 0xfffe) == 0x1234;
        ok &= board.GetRAMByte(0, 0xfffe) == 0x34;
        ok &= board.GetRAMByte(0, 0xffff) == 0x12;
        board.GetCPU()->SetReg(0, 0x1234);
        board.GetPPU()->SetReg(1, 0xabcd);
        board.GetCPUMemoryController()->SetWord(0176640, false, 0123);
        std::vector<uint8_t> snapshot(UKNCIMAGE_SIZE + 2, 0xcc);
        board.SaveToImage(std::span<uint8_t, UKNCIMAGE_SIZE>{snapshot.data() + 1, UKNCIMAGE_SIZE});  // Deliberately unaligned.
        ok &= snapshot.front() == 0xcc && snapshot.back() == 0xcc;
        ok &= snapshot[1 + 160 + 2] == 0x34 && snapshot[1 + 160 + 3] == 0x12;
        ok &= snapshot[1 + 224 + 4] == 0xcd && snapshot[1 + 224 + 5] == 0xab;
        ok &= snapshot[1 + 288] == 0123 && snapshot[1 + 289] == 0;
        for (size_t i = 1; i <= 32; ++i) ok &= snapshot[i] == 0xcc;
        CMotherboard restored;
        restored.Reset();
        restored.LoadFromImage(std::span<const uint8_t, UKNCIMAGE_SIZE>{snapshot.data() + 1, UKNCIMAGE_SIZE});
        ok &= restored.GetCPU()->GetReg(0) == 0x1234;
        ok &= restored.GetPPU()->GetReg(1) == 0xabcd;
        ok &= restored.GetCPUMemoryController()->GetWord(0176640, false, false) == 0123;
        ok &= restored.GetRAMWord(0, 0xfffe) == 0x1234;
        std::vector<uint8_t> savedAgain(UKNCIMAGE_SIZE + 2, 0xcc);
        restored.SaveToImage(std::span<uint8_t, UKNCIMAGE_SIZE>{savedAgain.data() + 1, UKNCIMAGE_SIZE});
        ok &= savedAgain == snapshot;
        ok &= !board.AttachHardImage(1, _T("?:\\invalid-ukncbtl-image"));
        ok &= !board.IsHardImageAttached(1);
        const auto cpu = board.GetCPUBusDevices();
        const auto ppu = board.GetPPUBusDevices();
        ok &= cpu.size() == 5 && ppu.size() == 7;
        for (const auto* device : cpu) ok &= device != nullptr;
        for (const auto* device : ppu) ok &= device != nullptr;
        TCHAR directory[MAX_PATH] = {}, path[MAX_PATH] = {};
        if (GetTempPath(MAX_PATH, directory) && GetTempFileName(directory, _T("hdd"), 0, path))
        {
            std::array<uint8_t, 4 * 2 * 8 * 512> image{};
            image[0] = 8;
            image[1] = 2;
            FILE* file = _tfopen(path, _T("wb"));
            bool wrote = file != nullptr && fwrite(image.data(), 1, image.size(), file) == image.size();
            if (file) wrote = fclose(file) == 0 && wrote;
            ok &= wrote;
            if (wrote)
            {
                CHardDrive drive;
                ok &= drive.AttachImage(path);
                drive.Reset();
                drive.WritePort(0x1f2, 1);
                drive.WritePort(0x1f3, 1);
                drive.WritePort(0x1f7, 0x20);
                for (int i = 0; i < 5000 && (drive.ReadPort(0x1f7) & 0x80); ++i)
                    drive.Periodic();
                ok &= (drive.ReadPort(0x1f7) & 0x09) == 0x08;
                ok &= drive.ReadPort(0x1f0) == static_cast<uint16_t>(~0x0208u);
                for (int i = 1; i < 256; ++i) drive.ReadPort(0x1f0);
                drive.WritePort(0x1f2, 1);
                drive.WritePort(0x1f3, 0); // Sector numbers are one based.
                drive.WritePort(0x1f7, 0x20);
                for (int i = 0; i < 5000 && (drive.ReadPort(0x1f7) & 0x80); ++i)
                    drive.Periodic();
                const uint16_t status = drive.ReadPort(0x1f7);
                ok &= (status & 0x01) != 0 && (status & 0x08) == 0;
            }
            if (wrote && SetFileAttributes(path, FILE_ATTRIBUTE_READONLY))
            {
                {
                    CFloppyController floppy;
                    bool attached = floppy.AttachImage(0, path);
                    if (!attached || !floppy.IsReadOnly(0)) fprintf(stderr, "floppy read-only attach failed\n");
                    ok &= attached && floppy.IsReadOnly(0);
                    floppy.SetCommand(FLOPPY_CMD_ENGINESTART | 02000 | 3);
                    for (int i = 0; i < 47; ++i) floppy.Periodic();
                    floppy.WriteData(0x1234);
                    floppy.Periodic();
                    floppy.DetachImage(0);
                    if (floppy.IsAttached(0)) fprintf(stderr, "write-protected floppy failed to detach\n");
                    ok &= !floppy.IsAttached(0);
                    ok &= (floppy.GetState() & FLOPPY_STATUS_WRITEPROTECT) != 0;
                }
                SetFileAttributes(path, FILE_ATTRIBUTE_NORMAL);
            }
            else ok = false;
            DeleteFile(path);
        }
        else ok = false;
        board.DebugTicks();
    }
    CProcessor::Done();
    if (!ok) fprintf(stderr, "Core regression test failed\n");
    return ok ? 0 : 1;
}
