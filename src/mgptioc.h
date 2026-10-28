/* mgptioc.h - control interface shared by the MBRGPTNT driver and tools.
   Opened via \\.\MbrGptNt. */

#ifndef MGPTIOC_H
#define MGPTIOC_H

#ifndef CTL_CODE
#define CTL_CODE(t,f,m,a) (((t)<<16)|((a)<<14)|((f)<<2)|(m))
#define METHOD_BUFFERED 0
#define FILE_ANY_ACCESS 0
#endif

#define MGPT_DEVTYPE 0x8000

#define IOCTL_MGPT_LIST      CTL_CODE(MGPT_DEVTYPE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MGPT_GETLAYOUT CTL_CODE(MGPT_DEVTYPE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MGPT_SETLAYOUT CTL_CODE(MGPT_DEVTYPE, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MGPT_CREATE    CTL_CODE(MGPT_DEVTYPE, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MGPT_DELETE    CTL_CODE(MGPT_DEVTYPE, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MGPT_INIT      CTL_CODE(MGPT_DEVTYPE, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MGPT_SETTYPE   CTL_CODE(MGPT_DEVTYPE, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MGPT_SETACTIVE CTL_CODE(MGPT_DEVTYPE, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MGPT_SETLETTER CTL_CODE(MGPT_DEVTYPE, 0x808, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MGPT_CONVERT   CTL_CODE(MGPT_DEVTYPE, 0x809, METHOD_BUFFERED, FILE_ANY_ACCESS)
/* Look for disks that appeared, disappeared or were swapped now (instead
   of waiting for the next check, every 2 s) and publish / remove their
   volumes. Output: ULONG change counter, as IOCTL_MGPT_GETGEN. */
#define IOCTL_MGPT_RESCAN    CTL_CODE(MGPT_DEVTYPE, 0x80A, METHOD_BUFFERED, FILE_ANY_ACCESS)
/* Output: ULONG counter that changes whenever a disk appears / disappears
   or a partition table, volume or drive letter changes. A tool polls it to
   know when to reload. */
#define IOCTL_MGPT_GETGEN    CTL_CODE(MGPT_DEVTYPE, 0x80B, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* Dynamic disks (LDM). */
#define IOCTL_MGPT_LDMINFO   CTL_CODE(MGPT_DEVTYPE, 0x80C, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MGPT_LDMCREATE CTL_CODE(MGPT_DEVTYPE, 0x80D, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MGPT_LDMDELETE CTL_CODE(MGPT_DEVTYPE, 0x80E, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MGPT_LDMLETTER CTL_CODE(MGPT_DEVTYPE, 0x80F, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MGPT_DYNAMIC   CTL_CODE(MGPT_DEVTYPE, 0x810, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define MGPT_MAX_DISKS      24   /* hard disks and CD / DVD drives */
#define MGPT_CDROM        0x100  /* DiskNumber of CD / DVD drive n: MGPT_CDROM | n */
#define MGPT_MAX_PARTS     128

#define MGPT_F_BOOT        0x01
#define MGPT_F_LOGICAL     0x02
#define MGPT_F_RECOGNIZED  0x04
#define MGPT_F_GPTENTRY    0x08
#define MGPT_F_PROTECTED   0x10   /* running system/boot partition - cannot change */
#define MGPT_F_LDM         0x20   /* an extent of a dynamic volume (see LdmVol) */
#define MGPT_F_BEYOND      0x40   /* reaches past the end the disk driver reaches */

/* MGPT_DISKINFO.Reach / MGPT_LAYOUT.Reach: the disk driver below sees only
   part of the disk (a disk past 2 TiB behind 32-bit sector numbers) */
#define MGPT_R_TABLE       0x01   /* the GPT was written for a larger disk */
#define MGPT_R_CAPPED      0x02   /* the driver's 2 TiB limit: 2^32 or 2^32 - 1
                                     sectors (or none) reported, or the device
                                     says it is larger (DeviceSectors) */
#define MGPT_R_PARTS       0x04   /* partitions reach past the reachable end */
#define MGPT_R_EXTENDED    0x08   /* the disk driver reaches only DriverSectors;
                                     MBRGPTNT reaches the rest itself */
#define MGPT_R_WHOLE       0x10   /* no partition table: one file system over
                                     the whole disk (Part[0]); only shown */

/* dynamic volume layouts and states */
#define MGPT_LDM_SIMPLE    1
#define MGPT_LDM_SPANNED   2
#define MGPT_LDM_STRIPED   3
#define MGPT_LDM_MIRROR    4
#define MGPT_LDM_RAID5     5
#define MGPT_LDMS_HEALTHY  0
#define MGPT_LDMS_DEGRADED 1          /* redundancy lost: read-only */
#define MGPT_LDMS_FAILED   2          /* members missing: not available */

