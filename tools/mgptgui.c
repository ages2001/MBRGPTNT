/* mgptgui.c - MBRGPTNT Disk / Partition Manager (graphical).

   A plain Win32 USER32/GDI32 application (no common controls, no resource
   script) so the one binary runs on Windows NT 3.1 through 2000/XP. The
   main window is laid out like a modern disk manager: an icon toolbar, a
   partition table and a disk map. Every operation opens its own dialog for
   its values and reports the result in a message box afterwards. All
   icons are drawn with GDI at run time. */

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
typedef void          *HWND;
typedef void          *HDC;
typedef void          *HINSTANCE;
typedef void          *HMENU;
typedef void          *HICON;
typedef void          *HCURSOR;
typedef void          *HBRUSH;
typedef void          *HFONT;
typedef void          *HGDIOBJ;
typedef void          *HPEN;
typedef void          *HBITMAP;
typedef void          *LPVOID;
typedef const char    *LPCSTR;
typedef char          *LPSTR;
typedef DWORD         *LPDWORD;
typedef unsigned int   UINT;
typedef long           LONG;
typedef DWORD          COLORREF;
typedef ULONG_PTR      WPARAM;
typedef LONG_PTR       LPARAM;
typedef LONG_PTR       LRESULT;

#define NULL ((void*)0)
#define CALLBACK __stdcall
#define WINAPI   __stdcall
#define WINAPIV  __cdecl

#define GENERIC_READ   0x80000000UL
#define GENERIC_WRITE  0x40000000UL
#define FILE_SHARE_READ  1
#define FILE_SHARE_WRITE 2
#define OPEN_EXISTING  3
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)

#define WS_OVERLAPPEDWINDOW 0x00CF0000UL
#define WS_POPUP       0x80000000UL
#define WS_CHILD       0x40000000UL
#define WS_VISIBLE     0x10000000UL
#define WS_CLIPCHILDREN 0x02000000UL
#define WS_CAPTION     0x00C00000UL
#define WS_BORDER      0x00800000UL
#define WS_SYSMENU     0x00080000UL
#define WS_VSCROLL     0x00200000UL
#define WS_GROUP       0x00020000UL
#define WS_TABSTOP     0x00010000UL
#define WS_EX_DLGMODALFRAME 0x00000001UL
#define BS_PUSHBUTTON  0x00000000UL
#define BS_DEFPUSHBUTTON 0x00000001UL
#define BS_AUTORADIOBUTTON 0x00000009UL
#define BS_AUTOCHECKBOX 0x00000003UL
#define ES_AUTOHSCROLL 0x00000080UL
#define SS_LEFT        0x00000000UL
#define CBS_DROPDOWN     0x0002
#define CBS_DROPDOWNLIST 0x0003
#define CW_USEDEFAULT  ((int)0x80000000)
#define SW_SHOW        5
#define SW_SHOWMAXIMIZED 3

#define WM_CREATE      0x0001
#define WM_DESTROY     0x0002
#define WM_SIZE        0x0005
#define WM_PAINT       0x000F
#define WM_CLOSE       0x0010
#define WM_ERASEBKGND  0x0014
#define WM_SETFONT     0x0030
#define WM_KEYDOWN     0x0100
#define WM_TIMER       0x0113
#define WM_MOUSEWHEEL  0x020A
#define WM_COMMAND     0x0111
#define WM_CTLCOLORBTN    0x0135
#define WM_CTLCOLORSTATIC 0x0138
#define WM_MOUSEMOVE   0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP   0x0202
#define WM_RBUTTONDOWN 0x0204
#define WM_DEVICECHANGE 0x0219
#define DBT_DEVNODES_CHANGED 0x0007
#define DBT_DEVICEARRIVAL 0x8000
#define DBT_DEVICEREMOVECOMPLETE 0x8004
#define HWND_BROADCAST ((HWND)(ULONG_PTR)0xFFFF)

#define BM_GETCHECK    0x00F0
#define BM_SETCHECK    0x00F1
#define CB_SETEDITSEL  0x0142
#define CB_ADDSTRING   0x0143
#define CB_GETCURSEL   0x0147
#define CB_SETCURSEL   0x014E

#define MB_OK          0
#define MB_ICONERROR       0x10
#define MB_ICONEXCLAMATION 0x30
#define MB_ICONINFORMATION 0x40
#define MB_YESNOCANCEL 3
#define IDYES 6
#define IDNO  7
#define ERROR_ACCESS_DENIED 5

#define MF_STRING     0x0000
#define MF_GRAYED     0x0001
#define MF_SEPARATOR  0x0800
#define TPM_RIGHTBUTTON 0x0002

#define VK_DELETE 0x2E
#define VK_UP     0x26
#define VK_DOWN   0x28
#define VK_PRIOR  0x21
#define VK_NEXT   0x22
#define VK_END    0x23
#define VK_HOME   0x24
#define VK_F5     0x74

#define TRANSPARENT     1
#define ANSI_VAR_FONT   4
#define NULL_BRUSH      5
#define IDC_ARROW ((LPCSTR)(ULONG_PTR)32512)
#define IDC_WAIT  ((LPCSTR)(ULONG_PTR)32514)
#define HS_FDIAGONAL 3
#define HS_DIAGCROSS 5
#define SRCCOPY  0x00CC0020UL
#define BITSPIXEL 12
#define PLANES    14
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1

typedef struct { LONG left, top, right, bottom; } RECT;
typedef RECT *LPRECT;
typedef struct { LONG x, y; } POINT;
typedef struct { LONG cx, cy; } SIZE;
typedef struct { HDC hdc; BOOL fErase; RECT rcPaint; BOOL fRestore; BOOL fIncUpdate; BYTE rgbReserved[32]; } PAINTSTRUCT;
typedef struct { UINT style; LPVOID lpfnWndProc; int cbClsExtra, cbWndExtra;
    HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground;
    LPCSTR lpszMenuName, lpszClassName; } WNDCLASSA;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; LONG ptx, pty; } MSG;

#define RGB(r,g,b) ((COLORREF)(((BYTE)(r))|((BYTE)(g)<<8)|((DWORD)(BYTE)(b)<<16)))

/* kernel32 */
__declspec(dllimport) HANDLE WINAPI CreateFileA(LPCSTR,DWORD,DWORD,LPVOID,DWORD,DWORD,HANDLE);
__declspec(dllimport) BOOL   WINAPI DeviceIoControl(HANDLE,DWORD,LPVOID,DWORD,LPVOID,DWORD,LPDWORD,LPVOID);
__declspec(dllimport) BOOL   WINAPI CloseHandle(HANDLE);
__declspec(dllimport) void   WINAPI ExitProcess(UINT);
__declspec(dllimport) HINSTANCE WINAPI GetModuleHandleA(LPCSTR);
__declspec(dllimport) DWORD  WINAPI GetLastError(void);
__declspec(dllimport) DWORD  WINAPI GetLogicalDrives(void);
__declspec(dllimport) DWORD  WINAPI GetVersion(void);
__declspec(dllimport) LPVOID WINAPI GetProcAddress(HINSTANCE,LPCSTR);
__declspec(dllimport) HICON  WINAPI LoadIconA(HINSTANCE,LPCSTR);
/* user32 */
__declspec(dllimport) WORD   WINAPI RegisterClassA(const WNDCLASSA*);
__declspec(dllimport) HWND   WINAPI CreateWindowExA(DWORD,LPCSTR,LPCSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,LPVOID);
__declspec(dllimport) LRESULT WINAPI DefWindowProcA(HWND,UINT,WPARAM,LPARAM);
__declspec(dllimport) BOOL   WINAPI ShowWindow(HWND,int);
__declspec(dllimport) BOOL   WINAPI UpdateWindow(HWND);
__declspec(dllimport) BOOL   WINAPI GetMessageA(MSG*,HWND,UINT,UINT);
__declspec(dllimport) BOOL   WINAPI TranslateMessage(const MSG*);
__declspec(dllimport) LRESULT WINAPI DispatchMessageA(const MSG*);
__declspec(dllimport) void   WINAPI PostQuitMessage(int);
__declspec(dllimport) LRESULT WINAPI SendMessageA(HWND,UINT,WPARAM,LPARAM);
__declspec(dllimport) BOOL   WINAPI PostMessageA(HWND,UINT,WPARAM,LPARAM);
__declspec(dllimport) HCURSOR WINAPI LoadCursorA(HINSTANCE,LPCSTR);
__declspec(dllimport) HCURSOR WINAPI SetCursor(HCURSOR);
__declspec(dllimport) int    WINAPI MessageBoxA(HWND,LPCSTR,LPCSTR,UINT);
__declspec(dllimport) HDC    WINAPI BeginPaint(HWND,PAINTSTRUCT*);
__declspec(dllimport) BOOL   WINAPI EndPaint(HWND,const PAINTSTRUCT*);
__declspec(dllimport) BOOL   WINAPI GetClientRect(HWND,LPRECT);
__declspec(dllimport) BOOL   WINAPI GetWindowRect(HWND,LPRECT);
__declspec(dllimport) BOOL   WINAPI AdjustWindowRectEx(LPRECT,DWORD,BOOL,DWORD);
__declspec(dllimport) BOOL   WINAPI InvalidateRect(HWND,const RECT*,BOOL);
__declspec(dllimport) int    WINAPI FillRect(HDC,const RECT*,HBRUSH);
__declspec(dllimport) int    WINAPI FrameRect(HDC,const RECT*,HBRUSH);
__declspec(dllimport) int    WINAPI GetWindowTextA(HWND,LPSTR,int);
__declspec(dllimport) BOOL   WINAPI SetWindowTextA(HWND,LPCSTR);
__declspec(dllimport) int    WINAPIV wsprintfA(LPSTR,LPCSTR,...);
__declspec(dllimport) BOOL   WINAPI IsDialogMessageA(HWND,MSG*);
__declspec(dllimport) BOOL   WINAPI EnableWindow(HWND,BOOL);
__declspec(dllimport) BOOL   WINAPI DestroyWindow(HWND);
__declspec(dllimport) HWND   WINAPI SetFocus(HWND);
__declspec(dllimport) HWND   WINAPI SetCapture(HWND);
__declspec(dllimport) BOOL   WINAPI ReleaseCapture(void);
__declspec(dllimport) UINT   WINAPI SetTimer(HWND,UINT,UINT,LPVOID);
__declspec(dllimport) BOOL   WINAPI KillTimer(HWND,UINT);
__declspec(dllimport) HMENU  WINAPI CreatePopupMenu(void);
__declspec(dllimport) BOOL   WINAPI AppendMenuA(HMENU,UINT,UINT,LPCSTR);
__declspec(dllimport) BOOL   WINAPI TrackPopupMenu(HMENU,UINT,int,int,int,HWND,const RECT*);
__declspec(dllimport) BOOL   WINAPI DestroyMenu(HMENU);
__declspec(dllimport) BOOL   WINAPI ClientToScreen(HWND,POINT*);
__declspec(dllimport) int    WINAPI GetSystemMetrics(int);
/* gdi32 */
__declspec(dllimport) HBRUSH WINAPI CreateSolidBrush(COLORREF);
__declspec(dllimport) HBRUSH WINAPI CreateHatchBrush(int,COLORREF);
__declspec(dllimport) HPEN   WINAPI CreatePen(int,int,COLORREF);
__declspec(dllimport) BOOL   WINAPI DeleteObject(HGDIOBJ);
__declspec(dllimport) HGDIOBJ WINAPI SelectObject(HDC,HGDIOBJ);
__declspec(dllimport) HGDIOBJ WINAPI GetStockObject(int);
__declspec(dllimport) COLORREF WINAPI SetTextColor(HDC,COLORREF);
__declspec(dllimport) COLORREF WINAPI SetBkColor(HDC,COLORREF);
__declspec(dllimport) int    WINAPI SetBkMode(HDC,int);
__declspec(dllimport) BOOL   WINAPI TextOutA(HDC,int,int,LPCSTR,int);
__declspec(dllimport) BOOL   WINAPI GetTextExtentPointA(HDC,LPCSTR,int,SIZE*);
__declspec(dllimport) HFONT  WINAPI CreateFontA(int,int,int,int,int,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,LPCSTR);
__declspec(dllimport) BOOL   WINAPI Ellipse(HDC,int,int,int,int);
__declspec(dllimport) BOOL   WINAPI RoundRect(HDC,int,int,int,int,int,int);
__declspec(dllimport) BOOL   WINAPI Polygon(HDC,const POINT*,int);
__declspec(dllimport) BOOL   WINAPI MoveToEx(HDC,int,int,POINT*);
__declspec(dllimport) BOOL   WINAPI LineTo(HDC,int,int);
__declspec(dllimport) BOOL   WINAPI Arc(HDC,int,int,int,int,int,int,int,int);
__declspec(dllimport) HDC    WINAPI CreateCompatibleDC(HDC);
__declspec(dllimport) HBITMAP WINAPI CreateCompatibleBitmap(HDC,int,int);
__declspec(dllimport) BOOL   WINAPI BitBlt(HDC,int,int,int,int,HDC,int,int,DWORD);
__declspec(dllimport) BOOL   WINAPI DeleteDC(HDC);
__declspec(dllimport) int    WINAPI GetDeviceCaps(HDC,int);

#include "../src/mgptioc.h"
#include "../src/gptcore.h"   /* GPT_FS_* kinds and their names */

/* ------------------------------------------------------------------ */
/* commands and icons                                                 */

#define CMD_NEW     401
#define CMD_DELETE  402
#define CMD_ACTIVE  403
#define CMD_TYPE    404
#define CMD_LETTER  405
#define CMD_CONVERT 406
#define CMD_INIT    407
#define CMD_REFRESH 408
#define CMD_DYNAMIC 409

#define IC_NEW     0
#define IC_DELETE  1
#define IC_ACTIVE  2
#define IC_TYPE    3
#define IC_LETTER  4
#define IC_CONVERT 5
#define IC_INIT    6
#define IC_REFRESH 7
#define IC_DISK    8
#define IC_DYNAMIC 9

static const struct { int cmd, icon, group; const char *label; } g_tbdef[] = {
    { CMD_NEW,     IC_NEW,     0, "New"          },
    { CMD_DELETE,  IC_DELETE,  0, "Delete"       },
    { CMD_ACTIVE,  IC_ACTIVE,  0, "Set Active"   },
    { CMD_TYPE,    IC_TYPE,    0, "Change Type"  },
    { CMD_LETTER,  IC_LETTER,  0, "Drive Letter" },
    { CMD_CONVERT, IC_CONVERT, 1, "Convert"      },
    { CMD_DYNAMIC, IC_DYNAMIC, 1, "Dynamic"      },
    { CMD_INIT,    IC_INIT,    1, "Initialize"   },
    { CMD_REFRESH, IC_REFRESH, 2, "Refresh"      },
};
#define NTB 9

/* ------------------------------------------------------------------ */
/* state                                                              */

static HANDLE    g_drv = INVALID_HANDLE_VALUE;
static HINSTANCE g_inst;
static HWND      g_main;
static HFONT     g_font, g_fontB, g_fontH;
static HBRUSH    g_brWhite;
static int       g_selDisk = -1, g_selPart = -1;
static char      g_status[160] = "";
static int       g_lowColor = 0;
static int       g_oldNt = 0;     /* NT 3.x / 4.0: no FAT32 support */

static MGPT_DISKLIST g_dl;
static MGPT_LAYOUT   g_lay[MGPT_MAX_DISKS];
static int           g_layOk[MGPT_MAX_DISKS];
static MGPT_LDMINFO  g_ldm;           /* dynamic disk groups */

static RECT g_tbr[NTB];
static int  g_hot = -1, g_down = -1;

static struct { int x0,y0,x1,y1; int disk; int part; } g_seg[512];
static int g_nseg;

static struct { int disk, part; } g_rows[MGPT_MAX_DISKS * (MGPT_MAX_PARTS + 1)];
static int g_nrows, g_rowTop, g_vis = 1;
static int g_lastSelDisk = -2, g_lastSelPart = -2;   /* selection the table last scrolled to */

/* table scroll bar: drawn by hand so it looks the same on every release */
#define SB_W 16
#define SB_NONE 0
#define SB_UP 1
#define SB_DOWN 2
#define SB_PGUP 3
#define SB_PGDN 4
#define SB_THUMB 5
static struct { int on, x0, y0, x1, y1, t0, t1; } g_sb;
static int g_sbAct = SB_NONE, g_sbDrag, g_sbMy;
#define SB_TIMER 7
#define GEN_TIMER 8   /* polls the driver's change counter */
#define DEV_TIMER 9   /* rescan shortly after WM_DEVICECHANGE */

/* ------------------------------------------------------------------ */
/* colors: crisp VGA-palette colors on 16/256-color displays, softer   */
/* modern tones on high-color displays                                 */

#define LC(lo,hi) (g_lowColor ? (lo) : (hi))
#define COL_BG      RGB(255,255,255)
#define COL_TB      LC(RGB(192,192,192),RGB(241,243,246))
#define COL_TBLINE  LC(RGB(128,128,128),RGB(212,217,224))
#define COL_TBHOT   LC(RGB(255,255,255),RGB(225,237,250))
#define COL_TBHOTF  LC(RGB(128,128,128),RGB(150,186,228))
#define COL_TBDOWN  LC(RGB(128,128,128),RGB(196,218,244))
#define COL_TEXT    LC(RGB(0,0,0),RGB(30,38,50))
#define COL_TEXT2   LC(RGB(128,128,128),RGB(98,108,122))
#define COL_DIS     LC(RGB(128,128,128),RGB(165,170,178))
#define COL_HDR     LC(RGB(192,192,192),RGB(236,239,243))
#define COL_GRID    LC(RGB(128,128,128),RGB(221,225,230))
#define COL_DISKROW LC(RGB(255,255,255),RGB(246,248,250))
#define COL_SEL     LC(RGB(0,0,128),RGB(204,228,247))
#define COL_SELTXT  LC(RGB(255,255,255),RGB(18,28,42))
#define COL_ACCENT  LC(RGB(0,0,128),RGB(36,104,192))
#define COL_STATUS  LC(RGB(192,192,192),RGB(241,243,246))
#define COL_DLGHDR  LC(RGB(255,255,255),RGB(245,248,251))
#define COL_DLGBAND LC(RGB(192,192,192),RGB(232,234,237))
#define COL_WARN    LC(RGB(128,0,0),RGB(178,38,38))
#define COL_SBTRK   LC(RGB(192,192,192),RGB(240,242,245))
#define COL_SBBTN   LC(RGB(192,192,192),RGB(240,242,245))
#define COL_SBARR   LC(RGB(0,0,0),RGB(110,120,134))
#define COL_SBTHM   LC(RGB(128,128,128),RGB(190,198,208))
#define COL_SBHOT   LC(RGB(0,0,128),RGB(150,162,178))

/* ------------------------------------------------------------------ */
/* small helpers (no CRT)                                             */

static int  slen(const char*s){int n=0;while(s[n])n++;return n;}
static int  scmp(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a!=*b;}
static void scopy(char*d,const char*s){while((*d++=*s++)!=0);}
static void scat(char*d,const char*s){while(*d)d++;scopy(d,s);}
static void sjoin(char*d,const char*s){if(d[0])scat(d,", ");scat(d,s);}

/* Sector numbers and counts stay 64-bit everywhere: a disk past 2 TiB has
   more than 2^32 sectors. The sector sizes are powers of two, so sectors
   become MB by a shift; the one real division (the disk map's scale) is
   udiv64, as the tools link no C run-time. */
static DWORD u64lo(mgpt_u64 v){ return (DWORD)(v&0xFFFFFFFFu); }
static int   mbShift(DWORD ss){ int k=20; while(ss>1&&k>0){ ss>>=1; k--; } return k; }
static DWORD perMB(DWORD ss){ return 1UL<<mbShift(ss); }
static mgpt_u64 toMB(mgpt_u64 sectors, DWORD ss){ return sectors>>mbShift(ss); }
static mgpt_u64 udiv64(mgpt_u64 n, mgpt_u64 d){
    mgpt_u64 q=0, r=0; int i;
    if(!d) return 0;
    for(i=63;i>=0;i--){ r=(r<<1)|((n>>i)&1); if(r>=d){ r-=d; q|=(mgpt_u64)1<<i; } }
    return q;
}
/* A size in the largest unit it fills at least once (1024 of the unit
   below): "512 KB", "500 MB", "1.4 GB", "2.7 TB", "12.7 TB", "1.5 PB" */
