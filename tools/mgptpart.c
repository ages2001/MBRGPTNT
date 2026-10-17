/* mgptpart.c - console partition tool for the MBRGPTNT driver.
   Runs on Windows NT 3.1 through 2000/XP. */

typedef unsigned long  DWORD;
#if defined(_WIN64)
typedef unsigned long long ULONG_PTR;
typedef long long          LONG_PTR;
#else
typedef unsigned long      ULONG_PTR;
typedef long               LONG_PTR;
#endif
typedef unsigned short WORD;
typedef unsigned char  BYTE;
typedef int            BOOL;
typedef void          *HANDLE;
typedef void          *LPVOID;
typedef const char    *LPCSTR;
typedef char          *LPSTR;
typedef DWORD         *LPDWORD;

#define NULL ((void*)0)
#define GENERIC_READ   0x80000000UL
#define GENERIC_WRITE  0x40000000UL
#define FILE_SHARE_READ  1
#define FILE_SHARE_WRITE 2
#define OPEN_EXISTING  3
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#define STD_OUTPUT_HANDLE ((DWORD)-11)

__declspec(dllimport) HANDLE __stdcall CreateFileA(LPCSTR,DWORD,DWORD,LPVOID,DWORD,DWORD,HANDLE);
__declspec(dllimport) BOOL __stdcall DeviceIoControl(HANDLE,DWORD,LPVOID,DWORD,LPVOID,DWORD,LPDWORD,LPVOID);
__declspec(dllimport) BOOL __stdcall CloseHandle(HANDLE);
__declspec(dllimport) HANDLE __stdcall GetStdHandle(DWORD);
__declspec(dllimport) BOOL __stdcall WriteFile(HANDLE,const void*,DWORD,LPDWORD,LPVOID);
__declspec(dllimport) LPSTR __stdcall GetCommandLineA(void);
__declspec(dllimport) void __stdcall ExitProcess(DWORD);
__declspec(dllimport) DWORD __stdcall GetLastError(void);
__declspec(dllimport) DWORD __stdcall GetVersion(void);
#define ERROR_ACCESS_DENIED 5

#include "../src/mgptioc.h"

static HANDLE g_out;
static DWORD slen(const char*s){DWORD n=0;while(s[n])n++;return n;}
static void outs(const char*s){DWORD w;WriteFile(g_out,s,slen(s),&w,NULL);}
static char* u64dec(mgpt_u64 v,char*buf){
    union { mgpt_u64 q; struct { DWORD lo,hi; } p; } u; u.q=v;
    char tmp[16]; int i=0,j; DWORD x;
    if(u.p.hi!=0){const char*h="0123456789ABCDEF";int k;buf[0]='0';buf[1]='x';
        for(k=0;k<8;k++)buf[2+k]=h[(u.p.hi>>((7-k)*4))&0xF];
        for(k=0;k<8;k++)buf[10+k]=h[(u.p.lo>>((7-k)*4))&0xF];buf[18]=0;return buf;}
    x=u.p.lo; if(x==0){buf[0]='0';buf[1]=0;return buf;}
    while(x){tmp[i++]=(char)('0'+(int)(x%10));x/=10;}
    for(j=0;j<i;j++)buf[j]=tmp[i-1-j];buf[i]=0;return buf;
}
static char* u32hex(DWORD v,int digits,char*buf){const char*h="0123456789ABCDEF";int i;for(i=0;i<digits;i++)buf[digits-1-i]=h[(v>>(i*4))&0xF];buf[digits]=0;return buf;}
static void outdec(mgpt_u64 v){char b[24];outs(u64dec(v,b));}
static void outhex(DWORD v,int d){char b[12];outs(u32hex(v,d,b));}

