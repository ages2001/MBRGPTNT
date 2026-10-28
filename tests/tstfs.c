/* tstfs.c - file-system detection on an image: tstfs IMAGE [SECTORSIZE] [raw]
   prints every partition with the file system GptParse found on it; with
   "raw" the image is one volume without a partition table. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gptcore.h"
void *GptPlatAlloc(gpt_u32 n){return malloc(n);}
void  GptPlatFree(void *p){free(p);}
struct ctx{FILE*f;gpt_u32 ss;};
static int rd(void*c,gpt_u64 lba,gpt_u32 cnt,void*buf){struct ctx*x=c;if(fseeko(x->f,(off_t)lba*x->ss,SEEK_SET))return 1;if(fread(buf,x->ss,cnt,x->f)!=cnt)memset(buf,0,(size_t)cnt*x->ss);return 0;}
static const char *names[GPT_FS_KINDS]=GPT_FS_NAME_LIST;
int main(int argc,char**argv){
    struct ctx c; GPT_DISK *d=malloc(sizeof(GPT_DISK)); gpt_u64 n; gpt_u32 i; int rc;
    if(argc<2)return 2;
    c.f=fopen(argv[1],"rb"); if(!c.f)return 2; c.ss=argc>2?(gpt_u32)atoi(argv[2]):512;
    fseeko(c.f,0,SEEK_END); n=(gpt_u64)ftello(c.f)/c.ss;
    if(argc>3&&!strcmp(argv[3],"raw")){gpt_u8 k;int r;static gpt_u8 b[4096];
        gpt_u8 t=GptDetectFs(rd,&c,c.ss,0,n,b,&r,&k);printf("%s type=%02X recog=%d\n",names[k],t,r);return 0;}
    rc=GptParse(rd,&c,c.ss,n,d); if(rc){printf("parse error %d\n",rc);return 1;}
    for(i=0;i<d->Count;i++)printf("%u%s %s type=%02X recog=%u\n",i+1,d->Whole?" (whole disk)":"",names[d->Part[i].FsKind],d->Part[i].MbrType,d->Part[i].Recognized);
    return 0;
}
