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

// Save screenshot as .BMP file
BOOL BmpFile_SaveScreenshot(
    const DWORD* bits,
    const DWORD* colors,
    LPCTSTR sFileName);

// Save screenshot as .PNG file
BOOL PngFile_SaveScreenshot(
    const DWORD* bits,
    const DWORD* colors,
    LPCTSTR sFileName);


//////////////////////////////////////////////////////////////////////

DECLARE_HANDLE(HAPNGFILE);

// Create new APNG file
HAPNGFILE ApngFile_Create(LPCTSTR filename);
// Close APNG file
void ApngFile_Close(HAPNGFILE apngfile);
// Append one frame to the APNG file
BOOL ApngFile_WriteFrame(
    HAPNGFILE apngfile,
    const DWORD* bits,
    const DWORD* colors);


//////////////////////////////////////////////////////////////////////
