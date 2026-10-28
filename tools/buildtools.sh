#!/bin/bash
# Build the user-mode tools (console, GUI, installer): i386 for NT 3.1 - XP
# (build/tools), x64 for XP x64 and later (build/tools64; "$0 x64").
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ARCH="${1:-x86}"
if [ "$ARCH" = "x64" ]; then
    OUT="$ROOT/build/tools64"
    CC="clang --target=x86_64-pc-windows-msvc -ffreestanding -fno-stack-protector -fno-builtin -O1"
    SUB="5.02"; E() { echo "$1"; }
else
    OUT="$ROOT/build/tools"
    CC="clang --target=i686-pc-windows-msvc -march=i486 -ffreestanding -fno-stack-protector -fno-builtin -O1"
    SUB="3.10"; E() { echo "$1@$2"; }
fi
mkdir -p "$OUT"
cd "$OUT"

deflib() {
    local dll="$1"; shift
    local base="${dll%.dll}" s
    { echo "LIBRARY $dll"; echo "EXPORTS"
      for s in "$@"; do if [ "$ARCH" = "x64" ]; then echo "${s%@*}"; else echo "$s"; fi; done; } > "$base.def"
    if [ "$ARCH" = "x64" ]; then llvm-dlltool -m i386:x86-64 -d "$base.def" -l "$base.lib"
    else llvm-dlltool -m i386 --kill-at -d "$base.def" -l "$base.lib"; fi
}

deflib kernel32.dll ExitProcess@4 GetStdHandle@4 WriteFile@20 GetCommandLineA@0 \
    CreateFileA@28 DeviceIoControl@32 CloseHandle@4 GetModuleHandleA@4 GetLastError@0 \
    GetModuleFileNameA@12 GetWindowsDirectoryA@8 CopyFileA@12 GetLogicalDrives@0 GetVersion@0 GetProcAddress@8 \
    DeleteFileA@4 MoveFileExA@12
deflib advapi32.dll RegCloseKey@4 RegCreateKeyExA@36 RegSetValueExA@24 RegOpenKeyExA@20 RegEnumKeyA@16 RegDeleteKeyA@8
deflib user32.dll RegisterClassA@4 CreateWindowExA@48 DefWindowProcA@16 \
    ShowWindow@8 UpdateWindow@4 GetMessageA@16 TranslateMessage@4 DispatchMessageA@4 \
    PostQuitMessage@4 SendMessageA@16 LoadCursorA@8 MessageBoxA@16 \
    BeginPaint@8 EndPaint@8 GetClientRect@8 InvalidateRect@12 FillRect@12 FrameRect@12 \
    MoveWindow@24 GetWindowTextA@12 SetWindowTextA@8 GetDlgItem@8 IsDialogMessageA@8 wsprintfA \
    PostMessageA@16 SetCursor@4 GetWindowRect@8 AdjustWindowRectEx@16 EnableWindow@8 \
    DestroyWindow@4 SetFocus@4 SetCapture@4 ReleaseCapture@0 CreatePopupMenu@0 \
    AppendMenuA@16 TrackPopupMenu@28 DestroyMenu@4 ClientToScreen@8 GetSystemMetrics@4 \
    SetTimer@16 KillTimer@8 LoadIconA@8 DdeInitializeA@16 DdeCreateStringHandleA@12 \
    DdeFreeStringHandle@8 DdeConnect@16 DdeDisconnect@4 DdeClientTransaction@32 DdeFreeDataHandle@4 \
    DdeUninitialize@4
deflib gdi32.dll CreateSolidBrush@4 CreateHatchBrush@8 DeleteObject@4 SelectObject@8 \
    SetTextColor@8 SetBkColor@8 SetBkMode@8 TextOutA@20 Rectangle@20 GetStockObject@4 \
    CreateFontA@56 CreatePen@12 GetTextExtentPointA@16 Ellipse@20 RoundRect@28 \
    Polygon@12 MoveToEx@16 LineTo@12 Arc@36 CreateCompatibleDC@4 \
    CreateCompatibleBitmap@12 BitBlt@36 DeleteDC@4 GetDeviceCaps@8

cham() { python3 -c "import sys,pefile;p=pefile.PE(sys.argv[1]);p.OPTIONAL_HEADER.CheckSum=p.generate_checksum();p.write(sys.argv[1])" "$1"; }

# the icons (drawn by mkicon.py): the manager's, and one with a badge for
# each other tool
for r in mgptnt mgptpart mbrginst mbrgunin; do llvm-rc /nologo /fo $r.res "$ROOT/tools/$r.rc"; done

echo "[*] mgptpart.exe"
$CC -c "$ROOT/tools/mgptpart.c" -o mgptpart.obj
lld-link /nologo /subsystem:console,$SUB /entry:$(E MgptMain 0) /nodefaultlib /out:mgptpart.exe mgptpart.obj mgptpart.res kernel32.lib
cham mgptpart.exe

echo "[*] mbrginst.exe"
$CC -c "$ROOT/tools/mbrginst.c" -o mbrginst.obj
lld-link /nologo /subsystem:console,$SUB /entry:$(E MbrgInstEntry 0) /nodefaultlib /out:mbrginst.exe mbrginst.obj mbrginst.res kernel32.lib advapi32.lib user32.lib
cham mbrginst.exe

echo "[*] mbrgunin.exe"
$CC -c "$ROOT/tools/mbrgunin.c" -o mbrgunin.obj
lld-link /nologo /subsystem:windows,$SUB /entry:$(E MbrgUninEntry 0) /nodefaultlib /out:mbrgunin.exe mbrgunin.obj mbrgunin.res kernel32.lib advapi32.lib user32.lib
cham mbrgunin.exe

echo "[*] mgptgui.exe"
$CC -c "$ROOT/tools/mgptgui.c" -o mgptgui.obj
lld-link /nologo /subsystem:windows,$SUB /entry:$(E GuiMain 0) /nodefaultlib /out:mgptgui.exe mgptgui.obj mgptnt.res kernel32.lib user32.lib gdi32.lib
cham mgptgui.exe

echo "[*] tools built in $OUT"
ls -l "$OUT"/*.exe
