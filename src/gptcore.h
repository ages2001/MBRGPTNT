/* gptcore.h - portable GPT/MBR partition parser + writer. */

#ifndef GPTCORE_H
#define GPTCORE_H

#ifndef GPT_TYPES_DEFINED
#define GPT_TYPES_DEFINED
typedef unsigned char      gpt_u8;
typedef unsigned short     gpt_u16;
typedef unsigned int       gpt_u32;
#if defined(_MSC_VER)
typedef unsigned __int64   gpt_u64;
#else
typedef unsigned long long gpt_u64;
#endif
#endif

#define GPT_MAX_PARTITIONS   128
#define GPT_LBA_INVALID      ((gpt_u64)~(gpt_u64)0)

#define GPT_OK               0
#define GPT_ERR_IO           1
#define GPT_ERR_PARAM        2
#define GPT_ERR_CORRUPT      3

#define GPT_MBRTYPE_EMPTY    0x00
#define GPT_MBRTYPE_FAT12    0x01
#define GPT_MBRTYPE_FAT16S   0x04
#define GPT_MBRTYPE_EXTENDED 0x05
#define GPT_MBRTYPE_FAT16    0x06
#define GPT_MBRTYPE_IFS      0x07
#define GPT_MBRTYPE_FAT32    0x0B
#define GPT_MBRTYPE_FAT32L   0x0C
#define GPT_MBRTYPE_FAT16L   0x0E
#define GPT_MBRTYPE_EXTENDL  0x0F
#define GPT_MBRTYPE_EFISYS   0xEF
#define GPT_MBRTYPE_GPTPROT  0xEE
#define GPT_MBRTYPE_LDM      0x42   /* dynamic disk (also used for the GPT LDM partitions) */
#define GPT_MBRTYPE_MSR      0xE3   /* pseudo type of a GPT Microsoft reserved partition */

/* Finer file-system kind, used for display (distinguishes NTFS / HPFS /
   exFAT, which all share MBR type byte 0x07). */
#define GPT_FS_UNKNOWN   0    /* RAW: no file system recognised */
#define GPT_FS_FAT12     1
#define GPT_FS_FAT16     2
#define GPT_FS_FAT32     3
#define GPT_FS_NTFS      4
#define GPT_FS_HPFS      5
#define GPT_FS_EXFAT     6
#define GPT_FS_EXTENDED  7
#define GPT_FS_EFI       8
#define GPT_FS_LINUX     9    /* type 83 with no file system recognised */
#define GPT_FS_EXT2      10
#define GPT_FS_EXT3      11
#define GPT_FS_EXT4      12
#define GPT_FS_REFS      13
#define GPT_FS_BTRFS     14
#define GPT_FS_MSR       15   /* GPT Microsoft reserved partition */
#define GPT_FS_LDMMETA   16   /* GPT LDM metadata partition */
#define GPT_FS_LDMDATA   17   /* GPT LDM data partition / MBR type 0x42 */
#define GPT_FS_KINDS     18

typedef int (*GptReadProc)(void *Ctx, gpt_u64 Lba, gpt_u32 Count, void *Buf);
typedef int (*GptWriteProc)(void *Ctx, gpt_u64 Lba, gpt_u32 Count, const void *Buf);

typedef struct _GPT_PARTITION {
    gpt_u64  StartLba;
    gpt_u64  SectorCount;
    gpt_u8   TypeGuid[16];
    gpt_u8   UniqueGuid[16];
    gpt_u8   MbrType;
    gpt_u8   FsKind;
    gpt_u8   BootIndicator;
    gpt_u8   Recognized;
    gpt_u8   IsGptEntry;
    gpt_u8   IsLogical;
    gpt_u32  GptIndex;
    gpt_u64  EbrLba;
    gpt_u64  Attributes;    /* GPT attribute bits, kept as found */
    gpt_u16  Name[36];      /* GPT partition name (UTF-16), kept as found */
} GPT_PARTITION;

typedef struct _GPT_DISK {
    gpt_u8        IsGpt;
    gpt_u8        UsedBackup;
    gpt_u8        DiskGuid[16];
    gpt_u32       SectorSize;
    gpt_u64       DiskSectors;
    gpt_u64       DiskSignatureLba0;
    /* BIOS-style geometry used for the CHS fields of MBR entries and the
       CHS / LBA type choice (0 = default 255 heads x 63 sectors) */
    gpt_u32       Heads;
    gpt_u32       SectorsPerTrack;
    gpt_u32       Count;
    GPT_PARTITION Part[GPT_MAX_PARTITIONS];
} GPT_DISK;

