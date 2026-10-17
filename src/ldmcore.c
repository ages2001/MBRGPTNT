/* ldmcore.c - portable Logical Disk Manager support. See ldmcore.h.

   On-disk format (all numbers big-endian):
     PRIVHEAD   private header: disk / group GUIDs, where the public region
                (volumes) and the private region (database) are. On an MBR
                disk a copy at byte 3072 points to the database at the end
                of the disk; on a GPT disk it is in the LDM metadata
                partition. The copies at PrivStart+PhPrim / PhSec are the
                current ones.
     TOCBLOCK   table of contents of the private region: "config" and "log".
     VMDB       header of the config region, followed by 128-byte VBLKs.
     VBLK       slot of one record (disk group, disk, volume, component,
                partition); a record larger than one slot is fragmented.
     KLOG       transaction log pages. */

#include "ldmcore.h"

extern void *GptPlatAlloc(gpt_u32 Size);
extern void  GptPlatFree(void *P);

const gpt_u8 LdmGuidMetadata[16] = {0xAA,0xC8,0x08,0x58,0x8F,0x7E,0xE0,0x42,0x85,0xD2,0xE1,0xE9,0x04,0x34,0xCF,0xB3};
const gpt_u8 LdmGuidData[16]     = {0xA0,0x60,0x9B,0xAF,0x31,0x14,0x62,0x4F,0xBC,0x68,0x33,0x11,0x71,0x4A,0x69,0xAD};
const gpt_u8 LdmGuidMsr[16]      = {0x16,0xE3,0xC9,0xE3,0x5C,0x0B,0xB8,0x4D,0x81,0x7D,0xF9,0x2D,0xF0,0x02,0x15,0xAE};

static const char HostGuid[] = "1b77da20-c717-11d0-a5be-00a0c91db73c";

/* ---- helpers ---- */
static gpt_u32 Be16(const gpt_u8 *p){return ((gpt_u32)p[0]<<8)|p[1];}
static gpt_u32 Be32(const gpt_u8 *p){return ((gpt_u32)p[0]<<24)|((gpt_u32)p[1]<<16)|((gpt_u32)p[2]<<8)|p[3];}
static gpt_u64 Be64(const gpt_u8 *p){return ((gpt_u64)Be32(p)<<32)|Be32(p+4);}
static void PBe16(gpt_u8 *p,gpt_u32 v){p[0]=(gpt_u8)(v>>8);p[1]=(gpt_u8)v;}
static void PBe32(gpt_u8 *p,gpt_u32 v){p[0]=(gpt_u8)(v>>24);p[1]=(gpt_u8)(v>>16);p[2]=(gpt_u8)(v>>8);p[3]=(gpt_u8)v;}
static void PBe64(gpt_u8 *p,gpt_u64 v){PBe32(p,(gpt_u32)(v>>32));PBe32(p+4,(gpt_u32)v);}
static gpt_u32 Le32(const gpt_u8 *p){return (gpt_u32)p[0]|((gpt_u32)p[1]<<8)|((gpt_u32)p[2]<<16)|((gpt_u32)p[3]<<24);}
static void Copy(void *d,const void *s,gpt_u32 n){gpt_u8 *a=(gpt_u8*)d;const gpt_u8 *b=(const gpt_u8*)s;while(n--)*a++=*b++;}
static void Zero(void *d,gpt_u32 n){gpt_u8 *a=(gpt_u8*)d;while(n--)*a++=0;}
static int  Same(const void *a,const void *b,gpt_u32 n){const gpt_u8 *x=(const gpt_u8*)a,*y=(const gpt_u8*)b;while(n--)if(*x++!=*y++)return 0;return 1;}
static gpt_u32 Slen(const char *s){gpt_u32 n=0;while(s[n])n++;return n;}
static void Scopy(char *d,const char *s,gpt_u32 max){gpt_u32 i=0;while(s[i]&&i+1<max){d[i]=s[i];i++;}d[i]=0;}
static int  Seq(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static void Xor(gpt_u8 *d,const gpt_u8 *s,gpt_u32 n){while(n--)*d++^=*s++;}

/* 64-bit by 32-bit division without the compiler's runtime helpers (the
   NT 3.1 kernel does not export them) */
static gpt_u64 Div64(gpt_u64 n, gpt_u32 d, gpt_u32 *rem)
{
    gpt_u64 q = 0, r = 0; int i;
    if ((n >> 32) == 0) { gpt_u32 a = (gpt_u32)n; if (rem) *rem = a % d; return a / d; }
    for (i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) { r -= d; q |= (gpt_u64)1 << i; }
    }
    if (rem) *rem = (gpt_u32)r;
    return q;
}

static gpt_u32 Checksum(const gpt_u8 *s)
{ gpt_u32 i, c = 0; for (i = 0; i < 512; i++) if (i < 8 || i > 11) c += s[i]; return c; }

static int Hex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}

/* GUID text <-> binary, in the mixed-endian layout Windows stores binary
   GUIDs in (same as GPT) */
int LdmStringToGuid(const char *s, gpt_u8 *g)
{
    static const int pos[16] = {6,4,2,0, 11,9, 16,14, 19,21, 24,26,28,30,32,34};
    int i;
    if (Slen(s) < 36) return 0;
    for (i = 0; i < 16; i++) {
        int h = Hex(s[pos[i]]), l = Hex(s[pos[i] + 1]);
        if (h < 0 || l < 0) return 0;
        g[i] = (gpt_u8)(h * 16 + l);
    }
    return 1;
}

void LdmGuidToString(const gpt_u8 *g, char *o)
{
    static const int ord[16] = {3,2,1,0, 5,4, 7,6, 8,9, 10,11,12,13,14,15};
    const char *hx = "0123456789abcdef"; int i, k = 0;
    for (i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) o[k++] = '-';
        o[k++] = hx[g[ord[i]] >> 4]; o[k++] = hx[g[ord[i]] & 15];
    }
    o[k] = 0;
}

/* ---- variable-length fields of the records ---- */
typedef struct { const gpt_u8 *p, *end; int bad; } RD;
static gpt_u32 RByte(RD *r){if(r->p>=r->end){r->bad=1;return 0;}return *r->p++;}
static void RSkip(RD *r,gpt_u32 n){if(r->p+n>r->end){r->bad=1;r->p=r->end;}else r->p+=n;}
static gpt_u64 RFix(RD *r,gpt_u32 n){gpt_u64 v=0;while(n--)v=(v<<8)|RByte(r);return v;}
static gpt_u64 RVar(RD *r){gpt_u32 n=RByte(r);if(n>8){r->bad=1;return 0;}return RFix(r,n);}
static void RStr(RD *r,char *out,gpt_u32 max)
{
    gpt_u32 n = RByte(r), i;
    for (i = 0; i < n; i++) { gpt_u32 c = RByte(r); if (out && i + 1 < max) out[i] = (char)c; }
    if (out) out[(n + 1 < max) ? n : max - 1] = 0;
}

typedef struct { gpt_u8 *p; gpt_u32 n, max; } WR;
static void WByte(WR *w,gpt_u32 v){if(w->n<w->max)w->p[w->n]=(gpt_u8)v;w->n++;}
static void WFix(WR *w,gpt_u64 v,gpt_u32 n){while(n--)WByte(w,(gpt_u32)(v>>(8*n))&0xFF);}
static void WVar(WR *w,gpt_u64 v){gpt_u32 n=0;gpt_u64 t=v;while(t){n++;t>>=8;}WByte(w,n);WFix(w,v,n);}
/* "padded" integers: 1-4 bytes, or 8 */
static void WPad(WR *w,gpt_u64 v){gpt_u32 n=0;gpt_u64 t=v;while(t){n++;t>>=8;}if(n>4)n=8;WByte(w,n);WFix(w,v,n);}
static void WStr(WR *w,const char *s){gpt_u32 n=Slen(s),i;WByte(w,n);for(i=0;i<n;i++)WByte(w,(gpt_u8)s[i]);}
static void WRaw(WR *w,const gpt_u8 *s,gpt_u32 n){gpt_u32 i;for(i=0;i<n;i++)WByte(w,s[i]);}

/* ---- headers ---- */

static int ParsePh(const gpt_u8 *p, LDM_HDR *h)
{
    if (!Same(p, "PRIVHEAD", 8)) return 0;
    if (Be32(p + 8) != Checksum(p)) return 0;
    h->Usn = Be64(p + 0x18);
    h->PhPrim = Be64(p + 0x20);
    h->PhSec = Be64(p + 0x28);
    Scopy(h->DiskGuid, (const char *)p + 0x30, 40);
    Scopy(h->HostGuid, (const char *)p + 0x70, 40);
    Scopy(h->DgGuid,   (const char *)p + 0xB0, 40);
    { char n[32]; Copy(n, p + 0xF0, 31); n[31] = 0; Scopy(h->DgName, n, 32); }
    h->PubStart  = Be64(p + 0x11B);
    h->PubSize   = Be64(p + 0x123);
    h->PrivStart = Be64(p + 0x12B);
    h->PrivSize  = Be64(p + 0x133);
    h->Toc1 = Be64(p + 0x13B);
    h->Toc2 = Be64(p + 0x143);
    Copy(h->Ph, p, 512);
    return 1;
}

