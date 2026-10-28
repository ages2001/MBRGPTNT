/* mbrgunin.c - removes MBRGPTNT: the service, the driver, the tools and
   the program group. NT 3.1 - XP, i386 and x64. Files still in use (the
   loaded driver, this program) are deleted at the next restart. */

typedef unsigned long  DWORD;
#if defined(_WIN64)
typedef unsigned long long ULONG_PTR;
typedef long long          LONG_PTR;
#else
typedef unsigned long      ULONG_PTR;
typedef long               LONG_PTR;
#endif
typedef unsigned char  BYTE;
typedef int            BOOL;
typedef void          *HANDLE;
typedef void          *HWND;
typedef void          *LPVOID;
typedef const char    *LPCSTR;
typedef char          *LPSTR;
typedef HANDLE         HKEY;

#define NULL ((void*)0)
#define HKEY_LOCAL_MACHINE ((HKEY)(LONG_PTR)(long)0x80000002UL)
#define KEY_ALL_ACCESS 0xF003F
#define ERROR_SUCCESS 0L
#define MAX_PATH 260
#define MB_YESNO 4
#define MB_ICONQUESTION 0x20
#define MB_ICONINFORMATION 0x40
#define MB_ICONEXCLAMATION 0x30
#define IDYES 6
#define MOVEFILE_DELAY_UNTIL_REBOOT 4

__declspec(dllimport) long  __stdcall RegOpenKeyExA(HKEY,LPCSTR,DWORD,DWORD,HKEY*);
__declspec(dllimport) long  __stdcall RegEnumKeyA(HKEY,DWORD,LPSTR,DWORD);
__declspec(dllimport) long  __stdcall RegDeleteKeyA(HKEY,LPCSTR);
__declspec(dllimport) long  __stdcall RegCloseKey(HKEY);
__declspec(dllimport) void  __stdcall ExitProcess(DWORD);
__declspec(dllimport) DWORD __stdcall GetWindowsDirectoryA(LPSTR,DWORD);
__declspec(dllimport) BOOL  __stdcall DeleteFileA(LPCSTR);
__declspec(dllimport) BOOL  __stdcall MoveFileExA(LPCSTR,LPCSTR,DWORD);
__declspec(dllimport) int   __stdcall MessageBoxA(HWND,LPCSTR,LPCSTR,unsigned int);

#define PM_REMOVE_ONLY
#include "pmgroup.h"

static void scopy(char*d,const char*s){while((*d++=*s++)!=0);}
static void scat(char*d,const char*s){while(*d)d++;while((*d++=*s++)!=0);}

/* delete a key with all of its subkeys */
static long DeleteTree(HKEY parent, const char *name)
{
    HKEY k; char sub[260];
    if (RegOpenKeyExA(parent, name, 0, KEY_ALL_ACCESS, &k) != ERROR_SUCCESS) return 1;
    while (RegEnumKeyA(k, 0, sub, sizeof(sub)) == ERROR_SUCCESS)
        if (DeleteTree(k, sub) != ERROR_SUCCESS) break;
    RegCloseKey(k);
    return RegDeleteKeyA(parent, name);
}

/* delete now, or at the next restart when the file is in use;
   returns 1 when it had to wait */
static int Remove(const char *path)
{
    if (DeleteFileA(path)) return 0;
    if (MoveFileExA(path, NULL, MOVEFILE_DELAY_UNTIL_REBOOT)) return 1;
    return 0;
}

void __stdcall MbrgUninEntry(void)
{
    static const char *tools[] = { "mgptgui.exe", "mgptpart.exe", "mbrginst.exe", "mbrgunin.exe" };
    char win[MAX_PATH], path[MAX_PATH]; int later = 0, i; long rc; PMCONN pm;
    if (MessageBoxA(NULL,
            "Remove MBRGPTNT from this computer?\n\n"
            "The driver, the Disk / Partition Manager, the command-line tool and "
            "the MBRGPTNT program group are removed. After the next restart, GPT "
            "partitions and dynamic volumes that MBRGPTNT mounts are no longer "
            "available; the data on the disks is not changed.",
            "Uninstall MBRGPTNT", MB_YESNO | MB_ICONQUESTION) != IDYES)
        ExitProcess(1);

    if (PmOpen(&pm)) { PmDeleteGroup(&pm); PmClose(&pm); }

    rc = DeleteTree(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\mbrgptnt");

    GetWindowsDirectoryA(win, MAX_PATH);
    scopy(path, win); scat(path, "\\System32\\Drivers\\mbrgptnt.sys");
    later |= Remove(path);
    for (i = 0; i < 4; i++) {
        scopy(path, win); scat(path, "\\System32\\"); scat(path, tools[i]);
        later |= Remove(path);
    }

    if (rc != ERROR_SUCCESS)
        MessageBoxA(NULL, "The files and the program group were removed, but the driver's "
                    "service entry could not be deleted (are you an administrator?).",
                    "Uninstall MBRGPTNT", MB_ICONEXCLAMATION);
    else
        MessageBoxA(NULL, later ? "MBRGPTNT was removed. Restart Windows to finish: the driver "
                                  "and the files in use are deleted then."
                                : "MBRGPTNT was removed. Restart Windows to unload the driver.",
                    "Uninstall MBRGPTNT", MB_ICONINFORMATION);
    ExitProcess(0);
}
