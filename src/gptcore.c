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

static const gpt_u8 GuidLinuxSwap[16] = { 0x6D,0xFD,0x57,0x06,0xAB,0xA4,0xC4,0x43,0x84,0xE5,0x09,0x33,0xC8,0x4B,0x4F,0x4F };
static const gpt_u8 GuidLinuxLvm[16] = { 0x79,0xD3,0xD6,0xE6,0x07,0xF5,0xC2,0x44,0xA2,0x3C,0x23,0x8F,0x2A,0x3D,0xF9,0x28 };
static const gpt_u8 GuidLinuxRaid[16] = { 0x0F,0x88,0x9D,0xA1,0xFC,0x05,0x3B,0x4D,0xA0,0x06,0x74,0x3F,0x0F,0x84,0x91,0x1E };
static const gpt_u8 GuidLinuxLuks[16] = { 0xCB,0x7C,0x7D,0xCA,0xED,0x63,0x53,0x4C,0x86,0x1C,0x17,0x42,0x53,0x60,0x59,0xCC };
static const gpt_u8 GuidLinuxData[16] = { 0xAF,0x3D,0xC6,0x0F,0x83,0x84,0x72,0x47,0x8E,0x79,0x3D,0x69,0xD8,0x47,0x7D,0xE4 };
static const gpt_u8 GuidBiosBoot[16] = { 0x48,0x61,0x68,0x21,0x49,0x64,0x6F,0x6E,0x74,0x4E,0x65,0x65,0x64,0x45,0x46,0x49 };
static const gpt_u8 GuidAppleHfs[16] = { 0x00,0x53,0x46,0x48,0x00,0x00,0xAA,0x11,0xAA,0x11,0x00,0x30,0x65,0x43,0xEC,0xAC };
static const gpt_u8 GuidAppleApfs[16] = { 0xEF,0x57,0x34,0x7C,0x00,0x00,0xAA,0x11,0xAA,0x11,0x00,0x30,0x65,0x43,0xEC,0xAC };
static const gpt_u8 GuidAppleBoot[16] = { 0x74,0x6F,0x6F,0x42,0x00,0x00,0xAA,0x11,0xAA,0x11,0x00,0x30,0x65,0x43,0xEC,0xAC };
static const gpt_u8 GuidAppleCs[16] = { 0x72,0x6F,0x74,0x53,0x67,0x61,0xAA,0x11,0xAA,0x11,0x00,0x30,0x65,0x43,0xEC,0xAC };
static const gpt_u8 GuidFbsdSwap[16] = { 0xB5,0x7C,0x6E,0x51,0xCF,0x6E,0xD6,0x11,0x8F,0xF8,0x00,0x02,0x2D,0x09,0x71,0x2B };
static const gpt_u8 GuidFbsdUfs[16] = { 0xB6,0x7C,0x6E,0x51,0xCF,0x6E,0xD6,0x11,0x8F,0xF8,0x00,0x02,0x2D,0x09,0x71,0x2B };
static const gpt_u8 GuidFbsdZfs[16] = { 0xBA,0x7C,0x6E,0x51,0xCF,0x6E,0xD6,0x11,0x8F,0xF8,0x00,0x02,0x2D,0x09,0x71,0x2B };
static const gpt_u8 GuidSolZfs[16] = { 0xC3,0x8C,0x89,0x6A,0xD2,0x1D,0xB2,0x11,0x99,0xA6,0x08,0x00,0x20,0x73,0x66,0x31 };

/* When no file system is found in a GPT partition, its type GUID can
   still say what it is (a swap area, an LVM or RAID member, ...): the
   kind, and the MBR type byte that goes with it; 0 when the GUID says
   nothing more */
