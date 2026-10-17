/* gptcore.c - portable GPT/MBR partition parser + writer. See gptcore.h. */

#include "gptcore.h"

/* ---- LE readers ---- */
static gpt_u16 Le16(const gpt_u8 *p){return (gpt_u16)((gpt_u16)p[0]|((gpt_u16)p[1]<<8));}
static gpt_u32 Le32(const gpt_u8 *p){return (gpt_u32)p[0]|((gpt_u32)p[1]<<8)|((gpt_u32)p[2]<<16)|((gpt_u32)p[3]<<24);}
static gpt_u64 Le64(const gpt_u8 *p){return (gpt_u64)Le32(p)|((gpt_u64)Le32(p+4)<<32);}
static void CopyBytes(gpt_u8 *d,const gpt_u8 *s,gpt_u32 n){gpt_u32 i;for(i=0;i<n;i++)d[i]=s[i];}
static int MemEqual(const gpt_u8 *a,const gpt_u8 *b,gpt_u32 n){gpt_u32 i;for(i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
static void ZeroBytes(gpt_u8 *p,gpt_u32 n){gpt_u32 i;for(i=0;i<n;i++)p[i]=0;}

/* ---- CRC-32 (reflected, poly 0xEDB88320) ---- */
gpt_u32 GptCrc32(const void *Data, gpt_u32 Len)
{
    const gpt_u8 *p = (const gpt_u8 *)Data;
    gpt_u32 crc = 0xFFFFFFFFu, i, b;
    for (i = 0; i < Len; i++) {
        crc ^= p[i];
        for (b = 0; b < 8; b++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320u;
            else         crc = (crc >> 1);
        }
    }
    return ~crc;
}

static const gpt_u8 GptSignature[8] = {0x45,0x46,0x49,0x20,0x50,0x41,0x52,0x54};
static const gpt_u8 GuidEfiSystem[16] = {
    0x28,0x73,0x2A,0xC1,0x1F,0xF8,0xD2,0x11,0xBA,0x4B,0x00,0xA0,0xC9,0x3E,0xC9,0x3B};
static const gpt_u8 GuidMsReserved[16] = {
    0x16,0xE3,0xC9,0xE3,0x5C,0x0B,0xB8,0x4D,0x81,0x7D,0xF9,0x2D,0xF0,0x02,0x15,0xAE};

static const gpt_u8 GuidLdmMeta[16] = {
    0xAA,0xC8,0x08,0x58,0x8F,0x7E,0xE0,0x42,0x85,0xD2,0xE1,0xE9,0x04,0x34,0xCF,0xB3};
static const gpt_u8 GuidLdmData[16] = {
    0xA0,0x60,0x9B,0xAF,0x31,0x14,0x62,0x4F,0xBC,0x68,0x33,0x11,0x71,0x4A,0x69,0xAD};

static int GuidIsZero(const gpt_u8 *g){int i;for(i=0;i<16;i++)if(g[i])return 0;return 1;}

static int StrAt(const gpt_u8 *buf,int off,const char *s)
{int i;for(i=0;s[i];i++)if(buf[off+i]!=(gpt_u8)s[i])return 0;return 1;}

/* What a partition is when no file system is found on it: only the types
   that are not a file system themselves say something; anything else is
   RAW (an unformatted 06 is not "FAT16"). */
static gpt_u8 KindFromType(gpt_u8 t)
{
    switch (t) {
    case GPT_MBRTYPE_EXTENDED:
    case GPT_MBRTYPE_EXTENDL: return GPT_FS_EXTENDED;
    case GPT_MBRTYPE_EFISYS:  return GPT_FS_EFI;
    case 0x83:                return GPT_FS_LINUX;
    case GPT_MBRTYPE_LDM:     return GPT_FS_LDMDATA;
    default:                  return GPT_FS_UNKNOWN;
    }
}

/* FAT12 / FAT16 / FAT32 from a valid BIOS parameter block. The variant is
   decided by the count of clusters, as Microsoft's FAT specification
   defines it - not by the informational "FAT16   " label, which FORMAT
   does not always write. Returns 0 when this is not a FAT boot sector. */
static int FatFromBpb(const gpt_u8 *b, gpt_u8 *kind)
{
    gpt_u32 bps = Le16(b + 11), spc = b[13], rsvd = Le16(b + 14), nf = b[16];
    gpt_u32 rootEnt = Le16(b + 17), tot16 = Le16(b + 19), fsz16 = Le16(b + 22);
    gpt_u32 tot32 = Le32(b + 32), fsz32 = Le32(b + 36);
    gpt_u32 fsz, tot, rootSec, meta, clus;
    if (b[0] != 0xEB && b[0] != 0xE9) return 0;
    if (bps != 512 && bps != 1024 && bps != 2048 && bps != 4096) return 0;
    if (!spc || (spc & (spc - 1))) return 0;
    if (!rsvd || !nf || nf > 4) return 0;
    if (b[21] != 0xF0 && b[21] < 0xF8) return 0;          /* media descriptor */
    fsz = fsz16 ? fsz16 : fsz32;
    tot = tot16 ? tot16 : tot32;
    if (!fsz || !tot) return 0;
    rootSec = (rootEnt * 32 + bps - 1) / bps;
    meta = rsvd + nf * fsz + rootSec;
    if (meta >= tot) return 0;
    clus = (tot - meta) / spc;
    if (clus < 4085) *kind = GPT_FS_FAT12;
    else if (clus < 65525) *kind = GPT_FS_FAT16;
    else *kind = GPT_FS_FAT32;
    return 1;
}

/* Identify the file system on a partition. Sets *kind (GPT_FS_*) and
   *recognized (Windows NT can mount it), and returns the MBR type byte
   that fits it, or 0 when no file system is found.
     sector 0      NTFS / exFAT / ReFS (OEM id), HPFS (0x36), FAT (BPB)
     byte 1024     ext2 / ext3 / ext4 superblock (magic EF53)
     byte 8192     HPFS superblock (magic F995E849), for a damaged boot block
     byte 65536    Btrfs superblock ("_BHRfS_M" at +0x40) */
static gpt_u8 DetectFs(GptReadProc Read, void *Ctx, gpt_u32 ss, gpt_u64 start,
                       gpt_u64 count, gpt_u8 *buf, int *recognized, gpt_u8 *kind);
gpt_u8 GptDetectFs(GptReadProc Read, void *Ctx, gpt_u32 ss, gpt_u64 start,
                   gpt_u64 count, gpt_u8 *buf, int *recognized, gpt_u8 *kind)
{ return DetectFs(Read, Ctx, ss, start, count, buf, recognized, kind); }

static gpt_u8 DetectFs(GptReadProc Read, void *Ctx, gpt_u32 ss, gpt_u64 start,
                       gpt_u64 count, gpt_u8 *buf, int *recognized, gpt_u8 *kind)
{
    gpt_u64 k;
    *recognized = 0;
    *kind = GPT_FS_UNKNOWN;
    if (Read(Ctx, start, 1, buf) != 0) return 0;
    if (StrAt(buf, 3, "NTFS    "))  { *kind = GPT_FS_NTFS;  *recognized = 1; return GPT_MBRTYPE_IFS; }
    if (StrAt(buf, 3, "EXFAT   "))  { *kind = GPT_FS_EXFAT; *recognized = 1; return GPT_MBRTYPE_IFS; }
    if (StrAt(buf, 3, "ReFS") && buf[7] == 0) { *kind = GPT_FS_REFS; return GPT_MBRTYPE_IFS; }
    if (StrAt(buf, 0x36, "HPFS    ")) { *kind = GPT_FS_HPFS; *recognized = 1; return GPT_MBRTYPE_IFS; }
    if (FatFromBpb(buf, kind)) {
        *recognized = 1;
        if (*kind == GPT_FS_FAT12) return GPT_MBRTYPE_FAT12;
        if (*kind == GPT_FS_FAT16) return (count < 65536) ? GPT_MBRTYPE_FAT16S : GPT_MBRTYPE_FAT16;
        return GPT_MBRTYPE_FAT32L;
    }
    /* ext2 / ext3 / ext4: told apart the way blkid does - a journal makes
       ext3; any feature ext3 does not know (extents, 64-bit, flex_bg,
       huge files, metadata checksums, ...) makes ext4 */
    k = 1024 / ss;
    if (count > k + 1 && Read(Ctx, start + k, 1, buf) == 0) {
        const gpt_u8 *sb = buf + (1024 % ss);
        if (Le16(sb + 0x38) == 0xEF53) {
            gpt_u32 compat = Le32(sb + 0x5C), incompat = Le32(sb + 0x60), ro = Le32(sb + 0x64);
            if ((incompat & ~0x16u) || (ro & ~0x07u)) *kind = GPT_FS_EXT4;
            else if (compat & 0x04u)                   *kind = GPT_FS_EXT3;
            else                                       *kind = GPT_FS_EXT2;
            return 0x83;
        }
    }
    k = 8192 / ss;
    if (count > k + 1 && Read(Ctx, start + k, 1, buf) == 0) {
        const gpt_u8 *sb = buf + (8192 % ss);
        if (Le32(sb) == 0xF995E849u) { *kind = GPT_FS_HPFS; *recognized = 1; return GPT_MBRTYPE_IFS; }
    }
    k = 65536 / ss;
    if (count > k + 1 && Read(Ctx, start + k, 1, buf) == 0 &&
        StrAt(buf, (int)(65536 % ss) + 0x40, "_BHRfS_M")) { *kind = GPT_FS_BTRFS; return 0x83; }
    return 0;
}

#define GPT_MAX_ENTRY_BYTES  (GPT_MAX_PARTITIONS * 128)

typedef struct _GPT_SCRATCH {
    gpt_u8 Sector[4096];
    gpt_u8 Entries[GPT_MAX_ENTRY_BYTES];
    gpt_u8 Vbr[4096];
} GPT_SCRATCH;

static int ParseHeaderAt(GptReadProc Read, void *Ctx, gpt_u32 ss,
                         gpt_u64 diskSectors, gpt_u64 headerLba,
                         GPT_SCRATCH *sc, GPT_DISK *out)
{
    gpt_u8 *h = sc->Sector;
    gpt_u32 headerSize, entryCount, entrySize, i;
    gpt_u32 storedHeaderCrc, calcHeaderCrc, storedEntryCrc, calcEntryCrc;
    gpt_u64 entryLba, firstUsable, lastUsable;
    gpt_u32 entriesBytes, sectorsForEntries, got;

    if (Read(Ctx, headerLba, 1, h) != 0) return GPT_ERR_IO;
    if (!MemEqual(h, GptSignature, 8)) return GPT_ERR_CORRUPT;
    headerSize = Le32(h + 12);
    if (headerSize < 92 || headerSize > ss) return GPT_ERR_CORRUPT;
    storedHeaderCrc = Le32(h + 16);
    h[16]=h[17]=h[18]=h[19]=0;
    calcHeaderCrc = GptCrc32(h, headerSize);
    if (calcHeaderCrc != storedHeaderCrc) return GPT_ERR_CORRUPT;
    if (Le64(h + 24) != headerLba) return GPT_ERR_CORRUPT;

    entryLba   = Le64(h + 72);
    entryCount = Le32(h + 80);
    entrySize  = Le32(h + 84);
    storedEntryCrc = Le32(h + 88);
    firstUsable = Le64(h + 40);
    lastUsable  = Le64(h + 48);
    CopyBytes(out->DiskGuid, h + 56, 16);

    if (entrySize < 128 || (entrySize & 7) != 0) return GPT_ERR_CORRUPT;
    if (entryCount == 0 || entryCount > GPT_MAX_PARTITIONS) return GPT_ERR_CORRUPT;
    entriesBytes = entryCount * entrySize;
    if (entriesBytes > GPT_MAX_ENTRY_BYTES) return GPT_ERR_CORRUPT;

    sectorsForEntries = (entriesBytes + ss - 1) / ss;
    if (Read(Ctx, entryLba, sectorsForEntries, sc->Entries) != 0) return GPT_ERR_IO;
    calcEntryCrc = GptCrc32(sc->Entries, entriesBytes);
    if (calcEntryCrc != storedEntryCrc) return GPT_ERR_CORRUPT;

    out->IsGpt = 1;
    out->SectorSize = ss;
    out->DiskSectors = diskSectors;
    out->Count = 0;
    (void)firstUsable; (void)lastUsable;

    for (i = 0; i < entryCount; i++) {
        const gpt_u8 *e = sc->Entries + (gpt_u64)i * entrySize;
        gpt_u64 start, end, count, attrs;
        GPT_PARTITION *pp;
        int recog;
        if (GuidIsZero(e)) continue;
        start = Le64(e + 32);
        end   = Le64(e + 40);
        if (end < start) continue;
        count = end - start + 1;
        attrs = Le64(e + 48);
        if (out->Count >= GPT_MAX_PARTITIONS) break;
        pp = &out->Part[out->Count];
        ZeroBytes((gpt_u8 *)pp, (gpt_u32)sizeof(*pp));
        pp->StartLba = start;
        pp->SectorCount = count;
        CopyBytes(pp->TypeGuid, e, 16);
        CopyBytes(pp->UniqueGuid, e + 16, 16);
        pp->IsGptEntry = 1;
        pp->GptIndex = i;
        pp->BootIndicator = (gpt_u8)((attrs & 0x4) ? 0x80 : 0x00);
        pp->Attributes = attrs;
        { gpt_u32 k; for (k = 0; k < 36; k++) pp->Name[k] = Le16(e + 56 + 2 * k); }
        if (MemEqual(e, GuidEfiSystem, 16)) {
            pp->MbrType = GPT_MBRTYPE_EFISYS;
            pp->FsKind = GPT_FS_EFI;
            pp->Recognized = 0;
        } else if (MemEqual(e, GuidMsReserved, 16)) {
            pp->MbrType = GPT_MBRTYPE_MSR;
            pp->FsKind = GPT_FS_MSR;
        } else if (MemEqual(e, GuidLdmMeta, 16) || MemEqual(e, GuidLdmData, 16)) {
            pp->MbrType = GPT_MBRTYPE_LDM;
            pp->FsKind = MemEqual(e, GuidLdmMeta, 16) ? GPT_FS_LDMMETA : GPT_FS_LDMDATA;
        } else {
            /* GPT has no type byte: synthesise the one that fits the file
               system (07 when none is found) */
            got = DetectFs(Read, Ctx, ss, start, count, sc->Vbr, &recog, &pp->FsKind);
            pp->MbrType = got ? (gpt_u8)got : GPT_MBRTYPE_IFS;
            pp->Recognized = (gpt_u8)recog;
        }
        out->Count++;
    }
    return GPT_OK;
}

static int IsExtendedType(gpt_u8 t)
{ return (t==GPT_MBRTYPE_EXTENDED||t==GPT_MBRTYPE_EXTENDL||t==0x85); }

static void AddMbrPartition(GPT_DISK *out, gpt_u8 boot, gpt_u8 type,
                            gpt_u64 start, gpt_u64 count, int logical,
                            GptReadProc Read, void *Ctx, GPT_SCRATCH *sc)
{
    GPT_PARTITION *pp;
    int recog;
    if (out->Count >= GPT_MAX_PARTITIONS) return;
    if (count == 0) return;
    pp = &out->Part[out->Count];
    ZeroBytes((gpt_u8 *)pp, (gpt_u32)sizeof(*pp));
    pp->StartLba = start;
    pp->SectorCount = count;
    pp->BootIndicator = (gpt_u8)(boot == 0x80 ? 0x80 : 0x00);
    pp->IsGptEntry = 0;
    pp->IsLogical = (gpt_u8)(logical ? 1 : 0);
    pp->MbrType = type;
    pp->FsKind = KindFromType(type);
    recog = 0;
    /* the type byte stays what the table says; the volume boot record only
       refines the displayed file system */
    if (!IsExtendedType(type) && type != GPT_MBRTYPE_LDM) {
        int r2;
        if (DetectFs(Read, Ctx, out->SectorSize, start, count, sc->Vbr, &r2, &pp->FsKind)) { if (r2) recog = 1; }
        else pp->FsKind = KindFromType(type);
    }
    if (type==GPT_MBRTYPE_FAT12||type==GPT_MBRTYPE_FAT16S||type==GPT_MBRTYPE_FAT16||
        type==GPT_MBRTYPE_IFS||type==GPT_MBRTYPE_FAT16L) recog = 1;
    pp->Recognized = (gpt_u8)recog;
    out->Count++;
}

static int ParseMbr(GptReadProc Read, void *Ctx, gpt_u32 ss,
                    gpt_u64 diskSectors, GPT_SCRATCH *sc, GPT_DISK *out)
{
    gpt_u8 *m = sc->Sector;
    int i;
    gpt_u64 extBase = 0, extNext = 0;
    int haveExt = 0, guard;

    if (Read(Ctx, 0, 1, m) != 0) return GPT_ERR_IO;
    if (Le16(m + 510) != 0xAA55) return GPT_OK;
    out->IsGpt = 0;
    out->SectorSize = ss;
    out->DiskSectors = diskSectors;
    out->DiskSignatureLba0 = Le32(m + 440);

    for (i = 0; i < 4; i++) {
        const gpt_u8 *e = m + 446 + 16 * i;
        gpt_u8 boot = e[0], type = e[4];
        gpt_u64 lba = Le32(e + 8), cnt = Le32(e + 12);
        if (type == 0) continue;
        if (IsExtendedType(type)) {
            if (!haveExt) { extBase = lba; extNext = lba; haveExt = 1; }
            AddMbrPartition(out, boot, type, lba, cnt, 0, Read, Ctx, sc);
            continue;
        }
        AddMbrPartition(out, boot, type, lba, cnt, 0, Read, Ctx, sc);
    }

    guard = 0;
    while (haveExt && extNext != 0 && guard < GPT_MAX_PARTITIONS) {
        const gpt_u8 *e0, *e1;
        gpt_u64 logStart, logCnt, nextRel;
        gpt_u8 logBoot, logType, nextType;
        guard++;
        if (Read(Ctx, extNext, 1, sc->Vbr) != 0) break;
        if (Le16(sc->Vbr + 510) != 0xAA55) break;
        e0 = sc->Vbr + 446; e1 = sc->Vbr + 446 + 16;
        logBoot = e0[0]; logType = e0[4];
        logStart = extNext + Le32(e0 + 8); logCnt = Le32(e0 + 12);
        nextType = e1[4]; nextRel = Le32(e1 + 8);
        if (logType != 0) {
            AddMbrPartition(out, logBoot, logType, logStart, logCnt, 1, Read, Ctx, sc);
            if (out->Count > 0) out->Part[out->Count - 1].EbrLba = extNext;
        }
        if (nextType != 0 && IsExtendedType(nextType)) extNext = extBase + nextRel;
        else extNext = 0;
    }
    return GPT_OK;
}

extern void *GptPlatAlloc(gpt_u32 Size);
extern void  GptPlatFree(void *P);
static GPT_SCRATCH *GptScratchAlloc(void){return (GPT_SCRATCH *)GptPlatAlloc((gpt_u32)sizeof(GPT_SCRATCH));}
static void GptScratchFree(GPT_SCRATCH *p){GptPlatFree(p);}

int GptParse(GptReadProc Read, void *Ctx, gpt_u32 SectorSize,
             gpt_u64 DiskSectors, GPT_DISK *Out)
{
    GPT_SCRATCH *sc;
    gpt_u8 *m;
    int i, protective = 0, rc;
    if (!Read || !Out) return GPT_ERR_PARAM;
    if (SectorSize!=512&&SectorSize!=1024&&SectorSize!=2048&&SectorSize!=4096) return GPT_ERR_PARAM;
    ZeroBytes((gpt_u8 *)Out, (gpt_u32)sizeof(*Out));
    Out->SectorSize = SectorSize;
    Out->DiskSectors = DiskSectors;
    sc = GptScratchAlloc();
    if (!sc) return GPT_ERR_PARAM;
    m = sc->Sector;
    if (Read(Ctx, 0, 1, m) != 0) { GptScratchFree(sc); return GPT_ERR_IO; }
    if (Le16(m + 510) == 0xAA55)
        for (i = 0; i < 4; i++)
            if (m[446 + 16 * i + 4] == GPT_MBRTYPE_GPTPROT) protective = 1;
    if (protective) {
        rc = ParseHeaderAt(Read, Ctx, SectorSize, DiskSectors, 1, sc, Out);
        if (rc != GPT_OK && DiskSectors > 1) {
            int rc2 = ParseHeaderAt(Read, Ctx, SectorSize, DiskSectors, DiskSectors - 1, sc, Out);
            if (rc2 == GPT_OK) { Out->UsedBackup = 1; rc = GPT_OK; }
        }
        GptScratchFree(sc);
        return rc;
    }
    rc = ParseMbr(Read, Ctx, SectorSize, DiskSectors, sc, Out);
    GptScratchFree(sc);
    return rc;
}

int GptBuildHeaderCrc(gpt_u8 *Header, gpt_u32 HeaderSize,
                      const gpt_u8 *EntryArray, gpt_u32 EntryArrayLen)
{
    gpt_u32 entryCrc, headerCrc;
    if (!Header || HeaderSize < 92) return GPT_ERR_PARAM;
    entryCrc = GptCrc32(EntryArray, EntryArrayLen);
    Header[88]=(gpt_u8)entryCrc; Header[89]=(gpt_u8)(entryCrc>>8);
    Header[90]=(gpt_u8)(entryCrc>>16); Header[91]=(gpt_u8)(entryCrc>>24);
    Header[16]=Header[17]=Header[18]=Header[19]=0;
    headerCrc = GptCrc32(Header, HeaderSize);
    Header[16]=(gpt_u8)headerCrc; Header[17]=(gpt_u8)(headerCrc>>8);
    Header[18]=(gpt_u8)(headerCrc>>16); Header[19]=(gpt_u8)(headerCrc>>24);
    return GPT_OK;
}

/* ================= layout editing + writing ================= */

static gpt_u32 g_rng = 0x2545F491u;
void GptSeed(gpt_u32 s){if(s)g_rng=s;}
static gpt_u32 Rng(void){g_rng=g_rng*1664525u+1013904223u;return g_rng;}
static void Wr32(gpt_u8 *p,gpt_u32 v){p[0]=(gpt_u8)v;p[1]=(gpt_u8)(v>>8);p[2]=(gpt_u8)(v>>16);p[3]=(gpt_u8)(v>>24);}
static void Wr64(gpt_u8 *p,gpt_u64 v){Wr32(p,(gpt_u32)v);Wr32(p+4,(gpt_u32)(v>>32));}

static void GenGuid(gpt_u8 *g)
{
    int i;
    for (i = 0; i < 16; i++) g[i] = (gpt_u8)(Rng() >> 13);
    g[7] = (gpt_u8)((g[7] & 0x0F) | 0x40);
    g[8] = (gpt_u8)((g[8] & 0x3F) | 0x80);
}

static const gpt_u8 GuidMsBasic[16] = {
    0xA2,0xA0,0xD0,0xEB,0xE5,0xB9,0x33,0x44,0x87,0xC0,0x68,0xB6,0xB7,0x26,0x99,0xC7};

static void MbrTypeToGuid(gpt_u8 t, gpt_u8 *out)
{
    if (t == GPT_MBRTYPE_EFISYS) CopyBytes(out, GuidEfiSystem, 16);
    else CopyBytes(out, GuidMsBasic, 16);
}

#define GPT_ALIGN 2048u
static gpt_u32 EntryArraySectors(gpt_u32 ss){return (GPT_MAX_ENTRY_BYTES + ss - 1) / ss;}

static void UsableRange(const GPT_DISK *d, gpt_u64 *first, gpt_u64 *last)
{
    if (d->IsGpt) {
        gpt_u32 es = EntryArraySectors(d->SectorSize);
        *first = 2 + es;
        *last  = d->DiskSectors - 2 - es;
    } else {
        *first = GPT_ALIGN;
        *last  = d->DiskSectors - 1;
    }
}

static gpt_u64 AlignUp(gpt_u64 v){gpt_u32 r=(gpt_u32)(v%GPT_ALIGN);return r?(v+(GPT_ALIGN-r)):v;}

static int FindGap(const GPT_DISK *d, gpt_u64 Need, gpt_u64 *Start, gpt_u64 *Len)
{
    gpt_u64 first, last, cur, bestStart = 0, bestLen = 0;
    UsableRange(d, &first, &last);
    cur = AlignUp(first);
    while (cur <= last) {
        gpt_u64 nextBlock = last + 1, gapEnd, gapLen;
        gpt_u32 i; int inside = 0;
        for (i = 0; i < d->Count; i++) {
            gpt_u64 ps = d->Part[i].StartLba, pe = ps + d->Part[i].SectorCount - 1;
            if (d->Part[i].IsLogical) continue;
            if (cur >= ps && cur <= pe) { cur = AlignUp(pe + 1); inside = 1; break; }
        }
        if (inside) continue;
        for (i = 0; i < d->Count; i++) {
            gpt_u64 ps = d->Part[i].StartLba;
            if (d->Part[i].IsLogical) continue;
            if (ps > cur && ps < nextBlock) nextBlock = ps;
        }
        gapEnd = (nextBlock <= last) ? (nextBlock - 1) : last;
        if (gapEnd >= cur) {
            gapLen = gapEnd - cur + 1;
            if (Need != 0 && gapLen >= Need) { *Start = cur; *Len = gapLen; return 1; }
            if (Need == 0 && gapLen > bestLen) { bestStart = cur; bestLen = gapLen; }
        }
        if (nextBlock > last) break;
        cur = AlignUp(nextBlock);
    }
    if (Need == 0 && bestLen > 0) { *Start = bestStart; *Len = bestLen; return 1; }
    return 0;
}

gpt_u64 GptLargestFreeGap(const GPT_DISK *d, gpt_u64 *OutStart)
{
    gpt_u64 s = 0, l = 0;
    if (FindGap(d, 0, &s, &l)) { if (OutStart) *OutStart = s; return l; }
    if (OutStart) *OutStart = 0;
    return 0;
}

/* Find free space INSIDE an extended container for a new logical drive.
   Each logical reserves GPT_ALIGN sectors before its data for its EBR, so
   a candidate's occupancy is [data-GPT_ALIGN .. data+need-1]. Returns the
   data start LBA and the max data length available there. */
static int FindInnerGap(const GPT_DISK *d, gpt_u64 Need, gpt_u64 *Start, gpt_u64 *Len)
{
    gpt_u32 e, i;
    gpt_u64 bestStart = 0, bestLen = 0;
    for (e = 0; e < d->Count; e++) {
        gpt_u64 es, ee, cur;
        gpt_u8 t = d->Part[e].MbrType;
        if (d->Part[e].IsLogical) continue;
        if (t != GPT_MBRTYPE_EXTENDED && t != GPT_MBRTYPE_EXTENDL) continue;
        es = d->Part[e].StartLba;
        ee = es + d->Part[e].SectorCount - 1;
        cur = AlignUp(es + GPT_ALIGN);          /* room for this gap's EBR */
        while (cur <= ee) {
            gpt_u64 nextBlock = ee + 1, gapEnd, gapLen;
            int inside = 0;
            for (i = 0; i < d->Count; i++) {
                gpt_u64 lo, hi;
                if (!d->Part[i].IsLogical) continue;
                lo = d->Part[i].StartLba;
                if (lo >= GPT_ALIGN) lo -= GPT_ALIGN;          /* include its EBR */
                hi = d->Part[i].StartLba + d->Part[i].SectorCount - 1;
                if (cur >= lo && cur <= hi) { cur = AlignUp(hi + 1 + GPT_ALIGN); inside = 1; break; }
            }
            if (inside) continue;
            for (i = 0; i < d->Count; i++) {
                gpt_u64 lo;
                if (!d->Part[i].IsLogical) continue;
                lo = d->Part[i].StartLba;
                if (lo >= GPT_ALIGN) lo -= GPT_ALIGN;
                if (lo > cur && lo < nextBlock) nextBlock = lo;
            }
            gapEnd = (nextBlock <= ee) ? (nextBlock - 1) : ee;
            if (gapEnd >= cur) {
                gapLen = gapEnd - cur + 1;
                if (Need != 0 && gapLen >= Need) { *Start = cur; *Len = gapLen; return 1; }
                if (Need == 0 && gapLen > bestLen) { bestStart = cur; bestLen = gapLen; }
            }
            if (nextBlock > ee) break;
            cur = AlignUp(nextBlock + GPT_ALIGN);
        }
    }
    if (Need == 0 && bestLen > 0) { *Start = bestStart; *Len = bestLen; return 1; }
    return 0;
}

int GptInitDisk(GPT_DISK *d, int Scheme, gpt_u32 ss, gpt_u64 sectors)
{
    if (!d) return GPT_ERR_PARAM;
    if (ss!=512&&ss!=1024&&ss!=2048&&ss!=4096) return GPT_ERR_PARAM;
    ZeroBytes((gpt_u8 *)d, (gpt_u32)sizeof(*d));
    d->SectorSize = ss;
    d->DiskSectors = sectors;
    d->IsGpt = (gpt_u8)(Scheme == GPT_SCHEME_GPT ? 1 : 0);
    d->Count = 0;
    if (d->IsGpt) GenGuid(d->DiskGuid);
    else d->DiskSignatureLba0 = Rng();
    return GPT_OK;
}

static int IsRecognizedType(gpt_u8 t)
{
    return (t==GPT_MBRTYPE_FAT12||t==GPT_MBRTYPE_FAT16S||t==GPT_MBRTYPE_FAT16||
            t==GPT_MBRTYPE_IFS||t==GPT_MBRTYPE_FAT16L||t==GPT_MBRTYPE_FAT32||t==GPT_MBRTYPE_FAT32L);
}

int GptAddPartition(GPT_DISK *d, gpt_u64 StartLba, gpt_u64 SectorCount,
                    gpt_u8 MbrType, gpt_u8 Bootable, gpt_u32 *OutIndex)
{
    GPT_PARTITION *pp;
    gpt_u64 start = StartLba, count = SectorCount, gapStart, gapLen, first, last;
    int primFull = 0;
    if (!d) return GPT_ERR_PARAM;
    if (d->Count >= GPT_MAX_PARTITIONS) return GPT_ERR_PARAM;
    UsableRange(d, &first, &last);

    if (!d->IsGpt) {
        /* an MBR holds at most 4 primary entries (the extended container
           takes one); a 5th primary would be lost when the table is written */
        gpt_u32 i, prim = 0;
        for (i = 0; i < d->Count; i++) if (!d->Part[i].IsLogical) prim++;
        primFull = (prim >= 4);
    }
    if (start == 0) {
        /* outer free space first (new primary); else inside an extended
           container (new logical). */
        if (!primFull && FindGap(d, count ? count : 1, &gapStart, &gapLen)) {
            start = gapStart;
            if (count == 0) count = gapLen;
        } else if (!d->IsGpt && FindInnerGap(d, count ? count : 1, &gapStart, &gapLen)) {
            start = gapStart;
            if (count == 0) count = gapLen;
        } else {
            return GPT_ERR_PARAM;
        }
    } else if (count == 0) {
        gpt_u64 nb = last + 1; gpt_u32 i;
        for (i = 0; i < d->Count; i++) {
            gpt_u64 ps = d->Part[i].StartLba;
            if (d->Part[i].IsLogical) continue;
            if (ps > start && ps < nb) nb = ps;
        }
        count = ((nb <= last) ? (nb - 1) : last) - start + 1;
    }
    if (count == 0) return GPT_ERR_PARAM;
    if (start < first || start + count - 1 > last) return GPT_ERR_PARAM;

    {
        gpt_u32 i, extIdx = 0xFFFFFFFF;
        int willLogical = 0;
        gpt_u64 e = start + count - 1;
        if (!d->IsGpt) {
            for (i = 0; i < d->Count; i++) {
                gpt_u8 t = d->Part[i].MbrType;
                if (!d->Part[i].IsLogical &&
                    (t == GPT_MBRTYPE_EXTENDED || t == GPT_MBRTYPE_EXTENDL) &&
                    start >= d->Part[i].StartLba &&
                    e <= d->Part[i].StartLba + d->Part[i].SectorCount - 1) {
                    willLogical = 1; extIdx = i; break;
                }
            }
            /* no room for another primary entry */
            if (!willLogical && primFull) return GPT_ERR_PARAM;
            /* only one extended container per disk, and not inside itself */
            if (IsExtendedType(MbrType)) {
                if (willLogical) return GPT_ERR_PARAM;
                for (i = 0; i < d->Count; i++)
                    if (!d->Part[i].IsLogical && IsExtendedType(d->Part[i].MbrType)) return GPT_ERR_PARAM;
            }
        }
        for (i = 0; i < d->Count; i++) {
            gpt_u64 ps, pe;
            if (i == extIdx) continue;
            if (willLogical && !d->Part[i].IsLogical) continue;
            if (!willLogical && d->Part[i].IsLogical) continue;
            ps = d->Part[i].StartLba; pe = ps + d->Part[i].SectorCount - 1;
            if (start <= pe && ps <= e) return GPT_ERR_PARAM;
        }
        pp = &d->Part[d->Count];
        ZeroBytes((gpt_u8 *)pp, (gpt_u32)sizeof(*pp));
        pp->StartLba = start;
        pp->SectorCount = count;
        pp->MbrType = MbrType;
        pp->FsKind = KindFromType(MbrType);
        pp->BootIndicator = (gpt_u8)(Bootable ? 0x80 : 0x00);
        pp->Recognized = (gpt_u8)IsRecognizedType(MbrType);
        if (d->IsGpt) {
            pp->IsGptEntry = 1;
            MbrTypeToGuid(MbrType, pp->TypeGuid);
            GenGuid(pp->UniqueGuid);
        } else {
            pp->IsGptEntry = 0;
            pp->IsLogical = (gpt_u8)willLogical;
        }
    }
    if (OutIndex) *OutIndex = d->Count;
    d->Count++;
    return GPT_OK;
}

int GptDeletePartition(GPT_DISK *d, gpt_u32 Index)
{
    gpt_u32 i;
    if (!d || Index >= d->Count) return GPT_ERR_PARAM;
    for (i = Index; i + 1 < d->Count; i++) d->Part[i] = d->Part[i + 1];
    d->Count--;
    return GPT_OK;
}

int GptSetType(GPT_DISK *d, gpt_u32 Index, gpt_u8 MbrType)
{
    if (!d || Index >= d->Count) return GPT_ERR_PARAM;
    d->Part[Index].MbrType = MbrType;
    d->Part[Index].FsKind = KindFromType(MbrType);
    d->Part[Index].Recognized = (gpt_u8)IsRecognizedType(MbrType);
    if (d->IsGpt) MbrTypeToGuid(MbrType, d->Part[Index].TypeGuid);
    return GPT_OK;
}

int GptSetActive(GPT_DISK *d, gpt_u32 Index, gpt_u8 Active)
{
    gpt_u32 i;
    if (!d || Index >= d->Count) return GPT_ERR_PARAM;
    if (Active) {
        /* only one active primary at a time on MBR */
        if (!d->IsGpt)
            for (i = 0; i < d->Count; i++)
                if (!d->Part[i].IsLogical) d->Part[i].BootIndicator = 0;
        d->Part[Index].BootIndicator = 0x80;
    } else {
        d->Part[Index].BootIndicator = 0;
    }
    return GPT_OK;
}

static void Geometry(const GPT_DISK *d, gpt_u32 *H, gpt_u32 *S)
{
    *H = d->Heads ? d->Heads : 255;
    *S = d->SectorsPerTrack ? d->SectorsPerTrack : 63;
    if (*H > 255) *H = 255;
    if (*S > 63) *S = 63;
}

gpt_u64 GptChsLimit(const GPT_DISK *d)
{
    gpt_u32 H, S;
    Geometry(d, &H, &S);
    return (gpt_u64)(1024u * H * S);
}

/* CHS triple of an absolute LBA; past the CHS range the conventional
   "maximum" value FE FF FF is written (the LBA fields then apply) */
static void LbaToChs(const GPT_DISK *d, gpt_u64 lba, gpt_u8 *out)
{
    gpt_u32 H, S, l, c, r, h, sct;
    Geometry(d, &H, &S);
    if (lba >= (gpt_u64)(1024u * H * S)) { out[0] = 0xFE; out[1] = 0xFF; out[2] = 0xFF; return; }
    l = (gpt_u32)lba;
    c = l / (H * S); r = l % (H * S);
    h = r / S; sct = r % S + 1;
    out[0] = (gpt_u8)h;
    out[1] = (gpt_u8)((sct & 0x3F) | ((c >> 2) & 0xC0));
    out[2] = (gpt_u8)(c & 0xFF);
}

gpt_u32 GptTypeFamily(gpt_u8 t)
{
    if (t == GPT_MBRTYPE_FAT16S || t == GPT_MBRTYPE_FAT16 || t == GPT_MBRTYPE_FAT16L) return GPT_FAMILY_FAT16;
    if (t == GPT_MBRTYPE_FAT32 || t == GPT_MBRTYPE_FAT32L) return GPT_FAMILY_FAT32;
    if (t == GPT_MBRTYPE_EXTENDED || t == GPT_MBRTYPE_EXTENDL) return GPT_FAMILY_EXTENDED;
    return 0;
}

gpt_u8 GptPickType(const GPT_DISK *d, gpt_u32 family, gpt_u64 start, gpt_u64 count)
{
    int beyond = (count && start + count - 1 >= GptChsLimit(d)) ? 1 : 0;
    switch (family) {
    case GPT_FAMILY_FAT16:
        if (beyond) return GPT_MBRTYPE_FAT16L;
        return (count < 65536) ? GPT_MBRTYPE_FAT16S : GPT_MBRTYPE_FAT16;
    case GPT_FAMILY_FAT32:
        return beyond ? GPT_MBRTYPE_FAT32L : GPT_MBRTYPE_FAT32;
    case GPT_FAMILY_EXTENDED:
        return beyond ? GPT_MBRTYPE_EXTENDL : GPT_MBRTYPE_EXTENDED;
    }
    return GPT_MBRTYPE_FAT16;
}

/* One 16-byte MBR / EBR entry. relStart is what goes into the LBA field
   (relative for EBR entries); absStart is the absolute LBA, used for the
   CHS fields. */
static void FillMbrEntry(const GPT_DISK *d, gpt_u8 *e, gpt_u8 boot, gpt_u8 type,
                         gpt_u32 relStart, gpt_u32 count, gpt_u64 absStart)
{
    e[0] = (gpt_u8)(boot ? 0x80 : 0x00);
    LbaToChs(d, absStart, e + 1);
    e[4] = type;
    LbaToChs(d, absStart + (count ? count - 1 : 0), e + 5);
    Wr32(e + 8, relStart);
    Wr32(e + 12, count);
}

int GptWriteDisk(GptReadProc Read, GptWriteProc Write, void *Ctx, GPT_DISK *d)
{
    GPT_SCRATCH *sc;
    gpt_u8 *m, *hdr, *ent;
    gpt_u32 ss, es, i;
    gpt_u64 diskSectors;
    int rc = GPT_OK;

    if (!Read || !Write || !d) return GPT_ERR_PARAM;
    ss = d->SectorSize;
    diskSectors = d->DiskSectors;
    sc = GptScratchAlloc();
    if (!sc) return GPT_ERR_PARAM;
    m = sc->Sector; ent = sc->Entries; hdr = sc->Vbr;

    if (d->IsGpt) {
        es = EntryArraySectors(ss);
        if (Read(Ctx, 0, 1, m) != 0) ZeroBytes(m, ss);
        ZeroBytes(m + 446, 64);
        FillMbrEntry(d, m + 446, 0, GPT_MBRTYPE_GPTPROT, 1,
                     (diskSectors - 1 > 0xFFFFFFFF) ? 0xFFFFFFFF : (gpt_u32)(diskSectors - 1), 1);
        m[446 + 1] = 0x00; m[446 + 2] = 0x02; m[446 + 3] = 0x00;
        m[510] = 0x55; m[511] = 0xAA;
        if (Write(Ctx, 0, 1, m) != 0) { rc = GPT_ERR_IO; goto done; }
        ZeroBytes(ent, GPT_MAX_ENTRY_BYTES);
        for (i = 0; i < d->Count && i < GPT_MAX_PARTITIONS; i++) {
            gpt_u8 *e = ent + (gpt_u64)i * 128;
            GPT_PARTITION *p = &d->Part[i];
            CopyBytes(e, p->TypeGuid, 16);
            CopyBytes(e + 16, p->UniqueGuid, 16);
            Wr64(e + 32, p->StartLba);
            Wr64(e + 40, p->StartLba + p->SectorCount - 1);
            Wr64(e + 48, (p->Attributes & ~(gpt_u64)0x4) | (gpt_u64)(p->BootIndicator ? 0x4 : 0x0));
            { gpt_u32 k; for (k = 0; k < 36; k++) { e[56 + 2 * k] = (gpt_u8)p->Name[k]; e[57 + 2 * k] = (gpt_u8)(p->Name[k] >> 8); } }
        }
        ZeroBytes(hdr, ss);
        CopyBytes(hdr, GptSignature, 8);
        Wr32(hdr + 8, 0x00010000); Wr32(hdr + 12, 92);
        Wr64(hdr + 24, 1); Wr64(hdr + 32, diskSectors - 1);
        Wr64(hdr + 40, 2 + es); Wr64(hdr + 48, diskSectors - 2 - es);
        CopyBytes(hdr + 56, d->DiskGuid, 16);
        Wr64(hdr + 72, 2); Wr32(hdr + 80, GPT_MAX_PARTITIONS); Wr32(hdr + 84, 128);
        GptBuildHeaderCrc(hdr, 92, ent, GPT_MAX_ENTRY_BYTES);
        if (Write(Ctx, 2, es, ent) != 0) { rc = GPT_ERR_IO; goto done; }
        if (Write(Ctx, 1, 1, hdr) != 0) { rc = GPT_ERR_IO; goto done; }
        if (Write(Ctx, diskSectors - 1 - es, es, ent) != 0) { rc = GPT_ERR_IO; goto done; }
        ZeroBytes(hdr, ss);
        CopyBytes(hdr, GptSignature, 8);
        Wr32(hdr + 8, 0x00010000); Wr32(hdr + 12, 92);
        Wr64(hdr + 24, diskSectors - 1); Wr64(hdr + 32, 1);
        Wr64(hdr + 40, 2 + es); Wr64(hdr + 48, diskSectors - 2 - es);
        CopyBytes(hdr + 56, d->DiskGuid, 16);
        Wr64(hdr + 72, diskSectors - 1 - es); Wr32(hdr + 80, GPT_MAX_PARTITIONS); Wr32(hdr + 84, 128);
        GptBuildHeaderCrc(hdr, 92, ent, GPT_MAX_ENTRY_BYTES);
        if (Write(Ctx, diskSectors - 1, 1, hdr) != 0) { rc = GPT_ERR_IO; goto done; }
    } else {
        gpt_u32 slot = 0;
        gpt_u64 extBase = 0;
        int haveExt = 0;
        gpt_u32 logicals[GPT_MAX_PARTITIONS], nlog = 0, a, b, k;
        gpt_u64 ebrs[GPT_MAX_PARTITIONS];
        if (Read(Ctx, 0, 1, m) != 0) ZeroBytes(m, ss);
        ZeroBytes(m + 446, 64);
        if (d->DiskSignatureLba0) Wr32(m + 440, (gpt_u32)d->DiskSignatureLba0);
        for (i = 0; i < d->Count && slot < 4; i++) {
            GPT_PARTITION *p = &d->Part[i];
            if (p->IsLogical) continue;
            FillMbrEntry(d, m + 446 + 16 * slot, p->BootIndicator, p->MbrType,
                         (gpt_u32)p->StartLba, (gpt_u32)p->SectorCount, p->StartLba);
            slot++;
        }
        m[510] = 0x55; m[511] = 0xAA;
        if (Write(Ctx, 0, 1, m) != 0) { rc = GPT_ERR_IO; goto done; }
        for (i = 0; i < d->Count; i++) {
            gpt_u8 t = d->Part[i].MbrType;
            if (!d->Part[i].IsLogical && (t==GPT_MBRTYPE_EXTENDED||t==GPT_MBRTYPE_EXTENDL)) {
                extBase = d->Part[i].StartLba; haveExt = 1;
            }
        }
        if (haveExt) {
            for (i = 0; i < d->Count; i++) if (d->Part[i].IsLogical) logicals[nlog++] = i;
            for (a = 1; a < nlog; a++) {
                k = logicals[a]; b = a;
                while (b > 0 && d->Part[logicals[b - 1]].StartLba > d->Part[k].StartLba) {
                    logicals[b] = logicals[b - 1]; b--;
                }
                logicals[b] = k;
            }
            /* an empty container still needs a valid (empty) first EBR,
               or stale data there would be read as logical drives */
            if (nlog == 0) {
                ZeroBytes(hdr, ss);
                hdr[510] = 0x55; hdr[511] = 0xAA;
                if (Write(Ctx, extBase, 1, hdr) != 0) { rc = GPT_ERR_IO; goto done; }
            }
            /* where each logical drive's EBR goes: the first one at the
               container's first sector, every other one between the end of
               the previous logical drive and its own data. A position read
               from the disk is reused only when it still fits there - after
               a delete or a create the old chain no longer applies. */
            {
                gpt_u64 prevEnd = extBase;
                for (i = 0; i < nlog; i++) {
                    GPT_PARTITION *p = &d->Part[logicals[i]];
                    gpt_u64 e = p->EbrLba;
                    if (i == 0) e = extBase;
                    else if (e <= prevEnd || e >= p->StartLba)
                        e = (p->StartLba >= GPT_ALIGN && p->StartLba - GPT_ALIGN > prevEnd)
                                ? p->StartLba - GPT_ALIGN : p->StartLba - 1;
                    ebrs[i] = e;
                    prevEnd = p->StartLba + p->SectorCount - 1;
                }
            }
            for (i = 0; i < nlog; i++) {
                GPT_PARTITION *p = &d->Part[logicals[i]];
                gpt_u64 ebr = ebrs[i];
                ZeroBytes(hdr, ss);
                FillMbrEntry(d, hdr + 446, p->BootIndicator, p->MbrType,
                             (gpt_u32)(p->StartLba - ebr), (gpt_u32)p->SectorCount, p->StartLba);
                if (i + 1 < nlog) {
                    GPT_PARTITION *pn = &d->Part[logicals[i + 1]];
                    gpt_u64 nebr = ebrs[i + 1];
                    /* link entries use type 05, as DOS and Windows write them
                       and as every in-box disk driver follows them */
                    FillMbrEntry(d, hdr + 446 + 16, 0, GPT_MBRTYPE_EXTENDED,
                                 (gpt_u32)(nebr - extBase),
                                 (gpt_u32)(pn->SectorCount + (pn->StartLba - nebr)), nebr);
                }
                hdr[510] = 0x55; hdr[511] = 0xAA;
                if (Write(Ctx, ebr, 1, hdr) != 0) { rc = GPT_ERR_IO; goto done; }
            }
        }
    }
done:
    GptScratchFree(sc);
    return rc;
}

/* ================= lossless scheme conversion ================= */

#define GPT_U32MAX ((gpt_u64)0xFFFFFFFFu)

int GptConvertScheme(const GPT_DISK *Src, int TargetScheme, GPT_DISK *Dst,
                     int *Reason)
{
    gpt_u64 start[GPT_MAX_PARTITIONS], count[GPT_MAX_PARTITIONS];
    gpt_u8  type[GPT_MAX_PARTITIONS], boot[GPT_MAX_PARTITIONS], kind[GPT_MAX_PARTITIONS];
    gpt_u32 n = 0, i, a, b;
    gpt_u32 ss, es;
    gpt_u64 sectors, firstU, lastU;
    int seenActive = 0;

    if (Reason) *Reason = GPT_CVT_OK;
    if (!Src || !Dst) { if (Reason) *Reason = GPT_CVT_EMPTY; return GPT_ERR_PARAM; }
    ss = Src->SectorSize;
    sectors = Src->DiskSectors;

    /* already that scheme? nothing to convert losslessly */
    if (Src->IsGpt && TargetScheme == GPT_SCHEME_GPT) { if (Reason) *Reason = GPT_CVT_SAME; return GPT_ERR_PARAM; }
    if (!Src->IsGpt && TargetScheme == GPT_SCHEME_MBR) { if (Reason) *Reason = GPT_CVT_SAME; return GPT_ERR_PARAM; }

    /* gather the data partitions (skip MBR extended containers; their
       logical drives are kept as ordinary partitions; a GPT reserved
       partition has no MBR counterpart). A dynamic disk is converted by
       its own rules, not here. */
    for (i = 0; i < Src->Count; i++) {
        gpt_u8 t = Src->Part[i].MbrType;
        if (t == GPT_MBRTYPE_LDM) { if (Reason) *Reason = GPT_CVT_DYNAMIC; return GPT_ERR_PARAM; }
        if (!Src->IsGpt && IsExtendedType(t)) continue;
        if (Src->IsGpt && Src->Part[i].FsKind == GPT_FS_MSR) continue;
        if (n >= GPT_MAX_PARTITIONS) break;
        start[n] = Src->Part[i].StartLba;
        count[n] = Src->Part[i].SectorCount;
        type[n]  = Src->Part[i].MbrType;
        boot[n]  = (gpt_u8)(Src->Part[i].BootIndicator ? 0x80 : 0x00);
        kind[n]  = Src->Part[i].FsKind;
        n++;
    }
    /* sort ascending by start LBA */
    for (a = 1; a < n; a++) {
        gpt_u64 s = start[a], c = count[a]; gpt_u8 t = type[a], bo = boot[a], k = kind[a];
        b = a;
        while (b > 0 && start[b - 1] > s) {
            start[b] = start[b - 1]; count[b] = count[b - 1];
            type[b] = type[b - 1]; boot[b] = boot[b - 1]; kind[b] = kind[b - 1]; b--;
        }
        start[b] = s; count[b] = c; type[b] = t; boot[b] = bo; kind[b] = k;
    }

    es = EntryArraySectors(ss);

    if (TargetScheme == GPT_SCHEME_GPT) {
        /* GPT reserves the primary header + entry array at the start and a
           backup copy at the end; every partition must fit between them. */
        firstU = 2 + es;
        lastU  = sectors - 2 - es;
        if (n > GPT_MAX_PARTITIONS) { if (Reason) *Reason = GPT_CVT_TOOMANY; return GPT_ERR_PARAM; }
        for (i = 0; i < n; i++) {
            if (start[i] < firstU) { if (Reason) *Reason = GPT_CVT_NOROOM_GPT; return GPT_ERR_PARAM; }
            if (start[i] + count[i] - 1 > lastU) { if (Reason) *Reason = GPT_CVT_NOROOM_GPT; return GPT_ERR_PARAM; }
        }
        GptInitDisk(Dst, GPT_SCHEME_GPT, ss, sectors);
        Dst->Heads = Src->Heads; Dst->SectorsPerTrack = Src->SectorsPerTrack;
        for (i = 0; i < n; i++) {
            GPT_PARTITION *pp = &Dst->Part[i];
            ZeroBytes((gpt_u8 *)pp, (gpt_u32)sizeof(*pp));
            pp->StartLba = start[i]; pp->SectorCount = count[i];
            pp->MbrType = type[i]; pp->FsKind = kind[i];
            pp->BootIndicator = boot[i];
            pp->Recognized = (gpt_u8)IsRecognizedType(type[i]);
            pp->IsGptEntry = 1;
            MbrTypeToGuid(type[i], pp->TypeGuid);
            GenGuid(pp->UniqueGuid);
        }
        Dst->Count = n;
        return GPT_OK;
    }

    /* target MBR: 32-bit LBA only */
    for (i = 0; i < n; i++) {
        if (start[i] < 1) { if (Reason) *Reason = GPT_CVT_NOROOM_GPT; return GPT_ERR_PARAM; }
        if (start[i] > GPT_U32MAX || start[i] + count[i] - 1 > GPT_U32MAX) { if (Reason) *Reason = GPT_CVT_TOOBIG; return GPT_ERR_PARAM; }
    }
    GptInitDisk(Dst, GPT_SCHEME_MBR, ss, sectors);
    Dst->Heads = Src->Heads; Dst->SectorsPerTrack = Src->SectorsPerTrack;
    /* the right MBR type byte for each partition, from its file system,
       size and position (GPT has no type bytes to carry over) */
    for (i = 0; i < n; i++) {
        if (kind[i] == GPT_FS_FAT16) type[i] = GptPickType(Dst, GPT_FAMILY_FAT16, start[i], count[i]);
        else if (kind[i] == GPT_FS_FAT32) type[i] = GptPickType(Dst, GPT_FAMILY_FAT32, start[i], count[i]);
        else if (kind[i] == GPT_FS_FAT12) type[i] = GPT_MBRTYPE_FAT12;
    }
    if (n <= 4) {
        /* all fit as primaries */
        for (i = 0; i < n; i++) {
            GPT_PARTITION *pp = &Dst->Part[i];
            ZeroBytes((gpt_u8 *)pp, (gpt_u32)sizeof(*pp));
            pp->StartLba = start[i]; pp->SectorCount = count[i];
            pp->MbrType = type[i]; pp->FsKind = kind[i];
            pp->BootIndicator = (gpt_u8)((boot[i] && !seenActive) ? 0x80 : 0x00);
            if (boot[i]) seenActive = 1;
            pp->Recognized = (gpt_u8)IsRecognizedType(type[i]);
            pp->IsGptEntry = 0; pp->IsLogical = 0;
        }
        Dst->Count = n;
        return GPT_OK;
    } else {
        /* 3 primaries + an extended container holding the rest as logical
           drives. Each logical needs a free sector in front of it for its
           boot record (EBR); if partitions are packed with no gap, a
           lossless conversion is impossible. */
        gpt_u64 extStart, lastEnd;
        gpt_u32 k = 0;
        for (i = 3; i < n; i++) {
            gpt_u64 prevEnd = (i == 3) ? (start[2] + count[2] - 1)
                                       : (start[i - 1] + count[i - 1] - 1);
            if (start[i] < prevEnd + 2) { if (Reason) *Reason = GPT_CVT_NOEBR; return GPT_ERR_PARAM; }
        }
        extStart = start[3] - 1;
        lastEnd  = start[n - 1] + count[n - 1] - 1;
        if (extStart > GPT_U32MAX || lastEnd > GPT_U32MAX) { if (Reason) *Reason = GPT_CVT_TOOBIG; return GPT_ERR_PARAM; }
        for (i = 0; i < 3; i++) {
            GPT_PARTITION *pp = &Dst->Part[k++];
            ZeroBytes((gpt_u8 *)pp, (gpt_u32)sizeof(*pp));
            pp->StartLba = start[i]; pp->SectorCount = count[i];
            pp->MbrType = type[i]; pp->FsKind = kind[i];
            pp->BootIndicator = (gpt_u8)((boot[i] && !seenActive) ? 0x80 : 0x00);
            if (boot[i]) seenActive = 1;
            pp->Recognized = (gpt_u8)IsRecognizedType(type[i]);
            pp->IsGptEntry = 0; pp->IsLogical = 0;
        }
        {
            GPT_PARTITION *pp = &Dst->Part[k++];
            ZeroBytes((gpt_u8 *)pp, (gpt_u32)sizeof(*pp));
            pp->StartLba = extStart; pp->SectorCount = lastEnd - extStart + 1;
            pp->MbrType = GptPickType(Dst, GPT_FAMILY_EXTENDED, extStart, lastEnd - extStart + 1);
            pp->FsKind = GPT_FS_EXTENDED;
            pp->IsGptEntry = 0; pp->IsLogical = 0;
        }
        for (i = 3; i < n; i++) {
            GPT_PARTITION *pp = &Dst->Part[k++];
            ZeroBytes((gpt_u8 *)pp, (gpt_u32)sizeof(*pp));
            pp->StartLba = start[i]; pp->SectorCount = count[i];
            pp->MbrType = type[i]; pp->FsKind = kind[i];
            pp->BootIndicator = 0;
            pp->Recognized = (gpt_u8)IsRecognizedType(type[i]);
            pp->IsGptEntry = 0; pp->IsLogical = 1;
            pp->EbrLba = start[i] - 1;
        }
        Dst->Count = k;
        return GPT_OK;
    }
}