static char g_args[16][64];
static int g_nargs;
static void parse_args(void){
    char*p=GetCommandLineA();int inq=0;g_nargs=0;
    while(*p==' ')p++;
    if(*p=='"'){p++;while(*p&&*p!='"')p++;if(*p)p++;}else{while(*p&&*p!=' ')p++;}
    for(;;){int k=0;while(*p==' ')p++;if(!*p)break;if(g_nargs>=16)break;
        while(*p&&(inq||*p!=' ')){if(*p=='"'){inq=!inq;p++;continue;}if(k<63)g_args[g_nargs][k++]=*p;p++;}
        g_args[g_nargs][k]=0;g_nargs++;}
}
static int streq(const char*a,const char*b){while(*a&&*b){if(*a!=*b)return 0;a++;b++;}return *a==*b;}
static DWORD atou(const char*s){DWORD v=0;while(*s>='0'&&*s<='9'){v=v*10+(DWORD)(*s-'0');s++;}return v;}
static DWORD atoux(const char*s){DWORD v=0;if(s[0]=='0'&&(s[1]=='x'||s[1]=='X'))s+=2;
    while(*s){char c=*s;DWORD d;if(c>='0'&&c<='9')d=(DWORD)(c-'0');else if(c>='a'&&c<='f')d=(DWORD)(c-'a'+10);else if(c>='A'&&c<='F')d=(DWORD)(c-'A'+10);else break;v=v*16+d;s++;}return v;}

