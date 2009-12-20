// TapeView.cpp

#include "stdafx.h"
#include <commdlg.h>
#include "UKNCBTL.h"
#include "Views.h"
#include "ToolWindow.h"
#include "util\\WavPcmFile.h"
#include "Emulator.h"


//////////////////////////////////////////////////////////////////////


HWND g_hwndTape = (HWND) INVALID_HANDLE_VALUE;  // Tape View window handle
WNDPROC m_wndprocTapeToolWindow = NULL;  // Old window proc address of the ToolWindow

HWND m_hwndTapeFile = (HWND) INVALID_HANDLE_VALUE;  // Tape file name - read-only edit control
HWND m_hwndTapeTotal = (HWND) INVALID_HANDLE_VALUE;  // Tape total time - static control
HWND m_hwndTapeCurrent = (HWND) INVALID_HANDLE_VALUE;  // Tape current time - static control
HWND m_hwndTapePlay = (HWND) INVALID_HANDLE_VALUE;
HWND m_hwndTapeRewind = (HWND) INVALID_HANDLE_VALUE;
HWND m_hwndTapeOpen = (HWND) INVALID_HANDLE_VALUE;
HWND m_hwndTapeSave = (HWND) INVALID_HANDLE_VALUE;

HFONT m_hfontTape = NULL;
BOOL m_okTapeInserted = FALSE;
BOOL m_okTapeRecording = FALSE;
TCHAR m_szTapeFile[MAX_PATH];
HWAVPCMFILE m_hTapeWavPcmFile = (HWAVPCMFILE) INVALID_HANDLE_VALUE;
BOOL m_okTapePlaying = FALSE;
DWORD m_dwTapePositionShown = 0;  // What we show (in seconds) in the m_hwndTapeCurrent control

void TapeView_CreateTape(LPCTSTR lpszFile);
void TapeView_OpenTape(LPCTSTR lpszFile);
void TapeView_CloseTape();
void TapeView_PlayTape();
void TapeView_StopTape();
void TapeView_UpdatePosition();

void TapeView_OnDraw(HDC hdc);
void TapeView_DoOpenWav();
void TapeView_DoSaveWav();
void TapeView_DoPlayStop();
void TapeView_DoRewind();

BOOL CALLBACK TapeView_TapeReadCallback(UINT samples);
void CALLBACK TapeView_TapeWriteCallback(int value, UINT samples);


//////////////////////////////////////////////////////////////////////


void TapeView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= TapeViewWndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= g_hInst;
    wcex.hIcon			= NULL;
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE+1);
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= CLASSNAME_TAPEVIEW;
    wcex.hIconSm		= NULL;

    RegisterClassEx(&wcex);
}

void TapeView_Init()
{
}

void TapeView_Done()
{
}

