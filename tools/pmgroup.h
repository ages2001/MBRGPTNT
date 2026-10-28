/* pmgroup.h - the MBRGPTNT program group, through Program Manager's DDE
   interface. Program Manager (NT 3.x) and Explorer (NT 4.0 and later, as
   a Start menu folder) both answer it. Included by mbrginst.c and
   mbrgunin.c; needs the Win32 types those files declare. */

typedef void *HSZ;
typedef void *HCONV;
typedef void *HDDEDATA;
typedef HDDEDATA (__stdcall *PFNDDECB)(unsigned int, unsigned int, HCONV, HSZ, HSZ, HDDEDATA, ULONG_PTR, ULONG_PTR);

__declspec(dllimport) unsigned int __stdcall DdeInitializeA(DWORD *, PFNDDECB, DWORD, DWORD);
__declspec(dllimport) HSZ      __stdcall DdeCreateStringHandleA(DWORD, LPCSTR, int);
__declspec(dllimport) BOOL     __stdcall DdeFreeStringHandle(DWORD, HSZ);
__declspec(dllimport) HCONV    __stdcall DdeConnect(DWORD, HSZ, HSZ, void *);
__declspec(dllimport) BOOL     __stdcall DdeDisconnect(HCONV);
__declspec(dllimport) HDDEDATA __stdcall DdeClientTransaction(BYTE *, DWORD, HCONV, HSZ, unsigned int, unsigned int, DWORD, DWORD *);
__declspec(dllimport) BOOL     __stdcall DdeFreeDataHandle(HDDEDATA);
__declspec(dllimport) BOOL     __stdcall DdeUninitialize(DWORD);

#define PM_GROUP        "MBRGPTNT"
#define CP_WINANSI      1004
#define APPCMD_CLIENTONLY 0x00000010UL
#define XTYP_EXECUTE    0x4050

static HDDEDATA __stdcall PmCallback(unsigned int t, unsigned int f, HCONV c, HSZ a, HSZ b, HDDEDATA d, ULONG_PTR x, ULONG_PTR y)
{ (void)t; (void)f; (void)c; (void)a; (void)b; (void)d; (void)x; (void)y; return 0; }

typedef struct { DWORD inst; HCONV conv; } PMCONN;

static int PmOpen(PMCONN *pm)
{
    HSZ s;
    pm->inst = 0; pm->conv = 0;
    if (DdeInitializeA(&pm->inst, PmCallback, APPCMD_CLIENTONLY, 0) != 0) return 0;
    s = DdeCreateStringHandleA(pm->inst, "PROGMAN", CP_WINANSI);
    pm->conv = DdeConnect(pm->inst, s, s, 0);
    DdeFreeStringHandle(pm->inst, s);
    if (!pm->conv) { DdeUninitialize(pm->inst); return 0; }
    return 1;
}

static int PmExec(PMCONN *pm, const char *cmd)
{
    DWORD n = 0, res = 0; HDDEDATA h;
    while (cmd[n]) n++;
    h = DdeClientTransaction((BYTE *)cmd, n + 1, pm->conv, 0, 0, XTYP_EXECUTE, 10000, &res);
    if (h) DdeFreeDataHandle(h);
    return h != 0;
}

static void PmClose(PMCONN *pm)
{
    DdeDisconnect(pm->conv);
    DdeUninitialize(pm->inst);
}

/* remove the group, as a common group and as a personal one */
static void PmDeleteGroup(PMCONN *pm)
{
    PmExec(pm, "[DeleteGroup(" PM_GROUP ",1)]");
    PmExec(pm, "[DeleteGroup(" PM_GROUP ")]");
}

#if !defined(PM_REMOVE_ONLY)
static void PmCat(char *d, const char *s) { while (*d) d++; while ((*d++ = *s++) != 0); }

/* [AddItem("cmd","name","icon",0)] */
static int PmAddItem(PMCONN *pm, const char *cmd, const char *name, const char *icon)
{
    char b[700];
    b[0] = 0;
    PmCat(b, "[AddItem(\""); PmCat(b, cmd); PmCat(b, "\",\""); PmCat(b, name);
    PmCat(b, "\",\""); PmCat(b, icon); PmCat(b, "\",0)]");
    return PmExec(pm, b);
}

/* (Re)create the group with the tools in sysdir (no trailing backslash).
   A common group when the user may create one, else a personal group. */
static int PmCreateGroup(PMCONN *pm, const char *sysdir)
{
    char gui[300], part[300], unin[300];
    gui[0] = part[0] = unin[0] = 0;
    PmCat(gui, sysdir); PmCat(gui, "\\mgptgui.exe");
    PmCat(part, sysdir); PmCat(part, "\\mgptpart.exe");
    PmCat(unin, sysdir); PmCat(unin, "\\mbrgunin.exe");
    PmDeleteGroup(pm);
    if (!PmExec(pm, "[CreateGroup(" PM_GROUP ",1)]") && !PmExec(pm, "[CreateGroup(" PM_GROUP ")]")) return 0;
    PmExec(pm, "[ShowGroup(" PM_GROUP ",1)]");
    if (!PmAddItem(pm, gui, "Disk / Partition Manager", gui)) return 0;
    PmAddItem(pm, "cmd.exe /k mgptpart", "MBRGPTNT Command Line", part);
    PmAddItem(pm, unin, "Uninstall MBRGPTNT", unin);
    return 1;
}
#endif
