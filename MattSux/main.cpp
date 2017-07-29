//*****************************************
// Author: Fritz Ammon
// 7/28/2017
// 
// JUST KIDDING, MATT! <3
//
// It would've been better to use a single spreadsheet, yes.
// Shh. 8)

#pragma comment(lib, "Winmm")

#include <cstdlib>
#include <ctime>
#include <tchar.h>
#include <Windows.h>
#include <TlHelp32.h>
#include "RunOnce.h"
#include "resource.h"

//#define keys_down(...) KeyDown(__VA_ARGS__, NULL)

bool improveSleepAcc(bool activate = true);
//bool KeyDown(int key, ...);
//DWORD WINAPI HotkeyProc(LPVOID lpParam);
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
DWORD WINAPI PaintProc(LPVOID lpParam);
DWORD WINAPI TaskManagerProc(LPVOID lpParam);

// Color to ignore when displaying window
const COLORREF COLOR_KEY = RGB(255, 255, 255);
const unsigned int NUM_FRAMES = 10;
const DWORD TICKS_PER_FRAME = 75;

CHAR g_szPassword[] = "MATTSUX";

HWND g_hWnd = 0;
BITMAP g_bm;
HDC g_hdcBm = 0;
HBITMAP g_hbm[NUM_FRAMES] = {0};
int g_hbmIdx = 0;
BOOL g_bPainting = FALSE;
int g_nPwdIdx = 0;
BOOL g_bDone = FALSE;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	MSG msg;
	WNDCLASSEX wcx;
	HDC hdcParent = 0;
	RunOnce* pRunOnce = 0;
	//HANDLE hHkThread = 0;
	bool timeSlice = false;
	HHOOK hHookMs = 0;
	HHOOK hHookKbd = 0;
	//DWORD dwTicksAtPaint = 0;
	HANDLE hPThread = 0;
	HANDLE hTMThread = 0;
	POINT ptMousePos;
	
	srand(static_cast<unsigned int> (time(reinterpret_cast<time_t*> (NULL))));

	// Initialize for return statement
	msg.wParam = 0;

	SecureZeroMemory(&wcx, sizeof(wcx));
	wcx.cbSize = sizeof(wcx);
	// NULL_BRUSH so that window movement does not cause flickering
	wcx.hbrBackground = static_cast<HBRUSH> (GetStockObject(NULL_BRUSH));
	wcx.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcx.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wcx.hInstance = hInstance;
	wcx.lpfnWndProc = WndProc;
	wcx.lpszClassName = TEXT("MattSux");
	wcx.lpszMenuName = NULL;
	wcx.style = CS_HREDRAW | CS_VREDRAW;

	if (!RegisterClassEx(&wcx))
		return 1;

	for (int bmp = IDB_BITMAP1; bmp <= IDB_BITMAP10; bmp++) {
		g_hbm[bmp - IDB_BITMAP1] = LoadBitmap(hInstance, MAKEINTRESOURCE(bmp));
		if (!g_hbm[bmp - IDB_BITMAP1])
			return 1;
		else
		{
			if (!GetObject(g_hbm[bmp - IDB_BITMAP1], sizeof(g_bm), &g_bm))
				return 1;
		}
	}

	GetCursorPos(&ptMousePos);

	g_hWnd = CreateWindowEx(
		WS_EX_LAYERED,
		TEXT("MattSux"),
		TEXT("Matt Sux"),
		WS_POPUP,
		ptMousePos.x - (g_bm.bmWidth >> 1),//GetSystemMetrics(SM_CXSCREEN) / 2 - g_bm.bmWidth / 2,
		ptMousePos.y - (g_bm.bmHeight >> 1),//GetSystemMetrics(SM_CYSCREEN) / 2 - g_bm.bmHeight / 2,
		g_bm.bmWidth,
		g_bm.bmHeight,
		NULL,
		NULL,
		hInstance,
		NULL);
	
	if (!g_hWnd)
		return 1;
	else
	{
		pRunOnce = new RunOnce(g_hWnd);
		if (!pRunOnce->PerformCheck(TEXT("MattSux.tmp")))
			goto exit;

		hdcParent = GetDC(g_hWnd);
		g_hdcBm = CreateCompatibleDC(hdcParent);
		ReleaseDC(g_hWnd, hdcParent);

		SetLayeredWindowAttributes(g_hWnd, COLOR_KEY, NULL, LWA_COLORKEY);

		ShowWindow(g_hWnd, nCmdShow);
		if (!UpdateWindow(g_hWnd))
			goto exit;
	}

	timeSlice = improveSleepAcc(true);

	//if ((hHkThread = CreateThread(NULL, 0, HotkeyProc,
	//	NULL, 0, 0)) == NULL)
	//	goto exit;

	if ((hPThread = CreateThread(NULL, 0, PaintProc,
		NULL, 0, 0)) == NULL)
		goto exit;

	if ((hTMThread = CreateThread(NULL, 0, TaskManagerProc,
		NULL, 0, 0)) == NULL)
		goto exit;

	if ((hHookMs = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0)) == NULL)
		goto exit;

	if ((hHookKbd = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0)) == NULL)
		goto exit;

	while (!g_bDone)
	{
		// Was originally going to place InvalidateRect under PeekMessage when app is idle,
		// but I wanted to be able to Sleep the thread because letting it run loose uses up a
		// good chunk of CPU. However, this caused problems for my synchronous keyboard and mouse
		// event callbacks (sleep would be in progress when one of those events needed processing).
		// So you would see lag for mouse movement, for example.
		// So I switched back to synchronous GetMessage and create a separate thread just for
		// calling InvalidateRect only when the app is not painting and enough ticks have passed.
		// Of course, the sleep to reduce CPU usage is there too. (Callback is called PaintProc)

		//if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != 0)
		//{
		//	switch (msg.message)
		//	{
		//	case WM_PAINT:
		//		TranslateMessage(&msg);
		//		DispatchMessage(&msg);
		//		break;

		//	default:
		//		// What?
		//		break;
		//	}
		//}
		//else
		//{
		//	if (GetTickCount() - dwTicksAtPaint >= TICKS_PER_FRAME)
		//	{
		//		InvalidateRect(g_hWnd, NULL, false);
		//		dwTicksAtPaint = GetTickCount();
		//	}
		//}

		if ((GetMessage(&msg, NULL, 0, 0) > 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
			g_bDone = TRUE;
	}

exit:
	if (timeSlice)
		improveSleepAcc(false);

	//if (hHkThread)
	//	CloseHandle(hHkThread);

	if (hPThread)
		CloseHandle(hPThread);

	if (hTMThread)
		CloseHandle(hTMThread);

	if (hHookMs)
		UnhookWindowsHookEx(hHookMs);

	if (hHookKbd)
		UnhookWindowsHookEx(hHookKbd);

	if (g_hdcBm)
	{
		DeleteDC(g_hdcBm);
	}

	if (pRunOnce)
		delete pRunOnce;

	return static_cast<int> (msg.wParam);
}

bool improveSleepAcc(bool activate)
{
	TIMECAPS tc;
	MMRESULT mmr;

	// Fill the TIMECAPS structure.
	if (timeGetDevCaps(&tc, sizeof(tc)) != MMSYSERR_NOERROR)
		return false;

	if (activate)
		mmr = timeBeginPeriod(tc.wPeriodMin);
	else
		mmr = timeEndPeriod(tc.wPeriodMin);

	if (mmr != TIMERR_NOERROR)
		return false;

	return true;
}

//bool KeyDown(int key, ...)
//{
//	short result;
//	va_list list;
//
//	result = GetAsyncKeyState(key);
//	for (va_start(list, key); key; key = va_arg(list, int))
//		result &= GetAsyncKeyState(key);
//
//	va_end(list);
//
//	return ((result & 0x8000) != 0);
//}

//DWORD WINAPI HotkeyProc(LPVOID lpParam)
//{
//	UNREFERENCED_PARAMETER(lpParam);
//
//	while (!g_bDone)
//	{
//		Sleep(1);
//
//		if (keys_down(VK_F1))
//		{
//
//		}
//	}
//
//	return 0;
//}

DWORD WINAPI PaintProc(LPVOID lpParam)
{
	UNREFERENCED_PARAMETER(lpParam);

	DWORD dwTicksAtPaint = 0;

	while (!g_bDone)
	{
		if ((GetTickCount() - dwTicksAtPaint >= TICKS_PER_FRAME) && !g_bPainting)
		{
			InvalidateRect(g_hWnd, NULL, false);
			dwTicksAtPaint = GetTickCount();
		}

		Sleep(1);
	}

	return 0;
}

// Task manager takes priority and gets messages before me, so let's kill it with fire--err, code.
DWORD WINAPI TaskManagerProc(LPVOID lpParam)
{
	UNREFERENCED_PARAMETER(lpParam);

	HANDLE snapshot = 0;
	PROCESSENTRY32 pe32;
	TCHAR *killProcs[] = {TEXT("taskmgr.exe")};
	TCHAR *tStrCopy = 0;
	HANDLE hProcess = 0;
	DWORD exitCode = 0;

	tStrCopy = new TCHAR[MAX_PATH];

	while (!g_bDone)
	{
		snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

		SecureZeroMemory(&pe32, sizeof(pe32));
		pe32.dwSize = sizeof(PROCESSENTRY32);
		if (!Process32First(snapshot, &pe32))
			return FALSE;

		// Sweet n^2 of peace.
		do
		{
			// Omg, for each in! Someday we'll use awesome sauce lambda closures too!
			for each (TCHAR *szName in killProcs)
			{
				SecureZeroMemory(tStrCopy, MAX_PATH);
				// Generates warning.
				_tcscpy(tStrCopy, pe32.szExeFile);
				for (TCHAR* tstr = tStrCopy; *tstr; ++tstr)
					*tstr = tolower(static_cast<TCHAR> (*tstr));

				if (!_tcscmp(szName, tStrCopy))
				{
					if ((hProcess = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION, FALSE, pe32.th32ProcessID)) == NULL)
						continue;

					if (GetExitCodeProcess(hProcess, &exitCode) != 0)
					{
						TerminateProcess(hProcess, exitCode);
					}

					CloseHandle(hProcess);
				}
			}

			Sleep(1);
		} while (Process32Next(snapshot, &pe32));

		CloseHandle(snapshot);
	}

	delete [] tStrCopy;
	tStrCopy = 0;

	return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc = 0;

	switch (uMsg)
	{
	case WM_PAINT:
		g_bPainting = TRUE;
		hdc = BeginPaint(hwnd, &ps);

		SelectObject(g_hdcBm, g_hbm[g_hbmIdx++]);

		if (g_hbmIdx == NUM_FRAMES)
			g_hbmIdx = 0;

		BitBlt(hdc, 0, 0, g_bm.bmWidth, g_bm.bmHeight, g_hdcBm, 0, 0, SRCCOPY);

		EndPaint(hwnd, &ps);
		g_bPainting = FALSE;
		break;

	// Little hack to trick Windows into thinking caption is being clicked on
	case WM_LBUTTONDOWN:
		SendMessage(hwnd, WM_SYSCOMMAND, SC_MOVE, lParam);
		break;

	case WM_NCHITTEST:
		if (DefWindowProc(hwnd, uMsg, wParam, lParam) == HTCLIENT)
			return HTCAPTION;
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	return 0;
}

// CAUTION: Be careful blocking execution of the low level callback below. Doing so can block the entire message chain for the whole system.
LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	MSLLHOOKSTRUCT* ms;

	if (nCode == HC_ACTION)
	{
		ms = (MSLLHOOKSTRUCT*)lParam;

		switch (wParam)
		{
		case WM_RBUTTONDOWN:
		case WM_LBUTTONDOWN:
			SetWindowPos(g_hWnd, HWND_TOPMOST, ms->pt.x - (g_bm.bmWidth >> 1), ms->pt.y - (g_bm.bmHeight >> 1), 0, 0, SWP_NOSIZE);
			break;

		case WM_MOUSEMOVE:
			SetWindowPos(g_hWnd, HWND_TOPMOST, ms->pt.x - (g_bm.bmWidth >> 1), ms->pt.y - (g_bm.bmHeight >> 1), 0, 0, SWP_NOSIZE);

			// It's more entertaining letting the mouse move around.
			return CallNextHookEx(NULL, nCode, wParam, lParam);
			break;

		default:
			return CallNextHookEx(NULL, nCode, wParam, lParam);
			// Lol.
			break;
		}

		return 1;
	}
	
	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// CAUTION: Be careful blocking execution of the low level callback below. Doing so can block the entire message chain for the whole system.
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	KBDLLHOOKSTRUCT* kbd;

	if (nCode == HC_ACTION)
	{
		kbd = (KBDLLHOOKSTRUCT*)lParam;

		switch (wParam)
		{
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if (g_szPassword[g_nPwdIdx] == kbd->vkCode)
			{
				if (++g_nPwdIdx == strlen(g_szPassword))
					DestroyWindow(g_hWnd);
			}
			else
				g_nPwdIdx = 0;
			break;

		default:
			return CallNextHookEx(NULL, nCode, wParam, lParam);
			// Lol.
			break;
		}

		return 1;
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}