static int ParseToc(const gpt_u8 *t, LDM_HDR *h, gpt_u64 *usn)
{
    int i, ok = 0;
    if (!Same(t, "TOCBLOCK", 8) || Be32(t + 8) != Checksum(t)) return 0;
    *usn = Be64(t + 0x0C);
    for (i = 0; i < 4; i++) {
        const gpt_u8 *e = t + 0x24 + 34 * i;
        if (Same(e, "config\0\0", 8)) { h->ConfStart = Be64(e + 10); h->ConfSize = Be64(e + 18); ok |= 1; }
        else if (Same(e, "log\0\0\0\0\0", 8)) { h->LogStart = Be64(e + 10); h->LogSize = Be64(e + 18); ok |= 2; }
    }
    return ok == 3;
}

int LdmReadHeader(GptReadProc Read, void *Ctx, gpt_u32 ss, gpt_u64 sectors,
                  const GPT_DISK *tbl, LDM_HDR *hdr)
{
    gpt_u8 *buf; LDM_HDR cand; int found = 0, isGpt = tbl->IsGpt, i;
    gpt_u64 lbas[4], tocLba[6], best = 0;
    gpt_u32 nl = 0, k;

    Zero(hdr, sizeof(*hdr));
    if (isGpt) {
        for (i = 0; i < (int)tbl->Count; i++)
            if (tbl->Part[i].IsGptEntry && Same(tbl->Part[i].TypeGuid, LdmGuidMetadata, 16)) break;
        if (i >= (int)tbl->Count) return LDM_ERR_NOTLDM;
        lbas[nl++] = tbl->Part[i].StartLba + tbl->Part[i].SectorCount - 1;
    } else {
        for (i = 0; i < (int)tbl->Count; i++) if (tbl->Part[i].MbrType == 0x42) break;
        if (i >= (int)tbl->Count) return LDM_ERR_NOTLDM;
        lbas[nl++] = 3072 / ss;           /* the copy after the partition table */
        lbas[nl++] = sectors - 1;
    }
    buf = (gpt_u8 *)GptPlatAlloc(4096 * 2);
    if (!buf) return GPT_ERR_IO;

    /* a first PRIVHEAD only for its pointers to the others */
    for (k = 0; k < nl && !found; k++) {
        Zero(&cand, sizeof(cand));
        if (Read(Ctx, lbas[k], 1, buf) == 0 && ParsePh(buf, &cand)) found = 1;
    }
    if (!found && isGpt) {
        /* the metadata partition: look for a PRIVHEAD anywhere in it */
        const GPT_PARTITION *m = &tbl->Part[i];
        gpt_u64 s;
        for (s = 0; s < m->SectorCount && s < 4096 && !found; s++)
            if (Read(Ctx, m->StartLba + m->SectorCount - 1 - s, 1, buf) == 0 && ParsePh(buf, &cand)) found = 1;
    }
    if (!found) { GptPlatFree(buf); return LDM_ERR_NOTLDM; }

    /* the current copies, newest wins (the copy at byte 3072 can be stale) */
    *hdr = cand;
    found = 0;
    {
        gpt_u64 c[3]; gpt_u32 nc = 0;
        c[nc++] = cand.PrivStart + cand.PhPrim;
        c[nc++] = cand.PrivStart + cand.PhSec;
        c[nc++] = sectors - 1;
        for (k = 0; k < nc; k++) {
            LDM_HDR t; Zero(&t, sizeof(t));
            if (c[k] >= sectors) continue;
            if (Read(Ctx, c[k], 1, buf) == 0 && ParsePh(buf, &t) && t.DgGuid[0] &&
                Seq(t.DiskGuid, cand.DiskGuid) && (!found || t.Usn > hdr->Usn)) { *hdr = t; found = 1; }
        }
        if (!found && cand.DgGuid[0]) found = 1;
        /* the copy after the partition table may carry only the disk's own
           fields (a disk made dynamic before it joined its group) while the
           current copies lie where the disk driver cannot read (past the
           last whole cylinder on NT 3.x): its pointers still lead to the
           table of contents, and the group comes from the database header */
        if (!found && cand.DiskGuid[0] && cand.PrivSize) found = 1;
    }
    if (!found || hdr->PrivSize == 0) { GptPlatFree(buf); return LDM_ERR_NOTLDM; }

    /* table of contents: the ones the header points to, then the usual
       places; the newest valid one wins */
    found = 0;
    tocLba[0] = hdr->Toc1; tocLba[1] = hdr->Toc2; tocLba[2] = 1; tocLba[3] = 2;
    tocLba[4] = hdr->PrivSize - 3; tocLba[5] = hdr->PrivSize - 2;
    for (k = 0; k < 6; k++) {
        LDM_HDR t = *hdr; gpt_u64 usn;
        if (tocLba[k] >= hdr->PrivSize) continue;
        if (Read(Ctx, hdr->PrivStart + tocLba[k], 1, buf) == 0 && ParseToc(buf, &t, &usn) &&
            (!found || usn > best)) { best = usn; t.Toc1 = hdr->Toc1; *hdr = t; found = 1; }
    }
    if (found && !hdr->DgGuid[0] && hdr->ConfSize) {
        if (Read(Ctx, hdr->PrivStart + hdr->ConfStart, 1, buf) == 0 && Same(buf, "VMDB", 4)) {
            char n[65];
            Copy(n, buf + 0x16, 31); n[31] = 0; Scopy(hdr->DgName, n, 32);
            Copy(n, buf + 0x35, 64); n[64] = 0; Scopy(hdr->DgGuid, n, 40);
        }
    }
    GptPlatFree(buf);
    if (!found || !hdr->DgGuid[0] || hdr->ConfSize == 0 || hdr->ConfStart + hdr->ConfSize > hdr->PrivSize) return LDM_ERR_NOTLDM;
    hdr->IsGpt = isGpt;
    hdr->SectorSize = ss;
    return GPT_OK;
}

/* ---- database ---- */

void LdmFreeDb(LDM_DB *db)
{
    gpt_u32 i;
    for (i = 0; i < db->NRecs; i++) if (db->Rec[i].Data) { GptPlatFree(db->Rec[i].Data); db->Rec[i].Data = 0; }
    db->NRecs = db->NDisks = db->NVols = db->NComps = db->NParts = 0;
}

