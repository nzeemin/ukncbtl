/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Emulator.cpp

#include "stdafx.h"
#include <stdio.h>
#include <Share.h>
#include "UKNCBTL.h"
#include "Emulator.h"
#include "Views.h"
#include "emubase\Emubase.h"
#include "SoundGen.h"

//NOTE: I know, we use unsafe string functions
#pragma warning( disable: 4996 )


//////////////////////////////////////////////////////////////////////


CMotherboard* g_pBoard = NULL;

BOOL g_okEmulatorInitialized = FALSE;
BOOL g_okEmulatorRunning = FALSE;

WORD m_wEmulatorCPUBreakpoint = 0177777;
WORD m_wEmulatorPPUBreakpoint = 0177777;

BOOL m_okEmulatorSound = FALSE;

WORD m_Settings_NetStation_Bits = 0;
BOOL m_okEmulatorNetwork = FALSE;
HANDLE m_hEmulatorNetPort = INVALID_HANDLE_VALUE;

BOOL m_okEmulatorSerial = FALSE;
HANDLE m_hEmulatorComPort = INVALID_HANDLE_VALUE;

BOOL m_okEmulatorParallel = FALSE;
FILE* m_fpEmulatorParallelOut = NULL;

long m_nFrameCount = 0;
DWORD m_dwTickCount = 0;
DWORD m_dwEmulatorUptime = 0;  // UKNC uptime, seconds, from turn on or reset, increments every 25 frames
long m_nUptimeFrameCount = 0;

BYTE* g_pEmulatorRam[3];  // RAM values - for change tracking
BYTE* g_pEmulatorChangedRam[3];  // RAM change flags
WORD g_wEmulatorCpuPC = 0177777;      // Current PC value
WORD g_wEmulatorPrevCpuPC = 0177777;  // Previous PC value
WORD g_wEmulatorPpuPC = 0177777;      // Current PC value
WORD g_wEmulatorPrevPpuPC = 0177777;  // Previous PC value


//////////////////////////////////////////////////////////////////////


const LPCTSTR FILE_NAME_UKNC_ROM = _T("uknc_rom.bin");

BOOL Emulator_LoadUkncRom()
{
    HRSRC hRes = ::FindResource(NULL, MAKEINTRESOURCE(IDR_UKNC_ROM), _T("BIN"));
    if (hRes == NULL)
        return false;
    DWORD dwDataSize = ::SizeofResource(NULL, hRes);
    if (dwDataSize < 32256)
        return false;
    HGLOBAL hResLoaded = ::LoadResource(NULL, hRes);
    if (hResLoaded == NULL)
        return false;
    void * pData = ::malloc(32768);
    if (pData == NULL)
        return false;
    ::memset(pData, 0, 32768);
    void * pResData = ::LockResource(hResLoaded);
    if (pResData == NULL)
    {
        ::free(pData);
        return false;
    }
    ::memcpy(pData, pResData, 32256);

    g_pBoard->LoadROM((const BYTE *)pData);

    ::free(pData);

    return true;
}

BOOL Emulator_Init()
{
    ASSERT(g_pBoard == NULL);

    ::memset(g_pEmulatorRam, 0, sizeof(g_pEmulatorRam));
    ::memset(g_pEmulatorChangedRam, 0, sizeof(g_pEmulatorChangedRam));
    CProcessor::Init();

    g_pBoard = new CMotherboard();

    if (! Emulator_LoadUkncRom())
    {
        AlertWarning(_T("Failed to load UKNC ROM from resources."));
        return false;
    }

    g_pBoard->SetNetStation(Settings_GetNetStation());

    g_pBoard->Reset();

    if (m_okEmulatorSound)
    {
        SoundGen_Initialize(Settings_GetSoundVolume());
        g_pBoard->SetSoundGenCallback(SoundGen_FeedDAC);
    }

    m_nUptimeFrameCount = 0;
    m_dwEmulatorUptime = 0;

    // Allocate memory for old RAM values
    for (int i = 0; i < 3; i++)
    {
        g_pEmulatorRam[i] = (BYTE*) ::malloc(65536);
        if (g_pEmulatorRam[i] != NULL) memset(g_pEmulatorRam[i], 0, 65536);
        g_pEmulatorChangedRam[i] = (BYTE*) ::malloc(65536);
        if (g_pEmulatorChangedRam[i] != NULL) memset(g_pEmulatorChangedRam[i], 0, 65536);
    }

    g_okEmulatorInitialized = TRUE;
    return true;
}

