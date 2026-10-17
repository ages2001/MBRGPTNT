#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gptcore.h"
void *GptPlatAlloc(gpt_u32 n){return malloc(n);} void GptPlatFree(void*p){free(p);}
static FILE*f;
static int rd(void*c,gpt_u64 l,gpt_u32 n,void*b){(void)c;fseeko(f,(off_t)l*512,SEEK_SET);if(fread(b,512,n,f)!=n)memset(b,0,n*512);return 0;}
static int wr(void*c,gpt_u64 l,gpt_u32 n,const void*b){(void)c;fseeko(f,(off_t)l*512,SEEK_SET);return fwrite(b,512,n,f)!=n;}
static void add(GPT_DISK*d,gpt_u64 s,gpt_u64 n,gpt_u32 fam){gpt_u32 i;
  if(GptAddPartition(d,s,n,fam==GPT_FAMILY_EXTENDED?5:6,0,&i)!=GPT_OK){printf("add fail %llu\n",(unsigned long long)s);return;}
  GptSetType(d,i,GptPickType(d,fam,d->Part[i].StartLba,d->Part[i].SectorCount));
  printf("  part start=%llu size=%lluMB fam=%x -> type %02X\n",(unsigned long long)d->Part[i].StartLba,(unsigned long long)(d->Part[i].SectorCount/2048),fam,d->Part[i].MbrType);}
/* usage: tstchs <20 GB image> - picks types and writes real CHS */
int main(int argc,char**argv){GPT_DISK d; gpt_u64 sec=20ULL*1024*1024*2; f=fopen(argv[1],"r+b");
  (void)argc; GptInitDisk(&d,GPT_SCHEME_MBR,512,sec);
  printf("CHS limit=%llu\n",(unsigned long long)GptChsLimit(&d));
  add(&d,2048,20*2048,GPT_FAMILY_FAT16);            /* 20MB  -> 04 */
  add(&d,0,100*2048,GPT_FAMILY_FAT16);              /* 100MB -> 06 */
  add(&d,9ULL*1024*1024*2,500*2048,GPT_FAMILY_FAT32); /* at 9GB -> 0C */
  add(&d,0,0,GPT_FAMILY_EXTENDED);                  /* fills from ~120MB to end -> 0F (or 05) */
  add(&d,0,50*2048,GPT_FAMILY_FAT16);               /* logical inside extended */
  add(&d,0,300*2048,GPT_FAMILY_FAT32);              /* logical FAT32 */
  if(GptWriteDisk(rd,wr,NULL,&d)){printf("write fail\n");return 1;}
  fflush(f); {GPT_DISK r; GptParse(rd,NULL,512,sec,&r); printf("reparsed %u:",r.Count); for(unsigned i=0;i<r.Count;i++)printf(" %02X%s",r.Part[i].MbrType,r.Part[i].IsLogical?"L":""); printf("\n");}
  return 0;}