static int ParseRec(LDM_DB *db, int ri)
{
    LDM_REC *rc = &db->Rec[ri];
    RD r; gpt_u32 flags = rc->Data[2], rev = rc->Type >> 4, type = rc->Type & 15;
    r.p = rc->Data + 8; r.end = rc->Data + rc->Len; r.bad = 0;
    switch (type) {
    case 1: {                                   /* volume */
        LDM_VOL *v; char ts[8];
        if (db->NVols >= LDM_MAX_VOLS) return 0;
        v = &db->Vol[db->NVols]; Zero(v, sizeof(*v));
        v->Id = RVar(&r); RStr(&r, v->Name, 32); RStr(&r, ts, 8);
        v->IsRaid5 = Seq(ts, "raid5");
        RStr(&r, 0, 0);                         /* disable drive letter assignment */
        RSkip(&r, 14);                          /* state */
        RSkip(&r, 1);                           /* read policy */
        v->Number = (gpt_u32)RVar(&r);
        v->Flags = (gpt_u32)RFix(&r, 4);
        v->NComps = (gpt_u32)RVar(&r);
        RSkip(&r, 16);                          /* commit id, transaction id */
        v->Size = RVar(&r);
        RSkip(&r, 4);
        v->PartType = (gpt_u8)RByte(&r);
        if (r.p + 16 <= r.end) { Copy(v->Guid, r.p, 16); r.p += 16; } else r.bad = 1;
        if (flags & 0x08) RVar(&r);
        if (flags & 0x20) RVar(&r);
        if (flags & 0x80) RVar(&r);
        if (flags & 0x02) RStr(&r, v->Hint, 8);
        (void)rev;
        if (r.bad) return 0;
        rc->Id = v->Id; v->Rec = ri; db->NVols++;
        return 1;
    }
    case 2: {                                   /* component */
        LDM_COMP *c;
        if (db->NComps >= LDM_MAX_COMPS) return 0;
        c = &db->Comp[db->NComps]; Zero(c, sizeof(*c));
        c->Id = RVar(&r); RStr(&r, c->Name, 32); RStr(&r, 0, 0);
        c->Layout = (gpt_u8)RByte(&r);
        c->Flags = (gpt_u32)RFix(&r, 4);
        c->NParts = (gpt_u32)RVar(&r);
        RSkip(&r, 16);
        c->VolId = RVar(&r);
        RVar(&r);                               /* log SD */
        if (flags & 0x10) { c->Stripe = RVar(&r); c->Columns = (gpt_u32)RVar(&r); }
        if (r.bad) return 0;
        rc->Id = c->Id; c->Rec = ri; db->NComps++;
        return 1;
    }
    case 3: {                                   /* partition (extent) */
        LDM_PART *p;
        if (db->NParts >= LDM_MAX_PARTS) return 0;
        p = &db->Part[db->NParts]; Zero(p, sizeof(*p));
        p->Id = RVar(&r); RStr(&r, p->Name, 32);
        p->Flags = (gpt_u32)RFix(&r, 4);
        RSkip(&r, 8);
        p->Start = RFix(&r, 8);
        p->ColOff = RFix(&r, 8);
        p->Size = RVar(&r);
        p->CompId = RVar(&r);
        p->DiskId = RVar(&r);
        if (flags & 0x08) p->Column = (gpt_u32)RVar(&r);
        if (r.bad) return 0;
        rc->Id = p->Id; p->Rec = ri; db->NParts++;
        return 1;
    }
    case 4: {                                   /* disk */
        LDM_DISK *d; char gs[40];
        if (db->NDisks >= LDM_MAX_DISKS) return 0;
        d = &db->Disk[db->NDisks]; Zero(d, sizeof(*d));
        d->Id = RVar(&r); RStr(&r, d->Name, 32);
        if (rev == 3) { RStr(&r, gs, 40); if (!LdmStringToGuid(gs, d->Guid)) return 0; }
        else if (r.p + 16 <= r.end) { Copy(d->Guid, r.p, 16); r.p += 16; }
        else return 0;
        if (r.bad) return 0;
        rc->Id = d->Id; d->Rec = ri; db->NDisks++;
        return 1;
    }
    case 5: {                                   /* disk group */
        char gs[40];
        db->DgId = RVar(&r); RStr(&r, db->DgName, 32);
        if (rev == 3) { RStr(&r, gs, 40); Scopy(db->DgGuid, gs, 40); }
        rc->Id = db->DgId;
        return r.bad ? 0 : 1;
    }
    default:
        return 1;                               /* unknown: kept as is */
    }
}

/* derive each volume's layout type */
static void Classify(LDM_DB *db)
{
    gpt_u32 v, c;
    for (v = 0; v < db->NVols; v++) {
        LDM_VOL *vo = &db->Vol[v]; gpt_u32 nc = 0, np = 0; gpt_u8 lay = 0;
        for (c = 0; c < db->NComps; c++)
            if (db->Comp[c].VolId == vo->Id) { nc++; np += db->Comp[c].NParts; lay = db->Comp[c].Layout; }
        if (lay == LDM_CL_RAID5 || vo->IsRaid5) vo->Type = LDM_VT_RAID5;
        else if (nc > 1) vo->Type = LDM_VT_MIRROR;
        else if (lay == LDM_CL_STRIPE) vo->Type = LDM_VT_STRIPED;
        else vo->Type = (np > 1) ? LDM_VT_SPANNED : LDM_VT_SIMPLE;
    }
}

#define MAXPEND 24
typedef struct { gpt_u32 grp, nfrag, got; gpt_u8 *buf; } PENDREC;

static int AddRec(LDM_DB *db, const gpt_u8 *payload, gpt_u32 avail)
{
    gpt_u32 len;
    LDM_REC *rc;
    if (avail < 8) return 1;
    len = Be32(payload + 4) + 8;
    if (len > avail || (payload[3] & 15) == 0) return 1;  /* blank / damaged */
    if (Be16(payload) == 0x0002) return 1;               /* pending activation */
    if (db->NRecs >= LDM_MAX_RECS) return 0;
    rc = &db->Rec[db->NRecs];
    rc->Data = (gpt_u8 *)GptPlatAlloc(len);
    if (!rc->Data) return 0;
    Copy(rc->Data, payload, len);
    rc->Len = len; rc->Type = payload[3]; rc->Id = 0;
    db->NRecs++;
    if (!ParseRec(db, (int)db->NRecs - 1)) { GptPlatFree(rc->Data); rc->Data = 0; db->NRecs--; }
    return 1;
}

int LdmReadDb(GptReadProc Read, void *Ctx, const LDM_HDR *h, LDM_DB *db)
{
    gpt_u32 ss = h->SectorSize, chunk = 64 * 1024, vs, first, slotData, i;
    gpt_u64 confLba = h->PrivStart + h->ConfStart, total = h->ConfSize * ss, pos;
    gpt_u8 *buf; PENDREC pend[MAXPEND]; gpt_u32 npend = 0; int rc = GPT_OK;

    Zero(db, sizeof(*db));
    buf = (gpt_u8 *)GptPlatAlloc(chunk);
    if (!buf) return GPT_ERR_IO;
    if (Read(Ctx, confLba, (4096 + ss - 1) / ss, buf) != 0) { GptPlatFree(buf); return GPT_ERR_IO; }
    if (!Same(buf, "VMDB", 4)) { GptPlatFree(buf); return GPT_ERR_CORRUPT; }
    Copy(db->Vmdb, buf, 512);
    vs = Be32(buf + 8); first = Be32(buf + 12);
    if (vs < 32 || vs > 4096 || first < 4 || first > 4096 || (first % vs) || (chunk % vs)) { GptPlatFree(buf); return GPT_ERR_CORRUPT; }
    db->VblkSize = vs; db->VblkFirst = first;
    db->Committed = Be64(buf + 0x75);
    { char n[32]; Copy(n, buf + 0x16, 31); n[31] = 0; Scopy(db->DgName, n, 32); }
    { char g[65]; Copy(g, buf + 0x35, 64); g[64] = 0; Scopy(db->DgGuid, g, 40); }
    db->ConfSectors = Div64(total, 512, 0);
    slotData = vs - 16;
    Zero(pend, sizeof(pend));

    for (pos = 0; pos < total && rc == GPT_OK; pos += chunk) {
        gpt_u32 n = (total - pos < chunk) ? (gpt_u32)(total - pos) : chunk, off;
        if (Read(Ctx, confLba + Div64(pos, ss, 0), (n + ss - 1) / ss, buf) != 0) { rc = GPT_ERR_IO; break; }
        for (off = 0; off + vs <= n; off += vs) {
            const gpt_u8 *b = buf + off;
            gpt_u32 grp, fr, nf;
            if (pos + off < first) continue;
            if (!Same(b, "VBLK", 4)) continue;
            grp = Be32(b + 8); fr = Be16(b + 12); nf = Be16(b + 14);
            if (grp == 0 || nf == 0 || fr >= nf) continue;
            if (nf == 1) { if (!AddRec(db, b + 16, slotData)) rc = LDM_ERR_FULL; continue; }
            for (i = 0; i < npend; i++) if (pend[i].grp == grp) break;
            if (i == npend) {
                if (npend >= MAXPEND || nf > 64) continue;
                pend[i].grp = grp; pend[i].nfrag = nf; pend[i].got = 0;
                pend[i].buf = (gpt_u8 *)GptPlatAlloc(nf * slotData);
                if (!pend[i].buf) { rc = LDM_ERR_FULL; break; }
                Zero(pend[i].buf, nf * slotData);
                npend++;
            }
            if (fr < pend[i].nfrag) { Copy(pend[i].buf + fr * slotData, b + 16, slotData); pend[i].got++; }
        }
    }
    for (i = 0; i < npend; i++) {
        if (rc == GPT_OK && pend[i].got == pend[i].nfrag && !AddRec(db, pend[i].buf, pend[i].nfrag * slotData)) rc = LDM_ERR_FULL;
        GptPlatFree(pend[i].buf);
    }
    GptPlatFree(buf);
    if (rc != GPT_OK) { LdmFreeDb(db); return rc; }
    Classify(db);
    return GPT_OK;
}

int LdmFindDisk(const LDM_DB *db, const char *gs)
{
    gpt_u8 g[16]; gpt_u32 i;
    if (!LdmStringToGuid(gs, g)) return -1;
    for (i = 0; i < db->NDisks; i++) if (Same(db->Disk[i].Guid, g, 16)) return (int)i;
    return -1;
}

static int DiskIdx(const LDM_DB *db, gpt_u64 id)
{ gpt_u32 i; for (i = 0; i < db->NDisks; i++) if (db->Disk[i].Id == id) return (int)i; return -1; }

/* ---- volume layout ---- */

