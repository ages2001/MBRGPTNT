/* tstbig.c - disks past 2 TiB: tstbig IMAGE SECTORSIZE [init]
   Parses the image twice - once with its full size, once as a disk driver
   limited to 32-bit sector numbers sees it (2^32 sectors) - and checks
   which partitions and tables are reachable; then adds a partition at the
   end of the full disk, writes the table and parses it again. With "init"
   a new GPT with a 1 TiB partition is written first. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gptcore.h"
void *GptPlatAlloc(gpt_u32 n){return malloc(n);}
void  GptPlatFree(void *p){free(p);}
struct ctx{FILE*f;gpt_u32 ss;gpt_u64 limit;};
static int rd(void*c,gpt_u64 lba,gpt_u32 cnt,void*buf){struct ctx*x=c;if(lba+cnt>x->limit)return 1;if(fseeko(x->f,(off_t)(lba*x->ss),SEEK_SET))return 1;if(fread(buf,x->ss,cnt,x->f)!=cnt)memset(buf,0,(size_t)cnt*x->ss);return 0;}
static int wr(void*c,gpt_u64 lba,gpt_u32 cnt,const void*buf){struct ctx*x=c;if(lba+cnt>x->limit)return 1;if(fseeko(x->f,(off_t)(lba*x->ss),SEEK_SET))return 1;return fwrite(buf,x->ss,cnt,x->f)!=cnt;}
static void dump(const char*t,GPT_DISK*d){gpt_u32 i;
    printf("%s: %s sectors=%llu table=%llu beyondReach=%d\n",t,d->IsGpt?"GPT":"MBR",(unsigned long long)d->DiskSectors,(unsigned long long)d->TableSectors,GptTableBeyondReach(d));
    for(i=0;i<d->Count;i++)printf("  [%u] start=%llu count=%llu (%llu GiB) beyond=%d\n",i,(unsigned long long)d->Part[i].StartLba,(unsigned long long)d->Part[i].SectorCount,
        (unsigned long long)(d->Part[i].SectorCount*d->SectorSize>>30),GptPartBeyondReach(d,&d->Part[i]));}
int main(int argc,char**argv){
    struct ctx c; GPT_DISK *d=malloc(sizeof(GPT_DISK)); gpt_u64 n; gpt_u32 idx; int rc;
    if(argc<3)return 2;
    c.f=fopen(argv[1],"r+b"); c.ss=(gpt_u32)atoi(argv[2]); fseeko(c.f,0,SEEK_END); n=(gpt_u64)ftello(c.f)/c.ss;
    c.limit=n;
    if(argc>3&&!strcmp(argv[3],"init")){GptSeed(1);GptInitDisk(d,GPT_SCHEME_GPT,c.ss,n);
        GptAddPartition(d,0,((gpt_u64)1<<40)/c.ss,0x07,0,&idx);printf("init write rc=%d\n",GptWriteDisk(rd,wr,&c,d));}
    rc=GptParse(rd,&c,c.ss,n,d); printf("full parse rc=%d\n",rc); dump("full",d);
    c.limit=(gpt_u64)1<<32; if(c.limit>n)c.limit=n;
    rc=GptParse(rd,&c,c.ss,c.limit,d); printf("32-bit parse rc=%d\n",rc); dump("32-bit",d);
    printf("32-bit write (must be refused): rc=%d\n",GptWriteDisk(rd,wr,&c,d));
    c.limit=n; rc=GptParse(rd,&c,c.ss,n,d);
    rc=GptAddPartition(d,0,0,0x07,0,&idx); printf("add at end rc=%d idx=%u\n",rc,idx);
    rc=GptWriteDisk(rd,wr,&c,d); printf("full write rc=%d\n",rc);
    rc=GptParse(rd,&c,c.ss,n,d); printf("reparse rc=%d\n",rc); dump("after",d);
    fclose(c.f); return 0;
}
