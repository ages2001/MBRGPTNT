/* tstldm - read the LDM database of disk images, list the disk group and
   write every volume out through the volume mapper.
   usage: tstldm OUTDIR img... [-miss N]  (-miss: pretend image N is absent) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ldmcore.h"

void *GptPlatAlloc(gpt_u32 n) { return malloc(n); }
void  GptPlatFree(void *p) { free(p); }

#define MAXIMG 16
static FILE *img[MAXIMG];
static int nimg;

static int rd(void *ctx, gpt_u64 lba, gpt_u32 n, void *buf)
{
    FILE *f = (FILE *)ctx;
    if (fseeko(f, (off_t)lba * 512, SEEK_SET)) return 1;
    return fread(buf, 512, n, f) == n ? 0 : 1;
}

static int wr(void *ctx, gpt_u64 lba, gpt_u32 n, const void *buf)
{
    FILE *f = (FILE *)ctx;
    if (fseeko(f, (off_t)lba * 512, SEEK_SET)) return 1;
    return fwrite(buf, 512, n, f) == n ? 0 : 1;
}

static int map[LDM_MAX_DISKS];          /* db disk index -> image */

static int io(void *ctx, gpt_u32 d, int write, gpt_u64 off, gpt_u32 len, void *buf)
{
    FILE *f;
    (void)ctx;
    if (map[d] < 0) return 1;
    f = img[map[d]];
    if (fseeko(f, (off_t)off, SEEK_SET)) return 1;
    if (write) return fwrite(buf, 1, len, f) == len ? 0 : 1;
    return fread(buf, 1, len, f) == len ? 0 : 1;
}

static const char *tn(int t)
{
    switch (t) { case LDM_VT_SIMPLE: return "simple"; case LDM_VT_SPANNED: return "spanned";
    case LDM_VT_STRIPED: return "striped"; case LDM_VT_MIRROR: return "mirror"; case LDM_VT_RAID5: return "raid5"; }
    return "?";
}