int LdmBuildMap(const LDM_DB *db, int vi, const gpt_u64 *pub, const gpt_u32 *bps, LDM_VMAP *m)
{
    const LDM_VOL *v = &db->Vol[vi]; gpt_u32 c, p;
    Zero(m, sizeof(*m));
    m->Type = v->Type;
    m->Size = v->Size * 512;
    for (c = 0; c < db->NComps; c++) {
        const LDM_COMP *co = &db->Comp[c]; LDM_PLEX *pl;
        if (co->VolId != v->Id) continue;
        if (m->NPlex >= LDM_MAX_PLEX) return LDM_ERR_FULL;
        pl = &m->Plex[m->NPlex++];
        pl->Layout = co->Layout;
        pl->Columns = (co->Layout == LDM_CL_CONCAT) ? 1 : co->Columns;
        pl->Unit = (gpt_u32)(co->Stripe * 512);
        if (co->Layout != LDM_CL_CONCAT && (pl->Columns == 0 || pl->Unit == 0)) return GPT_ERR_CORRUPT;
        if (co->Layout == LDM_CL_RAID5 && pl->Columns < 3) return GPT_ERR_CORRUPT;
        for (p = 0; p < db->NParts; p++) {
            const LDM_PART *pa = &db->Part[p]; LDM_X *x; int d;
            if (pa->CompId != co->Id) continue;
            d = DiskIdx(db, pa->DiskId);
            if (d < 0) return GPT_ERR_CORRUPT;
            if (pl->N >= LDM_MAX_XPP) return LDM_ERR_FULL;
            x = &pl->X[pl->N++];
            x->Disk = (gpt_u32)d;
            x->ByteStart = pub[d] * bps[d] + pa->Start * 512;
            x->Size = pa->Size * 512;
            x->Off = pa->ColOff * 512;
            x->Column = (co->Layout == LDM_CL_CONCAT) ? 0 : pa->Column;
            x->Stale = (pa->Flags & 0x0008) || (co->Flags & 0x06);
            if (x->Column >= pl->Columns) return GPT_ERR_CORRUPT;
        }
        if (pl->N == 0) return GPT_ERR_CORRUPT;
    }
    return m->NPlex ? GPT_OK : GPT_ERR_CORRUPT;
}

int LdmMapUsesDisk(const LDM_VMAP *m, gpt_u32 d)
{ gpt_u32 p, i; for (p = 0; p < m->NPlex; p++) for (i = 0; i < m->Plex[p].N; i++) if (m->Plex[p].X[i].Disk == d) return 1; return 0; }

static int Usable(const LDM_X *x, const gpt_u8 *pr) { return pr[x->Disk] && !x->Stale; }

static int PlexComplete(const LDM_PLEX *pl, const gpt_u8 *pr)
{ gpt_u32 i; for (i = 0; i < pl->N; i++) if (!Usable(&pl->X[i], pr)) return 0; return 1; }

int LdmMapState(const LDM_VMAP *m, const gpt_u8 *pr)
{
    gpt_u32 p, i, c, good = 0;
    if (m->Type == LDM_VT_RAID5) {
        const LDM_PLEX *pl = &m->Plex[0]; gpt_u32 bad = 0;
        for (c = 0; c < pl->Columns; c++) {
            int ok = 1;
            for (i = 0; i < pl->N; i++) if (pl->X[i].Column == c && !Usable(&pl->X[i], pr)) ok = 0;
            if (!ok) bad++;
        }
        return bad == 0 ? LDM_HEALTHY : bad == 1 ? LDM_DEGRADED : LDM_FAILED;
    }
    for (p = 0; p < m->NPlex; p++) if (PlexComplete(&m->Plex[p], pr)) good++;
    if (good == m->NPlex) return LDM_HEALTHY;
    return good ? LDM_DEGRADED : LDM_FAILED;
}

/* the extent of column col holding column offset coff */
static int ColFind(const LDM_PLEX *pl, gpt_u32 col, gpt_u64 coff, gpt_u32 *xi, gpt_u64 *dOff, gpt_u64 *run)
{
    gpt_u32 i;
    for (i = 0; i < pl->N; i++) {
        const LDM_X *x = &pl->X[i];
        if (x->Column == col && coff >= x->Off && coff < x->Off + x->Size) {
            *xi = i; *dOff = x->ByteStart + (coff - x->Off); *run = x->Off + x->Size - coff;
            return 1;
        }
    }
    return 0;
}

/* where a plex keeps plex offset off: data extent, disk offset, contiguous
   bytes; for RAID-5 also the parity column and the column offset */
static int PlexMap(const LDM_PLEX *pl, gpt_u64 off, gpt_u32 *xi, gpt_u64 *dOff, gpt_u64 *run,
                   gpt_u32 *pcol, gpt_u64 *coffOut)
{
    gpt_u32 within, col; gpt_u64 u, row, coff, r;
    if (pl->Layout == LDM_CL_CONCAT) return ColFind(pl, 0, off, xi, dOff, run);
    u = Div64(off, pl->Unit, &within);
    if (pl->Layout == LDM_CL_STRIPE) {
        row = Div64(u, pl->Columns, &col);
    } else {
        gpt_u32 pos, rm, pc;
        row = Div64(u, pl->Columns - 1, &pos);
        Div64(row, pl->Columns, &rm);
        pc = pl->Columns - 1 - rm;              /* left-symmetric parity rotation */
        col = (pc + 1 + pos) % pl->Columns;
        if (pcol) *pcol = pc;
    }
    coff = row * pl->Unit + within;
    if (coffOut) *coffOut = coff;
    if (!ColFind(pl, col, coff, xi, dOff, run)) return 0;
    r = pl->Unit - within;
    if (*run > r) *run = r;
    return 1;
}

int LdmMapContig(const LDM_VMAP *m, const gpt_u8 *pr, int write, gpt_u64 off, gpt_u32 len,
                 gpt_u32 *disk, gpt_u64 *dOff)
{
    gpt_u32 p, xi; gpt_u64 run;
    if (m->Type == LDM_VT_MIRROR && write) return 0;
    if (m->Type == LDM_VT_RAID5 && write) return 0;
    for (p = 0; p < m->NPlex; p++) {
        if (!PlexMap(&m->Plex[p], off, &xi, dOff, &run, 0, 0)) return 0;
        if (Usable(&m->Plex[p].X[xi], pr)) {
            if (run < len) return 0;
            *disk = m->Plex[p].X[xi].Disk;
            return 1;
        }
        if (m->Type != LDM_VT_MIRROR) return 0;
    }
    return 0;
}

int LdmVolIo(const LDM_VMAP *m, const gpt_u8 *pr, LdmDiskIo io, void *ctx, int write,
             gpt_u64 off, gpt_u32 len, void *bufv, gpt_u8 *scratch)
{
    gpt_u8 *buf = (gpt_u8 *)bufv;
    if ((off & 511) || (len & 511) || off + len > m->Size) return GPT_ERR_PARAM;
    while (len) {
        gpt_u32 xi, n, p; gpt_u64 dOff, run;
        if (m->Type == LDM_VT_RAID5) {
            const LDM_PLEX *pl = &m->Plex[0]; gpt_u32 pcol = 0, c; gpt_u64 coff;
            const LDM_X *dx;
            if (!PlexMap(pl, off, &xi, &dOff, &run, &pcol, &coff)) return GPT_ERR_CORRUPT;
            dx = &pl->X[xi];
            if (run > LDM_SCRATCH_BYTES) run = LDM_SCRATCH_BYTES;
            n = (run < len) ? (gpt_u32)run : len;
            if (!write && Usable(dx, pr)) {
                if (io(ctx, dx->Disk, 0, dOff, n, buf)) return GPT_ERR_IO;
            } else if (!write) {
                /* rebuild from the other columns (data and parity) */
                Zero(buf, n);
                for (c = 0; c < pl->Columns; c++) {
                    gpt_u32 cx; gpt_u64 cOff, crun;
                    if (c == dx->Column) continue;
                    if (!ColFind(pl, c, coff, &cx, &cOff, &crun) || !Usable(&pl->X[cx], pr) || crun < n) return LDM_ERR_MISSING;
                    if (io(ctx, pl->X[cx].Disk, 0, cOff, n, scratch)) return GPT_ERR_IO;
                    Xor(buf, scratch, n);
                }
            } else {
                /* parity = new data ^ the other data units of the row */
                gpt_u8 *par = scratch + LDM_SCRATCH_BYTES;
                gpt_u32 px; gpt_u64 pOff, prun;
                if (!Usable(dx, pr)) return LDM_ERR_MISSING;
                Copy(par, buf, n);
                for (c = 0; c < pl->Columns; c++) {
                    gpt_u32 cx; gpt_u64 cOff, crun;
                    if (c == dx->Column || c == pcol) continue;
                    if (!ColFind(pl, c, coff, &cx, &cOff, &crun) || !Usable(&pl->X[cx], pr) || crun < n) return LDM_ERR_MISSING;
                    if (io(ctx, pl->X[cx].Disk, 0, cOff, n, scratch)) return GPT_ERR_IO;
                    Xor(par, scratch, n);
                }
                if (!ColFind(pl, pcol, coff, &px, &pOff, &prun) || !Usable(&pl->X[px], pr) || prun < n) return LDM_ERR_MISSING;
                if (io(ctx, dx->Disk, 1, dOff, n, buf)) return GPT_ERR_IO;
                if (io(ctx, pl->X[px].Disk, 1, pOff, n, par)) return GPT_ERR_IO;
            }
        } else if (m->Type == LDM_VT_MIRROR) {
            gpt_u64 best = len; int done = 0;
            /* the run that is contiguous in every plex */
            for (p = 0; p < m->NPlex; p++) {
                if (!PlexMap(&m->Plex[p], off, &xi, &dOff, &run, 0, 0)) return GPT_ERR_CORRUPT;
                if (run < best) best = run;
            }
            n = (gpt_u32)best;
            for (p = 0; p < m->NPlex; p++) {
                PlexMap(&m->Plex[p], off, &xi, &dOff, &run, 0, 0);
                if (!Usable(&m->Plex[p].X[xi], pr)) continue;
                if (io(ctx, m->Plex[p].X[xi].Disk, write, dOff, n, buf)) return GPT_ERR_IO;
                done = 1;
                if (!write) break;
            }
            if (!done) return LDM_ERR_MISSING;
        } else {
            const LDM_PLEX *pl = &m->Plex[0];
            if (!PlexMap(pl, off, &xi, &dOff, &run, 0, 0)) return GPT_ERR_CORRUPT;
            if (!Usable(&pl->X[xi], pr)) return LDM_ERR_MISSING;
            n = (run < len) ? (gpt_u32)run : len;
            if (io(ctx, pl->X[xi].Disk, write, dOff, n, buf)) return GPT_ERR_IO;
        }
        off += n; buf += n; len -= n;
    }
    return GPT_OK;
}

