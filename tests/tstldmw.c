/* tstldmw - build a new disk group on blank MBR images with one volume of
   every layout, then write a file system image into each volume through
   the volume mapper.
   usage: tstldmw SRCIMG img0 img1 img2 [img3]   (images: blank, >= 200 MB) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ldmcore.h"

void *GptPlatAlloc(gpt_u32 n) { return malloc(n); }
void  GptPlatFree(void *p) { free(p); }

static FILE *img[4];
static gpt_u64 sec[4];

static int wr(void *ctx, gpt_u64 lba, gpt_u32 n, const void *buf)
{
    FILE *f = (FILE *)ctx;
    if (fseeko(f, (off_t)lba * 512, SEEK_SET)) return 1;
    return fwrite(buf, 512, n, f) == n ? 0 : 1;
}
static int io(void *ctx, gpt_u32 d, int write, gpt_u64 off, gpt_u32 len, void *buf)
{
    FILE *f = img[d]; (void)ctx;
    if (fseeko(f, (off_t)off, SEEK_SET)) return 1;
    if (write) return fwrite(buf, 1, len, f) == len ? 0 : 1;
    return fread(buf, 1, len, f) == len ? 0 : 1;
}

int main(int argc, char **argv)
{
    static LDM_DB db; static LDM_HDR h[4]; gpt_u8 g[16], mbr[512];
    gpt_u64 pubSize[4], pub[4]; gpt_u32 bps[4] = {512,512,512,512}; gpt_u8 present[4] = {1,1,1,1};
    int i, di, vi, rc, nimg = argc - 2; FILE *src;
    srand(12345);
    for (i = 0; i < 16; i++) g[i] = (gpt_u8)rand();
    LdmNewGroup(&db, "TESTDg0", g);
    for (i = 0; i < nimg; i++) {
        gpt_u64 priv, cyl = 16065, pubEnd;
        img[i] = fopen(argv[2 + i], "r+b");
        fseeko(img[i], 0, SEEK_END); sec[i] = (gpt_u64)ftello(img[i]) / 512;
        priv = sec[i] - 2048; pubEnd = priv / cyl * cyl;
        for (di = 0; di < 16; di++) g[di] = (gpt_u8)rand();
        g[7] = (g[7] & 0x0F) | 0x40; g[8] = (g[8] & 0x3F) | 0x80;
        LdmMakeHeader(&h[i], 0, 512, 63, pubEnd - 63, priv, 2048, g, &db);
        LdmAddDisk(&db, g, &di);
        pubSize[i] = pubEnd - 63; pub[i] = 63;
        memset(mbr, 0, 512);
        mbr[440] = (gpt_u8)rand(); mbr[441] = (gpt_u8)rand(); mbr[442] = 0x12; mbr[443] = 0x34;
        mbr[446 + 1] = 1; mbr[446 + 2] = 1; mbr[446 + 4] = 0x42; mbr[446 + 5] = 0xFE; mbr[446 + 6] = 0xFF; mbr[446 + 7] = 0xFF;
        mbr[446 + 8] = 63; { gpt_u32 n = (gpt_u32)(pubEnd - 63); mbr[446+12]=n; mbr[446+13]=n>>8; mbr[446+14]=n>>16; mbr[446+15]=n>>24; }
        mbr[510] = 0x55; mbr[511] = 0xAA;
        wr(img[i], 0, 1, mbr);
    }
    {
        static const struct { int type, n, d[3]; gpt_u64 mb[3]; } spec4[] = {
            { LDM_VT_SIMPLE,  1, {0},     {40} },
            { LDM_VT_SPANNED, 2, {0,1},   {20,30} },
            { LDM_VT_STRIPED, 3, {1,2,3}, {20,20,20} },
            { LDM_VT_MIRROR,  2, {2,3},   {30,30} },
            { LDM_VT_RAID5,   3, {0,1,2}, {20,20,20} },
        }, spec3[] = {
            { LDM_VT_SIMPLE,  1, {0},     {40} },
            { LDM_VT_SPANNED, 2, {0,1},   {20,30} },
            { LDM_VT_STRIPED, 2, {1,2},   {20,20} },
            { LDM_VT_MIRROR,  2, {0,2},   {30,30} },
            { LDM_VT_RAID5,   3, {0,1,2}, {20,20,20} },
        };
        const void *sp = nimg == 3 ? (const void *)spec3 : (const void *)spec4;
        #define spec ((const struct { int type, n, d[3]; gpt_u64 mb[3]; } *)sp)
        for (i = 0; i < 5; i++) {
            LDM_NEWVOL s; int k;
            memset(&s, 0, sizeof(s));
            s.Type = spec[i].type; s.NDisks = spec[i].n; s.PartType = 0x06;
            for (k = 0; k < spec[i].n; k++) { s.Disk[k] = spec[i].d[k]; s.Size[k] = spec[i].mb[k] * 2048; }
            for (k = 0; k < 16; k++) s.Guid[k] = (gpt_u8)rand();
            rc = LdmAddVolume(&db, &s, pubSize, &vi);
            printf("add type %d -> %d (vol %d)\n", spec[i].type, rc, vi);
        }
    }
    for (i = 0; i < nimg; i++) { rc = LdmWriteDisk(wr, img[i], &h[i], &db, 0x01DD56DDBD58F8CEull); printf("write disk %d -> %d\n", i, rc); }

    /* a FAT image of each volume's size, written through the mapper */
    src = fopen(argv[1], "rb");
    for (vi = 0; vi < (int)db.NVols; vi++) {
        LDM_VMAP m; static gpt_u8 buf[1 << 20], scratch[2 * LDM_SCRATCH_BYTES]; gpt_u64 off;
        rc = LdmBuildMap(&db, vi, pub, bps, &m);
        if (rc) { printf("map %d\n", rc); continue; }
        fseeko(src, 0, SEEK_SET);
        for (off = 0; off < m.Size; off += sizeof(buf)) {
            gpt_u32 n = (m.Size - off < sizeof(buf)) ? (gpt_u32)(m.Size - off) : sizeof(buf), k;
            size_t got = fread(buf, 1, n, src);
            for (k = (gpt_u32)got; k < n; k++) buf[k] = (gpt_u8)(off + k);
            rc = LdmVolIo(&m, present, io, 0, 1, off, n, buf, scratch);
            if (rc) { printf("write vol %d err %d\n", vi, rc); break; }
        }
        printf("vol %s size %llu written\n", db.Vol[vi].Name, (unsigned long long)m.Size);
    }
    for (i = 0; i < nimg; i++) fclose(img[i]);
    return 0;
}