/* result codes of the LDM requests (Reason) */
#define MGPT_LDMR_OK        0
#define MGPT_LDMR_PARAM     1         /* bad request (sizes, disks, type) */
#define MGPT_LDMR_SPACE     2         /* not enough free space on a disk */
#define MGPT_LDMR_MISSING   3         /* a disk of the group is not present */
#define MGPT_LDMR_INUSE     4         /* volume / disk in use */
#define MGPT_LDMR_NOROOM    5         /* no 1 MB free at the end / no MSR room */
#define MGPT_LDMR_LOGICAL   6         /* disk has logical drives */
#define MGPT_LDMR_NATIVE    7         /* Windows manages this disk group itself */
#define MGPT_LDMR_WRITE     8         /* writing failed */
#define MGPT_LDMR_FULL      9         /* database full */
#define MGPT_LDMR_SYSTEM    10        /* system disk */
#define MGPT_LDMR_SECTOR    11        /* sector size not supported */
#define MGPT_LDMR_NOTEMPTY  12        /* disk still holds volumes */
#define MGPT_LDMR_SAME      13        /* already dynamic / basic */
#define MGPT_LDMR_FOREIGN   14        /* another disk group is already present */
#define MGPT_LDMR_REACH     15        /* the disk driver does not reach the whole disk */

/* MGPT_CREATE.MbrType values above 0xFF: let the driver pick the exact
   type byte from the partition's size and position (see GptPickType) */
#define MGPT_TYPE_FAT16     0x100   /* 04, 06 or 0E */
#define MGPT_TYPE_FAT32     0x101   /* 0B or 0C     */
#define MGPT_TYPE_EXTENDED  0x102   /* 05 or 0F     */

/* reason codes returned in MGPT_CONVERT.Reason (0 = converted) */
#define MGPT_CVT_OK          0
#define MGPT_CVT_SAME        1
#define MGPT_CVT_SYSTEM      2
#define MGPT_CVT_NOROOM_GPT  3
#define MGPT_CVT_TOOMANY     4
#define MGPT_CVT_TOOBIG      5
#define MGPT_CVT_NOEBR       6
#define MGPT_CVT_EMPTY       7
#define MGPT_CVT_WRITE       8
#define MGPT_CVT_DYNAMIC     9   /* dynamic disk: convert to basic first */
#define MGPT_CVT_REACH      10   /* the disk driver does not reach the whole disk */
#define MGPT_CVT_WHOLE      11   /* no partition table (whole-disk file system) */

#if defined(_MSC_VER)
typedef unsigned __int64 mgpt_u64;
#else
typedef unsigned long long mgpt_u64;
#endif

typedef struct _MGPT_PART {
    mgpt_u64      StartLba;
    mgpt_u64      SectorCount;
    unsigned long MbrType;
    unsigned long Flags;
    unsigned long FsKind;
    unsigned long DriveLetter;   /* ASCII 'C'..'Z', 0 = none */
    /* dynamic volume extents (MGPT_F_LDM) */
    unsigned long LdmGroup;      /* index into MGPT_LDMINFO.Group */
    unsigned long LdmVol;        /* index into its Vol[] */
    unsigned long LdmType;       /* MGPT_LDM_* */
    unsigned long LdmState;      /* MGPT_LDMS_* */
    unsigned long LdmPiece;      /* column / mirror copy / piece number */
    unsigned long LdmPieces;     /* pieces of the volume */
    mgpt_u64      LdmVolSize;    /* sectors (512 bytes) of the whole volume */
    char          VolName[16];
} MGPT_PART;

typedef struct _MGPT_LAYOUT {
    unsigned long DiskNumber;
    unsigned long SectorSize;
    mgpt_u64      TotalSectors;
    unsigned long IsGpt;
    unsigned long Count;
    mgpt_u64      ChsLimit;      /* first LBA past the BIOS CHS range */
    unsigned long Dynamic;       /* 1: Part[] are the dynamic volume extents */
    unsigned long LdmGroup;      /* index into MGPT_LDMINFO.Group */
    unsigned long LdmWritable;   /* MBRGPTNT may change this disk group */
    unsigned long LdmMissing;    /* disk record of the group, disk not here */
    mgpt_u64      PubStart;      /* dynamic: area volumes live in (sectors) */
    mgpt_u64      PubSize;
    char          DgName[32];
    MGPT_PART     Part[MGPT_MAX_PARTS];
    mgpt_u64      TableSectors;  /* GPT: disk size the table was written for */
    unsigned long Reach;         /* MGPT_R_* */
    mgpt_u64      DeviceSectors; /* the device's real size, when larger than
                                    TotalSectors and known; else 0 */
    mgpt_u64      DriverSectors; /* MGPT_R_EXTENDED: what the disk driver reaches */
    unsigned long Media;         /* 0 hard disk, 1 CD / DVD drive (shown only) */
} MGPT_LAYOUT;

typedef struct _MGPT_DISKINFO {
    unsigned long DiskNumber;
    unsigned long SectorSize;
    mgpt_u64      TotalSectors;
    unsigned long IsGpt;
    unsigned long PartCount;
    unsigned long Owned;
    unsigned long Dynamic;
    mgpt_u64      TableSectors;  /* GPT: disk size the table was written for */
    unsigned long Reach;         /* MGPT_R_* */
    mgpt_u64      DeviceSectors; /* the device's real size, when larger than
                                    TotalSectors and known; else 0 */
    mgpt_u64      DriverSectors; /* MGPT_R_EXTENDED: what the disk driver reaches */
    unsigned long Media;         /* 0 hard disk, 1 CD / DVD drive (shown only) */
} MGPT_DISKINFO;

