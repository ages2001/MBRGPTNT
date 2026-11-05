/* mgptbc.h - shared by the tools.
   - Drive letters: MBRGPTNT makes and removes its drive letters in the
     kernel, where nothing tells the shell. Explorer (NT 4.0 - XP) only
     updates My Computer and its drive lists when a WM_DEVICECHANGE volume
     message arrives, so the tools send one for every letter that came or
     went (BroadcastSystemMessage, as DefineDosDevice's documentation asks).
   - Hardware rescan: Windows 2000 / XP look for devices plugged in while
     they run only when a bus driver reports one, or when asked to - as
     Device Manager's "Scan for hardware changes" does (CM_Reenumerate_DevNode
     on the root). A disk plugged into a controller whose driver does not
     report hot-plug events appears this way when the driver finds it on a
     new scan.
   Both are looked up at run time: NT 3.x has neither, NT 4.0 has no
   cfgmgr32.dll. The including file declares GetLogicalDrives and
   GetProcAddress. */

__declspec(dllimport) void * __stdcall LoadLibraryA(const char *);

typedef struct { DWORD Size, DeviceType, Reserved, UnitMask; WORD Flags; } MG_BCVOL;
typedef long  (__stdcall *MG_BSM)(DWORD, DWORD *, unsigned int, ULONG_PTR, LONG_PTR);
typedef DWORD (__stdcall *MG_CMLOC)(DWORD *, const char *, DWORD);
typedef DWORD (__stdcall *MG_CMREN)(DWORD, DWORD);

#define MG_WM_DEVICECHANGE          0x0219
#define MG_DBT_DEVICEARRIVAL        0x8000
#define MG_DBT_DEVICEREMOVECOMPLETE 0x8004
#define MG_DBT_DEVTYP_VOLUME        2
#define MG_BSF_IGNORECURRENTTASK    0x02
#define MG_BSF_NOHANG               0x08
#define MG_BSF_FORCEIFHUNG          0x20
#define MG_BSM_APPLICATIONS         0x08
#define MG_CM_REENUMERATE_SYNCHRONOUS 1

/* tell every top-level window that the drives in mask came or went */
static void mgBroadcast(DWORD event, DWORD mask)
{
    static MG_BSM bsm; static int tried;
    MG_BCVOL v; DWORD to = MG_BSM_APPLICATIONS;
    if (!tried) {
        void *u = LoadLibraryA("user32.dll");
        tried = 1;
        if (u) {
            bsm = (MG_BSM)GetProcAddress(u, "BroadcastSystemMessageA");
            if (!bsm) bsm = (MG_BSM)GetProcAddress(u, "BroadcastSystemMessage");
        }
    }
    if (!bsm || !mask) return;
    v.Size = sizeof(v); v.DeviceType = MG_DBT_DEVTYP_VOLUME; v.Reserved = 0; v.UnitMask = mask; v.Flags = 0;
    bsm(MG_BSF_IGNORECURRENTTASK | MG_BSF_NOHANG | MG_BSF_FORCEIFHUNG, &to, MG_WM_DEVICECHANGE, event, (LONG_PTR)&v);
}

/* the drive letters as last reported */
static DWORD g_mgMask;
static void mgDrivesInit(void) { g_mgMask = GetLogicalDrives(); }

/* report the letters that came or went since the last call */
static void mgDrivesChanged(void)
{
    DWORD now = GetLogicalDrives(), add = now & ~g_mgMask, rem = g_mgMask & ~now;
    g_mgMask = now;
    if (rem) mgBroadcast(MG_DBT_DEVICEREMOVECOMPLETE, rem);
    if (add) mgBroadcast(MG_DBT_DEVICEARRIVAL, add);
}

/* Have Windows 2000 / XP look for new hardware, and wait for it.
   Returns 0 where that is not possible (NT 3.x / 4.0, no rights). */
static int mgHardwareRescan(void)
{
    void *c = LoadLibraryA("cfgmgr32.dll"); MG_CMLOC loc; MG_CMREN ren; DWORD root;
    if (!c) return 0;
    loc = (MG_CMLOC)GetProcAddress(c, "CM_Locate_DevNodeA");
    ren = (MG_CMREN)GetProcAddress(c, "CM_Reenumerate_DevNode");
    if (!loc || !ren || loc(&root, (const char *)0, 0) != 0) return 0;
    return ren(root, MG_CM_REENUMERATE_SYNCHRONOUS) == 0;
}
