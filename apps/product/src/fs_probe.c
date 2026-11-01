#if defined(WF_FS_PROBE) || defined(WF_FS_PROBE_HOST)
#include "fs_probe.h"
#include <string.h>
#include <limits.h>

unsigned wf_fs_guard(const wf_fs_region_t *r, const wf_fs_device_t *d)
{
    if (!r || !d) return WF_FS_BAD_RANGE;
    unsigned guard=0;
    if (!d->handles || !d->nor || r->base!=d->base) guard|=WF_FS_BAD_HANDLE;
    if (!d->table_match || !d->rdid || d->rdid>0xffffffu || !d->table_bytes ||
        d->bytes!=d->table_bytes) guard|=WF_FS_BAD_ID;
    if (d->dual>1 || d->erase!=(4096u << (d->dual<=1 ? d->dual : 0)) ||
        !d->erase || r->offset%d->erase || r->bytes%d->erase) guard|=WF_FS_BAD_GEOMETRY;
    if (r->bytes<WF_FS_PREVIEW || r->offset>d->bytes || r->bytes>d->bytes-r->offset ||
        r->offset>UINT32_MAX-r->base || r->bytes>UINT32_MAX-r->base-r->offset ||
        r->base+r->offset+r->bytes!=r->next) guard|=WF_FS_BAD_RANGE;
    return guard;
}

static uint32_t crc_update(uint32_t crc, const uint8_t *p, size_t n)
{
    static const uint32_t table[16]={0,0x1db71064,0x3b6e20c8,0x26d930ac,
        0x76dc4190,0x6b6b51f4,0x4db26158,0x5005713c,0xedb88320,0xf00f9344,
        0xd6d6a3e8,0xcb61b38c,0x9b64c2b0,0x86d3d2d4,0xa00ae278,0xbdbdf21c};
    while(n--) { crc^=*p++;crc=(crc>>4)^table[crc&15];crc=(crc>>4)^table[crc&15]; }
    return crc;
}

unsigned wf_fs_scan(const wf_fs_region_t *r, const wf_fs_device_t *d,
                    const wf_fs_io_t *io, wf_fs_pass_t *out)
{
    if (!out) return WF_FS_GUARD;
    memset(out,0,sizeof *out);
    if (wf_fs_guard(r,d) || !io || !io->read || !io->now || !io->yield) return WF_FS_GUARD;
    uint8_t buffer[WF_FS_CHUNK];
    uint32_t started=io->now(io->context), crc=UINT32_MAX;
    unsigned result=WF_FS_OK;
    while(out->bytes<r->bytes) {
        out->ms=io->now(io->context)-started;
        if(out->ms>=WF_FS_BUDGET_MS) { result=WF_FS_TIMEOUT;break; }
        size_t n=r->bytes-out->bytes;
        if(n>sizeof buffer) n=sizeof buffer;
        /* Guard proves the complete interval; subtraction bounds every chunk. */
        uint32_t offset=out->bytes;
        ++out->calls;
        if(io->read(io->context,r->base+r->offset+offset,buffer,n)!=(int)n) { result=WF_FS_READ;break; }
        for(size_t i=0;i<n;++i) {
            if(buffer[i]==0xff) ++out->ff;else ++out->nonff;
            if(offset+i<WF_FS_PREVIEW) out->head[offset+i]=buffer[i];
            if(offset+i>=r->bytes-WF_FS_PREVIEW) out->tail[offset+i-(r->bytes-WF_FS_PREVIEW)]=buffer[i];
        }
        crc=crc_update(crc,buffer,n);
        out->bytes+=(uint32_t)n;
        if(!(out->calls%32)) io->yield(io->context);
    }
    out->ms=io->now(io->context)-started;
    if(out->ms>=WF_FS_BUDGET_MS && result==WF_FS_OK) result=WF_FS_TIMEOUT;
    out->crc=crc^UINT32_MAX;
    memset(buffer,0,sizeof buffer);
    return result;
}

bool wf_fs_equal(const wf_fs_pass_t *a, const wf_fs_pass_t *b)
{
    return a->bytes==b->bytes && a->crc==b->crc && a->ff==b->ff && a->nonff==b->nonff &&
        !memcmp(a->head,b->head,WF_FS_PREVIEW) && !memcmp(a->tail,b->tail,WF_FS_PREVIEW);
}
#endif
