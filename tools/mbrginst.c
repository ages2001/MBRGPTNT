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

static HANDLE g_out;
static DWORD slen(const char*s){DWORD n=0;while(s[n])n++;return n;}
static void outs(const char*s){DWORD w;WriteFile(g_out,s,slen(s),&w,NULL);}
static void scopy(char*d,const char*s){while((*d++=*s++)!=0);}
static void scat(char*d,const char*s){while(*d)d++;while((*d++=*s++)!=0);}
static const char SvcKey[]="SYSTEM\\CurrentControlSet\\Services\\mbrgptnt";

void __stdcall MbrgInstEntry(void){
    char exe[MAX_PATH],dir[MAX_PATH],src[MAX_PATH],dst[MAX_PATH],win[MAX_PATH];
    DWORD n,i,cut; HKEY hk; DWORD disp,dw; long rc;
    static const char *tools[]={"mgptgui.exe","mgptpart.exe","mbrginst.exe"};
    g_out=GetStdHandle(STD_OUTPUT_HANDLE);
    n=GetModuleFileNameA(NULL,exe,MAX_PATH);
    scopy(dir,exe);cut=0;for(i=0;i<n;i++)if(dir[i]=='\\')cut=i;dir[cut+1]=0;
    GetWindowsDirectoryA(win,MAX_PATH);
    /* driver -> System32\Drivers */
    scopy(src,dir);scat(src,"mbrgptnt.sys");
    scopy(dst,win);scat(dst,"\\System32\\Drivers\\mbrgptnt.sys");
    if(!CopyFileA(src,dst,0)){outs("could not copy driver (");outs(src);outs("). Run from the install folder.\r\n");ExitProcess(1);}
    /* tools (GUI, console, this installer) -> System32 so they are on PATH */
    for(i=0;i<3;i++){
        scopy(src,dir);scat(src,(char*)tools[i]);
        scopy(dst,win);scat(dst,"\\System32\\");scat(dst,(char*)tools[i]);
        if(CopyFileA(src,dst,0)){outs("  copied ");outs((char*)tools[i]);outs(" to System32\r\n");}
    }
    rc=RegCreateKeyExA(HKEY_LOCAL_MACHINE,SvcKey,0,NULL,0,KEY_ALL_ACCESS,NULL,&hk,&disp);
    if(rc!=ERROR_SUCCESS){outs("driver copied, service registration failed (Administrator?).\r\n");ExitProcess(1);}
    dw=1;RegSetValueExA(hk,"Type",0,REG_DWORD,(const BYTE*)&dw,4);
    dw=1;RegSetValueExA(hk,"Start",0,REG_DWORD,(const BYTE*)&dw,4);
    dw=1;RegSetValueExA(hk,"ErrorControl",0,REG_DWORD,(const BYTE*)&dw,4);
    RegCloseKey(hk);
    outs("MBRGPTNT installed. Reboot to load the driver.\r\n");
    ExitProcess(0);
}
