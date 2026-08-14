# KillAllWindows — working notes

Win32 C++ tray app (XKill clone). Single instance (named mutex), a hidden main
window, and a tray icon. Enter "kill mode" via the configurable global hotkey
(default Win+Shift+K) or by left-clicking the tray icon; right-click the tray for
the menu (Options / About / Exit).

## Architecture

- **Kill mode** shows the kill cursor over *everything* using a full-virtual-screen,
  topmost, layered, `WS_EX_NOACTIVATE` overlay window that owns the cursor via
  `WM_SETCURSOR`. (Earlier approaches — a 300x300 hidden tracking window, then
  `SetSystemCursor` — were dropped; the overlay is the current design.)
- Input in kill mode comes from low-level hooks (`WH_MOUSE_LL` + `WH_KEYBOARD_LL`),
  not the overlay: left-click acts, Ctrl = stay in kill mode (multi), Esc /
  right-click exit. The overlay is briefly hidden so `WindowFromPoint` finds the
  real target underneath.
- **Click behavior** is an enum `KillBehavior { KILL_CLOSE_WINDOW, KILL_END_PROCESS }`.
  Holding **Alt** at click-time inverts the configured default. Close = `WM_CLOSE`
  to the top-level window; End = `TerminateProcess`. explorer.exe is never
  terminated (its window is closed gracefully), the shell/desktop is left alone,
  and the app never acts on its own process.
- **Confirmation** (`ConfirmAction`) covers every destructive path when
  "Ask for confirmation" is on. The box is positioned at the click and shows the
  app icon, via a one-shot `WH_CBT` hook.
- **Options** persist in the registry under `HKCU\Software\FSC\KillAllWindows`
  (AskConfirmation, DefaultBehavior, CursorPath, HotkeyEnabled, HotkeyMods,
  HotkeyVk). "Start with Windows" is the `...\CurrentVersion\Run` value
  `KillAllWindows` — that key is the source of truth, not duplicated in ours.
- Custom kill cursor: a user-picked `.ani`/`.cur` loaded with `LoadCursorFromFile`
  (animated cursors animate); falls back to the embedded Undead cursor.

## Coding conventions

- Match the existing codebase style rather than defaults.
- Hungarian-style prefixes: `g_` globals, `b` bool, `e` enum, `h` handle,
  `w` wide string, `dw` DWORD, `u` UINT.
- Model multi-mode state with a named `enum`, not a `bool`.
- Wide-string formatting: counted CRT `_snwprintf_s(buf, _countof(buf), _TRUNCATE, ...)`.
  Not `wsprintf`, and no `<strsafe.h>`/`StringCchPrintf` unless asked.
- `KillAllWindows.rc` is UTF-16 — preserve that encoding when editing it. Git
  treats it as binary, so its changes don't appear in line diffs.

## Building

- MSBuild, VS2022 toolset v143. All four configs build (Debug/Release × x64/x86);
  the solution platform labels are `x64` and `x86` (x86 maps to project `Win32`).
- **Always terminate any running instance before rebuilding** — the app is
  tray-only and locks the output exe (`LNK1104: cannot open file ...exe`):

  ```
  Get-Process KillAllWindows -ErrorAction SilentlyContinue | Stop-Process -Force
  ```

## Git

- Keep commit commands minimal: `git add <files> && git commit -m "..."`. No
  trailing `git log`/`git status`/`git diff`, and skip the pre-commit status check
  when the changed files are already known.
- Commit with the repo's default identity. Do not pass `--author` or set
  `GIT_COMMITTER_*` unless explicitly asked.

## Working style

- Do not assume user intent. Always ask if unsure.
- Confirm each file change; don't batch edits without review.