static void sizestr(mgpt_u64 sectors, DWORD ss, char*out){
    static const char*const un[6]={"KB","MB","GB","TB","PB","EB"};
    mgpt_u64 v=sectors; DWORD d=ss; int u=0;
    while(d<1024u){ v>>=1; d<<=1; } while(d>1024u){ v<<=1; d>>=1; }   /* KB */
    while(v>=(mgpt_u64)1<<20&&u<4){ v>>=10; u++; }
    if(v<1024) wsprintfA(out,"%lu %s",u64lo(v),un[u]);
    else if(u==0) wsprintfA(out,"%lu MB",u64lo(v>>10));
    else wsprintfA(out,"%lu.%lu %s",u64lo(v>>10),u64lo(((v&1023)*10)>>10),un[u+1]);
}
/* "Disk 2", or "CD-ROM 0" for a CD / DVD drive */
static void diskName(int dn,char*b){ if(dn&MGPT_CDROM) wsprintfA(b,"CD-ROM %d",dn&0xFF); else wsprintfA(b,"Disk %d",dn); }
/* a size in 512-byte units as text, for one use in a format string */
static const char*freeStr(mgpt_u64 sectors512){ static char b[24]; sizestr(sectors512,512,b); return b; }

/* the size units of the dialogs: 0 KB, 1 MB, 2 GB, 3 TB, 4 PB */
static const char*g_unitNames[5]={"KB","MB","GB","TB","PB"};
static mgpt_u64 unitBytes(DWORD value, int unit){ return (mgpt_u64)value<<(10*(unit+1)); }
/* value + unit -> sector count for a given sector size (a power of two) */
static mgpt_u64 unitToSectors(DWORD value, int unit, DWORD ss){
    return unitBytes(value,unit)>>(20-mbShift(ss));
}

/* file-system kinds (mirror of GPT_FS_* in gptcore.h) */
#define FS_UNKNOWN 0
#define FS_FAT12   1
#define FS_FAT16   2
#define FS_FAT32   3
#define FS_NTFS    4
#define FS_HPFS    5
#define FS_EXFAT   6
#define FS_EXT     7
#define FS_EFI     8
#define FS_LINUX   9
#define FS_EXT2    10
#define FS_EXT3    11
#define FS_EXT4    12
#define FS_REFS    13
#define FS_BTRFS   14
#define FS_MSR     15
#define FS_LDMMETA 16
#define FS_LDMDATA 17
#define FS_UDF     18
#define FS_KINDS   GPT_FS_KINDS

static const char* const g_fsNames[GPT_FS_KINDS]=GPT_FS_NAME_LIST;
static const char* fsname(DWORD k){ return k<GPT_FS_KINDS?g_fsNames[k]:"RAW"; }

static COLORREF fscolor(DWORD k){switch(k){
    case FS_FAT12: return RGB(124,176,228);   /* light blue  */
    case FS_FAT16: return RGB(54,118,176);    /* steel blue  */
    case FS_FAT32: return RGB(46,160,110);    /* green       */
    case FS_NTFS:  return RGB(116,96,214);    /* indigo      */
    case FS_EXFAT: return RGB(198,110,186);   /* magenta     */
    case FS_HPFS:  return RGB(72,160,156);    /* teal        */
    case FS_REFS:  return RGB(28,150,210);    /* azure       */
    case FS_UDF:   return RGB(150,166,40);    /* olive       */
    case FS_EXT2:  return RGB(226,132,96);    /* light coral */
    case FS_EXT3:  return RGB(204,90,64);     /* coral       */
    case FS_EXT4:  return RGB(168,52,44);     /* brick       */
    case FS_BTRFS: return RGB(176,140,40);    /* ochre       */
    case FS_LINUX: return RGB(150,96,72);     /* brown       */
    case FS_EFI:   return RGB(240,150,40);    /* orange      */
    case FS_EXT:   return RGB(150,160,175);   /* gray        */
    case FS_MSR:   return RGB(170,178,192);   /* light slate */
    case FS_LDMMETA: return RGB(84,92,108);   /* dark slate  */
    case FS_LDMDATA: return RGB(140,146,158); /* slate       */
    /* Linux: warm reds, oranges and browns */
    case GPT_FS_XFS:      return RGB(220,96,40);    /* orange red  */
    case GPT_FS_JFS:      return RGB(186,120,60);   /* copper      */
    case GPT_FS_REISERFS: return RGB(232,170,90);   /* sand        */
    case GPT_FS_REISER4:  return RGB(200,150,70);   /* tan         */
    case GPT_FS_F2FS:     return RGB(236,110,120);  /* salmon      */
    case GPT_FS_NILFS2:   return RGB(190,80,110);   /* raspberry   */
    case GPT_FS_BCACHEFS: return RGB(140,60,40);    /* rust        */
    case GPT_FS_GFS2:     return RGB(170,100,90);   /* clay        */
    case GPT_FS_OCFS2:    return RGB(210,140,120);  /* rose tan    */
    case GPT_FS_SQUASHFS: return RGB(250,190,120);  /* peach       */
    case GPT_FS_CRAMFS:   return RGB(230,200,150);  /* wheat       */
    case GPT_FS_EROFS:    return RGB(240,160,60);   /* amber       */
    case GPT_FS_ROMFS:    return RGB(214,180,120);  /* buff        */
    case GPT_FS_JFFS2:    return RGB(196,110,70);   /* sienna      */
    case GPT_FS_MINIX:    return RGB(160,120,100);  /* taupe       */
    case GPT_FS_EXTFS:    return RGB(240,170,150);  /* pale coral  */
    case GPT_FS_XIAFS:    return RGB(180,140,130);  /* dusty rose  */
    /* containers and encryption: dark tones */
    case GPT_FS_SWAP:     return RGB(96,96,110);    /* graphite    */
    case GPT_FS_BSDSWAP:  return RGB(110,104,90);   /* dark khaki  */
    case GPT_FS_LUKS:     return RGB(60,70,90);     /* night blue  */
    case GPT_FS_BITLOCKER:return RGB(70,60,110);    /* deep violet */
    case GPT_FS_CORESTOR: return RGB(80,90,100);    /* dark steel  */
    case GPT_FS_LVM2:     return RGB(120,70,60);    /* dark brick  */
    case GPT_FS_MDRAID:   return RGB(110,60,100);   /* plum        */
    /* Apple: silvers and blue-greys */
    case GPT_FS_HFS:      return RGB(170,180,190);  /* silver      */
    case GPT_FS_HFSPLUS:  return RGB(120,140,160);  /* blue grey   */
    case GPT_FS_HFSX:     return RGB(100,120,150);  /* slate blue  */
    case GPT_FS_APFS:     return RGB(80,110,140);   /* steel       */
    case GPT_FS_MFS:      return RGB(190,196,200);  /* pale silver */
    case GPT_FS_APPLEBOOT:return RGB(150,160,170);  /* grey        */
    /* BSD and Unix: yellows, olives and teals */
    case GPT_FS_UFS:      return RGB(200,180,60);   /* mustard     */
    case GPT_FS_UFS2:     return RGB(170,150,30);   /* dark gold   */
    case GPT_FS_ZFS:      return RGB(40,120,120);   /* deep teal   */
    case GPT_FS_HAMMER:   return RGB(120,140,60);   /* moss        */
    case GPT_FS_HAMMER2:  return RGB(100,120,40);   /* dark moss   */
    case GPT_FS_SYSV:     return RGB(150,150,90);   /* khaki       */
    case GPT_FS_XENIX:    return RGB(170,170,110);  /* light khaki */
    case GPT_FS_UNIXBFS:  return RGB(130,130,80);   /* dark khaki  */
    case GPT_FS_EFS:      return RGB(60,150,170);   /* sea blue    */
    case GPT_FS_VXFS:     return RGB(90,170,140);   /* jade        */
    case GPT_FS_QNX6:     return RGB(60,140,90);    /* forest      */
    case GPT_FS_VMFS:     return RGB(110,170,200);  /* sky         */
    /* others */
    case GPT_FS_ISO9660:  return RGB(220,190,40);   /* gold        */
    case GPT_FS_BEFS:     return RGB(150,90,200);   /* purple      */
    case GPT_FS_AMIGA:    return RGB(230,80,60);    /* tomato      */
    case GPT_FS_BIOSBOOT: return RGB(220,120,30);   /* dark orange */
    default:       return RGB(128,133,143);   /* RAW         */
}}

static int isExtType(DWORD t){return t==0x05||t==0x0F;}

/* partition types offered in the type lists */
static const struct { DWORD t; const char *n; } g_types[] = {
    { 0x01, "01 - FAT12" },
    { 0x04, "04 - FAT16 (under 32 MB)" },
    { 0x06, "06 - FAT16" },
    { 0x0E, "0E - FAT16 (LBA)" },
    { 0x0B, "0B - FAT32" },
    { 0x0C, "0C - FAT32 (LBA)" },
    { 0x07, "07 - NTFS / HPFS / exFAT" },
    { 0x05, "05 - Extended" },
    { 0x0F, "0F - Extended (LBA)" },
    { 0x83, "83 - Linux" },
    { 0xEF, "EF - EFI System" },
};
#define NTYPES 11

static void typeDesc(DWORD t, char*out){
    int i;
    for(i=0;i<NTYPES;i++) if(g_types[i].t==t){ scopy(out,g_types[i].n); return; }
    wsprintfA(out,"%02X - Other",t);
}

/* ------------------------------------------------------------------ */
/* model                                                              */

static int diskIdx(int dn){DWORD i;for(i=0;i<g_dl.Count;i++)if((int)g_dl.Disk[i].DiskNumber==dn)return (int)i;return -1;}
static MGPT_LAYOUT*layOf(int dn){int i=diskIdx(dn);if(i<0||!g_layOk[i])return NULL;return &g_lay[i];}
static MGPT_DISKINFO*infoOf(int dn){int i=diskIdx(dn);return i<0?NULL:&g_dl.Disk[i];}
static MGPT_PART*selPart(void){
    MGPT_LAYOUT*l=layOf(g_selDisk);
    if(!l||g_selPart<0||(DWORD)g_selPart>=l->Count)return NULL;
    return &l->Part[g_selPart];
}
static int diskIsSystem(MGPT_LAYOUT*l){DWORD i;for(i=0;i<l->Count;i++)if(l->Part[i].Flags&MGPT_F_PROTECTED)return 1;return 0;}

/* ---- dynamic disks ---- */
static MGPT_LDMGROUP*grpOf(MGPT_LAYOUT*l){
    if(!l||!l->Dynamic||l->LdmGroup>=g_ldm.Count)return NULL;
    return &g_ldm.Group[l->LdmGroup];
}
static MGPT_LDMVOL*volOf(MGPT_PART*p){
    MGPT_LDMGROUP*g;
    if(!p||!(p->Flags&MGPT_F_LDM)||p->LdmGroup>=g_ldm.Count)return NULL;
    g=&g_ldm.Group[p->LdmGroup];
    return p->LdmVol<g->NVols?&g->Vol[p->LdmVol]:NULL;
}
static const char*ldmTypeName(DWORD t){switch(t){
    case MGPT_LDM_SIMPLE:return "Simple";case MGPT_LDM_SPANNED:return "Spanned";case MGPT_LDM_STRIPED:return "Striped";
    case MGPT_LDM_MIRROR:return "Mirror";case MGPT_LDM_RAID5:return "RAID-5";default:return "Volume";}}
static const char*ldmStateName(DWORD s){switch(s){
    case MGPT_LDMS_HEALTHY:return "Healthy";case MGPT_LDMS_DEGRADED:return "Degraded";default:return "Failed";}}
/* the LDM disk index of NT disk dn in group g, or -1 */
static int ldmDiskOf(MGPT_LDMGROUP*g,int dn){DWORD i;for(i=0;i<g->NDisks&&i<MGPT_LDM_DISKS;i++)if((int)g->Disk[i].NtDisk==dn)return (int)i;return -1;}
static int isDynamic(MGPT_LAYOUT*l){return l&&l->Dynamic;}
/* the NT disks holding extents of volume v of group gi, as "Disk 1, Disk 2" */
static void volDisks(DWORD gi,DWORD v,char*out){
    DWORD i,j; char t[16]; out[0]=0;
    for(i=0;i<g_dl.Count;i++){
        MGPT_LAYOUT*l=&g_lay[i]; int has=0;
        if(!g_layOk[i]||!l->Dynamic)continue;
        for(j=0;j<l->Count;j++) if((l->Part[j].Flags&MGPT_F_LDM)&&l->Part[j].LdmGroup==gi&&l->Part[j].LdmVol==v) has=1;
        if(has){ wsprintfA(t,"Disk %lu",l->DiskNumber); sjoin(out,t); }
    }
    /* "Disk 1, Disk 2, Disk 3" -> "Disk 1, Disk 2 and Disk 3" */
    { int k, last=-1; for(k=0;out[k];k++) if(out[k]==','&&out[k+1]==' ') last=k;
      if(last>=0){ char tail[80]; scopy(tail,out+last+2); out[last]=0; scat(out," and "); scat(out,tail); } }
}
static const char*ldmTypeWord(DWORD t){switch(t){
    case MGPT_LDM_SIMPLE:return "simple";case MGPT_LDM_SPANNED:return "spanned";case MGPT_LDM_STRIPED:return "striped";
    case MGPT_LDM_MIRROR:return "mirrored";case MGPT_LDM_RAID5:return "RAID-5";default:return "dynamic";}}

/* exact MBR type byte for a FAT16 / FAT32 / extended partition - mirrors
   GptPickType in the driver */
static DWORD typeFamily(DWORD t){
    if(t==0x04||t==0x06||t==0x0E) return MGPT_TYPE_FAT16;
    if(t==0x0B||t==0x0C) return MGPT_TYPE_FAT32;
    if(t==0x05||t==0x0F) return MGPT_TYPE_EXTENDED;
    return 0;
}
static int pastChs(MGPT_LAYOUT*l,mgpt_u64 start,mgpt_u64 count){ return count && start+count-1>=l->ChsLimit; }
static DWORD pickType(MGPT_LAYOUT*l,DWORD fam,mgpt_u64 start,mgpt_u64 count){
    int beyond=pastChs(l,start,count);
    if(fam==MGPT_TYPE_FAT16){ if(beyond)return 0x0E; return (count<65536)?0x04:0x06; }
    if(fam==MGPT_TYPE_FAT32) return beyond?0x0C:0x0B;
    return beyond?0x0F:0x05;
}
/* one sentence saying why a partition has its type byte */
static void typeReason(MGPT_LAYOUT*l,MGPT_PART*p,char*out,const char*v){
    char lim[24]; int beyond=pastChs(l,p->StartLba,p->SectorCount);
    sizestr(l->ChsLimit,l->SectorSize,lim);
    switch(p->MbrType){
    case 0x04: wsprintfA(out,"Type 04 (FAT16)%s because the partition is smaller than 32 MB.",v); break;
    case 0x06:
        if(beyond) wsprintfA(out,"Type 06 (FAT16) is set although the partition reaches past the BIOS CHS range (the first %s of this disk); 0E would be the right type.",lim);
        else wsprintfA(out,"Type 06 (FAT16)%s because the partition is 32 MB or larger and lies inside the BIOS CHS range (the first %s of this disk).",v,lim);
        break;
    case 0x0E: wsprintfA(out,"Type 0E (FAT16, LBA)%s because the partition reaches past the BIOS CHS range (the first %s of this disk).",v,lim); break;
    case 0x0B: wsprintfA(out,"Type 0B (FAT32)%s because the partition lies inside the BIOS CHS range (the first %s of this disk).",v,lim); break;
    case 0x0C: wsprintfA(out,"Type 0C (FAT32, LBA)%s because the partition reaches past the BIOS CHS range (the first %s of this disk).",v,lim); break;
    case 0x05:
        if(beyond) wsprintfA(out,"Type 05 (extended) is set although the container reaches past the BIOS CHS range (the first %s of this disk); 0F would be the right type.",lim);
        else wsprintfA(out,"Type 05 (extended)%s because the container lies inside the BIOS CHS range (the first %s of this disk).",v,lim);
        break;
    case 0x0F: wsprintfA(out,"Type 0F (extended, LBA)%s because the container reaches past the BIOS CHS range (the first %s of this disk).",v,lim); break;
    default: wsprintfA(out,"Type %02X.",p->MbrType); break;
    }
}

static DWORD g_gen;          /* driver change counter the model was loaded at */
static int   g_busy;         /* a command (dialog / result box) is running */

static void loadModel(void){
    DWORD ret,i;
    g_dl.Count=0;
    if(g_drv==INVALID_HANDLE_VALUE)return;
    if(!DeviceIoControl(g_drv,IOCTL_MGPT_GETGEN,NULL,0,&g_gen,sizeof(g_gen),&ret,NULL))g_gen=0;
    if(!DeviceIoControl(g_drv,IOCTL_MGPT_LIST,NULL,0,&g_dl,sizeof(g_dl),&ret,NULL)){g_dl.Count=0;return;}
    if(!DeviceIoControl(g_drv,IOCTL_MGPT_LDMINFO,NULL,0,&g_ldm,sizeof(g_ldm),&ret,NULL))g_ldm.Count=0;
    for(i=0;i<g_dl.Count&&i<MGPT_MAX_DISKS;i++){
        DWORD dn=g_dl.Disk[i].DiskNumber;
        g_layOk[i]=DeviceIoControl(g_drv,IOCTL_MGPT_GETLAYOUT,&dn,sizeof(dn),&g_lay[i],sizeof(g_lay[i]),&ret,NULL)?1:0;
    }
}

static void selectDefault(void){
    DWORD i; int chosen=-1; MGPT_LAYOUT*l;
    g_selDisk=-1; g_selPart=-1;
    if(g_dl.Count==0)return;
    for(i=0;i<g_dl.Count;i++) if(g_dl.Disk[i].Owned){chosen=(int)g_dl.Disk[i].DiskNumber;break;}
    if(chosen<0)chosen=(int)g_dl.Disk[0].DiskNumber;
    g_selDisk=chosen;
    l=layOf(chosen);
    g_selPart=(l&&l->Count>0)?0:-1;
}

static void fixSelection(void){
    MGPT_LAYOUT*l=layOf(g_selDisk);
    if(!l){ selectDefault(); return; }
    if(g_selPart>=(int)l->Count) g_selPart=(int)l->Count-1;
}

