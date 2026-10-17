/* ldmcore.h - portable Logical Disk Manager (Windows dynamic disk) support:
   reads and writes the LDM database and maps volume I/O for simple,
   spanned, striped, mirrored and RAID-5 volumes on MBR and GPT disks. */

#ifndef LDMCORE_H
#define LDMCORE_H

#include "gptcore.h"

#define LDM_MAX_DISKS    32
#define LDM_MAX_VOLS     64
#define LDM_MAX_COMPS    96
#define LDM_MAX_PARTS    192
#define LDM_MAX_RECS     (LDM_MAX_DISKS + LDM_MAX_VOLS + LDM_MAX_COMPS + LDM_MAX_PARTS + 4)
#define LDM_MAX_PLEX     4          /* mirrors of one volume */
#define LDM_MAX_XPP      32         /* extents of one plex */

/* volume layouts */
#define LDM_VT_SIMPLE    1
#define LDM_VT_SPANNED   2
#define LDM_VT_STRIPED   3
#define LDM_VT_MIRROR    4
#define LDM_VT_RAID5     5

/* component layouts (as stored) */
#define LDM_CL_STRIPE    1
#define LDM_CL_CONCAT    2
#define LDM_CL_RAID5     3

/* error codes (besides GPT_OK / GPT_ERR_*) */
#define LDM_ERR_NOTLDM   10
#define LDM_ERR_FULL     11         /* database or table limits reached */
#define LDM_ERR_SPACE    12         /* not enough free space */
#define LDM_ERR_MISSING  13         /* a member disk is not present */
#define LDM_ERR_INUSE    14         /* disk still holds volumes */
#define LDM_ERR_NOROOM   15         /* no 1 MB at the end of the disk / MSR too small */
#define LDM_ERR_LOGICAL  16         /* basic disk has logical drives */

/* GPT partition type GUIDs (on-disk byte order) */
extern const gpt_u8 LdmGuidMetadata[16];   /* 5808C8AA-7E8F-42E0-85D2-E1E90434CFB3 */
extern const gpt_u8 LdmGuidData[16];       /* AF9B60A0-1431-4F62-BC68-3311714A69AD */
extern const gpt_u8 LdmGuidMsr[16];        /* E3C9E316-0B5C-4DB8-817D-F92DF00215AE */

/* What the private header (PRIVHEAD) and table of contents of one disk say.
   Sector values are in units of the disk's sector size. */
typedef struct _LDM_HDR {
    int      IsGpt;
    gpt_u32  SectorSize;
    gpt_u64  PubStart, PubSize;     /* public region: where volumes live */
    gpt_u64  PrivStart, PrivSize;   /* private region: the database */
    gpt_u64  PhPrim, PhSec;         /* PRIVHEAD copies, relative to PrivStart */
    gpt_u64  Toc1, Toc2;            /* TOCBLOCKs, relative to PrivStart */
    gpt_u64  ConfStart, ConfSize;   /* config region, relative to PrivStart */
    gpt_u64  LogStart, LogSize;     /* log region, relative to PrivStart */
    gpt_u64  Usn;
    char     DiskGuid[40];
    char     HostGuid[40];
    char     DgGuid[40];
    char     DgName[32];
    gpt_u8   Ph[512];               /* the PRIVHEAD used, as a template */
} LDM_HDR;

typedef struct _LDM_REC {           /* one database record (VBLK payload) */
    gpt_u8   Type;                  /* low nibble type, high nibble revision */
    gpt_u64  Id;
    gpt_u32  Len;                   /* bytes in Data, incl. the 8-byte head */
    gpt_u8  *Data;
} LDM_REC;

typedef struct _LDM_DISK {
    gpt_u64  Id;
    char     Name[32];
    gpt_u8   Guid[16];
    int      Rec;
} LDM_DISK;

typedef struct _LDM_VOL {
    gpt_u64  Id;
    char     Name[32];
    int      IsRaid5;               /* volume type string "raid5" */
    int      Type;                  /* LDM_VT_* (computed) */
    gpt_u32  NComps;
    gpt_u32  Number;
    gpt_u32  Flags;
    gpt_u64  Size;                  /* 512-byte units */
    gpt_u8   PartType;
    gpt_u8   Guid[16];
    char     Hint[8];
    int      Rec;
} LDM_VOL;

typedef struct _LDM_COMP {
    gpt_u64  Id, VolId;
    char     Name[32];
    gpt_u8   Layout;                /* LDM_CL_* */
    gpt_u32  NParts;
    gpt_u64  Stripe;                /* 512-byte units */
    gpt_u32  Columns;
    gpt_u32  Flags;                 /* 0x02 recover, 0x04 iofail */
    int      Rec;
} LDM_COMP;

