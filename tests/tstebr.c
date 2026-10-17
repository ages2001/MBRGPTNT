#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gptcore.h"
void *GptPlatAlloc(gpt_u32 n){return malloc(n);} void GptPlatFree(void*p){free(p);}
static FILE*f; static gpt_u64 SEC=307200;
static int rd(void*c,gpt_u64 l,gpt_u32 n,void*b){(void)c;fseeko(f,(off_t)l*512,SEEK_SET);if(fread(b,512,n,f)!=n)memset(b,0,n*512);return 0;}
static int wr(void*c,gpt_u64 l,gpt_u32 n,const void*b){(void)c;fseeko(f,(off_t)l*512,SEEK_SET);return fwrite(b,512,n,f)!=n;}
static GPT_DISK d;
static void load(void){GptParse(rd,NULL,512,SEC,&d);d.Heads=16;d.SectorsPerTrack=63;}
static void dump(const char*t){unsigned i;load();printf("%s:",t);for(i=0;i<d.Count;i++)printf(" [%llu+%llu %02X%s]",(unsigned long long)d.Part[i].StartLba,(unsigned long long)d.Part[i].SectorCount,d.Part[i].MbrType,d.Part[i].IsLogical?"L":"");printf("\n");}
static void wrd(void){GptWriteDisk(rd,wr,NULL,&d);fflush(f);}
/* usage: tstebr  (reads e.img in the current directory, made by sfdisk:
   2 primaries + an 0F container holding 2 logical drives)
   delete the first logical, create in the gap, create again, delete one -
   the EBR chain must keep every logical drive */
int main(void){gpt_u32 i;f=fopen("e.img","r+b");
 dump("start");
 load(); for(i=0;i<d.Count;i++) if(d.Part[i].IsLogical){GptDeletePartition(&d,i);break;} wrd(); dump("after delete first logical");
 load(); if(GptAddPartition(&d,0,15*2048,6,0,&i))printf("add failed\n"); wrd(); dump("after create in gap");
 load(); GptAddPartition(&d,0,10*2048,6,0,&i); wrd(); dump("after 2nd create");
 load(); for(i=0;i<d.Count;i++) if(d.Part[i].IsLogical&&d.Part[i].StartLba>160000){GptDeletePartition(&d,i);break;} wrd(); dump("after delete middle/last");
 return 0;}
