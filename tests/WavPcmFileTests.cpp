#include "stdafx.h"
#include "util/WavPcmFile.h"
#include <cstdio>
#include <vector>

int AssertFailedLine(const char* file, int line)
{
    fprintf(stderr, "Assertion failed at %s:%d\n", file, line);
    abort();
}

namespace {
void Put32(std::vector<uint8_t>& bytes, size_t pos, uint32_t value)
{
    for (int i = 0; i < 4; ++i) bytes[pos + i] = static_cast<uint8_t>(value >> (8 * i));
}
std::vector<uint8_t> Wave()
{
    const uint8_t raw[] = {
        'R','I','F','F', 38,0,0,0, 'W','A','V','E',
        'f','m','t',' ', 16,0,0,0, 1,0, 1,0,
        0,0,0,0, 0,0,0,0, 1,0, 8,0,
        'd','a','t','a', 2,0,0,0, 0x80,0x81
    };
    std::vector<uint8_t> result(raw, raw + sizeof(raw));
    Put32(result, 24, 96000);
    Put32(result, 28, 96000);
    return result;
}
bool Write(const TCHAR* path, const std::vector<uint8_t>& bytes)
{
    FILE* file = _tfopen(path, _T("wb"));
    if (!file) return false;
    bool ok = fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    return fclose(file) == 0 && ok;
}
bool Check(bool condition, const char* message)
{
    if (!condition) fprintf(stderr, "%s\n", message);
    return condition;
}
}

int main()
{
    TCHAR directory[MAX_PATH] = {}, path[MAX_PATH] = {};
    if (!GetTempPath(MAX_PATH, directory) || !GetTempFileName(directory, _T("wav"), 0, path)) return 1;
    bool ok = true;
    auto bytes = Wave();
    const uint8_t junk[] = {'J','U','N','K',1,0,0,0,42,0};
    bytes.insert(bytes.begin() + 36, junk, junk + sizeof(junk));
    Put32(bytes, 4, static_cast<uint32_t>(bytes.size() - 8));
    ok &= Check(Write(path, bytes), "could not create fixture");
    auto wav = WavPcmFile_Open(path);
    ok &= Check(wav != INVALID_HANDLE_VALUE, "valid high-rate WAV rejected");
    if (wav != INVALID_HANDLE_VALUE) {
        ok &= Check(WavPcmFile_GetFrequency(wav) == 96000 && WavPcmFile_GetLength(wav) == 2 &&
                    WavPcmFile_ReadOne(wav) == 0x80000000u, "WAV sample mismatch");
        WavPcmFile_Close(wav);
    }
    for (int bits : {16, 32}) {
        bytes = Wave();
        bytes[32] = static_cast<uint8_t>(bits / 8);
        bytes[34] = static_cast<uint8_t>(bits);
        Put32(bytes, 28, 96000 * (bits / 8));
        Put32(bytes, 40, bits / 8);
        bytes[44] = 0x34; bytes[45] = 0x12;
        if (bits == 32) { bytes.push_back(0x78); bytes.push_back(0x56); }
        Put32(bytes, 4, static_cast<uint32_t>(bytes.size() - 8));
        ok &= Check(Write(path, bytes), "could not create multibyte WAV fixture");
        wav = WavPcmFile_Open(path);
        ok &= Check(wav != INVALID_HANDLE_VALUE, "multibyte WAV rejected");
        if (wav != INVALID_HANDLE_VALUE) {
            const auto expected = bits == 16 ? 0x12340000u : 0x56781234u;
            ok &= Check(WavPcmFile_GetLength(wav) == 1 && WavPcmFile_ReadOne(wav) == expected,
                        "multibyte WAV sample mismatch");
            WavPcmFile_Close(wav);
        }
    }
    for (int alignment : {0, 17}) {
        bytes = Wave(); bytes[32] = static_cast<uint8_t>(alignment);
        ok &= Check(Write(path, bytes), "could not create invalid fixture");
        wav = WavPcmFile_Open(path);
        ok &= Check(wav == INVALID_HANDLE_VALUE, "invalid alignment accepted");
        if (wav != INVALID_HANDLE_VALUE) WavPcmFile_Close(wav);
    }
    bytes = Wave(); bytes[12] = 'x';
    ok &= Check(Write(path, bytes), "could not create missing-format fixture");
    wav = WavPcmFile_Open(path);
    ok &= Check(wav == INVALID_HANDLE_VALUE, "missing format accepted");
    if (wav != INVALID_HANDLE_VALUE) WavPcmFile_Close(wav);
    wav = WavPcmFile_Create(path, 44100);
    ok &= Check(wav != INVALID_HANDLE_VALUE, "could not create WAV");
    if (wav != INVALID_HANDLE_VALUE) {
        ok &= Check(WavPcmFile_WriteOne(wav, 0x80000000u), "could not write sample");
        WavPcmFile_Close(wav);
        wav = WavPcmFile_Open(path);
        ok &= Check(wav != INVALID_HANDLE_VALUE, "odd-length recording rejected");
        if (wav != INVALID_HANDLE_VALUE) {
            ok &= Check(WavPcmFile_GetLength(wav) == 1, "wrong recording length");
            WavPcmFile_Close(wav);
        }
    }
    DeleteFile(path);
    return ok ? 0 : 1;
}