typedef struct _LDM_PART {
    gpt_u64  Id, CompId, DiskId;
    char     Name[32];
    gpt_u64  Start;                 /* 512-byte units from the public region start */
    gpt_u64  ColOff;                /* offset in its column / volume (512-byte units) */
    gpt_u64  Size;                  /* 512-byte units */
    gpt_u32  Column;
    gpt_u32  Flags;                 /* 0x0008 = recover: stale copy */
    int      Rec;
} LDM_PART;

typedef struct _LDM_DB {
    char     DgName[32];
    char     DgGuid[40];
    gpt_u64  DgId;
    gpt_u64  Committed;
    gpt_u32  VblkSize;
    gpt_u32  VblkFirst;             /* byte offset of the first VBLK from the VMDB */
    gpt_u64  ConfSectors;           /* config region size, 512-byte units */
    gpt_u8   Vmdb[512];             /* template */
    gpt_u32  NRecs, NDisks, NVols, NComps, NParts;
    LDM_REC  Rec[LDM_MAX_RECS];
    LDM_DISK Disk[LDM_MAX_DISKS];
    LDM_VOL  Vol[LDM_MAX_VOLS];
    LDM_COMP Comp[LDM_MAX_COMPS];
    LDM_PART Part[LDM_MAX_PARTS];
} LDM_DB;

/* ---- volume layout, ready for I/O ---- */
typedef struct _LDM_X {             /* one extent */
    gpt_u32  Disk;                  /* index into LDM_DB.Disk */
    gpt_u32  Stale;                 /* marked for recovery: never read */
    gpt_u64  ByteStart;             /* absolute byte offset on the disk */
    gpt_u64  Size;                  /* bytes */
    gpt_u64  Off;                   /* byte offset in its column (or plex) */
    gpt_u32  Column;
} LDM_X;

typedef struct _LDM_PLEX {
    gpt_u8   Layout;                /* LDM_CL_* */
    gpt_u32  Columns;
    gpt_u32  Unit;                  /* stripe unit in bytes */
    gpt_u32  N;
    LDM_X    X[LDM_MAX_XPP];
} LDM_PLEX;

typedef struct _LDM_VMAP {
    int      Type;                  /* LDM_VT_* */
    gpt_u64  Size;                  /* bytes */
    gpt_u32  NPlex;
    LDM_PLEX Plex[LDM_MAX_PLEX];
} LDM_VMAP;

/* member disk I/O for a volume: Disk is an index into LDM_DB.Disk */
typedef int (*LdmDiskIo)(void *Ctx, gpt_u32 Disk, int Write, gpt_u64 ByteOff,
                         gpt_u32 Len, void *Buf);

#define LDM_SCRATCH_BYTES  (32 * 1024)   /* per buffer; LdmVolIo needs two */

