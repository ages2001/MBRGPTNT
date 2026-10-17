/* tstconv.c - round-trip test for lossless GPT<->MBR conversion. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gptcore.h"
void *GptPlatAlloc(gpt_u32 n){return malloc(n);}
void  GptPlatFree(void *p){free(p);}
struct ctx{FILE*f;gpt_u32 ss;};
static int rd(void*c,gpt_u64 lba,gpt_u32 cnt,void*buf){struct ctx*x=c;if(fseeko(x->f,(off_t)lba*x->ss,SEEK_SET))return 1;if(fread(buf,x->ss,cnt,x->f)!=cnt)memset(buf,0,(size_t)cnt*x->ss);return 0;}
static int wr(void*c,gpt_u64 lba,gpt_u32 cnt,const void*buf){struct ctx*x=c;if(fseeko(x->f,(off_t)lba*x->ss,SEEK_SET))return 1;return fwrite(buf,x->ss,cnt,x->f)!=cnt;}
static void dump(const char*t,GPT_DISK*d){gpt_u32 i;printf("%s: %s count=%u\n",t,d->IsGpt?"GPT":"MBR",d->Count);for(i=0;i<d->Count;i++)printf("  [%u] start=%llu count=%llu type=0x%02X boot=%u log=%u\n",i,(unsigned long long)d->Part[i].StartLba,(unsigned long long)d->Part[i].SectorCount,d->Part[i].MbrType,d->Part[i].BootIndicator,d->Part[i].IsLogical);}

/* collect non-extended data partitions into start/len pairs, sorted */
static int collect(GPT_DISK*d,gpt_u64*s,gpt_u64*c){
    int n=0;gpt_u32 i,a,b;
    for(i=0;i<d->Count;i++){gpt_u8 t=d->Part[i].MbrType;if(!d->IsGpt&&(t==0x05||t==0x0F||t==0x85))continue;s[n]=d->Part[i].StartLba;c[n]=d->Part[i].SectorCount;n++;}
    for(a=1;a<(gpt_u32)n;a++){gpt_u64 ss=s[a],cc=c[a];b=a;while(b>0&&s[b-1]>ss){s[b]=s[b-1];c[b]=c[b-1];b--;}s[b]=ss;c[b]=cc;}
    return n;
}
static int samedata(GPT_DISK*a,GPT_DISK*b){
    gpt_u64 sa[160],ca[160],sb[160],cb[160];int na,nb,i;
    na=collect(a,sa,ca);nb=collect(b,sb,cb);
    if(na!=nb){printf("  MISMATCH count %d vs %d\n",na,nb);return 0;}
    for(i=0;i<na;i++)if(sa[i]!=sb[i]||ca[i]!=cb[i]){printf("  MISMATCH [%d] %llu/%llu vs %llu/%llu\n",i,(unsigned long long)sa[i],(unsigned long long)ca[i],(unsigned long long)sb[i],(unsigned long long)cb[i]);return 0;}
    return 1;
}

int main(int argc,char**argv){
    struct ctx c; GPT_DISK d,conv,re; gpt_u32 idx; long long bytes; gpt_u64 sectors; int reason,rc,target;
    if(argc<4){fprintf(stderr,"usage: tstconv img ss gpt|mbr(build) \n");return 2;}
    c.ss=(gpt_u32)atoi(argv[2]); c.f=fopen(argv[1],"r+b"); if(!c.f){perror("open");return 2;}
    fseeko(c.f,0,SEEK_END); bytes=(long long)ftello(c.f); sectors=(gpt_u64)(bytes/c.ss);
    GptSeed(0xCAFEBABE);
    GptInitDisk(&d,strcmp(argv[3],"gpt")==0?GPT_SCHEME_GPT:GPT_SCHEME_MBR,c.ss,sectors);
    if(d.IsGpt){
        GptAddPartition(&d,0,20u*1024*1024/c.ss,0x07,0,&idx);
        GptAddPartition(&d,0,30u*1024*1024/c.ss,0x06,1,&idx);
        GptAddPartition(&d,0,25u*1024*1024/c.ss,0x0B,0,&idx);
        target=GPT_SCHEME_MBR;
    } else {
        gpt_u64 es,el;
        GptAddPartition(&d,0,20u*1024*1024/c.ss,0x06,1,&idx);
        GptAddPartition(&d,0,30u*1024*1024/c.ss,0x07,0,&idx);
        es=GptLargestFreeGap(&d,&el);
        GptAddPartition(&d,el,es,0x0F,0,&idx);
        GptAddPartition(&d,el+2048,15u*1024*1024/c.ss,0x06,0,&idx);
        GptAddPartition(&d,el+2048+(20u*1024*1024/c.ss),10u*1024*1024/c.ss,0x0B,0,&idx);
        target=GPT_SCHEME_GPT;
    }
    if(GptWriteDisk(rd,wr,&c,&d)!=GPT_OK){printf("initial WRITE FAILED\n");return 1;}
    fflush(c.f);
    { GPT_DISK src; rc=GptParse(rd,&c,c.ss,sectors,&src); printf("parse src rc=%d\n",rc); dump("src",&src);
      rc=GptConvertScheme(&src,target,&conv,&reason);
      printf("convert -> %s rc=%d reason=%d\n",target==GPT_SCHEME_GPT?"GPT":"MBR",rc,reason);
      if(rc!=GPT_OK){printf("CONVERT REFUSED\n");fclose(c.f);return 1;}
      dump("converted",&conv);
      if(GptWriteDisk(rd,wr,&c,&conv)!=GPT_OK){printf("convert WRITE FAILED\n");return 1;}
      fflush(c.f);
      rc=GptParse(rd,&c,c.ss,sectors,&re); printf("reparse rc=%d\n",rc); dump("reparsed",&re);
      printf("data preserved: %s\n", samedata(&src,&re)?"YES":"NO");
    }
    fclose(c.f); return 0;
}
