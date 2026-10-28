/* mbrginst.c - installer for the MBRGPTNT driver. NT 3.1 - 2000/XP. */

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
typedef void          *LPVOID;
typedef const char    *LPCSTR;
typedef char          *LPSTR;
typedef DWORD         *LPDWORD;
typedef HANDLE         HKEY;
typedef HKEY          *PHKEY;

#define NULL ((void*)0)
#define HKEY_LOCAL_MACHINE ((HKEY)(LONG_PTR)(long)0x80000002UL)
#define KEY_ALL_ACCESS 0xF003F
#define REG_DWORD 4
#define ERROR_SUCCESS 0L
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define MAX_PATH 260

__declspec(dllimport) long  __stdcall RegCreateKeyExA(HKEY,LPCSTR,DWORD,LPSTR,DWORD,DWORD,LPVOID,PHKEY,LPDWORD);
__declspec(dllimport) long  __stdcall RegSetValueExA(HKEY,LPCSTR,DWORD,DWORD,const BYTE*,DWORD);
__declspec(dllimport) long  __stdcall RegCloseKey(HKEY);
__declspec(dllimport) HANDLE __stdcall GetStdHandle(DWORD);
__declspec(dllimport) BOOL  __stdcall WriteFile(HANDLE,const void*,DWORD,LPDWORD,LPVOID);
__declspec(dllimport) void  __stdcall ExitProcess(DWORD);
__declspec(dllimport) DWORD __stdcall GetModuleFileNameA(HANDLE,LPSTR,DWORD);
__declspec(dllimport) DWORD __stdcall GetWindowsDirectoryA(LPSTR,DWORD);
__declspec(dllimport) BOOL  __stdcall CopyFileA(LPCSTR,LPCSTR,BOOL);
__declspec(dllimport) BOOL  __stdcall MoveFileExA(LPCSTR,LPCSTR,DWORD);
#define MOVEFILE_REPLACE_EXISTING   1
#define MOVEFILE_DELAY_UNTIL_REBOOT 4

#include "pmgroup.h"

static HANDLE g_out;
static DWORD slen(const char*s){DWORD n=0;while(s[n])n++;return n;}
static void outs(const char*s){DWORD w;WriteFile(g_out,s,slen(s),&w,NULL);}
static void scopy(char*d,const char*s){while((*d++=*s++)!=0);}
static void scat(char*d,const char*s){while(*d)d++;while((*d++=*s++)!=0);}
/* Copy src to dst. A file in use (the loaded driver of an earlier
   version, a running tool) is replaced at the next restart instead.
   Returns 0 on failure, 1 when copied, 2 when replaced at the restart. */
static int Install(const char*src,const char*dst){
    char tmp[MAX_PATH];
    if(CopyFileA(src,dst,0))return 1;
    /* name.NEW beside it: 8.3, for the FAT system partitions of NT 3.x */
    {DWORD i,dot=0;scopy(tmp,dst);for(i=0;tmp[i];i++)if(tmp[i]=='.')dot=i;else if(tmp[i]=='\\')dot=0;
     if(dot)tmp[dot]=0;scat(tmp,".new");}
    if(!CopyFileA(src,tmp,0))return 0;
    if(MoveFileExA(tmp,dst,MOVEFILE_REPLACE_EXISTING|MOVEFILE_DELAY_UNTIL_REBOOT))return 2;
    return 0;
}

static const char SvcKey[]="SYSTEM\\CurrentControlSet\\Services\\mbrgptnt";

void __stdcall MbrgInstEntry(void){
    char exe[MAX_PATH],dir[MAX_PATH],src[MAX_PATH],dst[MAX_PATH],win[MAX_PATH];
    DWORD n,i,cut; HKEY hk; DWORD disp,dw; long rc;
    static const char *tools[]={"mgptgui.exe","mgptpart.exe","mbrginst.exe","mbrgunin.exe"};
    g_out=GetStdHandle(STD_OUTPUT_HANDLE);
    n=GetModuleFileNameA(NULL,exe,MAX_PATH);
    scopy(dir,exe);cut=0;for(i=0;i<n;i++)if(dir[i]=='\\')cut=i;dir[cut+1]=0;
    GetWindowsDirectoryA(win,MAX_PATH);
    /* driver -> System32\Drivers */
    scopy(src,dir);scat(src,"mbrgptnt.sys");
    scopy(dst,win);scat(dst,"\\System32\\Drivers\\mbrgptnt.sys");
    switch(Install(src,dst)){
    case 0: outs("could not copy driver (");outs(src);outs("). Run from the install folder.\r\n");ExitProcess(1);
    case 2: outs("  the driver in use is replaced at the next restart\r\n");break;
    }
    /* tools (GUI, console, this installer) -> System32 so they are on PATH */
    for(i=0;i<4;i++){
        scopy(src,dir);scat(src,(char*)tools[i]);
        scopy(dst,win);scat(dst,"\\System32\\");scat(dst,(char*)tools[i]);
        switch(Install(src,dst)){
        case 1: outs("  copied ");outs((char*)tools[i]);outs(" to System32\r\n");break;
        case 2: outs("  ");outs((char*)tools[i]);outs(" is in use: replaced at the next restart\r\n");break;
        }
    }
    rc=RegCreateKeyExA(HKEY_LOCAL_MACHINE,SvcKey,0,NULL,0,KEY_ALL_ACCESS,NULL,&hk,&disp);
    if(rc!=ERROR_SUCCESS){outs("driver copied, service registration failed (Administrator?).\r\n");ExitProcess(1);}
    dw=1;RegSetValueExA(hk,"Type",0,REG_DWORD,(const BYTE*)&dw,4);
    dw=1;RegSetValueExA(hk,"Start",0,REG_DWORD,(const BYTE*)&dw,4);
    dw=1;RegSetValueExA(hk,"ErrorControl",0,REG_DWORD,(const BYTE*)&dw,4);
    RegCloseKey(hk);
    /* the MBRGPTNT program group: the manager, the command line, uninstall */
    {
        PMCONN pm; char sys[MAX_PATH];
        scopy(sys,win);scat(sys,"\\System32");
        if(PmOpen(&pm)){
            if(PmCreateGroup(&pm,sys))outs("  added the MBRGPTNT program group\r\n");
            else outs("  the MBRGPTNT program group could not be created\r\n");
            PmClose(&pm);
        } else outs("  Program Manager is not running: no program group was added\r\n");
    }
    outs("MBRGPTNT installed. Reboot to load the driver.\r\n");
    ExitProcess(0);
}