/* name a partition the way the table shows it */
static void partLabel(MGPT_LAYOUT*l,int idx,char*b){
    int j,pr=0,lg=0; MGPT_PART*p=&l->Part[idx];
    if(l->Media){ scopy(b,"Disc"); return; }
    if(l->Reach&MGPT_R_WHOLE){ scopy(b,"Whole disk"); return; }
    if(p->Flags&MGPT_F_LDM){
        switch(p->LdmType){
        case MGPT_LDM_SPANNED: wsprintfA(b,"%s (part %lu of %lu)",p->VolName,p->LdmPiece+1,p->LdmPieces); break;
        case MGPT_LDM_STRIPED: case MGPT_LDM_RAID5: wsprintfA(b,"%s (column %lu of %lu)",p->VolName,p->LdmPiece+1,p->LdmPieces); break;
        case MGPT_LDM_MIRROR: wsprintfA(b,"%s (copy %lu of %lu)",p->VolName,p->LdmPiece+1,p->LdmPieces); break;
        default: scopy(b,p->VolName); break;
        }
        return;
    }
    if(p->FsKind==FS_MSR){ scopy(b,"Reserved partition"); return; }
    if(p->FsKind==FS_LDMMETA){ scopy(b,"LDM database"); return; }
    if(p->FsKind==FS_LDMDATA){ scopy(b,l->Dynamic?"Dynamic area (unread)":"LDM data"); return; }
    for(j=0;j<=idx;j++){p=&l->Part[j];if(isExtType(p->MbrType))continue;if(p->Flags&MGPT_F_LOGICAL)lg++;else pr++;}
    p=&l->Part[idx];
    if(isExtType(p->MbrType)) scopy(b,"Extended partition");
    else if(p->Flags&MGPT_F_LOGICAL) wsprintfA(b,"Logical drive %d",lg);
    else wsprintfA(b,"Partition %d",pr);
}
/* the disk driver below reaches only part of the disk (2 TiB limit) */
static void reachNote(MGPT_DISKINFO*di,char*b){
    char sz[24], t[120];
    if(di->Reach&MGPT_R_WHOLE) scat(b,"  \xB7  one file system over the whole disk, shown only");
    if(di->Reach&MGPT_R_EXTENDED){
        sizestr(di->DriverSectors,di->SectorSize,sz);
        wsprintfA(t,"  \xB7  past %s through MBRGPTNT's own 16-byte commands",sz); scat(b,t);
    }
    else if(di->DeviceSectors>di->TotalSectors&&di->TotalSectors){
        char s2[24]; sizestr(di->DeviceSectors,di->SectorSize,sz); sizestr(di->TotalSectors,di->SectorSize,s2);
        wsprintfA(t,"  \xB7  disk is %s, this Windows reaches only the first %s",sz,s2); scat(b,t);
    }
    else if(di->TotalSectors==0&&(di->Reach&MGPT_R_CAPPED)){
        scat(b,"  \xB7  past the 2 TiB limit: the disk driver reports no size");
        if(di->Reach&MGPT_R_TABLE){ sizestr(di->TableSectors,di->SectorSize,sz); wsprintfA(t," (disk is %s, read-only)",sz); scat(b,t); }
    }
    else if(di->Reach&MGPT_R_TABLE){ sizestr(di->TableSectors,di->SectorSize,sz); wsprintfA(t,"  \xB7  disk is %s, this Windows reaches only the first part (read-only)",sz); scat(b,t); }
    else if(di->Reach&MGPT_R_CAPPED) scat(b,"  \xB7  2 TiB disk-driver limit: the disk may be larger");
}
static void partStatus(MGPT_LAYOUT*l,MGPT_PART*p,char*b){
    b[0]=0;
    if(p->Flags&MGPT_F_LDM){
        char sz[24];
        scopy(b,ldmTypeName(p->LdmType));
        if(p->LdmPieces>1){ sizestr(p->LdmVolSize,512,sz); scat(b," "); scat(b,sz); }
        sjoin(b,ldmStateName(p->LdmState));
        if(p->LdmState==MGPT_LDMS_DEGRADED) scat(b," (read-only)");
        return;
    }
    if(p->FsKind==FS_MSR||p->FsKind==FS_LDMMETA||p->FsKind==FS_LDMDATA){ scopy(b,l->IsGpt?"GPT":"Primary"); sjoin(b,"Reserved"); return; }
    if(l->Media){ scopy(b,"CD / DVD (shown only)"); return; }
    if(l->Reach&MGPT_R_WHOLE){ scopy(b,"No partition table (shown only)"); return; }
    if(p->Flags&MGPT_F_BEYOND) sjoin(b,"Past the reachable end, not mounted");
    if(p->Flags&MGPT_F_PROTECTED) sjoin(b,"System");
    if(p->Flags&MGPT_F_BOOT) sjoin(b,"Active");
    if(isExtType(p->MbrType)) sjoin(b,"Container");
    else if(p->Flags&MGPT_F_LOGICAL) sjoin(b,"Logical");
    else sjoin(b,l->IsGpt?"GPT":"Primary");
}
/* "Partition 2 (E:)" */
static void partTitle(MGPT_LAYOUT*l,int idx,char*b){
    char t[8];
    partLabel(l,idx,b);
    if(l->Part[idx].DriveLetter){ wsprintfA(t," (%c:)",(char)l->Part[idx].DriveLetter); scat(b,t); }
}

/* largest free area (sectors) the driver could place a new partition in;
   mirrors the gap search in gptcore (2048-sector alignment, EBR reserve) */
static mgpt_u64 alignUp(mgpt_u64 v){return (v+2047u)&~(mgpt_u64)2047u;}
static mgpt_u64 gapScan(MGPT_LAYOUT*l,mgpt_u64 lo,mgpt_u64 hi,int inner){
    mgpt_u64 cur=alignUp(inner?lo+2048u:lo),best=0; DWORD i;
    while(cur<=hi){
        mgpt_u64 next=hi+1; int inside=0;
        for(i=0;i<l->Count;i++){
            MGPT_PART*p=&l->Part[i]; mgpt_u64 a,b;
            if(((p->Flags&MGPT_F_LOGICAL)?1:0)!=inner)continue;
            a=p->StartLba; b=a+p->SectorCount-1; if(inner&&a>=2048u)a-=2048u;
            if(cur>=a&&cur<=b){cur=alignUp(b+1+(inner?2048u:0));inside=1;break;}
        }
        if(inside)continue;
        for(i=0;i<l->Count;i++){
            MGPT_PART*p=&l->Part[i]; mgpt_u64 a;
            if(((p->Flags&MGPT_F_LOGICAL)?1:0)!=inner)continue;
            a=p->StartLba; if(inner&&a>=2048u)a-=2048u;
            if(a>cur&&a<next)next=a;
        }
        { mgpt_u64 ge=(next<=hi)?next-1:hi; if(ge>=cur&&ge-cur+1>best)best=ge-cur+1; }
        if(next>hi)break;
        cur=alignUp(next+(inner?2048u:0));
    }
    return best;
}
/* MBR entries hold 32-bit sector numbers: an MBR disk is used up to
   sector 2^32 - 1 (2 TiB with 512-byte sectors), as in the driver */
static mgpt_u64 largestFree(MGPT_LAYOUT*l){
    mgpt_u64 total=l->TotalSectors, first, last, best; DWORD ss=l->SectorSize, es=(16384u+ss-1)/ss, i;
    if(l->IsGpt){ if(total<2*es+8)return 0; first=2+es; last=total-2-es; }
    else { if(total<4096)return 0; first=2048; last=total-1; if(last>0xFFFFFFFFu)last=0xFFFFFFFFu; }
    best=gapScan(l,first,last,0);
    if(!l->IsGpt)
        for(i=0;i<l->Count;i++){
            MGPT_PART*p=&l->Part[i];
            if(!(p->Flags&MGPT_F_LOGICAL)&&isExtType(p->MbrType)){
                mgpt_u64 es2=p->StartLba, ee=es2+p->SectorCount-1, g=gapScan(l,es2,ee,1);
                if(g>best)best=g;
            }
        }
    return best;
}

static int cmdEnabled(int cmd){
    MGPT_LAYOUT*l; MGPT_PART*p;
    if(cmd==CMD_REFRESH)return 1;
    if(g_drv==INVALID_HANDLE_VALUE)return 0;
    l=layOf(g_selDisk); if(!l)return 0;
    p=selPart();
    if(l->Dynamic){
        MGPT_LDMGROUP*g=grpOf(l); int ldm=p&&(p->Flags&MGPT_F_LDM);
        switch(cmd){
        case CMD_NEW:     return g&&g->Writable;
        case CMD_DELETE:  return ldm&&g&&g->Writable;
        case CMD_LETTER:  return ldm&&g&&!g->Native&&p->LdmState!=MGPT_LDMS_FAILED;
        case CMD_DYNAMIC: { DWORD i; if(!g||g->Native)return 0; for(i=0;i<l->Count;i++)if(l->Part[i].Flags&MGPT_F_LDM)return 0; return 1; }
        case CMD_INIT:    return !diskIsSystem(l);
        }
        return 0;
    }
    /* a table this Windows does not reach to its end is only shown */
    if((l->Reach&MGPT_R_TABLE)&&cmd!=CMD_LETTER&&cmd!=CMD_INIT) return 0;
    if(l->Media) return 0;                  /* CD / DVD drives are only shown */
    /* a whole-disk file system: only Initialize (which replaces it) */
    if((l->Reach&MGPT_R_WHOLE)&&cmd!=CMD_INIT) return 0;
    switch(cmd){
    case CMD_NEW:     return 1;
    case CMD_DYNAMIC:
#if defined(_WIN64)
        return 0;                         /* x64 Windows manages dynamic disks itself */
#else
        if(l->SectorSize!=512||diskIsSystem(l))return 0;
        return l->IsGpt||g_oldNt;          /* Windows 2000+ converts MBR disks itself */
#endif
    case CMD_CONVERT:
    case CMD_INIT:    return !diskIsSystem(l);
    case CMD_DELETE:  return p && !(p->Flags&MGPT_F_PROTECTED);
    case CMD_TYPE:    return p && !(p->Flags&MGPT_F_PROTECTED) && !isExtType(p->MbrType);
    case CMD_LETTER:  return p && !(p->Flags&(MGPT_F_PROTECTED|MGPT_F_BEYOND)) && !isExtType(p->MbrType);
    case CMD_ACTIVE:  return p && !(p->Flags&MGPT_F_PROTECTED) && !(p->Flags&MGPT_F_LOGICAL) && !isExtType(p->MbrType);
    }
    return 0;
}
static const char*convertLabel(void){
    MGPT_LAYOUT*l=layOf(g_selDisk);
    if(!l)return "Convert";
    return l->IsGpt?"To MBR":"To GPT";
}
static const char*dynamicLabel(void){
    MGPT_LAYOUT*l=layOf(g_selDisk);
    return isDynamic(l)?"To Basic":"To Dynamic";
}

/* ------------------------------------------------------------------ */
/* drawing primitives                                                 */

static void fillR(HDC dc,int l,int t,int r,int b,COLORREF c){RECT q;HBRUSH h;q.left=l;q.top=t;q.right=r;q.bottom=b;h=CreateSolidBrush(c);FillRect(dc,&q,h);DeleteObject(h);}
static void frameR(HDC dc,int l,int t,int r,int b,COLORREF c){RECT q;HBRUSH h;q.left=l;q.top=t;q.right=r;q.bottom=b;h=CreateSolidBrush(c);FrameRect(dc,&q,h);DeleteObject(h);}
static void hline(HDC dc,int x0,int x1,int y,COLORREF c){fillR(dc,x0,y,x1,y+1,c);}
static int  textW(HDC dc,const char*s,HFONT f){SIZE z;SelectObject(dc,f);GetTextExtentPointA(dc,s,slen(s),&z);return (int)z.cx;}
static void text(HDC dc,int x,int y,const char*s,COLORREF c,HFONT f){SetBkMode(dc,TRANSPARENT);SetTextColor(dc,c);SelectObject(dc,f);TextOutA(dc,x,y,s,slen(s));}
/* draw text clipped to maxw pixels, ending with "..." when it does not fit */
static void textFit(HDC dc,int x,int y,int maxw,const char*s,COLORREF c,HFONT f){
    char b[200]; int n=slen(s);
    if(maxw<=0)return;
    if(textW(dc,s,f)<=maxw){text(dc,x,y,s,c,f);return;}
    if(n>190)n=190;
    while(n>0){int k;for(k=0;k<n;k++)b[k]=s[k];b[n]='.';b[n+1]='.';b[n+2]='.';b[n+3]=0;if(textW(dc,b,f)<=maxw)break;n--;}
    if(n>0)text(dc,x,y,b,c,f);
}

/* ---- icons: drawn with GDI on a 28x28 grid, scaled to any size ---- */
static int g_ix, g_iy, g_isz, g_icOn;
#define S(v)  (((v)*g_isz)/28)
#define IX(v) (g_ix+S(v))
#define IY(v) (g_iy+S(v))

static COLORREF icc(COLORREF c){
    int r,g,b,l;
    if(g_icOn)return c;
    r=(int)(c&255); g=(int)((c>>8)&255); b=(int)((c>>16)&255);
    l=(r*30+g*59+b*11)/100; l=150+(l*85)/255; if(l>236)l=236;
    return RGB(l,l,l);
}
static void sb(HDC dc,COLORREF pc,int pw,COLORREF bc,int nob,HGDIOBJ*op,HGDIOBJ*ob){
    HPEN p=CreatePen(0,pw,icc(pc));
    HGDIOBJ b=nob?GetStockObject(NULL_BRUSH):(HGDIOBJ)CreateSolidBrush(icc(bc));
    *op=SelectObject(dc,p); *ob=SelectObject(dc,b);
}
static void se(HDC dc,HGDIOBJ op,HGDIOBJ ob,int nob){
    HGDIOBJ p=SelectObject(dc,op), b=SelectObject(dc,ob);
    DeleteObject(p); if(!nob)DeleteObject(b);
}
static int pw2(int w){int v=S(w);return v<2?2:v;}
static void ln(HDC dc,int x0,int y0,int x1,int y1,COLORREF c,int w){
    HGDIOBJ op,ob; sb(dc,c,w,0,1,&op,&ob); MoveToEx(dc,x0,y0,NULL); LineTo(dc,x1,y1); se(dc,op,ob,1);
}
static void tri(HDC dc,int ax,int ay,int bx,int by,int cx,int cy,COLORREF f){
    POINT p[3]; HGDIOBJ op,ob;
    p[0].x=ax;p[0].y=ay;p[1].x=bx;p[1].y=by;p[2].x=cx;p[2].y=cy;
    sb(dc,f,1,f,0,&op,&ob); Polygon(dc,p,3); se(dc,op,ob,0);
}
static void icHdd(HDC dc){
    HGDIOBJ op,ob;
    sb(dc,RGB(70,80,95),1,RGB(196,202,212),0,&op,&ob);
    RoundRect(dc,IX(1),IY(8),IX(27),IY(22),S(6),S(6)); se(dc,op,ob,0);
    ln(dc,IX(5),IY(15),IX(15),IY(15),RGB(110,118,130),1);
    sb(dc,RGB(20,110,40),1,RGB(70,200,90),0,&op,&ob);
    Ellipse(dc,IX(19),IY(13),IX(24),IY(18)); se(dc,op,ob,0);
}
static void icBadge(HDC dc,COLORREF bg,int plus){
    HGDIOBJ op,ob; int w=pw2(2);
    sb(dc,RGB(255,255,255),1,bg,0,&op,&ob);
    Ellipse(dc,IX(13),IY(13),IX(28),IY(28)); se(dc,op,ob,0);
    if(plus){
        ln(dc,IX(20),IY(16),IX(20),IY(25),RGB(255,255,255),w);
        ln(dc,IX(16),IY(20),IX(25),IY(20),RGB(255,255,255),w);
    } else {
        ln(dc,IX(17),IY(17),IX(24),IY(24),RGB(255,255,255),w);
        ln(dc,IX(24),IY(17),IX(17),IY(24),RGB(255,255,255),w);
    }
}
static void drawIcon(HDC dc,int ic,int x,int y,int sz,int on){
    HGDIOBJ op,ob;
    g_ix=x; g_iy=y; g_isz=sz; g_icOn=on;
    switch(ic){
    case IC_NEW:    icHdd(dc); icBadge(dc,RGB(34,150,70),1); break;
    case IC_DELETE: icHdd(dc); icBadge(dc,RGB(210,48,48),0); break;
    case IC_DISK:   icHdd(dc); break;
    case IC_DYNAMIC:
        /* two disks joined into one volume */
        sb(dc,RGB(70,80,95),1,RGB(196,202,212),0,&op,&ob); RoundRect(dc,IX(1),IY(2),IX(19),IY(12),S(4),S(4)); se(dc,op,ob,0);
        sb(dc,RGB(70,80,95),1,RGB(196,202,212),0,&op,&ob); RoundRect(dc,IX(9),IY(16),IX(27),IY(26),S(4),S(4)); se(dc,op,ob,0);
        sb(dc,RGB(20,60,140),1,RGB(52,112,206),0,&op,&ob); RoundRect(dc,IX(4),IY(5),IX(16),IY(9),S(2),S(2)); se(dc,op,ob,0);
        sb(dc,RGB(20,60,140),1,RGB(52,112,206),0,&op,&ob); RoundRect(dc,IX(12),IY(19),IX(24),IY(23),S(2),S(2)); se(dc,op,ob,0);
        ln(dc,IX(10),IY(12),IX(10),IY(16),RGB(52,112,206),pw2(2));
        ln(dc,IX(18),IY(12),IX(18),IY(16),RGB(52,112,206),pw2(2));
        break;
    case IC_ACTIVE:
        ln(dc,IX(7),IY(3),IX(7),IY(26),RGB(70,80,95),pw2(2));
        { POINT p[3]; p[0].x=IX(8);p[0].y=IY(4);p[1].x=IX(26);p[1].y=IY(10);p[2].x=IX(8);p[2].y=IY(16);
          sb(dc,RGB(20,110,40),1,RGB(46,170,80),0,&op,&ob); Polygon(dc,p,3); se(dc,op,ob,0); }
        ln(dc,IX(3),IY(26),IX(13),IY(26),RGB(70,80,95),pw2(2));
        break;
    case IC_TYPE:
        { POINT p[5]; p[0].x=IX(2);p[0].y=IY(7);p[1].x=IX(17);p[1].y=IY(7);p[2].x=IX(26);p[2].y=IY(14);
          p[3].x=IX(17);p[3].y=IY(21);p[4].x=IX(2);p[4].y=IY(21);
          sb(dc,RGB(20,60,140),1,RGB(52,112,206),0,&op,&ob); Polygon(dc,p,5); se(dc,op,ob,0); }
        sb(dc,RGB(255,255,255),1,RGB(255,255,255),0,&op,&ob); Ellipse(dc,IX(17),IY(12),IX(21),IY(16)); se(dc,op,ob,0);
        ln(dc,IX(5),IY(12),IX(13),IY(12),RGB(255,255,255),1);
        ln(dc,IX(5),IY(16),IX(11),IY(16),RGB(255,255,255),1);
        break;
    case IC_LETTER:
        sb(dc,RGB(70,80,95),1,RGB(255,255,255),0,&op,&ob);
        RoundRect(dc,IX(1),IY(4),IX(27),IY(24),S(6),S(6)); se(dc,op,ob,0);
        if(sz>=24){ int w=textW(dc,"C:",g_fontB); text(dc,IX(14)-w/2,IY(7),"C:",icc(RGB(30,90,180)),g_fontB); }
        break;
    case IC_CONVERT:
        ln(dc,IX(3),IY(9),IX(19),IY(9),RGB(224,124,24),pw2(3));
        tri(dc,IX(18),IY(3),IX(26),IY(9),IX(18),IY(15),RGB(224,124,24));
        ln(dc,IX(9),IY(20),IX(25),IY(20),RGB(40,104,196),pw2(3));
        tri(dc,IX(10),IY(14),IX(2),IY(20),IX(10),IY(26),RGB(40,104,196));
        break;
    case IC_INIT:
        sb(dc,RGB(70,80,95),1,RGB(196,202,212),0,&op,&ob); Ellipse(dc,IX(2),IY(2),IX(27),IY(27)); se(dc,op,ob,0);
        sb(dc,RGB(140,148,160),1,0,1,&op,&ob); Ellipse(dc,IX(7),IY(7),IX(22),IY(22)); se(dc,op,ob,1);
        sb(dc,RGB(40,48,60),1,RGB(60,68,80),0,&op,&ob); Ellipse(dc,IX(12),IY(12),IX(17),IY(17)); se(dc,op,ob,0);
        ln(dc,IX(25),IY(3),IX(16),IY(13),RGB(40,48,60),pw2(2));
        break;
    case IC_REFRESH:
        sb(dc,RGB(34,140,70),pw2(3),0,1,&op,&ob);
        Arc(dc,IX(4),IY(4),IX(24),IY(24),IX(17),IY(3),IX(25),IY(11)); se(dc,op,ob,1);
        tri(dc,IX(18),IY(11),IX(28),IY(11),IX(23),IY(3),RGB(34,140,70));
        break;
    }
}

/* ------------------------------------------------------------------ */
/* main window painting                                               */

static void addSeg(int x0,int y0,int x1,int y1,int disk,int part){
    if(g_nseg>=512)return;
    g_seg[g_nseg].x0=x0;g_seg[g_nseg].y0=y0;g_seg[g_nseg].x1=x1;g_seg[g_nseg].y1=y1;
    g_seg[g_nseg].disk=disk;g_seg[g_nseg].part=part;g_nseg++;
}
static int hitTest(int px,int py,int*disk,int*part){
    int i;
    for(i=g_nseg-1;i>=0;i--)
        if(px>=g_seg[i].x0&&px<g_seg[i].x1&&py>=g_seg[i].y0&&py<g_seg[i].y1){
            *disk=g_seg[i].disk;*part=g_seg[i].part;return 1;
        }
    return 0;
}
static int tbHit(int x,int y){
    int i;
    for(i=0;i<NTB;i++) if(x>=g_tbr[i].left&&x<g_tbr[i].right&&y>=g_tbr[i].top&&y<g_tbr[i].bottom) return i;
    return -1;
}

#define TB_H 64

