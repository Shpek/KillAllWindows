# KillAllWindows
XKill clone for Windows

## Building

Build with Visual Studio 2017. Tested on Windows 10.

## Usage

The program sets a global hotkey **winkey + shift + K**. Pressing this will change the cursor to a hand. Clicking on a window while in this mode will cause the process that created that window to be terminated. If you want to kill more than one window hold **Ctrl** when clicking. Press **ESC** or right click with the mouse to exit the kill mode. You can close the program from the Task Manager.

There's a special handling for explorer.exe - the program will never kill it, but it will close gracefully File Explorer windows.