/* ---- changing the database ---- */

static gpt_u64 NextId(LDM_DB *db) { return ++db->Committed; }

static int NewRec(LDM_DB *db, gpt_u8 type, gpt_u64 id, const gpt_u8 *data, gpt_u32 len)
{
    LDM_REC *rc;
    if (db->NRecs >= LDM_MAX_RECS) return -1;
    rc = &db->Rec[db->NRecs];
    rc->Data = (gpt_u8 *)GptPlatAlloc(len);
    if (!rc->Data) return -1;
    Copy(rc->Data, data, len);
    rc->Len = len; rc->Type = type; rc->Id = id;
    return (int)db->NRecs++;
}

static void DropRec(LDM_DB *db, int ri)
{
    gpt_u32 i;
    if (ri < 0 || ri >= (int)db->NRecs) return;
    GptPlatFree(db->Rec[ri].Data);
    for (i = (gpt_u32)ri; i + 1 < db->NRecs; i++) db->Rec[i] = db->Rec[i + 1];
    db->NRecs--;
    for (i = 0; i < db->NDisks; i++) if (db->Disk[i].Rec > ri) db->Disk[i].Rec--;
    for (i = 0; i < db->NVols; i++) if (db->Vol[i].Rec > ri) db->Vol[i].Rec--;
    for (i = 0; i < db->NComps; i++) if (db->Comp[i].Rec > ri) db->Comp[i].Rec--;
    for (i = 0; i < db->NParts; i++) if (db->Part[i].Rec > ri) db->Part[i].Rec--;
}

static void Head(WR *w, gpt_u8 flags, gpt_u8 type)
{ WByte(w, 0); WByte(w, 0); WByte(w, flags); WByte(w, type); WFix(w, 0, 4); }
static void SetLen(gpt_u8 *b, gpt_u32 n) { PBe32(b + 4, n - 8); }

static void Num(char *o, const char *pfx, gpt_u32 n, int two)
{
    gpt_u32 k = 0, i; char t[12];
    while (pfx[k]) { o[k] = pfx[k]; k++; }
    i = 0; do { t[i++] = (char)('0' + n % 10); n /= 10; } while (n);
    if (two && i < 2) t[i++] = '0';
    while (i) o[k++] = t[--i];
    o[k] = 0;
}

static int NameUsed(const LDM_DB *db, const char *n)
{
    gpt_u32 i;
    for (i = 0; i < db->NVols; i++) if (Seq(db->Vol[i].Name, n)) return 1;
    for (i = 0; i < db->NParts; i++) if (Seq(db->Part[i].Name, n)) return 1;
    for (i = 0; i < db->NDisks; i++) if (Seq(db->Disk[i].Name, n)) return 1;
    return 0;
}

static gpt_u8 Rec[512];

int LdmNewGroup(LDM_DB *db, const char *name, const gpt_u8 *guid)
{
    WR w; char gs[40]; int ri;
    LdmFreeDb(db);
    Zero(db, sizeof(*db));
    Scopy(db->DgName, name, 32);
    LdmGuidToString(guid, gs); Scopy(db->DgGuid, gs, 40);
    db->VblkSize = 128; db->VblkFirst = 512;
    db->Committed = 0x400;
    db->DgId = NextId(db);
    w.p = Rec; w.n = 0; w.max = sizeof(Rec);
    Head(&w, 0x08, 0x35);
    WVar(&w, db->DgId); WStr(&w, db->DgName); WStr(&w, db->DgGuid);
    WFix(&w, 0, 4); WFix(&w, db->DgId, 8);
    WByte(&w, 4); WFix(&w, 0xFFFFFFFFu, 4);    /* config copies: all */
    WByte(&w, 4); WFix(&w, 0xFFFFFFFFu, 4);    /* log copies: all */
    SetLen(Rec, w.n);
    ri = NewRec(db, 0x35, db->DgId, Rec, w.n);
    return ri < 0 ? LDM_ERR_FULL : GPT_OK;
}

int LdmAddDisk(LDM_DB *db, const gpt_u8 *guid, int *idx)
{
    WR w; char name[16], gs[40]; gpt_u32 n = 1; LDM_DISK *d; int ri; gpt_u64 id;
    if (db->NDisks >= LDM_MAX_DISKS) return LDM_ERR_FULL;
    do Num(name, "Disk", n++, 0); while (NameUsed(db, name));
    id = NextId(db);
    LdmGuidToString(guid, gs);
    w.p = Rec; w.n = 0; w.max = sizeof(Rec);
    Head(&w, 0x00, 0x34);
    WVar(&w, id); WStr(&w, name); WStr(&w, gs); WStr(&w, "");
    WFix(&w, 0, 4); WFix(&w, id, 8);
    SetLen(Rec, w.n);
    ri = NewRec(db, 0x34, id, Rec, w.n);
    if (ri < 0) return LDM_ERR_FULL;
    d = &db->Disk[db->NDisks];
    Zero(d, sizeof(*d)); d->Id = id; Scopy(d->Name, name, 32); Copy(d->Guid, guid, 16); d->Rec = ri;
    *idx = (int)db->NDisks++;
    return GPT_OK;
}

int LdmRemoveDisk(LDM_DB *db, int di)
{
    gpt_u32 i;
    for (i = 0; i < db->NParts; i++) if (db->Part[i].DiskId == db->Disk[di].Id) return LDM_ERR_INUSE;
    DropRec(db, db->Disk[di].Rec);
    for (i = (gpt_u32)di; i + 1 < db->NDisks; i++) db->Disk[i] = db->Disk[i + 1];
    db->NDisks--;
    db->Committed++;
    return GPT_OK;
}

/* free runs of a disk's public region (512-byte units), sorted */
int LdmFreeRuns(const LDM_DB *db, int di, gpt_u64 pubSize, LDM_FREE *out, int max)
{
    gpt_u64 pos = 0; int n = 0;
    for (;;) {
        gpt_u64 next = pubSize, nextEnd = 0; gpt_u32 i; int any = 0;
        for (i = 0; i < db->NParts; i++) {
            const LDM_PART *p = &db->Part[i];
            if (p->DiskId != db->Disk[di].Id || p->Start + p->Size <= pos) continue;
            if (p->Start <= pos) { pos = p->Start + p->Size; any = 1; break; }
            if (p->Start < next) { next = p->Start; nextEnd = p->Start + p->Size; }
        }
        if (any) continue;
        if (next > pos && n < max) { out[n].Start = pos; out[n].Size = next - pos; n++; }
        if (next >= pubSize) break;
        pos = nextEnd;
    }
    return n;
}

static int FindSpace(const LDM_DB *db, int di, gpt_u64 pubSize, gpt_u64 size, gpt_u64 *start)
{
    LDM_FREE r[64]; int n = LdmFreeRuns(db, di, pubSize, r, 64), i;
    for (i = 0; i < n; i++) if (r[i].Size >= size) { *start = r[i].Start; return 1; }
    return 0;
}