#ifdef __cplusplus
extern "C" {
#endif

gpt_u32 GptCrc32(const void *Data, gpt_u32 Len);

int GptParse(GptReadProc Read, void *Ctx, gpt_u32 SectorSize,
             gpt_u64 DiskSectors, GPT_DISK *Out);

int GptBuildHeaderCrc(gpt_u8 *Header, gpt_u32 HeaderSize,
                      const gpt_u8 *EntryArray, gpt_u32 EntryArrayLen);

#define GPT_SCHEME_MBR 0
#define GPT_SCHEME_GPT 1

/* lossless scheme conversion reason codes (match MGPT_CVT_* in mgptioc.h) */
#define GPT_CVT_OK          0
#define GPT_CVT_SAME        1
#define GPT_CVT_NOROOM_GPT  3
#define GPT_CVT_TOOMANY     4
#define GPT_CVT_TOOBIG      5
#define GPT_CVT_NOEBR       6
#define GPT_CVT_EMPTY       7
#define GPT_CVT_DYNAMIC     9   /* dynamic (LDM) disk */

int GptInitDisk(GPT_DISK *Disk, int Scheme, gpt_u32 SectorSize,
                gpt_u64 DiskSectors);
int GptAddPartition(GPT_DISK *Disk, gpt_u64 StartLba, gpt_u64 SectorCount,
                    gpt_u8 MbrType, gpt_u8 Bootable, gpt_u32 *OutIndex);
int GptDeletePartition(GPT_DISK *Disk, gpt_u32 Index);
int GptSetType(GPT_DISK *Disk, gpt_u32 Index, gpt_u8 MbrType);
int GptSetActive(GPT_DISK *Disk, gpt_u32 Index, gpt_u8 Active);
gpt_u64 GptLargestFreeGap(const GPT_DISK *Disk, gpt_u64 *OutStartLba);
int GptWriteDisk(GptReadProc Read, GptWriteProc Write, void *Ctx,
                 GPT_DISK *Disk);

/* Build Dst as the other partitioning scheme of Src, keeping every data
   partition at the same start LBA and length (so no data is moved). On
   failure returns non-GPT_OK and sets *Reason to a GPT_CVT_* code that
   explains why a lossless conversion is not possible. */
int GptConvertScheme(const GPT_DISK *Src, int TargetScheme, GPT_DISK *Dst,
                     int *Reason);

/* Partition "families": let the library pick the exact MBR type byte. */
#define GPT_FAMILY_FAT16     0x100   /* 04, 06 or 0E */
#define GPT_FAMILY_FAT32     0x101   /* 0B or 0C     */
#define GPT_FAMILY_EXTENDED  0x102   /* 05 or 0F     */

/* First LBA that CHS can no longer address (1024 cylinders). */
gpt_u64 GptChsLimit(const GPT_DISK *Disk);
/* The right MBR type byte for a partition of the given family:
     FAT16    04 under 32 MB (65536 sectors), 06, or 0E past the CHS range
     FAT32    0B, or 0C past the CHS range
     extended 05, or 0F past the CHS range */
gpt_u8 GptPickType(const GPT_DISK *Disk, gpt_u32 Family, gpt_u64 StartLba,
                   gpt_u64 SectorCount);
/* Family of a type byte, or 0 when it is not FAT16 / FAT32 / extended. */
gpt_u32 GptTypeFamily(gpt_u8 MbrType);
/* Identify the file system at [Start, Start+Count): sets *Kind (GPT_FS_*)
   and *Recognized, returns the fitting MBR type byte or 0. Buf: one sector. */
gpt_u8 GptDetectFs(GptReadProc Read, void *Ctx, gpt_u32 SectorSize, gpt_u64 Start,
                   gpt_u64 Count, gpt_u8 *Buf, int *Recognized, gpt_u8 *Kind);
void GptSeed(gpt_u32 Seed);

#ifdef __cplusplus
}
#endif

#endif /* GPTCORE_H */
