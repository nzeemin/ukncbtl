/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include <gtest/gtest.h>
#include "util/BitmapFile.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
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

class BitmapEdges : public testing::Test {
protected:
    std::array<TCHAR,MAX_PATH> path{};
    const std::array<uint32_t,4> pixels{0x00ff0000,0x0000ff00,0x000000ff,0x00ffffff};
    void SetUp() override {
        std::array<TCHAR,MAX_PATH> directory{}; ASSERT_NE(0,GetTempPath(MAX_PATH,directory.data()));
        ASSERT_NE(0,GetTempFileName(directory.data(),L"bme",0,path.data())); BitmapFile_Init();
    }
    void TearDown() override { BitmapFile_Done(); DeleteFile(path.data()); }
    bool Save(BitmapFileFormat format=BitmapFileFormatPng) { return BitmapFile_SaveImageFile(pixels,path.data(),format,2,2); }
};
TEST_F(BitmapEdges, ShortPixelSpanDoesNotOverwriteExistingImage) {
    ASSERT_TRUE(Save());
    EXPECT_FALSE(BitmapFile_SaveImageFile(std::span(pixels).first(3),path.data(),BitmapFileFormatPng,2,2));
    EXPECT_TRUE(HasExpectedPixels(path.data()));
}
TEST_F(BitmapEdges, AllFormatsRoundTripAndReleaseFileForDeletion) {
    for(auto format : {BitmapFileFormatBmp,BitmapFileFormatPng,BitmapFileFormatTiff}) {
        ASSERT_TRUE(Save(format)); EXPECT_TRUE(HasExpectedPixels(path.data())); EXPECT_TRUE(DeleteFile(path.data()));
    }
}
TEST_F(BitmapEdges, InvalidDimensionsAndShortSpanPreserveExistingFile) {
    ASSERT_TRUE(Save());
    for(auto size : {std::pair{0,2},std::pair{2,0},std::pair{-1,2},std::pair{2,-1},std::pair{INT_MAX,INT_MAX},std::pair{3,2}})
        EXPECT_FALSE(BitmapFile_SaveImageFile(pixels,path.data(),BitmapFileFormatPng,size.first,size.second));
    EXPECT_TRUE(HasExpectedPixels(path.data()));
}
TEST_F(BitmapEdges, LockedDestinationFailsAndCanBeRetried) {
    HANDLE lock=CreateFile(path.data(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr); ASSERT_NE(INVALID_HANDLE_VALUE,lock);
    EXPECT_FALSE(Save()); CloseHandle(lock); EXPECT_TRUE(Save()); EXPECT_TRUE(HasExpectedPixels(path.data()));
}
TEST_F(BitmapEdges, MissingParentFailsAndLaterSaveSucceeds) {
    const auto missing=std::wstring(path.data())+L"/missing/output.png";
    EXPECT_FALSE(BitmapFile_SaveImageFile(pixels,missing.c_str(),BitmapFileFormatPng,2,2)); EXPECT_TRUE(Save());
}
TEST_F(BitmapEdges, UnknownFormatFallsBackToPng) {
    ASSERT_TRUE(Save(static_cast<BitmapFileFormat>(99))); EXPECT_TRUE(HasExpectedPixels(path.data()));
}
TEST_F(BitmapEdges, DoneIsIdempotentAndInitializationCanBeRepeated) {
    BitmapFile_Done(); BitmapFile_Done(); BitmapFile_Init(); ASSERT_TRUE(Save());
    BitmapFile_Done(); BitmapFile_Init(); ASSERT_TRUE(Save());
}
TEST_F(BitmapEdges, RepeatedInitBalancesComInitialization) {
    BitmapFile_Init(); BitmapFile_Done();
    const HRESULT changed=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    EXPECT_TRUE(SUCCEEDED(changed)) << std::hex << changed;
    if(SUCCEEDED(changed)) CoUninitialize();
    else if(changed==RPC_E_CHANGED_MODE) CoUninitialize(); // Clean up the leaked reference on regression failure.
}
TEST(BitmapLifecycle, ExistingMultithreadedApartmentRemainsOwnedByCaller) {
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED); ASSERT_TRUE(SUCCEEDED(initialized));
    BitmapFile_Init(); HBITMAP bitmap=BitmapFile_LoadPngFromResource(MAKEINTRESOURCE(201)); EXPECT_NE(nullptr,bitmap); if(bitmap) DeleteObject(bitmap);
    BitmapFile_Done(); const HRESULT probe=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED); EXPECT_EQ(RPC_E_CHANGED_MODE,probe);
    if(SUCCEEDED(probe)) CoUninitialize(); CoUninitialize();
}
TEST_F(BitmapEdges, MissingAndMalformedResourcesFail) {
    EXPECT_EQ(nullptr,BitmapFile_LoadPngFromResource(MAKEINTRESOURCE(299)));
    EXPECT_EQ(nullptr,BitmapFile_LoadPngFromResource(MAKEINTRESOURCE(202)));
}
TEST_F(BitmapEdges, ResourceBitmapHasExpectedDimensionsAndReleasesGdiObjects) {
    HBITMAP warm=BitmapFile_LoadPngFromResource(MAKEINTRESOURCE(201)); ASSERT_NE(nullptr,warm); DeleteObject(warm);
    const DWORD before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    for(int i=0;i<20;++i) {
        HBITMAP bitmap=BitmapFile_LoadPngFromResource(MAKEINTRESOURCE(201)); ASSERT_NE(nullptr,bitmap);
        BITMAP info{}; EXPECT_NE(0,GetObject(bitmap,sizeof(info),&info)); EXPECT_EQ(659,info.bmWidth); EXPECT_EQ(752,info.bmHeight); EXPECT_EQ(32,info.bmBitsPixel);
        EXPECT_TRUE(DeleteObject(bitmap));
    }
    EXPECT_EQ(before,GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS));
}
TEST_F(BitmapEdges, OddWidthAndSinglePixelSaveWithCorrectOrientation) {
    for(auto size : {std::pair{1,1},std::pair{3,5}}) {
        const int width=size.first,height=size.second; std::vector<uint32_t> input(width*height+1,0xdeadbeef);
        for(int i=0;i<width*height;++i) input[i]=0xa0000000|static_cast<uint32_t>(i*0x010307);
        ASSERT_TRUE(BitmapFile_SaveImageFile(input,path.data(),BitmapFileFormatPng,width,height));
        IWICImagingFactory* raw=nullptr; ASSERT_TRUE(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&raw)))); ComPtr<IWICImagingFactory> factory(raw);
        IWICBitmapDecoder* decoderRaw=nullptr; ASSERT_TRUE(SUCCEEDED(factory->CreateDecoderFromFilename(path.data(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoderRaw))); ComPtr<IWICBitmapDecoder> decoder(decoderRaw);
        IWICBitmapFrameDecode* frameRaw=nullptr; ASSERT_TRUE(SUCCEEDED(decoder->GetFrame(0,&frameRaw))); ComPtr<IWICBitmapFrameDecode> frame(frameRaw);
        IWICFormatConverter* converterRaw=nullptr; ASSERT_TRUE(SUCCEEDED(factory->CreateFormatConverter(&converterRaw))); ComPtr<IWICFormatConverter> converter(converterRaw);
        ASSERT_TRUE(SUCCEEDED(converter->Initialize(frame.get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)));
        UINT w=0,h=0; ASSERT_TRUE(SUCCEEDED(converter->GetSize(&w,&h))); EXPECT_EQ(width,w); EXPECT_EQ(height,h);
        std::vector<uint32_t> output(width*height); ASSERT_TRUE(SUCCEEDED(converter->CopyPixels(nullptr,width*4,static_cast<UINT>(output.size()*4),reinterpret_cast<BYTE*>(output.data()))));
        for(int y=0;y<height;++y) for(int x=0;x<width;++x) EXPECT_EQ(0xff000000|(input[(height-1-y)*width+x]&0xffffff),output[y*width+x]);
        EXPECT_EQ(0xdeadbeef,input.back());
    }
}

