#include "pch.h"
#include "resource.h"

#define WM_TRAYICON            (WM_APP + 1)
#define WM_APP_KILL            (WM_APP + 2)
#define WM_APP_EXIT_KILLMODE   (WM_APP + 3)
#define WM_APP_HOTKEY_CAPTURED (WM_APP + 4)

enum KillBehavior {
	KILL_CLOSE_WINDOW,
	KILL_END_PROCESS
};

struct Options {
	bool bAskConfirmation;
	KillBehavior eDefaultBehavior;
	wchar_t wCursorPath[MAX_PATH];
	bool bHotkeyEnabled;
	UINT uHotkeyMods;
	UINT uHotkeyVk;
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK OptionsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void KillWindowUnderCursor(POINT pt, KillBehavior behavior);
bool ConfirmAction(const wchar_t *prompt, const wchar_t *target);
LRESULT CALLBACK MsgBoxCbtProc(int nCode, WPARAM wParam, LPARAM lParam);
void UpdateAltBehaviorLabel(HWND hDlg);
void AddTrayIcon(HWND hWnd);
void RemoveTrayIcon();
void ShowTrayMenu(HWND hWnd);
void EnterKillMode();
void ExitKillMode();
HCURSOR LoadKillCursor();
void UpdateCursorPreview(HWND hDlg);
void ApplyHotkey(HWND hWnd);
void HotkeyToText(UINT mods, UINT vk, wchar_t *buf, int cch);
LRESULT CALLBACK CaptureKeyHookProc(int nCode, WPARAM wParam, LPARAM lParam);
bool IsAutoStartEnabled();
void SetAutoStart(bool enable);
void LoadOptions();
void SaveOptions();

HINSTANCE g_hInstance = NULL;
HCURSOR g_hKillCursor = NULL;
HCURSOR g_hActiveKillCursor = NULL;
HHOOK g_hMouseHook = NULL;
HHOOK g_hKeyboardHook = NULL;
HHOOK g_hCaptureHook = NULL;
HHOOK g_hMsgBoxHook = NULL;
NOTIFYICONDATA g_nidTrayIcon = { 0 };
wchar_t g_wBuf[MAX_PATH];
HWND g_hwndMain = NULL;
HWND g_hwndOverlay = NULL;
HWND g_hwndOptions = NULL;
bool g_bDebugMode = false;
bool g_bConfirming = false;
bool g_bKillMode = false;
bool g_bMultiKill = false;
bool g_bAltKillBehavior = false;
POINT g_ptKill = { 0 };
HWND g_hwndCaptureEdit = NULL;
UINT g_uCaptureMods = 0;
UINT g_uCaptureVk = 0;
UINT g_uCaptureModMask = 0;
HCURSOR g_hPreviewCursor = NULL;
Options g_options = { false };
const wchar_t *g_wRegKey = L"Software\\FSC\\KillAllWindows";
const wchar_t *g_wRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t *g_wRunValue = L"KillAllWindows";
const wchar_t *g_wExplorerName = L"explorer.exe";
const wchar_t *g_wProgramManagerName = L"Program Manager";
const wchar_t *g_wOverlayClassName = L"KillAllWindowsOverlay";

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

	WNDCLASS overlayCls = { 0 };
	overlayCls.lpfnWndProc = OverlayWndProc;
	overlayCls.hInstance = g_hInstance;
	overlayCls.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	overlayCls.lpszClassName = g_wOverlayClassName;

