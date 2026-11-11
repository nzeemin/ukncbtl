/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// BitmapFile.cpp

#include "stdafx.h"
#include "BitmapFile.h"

#include <wincodec.h>
#include <wincodecsdk.h>
#include <vector>
#include <memory>

template <typename T> struct ComRelease
{
    void operator()(T* value) const { if (value != nullptr) value->Release(); }
};
#pragma comment(lib, "WindowsCodecs.lib")


//////////////////////////////////////////////////////////////////////
// Globals

static std::unique_ptr<IWICImagingFactory, ComRelease<IWICImagingFactory>> BitmapFile_factory;
static bool BitmapFile_comInitialized = false;


void BitmapFile_Init()
{
    if (BitmapFile_factory)
        return;
    const HRESULT initialized = BitmapFile_comInitialized ? S_FALSE : CoInitialize(nullptr);
    BitmapFile_comInitialized = SUCCEEDED(initialized);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        return;
    IWICImagingFactory* factory = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&factory))))
        BitmapFile_factory.reset(factory);
}

void BitmapFile_Done()
{
    BitmapFile_factory.reset();
    if (BitmapFile_comInitialized)
        CoUninitialize();
    BitmapFile_comInitialized = false;
}


//////////////////////////////////////////////////////////////////////


HBITMAP CreateHBITMAP(IWICBitmapSource * ipBitmap)
{
    // get image attributes and check for valid image
    UINT width = 0;
    UINT height = 0;
    if (FAILED(ipBitmap->GetSize(&width, &height)) || width == 0 || height == 0 ||
        width > LONG_MAX || height > LONG_MAX || static_cast<uint64_t>(width) * height * 4 > UINT_MAX)
        return nullptr;

    // prepare structure giving bitmap information (negative height indicates a top-down DIB)
    BITMAPINFO bminfo{};
    bminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bminfo.bmiHeader.biWidth = width;
    bminfo.bmiHeader.biHeight = -((LONG)height);
    bminfo.bmiHeader.biPlanes = 1;
    bminfo.bmiHeader.biBitCount = 32;
    bminfo.bmiHeader.biCompression = BI_RGB;

    // create a DIB section that can hold the image
    void * pvImageBits = nullptr;
    HDC hdcScreen = ::GetDC(nullptr);
    HBITMAP hbmp = ::CreateDIBSection(hdcScreen, &bminfo, DIB_RGB_COLORS, &pvImageBits, nullptr, 0);
    ReleaseDC(nullptr, hdcScreen);
    if (hbmp == nullptr)
        return nullptr;

    // extract the image into the HBITMAP

    const UINT cbStride = width * 4;
    const UINT cbImage = cbStride * height;
    if (FAILED(ipBitmap->CopyPixels(nullptr, cbStride, cbImage, static_cast<BYTE *>(pvImageBits))))
    {
        // couldn't extract image; delete HBITMAP
        ::DeleteObject(hbmp);
        return nullptr;
    }

    return hbmp;
}

HBITMAP BitmapFile_LoadPngFromResource(LPCTSTR lpName)
{
    IWICImagingFactory * piFactory = BitmapFile_factory.get();
    ASSERT(piFactory != nullptr);
    if (piFactory == nullptr)
        return nullptr;

    HRESULT hr = S_OK;

    // find the resource
    HRSRC hrsrc = FindResource(nullptr, lpName, _T("IMAGE"));
    if (hrsrc == nullptr)
        return nullptr;

    // load the resource
    DWORD dwResourceSize = ::SizeofResource(nullptr, hrsrc);
    HGLOBAL hglbImage = ::LoadResource(nullptr, hrsrc);
    if (hglbImage == nullptr)
        return nullptr;

    // lock the resource, getting a pointer to its data
    LPVOID pvSourceResourceData = ::LockResource(hglbImage);
    if (pvSourceResourceData == nullptr)
        return nullptr;

    IWICStream* rawStream = nullptr;
    hr = piFactory->CreateStream(&rawStream);
    std::unique_ptr<IWICStream, ComRelease<IWICStream>> pIWICStream(rawStream);
    if (hr != S_OK)
        return nullptr;

    hr = pIWICStream->InitializeFromMemory(reinterpret_cast<BYTE*>(pvSourceResourceData), dwResourceSize);
    if (hr != S_OK)
        return nullptr;

    IWICBitmapDecoder* rawDecoder = nullptr;
    hr = piFactory->CreateDecoderFromStream(pIWICStream.get(), nullptr, WICDecodeMetadataCacheOnLoad, &rawDecoder);
    std::unique_ptr<IWICBitmapDecoder, ComRelease<IWICBitmapDecoder>> pIDecoder(rawDecoder);
    if (hr != S_OK)
        return nullptr;

    IWICBitmapFrameDecode* rawFrame = nullptr;
    hr = pIDecoder->GetFrame(0, &rawFrame);
    std::unique_ptr<IWICBitmapFrameDecode, ComRelease<IWICBitmapFrameDecode>> pIDecoderFrame(rawFrame);
    ::FreeResource(hglbImage);
    if (hr != S_OK)
        return nullptr;

    // convert the image to 32bpp BGRA format with pre-multiplied alpha
    //   (it may not be stored in that format natively in the PNG resource,
    //   but we need this format to create the DIB to use on-screen)
    IWICBitmapSource* rawBitmap = nullptr;
    hr = WICConvertBitmapSource(GUID_WICPixelFormat32bppPBGRA, pIDecoderFrame.get(), &rawBitmap);
    std::unique_ptr<IWICBitmapSource, ComRelease<IWICBitmapSource>> ipBitmap(rawBitmap);
    if (hr != S_OK)
        return nullptr;

    // create a HBITMAP containing the image
    HBITMAP hbmp = CreateHBITMAP(ipBitmap.get());
    if (hbmp == nullptr)
        return nullptr;

    return hbmp;
}


