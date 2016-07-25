//*****************************************
// Author: Fritz Ammon
//

#pragma comment(lib, "Winmm")

#include <iostream>
#include <io.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstdlib>
#include <ctime>
#include <cstdarg>
#include <Windows.h>
#include "RunOnce.h"
#include "SigScan.h"
#include "resource.h"

using std::cout;

#define keys_down(...) KeyDown(__VA_ARGS__, NULL)

void showInfo();
void signal(bool activated);
bool improveSleepAcc(bool activate = true);
bool checkProcess(SigScan * const sigScan);
bool KeyDown(int key, ...);
DWORD WINAPI HotkeyProc(LPVOID lpParam);
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

const COLORREF COLOR_KEY = RGB(255, 255, 255); // Color to ignore when displaying window
HWND g_hWnd = 0;
BITMAP g_bm;
HDC g_hdcBm = 0;
BOOL g_bDone = FALSE;
SigScan* g_pSigScan = 0;
UINT_PTR g_uipAddy, g_uipVelocity = 0;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	MSG msg;
	WNDCLASSEX wcx;
	HWND hWnd = 0;
	HBITMAP hbm = 0;
	HDC hdcParent = 0;
	HANDLE hPrevObj = 0;
	RunOnce* pRunOnce = 0;
	HANDLE hHkThread = 0;
	bool timeSlice = false;
	
	srand(static_cast<unsigned int> (time(reinterpret_cast<time_t*> (NULL))));

	msg.wParam = 0; // Initialize for return statement

	SecureZeroMemory(&wcx, sizeof(wcx));
	wcx.cbSize = sizeof(wcx);
	wcx.hbrBackground = static_cast<HBRUSH> (GetStockObject(NULL_BRUSH)); // So windows movement does not cause flickering
	wcx.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcx.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wcx.hInstance = hInstance;
	wcx.lpfnWndProc = WndProc;
	wcx.lpszClassName = TEXT("Port2Hack");
	wcx.lpszMenuName = NULL;
	wcx.style = CS_HREDRAW | CS_VREDRAW;

	if (!RegisterClassEx(&wcx))
		return 1;

	hbm = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_BITMAP1));
	if (!hbm)
		return 1;
	else
	{
		if (!GetObject(hbm, sizeof(g_bm), &g_bm))
			return 1;
	}

	hWnd = CreateWindowEx(
		WS_EX_LAYERED,
		TEXT("Port2Hack"),
		TEXT("Portal 2 Hack"),
		WS_POPUP,
		GetSystemMetrics(SM_CXSCREEN) / 2 - g_bm.bmWidth / 2,
		GetSystemMetrics(SM_CYSCREEN) / 2 - g_bm.bmHeight / 2,
		g_bm.bmWidth,
		g_bm.bmHeight,
		NULL,
		NULL,
		hInstance,
		NULL);
	
	if (!hWnd)
		return 1;
	else
	{
		pRunOnce = new RunOnce(hWnd);
		if (!pRunOnce->PerformCheck(TEXT("Port2Hack.tmp")))
			goto exit;

		hdcParent = GetDC(hWnd);
		g_hdcBm = CreateCompatibleDC(hdcParent);
		ReleaseDC(hWnd, hdcParent);

		hPrevObj = SelectObject(g_hdcBm, hbm);

		SetLayeredWindowAttributes(hWnd, COLOR_KEY, NULL, LWA_COLORKEY);

		ShowWindow(hWnd, nCmdShow);
		if (!UpdateWindow(hWnd))
			goto exit;
	}

	g_hWnd = hWnd;

	g_pSigScan = new SigScan(TEXT("portal2.exe"), TEXT("engine.dll"));

	timeSlice = improveSleepAcc(true);

	if ((hHkThread = CreateThread(NULL, 0, HotkeyProc,
		NULL, 0, 0)) == NULL)
		goto exit;

	while (!g_bDone)
	{
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

	if (hHkThread)
		CloseHandle(hHkThread);

	if (g_hdcBm && hPrevObj)
	{
		SelectObject(g_hdcBm, hPrevObj);
		DeleteDC(g_hdcBm);
	}

	if (g_pSigScan)
		delete g_pSigScan;

	if (pRunOnce)
		delete pRunOnce;

	return static_cast<int> (msg.wParam);
}

