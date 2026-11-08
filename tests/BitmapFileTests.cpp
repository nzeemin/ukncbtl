/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include <gtest/gtest.h>
#include "util/BitmapFile.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <wincodec.h>

#ifdef _DEBUG
BOOL AssertFailedLine(LPCSTR file, int line)
{
    std::fprintf(stderr, "Assertion failed at %s:%d\n", file, line);
    std::abort();
}
#endif

template<class T> struct ReleaseCom
{
    void operator()(T* value) const { if (value) value->Release(); }
};
template<class T> using ComPtr = std::unique_ptr<T, ReleaseCom<T>>;

bool HasExpectedPixels(LPCTSTR path)
{
    IWICImagingFactory* rawFactory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&rawFactory)))) return false;
    ComPtr<IWICImagingFactory> factory(rawFactory);
    IWICBitmapDecoder* rawDecoder = nullptr;
    if (FAILED(factory->CreateDecoderFromFilename(path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &rawDecoder))) return false;
    ComPtr<IWICBitmapDecoder> decoder(rawDecoder);
    IWICBitmapFrameDecode* rawFrame = nullptr;
    if (FAILED(decoder->GetFrame(0, &rawFrame))) return false;
    ComPtr<IWICBitmapFrameDecode> frame(rawFrame);
    IWICFormatConverter* rawConverter = nullptr;
    if (FAILED(factory->CreateFormatConverter(&rawConverter))) return false;
    ComPtr<IWICFormatConverter> converter(rawConverter);
    if (FAILED(converter->Initialize(frame.get(), GUID_WICPixelFormat32bppBGRA,
                                     WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) return false;
    std::array<uint8_t, 16> pixels{};
    if (FAILED(converter->CopyPixels(nullptr, 8, static_cast<UINT>(pixels.size()), pixels.data()))) return false;
    // WIC rows are top-down; each pixel is BGRA.
    constexpr std::array<uint8_t, 16> expected{
        0xff,0x00,0x00,0xff, 0xff,0xff,0xff,0xff,
        0x00,0x00,0xff,0xff, 0x00,0xff,0x00,0xff};
    return pixels == expected;
}

static int RunRegressionChecks()
{
    TCHAR directory[MAX_PATH]{}, path[MAX_PATH]{};
    if (!GetTempPath(MAX_PATH, directory) || !GetTempFileName(directory, _T("img"), 0, path)) return 1;
    BitmapFile_Init();
    // Source is bottom-up: red and green below blue and white.
    constexpr std::array<uint32_t, 4> pixels{0x00ff0000, 0x0000ff00, 0x000000ff, 0x00ffffff};
    bool ok = !BitmapFile_SaveImageFile(std::span{pixels}.first<3>(), path, BitmapFileFormatPng, 2, 2);
    ok &= !BitmapFile_SaveImageFile(pixels, path, BitmapFileFormatPng, 0, 2);
    for (const auto format : {BitmapFileFormatPng, BitmapFileFormatBmp})
        ok &= BitmapFile_SaveImageFile(pixels, path, format, 2, 2) && HasExpectedPixels(path);
    BitmapFile_Done();
    DeleteFile(path);
    if (!ok) std::fprintf(stderr, "bitmap round-trip failed\n");
    return ok ? 0 : 1;
}

TEST(BitmapFileTests, RegressionChecks)
{
    EXPECT_EQ(RunRegressionChecks(), 0);
}