//////////////////////////////////////////////////////////////////////


bool BitmapFile_SaveImageFile(
    std::span<const uint32_t> pixels,
    LPCTSTR sFileName, BitmapFileFormat format,
    int width, int height)
{
    ASSERT(sFileName != nullptr);

    IWICImagingFactory * piFactory = BitmapFile_factory.get();
    ASSERT(piFactory != nullptr);
    if (piFactory == nullptr || sFileName == nullptr || width <= 0 || height <= 0 ||
        static_cast<uint64_t>(width) * height * 4 > UINT_MAX ||
        static_cast<uint64_t>(width) * height > pixels.size())
        return false;

    GUID containerFormatGuid = GUID_ContainerFormatPng;
    switch (format)
    {
    case BitmapFileFormatBmp:
        containerFormatGuid = GUID_ContainerFormatBmp; break;
    case BitmapFileFormatTiff:
        containerFormatGuid = GUID_ContainerFormatTiff; break;
    default:
        containerFormatGuid = GUID_ContainerFormatPng;
    }

    WICPixelFormatGUID formatGUID = GUID_WICPixelFormat24bppBGR;
    IWICBitmapEncoder* rawEncoder = nullptr;
    if (FAILED(piFactory->CreateEncoder(containerFormatGuid, nullptr, &rawEncoder))) return false;
    std::unique_ptr<IWICBitmapEncoder, ComRelease<IWICBitmapEncoder>> encoder(rawEncoder);
    IWICStream* rawStream = nullptr;
    if (FAILED(piFactory->CreateStream(&rawStream))) return false;
    std::unique_ptr<IWICStream, ComRelease<IWICStream>> stream(rawStream);
    if (FAILED(stream->InitializeFromFilename(sFileName, GENERIC_WRITE)) ||
        FAILED(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache))) return false;
    IWICBitmapFrameEncode* rawFrame = nullptr;
    IPropertyBag2* rawProperties = nullptr;
    const HRESULT frameResult = encoder->CreateNewFrame(&rawFrame, &rawProperties);
    std::unique_ptr<IWICBitmapFrameEncode, ComRelease<IWICBitmapFrameEncode>> frame(rawFrame);
    std::unique_ptr<IPropertyBag2, ComRelease<IPropertyBag2>> properties(rawProperties);
    if (FAILED(frameResult)) return false;
    if (FAILED(frame->Initialize(properties.get())) || FAILED(frame->SetSize(width, height)) ||
        FAILED(frame->SetPixelFormat(&formatGUID)) || !IsEqualGUID(formatGUID, GUID_WICPixelFormat24bppBGR))
        return false;

    // Convert upside-down 32bpp bitmap to normal 24bpp bitmap
    const UINT cbStride = static_cast<UINT>(width) * 3;
    const UINT cbBufferSize = static_cast<UINT>(height) * cbStride;
    std::vector<BYTE> pixelBuffer(cbBufferSize);
    for (int line = 0; line < height; line++)
    {
        BYTE* pDst = pixelBuffer.data() + static_cast<size_t>(line) * cbStride;
        for (int x = 0; x < width; x++)
        {
            const uint32_t pixel = pixels[static_cast<size_t>(height - line - 1) * width + x];
            *pDst++ = static_cast<BYTE>(pixel);
            *pDst++ = static_cast<BYTE>(pixel >> 8);
            *pDst++ = static_cast<BYTE>(pixel >> 16);
        }
    }
    return SUCCEEDED(frame->WritePixels(height, cbStride, cbBufferSize, pixelBuffer.data())) &&
           SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
}


//////////////////////////////////////////////////////////////////////
