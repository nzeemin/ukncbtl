/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// TapeView.cpp

#include "stdafx.h"
#include <commdlg.h>
#include "UKNCBTL.h"
#include "Views.h"
#include "ToolWindow.h"
#include "util\\WavPcmFile.h"
#include "Emulator.h"
#include "Dialogs.h"


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
HWND m_hwndTapeGraph = (HWND) INVALID_HANDLE_VALUE;

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

void TapeView_ClearGraph();
void TapeView_DrawGraph(LPDRAWITEMSTRUCT lpdis);

void TapeView_OnDraw(HDC hdc);
void TapeView_DoOpenWav();
void TapeView_DoSaveWav();
void TapeView_DoPlayStop();
void TapeView_DoRewind();

BOOL CALLBACK TapeView_TapeReadCallback(UINT samples);
void CALLBACK TapeView_TapeWriteCallback(int value, UINT samples);

#define TAPE_BUFFER_SIZE 624
BYTE m_TapeBuffer[TAPE_BUFFER_SIZE];


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
    wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE + 1);
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

void TapeView_Create(HWND hwndParent, int x, int y, int width, int height)
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
            8, 62, 100, 18,
            g_hwndTape, NULL, g_hInst, NULL);
    m_hwndTapeTotal = CreateWindow(
            _T("STATIC"), NULL,
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            500 + 8 + 4, 4, rcClient.right - 8 * 2 - 500 - 4, 18,
            g_hwndTape, NULL, g_hInst, NULL);
    m_hwndTapeGraph = CreateWindow(
            _T("STATIC"), NULL,
            WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
            8, 22, TAPE_BUFFER_SIZE, 32,
            g_hwndTape, NULL, g_hInst, NULL);
    m_hwndTapePlay = CreateWindow(
            _T("BUTTON"), _T("Play"),
            WS_CHILD | WS_VISIBLE | WS_DISABLED,
            8 + 100 + 16, 60, 96, 22,
            g_hwndTape, NULL, g_hInst, NULL);
    m_hwndTapeRewind = CreateWindow(
            _T("BUTTON"), _T("<< Rewind"),
            WS_CHILD | WS_VISIBLE | WS_DISABLED,
            8 + 100 + 16 + 4 + 96, 60, 96, 22,
            g_hwndTape, NULL, g_hInst, NULL);
    m_hwndTapeOpen = CreateWindow(
            _T("BUTTON"), _T("Open WAV"),
            WS_CHILD | WS_VISIBLE,
            rcClient.right - 96 - 4 - 96 - 8, 60, 96, 22,
            g_hwndTape, NULL, g_hInst, NULL);
    m_hwndTapeSave = CreateWindow(
            _T("BUTTON"), _T("Save WAV"),
            WS_CHILD | WS_VISIBLE,
            rcClient.right - 96 - 8, 60, 96, 22,
            g_hwndTape, NULL, g_hInst, NULL);

    m_hfontTape = CreateDialogFont();
    SendMessage(m_hwndTapeCurrent, WM_SETFONT, (WPARAM) m_hfontTape, 0);
    SendMessage(m_hwndTapeTotal, WM_SETFONT, (WPARAM) m_hfontTape, 0);
    SendMessage(m_hwndTapeFile, WM_SETFONT, (WPARAM) m_hfontTape, 0);
    SendMessage(m_hwndTapePlay, WM_SETFONT, (WPARAM) m_hfontTape, 0);
    SendMessage(m_hwndTapeRewind, WM_SETFONT, (WPARAM) m_hfontTape, 0);
    SendMessage(m_hwndTapeOpen, WM_SETFONT, (WPARAM) m_hfontTape, 0);
    SendMessage(m_hwndTapeSave, WM_SETFONT, (WPARAM) m_hfontTape, 0);

    TapeView_ClearGraph();
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
    case WM_DRAWITEM:
        TapeView_DrawGraph((LPDRAWITEMSTRUCT)lParam);
        return (LRESULT)TRUE;
    default:
        return CallWindowProc(m_wndprocTapeToolWindow, hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

void TapeView_ClearGraph()
{
    memset(m_TapeBuffer, 0, sizeof(m_TapeBuffer));

    if (m_hwndTapeGraph != INVALID_HANDLE_VALUE)
    {
        InvalidateRect(m_hwndTapeGraph, NULL, FALSE);
    }
}
void TapeView_DrawGraph(LPDRAWITEMSTRUCT lpdis)
{
    HDC hdc = lpdis->hDC;
    RECT rcItem = lpdis->rcItem;
    PatBlt(hdc, rcItem.left, rcItem.top, rcItem.right - rcItem.left, rcItem.bottom - rcItem.top, WHITENESS);

    int x = rcItem.left;
    for (int i = 0; i < TAPE_BUFFER_SIZE; i++)
    {
        BYTE value = m_TapeBuffer[i];
        if (value == 0) continue;
        value = value >> 3;
        COLORREF color = (value >= 16) ? RGB(0, 192, 0) : RGB(192, 0, 0);
        SetPixel(hdc, x, rcItem.bottom - value - 1, color);
        x++;
    }
}

void TapeView_CreateTape(LPCTSTR lpszFile)
{
    m_hTapeWavPcmFile = WavPcmFile_Create(lpszFile, 44100);
    if (m_hTapeWavPcmFile == INVALID_HANDLE_VALUE)
        return;  //TODO: Report error

    _tcscpy_s(m_szTapeFile, MAX_PATH, lpszFile);
    m_okTapeInserted = TRUE;
    m_okTapeRecording = TRUE;

    EnableWindow(m_hwndTapePlay, TRUE);
    SetWindowText(m_hwndTapePlay, _T("Record"));
    EnableWindow(m_hwndTapeRewind, TRUE);
    SetWindowText(m_hwndTapeFile, lpszFile);

    TapeView_UpdatePosition();
    TapeView_ClearGraph();

    SetWindowText(m_hwndTapeSave, _T("Close WAV"));
    EnableWindow(m_hwndTapeOpen, FALSE);
}
void TapeView_OpenTape(LPCTSTR lpszFile)
{
    m_hTapeWavPcmFile = WavPcmFile_Open(lpszFile);
    if (m_hTapeWavPcmFile == INVALID_HANDLE_VALUE)
        return;  //TODO: Report about a bad WAV file

    _tcscpy_s(m_szTapeFile, MAX_PATH, lpszFile);
    m_okTapeInserted = TRUE;
    m_okTapeRecording = FALSE;

    EnableWindow(m_hwndTapePlay, TRUE);
    SetWindowText(m_hwndTapePlay, _T("Play"));
    EnableWindow(m_hwndTapeRewind, TRUE);
    SetWindowText(m_hwndTapeFile, lpszFile);

    TapeView_UpdatePosition();
    TapeView_ClearGraph();

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

    TapeView_ClearGraph();
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

    if (m_okTapeRecording)
        g_pBoard->SetTapeWriteCallback(NULL, 0);
    else
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
    BOOL okResult = ShowOpenDialog(g_hwnd,
            _T("Open WAV file"),
            _T("WAV files (*.wav)\0*.wav\0All Files (*.*)\0*.*\0\0"),
            bufFileName);
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
    BOOL okResult = ShowSaveDialog(g_hwnd,
            _T("Save WAV file"),
            _T("WAV files (*.wav)\0*.wav\0All Files (*.*)\0*.*\0\0"),
            _T("wav"),
            bufFileName);
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
BOOL CALLBACK TapeView_TapeReadCallback(unsigned int samples)
{
    if (samples == 0) return 0;

    // Scroll buffer
    memmove(m_TapeBuffer, m_TapeBuffer + samples, TAPE_BUFFER_SIZE - samples);

    UINT value = 0;
    for (UINT i = 0; i < samples; i++)
    {
        value = WavPcmFile_ReadOne(m_hTapeWavPcmFile);
        *(m_TapeBuffer + TAPE_BUFFER_SIZE - samples + i) = (BYTE)((value >> 24) & 0xff);
    }
    BOOL result = (value >= UINT_MAX / 2);

    InvalidateRect(m_hwndTapeGraph, NULL, FALSE);

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

    // Scroll buffer
    memmove(m_TapeBuffer, m_TapeBuffer + samples, TAPE_BUFFER_SIZE - samples);

    // Write samples to the file
    for (UINT i = 0; i < samples; i++)
    {
        WavPcmFile_WriteOne(m_hTapeWavPcmFile, value);
        //TODO: Check WavPcmFile_WriteOne result
        *(m_TapeBuffer + TAPE_BUFFER_SIZE - samples + i) = (BYTE)((value >> 24) & 0xff);
    }

    InvalidateRect(m_hwndTapeGraph, NULL, FALSE);

    DWORD wavPos = WavPcmFile_GetPosition(m_hTapeWavPcmFile);
    int wavFreq = WavPcmFile_GetFrequency(m_hTapeWavPcmFile);
    if (wavPos - m_dwTapePositionShown > (DWORD)(wavFreq / 6) || !m_okTapePlaying)
    {
        TapeView_UpdatePosition();
    }
}


//////////////////////////////////////////////////////////////////////
