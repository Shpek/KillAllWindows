#include "pch.h"
#include "resource.h"

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK CursorWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void KillWindowUnderCursor();

HINSTANCE g_hInstance = NULL;
HCURSOR g_hKillCursor = NULL;
wchar_t g_wBuf[MAX_PATH];
WNDCLASS g_wndClsCursor;
HWND g_hwndCursor;
bool g_bDebugMode = false;
const wchar_t *g_wExplorerName = L"explorer.exe";
const wchar_t *g_wProgramManagerName = L"Program Manager";

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow)
{
	HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS, 0, L"KillAllWindowsMutex");
	
	if (!hMutex) {
		hMutex = CreateMutex(0, 0, L"KillAllWindowsMutex");
	} else {
		return 0;
	}

	if (g_bDebugMode) {
		AllocConsole();
		FILE* pFile;
		freopen_s(&pFile, "CONOUT$", "w", stdout);
		std::wcout << "KillAllWindows started" << std::endl;
	}

	g_hKillCursor = 0;
	g_hwndCursor = NULL;

	g_hInstance = GetModuleHandle(NULL);
	MSG msg = { 0 };
	WNDCLASS mainWindowCls = { 0 };
	mainWindowCls.lpfnWndProc = WndProc;
	mainWindowCls.hInstance = g_hInstance;
	mainWindowCls.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
	mainWindowCls.lpszClassName = L"KillAllWindows";
	
	if (!RegisterClass(&mainWindowCls))
		return 1;

	if (!CreateWindow(mainWindowCls.lpszClassName,
		L"Kill All Windows",
		WS_POPUP,
		0, 0, 100, 100, HWND_MESSAGE, 0, g_hInstance, NULL))
		return 2;

	g_wndClsCursor = { 0 };
	g_wndClsCursor.lpfnWndProc = CursorWndProc;
	g_wndClsCursor.hInstance = g_hInstance;
	g_wndClsCursor.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
	g_wndClsCursor.lpszClassName = L"KillAllWindowsCursor";

	if (!RegisterClass(&g_wndClsCursor))
		return 1;

	g_hKillCursor = LoadCursor(g_hInstance, MAKEINTRESOURCE(IDC_CURSOR1));

	while (GetMessage(&msg, NULL, 0, 0) > 0)
		DispatchMessage(&msg);

	ReleaseMutex(hMutex);
	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		RegisterHotKey(hWnd, 100, MOD_WIN | MOD_SHIFT, 'K');
		break;
	case WM_SETCURSOR:
		SetCursor(g_hKillCursor);
		break;
	case WM_CLOSE:
		PostQuitMessage(0);
		break;
	case WM_HOTKEY:
		if (g_hwndCursor == NULL) {
			g_hwndCursor = CreateWindowEx(
				WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
				g_wndClsCursor.lpszClassName,
				L"Kill All Windows Cursor",
				WS_POPUP | WS_VISIBLE,
				0, 0, 300, 300, 0, 0, g_hInstance, NULL);
		}
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

LRESULT CALLBACK CursorWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		SetTimer(hWnd, 1, 10, NULL);
		break;
	case WM_TIMER:
		POINT pt;
		GetCursorPos(&pt);
		SetWindowPos(hWnd, HWND_TOP, pt.x - 150, pt.y - 150, 300, 300, 0);
		break;
	case WM_SETCURSOR:
		SetCursor(g_hKillCursor);
		break;
	case WM_LBUTTONUP: 
		{
			ShowWindow(hWnd, SW_HIDE);
		 	KillWindowUnderCursor();
			ShowWindow(hWnd, SW_SHOW);
			bool ctrlHeld = GetKeyState(VK_CONTROL) & 0x8000;
			if (!ctrlHeld) {
				g_hwndCursor = NULL;
				DestroyWindow(hWnd);
			}
		}
		break;
	case WM_RBUTTONUP:
		DestroyWindow(hWnd);
		g_hwndCursor = NULL;
		break;
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) {
			DestroyWindow(hWnd);
			g_hwndCursor = NULL;
		}
		break;
	case WM_QUIT:
		KillTimer(hWnd, 1);
		break;
	case WM_KILLFOCUS:
		SetFocus(hWnd);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

void KillWindowUnderCursor() 
{
	POINT pt;
	GetCursorPos(&pt);
	HWND wnd = WindowFromPoint(pt);
	
	if (g_bDebugMode) {
		GetWindowText(wnd, g_wBuf, sizeof(g_wBuf) / sizeof(g_wBuf[0]));
		std::wcout << "Window Name: " << g_wBuf << std::endl;
	}

	if (wnd != NULL) {
		DWORD dwProcessId = 0;
		GetWindowThreadProcessId(wnd, &dwProcessId);
		HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, false, dwProcessId);

		if (processHandle) {
			bool explorer = false;
			if (GetModuleFileNameEx(processHandle, 0, g_wBuf, MAX_PATH)) {
				if (g_bDebugMode) {
					std::wcout << dwProcessId << L": " << g_wBuf << std::endl;
				}
				int explorerLen = lstrlen(g_wExplorerName);
				int procLen = lstrlen(g_wBuf);

				if (procLen >= explorerLen) {
					if (lstrcmpi(g_wBuf + (procLen - explorerLen), g_wExplorerName) == 0) {
						explorer = true;
						HWND root = GetAncestor(wnd, GA_ROOT);
						HWND desktop = GetDesktopWindow();
						GetWindowText(root, g_wBuf, sizeof(g_wBuf) / sizeof(g_wBuf[0]));

						if (g_bDebugMode) {
							std::wcout << "Top Level: " << g_wBuf << std::endl;
						}

						if (!g_bDebugMode && 
							root != desktop && 
							lstrlen(g_wBuf) > 0 &&
							lstrcmp(g_wBuf, g_wProgramManagerName) != 0) 
						{
							SendMessage(root, WM_CLOSE, 0, 0);
						}
					}
				}
			}
			if (!g_bDebugMode && !explorer) {
				TerminateProcess(processHandle, 1);
			}
			CloseHandle(processHandle);
		}
	}
}
