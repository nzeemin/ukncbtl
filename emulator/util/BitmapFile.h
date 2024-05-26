/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// BitmapFile.h

#pragma once

//////////////////////////////////////////////////////////////////////


void BitmapFile_Init();
void BitmapFile_Done();

enum BitmapFileFormat
{
    BitmapFileFormatBmp = 1,
    BitmapFileFormatPng = 2,
    BitmapFileFormatTiff = 3,
};

HBITMAP BitmapFile_LoadPngFromResource(LPCTSTR lpName);

// Save the image as .PNG file
bool BitmapFile_SaveImageFile(
    const uint32_t* pBits,
    LPCTSTR sFileName, BitmapFileFormat format,
    int screenWidth, int screenHeight);


//////////////////////////////////////////////////////////////////////
