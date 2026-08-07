# KillAllWindows — working notes

Win32 C++ tray app (XKill clone). Single-instance, global hotkey (Win+Shift+K),
low-level mouse/keyboard hooks + `SetSystemCursor` to pick and kill the window
under the cursor. Tray menu: Options / About / Exit.

## Coding conventions

- Match the existing codebase style rather than defaults.
- Names use Hungarian-style prefixes: `g_` for globals, `b` bool, `h` handle,
  `w` wide string, `dw` DWORD (e.g. `g_bAskConfirmation`, struct member
  `bAskConfirmation`).
- For wide-string formatting use the counted CRT function
  `_snwprintf_s(buf, _countof(buf), _TRUNCATE, ...)`. Do **not** use `wsprintf`,
  and do not reach for `<strsafe.h>`/`StringCchPrintf` unless asked.

## Building

- MSBuild, VS2022 toolset v143. All four configs build (Debug/Release x64/x86).
  Solution platform labels are `x64` and `x86` (x86 maps to project `Win32`).

  ```
  & "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" \
    "D:\Projects\KillAllWindows\KillAllWindows.sln" /t:Rebuild /p:Configuration=Release /p:Platform=x64
  ```

- **Always terminate any running instance before rebuilding** — the app is
  tray-only and locks the output exe (`LNK1104: cannot open file ...exe`). Do it
  automatically, no need to ask:

  ```
  Get-Process KillAllWindows -ErrorAction SilentlyContinue | Stop-Process -Force
  ```

## Git

- Keep commit commands minimal: `git add <files> && git commit -m "..."`. Do not
  append `git log`/`git status`/`git diff` afterward, and skip the pre-commit
  status check when the changed files are already known.
- Commit with the repo's default identity. Do not pass `--author` or set
  `GIT_COMMITTER_*` unless explicitly asked.

## Working style

- Confirm each file change; don't batch edits without review. (Permission
  `defaultMode` is set to `default` in `.claude/settings.json` for this repo.)

## Project specifics

- Options persist in the registry under `HKCU\Software\FSC\KillAllWindows`.
- Copyright holder is "Shpek" (MIT, 2019).