static int SpaceFree(const LDM_DB *db, int di, gpt_u64 pubSize, gpt_u64 st, gpt_u64 size)
{
    gpt_u32 i;
    if (st + size > pubSize) return 0;
    for (i = 0; i < db->NParts; i++) {
        const LDM_PART *p = &db->Part[i];
        if (p->DiskId == db->Disk[di].Id && st < p->Start + p->Size && p->Start < st + size) return 0;
    }
    return 1;
}

int LdmAddVolume(LDM_DB *db, const LDM_NEWVOL *s, const gpt_u64 *pubSize, int *volIdx)
{
    gpt_u32 nd = s->NDisks, i, k, plexes, ncomp, partsPerComp, vnum;
    gpt_u64 stripe = s->Stripe ? s->Stripe : 128, volSize = 0, start[LDM_MAX_XPP], vid, cid[LDM_MAX_PLEX];
    char vname[16], cname[24], pname[24]; const char *pfx; WR w; int ri;
    LDM_VOL *v;

    if (nd == 0 || nd > LDM_MAX_XPP) return GPT_ERR_PARAM;
    switch (s->Type) {
    case LDM_VT_SIMPLE:  if (nd != 1) return GPT_ERR_PARAM; break;
    case LDM_VT_SPANNED: if (nd < 2) return GPT_ERR_PARAM; break;
    case LDM_VT_STRIPED: if (nd < 2) return GPT_ERR_PARAM; break;
    case LDM_VT_MIRROR:  if (nd < 2 || nd > LDM_MAX_PLEX) return GPT_ERR_PARAM; break;
    case LDM_VT_RAID5:   if (nd < 3) return GPT_ERR_PARAM; break;
    default: return GPT_ERR_PARAM;
    }
    if (db->NVols >= LDM_MAX_VOLS || db->NParts + nd > LDM_MAX_PARTS || db->NComps + nd > LDM_MAX_COMPS ||
        db->NRecs + 2 * nd + 2 > LDM_MAX_RECS) return LDM_ERR_FULL;
    for (i = 0; i < nd; i++) {
        gpt_u64 sz = s->Size[i];
        if (s->Disk[i] >= db->NDisks || sz == 0) return GPT_ERR_PARAM;
        for (k = 0; k < i; k++) if (s->Disk[k] == s->Disk[i] && s->Type != LDM_VT_SPANNED) return GPT_ERR_PARAM;
        if (s->Type == LDM_VT_STRIPED || s->Type == LDM_VT_RAID5) {
            if (sz != s->Size[0]) return GPT_ERR_PARAM;
            sz = Div64(sz, (gpt_u32)stripe, 0) * stripe;
            if (!sz) return GPT_ERR_PARAM;
        }
        if (s->Type == LDM_VT_MIRROR && sz != s->Size[0]) return GPT_ERR_PARAM;
        if (s->Fixed) {
            if (!SpaceFree(db, (int)s->Disk[i], pubSize[s->Disk[i]], s->Start[i], sz)) return LDM_ERR_SPACE;
            start[i] = s->Start[i];
        } else {
            /* first fit; for a spanned volume earlier pieces on the same
               disk are already counted as used below */
            if (!FindSpace(db, (int)s->Disk[i], pubSize[s->Disk[i]], sz, &start[i])) return LDM_ERR_SPACE;
            for (k = 0; k < i; k++)
                if (s->Disk[k] == s->Disk[i] && start[k] == start[i]) return LDM_ERR_SPACE;  /* spans: one piece per disk */
        }
        switch (s->Type) {
        case LDM_VT_SIMPLE: case LDM_VT_SPANNED: case LDM_VT_STRIPED: volSize += sz; break;
        case LDM_VT_MIRROR: volSize = sz; break;
        case LDM_VT_RAID5: volSize = sz * (nd - 1); break;
        }
        ((LDM_NEWVOL *)s)->Size[i] = sz;       /* rounded */
    }

    /* names: Volume<n> (simple, spanned, mirror), Stripe<n>, Raid<n> */
    pfx = (s->Type == LDM_VT_STRIPED) ? "Stripe" : (s->Type == LDM_VT_RAID5) ? "Raid" : "Volume";
    k = 1; do Num(vname, pfx, k++, 0); while (NameUsed(db, vname));
    for (vnum = 5;; vnum++) { for (i = 0; i < db->NVols; i++) if (db->Vol[i].Number == vnum) break; if (i == db->NVols) break; }

    plexes = (s->Type == LDM_VT_MIRROR) ? nd : 1;
    ncomp = plexes;
    partsPerComp = (s->Type == LDM_VT_MIRROR) ? 1 : nd;
    vid = NextId(db);
    for (k = 0; k < ncomp; k++) cid[k] = NextId(db);

    /* volume */
    w.p = Rec; w.n = 0; w.max = sizeof(Rec);
    Head(&w, s->Hint[0] ? 0x02 : 0x00, 0x51);
    WVar(&w, vid); WStr(&w, vname); WStr(&w, s->Type == LDM_VT_RAID5 ? "raid5" : "gen"); WStr(&w, "");
    { char st[14]; Zero(st, 14); Copy(st, "ACTIVE", 6); WRaw(&w, (const gpt_u8 *)st, 14); }
    WByte(&w, s->Type == LDM_VT_RAID5 ? 4 : 3);
    WVar(&w, vnum);
    WFix(&w, 0x11u | (s->Retain ? 0x800000u : 0), 4);
    WVar(&w, ncomp);
    WFix(&w, db->Committed + 1, 8); WFix(&w, vid, 8);
    WPad(&w, volSize);
    WFix(&w, 0, 4);
    WByte(&w, s->PartType ? s->PartType : 0x07);
    WRaw(&w, s->Guid, 16);
    if (s->Hint[0]) WStr(&w, s->Hint);
    SetLen(Rec, w.n);
    ri = NewRec(db, 0x51, vid, Rec, w.n);
    if (ri < 0) return LDM_ERR_FULL;
    v = &db->Vol[db->NVols];
    Zero(v, sizeof(*v));
    v->Id = vid; Scopy(v->Name, vname, 32); v->IsRaid5 = (s->Type == LDM_VT_RAID5); v->Type = s->Type;
    v->NComps = ncomp; v->Number = vnum; v->Flags = 0x11u | (s->Retain ? 0x800000u : 0);
    v->Size = volSize; v->PartType = s->PartType ? s->PartType : 0x07; Copy(v->Guid, s->Guid, 16); v->Rec = ri;
    Scopy(v->Hint, s->Hint, 8);
    *volIdx = (int)db->NVols++;

    /* components */
    for (k = 0; k < ncomp; k++) {
        LDM_COMP *c = &db->Comp[db->NComps];
        gpt_u8 lay = (s->Type == LDM_VT_STRIPED) ? LDM_CL_STRIPE : (s->Type == LDM_VT_RAID5) ? LDM_CL_RAID5 : LDM_CL_CONCAT;
        Num(cname, vname, 0, 0); cname[Slen(vname)] = 0;
        { char t[8]; Num(t, "-", k + 1, 1); i = Slen(cname); Scopy(cname + i, t, 24 - i); }
        w.p = Rec; w.n = 0; w.max = sizeof(Rec);
        Head(&w, (lay == LDM_CL_CONCAT) ? 0x00 : 0x10, 0x32);
        WVar(&w, cid[k]); WStr(&w, cname); WStr(&w, "ACTIVE");
        WByte(&w, lay); WFix(&w, 0, 4);
        WVar(&w, partsPerComp);
        WFix(&w, db->Committed + 1, 8); WFix(&w, 0, 8);
        WVar(&w, vid); WVar(&w, 0);
        if (lay != LDM_CL_CONCAT) { WPad(&w, stripe); WVar(&w, nd); }
        SetLen(Rec, w.n);
        ri = NewRec(db, 0x32, cid[k], Rec, w.n);
        if (ri < 0) return LDM_ERR_FULL;
        Zero(c, sizeof(*c));
        c->Id = cid[k]; c->VolId = vid; Scopy(c->Name, cname, 32); c->Layout = lay; c->NParts = partsPerComp;
        c->Stripe = (lay == LDM_CL_CONCAT) ? 0 : stripe; c->Columns = (lay == LDM_CL_CONCAT) ? 0 : nd; c->Rec = ri;
        db->NComps++;
    }

    /* partitions (extents) */
    {
        gpt_u64 coff = 0;
        for (i = 0; i < nd; i++) {
            LDM_PART *p = &db->Part[db->NParts];
            LDM_DISK *d = &db->Disk[s->Disk[i]];
            gpt_u64 pid = NextId(db), comp = (s->Type == LDM_VT_MIRROR) ? cid[i] : cid[0];
            gpt_u32 col = (s->Type == LDM_VT_STRIPED || s->Type == LDM_VT_RAID5) ? i : 0;
            gpt_u64 off = (s->Type == LDM_VT_SPANNED || s->Type == LDM_VT_SIMPLE) ? coff : 0;
            k = 1; do { Num(pname, d->Name, 0, 0); pname[Slen(d->Name)] = 0;
                        { char t[8]; Num(t, "-", k++, 1); gpt_u32 l = Slen(pname); Scopy(pname + l, t, 24 - l); } }
            while (NameUsed(db, pname));
            w.p = Rec; w.n = 0; w.max = sizeof(Rec);
            Head(&w, col ? 0x08 : 0x00, 0x33);
            WVar(&w, pid); WStr(&w, pname); WFix(&w, 0, 4);
            WFix(&w, db->Committed + 1, 8);
            WFix(&w, start[i], 8); WFix(&w, off, 8);
            WPad(&w, s->Size[i]);
            WVar(&w, comp); WVar(&w, d->Id);
            if (col) WVar(&w, col);
            SetLen(Rec, w.n);
            ri = NewRec(db, 0x33, pid, Rec, w.n);
            if (ri < 0) return LDM_ERR_FULL;
            Zero(p, sizeof(*p));
            p->Id = pid; p->CompId = comp; p->DiskId = d->Id; Scopy(p->Name, pname, 32);
            p->Start = start[i]; p->ColOff = off; p->Size = s->Size[i]; p->Column = col; p->Rec = ri;
            db->NParts++;
            coff += s->Size[i];
        }
    }
    db->Committed++;
    return GPT_OK;
}

