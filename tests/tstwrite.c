#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gptcore.h"
void *GptPlatAlloc(gpt_u32 n){return malloc(n);}
void  GptPlatFree(void *p){free(p);}
struct ctx{FILE*f;gpt_u32 ss;};
static int rd(void*c,gpt_u64 lba,gpt_u32 cnt,void*buf){struct ctx*x=c;if(fseeko(x->f,(off_t)lba*x->ss,SEEK_SET))return 1;if(fread(buf,x->ss,cnt,x->f)!=cnt)memset(buf,0,(size_t)cnt*x->ss);return 0;}
static int wr(void*c,gpt_u64 lba,gpt_u32 cnt,const void*buf){struct ctx*x=c;if(fseeko(x->f,(off_t)lba*x->ss,SEEK_SET))return 1;return fwrite(buf,x->ss,cnt,x->f)!=cnt;}
static void dump(const char*t,GPT_DISK*d){gpt_u32 i;printf("%s: %s count=%u ss=%u sectors=%llu\n",t,d->IsGpt?"GPT":"MBR",d->Count,d->SectorSize,(unsigned long long)d->DiskSectors);for(i=0;i<d->Count;i++)printf("  [%u] start=%llu count=%llu type=0x%02X boot=%u log=%u\n",i,(unsigned long long)d->Part[i].StartLba,(unsigned long long)d->Part[i].SectorCount,d->Part[i].MbrType,d->Part[i].BootIndicator,d->Part[i].IsLogical);}
int main(int argc,char**argv){
    struct ctx c; GPT_DISK d; gpt_u32 idx; long long bytes; gpt_u64 sectors;
    if(argc<4){fprintf(stderr,"usage: tstwrite img ss gpt|mbr\n");return 2;}
    c.ss=(gpt_u32)atoi(argv[2]); c.f=fopen(argv[1],"r+b"); if(!c.f){perror("open");return 2;}
    fseeko(c.f,0,SEEK_END); bytes=(long long)ftello(c.f); sectors=(gpt_u64)(bytes/c.ss);
    GptSeed(0xCAFEBABE);
    GptInitDisk(&d,strcmp(argv[3],"gpt")==0?GPT_SCHEME_GPT:GPT_SCHEME_MBR,c.ss,sectors);
    if(d.IsGpt){
        GptAddPartition(&d,0,20u*1024*1024/c.ss,0x07,0,&idx);
        GptAddPartition(&d,0,30u*1024*1024/c.ss,0x06,1,&idx);
        GptAddPartition(&d,0,0,0xEF,0,&idx);
    } else {
        gpt_u64 es,el;
        GptAddPartition(&d,0,20u*1024*1024/c.ss,0x06,1,&idx);
        GptAddPartition(&d,0,30u*1024*1024/c.ss,0x07,0,&idx);
        es=GptLargestFreeGap(&d,&el);
        GptAddPartition(&d,el,es,0x0F,0,&idx);
        GptAddPartition(&d,el+2048,15u*1024*1024/c.ss,0x06,0,&idx);
        GptAddPartition(&d,el+2048+(20u*1024*1024/c.ss),10u*1024*1024/c.ss,0x0B,0,&idx);
    }
    dump("built",&d);
    if(GptWriteDisk(rd,wr,&c,&d)!=GPT_OK){printf("WRITE FAILED\n");return 1;}
    fflush(c.f);
    { GPT_DISK d2; int rc=GptParse(rd,&c,c.ss,sectors,&d2); printf("reparse rc=%d\n",rc); dump("reparsed",&d2); }
    fclose(c.f); return 0;
}