int main(int argc, char **argv)
{
    static GPT_DISK t; static LDM_HDR h[MAXIMG]; static LDM_DB db, tmp;
    gpt_u64 pub[LDM_MAX_DISKS]; gpt_u32 bps[LDM_MAX_DISKS]; gpt_u8 present[LDM_MAX_DISKS];
    const char *out = argv[1]; int i, miss = -1, have = 0, v;
    const char *putName[8], *putFile[8]; int nput = 0; const char *delName = 0;
    for (i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-miss")) { miss = atoi(argv[++i]); continue; }
        if (!strcmp(argv[i], "-del")) { delName = argv[++i]; continue; }
        if (!strcmp(argv[i], "-put")) { putName[nput] = argv[++i]; putFile[nput++] = argv[++i]; continue; }
        img[nimg] = fopen(argv[i], "r+b");
        if (!img[nimg]) { perror(argv[i]); return 1; }
        nimg++;
    }
    for (i = 0; i < nimg; i++) {
        off_t sz; int rc;
        fseeko(img[i], 0, SEEK_END); sz = ftello(img[i]);
        if (GptParse(rd, img[i], 512, (gpt_u64)sz / 512, &t) != GPT_OK) { printf("img%d: no table\n", i); continue; }
        rc = LdmReadHeader(rd, img[i], 512, (gpt_u64)sz / 512, &t, &h[i]);
        if (rc != GPT_OK) { printf("img%d: not LDM (%d)\n", i, rc); continue; }
        printf("img%d: disk %s group %s (%s) pub %llu+%llu priv %llu+%llu conf %llu+%llu\n", i, h[i].DiskGuid,
               h[i].DgName, h[i].DgGuid, (unsigned long long)h[i].PubStart, (unsigned long long)h[i].PubSize,
               (unsigned long long)h[i].PrivStart, (unsigned long long)h[i].PrivSize,
               (unsigned long long)h[i].ConfStart, (unsigned long long)h[i].ConfSize);
        rc = LdmReadDb(rd, img[i], &h[i], &tmp);
        if (rc != GPT_OK) { printf("  db error %d\n", rc); continue; }
        printf("  committed %llu, %u disks %u vols %u comps %u parts %u recs\n", (unsigned long long)tmp.Committed,
               tmp.NDisks, tmp.NVols, tmp.NComps, tmp.NParts, tmp.NRecs);
        if (!have || tmp.Committed > db.Committed) { if (have) LdmFreeDb(&db); db = tmp; have = 1; }
        else LdmFreeDb(&tmp);
    }
    if (!have) return 1;
    if (delName) {
        int k;
        for (v = 0; v < (int)db.NVols; v++) if (!strcmp(db.Vol[v].Name, delName)) break;
        if (v < (int)db.NVols) {
            printf("delete %s -> %d\n", delName, LdmDeleteVolume(&db, v));
            for (k = 0; k < nimg; k++) printf("write img%d -> %d\n", k, LdmWriteDisk(wr, img[k], &h[k], &db, 0x01DD56DDBD58F8CEull));
        }
    }
    printf("group %s\n", db.DgName);
    for (i = 0; i < (int)db.NDisks; i++) {
        char g[40]; int k;
        LdmGuidToString(db.Disk[i].Guid, g);
        map[i] = -1; pub[i] = 0; bps[i] = 512; present[i] = 0;
        for (k = 0; k < nimg; k++) if (h[k].DgGuid[0] && !strcmp(h[k].DiskGuid, g)) { map[i] = k; pub[i] = h[k].PubStart; }
        if (map[i] >= 0 && map[i] != miss) present[i] = 1;
        printf("  %-8s %s %s\n", db.Disk[i].Name, g, present[i] ? (map[i] >= 0 ? "present" : "") : "missing");
    }
    for (v = 0; v < (int)db.NVols; v++) {
        LDM_VMAP m; int rc = LdmBuildMap(&db, v, pub, bps, &m), st, p, x;
        char g[40]; LdmGuidToString(db.Vol[v].Guid, g);
        printf("  vol %-8s %-7s size %llu type %02X hint %s guid %s", db.Vol[v].Name, tn(db.Vol[v].Type),
               (unsigned long long)db.Vol[v].Size, db.Vol[v].PartType, db.Vol[v].Hint, g);
        if (rc != GPT_OK) { printf(" map error %d\n", rc); continue; }
        st = LdmMapState(&m, present);
        printf(" %s\n", st == LDM_HEALTHY ? "healthy" : st == LDM_DEGRADED ? "degraded" : "failed");
        for (p = 0; p < (int)m.NPlex; p++) for (x = 0; x < (int)m.Plex[p].N; x++)
            printf("     plex %d col %u: %s @%llu +%llu off %llu unit %u\n", p, m.Plex[p].X[x].Column,
                   db.Disk[m.Plex[p].X[x].Disk].Name, (unsigned long long)m.Plex[p].X[x].ByteStart / 512,
                   (unsigned long long)m.Plex[p].X[x].Size / 512, (unsigned long long)m.Plex[p].X[x].Off / 512, m.Plex[p].Unit);
        if (st == LDM_FAILED) continue;
        for (p = 0; p < nput; p++) if (!strcmp(putName[p], db.Vol[v].Name)) {
            FILE *f = fopen(putFile[p], "rb"); static gpt_u8 b2[1 << 20], sc2[2 * LDM_SCRATCH_BYTES]; gpt_u64 off;
            for (off = 0; off < m.Size; off += sizeof(b2)) {
                gpt_u32 n = (m.Size - off < sizeof(b2)) ? (gpt_u32)(m.Size - off) : sizeof(b2);
                size_t got = fread(b2, 1, n, f); if (got < n) memset(b2 + got, 0, n - got);
                rc = LdmVolIo(&m, present, io, 0, 1, off, n, b2, sc2);
                if (rc) { printf("     write error %d\n", rc); break; }
            }
            fclose(f); printf("     wrote %s\n", putFile[p]);
        }
        {
            char fn[512]; FILE *o; gpt_u64 off; static gpt_u8 buf[1 << 20], scratch[2 * LDM_SCRATCH_BYTES];
            snprintf(fn, sizeof(fn), "%s/%s.img", out, db.Vol[v].Name);
            o = fopen(fn, "wb");
            for (off = 0; off < m.Size; off += sizeof(buf)) {
                gpt_u32 n = (m.Size - off < sizeof(buf)) ? (gpt_u32)(m.Size - off) : sizeof(buf);
                rc = LdmVolIo(&m, present, io, 0, 0, off, n, buf, scratch);
                if (rc) { printf("     read error %d at %llu\n", rc, (unsigned long long)off); break; }
                fwrite(buf, 1, n, o);
            }
            fclose(o);
        }
    }
    return 0;
}