void CreateTapeView(HWND hwndParent, int x, int y, int width, int height)
{
    ASSERT(hwndParent != NULL);

    g_hwndTape = CreateWindow(
        CLASSNAME_TOOLWINDOW, NULL,
        WS_CHILD | WS_VISIBLE,
        x, y, width, height,
        hwndParent, NULL, g_hInst, NULL);
	SetWindowText(g_hwndTape, _T("Tape"));

    // ToolWindow subclassing
    m_wndprocTapeToolWindow = (WNDPROC) LongToPtr( SetWindowLongPtr(
            g_hwndTape, GWLP_WNDPROC, PtrToLong(TapeViewWndProc)) );

    RECT rcClient;  GetClientRect(g_hwndTape, &rcClient);

	m_hwndTapeFile = CreateWindow(
            _T("STATIC"), NULL,
            WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS,
            8, 4, 500, 18,
            g_hwndTape, NULL, g_hInst, NULL);
	m_hwndTapeCurrent = CreateWindow(
            _T("STATIC"), NULL,
            WS_CHILD | WS_VISIBLE,
            8, 26, 100, 18,
            g_hwndTape, NULL, g_hInst, NULL);
	m_hwndTapeTotal = CreateWindow(
            _T("STATIC"), NULL,
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            500 + 8 + 4, 4, rcClient.right - 8*2 - 500 - 4, 18,
            g_hwndTape, NULL, g_hInst, NULL);
	m_hwndTapePlay = CreateWindow(
            _T("BUTTON"), _T("Play"),
            WS_CHILD | WS_VISIBLE | WS_DISABLED,
            8 + 100 + 16, 24, 96, 22,
            g_hwndTape, NULL, g_hInst, NULL);
	m_hwndTapeRewind = CreateWindow(
            _T("BUTTON"), _T("<< Rewind"),
            WS_CHILD | WS_VISIBLE | WS_DISABLED,
            8 + 100 + 16 + 4 + 96, 24, 96, 22,
            g_hwndTape, NULL, g_hInst, NULL);
	m_hwndTapeOpen = CreateWindow(
            _T("BUTTON"), _T("Open WAV"),
            WS_CHILD | WS_VISIBLE,
            rcClient.right - 96 - 4 - 96 - 8, 24, 96, 22,
            g_hwndTape, NULL, g_hInst, NULL);
	m_hwndTapeSave = CreateWindow(
            _T("BUTTON"), _T("Save WAV"),
            WS_CHILD | WS_VISIBLE,
            rcClient.right - 96 - 8, 24, 96, 22,
            g_hwndTape, NULL, g_hInst, NULL);

	m_hfontTape = CreateDialogFont();
	SendMessage(m_hwndTapeCurrent, WM_SETFONT, (WPARAM) m_hfontTape, 0);
	SendMessage(m_hwndTapeTotal, WM_SETFONT, (WPARAM) m_hfontTape, 0);
	SendMessage(m_hwndTapeFile, WM_SETFONT, (WPARAM) m_hfontTape, 0);
	SendMessage(m_hwndTapePlay, WM_SETFONT, (WPARAM) m_hfontTape, 0);
	SendMessage(m_hwndTapeRewind, WM_SETFONT, (WPARAM) m_hfontTape, 0);
	SendMessage(m_hwndTapeOpen, WM_SETFONT, (WPARAM) m_hfontTape, 0);
	SendMessage(m_hwndTapeSave, WM_SETFONT, (WPARAM) m_hfontTape, 0);
}

