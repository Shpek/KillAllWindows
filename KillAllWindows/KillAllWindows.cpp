#include "pch.h"
#include "resource.h"

#define WM_TRAYICON          (WM_APP + 1)
#define WM_APP_KILL          (WM_APP + 2)
#define WM_APP_EXIT_KILLMODE (WM_APP + 3)

struct Options {
	bool bAskConfirmation;
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK OptionsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void KillWindowUnderCursor(POINT pt);
void AddTrayIcon(HWND hWnd);
void RemoveTrayIcon();
void ShowTrayMenu(HWND hWnd);
void EnterKillMode();
void ExitKillMode();
void LoadOptions();
void SaveOptions();

HINSTANCE g_hInstance = NULL;
HCURSOR g_hKillCursor = NULL;
HHOOK g_hMouseHook = NULL;
HHOOK g_hKeyboardHook = NULL;
NOTIFYICONDATA g_nid = { 0 };
wchar_t g_wBuf[MAX_PATH];
HWND g_hwndMain = NULL;
bool g_bDebugMode = false;
bool g_bConfirming = false;
bool g_bKillMode = false;
bool g_bMultiKill = false;
POINT g_ptKill = { 0 };
Options g_options = { false };
const wchar_t *g_wRegKey = L"Software\\FSC\\KillAllWindows";
const wchar_t *g_wExplorerName = L"explorer.exe";
const wchar_t *g_wProgramManagerName = L"Program Manager";
const int g_ocrCursors[] = {
	OCR_NORMAL, OCR_IBEAM, OCR_WAIT, OCR_CROSS, OCR_UP,
	OCR_SIZENWSE, OCR_SIZENESW, OCR_SIZEWE, OCR_SIZENS, OCR_SIZEALL,
	OCR_NO, OCR_HAND, OCR_APPSTARTING
};

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

	LoadOptions();

	g_hInstance = GetModuleHandle(NULL);
	g_hKillCursor = LoadCursor(g_hInstance, MAKEINTRESOURCE(IDC_CURSOR1));
	MSG msg = { 0 };
	WNDCLASS mainWindowCls = { 0 };
	mainWindowCls.lpfnWndProc = WndProc;
	mainWindowCls.hInstance = g_hInstance;
	mainWindowCls.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
	mainWindowCls.lpszClassName = L"KillAllWindows";

	if (!RegisterClass(&mainWindowCls)) {
		return 1;
	}

	g_hwndMain = CreateWindow(mainWindowCls.lpszClassName,
		L"Kill All Windows",
		WS_POPUP,
		0, 0, 100, 100, NULL, 0, g_hInstance, NULL);

	if (!g_hwndMain) {
		return 2;
	}

	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		DispatchMessage(&msg);
	}

	ReleaseMutex(hMutex);
	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		RegisterHotKey(hWnd, 100, MOD_WIN | MOD_SHIFT, 'K');
		AddTrayIcon(hWnd);
		break;
	case WM_TRAYICON:
		if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_LBUTTONUP) {
			ShowTrayMenu(hWnd);
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDM_OPTIONS:
			DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_OPTIONS), hWnd, OptionsDlgProc);
			break;
		case IDM_ABOUT:
			DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_ABOUT), hWnd, AboutDlgProc);
			break;
		case IDM_EXIT:
			if (MessageBox(hWnd, L"Close Kill All Windows?", L"Kill All Windows",
				MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
				DestroyWindow(hWnd);
			}
			break;
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		ExitKillMode();
		RemoveTrayIcon();
		PostQuitMessage(0);
		break;
	case WM_HOTKEY:
		EnterKillMode();
		break;
	case WM_APP_KILL:
		KillWindowUnderCursor(g_ptKill);

		if (!g_bMultiKill) {
			ExitKillMode();
		}
		break;
	case WM_APP_EXIT_KILLMODE:
		ExitKillMode();
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION && !g_bConfirming) {
		MSLLHOOKSTRUCT *pMouse = (MSLLHOOKSTRUCT *)lParam;
		switch (wParam) {
		case WM_LBUTTONDOWN:
			return 1;
		case WM_LBUTTONUP:
			g_ptKill = pMouse->pt;
			g_bMultiKill = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
			PostMessage(g_hwndMain, WM_APP_KILL, 0, 0);
			return 1;
		case WM_RBUTTONDOWN:
			return 1;
		case WM_RBUTTONUP:
			PostMessage(g_hwndMain, WM_APP_EXIT_KILLMODE, 0, 0);
			return 1;
		}
	}
	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION && !g_bConfirming) {
		KBDLLHOOKSTRUCT *pKey = (KBDLLHOOKSTRUCT *)lParam;
		
		if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && pKey->vkCode == VK_ESCAPE) {
			PostMessage(g_hwndMain, WM_APP_EXIT_KILLMODE, 0, 0);
			return 1;
		}
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void EnterKillMode()
{
	if (g_bKillMode) {
		return;
	}

	g_bKillMode = true;

	for (size_t i = 0; i < _countof(g_ocrCursors); i++) {
		SetSystemCursor(CopyCursor(g_hKillCursor), g_ocrCursors[i]);
	}

	g_hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, g_hInstance, 0);
	g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, g_hInstance, 0);
}