HBITMAP CreateHBITMAP(IWICBitmapSource*);
class BitmapSourceStub final : public IWICBitmapSource {
public:
    UINT width=1,height=1;
    HRESULT sizeResult=S_OK,copyResult=S_OK;
    unsigned copies=0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void** result) override { *result=nullptr; return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE GetSize(UINT* w,UINT* h) override { *w=width; *h=height; return sizeResult; }
    HRESULT STDMETHODCALLTYPE GetPixelFormat(WICPixelFormatGUID* format) override { *format=GUID_WICPixelFormat32bppPBGRA; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetResolution(double* x,double* y) override { *x=*y=96; return S_OK; }
    HRESULT STDMETHODCALLTYPE CopyPalette(IWICPalette*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CopyPixels(const WICRect*,UINT stride,UINT size,BYTE* data) override {
        ++copies; EXPECT_EQ(width*4,stride); EXPECT_EQ(width*height*4,size);
        if(SUCCEEDED(copyResult)) for(UINT i=0;i<size;++i) data[i]=static_cast<BYTE>(i);
        return copyResult;
    }
};
TEST(BitmapConversion, InvalidSourceDimensionsNeverAllocateOrCopy) {
    BitmapSourceStub source;
    for(auto size : {std::pair{0u,1u},std::pair{1u,0u},std::pair{UINT_MAX,1u},std::pair{1u,UINT_MAX},std::pair{65536u,65536u}}) {
        source.width=size.first; source.height=size.second; EXPECT_EQ(nullptr,CreateHBITMAP(&source));
    }
    source.width=source.height=1; source.sizeResult=E_FAIL; EXPECT_EQ(nullptr,CreateHBITMAP(&source)); EXPECT_EQ(0,source.copies);
}
TEST(BitmapConversion, CopyFailureDeletesAllocatedBitmap) {
    BitmapSourceStub source; HBITMAP warm=CreateHBITMAP(&source); ASSERT_NE(nullptr,warm); DeleteObject(warm);
    const DWORD before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS); source.copyResult=E_FAIL;
    for(int i=0;i<20;++i) EXPECT_EQ(nullptr,CreateHBITMAP(&source));
    EXPECT_EQ(before,GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS));
}
TEST(BitmapConversion, SuccessfulCopyProducesTopDownDibWithExactBytes) {
    BitmapSourceStub source; source.width=3; source.height=2; HBITMAP bitmap=CreateHBITMAP(&source); ASSERT_NE(nullptr,bitmap);
    DIBSECTION section{}; EXPECT_NE(0,GetObject(bitmap,sizeof(section),&section)); EXPECT_EQ(2,section.dsBm.bmHeight);
    if(section.dsBm.bmBits) for(int i=0;i<24;++i) EXPECT_EQ(i,static_cast<const BYTE*>(section.dsBm.bmBits)[i]);
    // GetObject reports an absolute height. Request top-down pixels to verify orientation.
    BITMAPINFO info{}; info.bmiHeader=section.dsBmih; info.bmiHeader.biHeight=-2;
    std::array<BYTE,24> topDown{}; HDC dc=GetDC(nullptr);
    EXPECT_EQ(2,GetDIBits(dc,bitmap,0,2,topDown.data(),&info,DIB_RGB_COLORS)); ReleaseDC(nullptr,dc);
    for(int i=0;i<24;++i) if(i%4!=3) EXPECT_EQ(i,topDown[i]);
    EXPECT_EQ(1,source.copies); EXPECT_TRUE(DeleteObject(bitmap));
}