	if (!RegisterClass(&overlayCls)) {
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
		ApplyHotkey(hWnd);
		AddTrayIcon(hWnd);
		break;
	case WM_TRAYICON:
		if (LOWORD(lParam) == WM_LBUTTONUP) {
			EnterKillMode();
		} else if (LOWORD(lParam) == WM_RBUTTONUP) {
			ShowTrayMenu(hWnd);
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDM_OPTIONS:
			if (g_hwndOptions) {
				SetForegroundWindow(g_hwndOptions);
			} else if (DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_OPTIONS), hWnd, OptionsDlgProc) == IDOK) {
				ApplyHotkey(hWnd);
			}
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
	{
		// Hide the overlay so WindowFromPoint finds the real target beneath it.
		ShowWindow(g_hwndOverlay, SW_HIDE);

		// Holding Alt inverts the configured default behavior.
		KillBehavior behavior = g_options.eDefaultBehavior;
		if (g_bAltKillBehavior) {
			behavior = (behavior == KILL_CLOSE_WINDOW) ? KILL_END_PROCESS : KILL_CLOSE_WINDOW;
		}
		KillWindowUnderCursor(g_ptKill, behavior);

		if (g_bMultiKill) {
			ShowWindow(g_hwndOverlay, SW_SHOWNOACTIVATE);
		} else {
			ExitKillMode();
		}
		break;
	}
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
			g_bAltKillBehavior = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
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

HCURSOR LoadKillCursor()
{
	if (g_options.wCursorPath[0] != L'\0') {
		HCURSOR hCursor = LoadCursorFromFile(g_options.wCursorPath);

		if (hCursor) {
			return hCursor;
		}
	}

	// No custom cursor (or it failed to load); fall back to the embedded one.
	return CopyCursor(g_hKillCursor);
}

// Show the cursor currently named in the path field (or the embedded one) in the
// preview box. HCURSOR and HICON are interchangeable, so the SS_ICON static draws it.
void UpdateCursorPreview(HWND hDlg)
{
	wchar_t path[MAX_PATH];
	GetDlgItemText(hDlg, IDC_CURSOR_PATH, path, _countof(path));

	HCURSOR hCursor = NULL;
	if (path[0] != L'\0') {
		hCursor = LoadCursorFromFile(path);
	}
	if (!hCursor) {
		hCursor = CopyCursor(g_hKillCursor);
	}

	SendDlgItemMessage(hDlg, IDC_CURSOR_PREVIEW, STM_SETIMAGE, IMAGE_ICON, (LPARAM)hCursor);

	if (g_hPreviewCursor) {
		DestroyCursor(g_hPreviewCursor);
	}
	g_hPreviewCursor = hCursor;
}

// The label shows what holding Alt will do - i.e. the opposite of the default.
void UpdateAltBehaviorLabel(HWND hDlg)
{
	bool defaultClose = IsDlgButtonChecked(hDlg, IDC_DEFAULT_CLOSE) == BST_CHECKED;
	SetDlgItemText(hDlg, IDC_ALT_BEHAVIOR_LABEL,
		defaultClose ? L"[Alt] key - End Process" : L"[Alt] key - Close Window");
}

void HotkeyToText(UINT mods, UINT vk, wchar_t *buf, int cch)
{
	if (vk == 0) {
		_snwprintf_s(buf, cch, _TRUNCATE, L"None");
		return;
	}

	wchar_t keyName[64] = L"";
	UINT scan = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
	LONG lp = (LONG)(scan << 16);

	switch (vk) {
	case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
	case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
	case VK_INSERT: case VK_DELETE: case VK_DIVIDE: case VK_NUMLOCK:
		lp |= (1 << 24);
		break;
	}

	if (GetKeyNameText(lp, keyName, _countof(keyName)) == 0) {
		_snwprintf_s(keyName, _countof(keyName), _TRUNCATE, L"0x%02X", vk);
	}

	_snwprintf_s(buf, cch, _TRUNCATE, L"%s%s%s%s%s",
		(mods & MOD_CONTROL) ? L"Ctrl + " : L"",
		(mods & MOD_ALT) ? L"Alt + " : L"",
		(mods & MOD_SHIFT) ? L"Shift + " : L"",
		(mods & MOD_WIN) ? L"Win + " : L"",
		keyName);
}

void ApplyHotkey(HWND hWnd)
{
	UnregisterHotKey(hWnd, 100);

	if (g_options.bHotkeyEnabled && g_options.uHotkeyVk != 0) {
		if (!RegisterHotKey(hWnd, 100, g_options.uHotkeyMods | MOD_NOREPEAT, g_options.uHotkeyVk)) {
			MessageBox(hWnd,
				L"The selected shortcut is already in use by another application "
				L"and could not be registered.",
				L"Kill All Windows", MB_OK | MB_ICONWARNING);
		}
	}
}

// Low-level keyboard hook active only while the shortcut field has focus. It
// records the pressed chord (tracking the modifier mask itself so Win-key combos
// work) and swallows every key so nothing else - Start menu, Win+X, etc. - fires.
LRESULT CALLBACK CaptureKeyHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION) {
		KBDLLHOOKSTRUCT *pKey = (KBDLLHOOKSTRUCT *)lParam;
		UINT vk = pKey->vkCode;
		bool down = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);

		UINT bit = 0;
		switch (vk) {
		case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
			bit = MOD_CONTROL;
			break;
		case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
			bit = MOD_SHIFT;
			break;
		case VK_MENU: case VK_LMENU: case VK_RMENU:
			bit = MOD_ALT;
			break;
		case VK_LWIN: case VK_RWIN:
			bit = MOD_WIN;
			break;
		}

		if (bit) {
			if (down) {
				g_uCaptureModMask |= bit;
			} else {
				g_uCaptureModMask &= ~bit;
			}
		} else if (down) {
			// Let the dialog validate + display it (a hook must not block on a dialog).
			PostMessage(GetParent(g_hwndCaptureEdit), WM_APP_HOTKEY_CAPTURED, g_uCaptureModMask, vk);
		}

		return 1;
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_SETCURSOR:
		SetCursor(g_hActiveKillCursor);
		return TRUE;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
}

