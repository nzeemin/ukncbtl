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


//////////////////////////////////////////////////////////////////////

BOOL BmpFile_SaveScreenshot(
    const DWORD* pBits,
    const DWORD* palette,
    LPCTSTR sFileName)
{
    ASSERT(pBits != NULL);
    ASSERT(palette != NULL);
    ASSERT(sFileName != NULL);

    // Create file
    HANDLE hFile = ::CreateFile(sFileName,
            GENERIC_WRITE, FILE_SHARE_READ, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    BITMAPFILEHEADER hdr;
    ::ZeroMemory(&hdr, sizeof(hdr));
    hdr.bfType = 0x4d42;  // "BM"
    BITMAPINFOHEADER bih;
    ::ZeroMemory(&bih, sizeof(bih));
    bih.biSize = sizeof( BITMAPINFOHEADER );
    bih.biWidth = UKNC_SCREEN_WIDTH;
    bih.biHeight = UKNC_SCREEN_HEIGHT;
    bih.biSizeImage = bih.biWidth * bih.biHeight / 2;
    bih.biPlanes = 1;
    bih.biBitCount = 4;
    bih.biCompression = BI_RGB;
    bih.biXPelsPerMeter = bih.biXPelsPerMeter = 2000;
    hdr.bfSize = (DWORD) sizeof(BITMAPFILEHEADER) + bih.biSize + bih.biSizeImage;
    hdr.bfOffBits = (DWORD) sizeof(BITMAPFILEHEADER) + bih.biSize + sizeof(RGBQUAD) * 16;

    DWORD dwBytesWritten = 0;

    BYTE * pData = (BYTE *) ::malloc(bih.biSizeImage);

    // Prepare the image data
    const DWORD * psrc = pBits;
    BYTE * pdst = pData;
    for (int i = 0; i < 640 * 288; i++)
    {
        DWORD rgb = *psrc;
        psrc++;
        BYTE color = 0;
        for (BYTE c = 0; c < 16; c++)
        {
            if (palette[c] == rgb)
            {
                color = c;
                break;
            }
        }
        if ((i & 1) == 0)
            *pdst = (color << 4);
        else
        {
            *pdst = (*pdst) & 0xf0 | color;
            pdst++;
        }
    }

    WriteFile(hFile, &hdr, sizeof(BITMAPFILEHEADER), &dwBytesWritten, NULL);
    if (dwBytesWritten != sizeof(BITMAPFILEHEADER))
    {
        ::free(pData);
        return FALSE;
    }
    WriteFile(hFile, &bih, sizeof(BITMAPINFOHEADER), &dwBytesWritten, NULL);
    if (dwBytesWritten != sizeof(BITMAPINFOHEADER))
    {
        ::free(pData);
        return FALSE;
    }
    WriteFile(hFile, palette, sizeof(RGBQUAD) * 16, &dwBytesWritten, NULL);
    if (dwBytesWritten != sizeof(RGBQUAD) * 16)
    {
        ::free(pData);
        return FALSE;
    }
    WriteFile(hFile, pData, bih.biSizeImage, &dwBytesWritten, NULL);
    ::free(pData);
    if (dwBytesWritten != bih.biSizeImage)
        return FALSE;

    // Close file
    CloseHandle(hFile);

    return TRUE;
}

// Declaration for CRC calculation function, see the definition below
unsigned long crc(unsigned char *buf, int len);

// Declaration for ADLER32 calculation function, see the definition below
unsigned long update_adler32(unsigned long adler, unsigned char *buf, int len);

DWORD ReadValueMSB(const BYTE * buffer)
{
  DWORD value = *(buffer++);  value <<= 8;
  value |= *(buffer++);  value <<= 8;
  value |= *(buffer++);  value <<= 8;
  value |= *buffer;
  return value;
}

void SaveValueMSB(BYTE * buffer, DWORD value)
{
    *(buffer++) = (BYTE)(value >> 24);
    *(buffer++) = (BYTE)(value >> 16);
    *(buffer++) = (BYTE)(value >> 8);
    * buffer    = (BYTE)(value);
}

void SavePngChunkChecksum(BYTE * chunk)
{
    DWORD datalen = ReadValueMSB(chunk);
    DWORD value = crc(chunk + 4, datalen + 4);
    BYTE* crcplace = (chunk + 8 + datalen);
    SaveValueMSB(crcplace, value);
}

BOOL PngFile_SaveScreenshot(
    const DWORD* pBits,
    const DWORD* palette,
    LPCTSTR sFileName)
{
    ASSERT(pBits != NULL);
    ASSERT(palette != NULL);
    ASSERT(sFileName != NULL);

    // Create file
    HANDLE hFile = ::CreateFile(sFileName,
            GENERIC_WRITE, FILE_SHARE_READ, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    DWORD dwBytesWritten = 0;

    const BYTE pngheader[] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
    WriteFile(hFile, pngheader, sizeof(pngheader), &dwBytesWritten, NULL);
    if (dwBytesWritten != sizeof(pngheader))
        return FALSE;

    { // Write IHDR chunk
        BYTE IHDRchunk[12 + 13];
        SaveValueMSB(IHDRchunk, 13);
        memcpy(IHDRchunk + 4, "IHDR", 4);
        SaveValueMSB(IHDRchunk + 8, UKNC_SCREEN_WIDTH);
        SaveValueMSB(IHDRchunk + 12, UKNC_SCREEN_HEIGHT);
        *(IHDRchunk + 16) = 4;  // Bit depth
        *(IHDRchunk + 17) = 3;  // Color type: indexed color
        *(IHDRchunk + 18) = 0;  // No compression
        *(IHDRchunk + 19) = 0;  // No filter
        *(IHDRchunk + 20) = 0;  // No interlace
        SavePngChunkChecksum(IHDRchunk);
        WriteFile(hFile, IHDRchunk, sizeof(IHDRchunk), &dwBytesWritten, NULL);
        if (dwBytesWritten != sizeof(IHDRchunk))
            return FALSE;
    }

    { // Write PLTE chunk
        BYTE PLTEchunk[12 + 16*3];
        SaveValueMSB(PLTEchunk, 16*3);
        memcpy(PLTEchunk + 4, "PLTE", 4);
        BYTE * p = PLTEchunk + 8;
        for (int i = 0; i < 16; i++)
        {
            DWORD color = *(palette++);
            *(p++) = (BYTE)(color >> 16);
            *(p++) = (BYTE)(color >> 8);
            *(p++) = (BYTE)(color >> 0);
        }
        SavePngChunkChecksum(PLTEchunk);
        WriteFile(hFile, PLTEchunk, sizeof(PLTEchunk), &dwBytesWritten, NULL);
        if (dwBytesWritten != sizeof(PLTEchunk))
            return FALSE;
    }

    // The IDAT chunk data format defined by RFC-1950 "ZLIB Compressed Data Format Specification version 3.3"
    // http://www.ietf.org/rfc/rfc1950.txt
    // We use uncomressed DEFLATE format, see RFC-1951
    // http://tools.ietf.org/html/rfc1951
    const DWORD pDataLength = 8 + 2 + (6 + UKNC_SCREEN_WIDTH / 2) * UKNC_SCREEN_HEIGHT + 4/*adler*/ + 4;
    BYTE * pData = (BYTE *) ::malloc(pDataLength);
    SaveValueMSB(pData, pDataLength - 12);
    memcpy(pData + 4, "IDAT", 4);

    const BYTE cmf = 8;
    pData[8] = cmf;                           // CM = 8, CMINFO = 0
    pData[9] = (31 - ((cmf << 8) % 31)) % 31; // FCHECK (FDICT/FLEVEL=0)

    BYTE * pdst = pData + 10;
    DWORD adler = 1L;
    for (int line = 0; line < 288; line++)
    {
        const WORD linelen = 320 + 1;  // Each line is 321-byte block of non-compressed data
        *(pdst++) = (line < 288-1) ? 0 : 1;  // Last?
        *(pdst++) = linelen & 0xff;
        *(pdst++) = (linelen >> 8) & 0xff;
        *(pdst++) = ~linelen & 0xff;
        *(pdst++) = (~linelen >> 8) & 0xff;

        BYTE * pline = pdst;
        *(pdst++) = 0;  // additional "filter-type" byte at the beginning of every scanline
        const DWORD * psrc = pBits + ((288-1 - line) * 640);
        for (int i = 0; i < 640; i++)
        {
            DWORD rgb = *(psrc++);
            BYTE color = 0;
            for (BYTE c = 0; c < 16; c++)
            {
                if (palette[c] == rgb)
                {
                    color = c;
                    break;
                }
            }
            if ((i & 1) == 0)
                *pdst = (color << 4);
            else
            {
                *pdst = (*pdst) & 0xf0 | color;
                pdst++;
            }
        }

        adler = update_adler32(adler, pline, linelen);
    }
    // ADLER32 checksum
    SaveValueMSB(pdst, adler);

    SavePngChunkChecksum(pData);

    // Write IDAT chunk
    WriteFile(hFile, pData, pDataLength, &dwBytesWritten, NULL);
    ::free(pData);
    if (dwBytesWritten != pDataLength)
        return FALSE;

    { // Write IEND chunk
        BYTE IENDchunk[12 + 0];
        *((DWORD*)IENDchunk) = 0;
        memcpy(IENDchunk + 4, "IEND", 4);
        SavePngChunkChecksum(IENDchunk);
        WriteFile(hFile, IENDchunk, sizeof(IENDchunk), &dwBytesWritten, NULL);
        if (dwBytesWritten != sizeof(IENDchunk))
            return FALSE;
    }

    // Close file
    CloseHandle(hFile);

    return TRUE;
}

//////////////////////////////////////////////////////////////////////

   /* Table of CRCs of all 8-bit messages. */
   unsigned long crc_table[256];
   
   /* Flag: has the table been computed? Initially false. */
   int crc_table_computed = 0;
   
   /* Make the table for a fast CRC. */
   void make_crc_table(void)
   {
     unsigned long c;
     int n, k;
   
     for (n = 0; n < 256; n++) {
       c = (unsigned long) n;
       for (k = 0; k < 8; k++) {
         if (c & 1)
           c = 0xedb88320L ^ (c >> 1);
         else
           c = c >> 1;
       }
       crc_table[n] = c;
     }
     crc_table_computed = 1;
   }
   
   /* Update a running CRC with the bytes buf[0..len-1]--the CRC
      should be initialized to all 1's, and the transmitted value
      is the 1's complement of the final running CRC (see the
      crc() routine below)). */
   
   unsigned long update_crc(unsigned long crc, unsigned char *buf,
                            int len)
   {
     unsigned long c = crc;
     int n;
   
     if (!crc_table_computed)
       make_crc_table();
     for (n = 0; n < len; n++) {
       c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
     }
     return c;
   }
   
   /* Return the CRC of the bytes buf[0..len-1]. */
   unsigned long crc(unsigned char *buf, int len)
   {
     return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
   }


//////////////////////////////////////////////////////////////////////
// Source: http://www.ietf.org/rfc/rfc1950.txt section 9. Appendix: Sample code

  #define BASE 65521 /* largest prime smaller than 65536 */

  unsigned long update_adler32(unsigned long adler,
     unsigned char *buf, int len)
  {
    unsigned long s1 = adler & 0xffff;
    unsigned long s2 = (adler >> 16) & 0xffff;
    int n;

    for (n = 0; n < len; n++) {
      s1 = (s1 + buf[n]) % BASE;
      s2 = (s2 + s1)     % BASE;
    }
    return (s2 << 16) + s1;
  }


//////////////////////////////////////////////////////////////////////

