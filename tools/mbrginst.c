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
__declspec(dllimport) DWORD __stdcall GetVersion(void);
__declspec(dllimport) unsigned int __stdcall WinExec(LPCSTR,unsigned int);
#define REG_SZ 1
#define REG_MULTI_SZ 7
#define SW_HIDE 0
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
    static const char *tools[]={"mgptgui.exe","mgptpart.exe","mbrginst.exe","mbrgunin.exe","mgptntfy.exe"};
    g_out=GetStdHandle(STD_OUTPUT_HANDLE);
    n=GetModuleFileNameA(NULL,exe,MAX_PATH);
    scopy(dir,exe);cut=0;for(i=0;i<n;i++)if(dir[i]=='\\')cut=i;dir[cut+1]=0;
    GetWindowsDirectoryA(win,MAX_PATH);
    /* driver -> System32\Drivers. On NT 5.0+ (2000/XP) prefer the PnP build
       from the nt50\ sub-folder if the package ships it; it imports NT5-only
       PnP APIs, so it must NOT be used on NT 3.x/4.0. The default
       mbrgptnt.sys beside the installer is the universal legacy build. */
    {
        int done=0;
        scopy(dst,win);scat(dst,"\\System32\\Drivers\\mbrgptnt.sys");
        if((GetVersion()&0xFF)>=5){
            int r; scopy(src,dir);scat(src,"nt50\\mbrgptnt.sys");
            r=Install(src,dst);
            if(r){ done=1; outs("  installed the PnP driver build (2000/XP)\r\n");
                   if(r==2) outs("  the driver in use is replaced at the next restart\r\n"); }
        }
        if(!done){
            scopy(src,dir);scat(src,"mbrgptnt.sys");
            switch(Install(src,dst)){
            case 0: outs("could not copy driver (");outs(src);outs("). Run from the install folder.\r\n");ExitProcess(1);
            case 2: outs("  the driver in use is replaced at the next restart\r\n");break;
            }
        }
    }
    /* tools (GUI, console, this installer) -> System32 so they are on PATH */
    for(i=0;i<5;i++){
        scopy(src,dir);scat(src,(char*)tools[i]);
        scopy(dst,win);scat(dst,"\\System32\\");scat(dst,(char*)tools[i]);
        switch(Install(src,dst)){
        case 1: outs("  copied ");outs((char*)tools[i]);outs(" to System32\r\n");break;
        case 2: outs("  ");outs((char*)tools[i]);outs(" is in use: replaced at the next restart\r\n");break;
        }
    }
    rc=RegCreateKeyExA(HKEY_LOCAL_MACHINE,SvcKey,0,NULL,0,KEY_ALL_ACCESS,NULL,&hk,&disp);
    if(rc!=ERROR_SUCCESS){outs("driver copied, service registration failed (Administrator?).\r\n");ExitProcess(1);}
    dw=1;RegSetValueExA(hk,"Type",0,REG_DWORD,(const BYTE*)&dw,4);         /* SERVICE_KERNEL_DRIVER */
    dw=0;RegSetValueExA(hk,"Start",0,REG_DWORD,(const BYTE*)&dw,4);        /* SERVICE_BOOT_START: load at boot on every OS so GPT system/pagefile volumes exist before the session manager needs them */
    dw=1;RegSetValueExA(hk,"ErrorControl",0,REG_DWORD,(const BYTE*)&dw,4); /* NORMAL: a load failure is logged, boot continues */
    /* No LoadOrderGroup: an ungrouped boot-start driver initialises after
       every named group on NT 3.1 - XP alike, i.e. after the disk stack, so
       Partition0 is openable. (The NT5-only "Boot Bus Extender" group name
       would mean nothing on NT 3.x/4.0 anyway.) Work is deferred to the enum
       thread, so this init order is never on a disk-access path. */
    RegCloseKey(hk);
    outs("  registered as a boot-start driver (Start=0)\r\n");
    /* NT 5.0 (2000) and later: register a root-enumerated PnP device whose
       Service is this driver, so the PnP manager loads MBRGPTNT as its
       function driver and calls AddDevice -> the volumes become real PnP
       PDOs with GUID_DEVINTERFACE_VOLUME. NT 3.x / 4.0 have no PnP manager
       and ignore this. The Enum\Root key can be ACL-protected (notably on
       XP), so this is best-effort; mbrgptnt.inf is the supported fallback. */
    if((GetVersion()&0xFF)>=5){
        static const char Enum0[]="SYSTEM\\CurrentControlSet\\Enum\\Root\\MBRGPTNT\\0000";
        static const char EnumP[]="SYSTEM\\CurrentControlSet\\Enum\\Root\\MBRGPTNT";
        if(RegCreateKeyExA(HKEY_LOCAL_MACHINE,Enum0,0,NULL,0,KEY_ALL_ACCESS,NULL,&hk,&disp)==ERROR_SUCCESS){
            RegSetValueExA(hk,"Service",0,REG_SZ,(const BYTE*)"mbrgptnt",9);
            RegSetValueExA(hk,"ClassGUID",0,REG_SZ,(const BYTE*)"{4D36E97D-E325-11CE-BFC1-08002BE10318}",39);
            RegSetValueExA(hk,"Class",0,REG_SZ,(const BYTE*)"System",7);
            RegSetValueExA(hk,"DeviceDesc",0,REG_SZ,(const BYTE*)"MBRGPTNT GPT Volume Bus",24);
            RegSetValueExA(hk,"HardwareID",0,REG_MULTI_SZ,(const BYTE*)"root\\mbrgptnt\0",14);
            dw=0;RegSetValueExA(hk,"ConfigFlags",0,REG_DWORD,(const BYTE*)&dw,4);
            RegCloseKey(hk);
            if(RegCreateKeyExA(HKEY_LOCAL_MACHINE,EnumP,0,NULL,0,KEY_ALL_ACCESS,NULL,&hk,&disp)==ERROR_SUCCESS){
                dw=1;RegSetValueExA(hk,"NextInstance",0,REG_DWORD,(const BYTE*)&dw,4);RegCloseKey(hk);
            }
            outs("  registered the PnP volume bus (2000/XP/2003)\r\n");
        } else
            outs("  PnP bus node not written (protected Enum key); use mbrgptnt.inf\r\n");
    }
    /* NT 4.0 and later: the drive-letter notifier runs in every logon
       session, so Explorer shows the letters of disks plugged in while
       Windows runs (NT 3.x has no Explorer and no Run key) */
    if((GetVersion()&0xFF)>=4){
        scopy(dst,win);scat(dst,"\\System32\\mgptntfy.exe");
        if(RegCreateKeyExA(HKEY_LOCAL_MACHINE,"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",0,NULL,0,KEY_ALL_ACCESS,NULL,&hk,&disp)==ERROR_SUCCESS){
            RegSetValueExA(hk,"MBRGPTNT",0,REG_SZ,(const BYTE*)dst,slen(dst)+1);
            RegCloseKey(hk);
            outs("  the drive-letter notifier starts at every logon\r\n");
        }
        WinExec(dst,SW_HIDE);   /* now as well (ends at once if already running) */
    }
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