void EnterKillMode()
{
	if (g_bKillMode) {
		return;
	}

	g_bKillMode = true;
	g_hActiveKillCursor = LoadKillCursor();

	// A full-virtual-screen, topmost, layered overlay that owns the cursor, so the
	// kill cursor shows over every window - even apps that set their own cursor.
	int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
	int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
	int cx = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	int cy = GetSystemMetrics(SM_CYVIRTUALSCREEN);

	g_hwndOverlay = CreateWindowEx(
		WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
		g_wOverlayClassName, L"", WS_POPUP,
		x, y, cx, cy, NULL, NULL, g_hInstance, NULL);

	// Nearly transparent but not click-through, so it still owns the cursor.
	SetLayeredWindowAttributes(g_hwndOverlay, 0, 1, LWA_ALPHA);
	ShowWindow(g_hwndOverlay, SW_SHOWNOACTIVATE);

	g_hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, g_hInstance, 0);
	g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, g_hInstance, 0);
}

void ExitKillMode()
{
	if (!g_bKillMode) {
		return;
	}

	g_bKillMode = false;

	if (g_hMouseHook) {
		UnhookWindowsHookEx(g_hMouseHook);
		g_hMouseHook = NULL;
	}

	if (g_hKeyboardHook) {
		UnhookWindowsHookEx(g_hKeyboardHook);
		g_hKeyboardHook = NULL;
	}

	if (g_hwndOverlay) {
		DestroyWindow(g_hwndOverlay);
		g_hwndOverlay = NULL;
	}

	if (g_hActiveKillCursor) {
		DestroyCursor(g_hActiveKillCursor);
		g_hActiveKillCursor = NULL;
	}
}