static HANDLE open_drv(void){
    HANDLE h=CreateFileA("\\\\.\\MbrGptNt",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
    if(h==INVALID_HANDLE_VALUE){outs("cannot open MbrGptNt (driver loaded?)\r\n");ExitProcess(1);}
    return h;
}
static void cmd_list(HANDLE h){
    static MGPT_DISKLIST dl;DWORD ret,i;
    if(!DeviceIoControl(h,IOCTL_MGPT_LIST,NULL,0,&dl,sizeof(dl),&ret,NULL)){outs("LIST failed\r\n");return;}
    outs("Disk  Scheme  Sector  Parts  Owned  TotalSectors\r\n");
    for(i=0;i<dl.Count;i++){MGPT_DISKINFO*d=&dl.Disk[i];
        outs("  ");outdec(d->DiskNumber);outs("   ");outs(d->Dynamic?(d->IsGpt?"DynG":"DynM"):d->IsGpt?"GPT ":"MBR ");
        outs("  ");outdec(d->SectorSize);outs("    ");outdec(d->PartCount);
        outs("     ");outs(d->Owned?"yes":"no ");outs("    ");outdec(d->TotalSectors);outs("\r\n");}
}
static const char* fsname(DWORD k){switch(k){
    case 1:return "FAT12";case 2:return "FAT16";case 3:return "FAT32";case 4:return "NTFS";
    case 5:return "HPFS";case 6:return "exFAT";case 7:return "Extended";case 8:return "EFI System";
    case 9:return "Linux";case 10:return "ext2";case 11:return "ext3";case 12:return "ext4";
    case 13:return "ReFS";case 14:return "Btrfs";case 15:return "MSR";case 16:return "LDM-meta";case 17:return "LDM-data";default:return "RAW";}}
static const char* ldmtype(DWORD t){switch(t){case 1:return "simple";case 2:return "spanned";case 3:return "striped";case 4:return "mirror";case 5:return "raid5";}return "?";}
static const char* ldmstate(DWORD s){return s==MGPT_LDMS_HEALTHY?"healthy":s==MGPT_LDMS_DEGRADED?"degraded (read-only)":"failed";}
static void pad(const char*s,int w){outs(s);{int k=(int)slen(s);while(k<w){outs(" ");k++;}}}

static void cmd_show(HANDLE h,DWORD disk){
    static MGPT_LAYOUT lay;DWORD ret,i;
    if(!DeviceIoControl(h,IOCTL_MGPT_GETLAYOUT,&disk,sizeof(disk),&lay,sizeof(lay),&ret,NULL)){outs("GETLAYOUT failed\r\n");return;}
    outs("Disk ");outdec(disk);outs(lay.Dynamic?(lay.IsGpt?"  (dynamic, GPT)":"  (dynamic, MBR)"):lay.IsGpt?"  (GPT)":"  (MBR)");
    outs("  sector=");outdec(lay.SectorSize);outs("  sectors=");outdec(lay.TotalSectors);outs("\r\n");
    if(lay.Dynamic){
        if(lay.LdmGroup==0xFFFFFFFFUL)outs("disk group: database not readable\r\n");
        else{outs("disk group ");outs(lay.DgName);outs(lay.LdmWritable?"":"  (read-only here)");
            if(lay.LdmMissing){outs("  ");outdec(lay.LdmMissing);outs(" disk(s) missing");}outs("\r\n");}
        outs("Volume / piece    Type    Filesystem Ltr Start    Sectors  State\r\n");
        for(i=0;i<lay.Count;i++){MGPT_PART*p=&lay.Part[i];char nm[40];
            if(p->Flags&MGPT_F_LDM){int k=0;while(p->VolName[k]&&k<15){nm[k]=p->VolName[k];k++;}
                if(p->LdmPieces>1){nm[k++]=' ';nm[k++]='#';nm[k++]=(char)('1'+(p->LdmPiece%9));}nm[k]=0;
                outs(" ");pad(nm,17);pad(ldmtype(p->LdmType),8);}
            else{outs(" ");pad(p->FsKind==15?"(reserved)":p->FsKind==16?"(LDM database)":"(partition)",17);pad("-",8);}
            pad(fsname(p->FsKind),11);
            {char lb[4];if(p->DriveLetter){lb[0]=(char)p->DriveLetter;lb[1]=':';lb[2]=0;}else{lb[0]='-';lb[1]=0;}pad(lb,4);}
            {char b[24];pad(u64dec(p->StartLba,b),9);pad(u64dec(p->SectorCount,b),9);}
            if(p->Flags&MGPT_F_LDM)outs(ldmstate(p->LdmState));
            outs("\r\n");}
        outs("(volumes: see 'mgptpart volumes')\r\n");
        return;
    }
    outs("Idx  Type  Filesystem   Ltr  Start        Sectors      Flags\r\n");
    for(i=0;i<lay.Count;i++){MGPT_PART*p=&lay.Part[i];
        outs(" ");outdec(i);outs("   0x");outhex(p->MbrType,2);outs(" ");
        {const char*n=fsname(p->FsKind);outs(n);{int k=(int)slen(n);while(k<12){outs(" ");k++;}}}
        {char lb[4];if(p->DriveLetter){lb[0]=(char)p->DriveLetter;lb[1]=':';lb[2]=0;}else{lb[0]='-';lb[1]=0;}outs(lb);{int k=(int)slen(lb);while(k<4){outs(" ");k++;}}}
        outs(" ");outdec(p->StartLba);outs("   ");outdec(p->SectorCount);outs("   ");
        if(p->Flags&MGPT_F_BOOT)outs("B");if(p->Flags&MGPT_F_LOGICAL)outs("L");if(p->Flags&MGPT_F_RECOGNIZED)outs("R");
        if(p->Flags&MGPT_F_PROTECTED)outs("*");
        outs("\r\n");}
    outs("(* = running system/boot partition - protected)\r\n");
}
static void cmd_init(HANDLE h,DWORD disk,int gpt){
    MGPT_INIT in;DWORD ret;in.DiskNumber=disk;in.Scheme=(DWORD)gpt;
    if(DeviceIoControl(h,IOCTL_MGPT_INIT,&in,sizeof(in),NULL,0,&ret,NULL))outs("initialized.\r\n");
    else if(GetLastError()==ERROR_ACCESS_DENIED)outs("cannot initialize: a volume on this disk is in use (mounted - e.g. the system/boot partition).\r\n");
    else outs("INIT failed.\r\n");}
static int ieq(const char*a,const char*b){while(*a&&*b){char x=*a,y=*b;if(x>='A'&&x<='Z')x=(char)(x+32);if(y>='A'&&y<='Z')y=(char)(y+32);if(x!=y)return 0;a++;b++;}return *a==*b;}
/* "fat16" / "fat32" / "ntfs" / "ext" or a hex byte; FAT16, FAT32 and
   extended bytes become their family so the driver picks the right one
   (04/06/0E, 0B/0C, 05/0F) for the partition's size and position */
static DWORD parse_type(const char*s){
    DWORD t;
    if(ieq(s,"fat16"))return MGPT_TYPE_FAT16; if(ieq(s,"fat32"))return MGPT_TYPE_FAT32;
    if(ieq(s,"ntfs"))return 0x07; if(ieq(s,"ext")||ieq(s,"extended"))return MGPT_TYPE_EXTENDED;
    t=atoux(s);
    if(t==0x04||t==0x06||t==0x0E)return MGPT_TYPE_FAT16;
    if(t==0x0B||t==0x0C)return MGPT_TYPE_FAT32;
    if(t==0x05||t==0x0F)return MGPT_TYPE_EXTENDED;
    return t;
}
static void cmd_create(HANDLE h,DWORD disk,DWORD type,DWORD sizeMB){
    static MGPT_LAYOUT lay; static mgpt_u64 old[MGPT_MAX_PARTS]; MGPT_CREATE c;DWORD ret,i,k,n;
    if(!DeviceIoControl(h,IOCTL_MGPT_GETLAYOUT,&disk,sizeof(disk),&lay,sizeof(lay),&ret,NULL)){outs("cannot read disk\r\n");return;}
    if(type==0||(type>0xFF&&type<MGPT_TYPE_FAT16)||type>MGPT_TYPE_EXTENDED){outs("bad type\r\n");return;}
    if(type==MGPT_TYPE_FAT16&&sizeMB>4095){outs("FAT16 holds at most 4095 MB\r\n");return;}
    if(type==MGPT_TYPE_FAT32&&sizeMB&&sizeMB<33){outs("FAT32 needs at least 33 MB\r\n");return;}
    n=lay.Count;for(i=0;i<n;i++)old[i]=lay.Part[i].StartLba;
    c.DiskNumber=disk;c.MbrType=type;c.StartLba=0;c.Boot=0;
    if(sizeMB==0)c.SectorCount=0;else c.SectorCount=(mgpt_u64)sizeMB*(DWORD)(1048576u/lay.SectorSize);
    if(!DeviceIoControl(h,IOCTL_MGPT_CREATE,&c,sizeof(c),NULL,0,&ret,NULL)){outs("CREATE failed\r\n");return;}
    outs("created");
    if(DeviceIoControl(h,IOCTL_MGPT_GETLAYOUT,&disk,sizeof(disk),&lay,sizeof(lay),&ret,NULL))
        for(i=0;i<lay.Count;i++){int isnew=1;for(k=0;k<n;k++)if(old[k]==lay.Part[i].StartLba){isnew=0;break;}
            if(!isnew)continue;
            if(!lay.IsGpt){outs(", type 0x");outhex(lay.Part[i].MbrType,2);}
            if(lay.Part[i].DriveLetter){char b[4];b[0]=(char)lay.Part[i].DriveLetter;b[1]=':';b[2]=0;outs(", drive ");outs(b);}
            break;}
    outs(".\r\n");}
static void cmd_delete(HANDLE h,DWORD disk,DWORD idx){
    MGPT_DELETE d;DWORD ret;d.DiskNumber=disk;d.Index=idx;
    if(DeviceIoControl(h,IOCTL_MGPT_DELETE,&d,sizeof(d),NULL,0,&ret,NULL))outs("deleted.\r\n");
    else if(GetLastError()==ERROR_ACCESS_DENIED)outs("cannot delete: partition is in use (mounted - e.g. the system/boot partition).\r\n");
    else if(GetLastError()==145)outs("cannot delete: the extended partition still holds logical drives - delete them first.\r\n");
    else outs("DELETE failed.\r\n");}
static void cmd_settype(HANDLE h,DWORD disk,DWORD idx,DWORD type){
    MGPT_SETTYPE s;DWORD ret;s.DiskNumber=disk;s.Index=idx;s.MbrType=type;
    if(!DeviceIoControl(h,IOCTL_MGPT_SETTYPE,&s,sizeof(s),NULL,0,&ret,NULL))outs("SETTYPE failed\r\n");else outs("type set.\r\n");}
static void cmd_active(HANDLE h,DWORD disk,DWORD idx,DWORD on){
    MGPT_SETACTIVE s;DWORD ret;s.DiskNumber=disk;s.Index=idx;s.Active=on;
    if(!DeviceIoControl(h,IOCTL_MGPT_SETACTIVE,&s,sizeof(s),NULL,0,&ret,NULL))outs("SETACTIVE failed\r\n");else outs("active flag set.\r\n");}
static void cmd_letter(HANDLE h,DWORD disk,DWORD idx,const char*ls){
    MGPT_SETLETTER s;DWORD ret;char c=ls[0];
    if(c>='a'&&c<='z')c=(char)(c-32);
    s.DiskNumber=disk;s.Index=idx;s.Letter=(DWORD)(unsigned char)c;
    if(DeviceIoControl(h,IOCTL_MGPT_SETLETTER,&s,sizeof(s),NULL,0,&ret,NULL))outs("drive letter changed.\r\n");
    else if(GetLastError()==ERROR_ACCESS_DENIED)outs("cannot change: the running system partition's letter is protected.\r\n");
    else outs("SETLETTER failed (letter in use, or extended partition).\r\n");}
static void cmd_convert(HANDLE h,DWORD disk,int gpt){
    MGPT_CONVERT cv;DWORD ret;cv.DiskNumber=disk;cv.Scheme=(DWORD)gpt;cv.Reason=0;
    if(!DeviceIoControl(h,IOCTL_MGPT_CONVERT,&cv,sizeof(cv),&cv,sizeof(cv),&ret,NULL)){outs("CONVERT failed (driver error).\r\n");return;}
    switch(cv.Reason){
    case MGPT_CVT_OK:        outs("converted (no data moved).\r\n");break;
    case MGPT_CVT_SAME:      outs("cannot convert: the disk already uses that format.\r\n");break;
    case MGPT_CVT_SYSTEM:    outs("cannot convert: this is the running system disk. Boot from other media to convert it.\r\n");break;
    case MGPT_CVT_NOROOM_GPT:outs("cannot convert to GPT: no room for the GPT headers at the start/end of the disk (a partition occupies it).\r\n");break;
    case MGPT_CVT_TOOMANY:   outs("cannot convert to MBR: too many partitions to fit as primaries/logicals without moving data.\r\n");break;
    case MGPT_CVT_TOOBIG:    outs("cannot convert to MBR: a partition lies beyond 2 TiB, which MBR cannot address.\r\n");break;
    case MGPT_CVT_NOEBR:     outs("cannot convert to MBR: no free sector before a partition for a logical drive's boot record. A gap between partitions is required.\r\n");break;
    case MGPT_CVT_WRITE:     outs("cannot convert: the new table could not be written.\r\n");break;
    default:                 outs("cannot convert.\r\n");break;}}

/* ---- dynamic disks ---- */
static MGPT_LDMINFO g_li;
static int load_ldm(HANDLE h){DWORD ret;if(!DeviceIoControl(h,IOCTL_MGPT_LDMINFO,NULL,0,&g_li,sizeof(g_li),&ret,NULL)){outs("LDMINFO failed\r\n");return 0;}return 1;}
static void cmd_volumes(HANDLE h){
    DWORD gi,i;
    if(!load_ldm(h))return;
    if(!g_li.Count){outs("no dynamic disks.\r\n");return;}
    for(gi=0;gi<g_li.Count;gi++){MGPT_LDMGROUP*g=&g_li.Group[gi];
        outs("Disk group ");outs(g->Name);outs(g->Native?"  (managed by Windows - read-only here)":g->Writable?"":"  (a disk is missing - read-only here)");outs("\r\n");
        outs("  Disks:\r\n");
        for(i=0;i<g->NDisks;i++){MGPT_LDMDISK*d=&g->Disk[i];
            outs("    ");pad(d->Name,8);
            if(d->NtDisk==0xFFFFFFFFUL)outs("missing");else{outs("disk ");outdec(d->NtDisk);outs("  free ");outdec(d->FreeTotal>>11);outs(" MB (largest ");outdec(d->FreeLargest>>11);outs(" MB)");if(d->Stale)outs("  stale");}
            outs("\r\n");}
        outs("  Volumes:\r\n");
        for(i=0;i<g->NVols;i++){MGPT_LDMVOL*v=&g->Vol[i];char lb[4];
            outs("    ");pad(v->Name,10);pad(ldmtype(v->Type),9);
            if(v->Letter){lb[0]=(char)v->Letter;lb[1]=':';lb[2]=0;}else{lb[0]='-';lb[1]=0;}pad(lb,4);
            pad(fsname(v->FsKind),10);outdec(v->Size>>11);outs(" MB  ");outs(ldmstate(v->State));outs("\r\n");}
    }
}
static int ieq2(const char*a,const char*b){while(*a&&*b){char x=*a,y=*b;if(x>='A'&&x<='Z')x=(char)(x+32);if(y>='A'&&y<='Z')y=(char)(y+32);if(x!=y)return 0;a++;b++;}return *a==*b;}
/* volume by name or drive letter ("E:") */
static MGPT_LDMVOL* find_vol(const char*s,DWORD*gi){
    DWORD g,i;
    for(g=0;g<g_li.Count;g++)for(i=0;i<g_li.Group[g].NVols;i++){MGPT_LDMVOL*v=&g_li.Group[g].Vol[i];
        int isL=(s[1]==':'||s[1]==0)&&s[0]&&((DWORD)(s[0]&~0x20)==v->Letter);
        if(ieq2(s,v->Name)||isL){*gi=g;return v;}}
    return NULL;
}
static void ldm_reason(DWORD r){
    switch(r){
    case MGPT_LDMR_OK:outs("done.\r\n");break;
    case MGPT_LDMR_PARAM:outs("bad request (number of disks / sizes for this type).\r\n");break;
    case MGPT_LDMR_SPACE:outs("not enough free space on a disk.\r\n");break;
    case MGPT_LDMR_MISSING:outs("a disk of the group is missing.\r\n");break;
    case MGPT_LDMR_INUSE:outs("in use (a file is open on the volume).\r\n");break;
    case MGPT_LDMR_NOROOM:outs("no room for the LDM database (1 MB at the end / in the reserved partition).\r\n");break;
    case MGPT_LDMR_LOGICAL:outs("the disk has logical drives.\r\n");break;
    case MGPT_LDMR_NATIVE:outs("Windows manages this dynamic disk itself - use Disk Management.\r\n");break;
    case MGPT_LDMR_WRITE:outs("writing failed.\r\n");break;
    case MGPT_LDMR_FULL:outs("database full.\r\n");break;
    case MGPT_LDMR_SYSTEM:outs("this is the system disk.\r\n");break;
    case MGPT_LDMR_SECTOR:outs("dynamic disks need 512-byte sectors.\r\n");break;
    case MGPT_LDMR_NOTEMPTY:outs("the disk still holds dynamic volumes.\r\n");break;
    case MGPT_LDMR_SAME:outs("the disk is already of that kind.\r\n");break;
    case MGPT_LDMR_FOREIGN:outs("another (Windows) disk group is present.\r\n");break;
    default:outs("failed.\r\n");}
}
/* vcreate <type> <MB> <disk>[:<MB>] ...   (MB 0 = as large as possible) */
static void cmd_vcreate(HANDLE h,int argi){
    static MGPT_LDMCREATE c; DWORD t=0,i,ret,mb,gi=0xFFFFFFFFUL; int a;
    static const char*tn[5]={"simple","spanned","striped","mirror","raid5"};
    if(!load_ldm(h))return;
    for(i=0;i<5;i++)if(ieq2(g_args[argi],tn[i]))t=i+1;
    if(!t){outs("type: simple|spanned|striped|mirror|raid5\r\n");return;}
    mb=atou(g_args[argi+1]);
    for(i=0;i<sizeof(c);i++)((char*)&c)[i]=0;
    c.Type=t;
    for(a=argi+2;a<g_nargs&&c.NDisks<MGPT_LDM_DISKS;a++){
        DWORD dn=atou(g_args[a]),g,k,dmb=mb; const char*q=g_args[a]; int found=0;
        while(*q&&*q!=':')q++; if(*q==':')dmb=atou(q+1);
        for(g=0;g<g_li.Count&&!found;g++)for(k=0;k<g_li.Group[g].NDisks;k++)if(g_li.Group[g].Disk[k].NtDisk==dn){
            MGPT_LDMDISK*d=&g_li.Group[g].Disk[k];
            if(gi!=0xFFFFFFFFUL&&gi!=g){outs("the disks belong to different disk groups\r\n");return;}
            gi=g;c.Disk[c.NDisks]=k;
            c.Size[c.NDisks]=dmb?((mgpt_u64)dmb<<11):d->FreeLargest;
            c.NDisks++;found=1;break;}
        if(!found){outs("disk ");outs(g_args[a]);outs(" is not a dynamic disk\r\n");return;}
    }
    if(gi==0xFFFFFFFFUL){outs("give the disks\r\n");return;}
    /* equal parts for striped / mirror / raid5 with no size: the smallest */
    if(t>=3){mgpt_u64 m=c.Size[0];for(i=1;i<c.NDisks;i++)if(c.Size[i]<m)m=c.Size[i];for(i=0;i<c.NDisks;i++)c.Size[i]=m;}
    c.Group=gi;
    if(!DeviceIoControl(h,IOCTL_MGPT_LDMCREATE,&c,sizeof(c),&c,sizeof(c),&ret,NULL)){outs("LDMCREATE failed\r\n");return;}
    if(c.Reason){ldm_reason(c.Reason);return;}
    outs("created, ");outdec(c.VolSize>>11);outs(" MB");
    if(c.Letter){char b[4];b[0]=(char)c.Letter;b[1]=':';b[2]=0;outs(", drive ");outs(b);}
    outs(".\r\n");
}
static void cmd_vreq(HANDLE h,DWORD code,const char*name,const char*letter){
    static MGPT_LDMVOLREQ r; DWORD gi,i,ret; MGPT_LDMVOL*v;
    if(!load_ldm(h))return;
    v=find_vol(name,&gi); if(!v){outs("no such volume\r\n");return;}
    for(i=0;i<sizeof(r);i++)((char*)&r)[i]=0;
    r.Group=gi;for(i=0;i<16;i++)r.Guid[i]=v->Guid[i];
    if(letter){char c=letter[0];if(c>='a'&&c<='z')c=(char)(c-32);r.Letter=ieq2(letter,"none")?0:(DWORD)(unsigned char)c;}
    if(!DeviceIoControl(h,code,&r,sizeof(r),&r,sizeof(r),&ret,NULL)){outs("request failed\r\n");return;}
    ldm_reason(r.Reason);
}
static void cmd_dynamic(HANDLE h,DWORD disk,int to){
    static MGPT_DYNAMIC r; DWORD ret; r.DiskNumber=disk; r.ToDynamic=(DWORD)to; r.Reason=0;
    if(!DeviceIoControl(h,IOCTL_MGPT_DYNAMIC,&r,sizeof(r),&r,sizeof(r),&ret,NULL)){outs("DYNAMIC failed\r\n");return;}
    ldm_reason(r.Reason);
}

void __stdcall MgptMain(void){
    HANDLE h; g_out=GetStdHandle(STD_OUTPUT_HANDLE); parse_args();
    if(g_nargs<1){
        outs("mgptpart commands:\r\n");
        outs("  list\r\n  show <disk>\r\n  init <disk> gpt|mbr\r\n");
        outs("  create <disk> fat16|fat32|ntfs|ext|<type-hex> [MB]\r\n  delete <disk> <index>\r\n");
        outs("  settype <disk> <index> <type-hex>\r\n  active <disk> <index> on|off\r\n");
        outs("  letter <disk> <index> <C-Z>\r\n  convert <disk> gpt|mbr\r\n");
        outs("  rescan   (look for disks plugged in or removed now, then list)\r\n");
        outs("dynamic disks:\r\n  volumes\r\n  dynamic <disk>   basic <disk>\r\n");
        outs("  vcreate simple|spanned|striped|mirror|raid5 <MB> <disk>[:<MB>] ...  (MB per disk, 0 = max)\r\n");
        outs("  vdelete <volume|X:>\r\n  vletter <volume|X:> <C-Z|none>\r\n");
        ExitProcess(0);
    }
    h=open_drv();
    if(streq(g_args[0],"list"))cmd_list(h);
    else if(streq(g_args[0],"rescan")){ DWORD gen,ret; DeviceIoControl(h,IOCTL_MGPT_RESCAN,NULL,0,&gen,sizeof(gen),&ret,NULL); cmd_list(h); }
    else if(streq(g_args[0],"show")&&g_nargs>=2)cmd_show(h,atou(g_args[1]));
    else if(streq(g_args[0],"init")&&g_nargs>=3)cmd_init(h,atou(g_args[1]),streq(g_args[2],"gpt"));
    else if(streq(g_args[0],"create")&&g_nargs>=3)cmd_create(h,atou(g_args[1]),parse_type(g_args[2]),g_nargs>=4?atou(g_args[3]):0);
    else if(streq(g_args[0],"delete")&&g_nargs>=3)cmd_delete(h,atou(g_args[1]),atou(g_args[2]));
    else if(streq(g_args[0],"settype")&&g_nargs>=4)cmd_settype(h,atou(g_args[1]),atou(g_args[2]),atoux(g_args[3]));
    else if(streq(g_args[0],"active")&&g_nargs>=4)cmd_active(h,atou(g_args[1]),atou(g_args[2]),streq(g_args[3],"on"));
    else if(streq(g_args[0],"letter")&&g_nargs>=4)cmd_letter(h,atou(g_args[1]),atou(g_args[2]),g_args[3]);
    else if(streq(g_args[0],"convert")&&g_nargs>=3)cmd_convert(h,atou(g_args[1]),streq(g_args[2],"gpt"));
    else if(streq(g_args[0],"volumes"))cmd_volumes(h);
    else if(streq(g_args[0],"dynamic")&&g_nargs>=2)cmd_dynamic(h,atou(g_args[1]),1);
    else if(streq(g_args[0],"basic")&&g_nargs>=2)cmd_dynamic(h,atou(g_args[1]),0);
    else if(streq(g_args[0],"vcreate")&&g_nargs>=4)cmd_vcreate(h,1);
    else if(streq(g_args[0],"vdelete")&&g_nargs>=2)cmd_vreq(h,IOCTL_MGPT_LDMDELETE,g_args[1],NULL);
    else if(streq(g_args[0],"vletter")&&g_nargs>=3)cmd_vreq(h,IOCTL_MGPT_LDMLETTER,g_args[1],g_args[2]);
    else outs("bad command\r\n");
    CloseHandle(h); ExitProcess(0);
}
