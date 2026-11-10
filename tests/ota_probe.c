#include "ota_probe.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint32_t mode, polls, sent; uint8_t out[80], image[4096]; int fault; } peer_t;
static uint32_t flags(void *p) { peer_t *s=p; ++s->polls; return s->mode; }
static void put(void *p,uint8_t b) { peer_t *s=p; assert(s->sent<80); s->out[s->sent++]=b; }
static int read_image(uint32_t off,uint8_t *out,uint32_t n,void *p)
{ peer_t *s=p; assert(n<=256 && off+n<=4096); memcpy(out,s->image+off,n); return s->fault?(int)n-1:(int)n; }
int main(int argc,char **argv)
{
    peer_t p={0}; uint32_t tag[4]={1,2,3,4}; wf_ota_probe_frame_t f;
    assert(wf_ota_probe_crc("123456789",9)==0xcbf43926u);
    wf_ota_probe_frame_init(&f,WF_OTA_PROBE_EXECUTED,tag); f.phase=3;f.status=1;
    f.pc=0x12020480; f.sp=0x20001f00;f.vtor=0x12020000;f.primask=1;
    f.alias[0]=0x20000;f.alias[1]=0x420000;f.alias[2]=0x400000;
    f.marker=0x42584950;f.marker_address=0x12020500;f.image_bytes=2048;
    wf_ota_probe_frame_seal(&f);
    wf_ota_probe_tx_t tx={flags,put,&p};
    p.mode=WF_OTA_PROBE_TXE|WF_OTA_PROBE_TC;
    assert(wf_ota_probe_send(&f,&tx,81) && p.sent==80 && p.polls==81 && !memcmp(&f,p.out,80));
    p.mode=0;p.polls=p.sent=0;
    assert(!wf_ota_probe_send(&f,&tx,100) && p.polls==100 && !p.sent);
    p.mode=WF_OTA_PROBE_TXE;p.polls=p.sent=0;
    assert(!wf_ota_probe_send(&f,&tx,100) && p.polls==100 && p.sent==80);
    p.mode=3;p.polls=p.sent=0;
    assert(!wf_ota_probe_send(&f,&tx,80) && p.sent==80 && p.polls==80);
    assert(!wf_ota_probe_send(&f,&tx,2000001));
    f.crc^=1;assert(!wf_ota_probe_send(&f,&tx,100));f.crc^=1;
    uint8_t expected[4096];for(unsigned i=0;i<4096;++i) expected[i]=p.image[i]=(uint8_t)(i*7u);
    assert(wf_ota_probe_match(expected,4096,read_image,&p));
    for(unsigned i=0;i<4096;++i) {p.image[i]^=1;assert(!wf_ota_probe_match(expected,4096,read_image,&p));p.image[i]^=1;}
    p.fault=1;assert(!wf_ota_probe_match(expected,4096,read_image,&p));
    assert(!wf_ota_probe_match(expected,1024,read_image,&p));assert(!wf_ota_probe_match(expected,4097,read_image,&p));
    if(argc==2) {FILE *out=fopen(argv[1],"wb");assert(out);assert(fwrite(&f,1,sizeof f,out)==sizeof f);assert(!fclose(out));}
    puts("ota_probe: bounded TX/TC, CRC and4096 byte corruptions passed");
    return 0;
}
