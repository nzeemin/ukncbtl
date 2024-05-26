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
#pragma comment(lib, "WindowsCodecs.lib")


//////////////////////////////////////////////////////////////////////
// Globals

IWICImagingFactory * BitmapFile_pIWICFactory = NULL;


void BitmapFile_Init()
{
    // Initialize COM
    CoInitialize(NULL);

    CoCreateInstance(
        CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&BitmapFile_pIWICFactory));
}

void BitmapFile_Done()
{
    BitmapFile_pIWICFactory->Release();
    BitmapFile_pIWICFactory = NULL;
}


//////////////////////////////////////////////////////////////////////


HBITMAP CreateHBITMAP(IWICBitmapSource * ipBitmap)
{
    // get image attributes and check for valid image
    UINT width = 0;
    UINT height = 0;
    if (FAILED(ipBitmap->GetSize(&width, &height)) || width == 0 || height == 0)
        return NULL;

    // prepare structure giving bitmap information (negative height indicates a top-down DIB)
    BITMAPINFO bminfo;
    ZeroMemory(&bminfo, sizeof(bminfo));
    bminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bminfo.bmiHeader.biWidth = width;
    bminfo.bmiHeader.biHeight = -((LONG)height);
    bminfo.bmiHeader.biPlanes = 1;
    bminfo.bmiHeader.biBitCount = 32;
    bminfo.bmiHeader.biCompression = BI_RGB;

    // create a DIB section that can hold the image
    void * pvImageBits = NULL;
    HDC hdcScreen = ::GetDC(NULL);
    HBITMAP hbmp = ::CreateDIBSection(hdcScreen, &bminfo, DIB_RGB_COLORS, &pvImageBits, NULL, 0);
    ReleaseDC(NULL, hdcScreen);
    if (hbmp == NULL)
        return NULL;

    // extract the image into the HBITMAP

    const UINT cbStride = width * 4;
    const UINT cbImage = cbStride * height;
    if (FAILED(ipBitmap->CopyPixels(NULL, cbStride, cbImage, static_cast<BYTE *>(pvImageBits))))
    {
        // couldn't extract image; delete HBITMAP
        ::DeleteObject(hbmp);
        return NULL;
    }

    return hbmp;
}

HBITMAP BitmapFile_LoadPngFromResource(LPCTSTR lpName)
{
    IWICImagingFactory * piFactory = BitmapFile_pIWICFactory;
    ASSERT(piFactory != NULL);

    HRESULT hr = NULL;

    // find the resource
    HRSRC hrsrc = FindResource(NULL, lpName, _T("IMAGE"));
    if (hrsrc == NULL)
        return NULL;

    // load the resource
    DWORD dwResourceSize = ::SizeofResource(NULL, hrsrc);
    HGLOBAL hglbImage = ::LoadResource(NULL, hrsrc);
    if (hglbImage == NULL)
        return NULL;

    // lock the resource, getting a pointer to its data
    LPVOID pvSourceResourceData = ::LockResource(hglbImage);
    if (pvSourceResourceData == NULL)
        return NULL;

    IWICStream *pIWICStream = NULL;
    hr = piFactory->CreateStream(&pIWICStream);
    if (hr != S_OK)
        return NULL;

    hr = pIWICStream->InitializeFromMemory(reinterpret_cast<BYTE*>(pvSourceResourceData), dwResourceSize);
    if (hr != S_OK)
        return NULL;

    IWICBitmapDecoder *pIDecoder = NULL;
    hr = piFactory->CreateDecoderFromStream(pIWICStream, NULL, WICDecodeMetadataCacheOnLoad, &pIDecoder);
    if (hr != S_OK)
        return NULL;

    IWICBitmapFrameDecode *pIDecoderFrame = NULL;
    hr = pIDecoder->GetFrame(0, &pIDecoderFrame);
    ::FreeResource(hglbImage);
    if (hr != S_OK)
        return NULL;

    // convert the image to 32bpp BGRA format with pre-multiplied alpha
    //   (it may not be stored in that format natively in the PNG resource,
    //   but we need this format to create the DIB to use on-screen)
    IWICBitmapSource * ipBitmap = NULL;
    hr = WICConvertBitmapSource(GUID_WICPixelFormat32bppPBGRA, pIDecoderFrame, &ipBitmap);
    pIDecoderFrame->Release();
    if (hr != S_OK)
        return NULL;

    // create a HBITMAP containing the image
    HBITMAP hbmp = CreateHBITMAP(ipBitmap);
    ipBitmap->Release();
    if (hbmp == NULL)
        return NULL;

    pIWICStream->Release();
    return hbmp;
}