void Emulator_Done()
{
    ASSERT(g_pBoard != NULL);

    CProcessor::Done();

    g_pBoard->SetSoundGenCallback(NULL);
    SoundGen_Finalize();

    g_pBoard->SetSerialCallbacks(NULL, NULL);
    if (m_hEmulatorComPort != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_hEmulatorComPort);
        m_hEmulatorComPort = INVALID_HANDLE_VALUE;
    }

    g_pBoard->SetNetworkCallbacks(NULL, NULL);
    if (m_hEmulatorNetPort != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_hEmulatorNetPort);
        m_hEmulatorNetPort = INVALID_HANDLE_VALUE;
    }

    delete g_pBoard;
    g_pBoard = NULL;

    // Free memory used for old RAM values
    for (int i = 0; i < 3; i++)
    {
        ::free(g_pEmulatorRam[i]);
        ::free(g_pEmulatorChangedRam[i]);
    }

    g_okEmulatorInitialized = FALSE;
}

void Emulator_Start()
{
    g_okEmulatorRunning = TRUE;

    // Set title bar text
    SetWindowText(g_hwnd, _T("UKNC Back to Life [run]"));
    MainWindow_UpdateMenu();

    m_nFrameCount = 0;
    m_dwTickCount = GetTickCount();
}
void Emulator_Stop()
{
    g_okEmulatorRunning = FALSE;
    m_wEmulatorCPUBreakpoint = 0177777;
    m_wEmulatorPPUBreakpoint = 0177777;

    if (m_fpEmulatorParallelOut != NULL)
        ::fflush(m_fpEmulatorParallelOut);

    // Reset title bar message
    SetWindowText(g_hwnd, _T("UKNC Back to Life [stop]"));
    MainWindow_UpdateMenu();
    // Reset FPS indicator
    MainWindow_SetStatusbarText(StatusbarPartFPS, _T(""));

    MainWindow_UpdateAllViews();
}

void Emulator_Reset()
{
    ASSERT(g_pBoard != NULL);

    g_pBoard->Reset();

    m_nUptimeFrameCount = 0;
    m_dwEmulatorUptime = 0;

    MainWindow_UpdateAllViews();
}

void Emulator_SetCPUBreakpoint(WORD address)
{
    m_wEmulatorCPUBreakpoint = address;
}
void Emulator_SetPPUBreakpoint(WORD address)
{
    m_wEmulatorPPUBreakpoint = address;
}
BOOL Emulator_IsBreakpoint()
{
    WORD wCPUAddr = g_pBoard->GetCPU()->GetPC();
    if (wCPUAddr == m_wEmulatorCPUBreakpoint)
        return TRUE;
    WORD wPPUAddr = g_pBoard->GetPPU()->GetPC();
    if (wPPUAddr == m_wEmulatorPPUBreakpoint)
        return TRUE;
    return FALSE;
}

void Emulator_SetSound(BOOL soundOnOff)
{
    if (m_okEmulatorSound != soundOnOff)
    {
        if (soundOnOff)
        {
            SoundGen_Initialize(Settings_GetSoundVolume());
            g_pBoard->SetSoundGenCallback(SoundGen_FeedDAC);
        }
        else
        {
            g_pBoard->SetSoundGenCallback(NULL);
            SoundGen_Finalize();
        }
    }

    m_okEmulatorSound = soundOnOff;
}

BOOL CALLBACK Emulator_NetworkIn_Callback(BYTE* pByte)
{
    DWORD dwBytesRead;
    BOOL result = ::ReadFile(m_hEmulatorNetPort, pByte, 1, &dwBytesRead, NULL);

#if !defined(PRODUCT)
    if (result && (dwBytesRead == 1))
        DebugLogFormat(_T("Net IN %02x\r\n"), (int)(*pByte));//DEBUG
#endif

    return result && (dwBytesRead == 1);
}

BOOL CALLBACK Emulator_NetworkOut_Callback(BYTE byte)
{
    DWORD dwBytesWritten;
    ::WriteFile(m_hEmulatorNetPort, &byte, 1, &dwBytesWritten, NULL);
#if !defined(PRODUCT)
    DebugLogFormat(_T("Net OUT %02x\r\n"), byte);//DEBUG
#endif

    return (dwBytesWritten == 1);
}