static gpt_u8 KindFromGuid(const gpt_u8 *g, gpt_u8 *kind)
{
    static const struct { const gpt_u8 *Guid; gpt_u8 Kind, Type; } map[] = {
        { GuidLinuxSwap, GPT_FS_SWAP, 0x82 },   { GuidLinuxLvm, GPT_FS_LVM2, 0x8E },
        { GuidLinuxRaid, GPT_FS_MDRAID, 0xFD }, { GuidLinuxLuks, GPT_FS_LUKS, 0x83 },
        { GuidLinuxData, GPT_FS_LINUX, 0x83 },  { GuidBiosBoot, GPT_FS_BIOSBOOT, 0xEF },
        { GuidAppleBoot, GPT_FS_APPLEBOOT, 0xAB }, { GuidAppleCs, GPT_FS_CORESTOR, 0xAF },
        { GuidFbsdSwap, GPT_FS_BSDSWAP, 0xA5 }, { GuidFbsdUfs, GPT_FS_UFS2, 0xA5 },
        { GuidFbsdZfs, GPT_FS_ZFS, 0xA5 },      { GuidSolZfs, GPT_FS_ZFS, 0xBF },
        { GuidAppleHfs, GPT_FS_HFSPLUS, 0xAF }, { GuidAppleApfs, GPT_FS_APFS, 0xAF }
    };
    int i;
    for (i = 0; i < (int)(sizeof(map) / sizeof(map[0])); i++)
        if (MemEqual(g, map[i].Guid, 16)) { *kind = map[i].Kind; return map[i].Type; }
    return 0;
}

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
    case 0x82:                return GPT_FS_SWAP;
    case 0x8E:                return GPT_FS_LVM2;
    case 0xFD:                return GPT_FS_MDRAID;
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
     byte 32768    UDF volume recognition sequence, with the anchor at block 256
     byte 65536    Btrfs superblock ("_BHRfS_M" at +0x40)
   The boot-sector file systems come first: when a partition was
   reformatted, the boot sector at sector 0 belongs to the newest one,
   while signatures further in (UDF's, for one) may be left over. */
static gpt_u8 DetectFs(GptReadProc Read, void *Ctx, gpt_u32 ss, gpt_u64 start,
                       gpt_u64 count, gpt_u8 *buf, int *recognized, gpt_u8 *kind);

/* ReFS boot sector (all versions): file system name "ReFS" padded with
   zeros, the "FSRS" structure at 0x10, a power-of-two sector size and a
   volume that fits the partition. A bare "ReFS" string is not enough: a
   partition reformatted with another file system can keep pieces of an
   old boot sector. */
static int IsRefsBoot(const gpt_u8 *b, gpt_u64 count)
{
    gpt_u32 bps = Le32(b + 0x20), spc = Le32(b + 0x24);
    gpt_u64 n = Le64(b + 0x18);
    if (!StrAt(b, 3, "ReFS") || b[7] || b[8] || b[9] || b[10]) return 0;
    if (!StrAt(b, 0x10, "FSRS")) return 0;
    if (bps < 512 || bps > 4096 || (bps & (bps - 1))) return 0;
    if (spc == 0 || (spc & (spc - 1))) return 0;
    if (n == 0) return 0;
    (void)count;    /* the size is in the volume's own sectors, which can
                       differ from the disk's; only sanity-checked above */
    return 1;
}

/* UDF (ECMA-167 / OSTA UDF 1.02 - 2.60). Two independent proofs are
   required:
     - the Volume Recognition Sequence at byte 32768: a BEA01 descriptor
       followed by NSR02 or NSR03;
     - an Anchor Volume Descriptor Pointer at block 256 with a valid tag
       (identifier 2, checksum, and its own location recorded in it), for
       every block size from the sector size up to 4096.
   Formatting another file system over UDF often leaves the recognition
   sequence behind; it is only taken as UDF when no other file system was
   found first and the anchor is intact too. */
static int UdfTagOk(const gpt_u8 *t, gpt_u32 loc, gpt_u16 id)
{
    gpt_u8 sum = 0; int i;
    if (Le16(t) != id) return 0;
    for (i = 0; i < 16; i++) if (i != 4) sum = (gpt_u8)(sum + t[i]);
    return sum == t[4] && Le32(t + 12) == loc;
}

static int IsUdf(GptReadProc Read, void *Ctx, gpt_u32 ss, gpt_u64 start,
                 gpt_u64 count, gpt_u8 *buf)
{
    gpt_u32 step, off, bs;
    int bea, nsr = 0, i;
    /* descriptors are 2048 bytes apart, or one block apart when blocks are
       larger: 4096 is the only larger block size seen on disks */
    for (step = ss > 2048 ? ss : 2048; step <= 4096 && !nsr; step <<= 1)
    for (bea = 0, i = 0, off = 32768; i < 16; i++, off += step) {
        gpt_u64 lba = off / ss;
        const gpt_u8 *d;
        if (lba + 1 >= count || Read(Ctx, start + lba, 1, buf) != 0) return 0;
        d = buf + (off % ss);
        if (d[0] != 0 || d[6] != 1) break;               /* type 0, version 1 */
        if (StrAt(d, 1, "BEA01")) bea = 1;
        else if (bea && (StrAt(d, 1, "NSR02") || StrAt(d, 1, "NSR03"))) { nsr = 1; break; }
        else if (StrAt(d, 1, "TEA01")) break;
        else if (!StrAt(d, 1, "CD001") && !StrAt(d, 1, "BOOT2") && !StrAt(d, 1, "CDW02")) break;
    }
    if (!nsr) return 0;
    for (bs = ss; bs <= 4096; bs <<= 1) {
        gpt_u64 byteoff = (gpt_u64)256 * bs, lba = byteoff / ss;
        if (lba + 1 >= count || Read(Ctx, start + lba, 1, buf) != 0) continue;
        if (UdfTagOk(buf + (gpt_u32)(byteoff % ss), 256, 2)) return 1;
    }
    return 0;
}
gpt_u8 GptDetectFs(GptReadProc Read, void *Ctx, gpt_u32 ss, gpt_u64 start,
                   gpt_u64 count, gpt_u8 *buf, int *recognized, gpt_u8 *kind)
{ return DetectFs(Read, Ctx, ss, start, count, buf, recognized, kind); }

/* reading a few bytes at a byte offset inside the volume */
typedef struct _FSRD {
    GptReadProc Read; void *Ctx; gpt_u64 Start, Count; gpt_u8 *Buf; int Shift; gpt_u32 Mask;
    gpt_u64 Lba;    /* the sector now in Buf, ~0 = none */
} FSRD;
static const gpt_u8 *At(FSRD *r, gpt_u64 off)
{
    gpt_u64 lba = off >> r->Shift;
    if (lba >= r->Count) return 0;
    if (r->Lba != lba) {
        if (r->Read(r->Ctx, r->Start + lba, 1, r->Buf) != 0) { r->Lba = ~(gpt_u64)0; return 0; }
        r->Lba = lba;
    }
    return r->Buf + ((gpt_u32)off & r->Mask);
}
static gpt_u32 Be32(const gpt_u8 *p){return ((gpt_u32)p[0]<<24)|((gpt_u32)p[1]<<16)|((gpt_u32)p[2]<<8)|p[3];}
static gpt_u16 Be16(const gpt_u8 *p){return (gpt_u16)(((gpt_u16)p[0]<<8)|p[1]);}
/* a 32-bit magic number in either byte order */
static int Magic32(const gpt_u8 *p, gpt_u32 m){ return p && (Le32(p) == m || Be32(p) == m); }

/* bcachefs: the original magic and the one bcachefs 1.x writes */
static const gpt_u8 BcachefsMagic[2][16] = {
    { 0xC6,0x85,0x73,0xF6,0x4E,0x1A,0x45,0xCA,0x82,0x65,0xF5,0x7F,0x48,0xBA,0x6D,0x81 },
    { 0xC6,0x85,0x73,0xF6,0x66,0xCE,0x90,0xA9,0xD9,0x6A,0x60,0xCF,0x80,0x3D,0xF7,0xEF } };

#define FS(k, t) do { *kind = (gpt_u8)(k); return (gpt_u8)(t); } while (0)

/* The checks, in the order blkid and the file systems themselves settle
   which signature wins: boot-sector file systems first (a reformatted
   partition keeps older signatures further in, but its first sector
   belongs to the newest), then the superblocks by position, then the
   weak or end-of-volume ones. The type byte returned is the one an MBR
   would carry for it (what a GPT partition is shown as). */
static gpt_u8 DetectFs(GptReadProc Read, void *Ctx, gpt_u32 ss, gpt_u64 start,
                       gpt_u64 count, gpt_u8 *buf, int *recognized, gpt_u8 *kind)
{
    FSRD r; const gpt_u8 *p, *q; gpt_u64 bytes; int i;
    *recognized = 0;
    *kind = GPT_FS_UNKNOWN;
    r.Read = Read; r.Ctx = Ctx; r.Start = start; r.Count = count; r.Buf = buf; r.Lba = ~(gpt_u64)0;
    r.Shift = 0; while ((1u << r.Shift) < ss) r.Shift++;
    r.Mask = ss - 1;
    bytes = count << r.Shift;

    /* ---- sector 0 ---- */
    p = At(&r, 0);
    if (!p) return 0;
    if (StrAt(p, 3, "-FVE-FS-"))  FS(GPT_FS_BITLOCKER, GPT_MBRTYPE_IFS);
    if (StrAt(p, 3, "NTFS    "))  { *recognized = 1; FS(GPT_FS_NTFS, GPT_MBRTYPE_IFS); }
    if (StrAt(p, 3, "EXFAT   "))  { *recognized = 1; FS(GPT_FS_EXFAT, GPT_MBRTYPE_IFS); }
    if (IsRefsBoot(p, count))     FS(GPT_FS_REFS, GPT_MBRTYPE_IFS);
    if (StrAt(p, 0x36, "HPFS    ")) { *recognized = 1; FS(GPT_FS_HPFS, GPT_MBRTYPE_IFS); }
    if (FatFromBpb(p, kind)) {
        *recognized = 1;
        if (*kind == GPT_FS_FAT12) return GPT_MBRTYPE_FAT12;
        if (*kind == GPT_FS_FAT16) return (count < 65536) ? GPT_MBRTYPE_FAT16S : GPT_MBRTYPE_FAT16;
        return GPT_MBRTYPE_FAT32L;
    }
    if (StrAt(p, 0, "LUKS\xBA\xBE") || StrAt(p, 0, "SKUL\xBA\xBE")) FS(GPT_FS_LUKS, 0x83);
    if (StrAt(p, 0, "XFSB"))      FS(GPT_FS_XFS, 0x83);
    if (StrAt(p, 32, "NXSB"))     FS(GPT_FS_APFS, 0xAF);
    if (StrAt(p, 0, "hsqs") || StrAt(p, 0, "sqsh")) FS(GPT_FS_SQUASHFS, 0x83);
    if (Magic32(p, 0x28CD3D45u))  FS(GPT_FS_CRAMFS, 0x83);
    if (Le32(p) == 0xA92B4EFCu)   FS(GPT_FS_MDRAID, 0xFD);        /* md 1.1 */
    if (StrAt(p, 0, "-rom1fs-"))  FS(GPT_FS_ROMFS, 0x83);
    if (Le32(p) == 0x1BADFACEu)   FS(GPT_FS_UNIXBFS, 0x83);
    if (Le32(p) == 0xC5523031u && Le32(p + 4) == 0xC8414D4Du) FS(GPT_FS_HAMMER, 0xA5);
    if (Le32(p) == 0x05172011u && Le32(p + 4) == 0x48414D32u) FS(GPT_FS_HAMMER2, 0xA5);
    if (StrAt(p, 0, "DOS") && p[3] <= 7) FS(GPT_FS_AMIGA, 0x30);
    if (Le16(p) == 0x1985 && (Le16(p + 2) == 0xE001 || Le16(p + 2) == 0xE002 || Le16(p + 2) == 0x2003 ||
                              Le16(p + 2) == 0x2004 || Le16(p + 2) == 0x2006 || Le16(p + 2) == 0xE008 ||
                              Le16(p + 2) == 0xE009)) FS(GPT_FS_JFFS2, 0x83);
    if ((q = At(&r, 572)) != 0 && Le32(q) == 0x012FD16Du) FS(GPT_FS_XIAFS, 0x83);

    /* ---- byte 512: LVM2 label, BeFS, SGI EFS ---- */
    if ((q = At(&r, 512)) != 0) {
        if (StrAt(q, 0, "LABELONE") && StrAt(q, 24, "LVM2 001")) FS(GPT_FS_LVM2, 0x8E);
        if (Le32(q + 32) == 0x42465331u) FS(GPT_FS_BEFS, 0xEB);
        if (Be32(q + 28) == 0x00072959u || Be32(q + 28) == 0x0007295Au) FS(GPT_FS_EFS, 0x83);
    }
    /* ---- byte 1024: ext, F2FS, NILFS2, EROFS, HFS family, VxFS, OCFS2 ---- */
    if ((q = At(&r, 1024)) != 0) {
        /* ext2 / ext3 / ext4: told apart the way blkid does - a journal
           makes ext3; any feature ext3 does not know (extents, 64-bit,
           flex_bg, huge files, metadata checksums, ...) makes ext4 */
        if (Le16(q + 0x38) == 0xEF53) {
            gpt_u32 compat = Le32(q + 0x5C), incompat = Le32(q + 0x60), ro = Le32(q + 0x64);
            if ((incompat & ~0x16u) || (ro & ~0x07u)) FS(GPT_FS_EXT4, 0x83);
            if (compat & 0x04u)                       FS(GPT_FS_EXT3, 0x83);
            FS(GPT_FS_EXT2, 0x83);
        }
        if (Le16(q + 0x38) == 0x137D) FS(GPT_FS_EXTFS, 0x83);
        if (Le32(q) == 0xF2F52010u)   FS(GPT_FS_F2FS, 0x83);
        if (Le16(q + 6) == 0x3434)    FS(GPT_FS_NILFS2, 0x83);
        if (Le32(q) == 0xE0F5E1E2u)   FS(GPT_FS_EROFS, 0x83);
        if (Le32(q) == 0xA501FCF5u)   FS(GPT_FS_VXFS, 0x83);
        if (StrAt(q, 0, "OCFSV2"))    FS(GPT_FS_OCFS2, 0x83);
        if (Be16(q) == 0x4244) {                      /* "BD": HFS, maybe wrapping HFS+ */
            if (Be16(q + 0x7C) == 0x482B) FS(GPT_FS_HFSPLUS, 0xAF);
            FS(GPT_FS_HFS, 0xAF);
        }
        if (Be16(q) == 0x482B && (Be16(q + 2) == 4 || Be16(q + 2) == 5)) FS(GPT_FS_HFSPLUS, 0xAF);
        if (Be16(q) == 0x4858 && (Be16(q + 2) == 4 || Be16(q + 2) == 5)) FS(GPT_FS_HFSX, 0xAF);
        if (Be16(q) == 0xD2D7)        FS(GPT_FS_MFS, 0xAF);
    }
    /* the System V family keeps its magic at 0x3F8 of the superblock */
    for (i = 0; i < 3; i++) {
        static const gpt_u32 base[3] = { 0, 512, 1024 };
        if ((q = At(&r, base[i] + 0x3F8)) == 0) continue;
        if (Magic32(q, 0xFD187E20u)) FS(GPT_FS_SYSV, 0x63);
        if ((q[0] == 0x2B && q[1] == 0x55 && q[2] == 0x44) || (q[0] == 0x44 && q[1] == 0x55 && q[2] == 0x2B)) FS(GPT_FS_XENIX, 0x02);
    }
    if ((q = At(&r, 2048)) != 0 && StrAt(q, 0, "OCFSV2")) FS(GPT_FS_OCFS2, 0x83);
    /* ---- byte 4096: bcachefs, md 1.2, swap (4 KB pages), OCFS2 ---- */
    if ((q = At(&r, 4096)) != 0) {
        int k, v;
        for (v = 0; v < 2; v++) {
            for (k = 0; k < 16 && q[24 + k] == BcachefsMagic[v][k]; k++) ;
            if (k == 16) FS(GPT_FS_BCACHEFS, 0x83);
        }
        if (Le32(q) == 0xA92B4EFCu) FS(GPT_FS_MDRAID, 0xFD);
        if (StrAt(q, 0, "OCFSV2")) FS(GPT_FS_OCFS2, 0x83);
    }
    if ((q = At(&r, 4086)) != 0 && (StrAt(q, 0, "SWAPSPACE2") || StrAt(q, 0, "SWAP-SPACE"))) FS(GPT_FS_SWAP, 0x82);
    /* ---- byte 8192: HPFS, UFS1, old ReiserFS, QNX6, OCFS2 ---- */
    if ((q = At(&r, 8192)) != 0) {
        if (Le32(q) == 0xF995E849u) { *recognized = 1; FS(GPT_FS_HPFS, GPT_MBRTYPE_IFS); }
        if (Le32(q) == 0x68191122u) FS(GPT_FS_QNX6, 0x4D);
        if (StrAt(q, 0, "OCFSV2"))  FS(GPT_FS_OCFS2, 0x83);
    }
    if ((q = At(&r, 8192 + 52)) != 0 && StrAt(q, 0, "ReIsErFs")) FS(GPT_FS_REISERFS, 0x83);
    /* UFS: the superblock is at 8 KB or 64 KB for either version */
    if ((q = At(&r, 8192 + 1372)) != 0) {
        if (Magic32(q, 0x19540119u)) FS(GPT_FS_UFS2, 0xA5);
        if (Magic32(q, 0x00011954u)) FS(GPT_FS_UFS, 0xA5);
    }
    /* ---- byte 16384: swap with 16 KB pages ---- */
    if ((q = At(&r, 16374)) != 0 && StrAt(q, 0, "SWAPSPACE2")) FS(GPT_FS_SWAP, 0x82);
    if ((q = At(&r, 8182)) != 0 && StrAt(q, 0, "SWAPSPACE2")) FS(GPT_FS_SWAP, 0x82);
    /* ---- byte 32768: JFS, UDF, ISO 9660 ---- */
    if ((q = At(&r, 32768)) != 0 && StrAt(q, 0, "JFS1")) FS(GPT_FS_JFS, 0x83);
    /* UDF before ISO 9660 (a UDF bridge disc has both) and before Btrfs:
       mkfs.btrfs wipes foreign signatures, mkudffs leaves byte 65536 alone,
       so with both present UDF is the newer one */
    if (IsUdf(Read, Ctx, ss, start, count, buf)) { r.Lba = ~(gpt_u64)0; FS(GPT_FS_UDF, GPT_MBRTYPE_IFS); }
    r.Lba = ~(gpt_u64)0;
    if ((q = At(&r, 32768)) != 0 && StrAt(q, 1, "CD001") && q[6] == 1) FS(GPT_FS_ISO9660, GPT_MBRTYPE_IFS);
    /* ---- byte 65536: Btrfs, ReiserFS 3.5 / 3.6, Reiser4, UFS2, GFS2, swap 64 KB ---- */
    if ((q = At(&r, 65536)) != 0) {
        if (StrAt(q, 0x40, "_BHRfS_M")) FS(GPT_FS_BTRFS, 0x83);
        if (StrAt(q, 0, "ReIsEr4")) FS(GPT_FS_REISER4, 0x83);
        if (Be32(q) == 0x01161970u && Be32(q + 4) == 1) FS(GPT_FS_GFS2, 0x83);
        if (StrAt(q, 52, "ReIsErFs") || StrAt(q, 52, "ReIsEr2Fs") || StrAt(q, 52, "ReIsEr3Fs")) FS(GPT_FS_REISERFS, 0x83);
    }
    if ((q = At(&r, 65536 + 1372)) != 0) {
        if (Magic32(q, 0x19540119u)) FS(GPT_FS_UFS2, 0xA5);
        if (Magic32(q, 0x00011954u)) FS(GPT_FS_UFS, 0xA5);
    }
    if ((q = At(&r, 65526)) != 0 && StrAt(q, 0, "SWAPSPACE2")) FS(GPT_FS_SWAP, 0x82);
    /* ---- byte 16384 / 131072: ZFS (vdev label nvlist, uberblocks) ---- */
    if ((q = At(&r, 16384 + 0x10)) != 0) {
        int k; for (k = 0; k + 9 <= 256 && k < (int)ss - 16 - 9; k++) if (StrAt(q, k, "pool_guid")) FS(GPT_FS_ZFS, 0xBF);
    }
    for (i = 0; i < 32; i++) {
        q = At(&r, 131072 + (gpt_u64)i * 1024);
        if (q && ((Le32(q) == 0x00BAB10Cu && Le32(q + 4) == 0) || (Be32(q + 4) == 0x00BAB10Cu && Be32(q) == 0))) FS(GPT_FS_ZFS, 0xBF);
    }
    /* ---- 1 MB: VMFS ---- */
    if ((q = At(&r, 1048576)) != 0 && Le32(q) == 0xC001D00Du) FS(GPT_FS_VMFS, 0xFB);
    /* ---- at the end: md RAID 0.90 and 1.0 ---- */
    if (bytes >= 131072) {
        gpt_u64 o90 = (bytes & ~(gpt_u64)65535) - 65536, o10 = ((bytes - 8192) & ~(gpt_u64)4095);
        if ((q = At(&r, o90)) != 0 && Magic32(q, 0xA92B4EFCu)) FS(GPT_FS_MDRAID, 0xFD);
        if ((q = At(&r, o10)) != 0 && Le32(q) == 0xA92B4EFCu) FS(GPT_FS_MDRAID, 0xFD);
    }
    /* ---- weak: Minix (a 16-bit magic only) ---- */
    if ((q = At(&r, 1024)) != 0) {
        gpt_u16 m = Le16(q + 16);
        if ((m == 0x137F || m == 0x138F || m == 0x2468 || m == 0x2478) && Le16(q + 2) != 0) FS(GPT_FS_MINIX, 0x81);
        if (Le16(q + 24) == 0x4D5A && Le16(q + 28) != 0) FS(GPT_FS_MINIX, 0x81);
    }
    return 0;
}
#undef FS

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
    /* the size the table was written for: the backup header is the last
       sector (read from the primary's pointer, or the backup's own LBA) */
    out->TableSectors = (headerLba == 1 ? Le64(h + 32) : headerLba) + 1;

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
            if (!got) got = KindFromGuid(e, &pp->FsKind);
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
    /* No partition table, one file system over the whole disk: a
       superfloppy (its first sector is a boot sector, whose code bytes
       would read as nonsense partition entries), or a disk without the MBR
       signature that holds a file system (an ISO 9660 or UDF image, a
       whole-disk ext / XFS / LUKS, ...) */
    if (DiskSectors > 0) {
        int recog = 0; gpt_u8 kind = 0, type;
        int sig = (Le16(m + 510) == 0xAA55);
        type = DetectFs(Read, Ctx, SectorSize, 0, DiskSectors, sc->Vbr, &recog, &kind);
        if (type && (!sig || kind == GPT_FS_NTFS || kind == GPT_FS_EXFAT || kind == GPT_FS_REFS ||
                     kind == GPT_FS_HPFS || kind == GPT_FS_FAT12 || kind == GPT_FS_FAT16 ||
                     kind == GPT_FS_FAT32 || kind == GPT_FS_BITLOCKER)) {
            GPT_PARTITION *pp = &Out->Part[0];
            ZeroBytes((gpt_u8 *)pp, (gpt_u32)sizeof(*pp));
            Out->IsGpt = 0; Out->Whole = 1; Out->SectorSize = SectorSize; Out->DiskSectors = DiskSectors;
            pp->StartLba = 0; pp->SectorCount = DiskSectors; pp->MbrType = type; pp->FsKind = kind;
            pp->Recognized = (gpt_u8)recog;
            Out->Count = 1;
            GptScratchFree(sc);
            return GPT_OK;
        }
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
        /* an MBR entry holds 32-bit sector numbers */
        if (*last > 0xFFFFFFFFu) *last = 0xFFFFFFFFu;
    }
}