//////////////////////////////////////////////////////////////////////


bool BitmapFile_SaveImageFile(
    const uint32_t* pBits,
    LPCTSTR sFileName, BitmapFileFormat format,
    int width, int height)
{
    ASSERT(pBits != NULL);
    ASSERT(sFileName != NULL);

    IWICImagingFactory * piFactory = BitmapFile_pIWICFactory;
    ASSERT(piFactory != NULL);

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

    bool result = false;
    HRESULT hr = NULL;
    IWICStream *piStream = NULL;
    IWICBitmapFrameEncode *piBitmapFrame = NULL;
    IPropertyBag2 *pPropertyBag = NULL;
    WICPixelFormatGUID formatGUID = GUID_WICPixelFormat24bppBGR;

    IWICBitmapEncoder *piEncoder = NULL;
    hr = piFactory->CreateEncoder(containerFormatGuid, NULL, &piEncoder);
    if (hr != S_OK) goto Cleanup;

    hr = piFactory->CreateStream(&piStream);
    if (hr != S_OK) goto Cleanup;

    hr = piStream->InitializeFromFilename(sFileName, GENERIC_WRITE);
    if (hr != S_OK) goto Cleanup;

    hr = piEncoder->Initialize(piStream, WICBitmapEncoderNoCache);
    if (hr != S_OK) goto Cleanup;

    hr = piEncoder->CreateNewFrame(&piBitmapFrame, &pPropertyBag);
    if (hr != S_OK) goto Cleanup;

    hr = piBitmapFrame->Initialize(pPropertyBag);
    if (hr != S_OK) goto Cleanup;

    hr = piBitmapFrame->SetSize(width, height);
    if (hr != S_OK) goto Cleanup;

    hr = piBitmapFrame->SetPixelFormat(&formatGUID);
    if (hr != S_OK) goto Cleanup;

    hr = IsEqualGUID(formatGUID, GUID_WICPixelFormat24bppBGR) ? S_OK : E_FAIL;
    if (hr != S_OK) goto Cleanup;

    // Convert upside-down 32bpp bitmap to normal 24bpp bitmap
    UINT cbStride = (width * 24 + 7) / 8;
    UINT cbBufferSize = height * cbStride;
    BYTE *pbBuffer = new BYTE[cbBufferSize];
    for (int line = 0; line < height; line++)
    {
        const BYTE* pSrc = ((const BYTE*)pBits) + (height - line - 1) * width * 4;
        BYTE* pDst = pbBuffer + line * width * 3;
        for (int x = 0; x < width; x++)
        {
            *pDst++ = *pSrc++;
            *pDst++ = *pSrc++;
            *pDst++ = *pSrc++;
            pSrc++;
        }
    }

    hr = piBitmapFrame->WritePixels(height, cbStride, cbBufferSize, pbBuffer);
    delete[] pbBuffer;
    if (hr != S_OK) goto Cleanup;

    hr = piBitmapFrame->Commit();
    if (hr != S_OK) goto Cleanup;

    hr = piEncoder->Commit();
    if (hr != S_OK) goto Cleanup;

    result = true;

Cleanup:
    if (piEncoder != NULL)
        piEncoder->Release();
    if (piBitmapFrame != NULL)
        piBitmapFrame->Release();
    if (pPropertyBag != NULL)
        pPropertyBag->Release();
    if (piStream != NULL)
        piStream->Release();

    return result;
}


//////////////////////////////////////////////////////////////////////