static void drawToolbar(HDC dc,int W){
    int i,x=6,y=4,bw=72,bh=56,prevg=0;
    /* narrower buttons on a small screen (640 x 480) */
    if(6+NTB*(bw+2)+2*10+4>W){ bw=(W-6-2*10-4)/NTB-2; if(bw<58)bw=58; }
    fillR(dc,0,0,W,TB_H,COL_TB); hline(dc,0,W,TB_H-1,COL_TBLINE);
    for(i=0;i<NTB;i++){
        int cmd=g_tbdef[i].cmd, on=cmdEnabled(cmd), hot=(i==g_hot&&on), dn=(i==g_down&&hot), d, tw;
        const char*lb=g_tbdef[i].label;
        if(g_tbdef[i].group!=prevg){ x+=4; fillR(dc,x,10,x+1,TB_H-10,COL_TBLINE); x+=6; prevg=g_tbdef[i].group; }
        g_tbr[i].left=x; g_tbr[i].top=y; g_tbr[i].right=x+bw; g_tbr[i].bottom=y+bh;
        if(dn){ fillR(dc,x,y,x+bw,y+bh,COL_TBDOWN); frameR(dc,x,y,x+bw,y+bh,COL_TBHOTF); }
        else if(hot){ fillR(dc,x,y,x+bw,y+bh,COL_TBHOT); frameR(dc,x,y,x+bw,y+bh,COL_TBHOTF); }
        d=dn?1:0;
        drawIcon(dc,g_tbdef[i].icon,x+(bw-28)/2+d,y+5+d,28,on);
        if(cmd==CMD_CONVERT) lb=convertLabel();
        if(cmd==CMD_DYNAMIC) lb=dynamicLabel();
        tw=textW(dc,lb,g_font);
        text(dc,x+(bw-tw)/2+d,y+37+d,lb,on?COL_TEXT:COL_DIS,g_font);
        x+=bw+2;
    }
}

static void buildRows(void){
    DWORD i,j;
    g_nrows=0;
    for(i=0;i<g_dl.Count;i++){
        int dn=(int)g_dl.Disk[i].DiskNumber;
        g_rows[g_nrows].disk=dn; g_rows[g_nrows].part=-1; g_nrows++;
        if(g_layOk[i])
            for(j=0;j<g_lay[i].Count;j++){ g_rows[g_nrows].disk=dn; g_rows[g_nrows].part=(int)j; g_nrows++; }
    }
}
static int rowOfSel(void){
    int r;
    for(r=0;r<g_nrows;r++) if(g_rows[r].disk==g_selDisk&&g_rows[r].part==g_selPart) return r;
    for(r=0;r<g_nrows;r++) if(g_rows[r].disk==g_selDisk&&g_rows[r].part==-1) return r;
    return -1;
}

static void drawRow(HDC dc,int r,int x,int y,int w,const int*c){
    int dn=g_rows[r].disk, pi=g_rows[r].part, sel=(dn==g_selDisk&&pi==g_selPart);
    MGPT_LAYOUT*l=layOf(dn); MGPT_DISKINFO*di=infoOf(dn);
    COLORREF tc=sel?COL_SELTXT:COL_TEXT, tc2=sel?COL_SELTXT:COL_TEXT2;
    char b[160], sz[24];
    if(pi<0){
        fillR(dc,x,y,x+w,y+18,sel?COL_SEL:COL_DISKROW);
        drawIcon(dc,IC_DISK,c[0],y+1,16,1);
        diskName(dn,b); text(dc,c[0]+21,y+2,b,tc,g_fontB);
        if(di&&di->Media){
            if(di->TotalSectors){ sizestr(di->TotalSectors,di->SectorSize,sz); wsprintfA(b,"CD / DVD  \xB7  %s  \xB7  2048 bytes/sector  \xB7  shown only",sz); }
            else scopy(b,"CD / DVD  \xB7  no data disc in the drive");
            textFit(dc,c[1],y+2,x+w-c[1]-6,b,tc2,g_font);
        } else if(di&&isDynamic(l)){
            MGPT_LDMGROUP*g=grpOf(l); char t[80];
            sizestr(di->TotalSectors,di->SectorSize,sz);
            wsprintfA(b,"Dynamic (%s)  \xB7  %s  \xB7  %lu bytes/sector",di->IsGpt?"GPT":"MBR",sz,di->SectorSize);
            if(g){
                wsprintfA(t,"  \xB7  disk group %s",g->Name); scat(b,t);
                if(g->Native) scat(b,"  \xB7  managed by Windows");
                if(l->LdmMissing){ wsprintfA(t,"  \xB7  %lu disk(s) of the group missing",l->LdmMissing); scat(b,t); }
            } else scat(b,"  \xB7  its database cannot be read");
            if(diskIsSystem(l)) scat(b,"  \xB7  system disk");
            reachNote(di,b);
            textFit(dc,c[1],y+2,x+w-c[1]-6,b,tc2,g_font);
        } else if(di){
            sizestr(di->TotalSectors,di->SectorSize,sz);
            wsprintfA(b,"%s  \xB7  %s  \xB7  %lu bytes/sector  \xB7  %s",
                (di->Reach&MGPT_R_WHOLE)?"No partition table":di->IsGpt?"GPT":"MBR",sz,di->SectorSize,di->Owned?"managed by MBRGPTNT":"in-box disk driver");
            if(l&&diskIsSystem(l)) scat(b,"  \xB7  system disk");
            reachNote(di,b);
            textFit(dc,c[1],y+2,x+w-c[1]-6,b,tc2,g_font);
        }
    } else if(l){
        MGPT_PART*p=&l->Part[pi]; int ind=(p->Flags&MGPT_F_LOGICAL)?14:0;
        fillR(dc,x,y,x+w,y+18,sel?COL_SEL:COL_BG);
        fillR(dc,c[0]+ind+2,y+4,c[0]+ind+12,y+14,fscolor(p->FsKind));
        frameR(dc,c[0]+ind+2,y+4,c[0]+ind+12,y+14,RGB(60,70,85));
        partLabel(l,pi,b); textFit(dc,c[0]+ind+17,y+2,c[1]-c[0]-ind-21,b,tc,g_font);
        if(p->DriveLetter){ wsprintfA(b,"%c:",(char)p->DriveLetter); text(dc,c[1],y+2,b,tc,g_fontB); }
        textFit(dc,c[2],y+2,c[3]-c[2]-6,fsname(p->FsKind),tc,g_font);
        sizestr(p->SectorCount,l->SectorSize,sz); text(dc,c[3],y+2,sz,tc,g_font);
        wsprintfA(b,"0x%02X",p->MbrType); text(dc,c[4],y+2,b,tc,g_font);
        partStatus(l,p,b); textFit(dc,c[5],y+2,x+w-c[5]-6,b,tc,g_font);
    }
    hline(dc,x,x+w,y+17,COL_GRID);
}

static void redraw(void);
static void sbArrow(HDC dc,int x,int y,int up,int pressed){
    POINT p[3]; HPEN pn; HBRUSH br; HGDIOBJ op,ob; COLORREF c=pressed?COL_SBHOT:COL_SBARR;
    int cx=x+SB_W/2, cy=y+SB_W/2;
    fillR(dc,x,y,x+SB_W,y+SB_W,pressed?COL_TBDOWN:COL_SBBTN);
    p[0].x=cx-4; p[1].x=cx+4; p[2].x=cx;
    if(up){ p[0].y=cy+2; p[1].y=cy+2; p[2].y=cy-2; } else { p[0].y=cy-2; p[1].y=cy-2; p[2].y=cy+2; }
    pn=CreatePen(0,1,c); br=CreateSolidBrush(c);
    op=SelectObject(dc,pn); ob=SelectObject(dc,br); Polygon(dc,p,3);
    SelectObject(dc,op); SelectObject(dc,ob); DeleteObject(pn); DeleteObject(br);
}

/* scroll bar geometry for the current g_rowTop / g_vis / g_nrows */
static void sbLayout(void){
    int trk=g_sb.y1-g_sb.y0-2*SB_W, len, maxTop=g_nrows-g_vis;
    if(trk<1)trk=1;
    len=trk*g_vis/(g_nrows>0?g_nrows:1); if(len<18)len=18; if(len>trk)len=trk;
    g_sb.t0=g_sb.y0+SB_W+(maxTop>0?(trk-len)*g_rowTop/maxTop:0);
    g_sb.t1=g_sb.t0+len;
}

static void drawScrollBar(HDC dc){
    int x=g_sb.x0;
    fillR(dc,x,g_sb.y0,g_sb.x1,g_sb.y1,COL_SBTRK);
    fillR(dc,x,g_sb.y0,x+1,g_sb.y1,COL_GRID);
    if(g_sbAct==SB_PGUP) fillR(dc,x+1,g_sb.y0+SB_W,g_sb.x1,g_sb.t0,COL_TBDOWN);
    if(g_sbAct==SB_PGDN) fillR(dc,x+1,g_sb.t1,g_sb.x1,g_sb.y1-SB_W,COL_TBDOWN);
    sbArrow(dc,x+1,g_sb.y0,1,g_sbAct==SB_UP);
    sbArrow(dc,x+1,g_sb.y1-SB_W,0,g_sbAct==SB_DOWN);
    fillR(dc,x+4,g_sb.t0+1,g_sb.x1-3,g_sb.t1-1,g_sbAct==SB_THUMB?COL_SBHOT:COL_SBTHM);
    if(g_lowColor) frameR(dc,x+4,g_sb.t0+1,g_sb.x1-3,g_sb.t1-1,RGB(0,0,0));
}

static int drawTable(HDC dc,int x,int y,int w,int maxh){
    int c[6], r, yy, sr, rw;
    static const char*hd[6]={"Partition","Letter","File system","Capacity","Type","Status"};
    buildRows();
    g_vis=(maxh-20)/18; if(g_vis<1)g_vis=1;
    g_sb.on=(g_nrows>g_vis);
    rw=g_sb.on?w-SB_W-1:w;
    if(g_rowTop>g_nrows-g_vis)g_rowTop=g_nrows-g_vis;
    if(g_rowTop<0)g_rowTop=0;
    /* bring a newly selected row into view; scrolling with the bar or the
       wheel leaves the selection where it is */
    sr=rowOfSel();
    if(sr>=0&&(g_selDisk!=g_lastSelDisk||g_selPart!=g_lastSelPart)){
        if(sr<g_rowTop)g_rowTop=sr;
        if(sr>=g_rowTop+g_vis)g_rowTop=sr-g_vis+1;
    }
    g_lastSelDisk=g_selDisk; g_lastSelPart=g_selPart;
    c[0]=x+8; c[1]=x+rw*25/100; c[2]=x+rw*34/100; c[3]=x+rw*49/100; c[4]=x+rw*62/100; c[5]=x+rw*72/100;
    fillR(dc,x,y,x+w,y+20,COL_HDR); hline(dc,x,x+w,y+19,COL_GRID);
    for(r=0;r<6;r++) text(dc,c[r],y+3,hd[r],COL_TEXT,g_font);
    yy=y+20;
    for(r=g_rowTop;r<g_nrows&&r<g_rowTop+g_vis;r++,yy+=18){
        drawRow(dc,r,x,yy,rw,c);
        addSeg(x,yy,x+rw,yy+18,g_rows[r].disk,g_rows[r].part);
    }
    if(g_sb.on){
        char b[48]; int tw;
        wsprintfA(b,"rows %d-%d of %d",g_rowTop+1,g_rowTop+g_vis<g_nrows?g_rowTop+g_vis:g_nrows,g_nrows);
        tw=textW(dc,b,g_font); text(dc,x+w-tw-8,y+3,b,COL_TEXT2,g_font);
        g_sb.x0=x+rw; g_sb.x1=x+w; g_sb.y0=y+20; g_sb.y1=yy;
        sbLayout(); drawScrollBar(dc);
    }
    frameR(dc,x,y,x+w,yy,COL_GRID);
    return yy;
}

/* scroll the table by n rows (negative = up) without moving the selection */
static void scrollRows(int n){
    int t=g_rowTop+n;
    if(t>g_nrows-g_vis)t=g_nrows-g_vis;
    if(t<0)t=0;
    if(t!=g_rowTop){ g_rowTop=t; redraw(); }
}

static int sbHit(int px,int py){
    if(!g_sb.on||px<g_sb.x0||px>=g_sb.x1||py<g_sb.y0||py>=g_sb.y1)return SB_NONE;
    if(py<g_sb.y0+SB_W)return SB_UP;
    if(py>=g_sb.y1-SB_W)return SB_DOWN;
    if(py<g_sb.t0)return SB_PGUP;
    if(py>=g_sb.t1)return SB_PGDN;
    return SB_THUMB;
}

/* one step of the pressed scroll-bar part (also repeated by the timer) */
static void sbStep(void){
    switch(g_sbAct){
    case SB_UP:   scrollRows(-1); break;
    case SB_DOWN: scrollRows(1); break;
    case SB_PGUP: if(g_sbMy<g_sb.t0) scrollRows(-(g_vis-1>0?g_vis-1:1)); break;
    case SB_PGDN: if(g_sbMy>=g_sb.t1) scrollRows(g_vis-1>0?g_vis-1:1); break;
    }
}

/* move the selection to row r (clamped) */
static void selectRow(int r){
    if(g_nrows<=0)return;
    if(r<0)r=0; if(r>=g_nrows)r=g_nrows-1;
    g_selDisk=g_rows[r].disk; g_selPart=g_rows[r].part; g_status[0]=0; redraw();
}

static void drawBar(HDC dc,int bx,int by,int bw,int bh,MGPT_LAYOUT*lay,int diskNo){
    RECT r; int i;
    mgpt_u64 total=lay->TotalSectors, spp;
    /* a GPT written for a larger disk than the driver reaches: the map
       shows the whole disk, the part out of reach hatched */
    if(lay->TableSectors>total) total=lay->TableSectors;
    if(lay->DeviceSectors>total) total=lay->DeviceSectors;
    if(total==0)total=1;
    spp=udiv64(total,(mgpt_u64)bw); if(!spp)spp=1;
    fillR(dc,bx,by,bx+bw,by+bh,RGB(226,229,234));
    { HBRUSH hb=CreateHatchBrush(HS_FDIAGONAL,RGB(196,201,208)); r.left=bx;r.top=by;r.right=bx+bw;r.bottom=by+bh;
      SetBkMode(dc,TRANSPARENT); FillRect(dc,&r,hb); DeleteObject(hb); }
    addSeg(bx,by,bx+bw,by+bh,diskNo,-1);
    for(i=0;i<(int)lay->Count;i++){
        MGPT_PART*p=&lay->Part[i]; int x0,x1;
        if(!isExtType(p->MbrType))continue;
        if(p->StartLba>=total)continue;
        x0=bx+(int)udiv64(p->StartLba,spp); x1=bx+(int)udiv64(p->StartLba+p->SectorCount,spp);
        if(x1>bx+bw)x1=bx+bw; if(x1<=x0)x1=x0+1;
        fillR(dc,x0,by,x1,by+bh,RGB(236,238,242)); frameR(dc,x0,by,x1,by+bh,RGB(150,160,175));
        addSeg(x0,by,x1,by+bh,diskNo,i);
    }
    for(i=0;i<(int)lay->Count;i++){
        MGPT_PART*p=&lay->Part[i]; int x0,x1,in;
        if(isExtType(p->MbrType)||p->StartLba>=total)continue;
        x0=bx+(int)udiv64(p->StartLba,spp); x1=bx+(int)udiv64(p->StartLba+p->SectorCount,spp);
        if(x1>bx+bw)x1=bx+bw; if(x1<=x0)x1=x0+2;
        in=(p->Flags&MGPT_F_LOGICAL)?4:0;
        fillR(dc,x0,by+in,x1,by+bh-in,fscolor(p->FsKind));
        if((p->Flags&MGPT_F_LDM)&&p->LdmState==MGPT_LDMS_FAILED){
            HBRUSH hb=CreateHatchBrush(HS_FDIAGONAL,RGB(200,40,40)); RECT q; q.left=x0;q.top=by+in;q.right=x1;q.bottom=by+bh-in;
            SetBkMode(dc,TRANSPARENT); FillRect(dc,&q,hb); DeleteObject(hb);
        }
        /* the type of a dynamic volume: a band across the top */
        if((p->Flags&MGPT_F_LDM)&&x1-x0>=4){
            COLORREF bc=p->LdmType==MGPT_LDM_MIRROR?RGB(150,70,170):p->LdmType==MGPT_LDM_RAID5?RGB(20,140,140):
                        p->LdmType==MGPT_LDM_STRIPED?RGB(210,120,20):p->LdmType==MGPT_LDM_SPANNED?RGB(170,40,90):RGB(30,60,120);
            fillR(dc,x0,by+in,x1,by+in+4,bc);
        }
        if(g_selDisk==diskNo&&g_selPart==i){
            frameR(dc,x0,by+in,x1,by+bh-in,RGB(255,205,40)); frameR(dc,x0+1,by+in+1,x1-1,by+bh-in-1,RGB(255,205,40));
        } else {
            /* the other pieces of the selected dynamic volume */
            MGPT_LAYOUT*sl=layOf(g_selDisk); MGPT_PART*sp=(sl&&g_selPart>=0&&(DWORD)g_selPart<sl->Count)?&sl->Part[g_selPart]:NULL;
            if(sp&&(sp->Flags&MGPT_F_LDM)&&(p->Flags&MGPT_F_LDM)&&sp->LdmGroup==p->LdmGroup&&sp->LdmVol==p->LdmVol)
                { frameR(dc,x0,by+in,x1,by+bh-in,RGB(255,205,40)); frameR(dc,x0+1,by+in+1,x1-1,by+bh-in-1,RGB(40,50,65)); }
            else frameR(dc,x0,by+in,x1,by+bh-in,RGB(40,50,65));
        }
        if(x1-x0>=44){
            char nm[40], sz[24], ln2[48]; const char*f=fsname(p->FsKind);
            if(p->DriveLetter) wsprintfA(nm,"%c:  %s",(char)p->DriveLetter,f); else scopy(nm,f);
            textFit(dc,x0+5,by+in+5,x1-x0-8,nm,RGB(255,255,255),g_fontB);
            sizestr(p->SectorCount,lay->SectorSize,sz);
            if(p->Flags&MGPT_F_LDM){
                wsprintfA(ln2,"%s  \xB7  %s",p->VolName,sz);
                if(p->LdmState!=MGPT_LDMS_HEALTHY){ scat(ln2,"  \xB7  "); scat(ln2,ldmStateName(p->LdmState)); }
                textFit(dc,x0+5,by+in+21,x1-x0-8,ln2,RGB(255,255,255),g_font);
            } else textFit(dc,x0+5,by+in+21,x1-x0-8,sz,RGB(255,255,255),g_font);
        }
        addSeg(x0,by+in,x1,by+bh-in,diskNo,i);
    }
    if(lay->TableSectors>lay->TotalSectors||lay->DeviceSectors>lay->TotalSectors){
        int xr=bx+(int)udiv64(lay->TotalSectors,spp); HBRUSH hb=CreateHatchBrush(HS_DIAGCROSS,RGB(170,40,40)); RECT q;
        if(xr<bx+bw){
            q.left=xr;q.top=by;q.right=bx+bw;q.bottom=by+bh; SetBkMode(dc,TRANSPARENT); FillRect(dc,&q,hb);
            fillR(dc,xr,by,xr+2,by+bh,RGB(170,40,40));
        }
        DeleteObject(hb);
    }
    frameR(dc,bx,by,bx+bw,by+bh,RGB(90,100,115));
}

static void drawDiskMap(HDC dc,int x,int y,int w,int bottom){
    DWORD i; int bh=46, boxw=110;
    for(i=0;i<g_dl.Count;i++){
        MGPT_DISKINFO*di=&g_dl.Disk[i]; int dn=(int)di->DiskNumber, sel=(dn==g_selDisk&&g_selPart<0);
        char b[48], sz[24];
        if(y+bh>bottom)break;
        fillR(dc,x,y,x+boxw,y+bh,COL_DISKROW);
        frameR(dc,x,y,x+boxw,y+bh,sel?COL_ACCENT:COL_GRID);
        if(sel) frameR(dc,x+1,y+1,x+boxw-1,y+bh-1,COL_ACCENT);
        drawIcon(dc,IC_DISK,x+6,y+4,20,1);
        diskName(dn,b); text(dc,x+31,y+6,b,COL_TEXT,g_fontB);
        { mgpt_u64 n=di->TotalSectors; if(di->TableSectors>n)n=di->TableSectors; if(di->DeviceSectors>n)n=di->DeviceSectors; sizestr(n,di->SectorSize,sz); }
        if(di->Media) { if(di->TotalSectors) wsprintfA(b,"CD / DVD  \xB7  %s",sz); else scopy(b,"No disc"); }
        else if(di->Dynamic) wsprintfA(b,"Dynamic  \xB7  %s",sz);
        else if(di->Reach&MGPT_R_WHOLE) wsprintfA(b,"No table  \xB7  %s",sz);
        else wsprintfA(b,"%s  \xB7  %s",di->IsGpt?"GPT":"MBR",sz);
        textFit(dc,x+7,y+26,boxw-10,b,COL_TEXT2,g_font);
        addSeg(x,y,x+boxw,y+bh,dn,-1);
        if(g_layOk[i]) drawBar(dc,x+boxw+6,y,w-boxw-6,bh,&g_lay[i],dn);
        y+=bh+8;
    }
}