int LdmDeleteVolume(LDM_DB *db, int vi)
{
    gpt_u64 vid = db->Vol[vi].Id; gpt_u32 i, c;
    for (c = 0; c < db->NComps;) {
        if (db->Comp[c].VolId != vid) { c++; continue; }
        for (i = 0; i < db->NParts;) {
            if (db->Part[i].CompId != db->Comp[c].Id) { i++; continue; }
            DropRec(db, db->Part[i].Rec);
            { gpt_u32 k; for (k = i; k + 1 < db->NParts; k++) db->Part[k] = db->Part[k + 1]; db->NParts--; }
        }
        DropRec(db, db->Comp[c].Rec);
        { gpt_u32 k; for (k = c; k + 1 < db->NComps; k++) db->Comp[k] = db->Comp[k + 1]; db->NComps--; }
    }
    DropRec(db, db->Vol[vi].Rec);
    for (i = (gpt_u32)vi; i + 1 < db->NVols; i++) db->Vol[i] = db->Vol[i + 1];
    db->NVols--;
    db->Committed++;
    return GPT_OK;
}

/* Rewrite the volume record with a new partition type byte. The byte sits
   right before the 16-byte GUID, which precedes the optional fields. */
int LdmSetVolumeType(LDM_DB *db, int vi, gpt_u8 t)
{
    LDM_REC *rc = &db->Rec[db->Vol[vi].Rec]; RD r; gpt_u32 flags = rc->Data[2];
    r.p = rc->Data + 8; r.end = rc->Data + rc->Len; r.bad = 0;
    RVar(&r); RStr(&r, 0, 0); RStr(&r, 0, 0); RStr(&r, 0, 0); RSkip(&r, 15);
    RVar(&r); RSkip(&r, 4); RVar(&r); RSkip(&r, 16); RVar(&r); RSkip(&r, 4);
    if (r.bad || r.p >= r.end) return GPT_ERR_CORRUPT;
    *(gpt_u8 *)r.p = t;
    db->Vol[vi].PartType = t;
    (void)flags;
    db->Committed++;
    return GPT_OK;
}

/* Rewrite the volume record with a new drive letter hint: the fields up to
   the optional ones are kept, the hint (the last optional field) replaced. */
int LdmSetVolumeHint(LDM_DB *db, int vi, const char *hint)
{
    LDM_REC *rc = &db->Rec[db->Vol[vi].Rec]; RD r; gpt_u32 flags = rc->Data[2], keep; WR w; gpt_u8 *nd;
    r.p = rc->Data + 8; r.end = rc->Data + rc->Len; r.bad = 0;
    RVar(&r); RStr(&r, 0, 0); RStr(&r, 0, 0); RStr(&r, 0, 0); RSkip(&r, 15);
    RVar(&r); RSkip(&r, 4); RVar(&r); RSkip(&r, 16); RVar(&r); RSkip(&r, 4); RSkip(&r, 1); RSkip(&r, 16);
    if (flags & 0x08) RVar(&r);
    if (flags & 0x20) RVar(&r);
    if (flags & 0x80) RVar(&r);
    if (r.bad) return GPT_ERR_CORRUPT;
    keep = (gpt_u32)(r.p - rc->Data);
    if (keep > sizeof(Rec) - 16) return LDM_ERR_FULL;
    Copy(Rec, rc->Data, keep);
    w.p = Rec; w.n = keep; w.max = sizeof(Rec);
    if (hint && hint[0]) { WStr(&w, hint); Rec[2] = (gpt_u8)(flags | 0x02); }
    else Rec[2] = (gpt_u8)(flags & ~0x02u);
    SetLen(Rec, w.n);
    nd = (gpt_u8 *)GptPlatAlloc(w.n);
    if (!nd) return LDM_ERR_FULL;
    Copy(nd, Rec, w.n);
    GptPlatFree(rc->Data);
    rc->Data = nd; rc->Len = w.n;
    Scopy(db->Vol[vi].Hint, hint ? hint : "", 8);
    db->Committed++;
    return GPT_OK;
}

/* ---- writing a disk's copy ---- */

static void PutStrField(gpt_u8 *p, const char *s, gpt_u32 n) { Zero(p, n); Copy(p, s, Slen(s) < n ? Slen(s) : n); }

void LdmMakeHeader(LDM_HDR *h, int isGpt, gpt_u32 ss, gpt_u64 pubStart, gpt_u64 pubSize,
                   gpt_u64 privStart, gpt_u64 privSize, const gpt_u8 *diskGuid, const LDM_DB *db)
{
    Zero(h, sizeof(*h));
    h->IsGpt = isGpt; h->SectorSize = ss;
    h->PubStart = pubStart; h->PubSize = pubSize;
    h->PrivStart = privStart; h->PrivSize = privSize;
    h->PhPrim = privSize - 1; h->PhSec = privSize - 192;
    h->Toc1 = 1; h->Toc2 = privSize - 2;
    h->ConfStart = 17; h->ConfSize = 1481;
    h->LogStart = 1498; h->LogSize = 224;
    h->Usn = 0;
    LdmGuidToString(diskGuid, h->DiskGuid);
    Scopy(h->HostGuid, HostGuid, 40);
    Scopy(h->DgGuid, db->DgGuid, 40);
    Scopy(h->DgName, db->DgName, 32);
}

static void BuildPh(const LDM_HDR *h, gpt_u64 usn, gpt_u64 now, gpt_u32 sig, gpt_u8 *p)
{
    Zero(p, 512);
    Copy(p, "PRIVHEAD", 8);
    PBe16(p + 0x0C, 2); PBe16(p + 0x0E, 11);
    PBe64(p + 0x10, now);
    PBe64(p + 0x18, usn);
    PBe64(p + 0x20, h->PhPrim); PBe64(p + 0x28, h->PhSec);
    PutStrField(p + 0x30, h->DiskGuid, 64);
    PutStrField(p + 0x70, h->HostGuid[0] ? h->HostGuid : HostGuid, 64);
    PutStrField(p + 0xB0, h->DgGuid, 64);
    PutStrField(p + 0xF0, h->DgName, 31);
    PBe32(p + 0x10F, 512);
    PBe64(p + 0x11B, h->PubStart); PBe64(p + 0x123, h->PubSize);
    PBe64(p + 0x12B, h->PrivStart); PBe64(p + 0x133, h->PrivSize);
    PBe64(p + 0x13B, h->Toc1); PBe64(p + 0x143, h->Toc2);
    PBe32(p + 0x14B, 1); PBe32(p + 0x14F, 1);
    PBe64(p + 0x153, h->ConfSize); PBe64(p + 0x15B, h->LogSize);
    PBe32(p + 0x163, sig);
    PBe32(p + 8, Checksum(p));
}

static void BuildToc(const LDM_HDR *h, gpt_u64 usn, gpt_u8 *t)
{
    gpt_u8 *e;
    Zero(t, 512);
    Copy(t, "TOCBLOCK", 8);
    PBe64(t + 0x0C, usn);
    e = t + 0x24;
    Copy(e, "config", 6); PBe64(e + 10, h->ConfStart); PBe64(e + 18, h->ConfSize); e[27] = 6; e[29] = 1;
    e = t + 0x24 + 34;
    Copy(e, "log", 3);    PBe64(e + 10, h->LogStart);  PBe64(e + 18, h->LogSize);  e[27] = 6; e[29] = 1;
    PBe32(t + 8, Checksum(t));
}

