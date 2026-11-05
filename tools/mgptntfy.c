/* mgptntfy.c - MBRGPTNT drive-letter notifier. NT 4.0 - XP.

   Runs hidden in each logon session (started from the Run key by
   mbrginst). It asks the driver every second whether its disks changed
   (IOCTL_MGPT_GETGEN) - a disk plugged in or removed while Windows runs,
   e-SATA or USB, or a table changed by a tool - and then tells Explorer
   which drive letters came or went (see mgptbc.h), so they show in My
   Computer at once. One instance per session; "mgptntfy /close" ends it. */

typedef unsigned long  DWORD;
#if defined(_WIN64)
typedef unsigned long long ULONG_PTR;
typedef long long          LONG_PTR;
#else
typedef unsigned long      ULONG_PTR;
typedef long               LONG_PTR;
#endif
typedef unsigned short WORD;
typedef int            BOOL;
typedef void          *HANDLE;
typedef void          *HWND;
typedef void          *HINSTANCE;
typedef void          *LPVOID;
typedef const char    *LPCSTR;
typedef char          *LPSTR;
typedef DWORD         *LPDWORD;
typedef unsigned int   UINT;
typedef long           LONG;
typedef ULONG_PTR      WPARAM;
typedef LONG_PTR       LPARAM;
typedef LONG_PTR       LRESULT;
#define NULL ((void*)0)
#define WINAPI __stdcall
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#define GENERIC_READ  0x80000000UL
#define GENERIC_WRITE 0x40000000UL
#define FILE_SHARE_READ  1
#define FILE_SHARE_WRITE 2
#define OPEN_EXISTING 3
#define WM_DESTROY 0x0002
#define WM_CLOSE   0x0010
#define WM_TIMER   0x0113

typedef struct { UINT style; LPVOID lpfnWndProc; int cbClsExtra, cbWndExtra;
    HINSTANCE hInstance; HANDLE hIcon, hCursor, hbrBackground;
    LPCSTR lpszMenuName, lpszClassName; } WNDCLASSA;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; LONG ptx, pty; } MSG;

__declspec(dllimport) HANDLE WINAPI CreateFileA(LPCSTR,DWORD,DWORD,LPVOID,DWORD,DWORD,HANDLE);
__declspec(dllimport) BOOL   WINAPI DeviceIoControl(HANDLE,DWORD,LPVOID,DWORD,LPVOID,DWORD,LPDWORD,LPVOID);
__declspec(dllimport) BOOL   WINAPI CloseHandle(HANDLE);
__declspec(dllimport) void   WINAPI ExitProcess(UINT);
__declspec(dllimport) HINSTANCE WINAPI GetModuleHandleA(LPCSTR);
__declspec(dllimport) DWORD  WINAPI GetLogicalDrives(void);
__declspec(dllimport) LPVOID WINAPI GetProcAddress(HINSTANCE,LPCSTR);
__declspec(dllimport) LPSTR  WINAPI GetCommandLineA(void);
__declspec(dllimport) WORD   WINAPI RegisterClassA(const WNDCLASSA*);
__declspec(dllimport) HWND   WINAPI CreateWindowExA(DWORD,LPCSTR,LPCSTR,DWORD,int,int,int,int,HWND,HANDLE,HINSTANCE,LPVOID);
__declspec(dllimport) LRESULT WINAPI DefWindowProcA(HWND,UINT,WPARAM,LPARAM);
__declspec(dllimport) BOOL   WINAPI GetMessageA(MSG*,HWND,UINT,UINT);
__declspec(dllimport) BOOL   WINAPI TranslateMessage(const MSG*);
__declspec(dllimport) LRESULT WINAPI DispatchMessageA(const MSG*);
__declspec(dllimport) void   WINAPI PostQuitMessage(int);
__declspec(dllimport) BOOL   WINAPI PostMessageA(HWND,UINT,WPARAM,LPARAM);
__declspec(dllimport) ULONG_PTR WINAPI SetTimer(HWND,ULONG_PTR,UINT,LPVOID);
__declspec(dllimport) HWND   WINAPI FindWindowA(LPCSTR,LPCSTR);
__declspec(dllimport) BOOL   WINAPI DestroyWindow(HWND);

#include "../src/mgptioc.h"
#include "mgptbc.h"

static const char Class[] = "MbrGptNtNotify";
static HANDLE g_drv = INVALID_HANDLE_VALUE;
static DWORD  g_gen;
static int    g_haveGen;

static void Check(void)
{
    DWORD gen, ret;
    if (g_drv == INVALID_HANDLE_VALUE) {
        g_drv = CreateFileA("\\\\.\\MbrGptNt", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (g_drv == INVALID_HANDLE_VALUE) return;
    }
    if (!DeviceIoControl(g_drv, IOCTL_MGPT_GETGEN, NULL, 0, &gen, sizeof(gen), &ret, NULL)) {
        CloseHandle(g_drv); g_drv = INVALID_HANDLE_VALUE; return;
    }
    if (g_haveGen && gen == g_gen) return;
    if (g_haveGen) mgDrivesChanged(); else mgDrivesInit();
    g_gen = gen; g_haveGen = 1;
}

static LRESULT WINAPI WndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_TIMER: Check(); return 0;
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

static int HasArg(const char *cl, const char *a)
{
    const char *p, *q;
    for (p = cl; *p; p++) {
        if (*p != '/' && *p != '-') continue;
        for (q = a; *q && (p[1 + (q - a)] | 0x20) == *q; q++) ;
        if (!*q) return 1;
    }
    return 0;
}

void __stdcall NotifyMain(void)
{
    WNDCLASSA wc; MSG msg; HWND h, old; HINSTANCE inst = GetModuleHandleA(NULL);
    old = FindWindowA(Class, NULL);
    if (HasArg(GetCommandLineA(), "close")) { if (old) PostMessageA(old, WM_CLOSE, 0, 0); ExitProcess(0); }
    if (old) ExitProcess(0);                      /* already running here */
    wc.style = 0; wc.lpfnWndProc = (LPVOID)WndProc; wc.cbClsExtra = 0; wc.cbWndExtra = 0;
    wc.hInstance = inst; wc.hIcon = NULL; wc.hCursor = NULL; wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL; wc.lpszClassName = Class;
    if (!RegisterClassA(&wc)) ExitProcess(1);
    /* a top-level window (never shown): broadcasts reach it, and the
       uninstaller finds it to close it */
    h = CreateWindowExA(0, Class, "MBRGPTNT", 0, 0, 0, 0, 0, NULL, NULL, inst, NULL);
    if (!h) ExitProcess(1);
    Check();
    SetTimer(h, 1, 1000, NULL);
    while (GetMessageA(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    if (g_drv != INVALID_HANDLE_VALUE) CloseHandle(g_drv);
    ExitProcess(0);
}