void ExitKillMode()
{
	if (!g_bKillMode)
		return;
	g_bKillMode = false;

	if (g_hMouseHook) {
		UnhookWindowsHookEx(g_hMouseHook);
		g_hMouseHook = NULL;
	}
	if (g_hKeyboardHook) {
		UnhookWindowsHookEx(g_hKeyboardHook);
		g_hKeyboardHook = NULL;
	}

	// Reload the default system cursors, undoing the SetSystemCursor swaps.
	SystemParametersInfo(SPI_SETCURSORS, 0, NULL, SPIF_SENDCHANGE);
}

void KillWindowUnderCursor(POINT pt)
{
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
				bool confirmed = true;
				if (g_options.bAskConfirmation) {
					wchar_t msg[MAX_PATH + 64];
					_snwprintf_s(msg, _countof(msg), _TRUNCATE, L"Kill this process?\n\n%s", g_wBuf);
					g_bConfirming = true;
					confirmed = MessageBox(NULL, msg, L"Kill All Windows",
						MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND) == IDYES;
					g_bConfirming = false;
				}
				if (confirmed) {
					TerminateProcess(processHandle, 1);
				}
			}
			CloseHandle(processHandle);
		}
	}
}

void AddTrayIcon(HWND hWnd)
{
	g_nid = { 0 };
	g_nid.cbSize = sizeof(g_nid);
	g_nid.hWnd = hWnd;
	g_nid.uID = 1;
	g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	g_nid.uCallbackMessage = WM_TRAYICON;
	g_nid.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON1));
	lstrcpy(g_nid.szTip, L"Kill All Windows");
	Shell_NotifyIcon(NIM_ADD, &g_nid);
}

void RemoveTrayIcon()
{
	Shell_NotifyIcon(NIM_DELETE, &g_nid);
}

void ShowTrayMenu(HWND hWnd)
{
	POINT pt;
	GetCursorPos(&pt);

	HMENU hMenu = CreatePopupMenu();
	AppendMenu(hMenu, MF_STRING, IDM_OPTIONS, L"Options");
	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
	AppendMenu(hMenu, MF_STRING, IDM_ABOUT, L"About");
	AppendMenu(hMenu, MF_STRING, IDM_EXIT, L"Exit");

	// Required so the menu dismisses correctly when the user clicks elsewhere.
	SetForegroundWindow(hWnd);
	TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
	PostMessage(hWnd, WM_NULL, 0, 0);
	DestroyMenu(hMenu);
}

INT_PTR CALLBACK OptionsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		CheckDlgButton(hDlg, IDC_ASK_CONFIRM, g_options.bAskConfirmation ? BST_CHECKED : BST_UNCHECKED);
		return (INT_PTR)TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK) {
			g_options.bAskConfirmation = IsDlgButtonChecked(hDlg, IDC_ASK_CONFIRM) == BST_CHECKED;
			SaveOptions();
			EndDialog(hDlg, IDOK);
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDCANCEL) {
			EndDialog(hDlg, IDCANCEL);
			return (INT_PTR)TRUE;
		}
		break;
	case WM_CLOSE:
		EndDialog(hDlg, IDCANCEL);
		return (INT_PTR)TRUE;
	}
	return (INT_PTR)FALSE;
}

INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	case WM_CLOSE:
		EndDialog(hDlg, IDCANCEL);
		return (INT_PTR)TRUE;
	}
	return (INT_PTR)FALSE;
}

void LoadOptions()
{
	g_options.bAskConfirmation = false;

	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, g_wRegKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		DWORD dwValue = 0;
		DWORD dwSize = sizeof(dwValue);
		DWORD dwType = 0;
		if (RegQueryValueEx(hKey, L"AskConfirmation", NULL, &dwType, (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS &&
			dwType == REG_DWORD) {
			g_options.bAskConfirmation = dwValue != 0;
		}
		RegCloseKey(hKey);
	}
}

void SaveOptions()
{
	HKEY hKey;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, g_wRegKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
		DWORD dwValue = g_options.bAskConfirmation ? 1 : 0;
		RegSetValueEx(hKey, L"AskConfirmation", 0, REG_DWORD, (const BYTE*)&dwValue, sizeof(dwValue));
		RegCloseKey(hKey);
	}
}