static int W1(GptWriteProc Write, void *Ctx, gpt_u32 ss, gpt_u64 lba, const gpt_u8 *s512, gpt_u8 *tmp)
{
    /* a 512-byte structure in its own sector, rest zero */
    Zero(tmp, ss); Copy(tmp, s512, 512);
    return Write(Ctx, lba, 1, tmp);
}

int LdmWriteDisk(GptWriteProc Write, void *Ctx, const LDM_HDR *h, const LDM_DB *db, gpt_u64 now)
{
    gpt_u32 ss = h->SectorSize, vs = db->VblkSize ? db->VblkSize : 128, slot, chunk = 64 * 1024;
    gpt_u32 first = db->VblkFirst ? db->VblkFirst : 512, data = vs - 16, i;
    gpt_u64 total = h->ConfSize * ss, pos, usn = h->Usn + 1;
    gpt_u32 nvol = 0, ncomp = 0, npart = 0, ndisk = 0, totalSlots = (gpt_u32)Div64(total, vs, 0);
    gpt_u8 *buf, *v; int rc = GPT_OK;
    /* record r occupies slots [base[r], base[r]+nfrag[r]) */
    gpt_u32 *base, *nfr;

    if ((ss != 512 && ss != 4096) || (chunk % ss) || (chunk % vs)) return GPT_ERR_PARAM;
    buf = (gpt_u8 *)GptPlatAlloc(chunk + 8 * LDM_MAX_RECS);
    if (!buf) return GPT_ERR_IO;
    base = (gpt_u32 *)(buf + chunk); nfr = base + LDM_MAX_RECS;

    slot = first / vs;
    for (i = 0; i < db->NRecs; i++) {
        nfr[i] = (db->Rec[i].Len + data - 1) / data;
        base[i] = slot; slot += nfr[i];
        switch (db->Rec[i].Type & 15) { case 1: nvol++; break; case 2: ncomp++; break; case 3: npart++; break; case 4: ndisk++; break; }
    }
    if (slot > totalSlots) { GptPlatFree(buf); return LDM_ERR_FULL; }

    /* config region: VMDB + VBLKs, in 64 KB pieces */
    for (pos = 0; pos < total && rc == GPT_OK; pos += chunk) {
        gpt_u32 n = (total - pos < chunk) ? (gpt_u32)(total - pos) : chunk, off;
        Zero(buf, n);
        for (off = 0; off < n; off += vs) {
            gpt_u64 abs = pos + off; gpt_u32 s = (gpt_u32)Div64(abs, vs, 0), r;
            gpt_u8 *b = buf + off;
            if (abs == 0) {
                v = b;
                if (db->Vmdb[0] == 'V') Copy(v, db->Vmdb, 512 < n ? 512 : n);
                else {
                    Copy(v, "VMDB", 4);
                    PBe16(v + 0x12, 4); PBe16(v + 0x14, 10);
                }
                PBe32(v + 4, totalSlots); PBe32(v + 8, vs); PBe32(v + 12, first);
                PBe16(v + 0x10, 1);
                Zero(v + 0x16, 31); Copy(v + 0x16, db->DgName, Slen(db->DgName) < 31 ? Slen(db->DgName) : 31);
                Zero(v + 0x35, 64); Copy(v + 0x35, db->DgGuid, Slen(db->DgGuid));
                PBe64(v + 0x75, db->Committed); PBe64(v + 0x7D, db->Committed);
                PBe32(v + 0x85, nvol); PBe32(v + 0x89, ncomp); PBe32(v + 0x8D, npart); PBe32(v + 0x91, ndisk);
                PBe32(v + 0x95, 0); PBe32(v + 0x99, 0); PBe32(v + 0x9D, 0);
                PBe32(v + 0xA1, nvol); PBe32(v + 0xA5, ncomp); PBe32(v + 0xA9, npart); PBe32(v + 0xAD, ndisk);
                PBe32(v + 0xB1, 0); PBe32(v + 0xB5, 0); PBe32(v + 0xB9, 0);
                PBe64(v + 0xBD, now);
                continue;
            }
            if (abs < first) continue;
            for (r = 0; r < db->NRecs; r++) if (s >= base[r] && s < base[r] + nfr[r]) break;
            if (r == db->NRecs) {
                /* an empty slot still carries its header (Windows rejects
                   a configuration copy with blank slots) */
                Copy(b, "VBLK", 4); PBe32(b + 4, s);
                continue;
            }
            {
                gpt_u32 fr = s - base[r], at = fr * data, cnt = db->Rec[r].Len - at;
                if (cnt > data) cnt = data;
                Copy(b, "VBLK", 4);
                PBe32(b + 4, s); PBe32(b + 8, r + 1);
                PBe16(b + 12, fr); PBe16(b + 14, nfr[r]);
                Copy(b + 16, db->Rec[r].Data + at, cnt);
            }
        }
        if (Write(Ctx, h->PrivStart + h->ConfStart + Div64(pos, ss, 0), n / ss, buf)) rc = GPT_ERR_IO;
    }

    /* log region: two KLOG pages, one commit entry */
    if (rc == GPT_OK) {
        gpt_u64 lpos, ltotal = h->LogSize * ss;
        for (lpos = 0; lpos < ltotal && rc == GPT_OK; lpos += chunk) {
            gpt_u32 n = (ltotal - lpos < chunk) ? (gpt_u32)(ltotal - lpos) : chunk, pg;
            Zero(buf, n);
            for (pg = 0; pg < 2; pg++) {
                gpt_u64 at = (gpt_u64)pg * 512;
                gpt_u8 *k;
                if (at < lpos || at >= lpos + n) continue;
                k = buf + (gpt_u32)(at - lpos);
                Copy(k, "KLOG", 4);
                PBe64(k + 4, db->DgId + 3);
                PBe32(k + 0x0C, 0); PBe32(k + 0x10, 2); PBe32(k + 0x14, pg);
                if (pg == 0) { k[0x18] = 3; PBe64(k + 0x19, db->Committed); PBe64(k + 0x21, db->Committed); }
            }
            if (Write(Ctx, h->PrivStart + h->LogStart + Div64(lpos, ss, 0), n / ss, buf)) rc = GPT_ERR_IO;
        }
    }

    /* tables of contents, then the private headers */
    if (rc == GPT_OK) {
        gpt_u8 st[512]; gpt_u32 sig = 0;
        BuildToc(h, usn, st);
        if (W1(Write, Ctx, ss, h->PrivStart + 1, st, buf) || W1(Write, Ctx, ss, h->PrivStart + 2, st, buf) ||
            W1(Write, Ctx, ss, h->PrivStart + h->PrivSize - 3, st, buf) ||
            W1(Write, Ctx, ss, h->PrivStart + h->PrivSize - 2, st, buf)) rc = GPT_ERR_IO;
        if (h->Ph[0] == 'P') sig = Be32(h->Ph + 0x163);
        if (rc == GPT_OK) {
            LDM_HDR t = *h; t.Toc1 = 1; t.Toc2 = h->PrivSize - 2;
            BuildPh(&t, usn, now, sig, st);
            if (W1(Write, Ctx, ss, h->PrivStart + h->PhSec, st, buf) ||
                W1(Write, Ctx, ss, h->PrivStart + h->PhPrim, st, buf)) rc = GPT_ERR_IO;
            if (rc == GPT_OK && !h->IsGpt && W1(Write, Ctx, ss, 3072 / ss, st, buf)) rc = GPT_ERR_IO;
        }
    }
    GptPlatFree(buf);
    (void)Le32;
    return rc;
}

int LdmWipeHeader(GptWriteProc Write, void *Ctx, const LDM_HDR *h)
{
    gpt_u8 *z = (gpt_u8 *)GptPlatAlloc(4096); int rc = GPT_OK; gpt_u32 k;
    gpt_u64 l[7];
    if (!z) return GPT_ERR_IO;
    Zero(z, 4096);
    l[0] = h->PrivStart + h->PhPrim; l[1] = h->PrivStart + h->PhSec;
    l[2] = h->PrivStart + 1; l[3] = h->PrivStart + 2;
    l[4] = h->PrivStart + h->PrivSize - 3; l[5] = h->PrivStart + h->PrivSize - 2;
    l[6] = h->PrivStart + h->ConfStart;
    for (k = 0; k < 7; k++) if (Write(Ctx, l[k], 1, z)) rc = GPT_ERR_IO;
    if (!h->IsGpt && Write(Ctx, 3072 / h->SectorSize, 1, z)) rc = GPT_ERR_IO;
    GptPlatFree(z);
    return rc;
}