static void describeSel(char*b){
    MGPT_LAYOUT*l=layOf(g_selDisk); MGPT_PART*p=selPart(); char t[48], sz[24];
    if(g_drv==INVALID_HANDLE_VALUE){ scopy(b,"The MBRGPTNT driver is not loaded."); return; }
    if(!l){ scopy(b,"Select a disk or a partition."); return; }
    if(p&&(p->Flags&MGPT_F_LDM)){
        char vs[24], ds[64];
        sizestr(p->LdmVolSize,512,vs); sizestr(p->SectorCount,l->SectorSize,sz); volDisks(p->LdmGroup,p->LdmVol,ds);
        if(p->DriveLetter) wsprintfA(t,"%s (%c:)",p->VolName,(char)p->DriveLetter); else scopy(t,p->VolName);
        wsprintfA(b,"%s  \xB7  %s volume  \xB7  %s  \xB7  %s  \xB7  %s  \xB7  on %s  \xB7  this piece %s",
            t,ldmTypeName(p->LdmType),fsname(p->FsKind),vs,ldmStateName(p->LdmState),ds,sz);
    } else if(p){
        partTitle(l,g_selPart,t); sizestr(p->SectorCount,l->SectorSize,sz);
        { char dn[24]; diskName(g_selDisk,dn); wsprintfA(b,"%s  \xB7  %s  \xB7  %s  \xB7  %s",dn,t,fsname(p->FsKind),sz); }
    } else if(l->Media){
        char dn[24]; diskName(g_selDisk,dn); sizestr(l->TotalSectors,l->SectorSize,sz);
        if(l->TotalSectors) wsprintfA(b,"%s  \xB7  CD / DVD  \xB7  %s",dn,sz); else wsprintfA(b,"%s  \xB7  no data disc",dn);
    } else if(isDynamic(l)){
        MGPT_LDMGROUP*g=grpOf(l); int k=g?ldmDiskOf(g,g_selDisk):-1;
        sizestr(l->TotalSectors,l->SectorSize,sz);
        if(g&&k>=0) wsprintfA(b,"Disk %d  \xB7  dynamic (%s)  \xB7  %s  \xB7  disk group %s (%s)  \xB7  %s free",
                              g_selDisk,l->IsGpt?"GPT":"MBR",sz,g->Name,g->Disk[k].Name,freeStr(g->Disk[k].FreeTotal));
        else wsprintfA(b,"Disk %d  \xB7  dynamic (%s)  \xB7  %s",g_selDisk,l->IsGpt?"GPT":"MBR",sz);
    } else {
        sizestr(l->TotalSectors,l->SectorSize,sz);
        { char fr[24]; sizestr(largestFree(l),l->SectorSize,fr);
          wsprintfA(b,"Disk %d  \xB7  %s  \xB7  %s  \xB7  %s unallocated",g_selDisk,l->IsGpt?"GPT":"MBR",sz,fr); }
    }
}

static void paintAll(HWND hwnd,HDC dc){
    RECT rc; int W,H,top,bottom,tb,mapH,tmax,statusY,legY;
    GetClientRect(hwnd,&rc); W=rc.right; H=rc.bottom;
    g_lowColor=(GetDeviceCaps(dc,BITSPIXEL)*GetDeviceCaps(dc,PLANES)<=8)?1:0;
    g_nseg=0;
    fillR(dc,0,0,W,H,COL_BG);
    drawToolbar(dc,W);
    statusY=H-22; legY=statusY-20;
    top=TB_H+8; bottom=legY-4;

    if(g_drv==INVALID_HANDLE_VALUE){
        drawIcon(dc,IC_DISK,16,top+4,28,0);
        text(dc,54,top+6,"The MBRGPTNT driver is not loaded.",COL_WARN,g_fontB);
        text(dc,54,top+24,"Run mbrginst as an administrator, then restart Windows NT.",COL_TEXT2,g_font);
    } else {
        mapH=(int)g_dl.Count*54;
        tmax=bottom-top-mapH-10; if(tmax<20+18*3)tmax=20+18*3;
        tb=drawTable(dc,8,top,W-16,tmax);
        drawDiskMap(dc,8,tb+10,W-16,bottom);
    }

    /* legend: the file systems present on the disks, in a fixed order */
    {
        static const int order[]={FS_FAT12,FS_FAT16,FS_FAT32,FS_NTFS,FS_EXFAT,FS_HPFS,FS_REFS,GPT_FS_BITLOCKER,FS_UDF,GPT_FS_ISO9660,
            FS_EXT2,FS_EXT3,FS_EXT4,GPT_FS_EXTFS,GPT_FS_XIAFS,GPT_FS_MINIX,FS_BTRFS,GPT_FS_XFS,GPT_FS_JFS,GPT_FS_REISERFS,GPT_FS_REISER4,
            GPT_FS_F2FS,GPT_FS_NILFS2,GPT_FS_BCACHEFS,GPT_FS_GFS2,GPT_FS_OCFS2,GPT_FS_SQUASHFS,GPT_FS_CRAMFS,GPT_FS_EROFS,GPT_FS_ROMFS,
            GPT_FS_JFFS2,GPT_FS_SWAP,GPT_FS_LUKS,GPT_FS_LVM2,GPT_FS_MDRAID,FS_LINUX,
            GPT_FS_HFS,GPT_FS_HFSPLUS,GPT_FS_HFSX,GPT_FS_APFS,GPT_FS_MFS,GPT_FS_CORESTOR,GPT_FS_APPLEBOOT,
            GPT_FS_UFS,GPT_FS_UFS2,GPT_FS_ZFS,GPT_FS_HAMMER,GPT_FS_HAMMER2,GPT_FS_BSDSWAP,GPT_FS_SYSV,GPT_FS_XENIX,GPT_FS_UNIXBFS,
            GPT_FS_EFS,GPT_FS_VXFS,GPT_FS_QNX6,GPT_FS_VMFS,GPT_FS_BEFS,GPT_FS_AMIGA,
            FS_EFI,GPT_FS_BIOSBOOT,FS_MSR,FS_LDMMETA,FS_LDMDATA,FS_EXT,FS_UNKNOWN};
        int present[FS_KINDS], lx=8, k; DWORD i,j;
        for(k=0;k<FS_KINDS;k++)present[k]=0;
        for(i=0;i<g_dl.Count;i++) if(g_layOk[i]) for(j=0;j<g_lay[i].Count;j++){ DWORD f=g_lay[i].Part[j].FsKind; if(f<FS_KINDS)present[f]=1; }
        for(k=0;k<(int)(sizeof(order)/sizeof(order[0]));k++){
            const char*nm; int tw;
            if(!present[order[k]])continue;
            nm=fsname((DWORD)order[k]); tw=textW(dc,nm,g_font);
            if(lx+14+tw>W-4)break;
            fillR(dc,lx,legY+4,lx+11,legY+15,fscolor((DWORD)order[k])); frameR(dc,lx,legY+4,lx+11,legY+15,RGB(60,70,85));
            text(dc,lx+14,legY+3,nm,COL_TEXT2,g_font);
            lx+=14+tw+7;
        }
    }
    /* status bar */
    fillR(dc,0,statusY,W,H,COL_STATUS); hline(dc,0,W,statusY,COL_TBLINE);
    {
        char b[200]; const char*hint="Right-click a partition for actions"; int hw=textW(dc,hint,g_font);
        if(g_status[0]) scopy(b,g_status); else describeSel(b);
        textFit(dc,8,statusY+4,W-hw-30,b,COL_TEXT,g_font);
        text(dc,W-hw-8,statusY+4,hint,COL_TEXT2,g_font);
    }
}

static void redraw(void){ InvalidateRect(g_main,NULL,0); }
static void setStatus(const char*s){ scopy(g_status,s); InvalidateRect(g_main,NULL,0); UpdateWindow(g_main); }

/* ------------------------------------------------------------------ */
/* popup dialogs                                                      */

static HWND  g_dlg, g_hint, g_dlgFirst, g_radio1, g_radio2;
static HWND  g_cType, g_eSize, g_cUnit, g_cLetter;
static int   g_dlgDone, g_dlgRes, g_dlgIcon, g_focusCancel;
static char  g_dlgHead[64], g_dlgSub[160], g_dlgWarn[160], g_body[320], g_hintText[160], g_curText[64];
static int (*g_dlgValidate)(void);

/* values handed from the dialogs to the operations */
static int   g_ctxGpt, g_ctxLogical;
static MGPT_LAYOUT *g_ctxLay; static mgpt_u64 g_ctxStart, g_ctxCount;
static DWORD g_ctxCurType, g_ctxCurLetter;
static DWORD g_vType, g_vSize, g_vScheme, g_vLetter; static int g_vUnit;
static BYTE  g_letterMap[32];

static HWND dmk(LPCSTR cls,LPCSTR txt,DWORD style,int x,int y,int w,int h,int id,HFONT f){
    HWND c=CreateWindowExA(0,cls,txt,WS_CHILD|WS_VISIBLE|style,x,y,w,h,g_dlg,(HMENU)(ULONG_PTR)id,g_inst,NULL);
    SendMessageA(c,WM_SETFONT,(WPARAM)f,1);
    return c;
}

static LRESULT CALLBACK DlgProc(HWND h,UINT m,WPARAM wp,LPARAM lp){
    switch(m){
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps); RECT rc; GetClientRect(h,&rc);
        fillR(dc,0,0,rc.right,58,COL_DLGHDR); hline(dc,0,rc.right,58,COL_GRID);
        drawIcon(dc,g_dlgIcon,16,15,28,1);
        text(dc,56,12,g_dlgHead,COL_TEXT,g_fontH);
        textFit(dc,56,34,rc.right-64,g_dlgSub,COL_TEXT2,g_font);
        if(g_dlgWarn[0]){
            /* a leading '~' marks an informational note; otherwise a warning */
            int y=rc.bottom-48-26, note=(g_dlgWarn[0]=='~');
            fillR(dc,16,y+2,30,y+16,note?COL_ACCENT:COL_WARN);
            text(dc,note?22:21,y+2,note?"i":"!",RGB(255,255,255),g_fontB);
            textFit(dc,36,y+2,rc.right-52,g_dlgWarn+note,note?COL_TEXT2:COL_WARN,g_font);
        }
        fillR(dc,0,rc.bottom-48,rc.right,rc.bottom,COL_DLGBAND); hline(dc,0,rc.right,rc.bottom-48,COL_GRID);
        EndPaint(h,&ps); return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        if(m==WM_CTLCOLORBTN && (HWND)lp!=g_radio1 && (HWND)lp!=g_radio2) break;
        SetBkColor((HDC)wp,RGB(255,255,255));
        SetTextColor((HDC)wp,((HWND)lp==g_hint)?COL_TEXT2:COL_TEXT);
        return (LRESULT)g_brWhite;
    case WM_COMMAND: {
        int id=(int)(wp&0xFFFF);
        if(id==1){ if(!g_dlgValidate||g_dlgValidate()){ g_dlgRes=1; g_dlgDone=1; } return 0; }
        if(id==2){ g_dlgRes=0; g_dlgDone=1; return 0; }
        return 0;
    }
    case WM_CLOSE: g_dlgRes=0; g_dlgDone=1; return 0;
    }
    return DefWindowProcA(h,m,wp,lp);
}

/* Run a modal popup. build() adds the value fields under the header;
   validate() checks them when OK is pressed (return 0 to stay open). */
static int runDialog(int icon,const char*head,const char*sub,const char*warn,const char*ok,
                     int w,int h,void(*build)(int),int(*validate)(void)){
    RECT r,mr; MSG msg; DWORD st=WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN; int x,y,rw,rh; HWND bOk,bCancel;
    scopy(g_dlgHead,head); scopy(g_dlgSub,sub); scopy(g_dlgWarn,warn?warn:"");
    g_dlgIcon=icon; g_dlgValidate=validate;
    r.left=0;r.top=0;r.right=w;r.bottom=h; AdjustWindowRectEx(&r,st,0,WS_EX_DLGMODALFRAME);
    rw=r.right-r.left; rh=r.bottom-r.top;
    GetWindowRect(g_main,&mr);
    x=mr.left+((mr.right-mr.left)-rw)/2; y=mr.top+((mr.bottom-mr.top)-rh)/2; if(x<0)x=0; if(y<0)y=0;
    g_hint=NULL; g_dlgFirst=NULL; g_radio1=NULL; g_radio2=NULL;
    g_dlg=CreateWindowExA(WS_EX_DLGMODALFRAME,"MbrGptNtDlg",head,st,x,y,rw,rh,g_main,NULL,g_inst,NULL);
    if(!g_dlg)return 0;
    if(build)build(w);
    bOk=dmk("BUTTON",ok,BS_DEFPUSHBUTTON|WS_TABSTOP,w-200,h-37,90,26,1,g_font);
    bCancel=dmk("BUTTON","Cancel",BS_PUSHBUTTON|WS_TABSTOP,w-102,h-37,90,26,2,g_font);
    EnableWindow(g_main,0);
    ShowWindow(g_dlg,SW_SHOW); UpdateWindow(g_dlg);
    if(g_dlgFirst){ SetFocus(g_dlgFirst); if(g_dlgFirst==g_cType) SendMessageA(g_cType,CB_SETEDITSEL,0,(LPARAM)0xFFFF0000L); }
    else SetFocus(g_focusCancel?bCancel:bOk);
    g_dlgDone=0; g_dlgRes=0;
    while(!g_dlgDone){
        if(GetMessageA(&msg,NULL,0,0)<=0){ PostQuitMessage(0); break; }
        if(!IsDialogMessageA(g_dlg,&msg)){ TranslateMessage(&msg); DispatchMessageA(&msg); }
    }
    EnableWindow(g_main,1);
    DestroyWindow(g_dlg); g_dlg=NULL;
    SetFocus(g_main);
    g_focusCancel=0;
    return g_dlgRes;
}

static int parseHex(HWND h,DWORD*out){
    char b[48]; int i=0,n=0; DWORD v=0;
    GetWindowTextA(h,b,48);
    while(b[i]==' ')i++;
    if(b[i]=='0'&&(b[i+1]=='x'||b[i+1]=='X'))i+=2;
    for(;b[i];i++){
        char c=b[i]; DWORD d;
        if(c>='0'&&c<='9')d=(DWORD)(c-'0'); else if(c>='a'&&c<='f')d=(DWORD)(c-'a'+10);
        else if(c>='A'&&c<='F')d=(DWORD)(c-'A'+10); else break;
        v=v*16+d; if(++n>3)return 0;
    }
    if(!n)return 0;
    *out=v; return 1;
}
static void fillTypes(HWND cb){
    int i;
    for(i=0;i<NTYPES;i++){
        if((g_ctxGpt||g_ctxLogical)&&isExtType(g_types[i].t))continue;
        SendMessageA(cb,CB_ADDSTRING,0,(LPARAM)g_types[i].n);
    }
}
static void dlgErr(const char*s){ MessageBoxA(g_dlg,s,"MBRGPTNT",MB_OK|MB_ICONEXCLAMATION); }

/* ---- New partition ---- */
/* names offered when creating: FAT16 / FAT32 / extended let the driver
   pick the exact type byte (04/06/0E, 0B/0C, 05/0F) for the partition's
   size and position */
static const char *g_newNames[] = { "FAT16", "FAT32", "NTFS", "Extended", "Linux", "EFI System" };
static mgpt_u64 g_ctxFreeMB;

static void bNew(int w){
    int y=74, i;
    dmk("STATIC","File system:",SS_LEFT,16,y+4,108,16,300,g_font);
    g_cType=dmk("COMBOBOX","",CBS_DROPDOWN|WS_VSCROLL|WS_TABSTOP,128,y,w-144,140,301,g_font);
    for(i=0;i<6;i++){ if(g_ctxGpt&&i==3)continue; SendMessageA(g_cType,CB_ADDSTRING,0,(LPARAM)g_newNames[i]); }
    SetWindowTextA(g_cType,"FAT16");
    y+=32;
    dmk("STATIC","Size:",SS_LEFT,16,y+4,108,16,304,g_font);
    g_eSize=dmk("EDIT","",ES_AUTOHSCROLL|WS_BORDER|WS_TABSTOP,128,y,100,22,302,g_font);
    g_cUnit=dmk("COMBOBOX","",CBS_DROPDOWN|WS_VSCROLL|WS_TABSTOP,234,y,64,124,303,g_font);
    for(i=0;i<5;i++) SendMessageA(g_cUnit,CB_ADDSTRING,0,(LPARAM)g_unitNames[i]);
    SendMessageA(g_cUnit,CB_SETCURSEL,1,0);
    y+=28;
    g_hint=dmk("STATIC",g_hintText,SS_LEFT,128,y,w-144,30,390,g_font);
    g_dlgFirst=g_cType;
}
static int startsWith(const char*s,const char*p){
    while(*p){ char a=*s,b=*p; if(a>='a'&&a<='z')a=(char)(a-32); if(b>='a'&&b<='z')b=(char)(b-32); if(a!=b)return 0; s++; p++; }
    return 1;
}
/* text of the type box -> MGPT_TYPE_* family or an exact type byte */
static int parseNewType(DWORD*out){
    char b[48]; int i=0; DWORD t;
    GetWindowTextA(g_cType,b,48);
    while(b[i]==' ')i++;
    if(startsWith(b+i,"FAT16")){ *out=MGPT_TYPE_FAT16; return 1; }
    if(startsWith(b+i,"FAT32")){ *out=MGPT_TYPE_FAT32; return 1; }
    if(startsWith(b+i,"NTFS")) { *out=0x07; return 1; }
    if(startsWith(b+i,"EXT"))  { *out=MGPT_TYPE_EXTENDED; return 1; }
    if(startsWith(b+i,"LINUX")){ *out=0x83; return 1; }
    if(startsWith(b+i,"EFI"))  { *out=0xEF; return 1; }
    if(!parseHex(g_cType,&t)||t==0||t>0xFF||t==0xEE) return 0;
    /* a FAT16 / FAT32 / extended byte typed by hand: still pick the right
       member of that family for where the partition lands */
    if(t==0x04||t==0x06||t==0x0E) t=MGPT_TYPE_FAT16;
    else if(t==0x0B||t==0x0C) t=MGPT_TYPE_FAT32;
    else if(t==0x05||t==0x0F) t=MGPT_TYPE_EXTENDED;
    *out=t; return 1;
}
static int vNew(void){
    DWORD t,v=0; mgpt_u64 mb; char b[32]; int i;
    if(!parseNewType(&t)){ dlgErr("Pick a file system from the list, or enter a partition type in hex (01 - FF)."); SetFocus(g_cType); return 0; }
    if(g_ctxGpt&&t==MGPT_TYPE_EXTENDED){ dlgErr("A GPT disk has no extended partitions. Choose a file system such as FAT16, FAT32 or NTFS."); SetFocus(g_cType); return 0; }
    GetWindowTextA(g_eSize,b,32);
    for(i=0;b[i]==' ';i++);
    for(;b[i];i++){ if(b[i]<'0'||b[i]>'9'){ dlgErr("Enter the size as a whole number, or leave it empty to use all the free space."); SetFocus(g_eSize); return 0; } v=v*10+(DWORD)(b[i]-'0'); }
    GetWindowTextA(g_cUnit,b,32);
    for(i=0;b[i]==' ';i++);
    if(b[i]=='K'||b[i]=='k') g_vUnit=0;
    else if(b[i]=='M'||b[i]=='m'||b[i]==0) g_vUnit=1;
    else if(b[i]=='G'||b[i]=='g') g_vUnit=2;
    else if(b[i]=='T'||b[i]=='t') g_vUnit=3;
    else if(b[i]=='P'||b[i]=='p') g_vUnit=4;
    else { dlgErr("The unit must be KB, MB, GB, TB or PB."); SetFocus(g_cUnit); return 0; }
    if(v>(DWORD)0x3FFFFF&&g_vUnit==4){ dlgErr("That size is too large."); SetFocus(g_eSize); return 0; }
    /* the partition's size in MB, for the file-system limits */
    if(!v) mb=g_ctxFreeMB;
    else mb=unitBytes(v,g_vUnit)>>20;
    if(t==MGPT_TYPE_FAT16&&mb>4095){
        dlgErr(v?"A FAT16 partition can be at most 4095 MB. Enter a smaller size, or choose FAT32 or NTFS."
                :"The free area is larger than FAT16 allows (4095 MB). Enter a size of at most 4095 MB, or choose FAT32 or NTFS.");
        SetFocus(g_eSize); return 0;
    }
    if(t==MGPT_TYPE_FAT32&&mb<33){ dlgErr("A FAT32 partition needs at least 33 MB. Enter a larger size, or choose FAT16."); SetFocus(g_eSize); return 0; }
    if(v&&mb>g_ctxFreeMB){
        char e[200], sz[24]; sizestr(g_ctxFreeMB<<11,512,sz);
        wsprintfA(e,"The largest free area on this disk is %s (%lu MB). Enter a smaller size, or leave the size empty to use all of it.",sz,u64lo(g_ctxFreeMB));
        dlgErr(e); SetFocus(g_eSize); return 0;
    }
    g_vType=t; g_vSize=v;
    return 1;
}