void signal(bool activated)
{
	if (activated)
	{
		Beep(1000, 100);
		Beep(1250, 100);
	}
	else
	{
		Beep(1250, 100);
		Beep(1000, 100);
	}
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

bool KeyDown(int key, ...)
{
	short result;
	va_list list;

	result = GetAsyncKeyState(key);
	for (va_start(list, key); key; key = va_arg(list, int))
		result &= GetAsyncKeyState(key);

	va_end(list);

	return ((result & 0x8000) != 0);
}

bool checkProcess(SigScan * const sigScan)
{
	SigScan sigTemp(TEXT("portal2.exe"), TEXT("engine.dll"));

	sigTemp.FindProcessId();
	
	if (!sigTemp.GetProcessId())
		return false;

	if (sigTemp.GetProcessId() != sigScan->GetProcessId() || sigScan->GetProcessId() == NULL)
	{
		sigScan->SetProcessId(sigTemp.GetProcessId());

		if (!sigScan->FindModInfo())
			return false;

		if (!sigScan->OpenProcess())
			return false;

		g_uipAddy = sigScan->GetBaseAddr() + 0x69FEB0;
	}

	if (!ReadProcessMemory(sigScan->GetProcess(), reinterpret_cast<LPCVOID> (g_uipAddy),
		reinterpret_cast<LPVOID> (&g_uipVelocity), sizeof(g_uipVelocity), NULL))
		return false;

	if (!ReadProcessMemory(sigScan->GetProcess(), reinterpret_cast<LPCVOID> (g_uipVelocity + 0x1D754),
		reinterpret_cast<LPVOID> (&g_uipVelocity), sizeof(g_uipVelocity), NULL))
		return false;

	if (!ReadProcessMemory(sigScan->GetProcess(), reinterpret_cast<LPCVOID> (g_uipVelocity + 0x0C),
		reinterpret_cast<LPVOID> (&g_uipVelocity), sizeof(g_uipVelocity), NULL))
		return false;

	g_uipVelocity += 0x16C;

	return true;
}

struct vel
{
	float x, y, z;
	vel()
	{
		x = y = z = 0.0f;
	}
};

DWORD WINAPI HotkeyProc(LPVOID lpParam)
{
	float MAX_SPEED = 200.0f;
	float RATE = 1.01f;
	float temp = 0.0f;
	bool showConsole = true;
	RECT rct, rctCon;
	vel velocity;

	UNREFERENCED_PARAMETER(lpParam);

	while (!g_bDone)
	{
		Sleep(1);

		if (keys_down(VK_F1))
		{
			if (showConsole)
			{
				GetWindowRect(g_hWnd, &rct);
				GetWindowRect(GetConsoleWindow(), &rctCon);
				SetWindowPos(GetConsoleWindow(), HWND_TOP,
					rct.left - ((rctCon.right - rctCon.left) / 2 - (rct.right - rct.left) / 2),
					rct.bottom + 6,
					0, 0, SWP_NOSIZE);
				ShowWindow(GetConsoleWindow(), SW_SHOW);
			}
			else
			{
				ShowWindow(GetConsoleWindow(), SW_HIDE);
			}

			showConsole = !showConsole;

			Sleep(250);
		}

		if (keys_down('Q'))
		{
			if (!checkProcess(g_pSigScan))
				continue;

			temp = MAX_SPEED;
			if (!WriteProcessMemory(g_pSigScan->GetProcess(), reinterpret_cast<LPVOID> (g_uipVelocity + 8),
				reinterpret_cast<LPCVOID> (&temp), sizeof(temp), NULL))
				return false;
		}

		if (keys_down('E'))
		{
			if (!checkProcess(g_pSigScan))
				continue;

			temp = -MAX_SPEED;
			if (!WriteProcessMemory(g_pSigScan->GetProcess(), reinterpret_cast<LPVOID> (g_uipVelocity + 8),
				reinterpret_cast<LPCVOID> (&temp), sizeof(temp), NULL))
				return false;
		}

		if (keys_down('R'))
		{
			if (!checkProcess(g_pSigScan))
				continue;

			temp = 0.0f;
			if (!WriteProcessMemory(g_pSigScan->GetProcess(), reinterpret_cast<LPVOID> (g_uipVelocity),
				reinterpret_cast<LPCVOID> (&temp), sizeof(temp), NULL))
				return false;

			if (!WriteProcessMemory(g_pSigScan->GetProcess(), reinterpret_cast<LPVOID> (g_uipVelocity + 4),
				reinterpret_cast<LPCVOID> (&temp), sizeof(temp), NULL))
				return false;

			temp = 10.0f;
			if (!WriteProcessMemory(g_pSigScan->GetProcess(), reinterpret_cast<LPVOID> (g_uipVelocity + 8),
				reinterpret_cast<LPCVOID> (&temp), sizeof(temp), NULL))
				return false;
		}

		if (keys_down(VK_UP))
		{
			if (MAX_SPEED < 1000.0f)
			{
				Beep(1000, 100);
				MAX_SPEED += 200.0f;
				Beep(1250, 100);
			}
		}

		if (keys_down(VK_DOWN))
		{
			if (MAX_SPEED > 200.0f)
			{
				Beep(1250, 100);
				MAX_SPEED -= 200.0f;
				Beep(1000, 100);
			}
		}

		if (keys_down(VK_SHIFT, 'W'))
		{
			if (!checkProcess(g_pSigScan))
				continue;

			temp = MAX_SPEED;
			if (!WriteProcessMemory(g_pSigScan->GetProcess(), reinterpret_cast<LPVOID> (g_uipVelocity),
				reinterpret_cast<LPCVOID> (&temp), sizeof(temp), NULL))
				return false;
		}

		if (keys_down(VK_SHIFT, 'S'))
		{
			if (!checkProcess(g_pSigScan))
				continue;

			temp = -MAX_SPEED;
			if (!WriteProcessMemory(g_pSigScan->GetProcess(), reinterpret_cast<LPVOID> (g_uipVelocity),
				reinterpret_cast<LPCVOID> (&temp), sizeof(temp), NULL))
				return false;
		}

		if (keys_down(VK_SHIFT, 'A'))
		{
			if (!checkProcess(g_pSigScan))
				continue;

			temp = MAX_SPEED;
			if (!WriteProcessMemory(g_pSigScan->GetProcess(), reinterpret_cast<LPVOID> (g_uipVelocity + 4),
				reinterpret_cast<LPCVOID> (&temp), sizeof(temp), NULL))
				return false;
		}

		if (keys_down(VK_SHIFT, 'D'))
		{
			if (!checkProcess(g_pSigScan))
				continue;

			temp = -MAX_SPEED;
			if (!WriteProcessMemory(g_pSigScan->GetProcess(), reinterpret_cast<LPVOID> (g_uipVelocity + 4),
				reinterpret_cast<LPCVOID> (&temp), sizeof(temp), NULL))
				return false;
		}

		if (keys_down('F'))
		{
			if (!checkProcess(g_pSigScan))
				continue;

			if (!ReadProcessMemory(g_pSigScan->GetProcess(), reinterpret_cast<LPCVOID> (g_uipVelocity),
				reinterpret_cast<LPVOID> (&velocity), sizeof(velocity), NULL))
				return false;

			velocity.x *= RATE;
			velocity.y *= RATE;
			velocity.z = 10.0f;

			if (!WriteProcessMemory(g_pSigScan->GetProcess(), reinterpret_cast<LPVOID> (g_uipVelocity),
				reinterpret_cast<LPCVOID> (&velocity), sizeof(velocity), NULL))
				return false;
		}
	}

	return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int hConHandle;
	HANDLE hStdHandle;
	FILE* fp;
	PAINTSTRUCT ps;
	HDC hdc = 0;
	HMENU hMenu = 0;

	switch (uMsg)
	{
	case WM_CREATE:
		AllocConsole();
		ShowWindow(GetConsoleWindow(), SW_HIDE);

		hMenu = GetSystemMenu(GetConsoleWindow(), FALSE);
		DeleteMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);

		SetConsoleTitle(TEXT("Portal 2 Hack v1.0 Info (Hotkeys)"));

		hStdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		hConHandle = _open_osfhandle(reinterpret_cast<intptr_t> (hStdHandle), _O_TEXT);
		fp = _fdopen(hConHandle, "w");
		*stdout = *fp;
		setvbuf(stdout, NULL, _IONBF, 0);
		ios::sync_with_stdio();

		cout << "Press F1 to hide/show info.\n\n";

		cout << "Hotkeys:\n\n";
		cout << "\tQ - Increase height\n"
			 << "\tE - Decrease height\n"
			 << "\tR - Freeze, F - Speed up\n"
			 << "\tUP/DOWN - Change speed\n"
			 << "\tShift + WASD - Move in direction\n";
		break;

	case WM_PAINT:
		hdc = BeginPaint(hwnd, &ps);

		BitBlt(hdc, 0, 0, g_bm.bmWidth, g_bm.bmHeight, g_hdcBm, 0, 0, SRCCOPY);

		EndPaint(hwnd, &ps);
		break;

	case WM_LBUTTONDOWN:
		SendMessage(hwnd, WM_SYSCOMMAND, SC_MOVE, lParam);
		break;

	case WM_NCHITTEST: // Little hack to trick Windows into thinking caption is being clicked on
		if (DefWindowProc(hwnd, uMsg, wParam, lParam) == HTCLIENT)
			return HTCAPTION;

		break;

	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
			DestroyWindow(hwnd);
		break;

	case WM_DESTROY:
		FreeConsole();
		g_bDone = TRUE;
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	return 0;
}