#ifdef __cplusplus
extern "C" {
#endif

/* Is this disk dynamic, and where is its database? Table is the disk's
   parsed MBR / GPT. Returns GPT_OK or LDM_ERR_NOTLDM / GPT_ERR_*. */
int LdmReadHeader(GptReadProc Read, void *Ctx, gpt_u32 SectorSize,
                  gpt_u64 DiskSectors, const GPT_DISK *Table, LDM_HDR *Hdr);

/* Read and parse the database of a dynamic disk. LdmFreeDb releases the
   record data. */
int  LdmReadDb(GptReadProc Read, void *Ctx, const LDM_HDR *Hdr, LDM_DB *Db);
void LdmFreeDb(LDM_DB *Db);

int  LdmFindDisk(const LDM_DB *Db, const char *GuidString);   /* index or -1 */
void LdmGuidToString(const gpt_u8 *Guid, char *Out);         /* 37 bytes */
int  LdmStringToGuid(const char *Str, gpt_u8 *Guid);

/* Layout of volume VolIdx. PubStart[i] / Bps[i] give, for every disk record
   i of the database, the start of that disk's public region (in its own
   sectors) and its sector size. */
int LdmBuildMap(const LDM_DB *Db, int VolIdx, const gpt_u64 *PubStart,
                const gpt_u32 *Bps, LDM_VMAP *Map);

/* Does the volume need disk i? Is it readable with the present disks? */
int LdmMapUsesDisk(const LDM_VMAP *Map, gpt_u32 Disk);
#define LDM_HEALTHY  0
#define LDM_DEGRADED 1              /* readable, redundancy lost */
#define LDM_FAILED   2
int LdmMapState(const LDM_VMAP *Map, const gpt_u8 *Present);

/* Read or write Len bytes at byte offset Off of the volume. Off and Len
   must be multiples of 512. Scratch: 2 * LDM_SCRATCH_BYTES bytes, used for
   RAID-5 parity and reconstruction. Writes need every member present. */
int LdmVolIo(const LDM_VMAP *Map, const gpt_u8 *Present, LdmDiskIo Io,
             void *Ctx, int Write, gpt_u64 Off, gpt_u32 Len, void *Buf,
             gpt_u8 *Scratch);

/* The single contiguous disk range behind [Off, Off+Len) when there is one
   (any layout without parity, plex 0 / first present plex for reads).
   Returns 1 and sets *Disk / *ByteOff, else 0. */
int LdmMapContig(const LDM_VMAP *Map, const gpt_u8 *Present, int Write,
                 gpt_u64 Off, gpt_u32 Len, gpt_u32 *Disk, gpt_u64 *ByteOff);

/* ---- changing the database ---- */

/* free space of a disk's public region, in 512-byte units, as runs */
typedef struct _LDM_FREE { gpt_u64 Start, Size; } LDM_FREE;
int LdmFreeRuns(const LDM_DB *Db, int DiskIdx, gpt_u64 PubSize512,
                LDM_FREE *Runs, int MaxRuns);

typedef struct _LDM_NEWVOL {
    int      Type;                  /* LDM_VT_* */
    gpt_u32  NDisks;
    gpt_u32  Disk[LDM_MAX_XPP];     /* disk indices; mirror: one per plex */
    gpt_u64  Size[LDM_MAX_XPP];     /* per disk, 512-byte units (spanned: each; others: all equal) */
    gpt_u64  Stripe;                /* 512-byte units, 0 = default (128) */
    gpt_u8   PartType;              /* 0x07 etc. */
    gpt_u8   Guid[16];
    gpt_u32  Retain;                /* keep an MBR entry (converted partition) */
    gpt_u32  Fixed;                 /* Start[] given: place the extents there */
    gpt_u64  Start[LDM_MAX_XPP];    /* 512-byte units in the public region */
    char     Hint[8];               /* drive letter hint ("E:"), "" = none */
} LDM_NEWVOL;

/* PubSize512[i]: public region size of disk record i in 512-byte units.
   On success *VolIdx is the new volume. */
int LdmAddVolume(LDM_DB *Db, const LDM_NEWVOL *Spec, const gpt_u64 *PubSize512,
                 int *VolIdx);
/* change the stored partition type byte of a volume (FORMAT does this) */
int LdmSetVolumeType(LDM_DB *Db, int VolIdx, gpt_u8 PartType);
/* change the drive letter hint of a volume ("" removes it) */
int LdmSetVolumeHint(LDM_DB *Db, int VolIdx, const char *Hint);
int LdmDeleteVolume(LDM_DB *Db, int VolIdx);
/* new empty disk group */
int LdmNewGroup(LDM_DB *Db, const char *DgName, const gpt_u8 *DgGuid);
int LdmAddDisk(LDM_DB *Db, const gpt_u8 *DiskGuid, int *DiskIdx);
int LdmRemoveDisk(LDM_DB *Db, int DiskIdx);

/* Write the whole database to one disk of the group: PRIVHEADs, TOCs,
   VMDB + VBLKs, KLOG. Hdr describes that disk (from LdmReadHeader, or
   LdmMakeHeader for a disk being converted). */
int LdmWriteDisk(GptWriteProc Write, void *Ctx, const LDM_HDR *Hdr,
                 const LDM_DB *Db, gpt_u64 Now);

/* A header for a disk being made dynamic. MBR: database in the last 1 MB
   (PrivStart = DiskSectors - 1MB), public region from 63 to the last
   whole cylinder before it. GPT: database in the LDM metadata partition
   [MetaStart, MetaStart+MetaSize), public region = the LDM data partition. */
void LdmMakeHeader(LDM_HDR *Hdr, int IsGpt, gpt_u32 SectorSize,
                   gpt_u64 PubStart, gpt_u64 PubSize, gpt_u64 PrivStart,
                   gpt_u64 PrivSize, const gpt_u8 *DiskGuid, const LDM_DB *Db);

/* Wipe the PRIVHEADs and TOCBLOCKs of a disk being made basic again. */
int LdmWipeHeader(GptWriteProc Write, void *Ctx, const LDM_HDR *Hdr);

#ifdef __cplusplus
}
#endif

#endif /* LDMCORE_H */