typedef struct _MGPT_DISKLIST {
    unsigned long Count;
    MGPT_DISKINFO Disk[MGPT_MAX_DISKS];
} MGPT_DISKLIST;

typedef struct _MGPT_CREATE {
    unsigned long DiskNumber;
    unsigned long MbrType;
    mgpt_u64      StartLba;       /* 0 = auto-place */
    mgpt_u64      SectorCount;    /* 0 = fill gap */
    unsigned long Boot;
} MGPT_CREATE;

typedef struct _MGPT_DELETE {
    unsigned long DiskNumber;
    unsigned long Index;
} MGPT_DELETE;

typedef struct _MGPT_INIT {
    unsigned long DiskNumber;
    unsigned long Scheme;        /* 1 = GPT, 0 = MBR */
} MGPT_INIT;

typedef struct _MGPT_SETTYPE {
    unsigned long DiskNumber;
    unsigned long Index;
    unsigned long MbrType;
} MGPT_SETTYPE;

typedef struct _MGPT_SETACTIVE {
    unsigned long DiskNumber;
    unsigned long Index;
    unsigned long Active;
} MGPT_SETACTIVE;

typedef struct _MGPT_SETLETTER {
    unsigned long DiskNumber;
    unsigned long Index;
    unsigned long Letter;        /* ASCII 'C'..'Z'; 0 = remove the letter */
} MGPT_SETLETTER;

typedef struct _MGPT_CONVERT {
    unsigned long DiskNumber;
    unsigned long Scheme;        /* target: 1 = GPT, 0 = MBR */
    unsigned long Reason;        /* out: MGPT_CVT_* (0 = converted) */
} MGPT_CONVERT;

/* ---- dynamic disks ---- */
#define MGPT_LDM_GROUPS    4
#define MGPT_LDM_DISKS     32
#define MGPT_LDM_VOLS      64

typedef struct _MGPT_LDMDISK {
    char          Name[16];      /* Disk1, ... */
    unsigned long NtDisk;        /* \Device\Harddisk<n>, 0xFFFFFFFF = missing */
    unsigned long Stale;         /* present, but its database copy is old */
    mgpt_u64      PubSize;       /* sectors (512 bytes) for volumes */
    mgpt_u64      FreeLargest;   /* largest free run, sectors */
    mgpt_u64      FreeTotal;
} MGPT_LDMDISK;

typedef struct _MGPT_LDMVOL {
    char          Name[16];
    unsigned char Guid[16];
    unsigned long Type;          /* MGPT_LDM_* */
    unsigned long State;         /* MGPT_LDMS_* */
    unsigned long Letter;
    unsigned long FsKind;
    unsigned long PartType;
    unsigned long Pieces;
    mgpt_u64      Size;          /* sectors (512 bytes) */
} MGPT_LDMVOL;

typedef struct _MGPT_LDMGROUP {
    char          Name[32];
    char          Guid[40];
    unsigned long Writable;      /* MBRGPTNT may change it (all disks present, not native) */
    unsigned long Native;        /* Windows' own LDM driver manages it: shown only */
    unsigned long NDisks, NVols;
    MGPT_LDMDISK  Disk[MGPT_LDM_DISKS];
    MGPT_LDMVOL   Vol[MGPT_LDM_VOLS];
} MGPT_LDMGROUP;

typedef struct _MGPT_LDMINFO {
    unsigned long Count;
    MGPT_LDMGROUP Group[MGPT_LDM_GROUPS];
} MGPT_LDMINFO;

typedef struct _MGPT_LDMCREATE {
    unsigned long Group;          /* index from MGPT_LDMINFO */
    unsigned long Type;           /* MGPT_LDM_* */
    unsigned long NDisks;
    unsigned long Disk[MGPT_LDM_DISKS];   /* LDM disk indices */
    mgpt_u64      Size[MGPT_LDM_DISKS];   /* sectors (512 bytes) per disk */
    mgpt_u64      Stripe;         /* sectors, 0 = 64 KB */
    unsigned long Reason;         /* out: MGPT_LDMR_* */
    unsigned long Letter;         /* out */
    mgpt_u64      VolSize;        /* out: sectors */
} MGPT_LDMCREATE;

typedef struct _MGPT_LDMVOLREQ {  /* delete / letter */
    unsigned long Group;
    unsigned char Guid[16];       /* the volume */
    unsigned long Letter;         /* letter request: 'C'..'Z', 0 = none */
    unsigned long Reason;         /* out */
} MGPT_LDMVOLREQ;

typedef struct _MGPT_DYNAMIC {
    unsigned long DiskNumber;
    unsigned long ToDynamic;      /* 1: basic -> dynamic, 0: dynamic -> basic */
    unsigned long Reason;         /* out: MGPT_LDMR_* */
} MGPT_DYNAMIC;

#endif /* MGPTIOC_H */
