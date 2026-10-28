/* tstioctl.c - asks a volume (tstioctl X:) for its size and position the
   ways Windows 2000 / XP file systems do, and prints the answers. */
typedef unsigned long DWORD; typedef int BOOL; typedef void *HANDLE; typedef const char *LPCSTR;
__declspec(dllimport) HANDLE __stdcall CreateFileA(LPCSTR,DWORD,DWORD,void*,DWORD,DWORD,HANDLE);
__declspec(dllimport) BOOL __stdcall DeviceIoControl(HANDLE,DWORD,void*,DWORD,void*,DWORD,DWORD*,void*);
__declspec(dllimport) HANDLE __stdcall GetStdHandle(DWORD);
__declspec(dllimport) BOOL __stdcall WriteFile(HANDLE,const void*,DWORD,DWORD*,void*);
__declspec(dllimport) char *__stdcall GetCommandLineA(void);
__declspec(dllimport) void __stdcall ExitProcess(DWORD);
__declspec(dllimport) DWORD __stdcall GetLastError(void);
static void out(const char *s){DWORD n=0,w;while(s[n])n++;WriteFile(GetStdHandle((DWORD)-11),s,n,&w,0);}
static void num(unsigned long long v){char b[24];int i=23;b[i]=0;do{b[--i]=(char)('0'+v%10);v/=10;}while(v);out(b+i);}
static void q(HANDLE h,const char*nm,DWORD code,unsigned char*b,DWORD len){
    DWORD got=0,i; for(i=0;i<256;i++)b[i]=0xCC;
    out(nm);
    if(!DeviceIoControl(h,code,0,0,b,len,&got,0)){out(" error ");num(GetLastError());out("\r\n");b[0]=0xEE;return;}
    out(" bytes=");num(got);
}
void __stdcall TstMain(void){
    char path[8]="\\\\.\\X:"; char*c=GetCommandLineA(); HANDLE h; unsigned char b[256];
    while(*c&&*c!=' ')c++; while(*c==' ')c++; if(!*c){out("usage: tstioctl X:\r\n");ExitProcess(2);}
    path[4]=*c;
    h=CreateFileA(path,0x80000000UL,3,0,3,0,0);
    if(h==(HANDLE)-1){out("open failed ");num(GetLastError());out("\r\n");ExitProcess(1);}
    q(h,"PARTITION_INFO",0x00074004UL,b,32);
    if(b[0]!=0xEE){out(" start=");num(*(unsigned long long*)b);out(" length=");num(*(unsigned long long*)(b+8));out(" number=");num(*(DWORD*)(b+20));out(" type=");num(b[24]);out("\r\n");}
    q(h,"LENGTH_INFO",0x0007405CUL,b,8);
    if(b[0]!=0xEE){out(" length=");num(*(unsigned long long*)b);out("\r\n");}
    q(h,"PARTITION_INFO_EX",0x00070048UL,b,144);
    if(b[0]!=0xEE){out(" style=");num(*(DWORD*)b);out(" start=");num(*(unsigned long long*)(b+8));out(" length=");num(*(unsigned long long*)(b+16));out(" number=");num(*(DWORD*)(b+24));out(" type=");num(b[32]);out("\r\n");}
    q(h,"DEVICE_NUMBER",0x002D1080UL,b,12);
    if(b[0]!=0xEE){out(" type=");num(*(DWORD*)b);out(" disk=");num(*(DWORD*)(b+4));out(" partition=");num(*(DWORD*)(b+8));out("\r\n");}
    ExitProcess(0);
}