BOOL Emulator_SetNetwork(BOOL networkOnOff, LPCTSTR networkPort)
{
    if (m_okEmulatorNetwork != networkOnOff)
    {
        m_Settings_NetStation_Bits = (WORD)Settings_GetNetStation();
        WORD rotateBits = (m_Settings_NetStation_Bits / 16);
        m_Settings_NetStation_Bits = (256 * (16 * rotateBits + m_Settings_NetStation_Bits));
        g_pBoard->SetNetStation(m_Settings_NetStation_Bits);

        if (networkOnOff)
        {
            // Prepare port name
            TCHAR port[15];
            wsprintf(port, _T("\\\\.\\%s"), networkPort);

            // Open port
            m_hEmulatorNetPort = ::CreateFile(port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (m_hEmulatorNetPort == INVALID_HANDLE_VALUE)
            {
                DWORD dwError = ::GetLastError();
                AlertWarningFormat(_T("Failed to open network COM port (0x%08lx)."), dwError);
                return FALSE;
            }

            // Set port settings
            DCB dcb;
            Settings_GetNetComConfig(&dcb);
            if (!::SetCommState(m_hEmulatorNetPort, &dcb))
            {
                DWORD dwError = ::GetLastError();
                ::CloseHandle(m_hEmulatorNetPort);
                m_hEmulatorNetPort = INVALID_HANDLE_VALUE;
                AlertWarningFormat(_T("Failed to configure the Network COM port (0x%08lx)."), dwError);
                return FALSE;
            }

            // Set timeouts: ReadIntervalTimeout value of MAXDWORD, combined with zero values for both the ReadTotalTimeoutConstant
            // and ReadTotalTimeoutMultiplier members, specifies that the read operation is to return immediately with the bytes
            // that have already been received, even if no bytes have been received.
            COMMTIMEOUTS timeouts;
            ::memset(&timeouts, 0, sizeof(timeouts));
            timeouts.ReadIntervalTimeout = MAXDWORD;
            timeouts.WriteTotalTimeoutConstant = 100;
            if (!::SetCommTimeouts(m_hEmulatorNetPort, &timeouts))
            {
                DWORD dwError = ::GetLastError();
                ::CloseHandle(m_hEmulatorNetPort);
                m_hEmulatorNetPort = INVALID_HANDLE_VALUE;
                AlertWarningFormat(_T("Failed to set the Network COM port timeouts (0x%08lx)."), dwError);
                return FALSE;
            }

            // Clear port input buffer
            ::PurgeComm(m_hEmulatorNetPort, PURGE_RXABORT | PURGE_RXCLEAR);

            // Set callbacks
            g_pBoard->SetNetworkCallbacks(Emulator_NetworkIn_Callback, Emulator_NetworkOut_Callback);
        }
        else
        {
            g_pBoard->SetNetworkCallbacks(NULL, NULL);  // Reset callbacks

            // Close port
            if (m_hEmulatorNetPort != INVALID_HANDLE_VALUE)
            {
                ::CloseHandle(m_hEmulatorNetPort);
                m_hEmulatorNetPort = INVALID_HANDLE_VALUE;
            }
        }
    }

    m_okEmulatorNetwork = networkOnOff;

    return TRUE;
}

BOOL CALLBACK Emulator_SerialIn_Callback(BYTE* pByte)
{
    DWORD dwBytesRead;
    BOOL result = ::ReadFile(m_hEmulatorComPort, pByte, 1, &dwBytesRead, NULL);

    return result && (dwBytesRead == 1);
}

BOOL CALLBACK Emulator_SerialOut_Callback(BYTE byte)
{
    DWORD dwBytesWritten;
    ::WriteFile(m_hEmulatorComPort, &byte, 1, &dwBytesWritten, NULL);

    return (dwBytesWritten == 1);
}

BOOL Emulator_SetSerial(BOOL serialOnOff, LPCTSTR serialPort)
{
    if (m_okEmulatorSerial != serialOnOff)
    {
        if (serialOnOff)
        {
            // Prepare port name
            TCHAR port[15];
            wsprintf(port, _T("\\\\.\\%s"), serialPort);

            // Open port
            m_hEmulatorComPort = ::CreateFile(port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (m_hEmulatorComPort == INVALID_HANDLE_VALUE)
            {
                DWORD dwError = ::GetLastError();
                AlertWarningFormat(_T("Failed to open COM port (0x%08lx)."), dwError);
                return FALSE;
            }

            // Set port settings
            DCB dcb;
            Settings_GetSerialConfig(&dcb);
            if (!::SetCommState(m_hEmulatorComPort, &dcb))
            {
                DWORD dwError = ::GetLastError();
                ::CloseHandle(m_hEmulatorComPort);
                m_hEmulatorComPort = INVALID_HANDLE_VALUE;
                AlertWarningFormat(_T("Failed to configure the COM port (0x%08lx)."), dwError);
                return FALSE;
            }

            // Set timeouts: ReadIntervalTimeout value of MAXDWORD, combined with zero values for both the ReadTotalTimeoutConstant
            // and ReadTotalTimeoutMultiplier members, specifies that the read operation is to return immediately with the bytes
            // that have already been received, even if no bytes have been received.
            COMMTIMEOUTS timeouts;
            ::memset(&timeouts, 0, sizeof(timeouts));
            timeouts.ReadIntervalTimeout = MAXDWORD;
            timeouts.WriteTotalTimeoutConstant = 100;
            if (!::SetCommTimeouts(m_hEmulatorComPort, &timeouts))
            {
                DWORD dwError = ::GetLastError();
                ::CloseHandle(m_hEmulatorComPort);
                m_hEmulatorComPort = INVALID_HANDLE_VALUE;
                AlertWarningFormat(_T("Failed to set the COM port timeouts (0x%08lx)."), dwError);
                return FALSE;
            }

            // Clear port input buffer
            ::PurgeComm(m_hEmulatorComPort, PURGE_RXABORT | PURGE_RXCLEAR);

            // Set callbacks
            g_pBoard->SetSerialCallbacks(Emulator_SerialIn_Callback, Emulator_SerialOut_Callback);
        }
        else
        {
            g_pBoard->SetSerialCallbacks(NULL, NULL);  // Reset callbacks

            // Close port
            if (m_hEmulatorComPort != INVALID_HANDLE_VALUE)
            {
                ::CloseHandle(m_hEmulatorComPort);
                m_hEmulatorComPort = INVALID_HANDLE_VALUE;
            }
        }
    }

    m_okEmulatorSerial = serialOnOff;

    return TRUE;
}

BOOL CALLBACK Emulator_ParallelOut_Callback(BYTE byte)
{
    if (m_fpEmulatorParallelOut != NULL)
    {
        ::fwrite(&byte, 1, 1, m_fpEmulatorParallelOut);
    }

    return TRUE;
}

void Emulator_SetParallel(BOOL parallelOnOff)
{
    if (m_okEmulatorParallel == parallelOnOff)
        return;

    if (!parallelOnOff)
    {
        g_pBoard->SetParallelOutCallback(NULL);
        if (m_fpEmulatorParallelOut != NULL)
            ::fclose(m_fpEmulatorParallelOut);
    }
    else
    {
        g_pBoard->SetParallelOutCallback(Emulator_ParallelOut_Callback);
        m_fpEmulatorParallelOut = ::_tfopen(_T("printer.log"), _T("wb"));
    }

    m_okEmulatorParallel = parallelOnOff;
}

int Emulator_SystemFrame()
{
    SoundGen_SetVolume(Settings_GetSoundVolume());

    g_pBoard->SetCPUBreakpoint(m_wEmulatorCPUBreakpoint);
    g_pBoard->SetPPUBreakpoint(m_wEmulatorPPUBreakpoint);

    ScreenView_ScanKeyboard();

    if (!g_pBoard->SystemFrame())
        return 0;

    // Calculate frames per second
    m_nFrameCount++;
    DWORD dwCurrentTicks = GetTickCount();
    long nTicksElapsed = dwCurrentTicks - m_dwTickCount;
    if (nTicksElapsed >= 1200)
    {
        double dFramesPerSecond = m_nFrameCount * 1000.0 / nTicksElapsed;
        double dSpeed = dFramesPerSecond / 25.0 * 100;
        TCHAR buffer[16];
        _stprintf(buffer, _T("%03.f%%"), dSpeed);
        MainWindow_SetStatusbarText(StatusbarPartFPS, buffer);

        BOOL floppyEngine = g_pBoard->IsFloppyEngineOn();
        MainWindow_SetStatusbarText(StatusbarPartFloppyEngine, floppyEngine ? _T("Motor") : NULL);

        m_nFrameCount = 0;
        m_dwTickCount = dwCurrentTicks;
    }

    // Calculate emulator uptime (25 frames per second)
    m_nUptimeFrameCount++;
    if (m_nUptimeFrameCount >= 25)
    {
        m_dwEmulatorUptime++;
        m_nUptimeFrameCount = 0;

        int seconds = (int) (m_dwEmulatorUptime % 60);
        int minutes = (int) (m_dwEmulatorUptime / 60 % 60);
        int hours   = (int) (m_dwEmulatorUptime / 3600 % 60);

        TCHAR buffer[20];
        _stprintf(buffer, _T("Uptime: %02d:%02d:%02d"), hours, minutes, seconds);
        MainWindow_SetStatusbarText(StatusbarPartUptime, buffer);
    }

    return 1;
}

DWORD Emulator_GetUptime()
{
    return m_dwEmulatorUptime;
}

// Update cached values after Run or Step
void Emulator_OnUpdate()
{
    // Update stored PC value
    g_wEmulatorPrevCpuPC = g_wEmulatorCpuPC;
    g_wEmulatorCpuPC = g_pBoard->GetCPU()->GetPC();
    g_wEmulatorPrevPpuPC = g_wEmulatorPpuPC;
    g_wEmulatorPpuPC = g_pBoard->GetPPU()->GetPC();

    // Update memory change flags
    for (int plane = 0; plane < 3; plane++)
    {
        BYTE* pOld = g_pEmulatorRam[plane];
        BYTE* pChanged = g_pEmulatorChangedRam[plane];
        WORD addr = 0;
        do
        {
            BYTE newvalue = g_pBoard->GetRAMByte(plane, addr);
            BYTE oldvalue = *pOld;
            *pChanged = (newvalue != oldvalue) ? 255 : 0;
            *pOld = newvalue;
            addr++;
            pOld++;  pChanged++;
        }
        while (addr < 65535);
    }
}

// Get RAM change flag
//   addrtype - address mode - see ADDRTYPE_XXX constants
WORD Emulator_GetChangeRamStatus(int addrtype, WORD address)
{
    switch (addrtype)
    {
    case ADDRTYPE_RAM0:
    case ADDRTYPE_RAM1:
    case ADDRTYPE_RAM2:
        return *((WORD*)(g_pEmulatorChangedRam[addrtype] + address));
    case ADDRTYPE_RAM12:
        if (address < 0170000)
            return MAKEWORD(
                    *(g_pEmulatorChangedRam[1] + address / 2),
                    *(g_pEmulatorChangedRam[2] + address / 2));
        else
            return 0;
    default:
        return 0;
    }
}

BOOL Emulator_LoadROMCartridge(int slot, LPCTSTR sFilePath)
{
    // Open file
    FILE* fpFile = ::_tfsopen(sFilePath, _T("rb"), _SH_DENYWR);
    if (fpFile == NULL)
    {
        AlertWarning(_T("Failed to load ROM cartridge image."));
        return FALSE;
    }

    // Allocate memory
    BYTE* pImage = (BYTE*) ::malloc(24 * 1024);
    if (pImage == NULL)
    {
        ::fclose(fpFile);
        return FALSE;
    }
    DWORD dwBytesRead = ::fread(pImage, 1, 24 * 1024, fpFile);
    ASSERT(dwBytesRead == 24 * 1024);

    g_pBoard->LoadROMCartridge(slot, pImage);

    // Free memory, close file
    ::free(pImage);
    ::fclose(fpFile);

    //TODO: Save the file name for a future SaveImage() call

    return TRUE;
}

void Emulator_PrepareScreenRGB32(void* pImageBits, const DWORD* colors)
{
    if (pImageBits == NULL) return;
    if (!g_okEmulatorInitialized) return;

    // Tag parsing loop
    BYTE cursorYRGB;
    BOOL okCursorType;
    BYTE cursorPos = 128;
    BOOL cursorOn = FALSE;
    BYTE cursorAddress;      // Address of graphical cursor
    WORD address = 0000270;  // Tag sequence start address
    BOOL okTagSize = FALSE;  // Tag size: TRUE - 4-word, FALSE - 2-word (first tag is always 2-word)
    BOOL okTagType = FALSE;  // Type of 4-word tag: TRUE - set palette, FALSE - set params
    int scale = 1;           // Horizontal scale: 1, 2, 4, or 8
    DWORD palette = 0;       // Palette
    DWORD palettecurrent[8];  memset(palettecurrent, 0, sizeof(palettecurrent)); // Current palette; update each time we change the "palette" variable
    BYTE pbpgpr = 0;         // 3-bit Y-value modifier
    for (int yy = 0; yy < 307; yy++)
    {
        if (okTagSize)  // 4-word tag
        {
            WORD tag1 = g_pBoard->GetRAMWord(0, address);
            address += 2;
            WORD tag2 = g_pBoard->GetRAMWord(0, address);
            address += 2;

            if (okTagType)  // 4-word palette tag
            {
                palette = MAKELONG(tag1, tag2);
            }
            else  // 4-word params tag
            {
                scale = (tag2 >> 4) & 3;  // Bits 4-5 - new scale value
                pbpgpr = (BYTE)((7 - (tag2 & 7)) << 4);  // Y-value modifier
                cursorYRGB = (BYTE)(tag1 & 15);  // Cursor color
                okCursorType = ((tag1 & 16) != 0);  // TRUE - graphical cursor, FALSE - symbolic cursor
                ASSERT(okCursorType == 0);  //DEBUG
                cursorPos = (BYTE)(((tag1 >> 8) >> scale) & 0x7f);  // Cursor position in the line
                cursorAddress = (BYTE)((tag1 >> 5) & 7);
                scale = 1 << scale;
            }
            for (BYTE c = 0; c < 8; c++)  // Update palettecurrent
            {
                BYTE valueYRGB = (BYTE) (palette >> (c << 2)) & 15;
                palettecurrent[c] = colors[pbpgpr | valueYRGB];
                //if (pbpgpr != 0) DebugLogFormat(_T("pbpgpr %02x\r\n"), pbpgpr | valueYRGB);
            }
        }

        WORD addressBits = g_pBoard->GetRAMWord(0, address);  // The word before the last word - is address of bits from all three memory planes
        address += 2;

        // Calculate size, type and address of the next tag
        WORD tagB = g_pBoard->GetRAMWord(0, address);  // Last word of the tag - is address and type of the next tag
        okTagSize = (tagB & 2) != 0;  // Bit 1 shows size of the next tag
        if (okTagSize)
        {
            address = tagB & ~7;
            okTagType = (tagB & 4) != 0;  // Bit 2 shows type of the next tag
        }
        else
            address = tagB & ~3;
        if ((tagB & 1) != 0)
            cursorOn = !cursorOn;

        // Draw bits into the bitmap, from line 20 to line 307
        if (yy < 19 /*|| yy > 306*/)
            continue;

        // Loop thru bits from addressBits, planes 0,1,2
        // For each pixel:
        //   Get bit from planes 0,1,2 and make value
        //   Map value to palette; result is 4-bit value YRGB
        //   Translate value to 24-bit RGB
        //   Put value to m_bits; repeat using scale value

        int xr = 640;
        int y = yy - 19;
        DWORD* pBits = ((DWORD*)pImageBits) + y * 640;
        int pos = 0;
        for (;;)
        {
            // Get bit from planes 0,1,2
            BYTE src0 = g_pBoard->GetRAMByte(0, addressBits);
            BYTE src1 = g_pBoard->GetRAMByte(1, addressBits);
            BYTE src2 = g_pBoard->GetRAMByte(2, addressBits);
            // Loop through the bits of the byte
            int bit = 0;
            for (;;)
            {
                DWORD valueRGB;
                if (cursorOn && (pos == cursorPos) && (!okCursorType || (okCursorType && bit == cursorAddress)))
                    valueRGB = colors[cursorYRGB];  // 4-bit to 32-bit color
                else
                {
                    // Make 3-bit value from the bits
                    BYTE value012 = (src0 & 1) | ((src1 & 1) << 1) | ((src2 & 1) << 2);
                    valueRGB = palettecurrent[value012];  // 3-bit to 32-bit color
                }

                // Put value to m_bits; repeat using scale value
                //WAS: for (int s = 0; s < scale; s++) *pBits++ = valueRGB;
                switch (scale)
                {
                case 8:
                    *pBits++ = valueRGB;
                    *pBits++ = valueRGB;
                    *pBits++ = valueRGB;
                    *pBits++ = valueRGB;
                case 4:
                    *pBits++ = valueRGB;
                    *pBits++ = valueRGB;
                case 2:
                    *pBits++ = valueRGB;
                case 1:
                    *pBits++ = valueRGB;
                default:
                    break;
                }

                xr -= scale;

                if (bit == 7)
                    break;
                bit++;

                // Shift to the next bit
                src0 >>= 1;
                src1 >>= 1;
                src2 >>= 1;
            }
            if (xr <= 0)
                break;  // End of line
            addressBits++;  // Go to the next byte
            pos++;
        }
    }
}


//////////////////////////////////////////////////////////////////////
//
// Emulator image format - see CMotherboard::SaveToImage()
// Image header format (32 bytes):
//   4 bytes        UKNC_IMAGE_HEADER1
//   4 bytes        UKNC_IMAGE_HEADER2
//   4 bytes        UKNC_IMAGE_VERSION
//   4 bytes        UKNC_IMAGE_SIZE
//   4 bytes        UKNC uptime
//   12 bytes       Not used
//TODO: 256 bytes * 2 - Cartridge 1..2 path
//TODO: 256 bytes * 4 - Floppy 1..4 path
//TODO: 256 bytes * 2 - Hard 1..2 path

BOOL Emulator_SaveImage(LPCTSTR sFilePath)
{
    // Create file
    FILE* fpFile = ::_tfsopen(sFilePath, _T("w+b"), _SH_DENYWR);
    if (fpFile == NULL)
        return FALSE;

    // Allocate memory
    BYTE* pImage = (BYTE*) ::malloc(UKNCIMAGE_SIZE);
    if (pImage == NULL)
    {
        ::fclose(fpFile);
        return FALSE;
    }
    memset(pImage, 0, UKNCIMAGE_SIZE);
    // Prepare header
    DWORD* pHeader = (DWORD*) pImage;
    *pHeader++ = UKNCIMAGE_HEADER1;
    *pHeader++ = UKNCIMAGE_HEADER2;
    *pHeader++ = UKNCIMAGE_VERSION;
    *pHeader++ = UKNCIMAGE_SIZE;
    // Store emulator state to the image
    g_pBoard->SaveToImage(pImage);
    *(DWORD*)(pImage + 16) = m_dwEmulatorUptime;

    // Save image to the file
    DWORD dwBytesWritten = ::fwrite(pImage, 1, UKNCIMAGE_SIZE, fpFile);
    ::free(pImage);
    ::fclose(fpFile);
    if (dwBytesWritten != UKNCIMAGE_SIZE)
        return FALSE;

    return TRUE;
}

BOOL Emulator_LoadImage(LPCTSTR sFilePath)
{
    Emulator_Stop();

    // Open file
    FILE* fpFile = ::_tfsopen(sFilePath, _T("rb"), _SH_DENYWR);
    if (fpFile == NULL)
        return FALSE;

    // Read header
    DWORD bufHeader[UKNCIMAGE_HEADER_SIZE / sizeof(DWORD)];
    DWORD dwBytesRead = ::fread(bufHeader, 1, UKNCIMAGE_HEADER_SIZE, fpFile);
    if (dwBytesRead != UKNCIMAGE_HEADER_SIZE)
    {
        ::fclose(fpFile);
        return FALSE;
    }

    //TODO: Check version and size

    // Allocate memory
    BYTE* pImage = (BYTE*) ::malloc(UKNCIMAGE_SIZE);
    if (pImage == NULL)
    {
        ::fclose(fpFile);
        return FALSE;
    }

    // Read image
    ::fseek(fpFile, 0, SEEK_SET);
    dwBytesRead = ::fread(pImage, 1, UKNCIMAGE_SIZE, fpFile);
    if (dwBytesRead != UKNCIMAGE_SIZE)
    {
        ::free(pImage);
        ::fclose(fpFile);
        return FALSE;
    }

    // Restore emulator state from the image
    g_pBoard->LoadFromImage(pImage);

    m_dwEmulatorUptime = *(DWORD*)(pImage + 16);

    // Free memory, close file
    ::free(pImage);
    ::fclose(fpFile);

    return TRUE;
}


//////////////////////////////////////////////////////////////////////
