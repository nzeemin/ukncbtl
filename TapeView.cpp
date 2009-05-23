// TapeView.cpp

#include "stdafx.h"
#include <commdlg.h>
#include "UKNCBTL.h"
#include "Views.h"
#include "util\\WavPcmFile.h"


//////////////////////////////////////////////////////////////////////


HWND g_hwndTape = (HWND) INVALID_HANDLE_VALUE;  // Tape View window handle

HWND m_hwndTapeGroup = (HWND) INVALID_HANDLE_VALUE;
HWND m_hwndTapeFile = (HWND) INVALID_HANDLE_VALUE;  // Tape file name - read-only edit control
HWND m_hwndTapeTotal = (HWND) INVALID_HANDLE_VALUE;  // Tape total time - static control
HWND m_hwndTapeCurrent = (HWND) INVALID_HANDLE_VALUE;  // Tape current time - static control
HWND m_hwndTapePlay = (HWND) INVALID_HANDLE_VALUE;
HWND m_hwndTapeEject = (HWND) INVALID_HANDLE_VALUE;

HFONT m_hfontTape = NULL;
BOOL m_okTapeInserted = FALSE;
TCHAR m_szTapeFile[MAX_PATH];
HWAVPCMFILE m_hTapeWavPcmFile = (HWAVPCMFILE) INVALID_HANDLE_VALUE;

void TapeView_OnDraw(HDC hdc);
void TapeView_InsertTape(LPCTSTR lpszFile);
void TapeView_RemoveTape();
void TapeView_DoEject();
void TapeView_DoPlayStop();


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
        CLASSNAME_TAPEVIEW, NULL,
        WS_CHILD | WS_VISIBLE,
        x, y, width, height,
        hwndParent, NULL, g_hInst, NULL);

    RECT rcClient;  GetClientRect(g_hwndTape, &rcClient);

	m_hwndTapeGroup = CreateWindow(
            _T("BUTTON"), NULL,
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            0, 0, rcClient.right, rcClient.bottom,
            g_hwndTape, NULL, g_hInst, NULL);
	m_hwndTapeFile = CreateWindow(
            _T("STATIC"), NULL,
            WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS,
            12, 20, rcClient.right - 24, 20,
            g_hwndTape, NULL, g_hInst, NULL);
	m_hwndTapeCurrent = CreateWindow(
            _T("STATIC"), NULL,
            WS_CHILD | WS_VISIBLE,
            12, 44, 100, 20,
            g_hwndTape, NULL, g_hInst, NULL);
	m_hwndTapeTotal = CreateWindow(
            _T("STATIC"), NULL,
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            rcClient.right - 212, 44, 200, 20,
            g_hwndTape, NULL, g_hInst, NULL);
	m_hwndTapePlay = CreateWindow(
            _T("BUTTON"), NULL,
            WS_CHILD | WS_VISIBLE | WS_DISABLED,
            8, 68, 120, 24,
            g_hwndTape, NULL, g_hInst, NULL);
	m_hwndTapeEject = CreateWindow(
            _T("BUTTON"), NULL,
            WS_CHILD | WS_VISIBLE,
            rcClient.right - 128, 68, 120, 24,
            g_hwndTape, NULL, g_hInst, NULL);

	SetWindowText(m_hwndTapeGroup, _T("Tape"));
	SetWindowText(m_hwndTapePlay, _T("Play / Stop"));
	SetWindowText(m_hwndTapeEject, _T("Eject"));

	m_hfontTape = CreateDialogFont();
	SendMessage(m_hwndTapeGroup, WM_SETFONT, (WPARAM) m_hfontTape, 0);
	SendMessage(m_hwndTapeCurrent, WM_SETFONT, (WPARAM) m_hfontTape, 0);
	SendMessage(m_hwndTapeTotal, WM_SETFONT, (WPARAM) m_hfontTape, 0);
	SendMessage(m_hwndTapeFile, WM_SETFONT, (WPARAM) m_hfontTape, 0);
	SendMessage(m_hwndTapePlay, WM_SETFONT, (WPARAM) m_hfontTape, 0);
	SendMessage(m_hwndTapeEject, WM_SETFONT, (WPARAM) m_hfontTape, 0);
}

LRESULT CALLBACK TapeViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    //case WM_PAINT:
    //    {
    //        PAINTSTRUCT ps;
    //        HDC hdc = BeginPaint(hWnd, &ps);

    //        TapeView_OnDraw(hdc);

    //        EndPaint(hWnd, &ps);
    //    }
    //    break;
	case WM_COMMAND:
		{
			HWND hwndCtl = (HWND)lParam;
			if (hwndCtl == m_hwndTapeEject)
				TapeView_DoEject();
			else if (hwndCtl == m_hwndTapePlay)
				TapeView_DoPlayStop();
		}
		break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

//void TapeView_OnDraw(HDC hdc)
//{
//	//TODO
//}

void TapeView_InsertTape(LPCTSTR lpszFile)
{
	m_hTapeWavPcmFile = WavPcmFile_Open(lpszFile);
	if (m_hTapeWavPcmFile == INVALID_HANDLE_VALUE)
		return;  //TODO: Report about a bad WAV file

	wcscpy_s(m_szTapeFile, MAX_PATH, lpszFile);
	m_okTapeInserted = TRUE;

	EnableWindow(m_hwndTapePlay, TRUE);
	SetWindowText(m_hwndTapeFile, lpszFile);

	SetWindowText(m_hwndTapeCurrent, _T("0:00.00"));

	DWORD wavLength = WavPcmFile_GetLength(m_hTapeWavPcmFile);
	int wavFreq = WavPcmFile_GetFrequency(m_hTapeWavPcmFile);
	double wavLengthSeconds = double(wavLength) / wavFreq;
	int wavMinutes = int(wavLengthSeconds / 60);
	wavLengthSeconds -= wavMinutes * 60;
	int wavSeconds = int(wavLengthSeconds);
	wavLengthSeconds -= wavSeconds;
	int wavPrec = int(wavLengthSeconds * 100);

	TCHAR buffer[64];
	wsprintf(buffer, _T("%d:%02d.%02d, %d Hz"), wavMinutes, wavSeconds, wavPrec, wavFreq);
	SetWindowText(m_hwndTapeTotal, buffer); // _T("1:00:00"));
}

void TapeView_RemoveTape()
{
	//TODO: Stop tape playback

	WavPcmFile_Close(m_hTapeWavPcmFile);
	m_hTapeWavPcmFile = (HWAVPCMFILE) INVALID_HANDLE_VALUE;

	m_okTapeInserted = FALSE;

	EnableWindow(m_hwndTapePlay, FALSE);
	SetWindowText(m_hwndTapeFile, NULL);
	SetWindowText(m_hwndTapeTotal, NULL);
	SetWindowText(m_hwndTapeCurrent, NULL);
}

void TapeView_DoEject()
{
	if (m_okTapeInserted)
	{
		TapeView_RemoveTape();
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

	TapeView_InsertTape(bufFileName);
}

void TapeView_DoPlayStop()
{
	//TODO
}


//////////////////////////////////////////////////////////////////////
