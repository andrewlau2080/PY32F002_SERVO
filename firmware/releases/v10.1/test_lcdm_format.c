#include "lcdm_format.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#define CHECK(format, ...) do { for (size_t size=0;size<160;size++) { unsigned char a[164],b[164];memset(a,0xA5,sizeof a);memset(b,0xA5,sizeof b);int x=lcdm_snprintf((char*)a,size,format,__VA_ARGS__);int y=snprintf((char*)b,size,format,__VA_ARGS__);assert(x==y);assert(memcmp(a,b,sizeof a)==0); } } while(0)
int main(void) {
 CHECK("%s.txt=\"%s\"","t0","V10.1");
 CHECK("fill %u,%u,%u,%u,%u",0U,65535U,320U,240U,65535U);
 CHECK("xstr %u,%u,%u,%u,%u,%u,%u,0,1,1,\"%s\"",1U,2U,3U,4U,0U,65535U,0U,"ADC");
 CHECK("%u %u %u",0U,UINT_MAX,65535U);
 CHECK("%d %u",INT_MIN,1000U);
 CHECK("%ld %s",LONG_MIN,"q4");
 CHECK("%ld %lu",LONG_MAX,ULONG_MAX);
 CHECK("%s%u %d","-",1000U,-32768);
 CHECK("%s %%","text");
 CHECK("%s","a long long long text used for checking truncation");
 assert(lcdm_snprintf(NULL,0,"%d",-123)==4);
 puts("All LCDM format/truncation/canary checks passed");
}
