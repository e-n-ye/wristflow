#include "fs_probe.h"
#include <assert.h>
#include <string.h>
#include <limits.h>

static const wf_fs_region_t region={0x12000000,0x9a0000,0x400000,0x12da0000};
static wf_fs_device_t device={0x12000000,0x1000000,0x1840ef,0x1000000,1,8192,true,true,true};
static uint32_t ticks, step, reads, fail_at, yields;
static int fail_delta=1;
static bool late, blank;
static uint8_t value(uint32_t offset) { return blank ? 255 : (uint8_t)((offset/4096+offset)%256); }
static int read_bytes(void *context,uint32_t address,uint8_t *buffer,size_t n)
{
    (void)context;
    assert(n && n<=1024 && address>=region.base+region.offset && address<region.next && n<=region.next-address);
    ++reads;ticks+=step;
    if(late && address+n==region.next) ticks+=12000;
    if(reads==fail_at) return (int)n-fail_delta;
    for(size_t i=0;i<n;++i) buffer[i]=value(address-region.base-region.offset+(uint32_t)i);
    return (int)n;
}
static uint32_t now(void *context) { (void)context;return ticks; }
static void yield(void *context) { (void)context;++yields; }
static uint32_t reference_crc(void)
{
    uint32_t crc=UINT32_MAX;
    for(uint32_t i=0;i<region.bytes;++i) {
        crc^=value(i);
        for(unsigned bit=0;bit<8;++bit) crc=(crc>>1) ^ ((crc&1) ? 0xedb88320u : 0);
    }
    return crc^UINT32_MAX;
}
int main(void)
{
    wf_fs_io_t io={NULL,read_bytes,now,yield};wf_fs_pass_t p,q;
    assert(!wf_fs_guard(&region,&device));
    assert(wf_fs_scan(&region,&device,&io,&p)==WF_FS_OK);
    assert(p.bytes==0x400000 && p.calls==4096 && reads==4096 && yields==128);
    assert(p.ff+p.nonff==p.bytes && p.ff==p.bytes/256 && p.crc==reference_crc());
    for(unsigned i=0;i<16;++i) assert(p.head[i]==value(i) && p.tail[i]==value(region.bytes-16+i));
    q=p;assert(wf_fs_equal(&p,&q));q.tail[0]^=1;assert(!wf_fs_equal(&p,&q));
    blank=true;reads=0;assert(!wf_fs_scan(&region,&device,&io,&q));
    assert(q.ff==region.bytes && !q.nonff && q.crc==reference_crc() && !wf_fs_equal(&p,&q));
    for(unsigned test=0;test<12;++test) {
        wf_fs_region_t r=region;wf_fs_device_t d=device;reads=0;
        switch(test) {
            case 0:d.handles=false;break;case 1:d.nor=false;break;case 2:d.table_match=false;break;
            case 3:d.bytes=0x80000;break;case 4:d.dual=32;break;case 5:d.erase=0;break;
            case 6:r.bytes=UINT32_MAX;break;case 7:r.base=UINT32_MAX;break;
            case 8:r.next++;break;case 9:r.offset++;break;case 10:r.bytes=0;break;
            case 11:d.rdid=UINT32_MAX;break;
        }
        assert(wf_fs_guard(&r,&d));assert(wf_fs_scan(&r,&d,&io,&q)==WF_FS_GUARD && !reads && !q.bytes);
    }
    reads=0;fail_at=1;assert(wf_fs_scan(&region,&device,&io,&q)==WF_FS_READ && !q.bytes && q.calls==1);
    reads=0;fail_delta=1024;assert(wf_fs_scan(&region,&device,&io,&q)==WF_FS_READ && !q.bytes);
    reads=0;fail_delta=1025;assert(wf_fs_scan(&region,&device,&io,&q)==WF_FS_READ && !q.bytes);
    fail_delta=1;
    reads=0;fail_at=4096;assert(wf_fs_scan(&region,&device,&io,&q)==WF_FS_READ && q.bytes==region.bytes-1024);
    fail_at=0;reads=0;ticks=0;step=1000;
    assert(wf_fs_scan(&region,&device,&io,&q)==WF_FS_TIMEOUT && reads==12 && q.ms==12000);
    step=0;late=true;reads=0;ticks=0;
    assert(wf_fs_scan(&region,&device,&io,&q)==WF_FS_TIMEOUT && q.bytes==region.bytes);
    late=false;step=1;reads=0;ticks=UINT32_MAX-2000;
    assert(!wf_fs_scan(&region,&device,&io,&q) && q.ms==4096);
    return 0;
}
