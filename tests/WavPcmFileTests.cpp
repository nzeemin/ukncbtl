#include "stdafx.h"
#include "util/WavPcmFile.h"
#include <array>
#include <cstdio>
#include <vector>

#ifdef _DEBUG
int AssertFailedLine(const char* file, int line)
{
    std::fprintf(stderr, "Assertion failed at %s:%d\n", file, line);
    std::abort();
}
#endif

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
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    return std::fclose(file) == 0 && ok;
}
bool Check(bool condition, const char* message)
{
    if (!condition) std::fprintf(stderr, "%s\n", message);
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
    const uint8_t afterData[] = {'L','I','S','T',2,0,0,0,0xaa,0xbb};
    bytes.insert(bytes.end(), afterData, afterData + sizeof(afterData));
    Put32(bytes, 4, static_cast<uint32_t>(bytes.size() - 8));
    ok &= Check(Write(path, bytes), "could not create fixture");
    {
        auto wav = WavPcmFile::Open(path);
        ok &= Check(wav.has_value(), "valid high-rate WAV rejected");
        if (wav) {
            ok &= Check(wav->Frequency() == 96000 && wav->Length() == 2 &&
                        wav->ReadOne() == 0x80000000u && wav->ReadOne() == 0x81000000u,
                        "8-bit WAV sample mismatch");
            ok &= Check(wav->ReadOne() == std::unexpected(WavError::end_of_data), "read past data chunk");
            ok &= Check(wav->SetPosition(0).has_value() && wav->ReadOne() == 0x80000000u, "rewind failed");
            const auto oldPosition = wav->Position();
            ok &= Check(wav->SetPosition(UINT_MAX) == std::unexpected(WavError::invalid_position) &&
                        wav->Position() == oldPosition, "invalid seek changed the position");
        }
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
        ok &= Check(Write(path, bytes), "could not create multibyte fixture");
        auto wav = WavPcmFile::Open(path);
        ok &= Check(wav.has_value(), "multibyte WAV rejected");
        if (wav) {
            const auto expected = bits == 16 ? 0x92340000u : 0xd6781234u;
            ok &= Check(wav->Length() == 1 && wav->ReadOne() == expected, "signed WAV sample mismatch");
        }
    }
    for (int bits : {16, 32}) {
        for (const uint8_t fill : {uint8_t{0}, uint8_t{0xff}}) {
            bytes = Wave(); bytes[32] = static_cast<uint8_t>(bits / 8); bytes[34] = static_cast<uint8_t>(bits);
            Put32(bytes, 28, 96000 * (bits / 8)); Put32(bytes, 40, bits / 8);
            bytes.resize(44 + bits / 8, fill);
            for (size_t i = 44; i < bytes.size(); ++i) bytes[i] = fill;
            Put32(bytes, 4, static_cast<uint32_t>(bytes.size() - 8));
            ok &= Check(Write(path, bytes), "could not create signed WAV fixture");
            auto wav = WavPcmFile::Open(path);
            const uint32_t expected = fill == 0 ? 0x80000000u : (bits == 16 ? 0x7fff0000u : 0x7fffffffu);
            ok &= Check(wav && wav->ReadOne() == expected, "signed zero or negative sample mismatch");
        }
    }
    for (int alignment : {0, 17}) {
        bytes = Wave(); bytes[32] = static_cast<uint8_t>(alignment);
        ok &= Check(Write(path, bytes), "could not create invalid fixture");
        auto wav = WavPcmFile::Open(path);
        ok &= Check(!wav && wav.error() == WavError::invalid_format, "invalid alignment accepted");
    }
    bytes = Wave(); bytes[12] = 'x';
    ok &= Check(Write(path, bytes), "could not create missing-format fixture");
    ok &= Check(!WavPcmFile::Open(path), "missing format accepted");
    {
        auto wav = WavPcmFile::Create(path, 44100);
        ok &= Check(wav.has_value(), "could not create WAV");
        if (wav) {
            ok &= Check(wav->WriteOne(0x80000000u).has_value() && wav->WriteOne(0x81000000u).has_value(), "write failed");
            ok &= Check(wav->SetPosition(0).has_value() && wav->WriteOne(0x82000000u).has_value(), "overwrite failed");
            ok &= Check(wav->Length() == 2, "overwrite incorrectly extended the recording");
            ok &= Check(wav->Close().has_value(), "could not finalize WAV");
        }
    }
    {
        auto wav = WavPcmFile::Open(path);
        ok &= Check(wav && wav->Length() == 2 && wav->ReadOne() == 0x82000000u && wav->ReadOne() == 0x81000000u,
                    "rewritten recording did not round trip");
    }
    {
        auto wav = WavPcmFile::Create(path, 44100);
        ok &= Check(wav && wav->WriteOne(0x80000000u).has_value() && wav->Close().has_value(), "odd recording failed");
    }
    {
        auto wav = WavPcmFile::Open(path);
        ok &= Check(wav && wav->Length() == 1 && wav->ReadOne() == 0x80000000u, "odd recording rejected");
    }
    DeleteFile(path);
    return ok ? 0 : 1;
}