LRESULT CALLBACK TapeViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
	case WM_COMMAND:
		{
			HWND hwndCtl = (HWND)lParam;
			if (hwndCtl == m_hwndTapeOpen)
				TapeView_DoOpenWav();
			else if (hwndCtl == m_hwndTapeSave)
				TapeView_DoSaveWav();
			else if (hwndCtl == m_hwndTapePlay)
				TapeView_DoPlayStop();
			else if (hwndCtl == m_hwndTapeRewind)
				TapeView_DoRewind();
			else
	            return CallWindowProc(m_wndprocTapeToolWindow, hWnd, message, wParam, lParam);
		}
		break;
    case WM_DESTROY:
        g_hwndTape = (HWND) INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
        return CallWindowProc(m_wndprocTapeToolWindow, hWnd, message, wParam, lParam);
    default:
        return CallWindowProc(m_wndprocTapeToolWindow, hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

void TapeView_CreateTape(LPCTSTR lpszFile)
{
	m_hTapeWavPcmFile = WavPcmFile_Create(lpszFile, 44100);
	if (m_hTapeWavPcmFile == INVALID_HANDLE_VALUE)
		return;  //TODO: Report error

	wcscpy_s(m_szTapeFile, MAX_PATH, lpszFile);
	m_okTapeInserted = TRUE;
	m_okTapeRecording = TRUE;

	EnableWindow(m_hwndTapePlay, TRUE);
	SetWindowText(m_hwndTapePlay, _T("Record"));
	EnableWindow(m_hwndTapeRewind, TRUE);
	SetWindowText(m_hwndTapeFile, lpszFile);

	TapeView_UpdatePosition();

	SetWindowText(m_hwndTapeSave, _T("Close WAV"));
	EnableWindow(m_hwndTapeOpen, FALSE);
}
void TapeView_OpenTape(LPCTSTR lpszFile)
{
	m_hTapeWavPcmFile = WavPcmFile_Open(lpszFile);
	if (m_hTapeWavPcmFile == INVALID_HANDLE_VALUE)
		return;  //TODO: Report about a bad WAV file

	wcscpy_s(m_szTapeFile, MAX_PATH, lpszFile);
	m_okTapeInserted = TRUE;
	m_okTapeRecording = FALSE;

	EnableWindow(m_hwndTapePlay, TRUE);
	SetWindowText(m_hwndTapePlay, _T("Play"));
	EnableWindow(m_hwndTapeRewind, TRUE);
	SetWindowText(m_hwndTapeFile, lpszFile);

	TapeView_UpdatePosition();

	DWORD wavLength = WavPcmFile_GetLength(m_hTapeWavPcmFile);
	int wavFreq = WavPcmFile_GetFrequency(m_hTapeWavPcmFile);
	double wavLengthSeconds = double(wavLength) / wavFreq;

	TCHAR buffer[64];
	wsprintf(buffer, _T("%d:%02d.%02d, %d Hz"),
		int(wavLengthSeconds) / 60, int(wavLengthSeconds) % 60, int(wavLengthSeconds * 100) % 100, wavFreq);
	SetWindowText(m_hwndTapeTotal, buffer);

	SetWindowText(m_hwndTapeOpen, _T("Close WAV"));
	EnableWindow(m_hwndTapeSave, FALSE);
}
void TapeView_CloseTape()
{
	// Stop tape playback
	TapeView_StopTape();

	WavPcmFile_Close(m_hTapeWavPcmFile);
	m_hTapeWavPcmFile = (HWAVPCMFILE) INVALID_HANDLE_VALUE;

	m_okTapeInserted = FALSE;

	EnableWindow(m_hwndTapePlay, FALSE);
	EnableWindow(m_hwndTapeRewind, FALSE);
	EnableWindow(m_hwndTapeOpen, TRUE);
	EnableWindow(m_hwndTapeSave, TRUE);
	SetWindowText(m_hwndTapeFile, NULL);
	SetWindowText(m_hwndTapeTotal, NULL);
	SetWindowText(m_hwndTapeCurrent, NULL);
	SetWindowText(m_hwndTapeOpen, _T("Open WAV"));
	SetWindowText(m_hwndTapeSave, _T("Save WAV"));
}
void TapeView_PlayTape()
{
	if (m_okTapePlaying) return;
	if (!m_okTapeInserted) return;

	int sampleRate = WavPcmFile_GetFrequency(m_hTapeWavPcmFile);
	
    if (m_okTapeRecording)
        g_pBoard->SetTapeWriteCallback(TapeView_TapeWriteCallback, sampleRate);
    else
        g_pBoard->SetTapeReadCallback(TapeView_TapeReadCallback, sampleRate);
	
    m_okTapePlaying = TRUE;
	SetWindowText(m_hwndTapePlay, _T("Stop"));
}
void TapeView_StopTape()
{
	if (!m_okTapePlaying) return;

	g_pBoard->SetTapeReadCallback(NULL, 0);
	m_okTapePlaying = FALSE;
	SetWindowText(m_hwndTapePlay, m_okTapeRecording ? _T("Record") : _T("Play"));
}

void TapeView_UpdatePosition()
{
	DWORD wavPos = WavPcmFile_GetPosition(m_hTapeWavPcmFile);
	int wavFreq = WavPcmFile_GetFrequency(m_hTapeWavPcmFile);
	double wavPosSeconds = double(wavPos) / wavFreq;
	TCHAR buffer[64];
	wsprintf(buffer, _T("%d:%02d.%02d"),
		int(wavPosSeconds) / 60, int(wavPosSeconds) % 60, int(wavPosSeconds * 100) % 100);
	SetWindowText(m_hwndTapeCurrent, buffer);

	m_dwTapePositionShown = wavPos;
}

void TapeView_DoOpenWav()
{
	if (m_okTapeInserted)
	{
		TapeView_CloseTape();
		return;
	}

	// File Open dialog
    TCHAR bufFileName[MAX_PATH];
    *bufFileName = 0;
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.hInstance = g_hInst;
    ofn.lpstrTitle = _T("Open WAV file");
    ofn.lpstrFilter = _T("WAV files (*.wav)\0*.wav\0All Files (*.*)\0*.*\0\0");
    ofn.Flags = OFN_FILEMUSTEXIST;
    ofn.lpstrFile = bufFileName;
    ofn.nMaxFile = sizeof(bufFileName) / sizeof(TCHAR);

    BOOL okResult = GetOpenFileName(&ofn);
    if (! okResult) return;

	TapeView_OpenTape(bufFileName);
}
void TapeView_DoSaveWav()
{
	if (m_okTapeInserted)
	{
		TapeView_CloseTape();
		return;
	}

	// File Save dialog
    TCHAR bufFileName[MAX_PATH];
    *bufFileName = 0;
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.hInstance = g_hInst;
    ofn.lpstrTitle = _T("Save WAV file");
    ofn.lpstrFilter = _T("WAV files (*.wav)\0*.wav\0All Files (*.*)\0*.*\0\0");
    ofn.Flags = OFN_FILEMUSTEXIST;
    ofn.lpstrFile = bufFileName;
    ofn.nMaxFile = sizeof(bufFileName) / sizeof(TCHAR);

    BOOL okResult = GetSaveFileName(&ofn);
    if (! okResult) return;

	TapeView_CreateTape(bufFileName);
}

void TapeView_DoPlayStop()
{
	if (m_okTapePlaying)
		TapeView_StopTape();
	else
		TapeView_PlayTape();
}

void TapeView_DoRewind()
{
	if (!m_okTapeInserted) return;

	WavPcmFile_SetPosition(m_hTapeWavPcmFile, 0);

	TapeView_UpdatePosition();
}


// Tape emulator callback used to read a tape recorded data.
// Input:
//   samples    Number of samples to play.
// Output:
//   result     Bit to put in tape input port.
BOOL CALLBACK TapeView_TapeReadCallback(UINT samples)
{
	if (samples == 0) return 0;

	UINT value = 0;
	for (UINT i = 0; i < samples; i++)
	{
		value = WavPcmFile_ReadOne(m_hTapeWavPcmFile);
	}
	BOOL result = (value > UINT_MAX / 2);
	
	DWORD wavLength = WavPcmFile_GetLength(m_hTapeWavPcmFile);
	DWORD wavPos = WavPcmFile_GetPosition(m_hTapeWavPcmFile);
	if (wavPos >= wavLength)  // End of tape
		TapeView_StopTape();

	int wavFreq = WavPcmFile_GetFrequency(m_hTapeWavPcmFile);
	if (wavPos - m_dwTapePositionShown > (DWORD)(wavFreq / 6) || !m_okTapePlaying)
	{
		TapeView_UpdatePosition();
	}

//#if !defined(PRODUCT)
//    DebugPrintFormat(_T("Tape: %d\r\n"), result);
//#endif

	return result;
}

void CALLBACK TapeView_TapeWriteCallback(int value, UINT samples)
{
    if (m_hTapeWavPcmFile == (HWAVPCMFILE)INVALID_HANDLE_VALUE) return;
    if (!m_okTapeRecording) return;
	if (samples == 0) return;

    // Write samples to the file
    for (UINT i = 0; i < samples; i++)
        WavPcmFile_WriteOne(m_hTapeWavPcmFile, value);

	DWORD wavPos = WavPcmFile_GetPosition(m_hTapeWavPcmFile);
	int wavFreq = WavPcmFile_GetFrequency(m_hTapeWavPcmFile);
	if (wavPos - m_dwTapePositionShown > (DWORD)(wavFreq / 6) || !m_okTapePlaying)
	{
		TapeView_UpdatePosition();
	}
}


//////////////////////////////////////////////////////////////////////