// CBT hook that positions the confirmation box near the click (centered on
// g_ptKill), clamped to the monitor under the click. MessageBox has no position arg.
LRESULT CALLBACK MsgBoxCbtProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HCBT_ACTIVATE) {
		HWND hMsgBox = (HWND)wParam;

		RECT rc;
		GetWindowRect(hMsgBox, &rc);
		int w = rc.right - rc.left;
		int h = rc.bottom - rc.top;
		int x = g_ptKill.x - w / 2;
		int y = g_ptKill.y - h / 2;

		MONITORINFO mi = { sizeof(mi) };
		if (GetMonitorInfo(MonitorFromPoint(g_ptKill, MONITOR_DEFAULTTONEAREST), &mi)) {
			if (x < mi.rcWork.left) x = mi.rcWork.left;
			if (y < mi.rcWork.top) y = mi.rcWork.top;
			if (x + w > mi.rcWork.right) x = mi.rcWork.right - w;
			if (y + h > mi.rcWork.bottom) y = mi.rcWork.bottom - h;
		}

		SetWindowPos(hMsgBox, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

		// Replace the standard icon with the app icon (0x0014 = the MessageBox icon control).
		HWND hIconCtrl = GetDlgItem(hMsgBox, 0x0014);
		if (hIconCtrl) {
			SendMessage(hIconCtrl, STM_SETIMAGE, IMAGE_ICON,
				(LPARAM)LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON1)));
		}

		// Only position/skin once, when the box first opens (not on later re-activation).
		if (g_hMsgBoxHook) {
			UnhookWindowsHookEx(g_hMsgBoxHook);
			g_hMsgBoxHook = NULL;
		}
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// Returns true to proceed. Respects the "Ask for confirmation" option; when off,
// always proceeds. g_bConfirming lets the input hooks pass through to the dialog.
bool ConfirmAction(const wchar_t *prompt, const wchar_t *target)
{
	if (!g_options.bAskConfirmation) {
		return true;
	}

	wchar_t msg[MAX_PATH + 64];
	_snwprintf_s(msg, _countof(msg), _TRUNCATE, L"%s\n\n%s", prompt, target);

	g_bConfirming = true;

	// The hook removes itself once it has positioned the box (see MsgBoxCbtProc).
	g_hMsgBoxHook = SetWindowsHookEx(WH_CBT, MsgBoxCbtProc, NULL, GetCurrentThreadId());
	bool bConfirmed = MessageBox(NULL, msg, L"Kill All Windows",
		MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND) == IDYES;

	g_bConfirming = false;

	return bConfirmed;
}

void KillWindowUnderCursor(POINT pt, KillBehavior behavior)
{
	HWND wnd = WindowFromPoint(pt);

	if (g_bDebugMode) {
		GetWindowText(wnd, g_wBuf, sizeof(g_wBuf) / sizeof(g_wBuf[0]));
		std::wcout << "Window Name: " << g_wBuf << std::endl;
	}

	if (wnd != NULL) {
		DWORD dwProcessId = 0;
		GetWindowThreadProcessId(wnd, &dwProcessId);

		// Never act on ourselves (e.g. clicking our own Options/About dialog).
		if (dwProcessId == GetCurrentProcessId()) {
			return;
		}

		if (behavior == KILL_CLOSE_WINDOW) {
			// Graceful close: ask the top-level window to close, leaving the shell alone.
			HWND root = GetAncestor(wnd, GA_ROOT);
			HWND desktop = GetDesktopWindow();
			GetWindowText(root, g_wBuf, sizeof(g_wBuf) / sizeof(g_wBuf[0]));

			if (root != desktop && lstrcmp(g_wBuf, g_wProgramManagerName) != 0 &&
				ConfirmAction(L"Close this window?", g_wBuf)) {
				SendMessage(root, WM_CLOSE, 0, 0);
			}
			return;
		}

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
							lstrcmp(g_wBuf, g_wProgramManagerName) != 0 &&
							ConfirmAction(L"Close this window?", g_wBuf))
						{
							SendMessage(root, WM_CLOSE, 0, 0);
						}
					}
				}
			}
			
			if (!g_bDebugMode && !explorer) {
				if (ConfirmAction(L"Kill this process?", g_wBuf)) {
					TerminateProcess(processHandle, 1);
				}
			}
			CloseHandle(processHandle);
		}
	}
}