/* ---- Change type ---- */
static void bType(int w){
    int y=74; char cur[8];
    dmk("STATIC","Current type:",SS_LEFT,16,y,108,16,300,g_font);
    dmk("STATIC",g_curText,SS_LEFT,128,y,w-144,16,305,g_fontB);
    y+=28;
    dmk("STATIC","New type:",SS_LEFT,16,y+4,108,16,306,g_font);
    g_cType=dmk("COMBOBOX","",CBS_DROPDOWN|WS_VSCROLL|WS_TABSTOP,128,y,w-144,140,301,g_font);
    fillTypes(g_cType);
    wsprintfA(cur,"%02X",g_ctxCurType); SetWindowTextA(g_cType,cur);
    y+=30;
    g_hint=dmk("STATIC","Type over the value or pick one from the list - e.g. 04, 06 or 0E for FAT16, 0B or 0C for FAT32, 07 for NTFS.",SS_LEFT,128,y,w-144,44,390,g_font);
    g_dlgFirst=g_cType;
}
static int vType(void){
    DWORD t;
    if(!parseHex(g_cType,&t)||t==0||t>0xFF||t==0xEE){ dlgErr("Enter a valid partition type in hex (01 - FF), for example 06, 07 or 0C."); SetFocus(g_cType); return 0; }
    if(t==g_ctxCurType){ dlgErr("The new type is the same as the current one. Enter a different value."); SetFocus(g_cType); return 0; }
    if(isExtType(t)){ dlgErr("A partition holding data cannot be turned into an extended container."); SetFocus(g_cType); return 0; }
    if(!g_ctxGpt&&typeFamily(t)&&g_ctxLay){
        DWORD rec=pickType(g_ctxLay,typeFamily(t),g_ctxStart,g_ctxCount);
        if(rec!=t){
            char m[400]; MGPT_PART tmp; char why[300]; int r;
            tmp.MbrType=rec; tmp.StartLba=g_ctxStart; tmp.SectorCount=g_ctxCount;
            typeReason(g_ctxLay,&tmp,why," is right");
            wsprintfA(m,"For this partition the right %s type is %02X, not %02X.\n\n%s\n\nUse %02X instead?",
                typeFamily(t)==MGPT_TYPE_FAT32?"FAT32":"FAT16",rec,t,why,rec);
            r=MessageBoxA(g_dlg,m,"MBRGPTNT",MB_YESNOCANCEL|MB_ICONEXCLAMATION);
            if(r==IDYES) t=rec; else if(r!=IDNO){ SetFocus(g_cType); return 0; }
            if(t==g_ctxCurType){ dlgErr("That is the current type already."); SetFocus(g_cType); return 0; }
        }
    }
    g_vType=t; return 1;
}

/* ---- Change drive letter ---- */
static void bLetter(int w){
    int y=74, n=0, sel=0; DWORD mask=GetLogicalDrives(); char c, s[32];
    dmk("STATIC","Current letter:",SS_LEFT,16,y,108,16,300,g_font);
    dmk("STATIC",g_curText,SS_LEFT,128,y,w-144,16,305,g_fontB);
    y+=24;
    g_hint=dmk("STATIC","Only free letters are listed. Choose (no letter) to remove the partition's letter.",SS_LEFT,128,y,w-144,30,390,g_font);
    y+=38;
    dmk("STATIC","New letter:",SS_LEFT,16,y+4,108,16,306,g_font);
    /* an editable combo: pick from the list or type the letter. (A
       drop-down-list combo on NT 3.1 keeps its whole dropped area as its
       window, which would hide the controls below it.) */
    g_cLetter=dmk("COMBOBOX","",CBS_DROPDOWN|WS_VSCROLL|WS_TABSTOP,128,y,130,120,311,g_font);
    SendMessageA(g_cLetter,CB_ADDSTRING,0,(LPARAM)"(no letter)"); g_letterMap[n++]=0;
    for(c='C';c<='Z';c++){
        if((DWORD)c!=g_ctxCurLetter && (mask&(1UL<<(c-'A')))) continue;
        s[0]=c; s[1]=':'; s[2]=0;
        if((DWORD)c==g_ctxCurLetter){ sel=n; scat(s,"  (current)"); }
        SendMessageA(g_cLetter,CB_ADDSTRING,0,(LPARAM)s); g_letterMap[n++]=(BYTE)c;
    }
    SendMessageA(g_cLetter,CB_SETCURSEL,(WPARAM)sel,0);
    g_dlgFirst=g_cLetter;
}
static int vLetter(void){
    char b[32]; int i=0; DWORD L, mask=GetLogicalDrives();
    GetWindowTextA(g_cLetter,b,32);
    while(b[i]==' ')i++;
    if(b[i]=='('){ L=0; }
    else {
        char c=b[i];
        if(c>='a'&&c<='z')c=(char)(c-32);
        if(c<'C'||c>'Z'||(b[i+1]&&b[i+1]!=':'&&b[i+1]!=' ')){ dlgErr("Enter a drive letter from C to Z, or pick one from the list."); SetFocus(g_cLetter); return 0; }
        L=(DWORD)c;
    }
    if(L==g_ctxCurLetter){ dlgErr("That is the current letter. Pick a different one."); SetFocus(g_cLetter); return 0; }
    if(L && (mask&(1UL<<(L-'A')))){ dlgErr("That drive letter is already in use. Pick a free letter from the list."); SetFocus(g_cLetter); return 0; }
    g_vLetter=L; return 1;
}

/* ---- Initialize ---- */
static int g_ctxCapped;
static void bInit(int w){
    int y=74;
    dmk("STATIC","Partition table:",SS_LEFT,16,y+2,108,16,300,g_font);
    g_radio1=dmk("BUTTON","GPT (GUID Partition Table)",BS_AUTORADIOBUTTON|WS_GROUP|WS_TABSTOP,128,y,w-144,20,321,g_font);
    g_radio2=dmk("BUTTON","MBR (Master Boot Record)",BS_AUTORADIOBUTTON,128,y+22,w-144,20,322,g_font);
    SendMessageA(g_vScheme?g_radio1:g_radio2,BM_SETCHECK,1,0);
    g_hint=dmk("STATIC",g_ctxCapped?
        "The disk driver of this Windows release stops at 2 TiB, and this disk did not answer the 16-byte commands MBRGPTNT uses to go further, so a GPT here would cover only 2 TiB. Initialize it with GPT on another system; its partitions in the first 2 TiB are usable here.":
        "GPT: any disk size, up to 128 partitions. MBR: up to 2 TiB and 4 primary partitions, also read by the in-box disk driver.",SS_LEFT,128,y+50,w-144,g_ctxCapped?72:44,390,g_font);
    if(g_ctxCapped) EnableWindow(g_radio1,0);
    g_dlgFirst=g_vScheme?g_radio1:g_radio2;
}
static int vInit(void){ g_vScheme=(SendMessageA(g_radio1,BM_GETCHECK,0,0)==1)?1:0; return 1; }

/* ---- plain text body (delete, active, convert) ---- */
static int g_bodyH = 56;
static void bBody(int w){ dmk("STATIC",g_body,SS_LEFT,16,74,w-32,g_bodyH,300,g_font); }

/* ------------------------------------------------------------------ */
/* operations                                                         */

static void notifyDrives(void){ PostMessageA(HWND_BROADCAST,WM_DEVICECHANGE,DBT_DEVNODES_CHANGED,0); }

static void busy(const char*s){ setStatus(s); SetCursor(LoadCursorA(NULL,IDC_WAIT)); }

/* reload the model after a change, repaint, then show the result box */
static void finish(int ok,const char*msg){
    SetCursor(LoadCursorA(NULL,IDC_ARROW));
    loadModel(); fixSelection(); g_status[0]=0;
    notifyDrives();
    InvalidateRect(g_main,NULL,0); UpdateWindow(g_main);
    MessageBoxA(g_main,msg,ok?"MBRGPTNT - Completed":"MBRGPTNT - Not completed",MB_OK|(ok?MB_ICONINFORMATION:MB_ICONERROR));
}
#define ERROR_SECTOR_NOT_FOUND 27
static const char*reachText(void){
    return "The disk driver of this Windows release does not reach the end of this disk - it is larger than 2 TiB (the limit with 512-byte sectors), or a partition ends in the last partial cylinder, which the NT 3.x / 4.0 disk drivers leave out. The GPT backup copy belongs there, so MBRGPTNT does not change this partition table here: it would overwrite data. Change it on a system that reaches the whole disk (for a disk past 2 TiB: Windows XP x64 or later).";
}
static const char*deniedText(void){
    return "The partition is in use by the running system, so it cannot be changed while Windows NT is running from it.";
}

static const char*convertText(DWORD r){
    switch(r){
    case MGPT_CVT_SAME: return "The disk already uses that partition table format.";
    case MGPT_CVT_SYSTEM: return "This is the running system disk, so its partition table cannot be converted while the system is running. Boot from other media to convert it.";
    case MGPT_CVT_NOROOM_GPT: return "Cannot convert to GPT without moving data: GPT needs about 17 KB at both the start and the end of the disk for its headers, but a partition occupies that space. Shrink or move the first/last partition and try again.";
    case MGPT_CVT_TOOMANY: return "Cannot convert to MBR: there are more partitions than MBR can hold as primaries and logical drives without moving data.";
    case MGPT_CVT_TOOBIG: return "Cannot convert to MBR: a partition starts or ends beyond 2 TiB, which MBR's 32-bit addressing cannot represent.";
    case MGPT_CVT_NOEBR: return "Cannot convert to MBR without moving data: there is no free sector in front of a partition to hold a logical drive's boot record (EBR). A gap between partitions is needed to turn them into logical drives.";
    case MGPT_CVT_EMPTY: return "Cannot read the disk's current partition table.";
    case MGPT_CVT_WRITE: return "The new partition table could not be written to the disk.";
    case MGPT_CVT_WHOLE: return "This disk has no partition table: one file system covers the whole disk. Initialize it to give it a partition table (its data is lost).";
    case MGPT_CVT_REACH: return "The disk driver of this Windows release does not reach the end of this disk - it is larger than 2 TiB (the limit with 512-byte sectors), or a partition ends in the last partial cylinder, which the NT 3.x / 4.0 disk drivers leave out. The GPT backup copy belongs there, so MBRGPTNT does not change this partition table here: it would overwrite data. Change it on a system that reaches the whole disk (for a disk past 2 TiB: Windows XP x64 or later).";
    default: return "The conversion could not be completed.";
    }
}

static void cmdNew(void){
    static mgpt_u64 old[MGPT_MAX_PARTS]; static char msg[900]; static char why[320];
    MGPT_LAYOUT*l=layOf(g_selDisk); MGPT_DISKINFO*di; MGPT_CREATE c; DWORD n,i,k,ret,freeMB; mgpt_u64 freeS; char sub[160], sz[24], t[64];
    if(!l)return;
    freeS=largestFree(l); freeMB=u64lo(toMB(freeS,l->SectorSize));
    if(freeS==0){ MessageBoxA(g_main,"There is no unallocated space on this disk for a new partition.","MBRGPTNT",MB_OK|MB_ICONINFORMATION); return; }
    g_ctxGpt=l->IsGpt; g_ctxLogical=0;
    g_ctxFreeMB=toMB(freeS,l->SectorSize);
    sizestr(freeS,l->SectorSize,sz); (void)freeMB;
    wsprintfA(g_hintText,"Empty size = the largest free area (%s). FAT16 holds up to 4095 MB, FAT32 needs 33 MB or more.",sz);
    wsprintfA(sub,"Disk %d  \xB7  %s  \xB7  %s unallocated",g_selDisk,l->IsGpt?"GPT":"MBR",sz);
    if(!runDialog(IC_NEW,"Create partition",sub,"~A new partition table will be written to the disk.","Create",390,242,bNew,vNew))return;
    n=l->Count; for(i=0;i<n;i++) old[i]=l->Part[i].StartLba;
    c.DiskNumber=(DWORD)g_selDisk; c.MbrType=g_vType; c.StartLba=0; c.Boot=0;
    c.SectorCount=g_vSize?unitToSectors(g_vSize,g_vUnit,l->SectorSize):0;
    busy("Creating the partition, please wait...");
    if(!DeviceIoControl(g_drv,IOCTL_MGPT_CREATE,&c,sizeof(c),NULL,0,&ret,NULL)){
        finish(0,GetLastError()==ERROR_SECTOR_NOT_FOUND?reachText():GetLastError()==ERROR_ACCESS_DENIED?deniedText():
            "The partition could not be created. Check that the size fits in the free space. An MBR disk holds at most 4 primary partitions (the extended partition counts as one) and only one extended partition; once the 4 entries are used, new partitions can only go inside the extended partition as logical drives.");
        return;
    }
    SetCursor(LoadCursorA(NULL,IDC_ARROW));
    loadModel(); fixSelection(); g_status[0]=0; notifyDrives();
    l=layOf(g_selDisk); di=infoOf(g_selDisk);
    wsprintfA(msg,"The partition was created on Disk %d.",g_selDisk);
    if(l) for(i=0;i<l->Count;i++){
        int isNew=1;
        for(k=0;k<n;k++) if(old[k]==l->Part[i].StartLba){ isNew=0; break; }
        if(!isNew)continue;
        g_selPart=(int)i;
        partLabel(l,(int)i,t); sizestr(l->Part[i].SectorCount,l->SectorSize,sz);
        wsprintfA(msg,"%s (%s) was created on Disk %d.",t,sz,g_selDisk);
        if(l->IsGpt) scat(msg,"\n\nOn a GPT disk it is a Basic Data partition (GPT has no type byte).");
        else if(typeFamily(l->Part[i].MbrType)){ typeReason(l,&l->Part[i],why," was chosen"); scat(msg,"\n\n"); scat(msg,why); }
        else { wsprintfA(why,"\n\nType: %02X.",l->Part[i].MbrType); scat(msg,why); }
        if(isExtType(l->Part[i].MbrType)) scat(msg,"\n\nCreate logical drives inside it with New.");
        else if(l->Part[i].DriveLetter){ wsprintfA(t,"\n\nDrive letter: %c:",(char)l->Part[i].DriveLetter); scat(msg,t); }
        else if(di&&!di->Owned&&!g_oldNt) scat(msg,"\n\nThe in-box disk driver will mount it and give it a drive letter after a restart.");
        else if(l->Part[i].FsKind!=FS_EFI&&l->Part[i].MbrType!=0x83) scat(msg,"\n\nNo drive letter was free for it. Free one, then assign it with Drive Letter.");
        if(typeFamily(l->Part[i].MbrType)==MGPT_TYPE_FAT32&&g_oldNt)
            scat(msg,"\n\nWindows NT 3.x and 4.0 cannot format or read FAT32; format it from Windows 2000 / XP or another system.");
        else if(!isExtType(l->Part[i].MbrType)) scat(msg,"\n\nFormat it before use - for example in File Manager (Disk > Format Disk) or with the FORMAT command.");
        break;
    }
    InvalidateRect(g_main,NULL,0); UpdateWindow(g_main);
    MessageBoxA(g_main,msg,"MBRGPTNT - Completed",MB_OK|MB_ICONINFORMATION);
}

static void cmdDelete(void){
    static char msg[300]; MGPT_LAYOUT*l=layOf(g_selDisk); MGPT_PART*p=selPart(); MGPT_DELETE d; DWORD ret,i,oldL; char t[48], sub[160], sz[24];
    if(!l||!p)return;
    if(isExtType(p->MbrType)){
        mgpt_u64 es=p->StartLba, ee=es+p->SectorCount;
        for(i=0;i<l->Count;i++) if((l->Part[i].Flags&MGPT_F_LOGICAL)&&l->Part[i].StartLba>=es&&l->Part[i].StartLba<ee){
            MessageBoxA(g_main,"This extended partition still contains logical drives. Delete the logical drives inside it first.","MBRGPTNT",MB_OK|MB_ICONEXCLAMATION);
            return;
        }
    }
    partTitle(l,g_selPart,t); sizestr(p->SectorCount,l->SectorSize,sz);
    wsprintfA(sub,"Disk %d  \xB7  %s  \xB7  %s  \xB7  %s",g_selDisk,t,fsname(p->FsKind),sz);
    wsprintfA(g_body,"%s on Disk %d will be removed from the partition table.",t,g_selDisk);
    g_focusCancel=1; g_bodyH=40;
    if(!runDialog(IC_DELETE,"Delete partition",sub,"All data in this partition will be lost.","Delete",390,196,bBody,NULL))return;
    d.DiskNumber=(DWORD)g_selDisk; d.Index=(DWORD)g_selPart; oldL=p->DriveLetter;
    busy("Deleting the partition, please wait...");
    if(DeviceIoControl(g_drv,IOCTL_MGPT_DELETE,&d,sizeof(d),NULL,0,&ret,NULL)){
        wsprintfA(msg,"%s was deleted from Disk %d.",t,g_selDisk);
        if(oldL&&(GetLogicalDrives()&(1UL<<(oldL-'A')))){ char x[120]; wsprintfA(x,"\n\nDrive %c: was in use, so it stays available until the next restart.",(char)oldL); scat(msg,x); }
        g_selPart=-1;
        finish(1,msg);
    } else finish(0,GetLastError()==ERROR_SECTOR_NOT_FOUND?reachText():GetLastError()==ERROR_ACCESS_DENIED?deniedText():
                  GetLastError()==145?"This extended partition still contains logical drives. Delete the logical drives inside it first.":
                  "The partition could not be deleted.");
}

static void cmdActive(void){
    static char msg[200]; MGPT_LAYOUT*l=layOf(g_selDisk); MGPT_PART*p=selPart(); MGPT_SETACTIVE s; DWORD ret; int isAct; char t[48], sub[160];
    if(!l||!p)return;
    isAct=(p->Flags&MGPT_F_BOOT)?1:0;
    partTitle(l,g_selPart,t);
    wsprintfA(sub,"Disk %d  \xB7  %s  \xB7  %s",g_selDisk,t,fsname(p->FsKind));
    if(isAct) wsprintfA(g_body,"The active (boot) flag of %s is cleared. The BIOS will no longer start from this partition.",t);
    else if(l->IsGpt) wsprintfA(g_body,"The legacy-BIOS bootable attribute is set on %s.",t);
    else wsprintfA(g_body,"%s becomes the active (boot) partition of Disk %d - the one the BIOS starts from. Any other active partition on this disk is cleared.",t,g_selDisk);
    g_bodyH=56;
    if(!runDialog(IC_ACTIVE,isAct?"Mark partition inactive":"Mark partition active",sub,NULL,isAct?"Clear":"Set Active",390,190,bBody,NULL))return;
    s.DiskNumber=(DWORD)g_selDisk; s.Index=(DWORD)g_selPart; s.Active=isAct?0:1;
    busy("Writing the partition table, please wait...");
    if(DeviceIoControl(g_drv,IOCTL_MGPT_SETACTIVE,&s,sizeof(s),NULL,0,&ret,NULL)){
        wsprintfA(msg,isAct?"%s is no longer active.":"%s is now the active partition.",t);
        finish(1,msg);
    } else finish(0,GetLastError()==ERROR_SECTOR_NOT_FOUND?reachText():GetLastError()==ERROR_ACCESS_DENIED?deniedText():"The active flag could not be changed.");
}