#define GPT_REACH_SLACK 16065u   /* one cylinder of 255 x 63 sectors */

int GptTableBeyondReach(const GPT_DISK *d)
{
    return d->IsGpt && d->TableSectors > d->DiskSectors + GPT_REACH_SLACK;
}

int GptPartBeyondReach(const GPT_DISK *d, const GPT_PARTITION *p)
{
    return p->SectorCount && p->StartLba + p->SectorCount > d->DiskSectors + GPT_REACH_SLACK;
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
    if (d->Whole) return GPT_ERR_PARAM;   /* would overwrite the file system's first sector */
    ss = d->SectorSize;
    diskSectors = d->DiskSectors;
    sc = GptScratchAlloc();
    if (!sc) return GPT_ERR_PARAM;
    m = sc->Sector; ent = sc->Entries; hdr = sc->Vbr;

    /* nothing is written when a partition would not survive it: one
       reaching into the GPT backup area at the end of the reachable disk
       (it would be overwritten), or one an MBR entry cannot describe */
    for (i = 0; i < d->Count; i++) {
        const GPT_PARTITION *p = &d->Part[i];
        gpt_u64 end = p->StartLba + p->SectorCount;
        if (p->SectorCount == 0) continue;
        if (d->IsGpt) {
            gpt_u32 e2 = EntryArraySectors(ss);
            if (diskSectors < 2 * (gpt_u64)e2 + 4 || p->StartLba < 2 + e2 || end > diskSectors - 1 - e2) {
                GptScratchFree(sc); return GPT_ERR_RANGE;
            }
        } else if (p->StartLba > 0xFFFFFFFFu || p->SectorCount > 0xFFFFFFFFu) {
            GptScratchFree(sc); return GPT_ERR_RANGE;
        }
    }

    if (d->IsGpt && diskSectors < 2 * (gpt_u64)EntryArraySectors(ss) + 4) { GptScratchFree(sc); return GPT_ERR_RANGE; }
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
