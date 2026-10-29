#include "storage_metadata.h"
#include "storage_snapshot.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t image[WF_STORAGE_BYTES], before[WF_STORAGE_BYTES], copied[WF_STORAGE_BYTES];
static unsigned calls;
static int fail_call;
static void put32(uint8_t *p,uint32_t v)
{ for (unsigned i=0;i<4;++i) p[i]=(uint8_t)(v>>(8*i)); }
/* Independent table/nibble fixture CRC; also checked against the standard vector. */
static uint32_t fixture_crc(uint32_t crc,const uint8_t *p,size_t n)
{
    static const uint32_t t[]={0,0x1db71064,0x3b6e20c8,0x26d930ac,0x76dc4190,0x6b6b51f4,0x4db26158,0x5005713c,
        0xedb88320,0xf00f9344,0xd6d6a3e8,0xcb61b38c,0x9b64c2b0,0x86d3d2d4,0xa00ae278,0xbdbdf21c};
    crc=~crc;
    for(size_t i=0;i<n;++i) { crc^=p[i]; crc=(crc>>4)^t[crc&15]; crc=(crc>>4)^t[crc&15]; }
    return ~crc;
}
static void blank(void)
{
    memset(image,255,sizeof image);
    for(unsigned i=0;i<4;++i) {
        uint8_t *p=image+i*4096; p[0]=0x3f;p[1]=0x7f;put32(p+4,0x30424446);
    }
}
static unsigned node(unsigned off,const char *key,unsigned n,uint8_t state)
{
    unsigned names=(unsigned)strlen(key),len=24+names+n;
    assert(off+len<=sizeof image);
    uint8_t *p=image+off;
    memset(p,255,len);p[0]=state;put32(p+4,0x3030564b);put32(p+8,len);p[16]=(uint8_t)names;put32(p+20,n);
    memcpy(p+24,key,names);
    for(unsigned i=0;i<n;++i) p[24+names+i]=(uint8_t)(i*17U);
    uint32_t crc=fixture_crc(0,p+16,8);crc=fixture_crc(crc,p+24,names+n);put32(p+12,crc);
    return off+len;
}
static bool read_image(void *ctx,uint32_t off,uint8_t *out,size_t n)
{
    assert(ctx==image && n<=128 && off<=sizeof image && n<=sizeof image-off);
    ++calls;
    if(fail_call>0 && calls==(unsigned)fail_call) return false;
    memcpy(out,image+off,n);return true;
}
static wf_storage_metadata_t scan(void)
{
    memcpy(before,image,sizeof image);calls=0;
    wf_storage_metadata_t r;
    assert(wf_storage_metadata_scan(read_image,image,sizeof image,&r));
    assert(!memcmp(before,image,sizeof image));
    assert(r.header+r.live+r.stale+r.pending+r.tail+r.unknown==sizeof image);
    assert(calls<3000 && r.read_bytes<40000);
    return r;
}
static bool held,lock_fail,unlock_fail;
static unsigned io_calls,unlocks;
static int short_read;
static uint32_t clock_ms,read_delay;
static bool take(void *ctx) { assert(ctx==image && !held); if(lock_fail)return false;held=true;return true; }
static bool release(void *ctx) { assert(ctx==image && held);held=false;++unlocks;return !unlock_fail; }
static uint32_t now(void *ctx) { assert(ctx==image);return clock_ms; }
static int copy_read(void *ctx,uint32_t off,uint8_t *out,size_t n)
{
    assert(held && ctx==image && n==128 && off<=sizeof image-n);++io_calls;clock_ms+=read_delay;
    memcpy(out,image+off,n);return short_read ? short_read : (int)n;
}
int main(void)
{
    assert(fixture_crc(0,(const uint8_t *)"123456789",9)==0xcbf43926U);
    blank();wf_storage_metadata_t r=scan();assert(r.coherent && r.tail==16320 && r.header==64);
    unsigned end=node(16,"preferences",24,0x3f);end=node(end,"layout_a",512,0x3f);
    end=node(end,"steps_a",132,0x0f);
    r=scan();assert(r.coherent && r.live==603 && r.stale==163 && r.known_mask==3);
    node(end,"preferences",24,0x3f);r=scan();assert(!r.coherent && r.duplicate_nodes==1);
    blank();node(16,"legacy_secret",16,0x3f);r=scan();assert(!r.coherent && r.extra_nodes==1);
    blank();node(16,"runtime_a",512,0x1f);r=scan();assert(r.pending==545 && !r.coherent && !r.stale);
    image[16]=0x7f;r=scan();assert(r.pending==545 && !r.coherent);
    image[16]=7;r=scan();assert(r.unknown==4080 && !r.coherent);
    blank();node(16,"preferences",24,0x3f);image[16+24+11]^=1;r=scan();assert(r.bad_crc==1 && r.unknown==4080);
    blank();node(16,"preferences",24,0x3f);image[16+17]=0;r=scan();assert(r.bad_headers && r.unknown==4080);
    blank();node(16,"preferences",24,0x3f);put32(image+24,UINT32_MAX);r=scan();assert(r.unknown==4080);
    blank();node(16,"preferences",24,0x3f);put32(image+36,UINT32_MAX);r=scan();assert(r.unknown==4080);
    blank();node(4080,"x",0,0x3f);r=scan();assert(!r.coherent && r.unknown>=4080);
    blank();image[4000]=0;r=scan();assert(r.unknown==4080 && r.tail==12240); /* Verify the whole erased tail. */
    blank();image[0]=0x7f;image[100]=0;r=scan();assert(!r.coherent && r.unknown==4080);
    blank();put32(image+8,0);r=scan();assert(!r.coherent && !r.sectors[0].valid && r.unknown==4080);
    blank();image[0]=0x5f;r=scan();assert(!r.coherent && r.bad_headers);
    blank();image[1]=0x1f;r=scan();assert(!r.coherent && !r.unknown);
    memset(image,255,sizeof image);r=scan();assert(!r.coherent && r.unknown==16320);
    blank();fail_call=1;r=scan();assert(r.read_errors==1 && r.unknown==4080);fail_call=0;
    memset(&r,0xcc,sizeof r);assert(!wf_storage_metadata_scan(read_image,image,sizeof image-1,&r));assert(!r.coherent);
    /* Deterministic corrupt-image stress: no OOB reads, mutation or lost bytes. */
    uint32_t seed=1;
    for(unsigned i=0;i<2000;++i) {
        blank();node(16,"preferences",24,0x3f);node(75,"layout_a",512,0x3f);
        for(unsigned j=0;j<8;++j) { seed=seed*1664525U+1013904223U;image[seed%sizeof image]^=(uint8_t)(seed>>24); }
        (void)scan();
    }
    blank();wf_storage_copy_io_t io={image,take,release,copy_read,now};wf_storage_copy_stats_t stats;
    clock_ms=UINT32_MAX-100;read_delay=1;
    assert(wf_storage_copy(&io,copied,sizeof copied,&stats)==0);
    assert(!held && io_calls==128 && unlocks==1 && stats.bytes==16384 && !memcmp(image,copied,sizeof image));
    lock_fail=true;assert(wf_storage_copy(&io,copied,sizeof copied,&stats)==WF_STORAGE_COPY_LOCK);assert(!held && unlocks==1);lock_fail=false;
    short_read=127;assert(wf_storage_copy(&io,copied,sizeof copied,&stats)==WF_STORAGE_COPY_READ);assert(!held && stats.bytes==0);short_read=0;
    read_delay=2000;assert(wf_storage_copy(&io,copied,sizeof copied,&stats)==WF_STORAGE_COPY_TIMEOUT);assert(!held && stats.calls==1);
    read_delay=1;unlock_fail=true;assert(wf_storage_copy(&io,copied,sizeof copied,&stats)==WF_STORAGE_COPY_UNLOCK);assert(!held);
    assert(wf_storage_copy(&io,copied,1,&stats)==WF_STORAGE_COPY_ARGUMENT);
    puts("storage metadata: valid/corrupt images, conservation, readonly reads and bounded copy passed");
    return 0;
}