static void cmdType(void){
    static char msg[200]; MGPT_LAYOUT*l=layOf(g_selDisk); MGPT_PART*p=selPart(); MGPT_SETTYPE s; DWORD ret; char t[48], sub[160], sz[24];
    if(!l||!p)return;
    g_ctxGpt=l->IsGpt; g_ctxLogical=(p->Flags&MGPT_F_LOGICAL)?1:0; g_ctxCurType=p->MbrType;
    g_ctxLay=l; g_ctxStart=p->StartLba; g_ctxCount=p->SectorCount;
    typeDesc(p->MbrType,g_curText);
    partTitle(l,g_selPart,t); sizestr(p->SectorCount,l->SectorSize,sz);
    wsprintfA(sub,"Disk %d  \xB7  %s  \xB7  %s  \xB7  %s",g_selDisk,t,fsname(p->FsKind),sz);
    if(!runDialog(IC_TYPE,"Change partition type",sub,"~The partition's data is not changed; only its type byte.","Apply",400,252,bType,vType))return;
    s.DiskNumber=(DWORD)g_selDisk; s.Index=(DWORD)g_selPart; s.MbrType=g_vType;
    busy("Writing the partition table, please wait...");
    if(DeviceIoControl(g_drv,IOCTL_MGPT_SETTYPE,&s,sizeof(s),NULL,0,&ret,NULL)){
        wsprintfA(msg,"The type of %s was changed from %02X to %02X.",t,g_ctxCurType,g_vType);
        if(l->IsGpt) scat(msg,"\n\nOn a GPT disk this sets the partition's type GUID; the manager keeps showing the file system it finds on the volume.");
        finish(1,msg);
    } else finish(0,GetLastError()==ERROR_SECTOR_NOT_FOUND?reachText():GetLastError()==ERROR_ACCESS_DENIED?deniedText():"The partition type could not be changed.");
}

static void cmdLetter(void){
    static char msg[200]; MGPT_LAYOUT*l=layOf(g_selDisk); MGPT_PART*p=selPart(); MGPT_SETLETTER s; DWORD ret; char t[48], sub[160];
    if(!l||!p)return;
    g_ctxCurLetter=p->DriveLetter;
    if(p->DriveLetter) wsprintfA(g_curText,"%c:",(char)p->DriveLetter); else scopy(g_curText,"(none)");
    partLabel(l,g_selPart,t);
    wsprintfA(sub,"Disk %d  \xB7  %s  \xB7  %s",g_selDisk,t,fsname(p->FsKind));
    if(!runDialog(IC_LETTER,"Change drive letter",sub,NULL,"Apply",380,240,bLetter,vLetter))return;
    s.DiskNumber=(DWORD)g_selDisk; s.Index=(DWORD)g_selPart; s.Letter=g_vLetter;
    busy("Changing the drive letter...");
    if(DeviceIoControl(g_drv,IOCTL_MGPT_SETLETTER,&s,sizeof(s),NULL,0,&ret,NULL)){
        if(!g_vLetter) wsprintfA(msg,"Drive letter %c: was removed from %s.",(char)g_ctxCurLetter,t);
        else if(!g_ctxCurLetter) wsprintfA(msg,"Drive letter %c: was assigned to %s.",(char)g_vLetter,t);
        else wsprintfA(msg,"%s now uses drive letter %c: (previously %c:).",t,(char)g_vLetter,(char)g_ctxCurLetter);
        finish(1,msg);
    } else if(GetLastError()==ERROR_ACCESS_DENIED)
        finish(0,"The running system partition's drive letter cannot be changed, because the system itself is using it.");
    else finish(0,"The drive letter could not be changed - it may already be in use by another volume.");
}

static void cmdConvert(void){
    static char msg[300]; MGPT_LAYOUT*l=layOf(g_selDisk); MGPT_CONVERT cv; DWORD ret; int toGpt; char sub[160], sz[24];
    if(!l)return;
    toGpt=l->IsGpt?0:1;
    sizestr(l->TotalSectors,l->SectorSize,sz);
    wsprintfA(sub,"Disk %d  \xB7  %s  \xB7  %s  \xB7  %lu partition(s)",g_selDisk,l->IsGpt?"GPT":"MBR",sz,l->Count);
    wsprintfA(g_body,"The partition table of Disk %d is converted from %s to %s. Every partition keeps its position and its data; only the table format changes. If that is not possible without moving data, nothing is changed and the reason is shown.",
        g_selDisk,toGpt?"MBR":"GPT",toGpt?"GPT":"MBR");
    g_bodyH=72;
    if(!runDialog(IC_CONVERT,toGpt?"Convert to GPT":"Convert to MBR",sub,NULL,"Convert",400,206,bBody,NULL))return;
    cv.DiskNumber=(DWORD)g_selDisk; cv.Scheme=(DWORD)toGpt; cv.Reason=0;
    busy("Converting the partition table, please wait...");
    if(!DeviceIoControl(g_drv,IOCTL_MGPT_CONVERT,&cv,sizeof(cv),&cv,sizeof(cv),&ret,NULL)){ finish(0,"The conversion could not be completed (driver error)."); return; }
    if(cv.Reason!=MGPT_CVT_OK){ finish(0,convertText(cv.Reason)); return; }
    g_selPart=-1;
    SetCursor(LoadCursorA(NULL,IDC_ARROW));
    loadModel(); fixSelection(); g_status[0]=0; notifyDrives();
    wsprintfA(msg,"Disk %d was converted to %s. All partitions kept their position and data.",g_selDisk,toGpt?"GPT":"MBR");
    { MGPT_DISKINFO*di=infoOf(g_selDisk);
      MGPT_LAYOUT*nl=layOf(g_selDisk); DWORD j; int stale=0;
      if(nl&&di&&!di->Owned) for(j=0;j<nl->Count;j++) if(nl->Part[j].DriveLetter) stale=1;
      if(di&&!di->Owned&&!g_oldNt) scat(msg,"\n\nThe disk is now handled by the in-box disk driver, which gives its partitions drive letters after a restart.");
      if(stale) scat(msg," Volumes that are in use keep their current letters until then."); }
    InvalidateRect(g_main,NULL,0); UpdateWindow(g_main);
    MessageBoxA(g_main,msg,"MBRGPTNT - Completed",MB_OK|MB_ICONINFORMATION);
}

static void cmdInit(void){
    static char msg[200]; MGPT_LAYOUT*l=layOf(g_selDisk); MGPT_INIT in; DWORD ret; char sub[160], sz[24], wr[96];
    if(!l)return;
    sizestr(l->TotalSectors,l->SectorSize,sz);
    wsprintfA(sub,"Disk %d  \xB7  %s  \xB7  currently %s with %lu partition(s)",g_selDisk,sz,l->IsGpt?"GPT":"MBR",l->Count);
    wsprintfA(wr,"All partitions and data on Disk %d will be erased.",g_selDisk);
    g_ctxCapped=(l->Reach&MGPT_R_CAPPED)?1:0;
    g_vScheme=g_ctxCapped?0:1;
    if(!runDialog(IC_INIT,"Initialize disk",sub,wr,"Initialize",400,g_ctxCapped?274:246,bInit,vInit))return;
    in.DiskNumber=(DWORD)g_selDisk; in.Scheme=g_vScheme;
    busy("Initializing the disk, please wait...");
    if(DeviceIoControl(g_drv,IOCTL_MGPT_INIT,&in,sizeof(in),NULL,0,&ret,NULL)){
        wsprintfA(msg,"Disk %d was initialized as an empty %s disk.\n\nCreate partitions on it with New.",g_selDisk,g_vScheme?"GPT":"MBR");
        g_selPart=-1;
        finish(1,msg);
    } else finish(0,GetLastError()==ERROR_SECTOR_NOT_FOUND?"The disk driver of this Windows release reaches only the first 2 TiB of this disk (or reports no size at all); the disk is larger. A GPT is not written here: its backup copy would land in the middle of the disk. Initialize it with GPT on a system that reaches the whole disk (Windows XP x64 or later), or choose MBR.":
                  GetLastError()==ERROR_ACCESS_DENIED?"This disk holds the running system, so it cannot be initialized.":"The disk could not be initialized.");
}


/* ---- dynamic volumes ---- */

static const char*ldmReasonText(DWORD r){
    switch(r){
    case MGPT_LDMR_PARAM:    return "The request does not fit this volume type: check the number of disks and the sizes.";
    case MGPT_LDMR_SPACE:    return "There is not enough free space on one of the disks for the size asked for.";
    case MGPT_LDMR_MISSING:  return "A disk of this disk group is not connected. The disk group can only be changed when all of its disks are present.";
    case MGPT_LDMR_INUSE:    return "The volume is in use: a program has a file on it open. Close it and try again.";
    case MGPT_LDMR_NOROOM:   return "There is no room for the dynamic disk database: it needs 1 MB at the end of the disk (MBR), or a reserved partition of at least 2 MB or 1 MB of free space (GPT).";
    case MGPT_LDMR_LOGICAL:  return "The disk has an extended partition with logical drives. Only primary partitions can become dynamic volumes; delete or move the logical drives first.";
    case MGPT_LDMR_NATIVE:   return "Windows itself manages this dynamic disk (its own Logical Disk Manager). Use Windows' Disk Management for it.";
    case MGPT_LDMR_WRITE:    return "The dynamic disk database could not be written.";
    case MGPT_LDMR_FULL:     return "The dynamic disk database is full.";
    case MGPT_LDMR_SYSTEM:   return "This is the running system disk; it cannot be converted while the system runs from it.";
    case MGPT_LDMR_SECTOR:   return "Dynamic disks need 512-byte sectors; this disk uses another sector size.";
    case MGPT_LDMR_NOTEMPTY: return "The disk still holds dynamic volumes. Delete them first.";
    case MGPT_LDMR_SAME:     return "The disk is already of that kind.";
    case MGPT_LDMR_FOREIGN:  return "Another disk group (managed by Windows) is present. MBRGPTNT keeps one disk group of its own.";
    case MGPT_LDMR_REACH:    return "A disk of this group is larger than the disk driver of this Windows release can address (2 TiB with 512-byte sectors), so the group is not changed here. Volume pieces past that point are treated as missing.";
    }
    return "The operation could not be completed.";
}

static HWND  g_cDisk[8];
static int   g_cDiskIdx[8], g_nDiskBox;
static MGPT_LDMGROUP *g_ctxGrp;
static DWORD g_vNd, g_vDisk[MGPT_LDM_DISKS]; static mgpt_u64 g_vSz[MGPT_LDM_DISKS]; static DWORD g_vLType;
static const char *g_volTypes[] = { "Simple", "Spanned", "Striped", "Mirror", "RAID-5" };

static void bVol(int w){
    int y=74, i; char b[96], fr[24];
    dmk("STATIC","Volume type:",SS_LEFT,16,y+4,100,16,300,g_font);
    g_cType=dmk("COMBOBOX","",CBS_DROPDOWN|WS_VSCROLL|WS_TABSTOP,120,y,w-136,120,301,g_font);
    for(i=0;i<5;i++) SendMessageA(g_cType,CB_ADDSTRING,0,(LPARAM)g_volTypes[i]);
    SetWindowTextA(g_cType,"Simple");
    y+=32;
    dmk("STATIC","Size:",SS_LEFT,16,y+4,100,16,304,g_font);
    g_eSize=dmk("EDIT","",ES_AUTOHSCROLL|WS_BORDER|WS_TABSTOP,120,y,90,22,302,g_font);
    dmk("STATIC","MB  (empty = as large as possible)",SS_LEFT,216,y+4,w-232,16,305,g_font);
    y+=32;
    dmk("STATIC","Disks:",SS_LEFT,16,y+2,100,16,306,g_font);
    for(i=0;i<g_nDiskBox;i++){
        MGPT_LDMDISK*d=&g_ctxGrp->Disk[g_cDiskIdx[i]];
        sizestr(d->FreeLargest,512,fr);
        wsprintfA(b,"Disk %lu  (%s, %s free)",d->NtDisk,d->Name,fr);
        g_cDisk[i]=dmk("BUTTON",b,BS_AUTOCHECKBOX|WS_TABSTOP,120,y,w-136,20,330+i,g_font);
        if((int)d->NtDisk==g_selDisk) SendMessageA(g_cDisk[i],BM_SETCHECK,1,0);
        y+=22;
    }
    y+=4;
    g_hint=dmk("STATIC","Simple: one disk.  Spanned: free space of several disks, one after another.  Striped: equal parts on 2 or more disks.  Mirror: the same data on 2 disks.  RAID-5: 3 or more disks with parity.",SS_LEFT,120,y,w-136,58,390,g_font);
    g_dlgFirst=g_cType;
}

static int vVol(void){
    char b[48]; int i; DWORD v=0, n=0, k, t=0; mgpt_u64 sum=0, rem, want, per, minFree=0;
    GetWindowTextA(g_cType,b,48);
    for(i=0;i<5;i++) if(startsWith(b,g_volTypes[i])) t=(DWORD)(i+1);
    if(!t){ dlgErr("Pick a volume type from the list."); SetFocus(g_cType); return 0; }
    GetWindowTextA(g_eSize,b,48);
    for(i=0;b[i]==' ';i++);
    for(;b[i];i++){ if(b[i]<'0'||b[i]>'9'){ dlgErr("Enter the size in MB as a whole number, or leave it empty for the largest volume possible."); SetFocus(g_eSize); return 0; } v=v*10+(DWORD)(b[i]-'0'); }
    for(i=0;i<g_nDiskBox;i++) if(SendMessageA(g_cDisk[i],BM_GETCHECK,0,0)==1){
        MGPT_LDMDISK*d=&g_ctxGrp->Disk[g_cDiskIdx[i]];
        if(d->FreeLargest==0){ wsprintfA(b,"Disk %lu has no free space.",d->NtDisk); dlgErr(b); return 0; }
        g_vDisk[n]=(DWORD)g_cDiskIdx[i]; g_vSz[n]=d->FreeLargest;
        if(!n||d->FreeLargest<minFree) minFree=d->FreeLargest;
        sum+=d->FreeLargest; n++;
    }
    want=(mgpt_u64)v*2048;
    switch(t){
    case MGPT_LDM_SIMPLE:
        if(n!=1){ dlgErr("A simple volume uses exactly one disk. Tick one disk."); return 0; }
        if(want){ if(want>g_vSz[0]){ dlgErr("The size is larger than the free space on that disk."); SetFocus(g_eSize); return 0; } g_vSz[0]=want; }
        break;
    case MGPT_LDM_SPANNED:
        if(n<2){ dlgErr("A spanned volume uses two or more disks. Tick at least two disks."); return 0; }
        if(want){
            if(want>sum){ dlgErr("The size is larger than the free space on the ticked disks together."); SetFocus(g_eSize); return 0; }
            for(rem=want,k=0;k<n;k++){ if(g_vSz[k]>rem)g_vSz[k]=rem; rem-=g_vSz[k]; }
            k=n; while(k>0&&g_vSz[k-1]==0) k--;
            n=k;
            if(n<2){ dlgErr("That size fits on the first disk alone: choose Simple, or a larger size."); SetFocus(g_eSize); return 0; }
        }
        break;
    case MGPT_LDM_STRIPED: case MGPT_LDM_RAID5: case MGPT_LDM_MIRROR: {
        DWORD cols=(t==MGPT_LDM_MIRROR)?1:(t==MGPT_LDM_RAID5)?n-1:n;
        if(t==MGPT_LDM_STRIPED&&n<2){ dlgErr("A striped volume uses two or more disks. Tick at least two disks."); return 0; }
        if(t==MGPT_LDM_MIRROR&&n!=2){ dlgErr("A mirrored volume uses exactly two disks. Tick two disks."); return 0; }
        if(t==MGPT_LDM_RAID5&&n<3){ dlgErr("A RAID-5 volume uses three or more disks. Tick at least three disks."); return 0; }
        per=want?udiv64(want+cols-1,cols):minFree;
        if(t!=MGPT_LDM_MIRROR) per=want?((per+127)&~(mgpt_u64)127):(per&~(mgpt_u64)127);
        if(per==0||per>minFree){ dlgErr("The size is larger than the free space allows: each disk needs an equal part, and the smallest free area limits it."); SetFocus(g_eSize); return 0; }
        for(k=0;k<n;k++) g_vSz[k]=per;
        break; }
    }
    g_vNd=n; g_vLType=t; return 1;
}

static void cmdNewVolume(void){
    static char msg[600]; MGPT_LAYOUT*l=layOf(g_selDisk); MGPT_LDMGROUP*g=grpOf(l); static MGPT_LDMCREATE c;
    DWORD i, ret, gi; char sub[160], sz[24], ds[80], oldNames[MGPT_LDM_VOLS][16]; DWORD nOld;
    if(!l||!g)return;
    gi=l->LdmGroup; g_ctxGrp=g; g_nDiskBox=0;
    for(i=0;i<g->NDisks&&i<MGPT_LDM_DISKS&&g_nDiskBox<8;i++) if(g->Disk[i].NtDisk!=0xFFFFFFFFUL) g_cDiskIdx[g_nDiskBox++]=(int)i;
    nOld=g->NVols; for(i=0;i<nOld&&i<MGPT_LDM_VOLS;i++) scopy(oldNames[i],g->Vol[i].Name);
    wsprintfA(sub,"Disk group %s  \xB7  %lu disk(s)  \xB7  %lu volume(s)",g->Name,g->NDisks,g->NVols);
    if(!runDialog(IC_NEW,"Create dynamic volume",sub,"~The disk group's database is updated on all of its disks.","Create",440,74+32+32+g_nDiskBox*22+4+62+26+48+8,bVol,vVol))return;
    for(i=0;i<sizeof(c);i++)((BYTE*)&c)[i]=0;
    c.Group=gi; c.Type=g_vLType; c.NDisks=g_vNd; c.Stripe=0;
    for(i=0;i<g_vNd;i++){ c.Disk[i]=g_vDisk[i]; c.Size[i]=g_vSz[i]; }
    busy(g_vLType==MGPT_LDM_RAID5?"Creating the RAID-5 volume (its space is cleared first), please wait...":"Creating the volume, please wait...");
    if(!DeviceIoControl(g_drv,IOCTL_MGPT_LDMCREATE,&c,sizeof(c),&c,sizeof(c),&ret,NULL)){ finish(0,"The volume could not be created (driver error)."); return; }
    if(c.Reason!=MGPT_LDMR_OK){ finish(0,ldmReasonText(c.Reason)); return; }
    SetCursor(LoadCursorA(NULL,IDC_ARROW));
    loadModel(); fixSelection(); g_status[0]=0; notifyDrives();
    g=(gi<g_ldm.Count)?&g_ldm.Group[gi]:NULL;
    sizestr(c.VolSize,512,sz);
    wsprintfA(msg,"A %s volume of %s was created.",ldmTypeWord(c.Type),sz);
    if(g) for(i=0;i<g->NVols;i++){
        DWORD k; int isNew=1;
        for(k=0;k<nOld;k++) if(!scmp(oldNames[k],g->Vol[i].Name)){ isNew=0; break; }
        if(!isNew)continue;
        volDisks(gi,i,ds);
        wsprintfA(msg,"%s, a %s volume of %s, was created on %s.",g->Vol[i].Name,ldmTypeWord(c.Type),sz,ds);
        break;
    }
    if(c.Letter){ wsprintfA(sub,"\n\nDrive letter: %c:",(char)c.Letter); scat(msg,sub); }
    else scat(msg,"\n\nNo drive letter was free for it. Free one, then assign it with Drive Letter.");
    scat(msg,"\n\nFormat it before use - for example in File Manager (Disk > Format Disk) or with the FORMAT command.");
    InvalidateRect(g_main,NULL,0); UpdateWindow(g_main);
    MessageBoxA(g_main,msg,"MBRGPTNT - Completed",MB_OK|MB_ICONINFORMATION);
}

static void cmdDeleteVolume(void){
    static char msg[300]; MGPT_LAYOUT*l=layOf(g_selDisk); MGPT_PART*p=selPart(); MGPT_LDMVOL*v=volOf(p);
    static MGPT_LDMVOLREQ r; DWORD ret,i; char sub[160], sz[24], ds[80], t[40];
    if(!l||!p||!v)return;
    sizestr(v->Size,512,sz); volDisks(p->LdmGroup,p->LdmVol,ds);
    if(v->Letter) wsprintfA(t,"%s (%c:)",v->Name,(char)v->Letter); else scopy(t,v->Name);
    wsprintfA(sub,"%s  \xB7  %s volume  \xB7  %s  \xB7  %s",t,ldmTypeName(v->Type),fsname(v->FsKind),sz);
    wsprintfA(g_body,"%s will be deleted. All of its pieces are freed: %s.",t,ds);
    g_focusCancel=1; g_bodyH=40;
    if(!runDialog(IC_DELETE,"Delete dynamic volume",sub,"All data on this volume will be lost.","Delete",420,196,bBody,NULL))return;
    for(i=0;i<sizeof(r);i++)((BYTE*)&r)[i]=0;
    r.Group=p->LdmGroup; for(i=0;i<16;i++) r.Guid[i]=v->Guid[i];
    busy("Deleting the volume, please wait...");
    if(!DeviceIoControl(g_drv,IOCTL_MGPT_LDMDELETE,&r,sizeof(r),&r,sizeof(r),&ret,NULL)){ finish(0,"The volume could not be deleted (driver error)."); return; }
    if(r.Reason!=MGPT_LDMR_OK){ finish(0,ldmReasonText(r.Reason)); return; }
    wsprintfA(msg,"%s was deleted.",t);
    g_selPart=-1;
    finish(1,msg);
}

