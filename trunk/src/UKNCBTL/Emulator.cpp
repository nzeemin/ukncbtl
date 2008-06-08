// Emulator.cpp

#include "stdafx.h"
#include "UKNCBTL.h"
#include "Emulator.h"
#include "Views.h"
#include "emubase\Emubase.h"


//////////////////////////////////////////////////////////////////////


CMotherboard* g_pBoard = NULL;

BOOL g_okEmulatorRunning = FALSE;

WORD m_wEmulatorCPUBreakpoint = 0177777;
WORD m_wEmulatorPPUBreakpoint = 0177777;

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

BOOL InitEmulator()
{
    ASSERT(g_pBoard == NULL);

    CProcessor::Init();

    g_pBoard = new CMotherboard();

    BYTE buffer[32768];
    DWORD dwBytesRead;

    // Load ROM file
    ZeroMemory(buffer, 32768);
    HANDLE hRomFile = CreateFile(FILE_NAME_UKNC_ROM, GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hRomFile == INVALID_HANDLE_VALUE)
    {
        AlertWarning(_T("Failed to load ROM file."));
        return false;
    }
    ReadFile(hRomFile, buffer, 32256, &dwBytesRead, NULL);
    ASSERT(dwBytesRead == 32256);
    CloseHandle(hRomFile);

    g_pBoard->LoadROM(buffer);

    g_pBoard->Reset();

    m_nUptimeFrameCount = 0;
    m_dwEmulatorUptime = 0;

    // Allocate memory for old RAM values
    for (int i = 0; i < 3; i++)
    {
        g_pEmulatorRam[i] = (BYTE*) ::LocalAlloc(LPTR, 65536);
        g_pEmulatorChangedRam[i] = (BYTE*) ::LocalAlloc(LPTR, 65536);
    }

    return TRUE;
}

void DoneEmulator()
{
    ASSERT(g_pBoard != NULL);

    delete g_pBoard;
    g_pBoard = NULL;

    // Free memory used for old RAM values
    for (int i = 0; i < 3; i++)
    {
        ::LocalFree(g_pEmulatorRam[i]);
        ::LocalFree(g_pEmulatorChangedRam[i]);
    }
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

int Emulator_SystemFrame()
{
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
		TCHAR buffer[16];
		swprintf_s(buffer, 16, _T("FPS: %05.2f"), dFramesPerSecond);
		MainWindow_SetStatusbarText(StatusbarPartFPS, buffer);

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
		swprintf_s(buffer, 20, _T("Uptime: %02d:%02d:%02d"), hours, minutes, seconds);
		MainWindow_SetStatusbarText(StatusbarPartUptime, buffer);
	}

    return 1;
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

void Emulator_LoadROMCartridge(int slot, LPCTSTR sFilePath)
{
    // Open file
    HANDLE hFile = CreateFile(sFilePath,
            GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        AlertWarning(_T("Failed to load ROM cartridge image."));
        return;
    }

    // Allocate memory
    BYTE* pImage = (BYTE*) ::LocalAlloc(LPTR, 24 * 1024);

    DWORD dwBytesRead = 0;
    ReadFile(hFile, pImage, 24 * 1024, &dwBytesRead, NULL);
    ASSERT(dwBytesRead == 24 * 1024);

    g_pBoard->LoadROMCartridge(slot, pImage);

    // Free memory, close file
    ::LocalFree(pImage);
    ::CloseHandle(hFile);
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

void Emulator_SaveImage(LPCTSTR sFilePath)
{
    // Create file
    HANDLE hFile = CreateFile(sFilePath,
            GENERIC_WRITE, FILE_SHARE_READ, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        AlertWarning(_T("Failed to save image file."));
        return;
    }

    // Allocate memory
    BYTE* pImage = (BYTE*) ::LocalAlloc(LPTR, UKNCIMAGE_SIZE);
    ZeroMemory(pImage, UKNCIMAGE_SIZE);
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
    DWORD dwBytesWritten = 0;
    WriteFile(hFile, pImage, UKNCIMAGE_SIZE, &dwBytesWritten, NULL);
    //TODO: Check if dwBytesWritten != UKNCIMAGE_SIZE

    // Free memory, close file
    LocalFree(pImage);
    CloseHandle(hFile);
}

void Emulator_LoadImage(LPCTSTR sFilePath)
{
    // Open file
    HANDLE hFile = CreateFile(sFilePath,
            GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        AlertWarning(_T("Failed to load image file."));
        return;
    }

    // Read header
    DWORD bufHeader[UKNCIMAGE_HEADER_SIZE / sizeof(DWORD)];
    DWORD dwBytesRead = 0;
    ReadFile(hFile, bufHeader, UKNCIMAGE_HEADER_SIZE, &dwBytesRead, NULL);
    //TODO: Check if dwBytesRead != UKNCIMAGE_HEADER_SIZE
    
    //TODO: Check version and size

    // Allocate memory
    BYTE* pImage = (BYTE*) ::LocalAlloc(LPTR, UKNCIMAGE_SIZE);

    // Read image
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    dwBytesRead = 0;
    ReadFile(hFile, pImage, UKNCIMAGE_SIZE, &dwBytesRead, NULL);
    //TODO: Check if dwBytesRead != UKNCIMAGE_SIZE

    // Restore emulator state from the image
    g_pBoard->LoadFromImage(pImage);

    m_dwEmulatorUptime = *(DWORD*)(pImage + 16);

    // Free memory, close file
    LocalFree(pImage);
    CloseHandle(hFile);

    g_okEmulatorRunning = FALSE;

    MainWindow_UpdateAllViews();
}


//////////////////////////////////////////////////////////////////////