void AddTrayIcon(HWND hWnd)
{
	g_nidTrayIcon = { 0 };
	g_nidTrayIcon.cbSize = sizeof(g_nidTrayIcon);
	g_nidTrayIcon.hWnd = hWnd;
	g_nidTrayIcon.uID = 1;
	g_nidTrayIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	g_nidTrayIcon.uCallbackMessage = WM_TRAYICON;
	g_nidTrayIcon.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON1));
	lstrcpy(g_nidTrayIcon.szTip, L"Kill All Windows");
	Shell_NotifyIcon(NIM_ADD, &g_nidTrayIcon);
}

void RemoveTrayIcon()
{
	Shell_NotifyIcon(NIM_DELETE, &g_nidTrayIcon);
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
	{
		g_hwndOptions = hDlg;

		HICON hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON1));
		SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
		SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

		CheckDlgButton(hDlg, IDC_ASK_CONFIRM, g_options.bAskConfirmation ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_START_WITH_WINDOWS, IsAutoStartEnabled() ? BST_CHECKED : BST_UNCHECKED);

		CheckRadioButton(hDlg, IDC_DEFAULT_CLOSE, IDC_DEFAULT_END,
			g_options.eDefaultBehavior == KILL_CLOSE_WINDOW ? IDC_DEFAULT_CLOSE : IDC_DEFAULT_END);
		UpdateAltBehaviorLabel(hDlg);

		SetDlgItemText(hDlg, IDC_CURSOR_PATH, g_options.wCursorPath);
		UpdateCursorPreview(hDlg);

		CheckDlgButton(hDlg, IDC_HOTKEY_ENABLE, g_options.bHotkeyEnabled ? BST_CHECKED : BST_UNCHECKED);
		g_uCaptureMods = g_options.uHotkeyMods;
		g_uCaptureVk = g_options.uHotkeyVk;

		wchar_t buf[128];
		HotkeyToText(g_uCaptureMods, g_uCaptureVk, buf, _countof(buf));
		SetDlgItemText(hDlg, IDC_HOTKEY_KEY, buf);
		EnableWindow(GetDlgItem(hDlg, IDC_HOTKEY_KEY), g_options.bHotkeyEnabled);
		return (INT_PTR)TRUE;
	}
	case WM_APP_HOTKEY_CAPTURED:
	{
		UINT mods = (UINT)wParam;
		UINT vk = (UINT)lParam;

		// Available if it's the shortcut we already own, or if a trial registration succeeds.
		bool available = g_options.bHotkeyEnabled &&
			mods == g_options.uHotkeyMods && vk == g_options.uHotkeyVk;

		if (!available && RegisterHotKey(hDlg, 0x100, mods | MOD_NOREPEAT, vk)) {
			UnregisterHotKey(hDlg, 0x100);
			available = true;
		}

		if (available) {
			g_uCaptureMods = mods;
			g_uCaptureVk = vk;
		} else {
			// Reject the change and fall back to the default shortcut.
			g_uCaptureMods = MOD_WIN | MOD_SHIFT;
			g_uCaptureVk = 'K';
			MessageBox(hDlg,
				L"That shortcut is already in use by another application. "
				L"Reverting to the default (Win + Shift + K).",
				L"Kill All Windows", MB_OK | MB_ICONWARNING);
		}

		wchar_t buf[128];
		HotkeyToText(g_uCaptureMods, g_uCaptureVk, buf, _countof(buf));
		SetDlgItemText(hDlg, IDC_HOTKEY_KEY, buf);
		return (INT_PTR)TRUE;
	}
	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_DEFAULT_CLOSE || LOWORD(wParam) == IDC_DEFAULT_END) {
			UpdateAltBehaviorLabel(hDlg);
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDC_HOTKEY_ENABLE) {
			BOOL enabled = IsDlgButtonChecked(hDlg, IDC_HOTKEY_ENABLE) == BST_CHECKED;
			EnableWindow(GetDlgItem(hDlg, IDC_HOTKEY_KEY), enabled);
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDC_HOTKEY_KEY && HIWORD(wParam) == EN_SETFOCUS) {
			g_hwndCaptureEdit = GetDlgItem(hDlg, IDC_HOTKEY_KEY);
			g_uCaptureModMask = 0;
			if (!g_hCaptureHook) {
				g_hCaptureHook = SetWindowsHookEx(WH_KEYBOARD_LL, CaptureKeyHookProc, g_hInstance, 0);
			}
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDC_HOTKEY_KEY && HIWORD(wParam) == EN_KILLFOCUS) {
			if (g_hCaptureHook) {
				UnhookWindowsHookEx(g_hCaptureHook);
				g_hCaptureHook = NULL;
			}
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDC_CURSOR_BROWSE) {
			wchar_t file[MAX_PATH] = L"";
			GetDlgItemText(hDlg, IDC_CURSOR_PATH, file, _countof(file));

			OPENFILENAME ofn = { 0 };
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hDlg;
			ofn.lpstrFilter = L"Cursor files (*.ani;*.cur)\0*.ani;*.cur\0All files (*.*)\0*.*\0";
			ofn.lpstrFile = file;
			ofn.nMaxFile = _countof(file);
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

			if (GetOpenFileName(&ofn)) {
				SetDlgItemText(hDlg, IDC_CURSOR_PATH, file);
				UpdateCursorPreview(hDlg);
			}
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDC_CURSOR_DEFAULT) {
			SetDlgItemText(hDlg, IDC_CURSOR_PATH, L"");
			UpdateCursorPreview(hDlg);
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDOK) {
			g_options.bAskConfirmation = IsDlgButtonChecked(hDlg, IDC_ASK_CONFIRM) == BST_CHECKED;
			g_options.eDefaultBehavior = IsDlgButtonChecked(hDlg, IDC_DEFAULT_CLOSE) == BST_CHECKED
				? KILL_CLOSE_WINDOW : KILL_END_PROCESS;
			SetAutoStart(IsDlgButtonChecked(hDlg, IDC_START_WITH_WINDOWS) == BST_CHECKED);
			GetDlgItemText(hDlg, IDC_CURSOR_PATH, g_options.wCursorPath, _countof(g_options.wCursorPath));
			g_options.bHotkeyEnabled = IsDlgButtonChecked(hDlg, IDC_HOTKEY_ENABLE) == BST_CHECKED;
			g_options.uHotkeyMods = g_uCaptureMods;
			g_options.uHotkeyVk = g_uCaptureVk;
			SaveOptions();
			EndDialog(hDlg, IDOK);
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDCANCEL) {
			EndDialog(hDlg, IDCANCEL);
			return (INT_PTR)TRUE;
		}
		break;
	case WM_DESTROY:
		g_hwndOptions = NULL;
		if (g_hCaptureHook) {
			UnhookWindowsHookEx(g_hCaptureHook);
			g_hCaptureHook = NULL;
		}
		if (g_hPreviewCursor) {
			DestroyCursor(g_hPreviewCursor);
			g_hPreviewCursor = NULL;
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
	g_options.eDefaultBehavior = KILL_END_PROCESS;
	g_options.wCursorPath[0] = L'\0';
	g_options.bHotkeyEnabled = true;
	g_options.uHotkeyMods = MOD_WIN | MOD_SHIFT;
	g_options.uHotkeyVk = 'K';

	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, g_wRegKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		DWORD dwValue = 0;
		DWORD dwSize = sizeof(dwValue);
		DWORD dwType = 0;
		if (RegQueryValueEx(hKey, L"AskConfirmation", NULL, &dwType, (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS &&
			dwType == REG_DWORD) {
			g_options.bAskConfirmation = dwValue != 0;
		}

		dwSize = sizeof(dwValue);
		if (RegQueryValueEx(hKey, L"DefaultBehavior", NULL, &dwType, (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS &&
			dwType == REG_DWORD) {
			g_options.eDefaultBehavior = dwValue == KILL_CLOSE_WINDOW ? KILL_CLOSE_WINDOW : KILL_END_PROCESS;
		}

		DWORD dwPathSize = sizeof(g_options.wCursorPath);
		if (RegQueryValueEx(hKey, L"CursorPath", NULL, &dwType, (LPBYTE)g_options.wCursorPath, &dwPathSize) == ERROR_SUCCESS &&
			dwType == REG_SZ) {
			// RegQueryValueEx does not guarantee termination; enforce it.
			g_options.wCursorPath[_countof(g_options.wCursorPath) - 1] = L'\0';
		} else {
			g_options.wCursorPath[0] = L'\0';
		}

		dwSize = sizeof(dwValue);
		if (RegQueryValueEx(hKey, L"HotkeyEnabled", NULL, &dwType, (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS &&
			dwType == REG_DWORD) {
			g_options.bHotkeyEnabled = dwValue != 0;
		}

		dwSize = sizeof(dwValue);
		if (RegQueryValueEx(hKey, L"HotkeyMods", NULL, &dwType, (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS &&
			dwType == REG_DWORD) {
			g_options.uHotkeyMods = dwValue;
		}

		dwSize = sizeof(dwValue);
		if (RegQueryValueEx(hKey, L"HotkeyVk", NULL, &dwType, (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS &&
			dwType == REG_DWORD) {
			g_options.uHotkeyVk = dwValue;
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
		dwValue = (DWORD)g_options.eDefaultBehavior;
		RegSetValueEx(hKey, L"DefaultBehavior", 0, REG_DWORD, (const BYTE*)&dwValue, sizeof(dwValue));
		RegSetValueEx(hKey, L"CursorPath", 0, REG_SZ, (const BYTE*)g_options.wCursorPath,
			(lstrlen(g_options.wCursorPath) + 1) * sizeof(wchar_t));

		dwValue = g_options.bHotkeyEnabled ? 1 : 0;
		RegSetValueEx(hKey, L"HotkeyEnabled", 0, REG_DWORD, (const BYTE*)&dwValue, sizeof(dwValue));
		dwValue = g_options.uHotkeyMods;
		RegSetValueEx(hKey, L"HotkeyMods", 0, REG_DWORD, (const BYTE*)&dwValue, sizeof(dwValue));
		dwValue = g_options.uHotkeyVk;
		RegSetValueEx(hKey, L"HotkeyVk", 0, REG_DWORD, (const BYTE*)&dwValue, sizeof(dwValue));

		RegCloseKey(hKey);
	}
}

bool IsAutoStartEnabled()
{
	bool enabled = false;

	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, g_wRunKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		enabled = RegQueryValueEx(hKey, g_wRunValue, NULL, NULL, NULL, NULL) == ERROR_SUCCESS;
		RegCloseKey(hKey);
	}

	return enabled;
}

void SetAutoStart(bool enable)
{
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, g_wRunKey, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
		if (enable) {
			wchar_t path[MAX_PATH];
			DWORD len = GetModuleFileName(NULL, path, _countof(path));
			if (len > 0 && len < _countof(path)) {
				wchar_t quoted[MAX_PATH + 2];
				_snwprintf_s(quoted, _countof(quoted), _TRUNCATE, L"\"%s\"", path);
				RegSetValueEx(hKey, g_wRunValue, 0, REG_SZ, (const BYTE*)quoted,
					(lstrlen(quoted) + 1) * sizeof(wchar_t));
			}
		} else {
			RegDeleteValue(hKey, g_wRunValue);
		}
		RegCloseKey(hKey);
	}
}