static void cmdLetterVolume(void){
    static char msg[200]; MGPT_PART*p=selPart(); MGPT_LDMVOL*v=volOf(p); static MGPT_LDMVOLREQ r; DWORD ret,i; char sub[160];
    if(!p||!v)return;
    g_ctxCurLetter=v->Letter;
    if(v->Letter) wsprintfA(g_curText,"%c:",(char)v->Letter); else scopy(g_curText,"(none)");
    wsprintfA(sub,"%s  \xB7  %s volume  \xB7  %s",v->Name,ldmTypeName(v->Type),fsname(v->FsKind));
    if(!runDialog(IC_LETTER,"Change drive letter",sub,NULL,"Apply",380,240,bLetter,vLetter))return;
    for(i=0;i<sizeof(r);i++)((BYTE*)&r)[i]=0;
    r.Group=p->LdmGroup; for(i=0;i<16;i++) r.Guid[i]=v->Guid[i]; r.Letter=g_vLetter;
    busy("Changing the drive letter...");
    if(!DeviceIoControl(g_drv,IOCTL_MGPT_LDMLETTER,&r,sizeof(r),&r,sizeof(r),&ret,NULL)||r.Reason!=MGPT_LDMR_OK){
        finish(0,r.Reason==MGPT_LDMR_INUSE?"The drive letter could not be changed - it is already in use by another volume.":ldmReasonText(r.Reason)); return; }
    if(!g_vLetter) wsprintfA(msg,"Drive letter %c: was removed from %s.",(char)g_ctxCurLetter,v->Name);
    else if(!g_ctxCurLetter) wsprintfA(msg,"Drive letter %c: was assigned to %s.",(char)g_vLetter,v->Name);
    else wsprintfA(msg,"%s now uses drive letter %c: (previously %c:).",v->Name,(char)g_vLetter,(char)g_ctxCurLetter);
    finish(1,msg);
}

static void cmdDynamic(void){
    static char msg[400]; MGPT_LAYOUT*l=layOf(g_selDisk); static MGPT_DYNAMIC r; DWORD ret,i; int toDyn; char sub[160], sz[24];
    MGPT_LDMGROUP*own=NULL;
    if(!l)return;
    toDyn=isDynamic(l)?0:1;
    for(i=0;i<g_ldm.Count;i++) if(!g_ldm.Group[i].Native){ own=&g_ldm.Group[i]; break; }
    sizestr(l->TotalSectors,l->SectorSize,sz);
    wsprintfA(sub,"Disk %d  \xB7  %s  \xB7  %s",g_selDisk,toDyn?(l->IsGpt?"GPT":"MBR"):"dynamic",sz);
    if(toDyn){
        DWORD n=0; for(i=0;i<l->Count;i++) if(!isExtType(l->Part[i].MbrType)&&l->Part[i].FsKind!=FS_MSR&&l->Part[i].FsKind!=FS_EFI) n++;
        if(own) wsprintfA(g_body,"Disk %d joins disk group %s as a dynamic disk.",g_selDisk,own->Name);
        else wsprintfA(g_body,"Disk %d becomes a dynamic disk in a new disk group.",g_selDisk);
        if(n) { char t[200]; wsprintfA(t," Its %lu partition(s) become simple volumes and keep their data and drive letters.",n); scat(g_body,t); }
        scat(g_body,l->IsGpt?" The disk group's database goes into 1 MB taken from the reserved partition (or from free space).":" The disk group's database goes into the last 1 MB of the disk.");
        g_bodyH=72;
        if(!runDialog(IC_DYNAMIC,"Convert to dynamic disk",sub,"~Volumes on the disk must not be in use while it is converted.","Convert",420,214,bBody,NULL))return;
    } else {
        wsprintfA(g_body,"Disk %d leaves its disk group and becomes an empty basic %s disk.",g_selDisk,l->IsGpt?"GPT":"MBR");
        g_bodyH=40;
        if(!runDialog(IC_DYNAMIC,"Convert to basic disk",sub,NULL,"Convert",400,176,bBody,NULL))return;
    }
    r.DiskNumber=(DWORD)g_selDisk; r.ToDynamic=(DWORD)toDyn; r.Reason=0;
    busy("Converting the disk, please wait...");
    if(!DeviceIoControl(g_drv,IOCTL_MGPT_DYNAMIC,&r,sizeof(r),&r,sizeof(r),&ret,NULL)){ finish(0,"The disk could not be converted (driver error)."); return; }
    if(r.Reason!=MGPT_LDMR_OK){ finish(0,ldmReasonText(r.Reason)); return; }
    g_selPart=-1;
    if(toDyn) wsprintfA(msg,"Disk %d is now a dynamic disk.\n\nCreate simple, spanned, striped, mirrored or RAID-5 volumes on it with New.",g_selDisk);
    else wsprintfA(msg,"Disk %d is now a basic disk.\n\nCreate partitions on it with New.",g_selDisk);
    finish(1,msg);
}

static DWORD diskMask(void){ DWORD m=0,i; for(i=0;i<g_dl.Count;i++) if(g_dl.Disk[i].DiskNumber<32) m|=1UL<<g_dl.Disk[i].DiskNumber; return m; }

/* say which disk came or went, after a reload */
static void reportDisks(DWORD before){
    DWORD after=diskMask(), add=after&~before, rem=before&~after; int i;
    for(i=0;i<32;i++){
        if(add&(1UL<<i)){ wsprintfA(g_status,"Disk %d was connected.",i); return; }
        if(rem&(1UL<<i)){ wsprintfA(g_status,"Disk %d was removed.",i); return; }
    }
}

/* Reload when the driver reports a change: a disk plugged in or removed
   (USB, card reader), or a table / letter changed by another program such
   as mgptpart. rescan=1 asks the driver to look at the disks first. Not
   while a dialog or result box is open - the next check picks it up. */
static void autoReload(int rescan){
    DWORD gen=0, ret, before;
    if(g_drv==INVALID_HANDLE_VALUE||g_busy)return;
    if(!DeviceIoControl(g_drv,rescan?IOCTL_MGPT_RESCAN:IOCTL_MGPT_GETGEN,NULL,0,&gen,sizeof(gen),&ret,NULL))return;
    if(gen==g_gen)return;
    before=diskMask();
    loadModel(); fixSelection(); g_status[0]=0; reportDisks(before);
    notifyDrives();
    redraw();
}

static void cmdRefresh(void){
    DWORD gen, ret, before=diskMask();
    if(g_drv==INVALID_HANDLE_VALUE)
        g_drv=CreateFileA("\\\\.\\MbrGptNt",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
    if(g_drv!=INVALID_HANDLE_VALUE) DeviceIoControl(g_drv,IOCTL_MGPT_RESCAN,NULL,0,&gen,sizeof(gen),&ret,NULL);
    loadModel(); fixSelection(); g_status[0]=0; reportDisks(before); redraw();
}

static void runCmd(int cmd){
    if(!cmdEnabled(cmd))return;
    g_busy++;
    switch(cmd){
    case CMD_NEW:     if(isDynamic(layOf(g_selDisk))) cmdNewVolume(); else cmdNew(); break;
    case CMD_DELETE:  if(volOf(selPart())) cmdDeleteVolume(); else cmdDelete(); break;
    case CMD_ACTIVE:  cmdActive(); break;
    case CMD_TYPE:    cmdType(); break;
    case CMD_LETTER:  if(volOf(selPart())) cmdLetterVolume(); else cmdLetter(); break;
    case CMD_DYNAMIC: cmdDynamic(); break;
    case CMD_CONVERT: cmdConvert(); break;
    case CMD_INIT:    cmdInit(); break;
    case CMD_REFRESH: cmdRefresh(); break;
    }
    g_busy--;
    redraw();
}

static void contextMenu(int x,int y){
    HMENU m=CreatePopupMenu(); POINT pt; MGPT_PART*p=selPart(); char cv[32];
    if(!m)return;
    if(isDynamic(layOf(g_selDisk))){
        AppendMenuA(m,MF_STRING|(cmdEnabled(CMD_NEW)?0:MF_GRAYED),CMD_NEW,"&New volume...");
        AppendMenuA(m,MF_STRING|(cmdEnabled(CMD_DELETE)?0:MF_GRAYED),CMD_DELETE,"&Delete volume...");
        AppendMenuA(m,MF_STRING|(cmdEnabled(CMD_LETTER)?0:MF_GRAYED),CMD_LETTER,"Change drive &letter...");
        AppendMenuA(m,MF_SEPARATOR,0,NULL);
        AppendMenuA(m,MF_STRING|(cmdEnabled(CMD_DYNAMIC)?0:MF_GRAYED),CMD_DYNAMIC,"Convert to &basic disk...");
        AppendMenuA(m,MF_STRING|(cmdEnabled(CMD_INIT)?0:MF_GRAYED),CMD_INIT,"&Initialize disk...");
        AppendMenuA(m,MF_SEPARATOR,0,NULL);
        AppendMenuA(m,MF_STRING,CMD_REFRESH,"&Refresh\tF5");
        pt.x=x; pt.y=y; ClientToScreen(g_main,&pt);
        TrackPopupMenu(m,TPM_RIGHTBUTTON,(int)pt.x,(int)pt.y,0,g_main,NULL);
        DestroyMenu(m);
        return;
    }
    AppendMenuA(m,MF_STRING|(cmdEnabled(CMD_NEW)?0:MF_GRAYED),CMD_NEW,"&New partition...");
    AppendMenuA(m,MF_STRING|(cmdEnabled(CMD_DELETE)?0:MF_GRAYED),CMD_DELETE,"&Delete partition...");
    AppendMenuA(m,MF_STRING|(cmdEnabled(CMD_ACTIVE)?0:MF_GRAYED),CMD_ACTIVE,(p&&(p->Flags&MGPT_F_BOOT))?"Mark &inactive...":"Mark as &active...");
    AppendMenuA(m,MF_STRING|(cmdEnabled(CMD_TYPE)?0:MF_GRAYED),CMD_TYPE,"Change &type...");
    AppendMenuA(m,MF_STRING|(cmdEnabled(CMD_LETTER)?0:MF_GRAYED),CMD_LETTER,"Change drive &letter...");
    AppendMenuA(m,MF_SEPARATOR,0,NULL);
    { MGPT_LAYOUT*l=layOf(g_selDisk); scopy(cv,(l&&l->IsGpt)?"&Convert to MBR...":"&Convert to GPT..."); }
    AppendMenuA(m,MF_STRING|(cmdEnabled(CMD_CONVERT)?0:MF_GRAYED),CMD_CONVERT,cv);
    AppendMenuA(m,MF_STRING|(cmdEnabled(CMD_DYNAMIC)?0:MF_GRAYED),CMD_DYNAMIC,"Convert to d&ynamic disk...");
    AppendMenuA(m,MF_STRING|(cmdEnabled(CMD_INIT)?0:MF_GRAYED),CMD_INIT,"&Initialize disk...");
    AppendMenuA(m,MF_SEPARATOR,0,NULL);
    AppendMenuA(m,MF_STRING,CMD_REFRESH,"&Refresh\tF5");
    pt.x=x; pt.y=y; ClientToScreen(g_main,&pt);
    TrackPopupMenu(m,TPM_RIGHTBUTTON,(int)pt.x,(int)pt.y,0,g_main,NULL);
    DestroyMenu(m);
}

/* ------------------------------------------------------------------ */
/* main window                                                        */

static void invalidateToolbar(void){ RECT r; GetClientRect(g_main,&r); r.bottom=TB_H; InvalidateRect(g_main,&r,0); }

static LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    int px=(int)(short)(lp&0xFFFF), py=(int)(short)((lp>>16)&0xFFFF);
    switch(msg){
    case WM_CREATE:
        g_main=hwnd;
        loadModel(); selectDefault();
        SetTimer(hwnd,GEN_TIMER,1500,NULL);
        return 0;
    case WM_DEVICECHANGE:
        /* Windows 2000 / XP: a device came or went - look a moment later,
           once the disk driver has its device ready (or gone) */
        if(wp==DBT_DEVICEARRIVAL||wp==DBT_DEVICEREMOVECOMPLETE||wp==DBT_DEVNODES_CHANGED)
            SetTimer(hwnd,DEV_TIMER,700,NULL);
        return 1;
    case WM_SIZE: InvalidateRect(hwnd,NULL,0); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc=BeginPaint(hwnd,&ps); RECT rc; GetClientRect(hwnd,&rc);
        if(rc.right>0&&rc.bottom>0){
            HDC mdc=CreateCompatibleDC(dc); HBITMAP bm=CreateCompatibleBitmap(dc,rc.right,rc.bottom);
            if(mdc&&bm){
                HGDIOBJ ob=SelectObject(mdc,bm);
                paintAll(hwnd,mdc);
                BitBlt(dc,0,0,rc.right,rc.bottom,mdc,0,0,SRCCOPY);
                SelectObject(mdc,ob);
            } else paintAll(hwnd,dc);
            if(bm)DeleteObject(bm); if(mdc)DeleteDC(mdc);
        }
        EndPaint(hwnd,&ps); return 0;
    }
    case WM_MOUSEMOVE: {
        int h;
        if(g_sbAct!=SB_NONE&&!(wp&1)){   /* button released elsewhere (capture lost) */
            g_sbAct=SB_NONE; KillTimer(hwnd,SB_TIMER); ReleaseCapture(); redraw();
        }
        if(g_sbAct!=SB_NONE){
            g_sbMy=py;
            if(g_sbAct==SB_THUMB){
                int trk=g_sb.y1-g_sb.y0-2*SB_W-(g_sb.t1-g_sb.t0), maxTop=g_nrows-g_vis, pos=py-g_sbDrag-(g_sb.y0+SB_W);
                if(trk>0&&maxTop>0){ int t=(pos*maxTop+trk/2)/trk; if(t<0)t=0; if(t>maxTop)t=maxTop; if(t!=g_rowTop){ g_rowTop=t; redraw(); } }
            }
            return 0;
        }
        h=tbHit(px,py);
        if(h!=g_hot){ g_hot=h; invalidateToolbar(); }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int h=tbHit(px,py), d, p;
        SetFocus(hwnd);
        if(h>=0){ if(cmdEnabled(g_tbdef[h].cmd)){ g_down=h; g_hot=h; SetCapture(hwnd); invalidateToolbar(); } return 0; }
        if((h=sbHit(px,py))!=SB_NONE){
            g_sbAct=h; g_sbMy=py; g_sbDrag=py-g_sb.t0;
            SetCapture(hwnd);
            if(h!=SB_THUMB){ sbStep(); SetTimer(hwnd,SB_TIMER,350,NULL); }
            redraw(); return 0;
        }
        if(hitTest(px,py,&d,&p)){ g_selDisk=d; g_selPart=p; g_status[0]=0; redraw(); }
        return 0;
    }
    case WM_TIMER:
        if(wp==GEN_TIMER){ autoReload(0); return 0; }
        if(wp==DEV_TIMER){ KillTimer(hwnd,DEV_TIMER); autoReload(1); return 0; }
        if(wp==SB_TIMER){
            if(g_sbAct==SB_NONE||g_sbAct==SB_THUMB){ KillTimer(hwnd,SB_TIMER); return 0; }
            SetTimer(hwnd,SB_TIMER,60,NULL); sbStep();
        }
        return 0;
    case WM_MOUSEWHEEL: {
        int dz=(int)(short)((wp>>16)&0xFFFF);
        scrollRows(-(dz/40));           /* 3 rows per notch (120) */
        return 0;
    }
    case WM_LBUTTONUP:
        if(g_sbAct!=SB_NONE){ g_sbAct=SB_NONE; KillTimer(hwnd,SB_TIMER); ReleaseCapture(); redraw(); return 0; }
        if(g_down>=0){
            int d=g_down; g_down=-1; ReleaseCapture(); invalidateToolbar();
            if(tbHit(px,py)==d){ UpdateWindow(hwnd); runCmd(g_tbdef[d].cmd); }
        }
        return 0;
    case WM_RBUTTONDOWN: {
        int d,p;
        if(py<TB_H)return 0;
        if(hitTest(px,py,&d,&p)){ g_selDisk=d; g_selPart=p; g_status[0]=0; InvalidateRect(hwnd,NULL,0); UpdateWindow(hwnd); }
        contextMenu(px,py);
        return 0;
    }
    case WM_KEYDOWN: {
        int k=(int)wp, r;
        if(k==VK_F5){ runCmd(CMD_REFRESH); return 0; }
        if(k==VK_DELETE){ runCmd(CMD_DELETE); return 0; }
        if(k==VK_UP||k==VK_DOWN||k==VK_PRIOR||k==VK_NEXT||k==VK_HOME||k==VK_END){
            int pg=g_vis-1>0?g_vis-1:1;
            buildRows(); r=rowOfSel();
            switch(k){
            case VK_UP:    r=(r<0)?0:r-1; break;
            case VK_DOWN:  r=(r<0)?0:r+1; break;
            case VK_PRIOR: r=(r<0)?0:r-pg; break;
            case VK_NEXT:  r=(r<0)?0:r+pg; break;
            case VK_HOME:  r=0; break;
            case VK_END:   r=g_nrows-1; break;
            }
            /* Up / Down stop at the ends instead of wrapping */
            selectRow(r);
            return 0;
        }
        break;
    }
    case WM_COMMAND:
        runCmd((int)(wp&0xFFFF));
        return 0;
    case WM_CLOSE: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hwnd,msg,wp,lp);
}

void __stdcall GuiMain(void){
    WNDCLASSA wc; HWND hwnd; MSG msg; int sw, w=880, h=620, show=SW_SHOW;
    g_inst=GetModuleHandleA(NULL);
    g_oldNt=((GetVersion()&0xFF)<5)?1:0;
    g_drv=CreateFileA("\\\\.\\MbrGptNt",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
    g_font =CreateFontA(13,0,0,0,400,0,0,0,0,0,0,0,0,"MS Sans Serif");
    g_fontB=CreateFontA(13,0,0,0,700,0,0,0,0,0,0,0,0,"MS Sans Serif");
    g_fontH=CreateFontA(16,0,0,0,700,0,0,0,0,0,0,0,0,"MS Sans Serif");
    if(!g_font)g_font=(HFONT)GetStockObject(ANSI_VAR_FONT);
    if(!g_fontB)g_fontB=g_font;
    if(!g_fontH)g_fontH=g_fontB;
    g_brWhite=CreateSolidBrush(RGB(255,255,255));

    wc.style=0; wc.lpfnWndProc=(LPVOID)WndProc; wc.cbClsExtra=0; wc.cbWndExtra=0;
    wc.hInstance=g_inst; wc.hIcon=LoadIconA(g_inst,(LPCSTR)(ULONG_PTR)1); wc.hCursor=LoadCursorA(NULL,IDC_ARROW);
    wc.hbrBackground=NULL; wc.lpszMenuName=NULL; wc.lpszClassName="MbrGptNtMgr";
    RegisterClassA(&wc);
    wc.lpfnWndProc=(LPVOID)DlgProc; wc.hbrBackground=g_brWhite; wc.lpszClassName="MbrGptNtDlg";
    RegisterClassA(&wc);

    sw=GetSystemMetrics(SM_CXSCREEN);
    if(sw<=900){ show=SW_SHOWMAXIMIZED; w=sw; h=GetSystemMetrics(SM_CYSCREEN); }
    hwnd=CreateWindowExA(0,"MbrGptNtMgr","MBRGPTNT - Disk / Partition Manager",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,w,h,NULL,NULL,g_inst,NULL);
    /* the 16 x 16 picture for the title bar and the task bar (Windows NT 4.0
       and later; LoadImageA does not exist on NT 3.x) */
    { typedef HANDLE (WINAPI *LIA)(HINSTANCE,LPCSTR,UINT,int,int,UINT);
      LIA li=(LIA)GetProcAddress(GetModuleHandleA("user32.dll"),"LoadImageA");
      if(li){ HANDLE sm=li(g_inst,(LPCSTR)(ULONG_PTR)1,1 /* IMAGE_ICON */,16,16,0);
              if(sm) SendMessageA(hwnd,0x0080 /* WM_SETICON */,0 /* ICON_SMALL */,(LPARAM)sm); } }
    ShowWindow(hwnd,show); UpdateWindow(hwnd);
    while(GetMessageA(&msg,NULL,0,0)>0){ TranslateMessage(&msg); DispatchMessageA(&msg); }
    if(g_drv!=INVALID_HANDLE_VALUE)CloseHandle(g_drv);
    ExitProcess(0);
}
