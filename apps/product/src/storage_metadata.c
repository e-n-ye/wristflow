#if defined(WF_STORAGE_PROBE) || defined(WF_STORAGE_METADATA_HOST)
#include "storage_metadata.h"
#include <string.h>

static uint32_t le32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static bool all_ff(const uint8_t *p, size_t n)
{ for (size_t i=0; i<n; ++i) if (p[i] != 255) return false; return true; }
static uint32_t crc32(uint32_t crc, const uint8_t *p, size_t n)
{
    crc = ~crc;
    for (size_t i=0; i<n; ++i) {
        crc ^= p[i];
        for (unsigned b=0; b<8; ++b) crc = (crc>>1) ^ ((0U-(crc&1U)) & 0xedb88320U);
    }
    return ~crc;
}
typedef struct { wf_storage_read_t read; void *context; wf_storage_metadata_t *out; } reader_t;
static bool get(reader_t *r, uint32_t off, uint8_t *buf, size_t n)
{
    if (n>128 || off>WF_STORAGE_BYTES || n>WF_STORAGE_BYTES-off) return false;
    ++r->out->read_calls;
    if (!r->read(r->context, off, buf, n)) { ++r->out->read_errors; return false; }
    r->out->read_bytes += (uint32_t)n;
    return true;
}
static bool erased(reader_t *r, uint32_t off, uint32_t n)
{
    uint8_t buf[128];
    while (n) {
        size_t take = n>sizeof buf ? sizeof buf : n;
        if (!get(r, off, buf, take) || !all_ff(buf, take)) return false;
        off += (uint32_t)take; n -= (uint32_t)take;
    }
    return true;
}
static int known_key(const uint8_t *name, size_t n)
{
    static const char *const keys[] = {"preferences", "layout_a", "layout_b", "steps_a", "steps_b",
        "alarm_a", "alarm_b", "alarm_ca", "alarm_cb", "runtime_a", "runtime_b", "__ver_num__"};
    for (unsigned i=0; i<sizeof keys/sizeof keys[0]; ++i)
        if (strlen(keys[i])==n && !memcmp(keys[i], name, n)) return (int)i;
    return -1;
}
static void sector(reader_t *r, unsigned index)
{
    wf_storage_sector_t *s = &r->out->sectors[index];
    uint32_t off=index*WF_STORAGE_SECTOR, end=off+WF_STORAGE_SECTOR;
    uint8_t h[24];
    s->unknown=WF_STORAGE_SECTOR-16;
    if (!get(r,off,h,16)) return;
    s->store=h[0]; s->dirty=h[1];
    bool store=h[0]==0x7f || h[0]==0x3f || h[0]==0x1f;
    bool dirty=h[1]==0x7f || h[1]==0x3f || h[1]==0x1f;
    if (!store || !dirty || !all_ff(h+2,2) || le32(h+4)!=0x30424446U ||
        le32(h+8)!=UINT32_MAX || le32(h+12)!=UINT32_MAX) {
        ++r->out->bad_headers; return;
    }
    s->valid=true; s->unknown=0; off+=16;
    /* EMPTY must really be empty. GC/pending state is reported even when the
     * rest of the bytes happen to parse successfully. */
    if (s->store==0x7f) {
        if (erased(r,off,end-off)) s->tail=end-off;
        else { s->unknown=end-off; ++r->out->bad_headers; }
        return;
    }
    while (off<end) {
        uint32_t left=end-off;
        if (left<24) {
            if (erased(r,off,left)) s->tail+=left; else s->unknown+=left;
            return;
        }
        if (!get(r,off,h,24)) { s->unknown+=left; return; }
        if (all_ff(h,24)) {
            if (erased(r,off+24,left-24)) s->tail+=left;
            else s->unknown+=left;
            return;
        }
        uint32_t len=le32(h+8), value=le32(h+20);
        uint8_t status=h[0], names=h[16];
        bool state=status==0x7f || status==0x3f || status==0x1f || status==0x0f;
        if (!state || !all_ff(h+1,3) || !all_ff(h+17,3) || le32(h+4)!=0x3030564bU ||
            names>64 || len<24 || len>left || names>len-24 || value!=len-24-names) {
            ++r->out->bad_headers; s->unknown+=left; return;
        }
        uint32_t crc=crc32(0,h+16,8), done=0;
        uint8_t name[64], buf[128];
        while (done<len-24) {
            uint32_t take=len-24-done;
            if (take>sizeof buf) take=sizeof buf;
            if (!get(r,off+24+done,buf,take)) { s->unknown+=left; return; }
            if (done<names) {
                uint32_t copy=names-done;
                if (copy>take) copy=take;
                memcpy(name+done,buf,copy);
            }
            crc=crc32(crc,buf,take); done+=take;
        }
        if (crc!=le32(h+12)) { ++r->out->bad_crc; s->unknown+=left; return; }
        if (status==0x3f) {
            s->live+=len;
            int key=known_key(name,names);
            if (key<0) ++r->out->extra_nodes;
            else {
                uint32_t bit=1U<<(unsigned)key;
                if (r->out->known_mask&bit) ++r->out->duplicate_nodes;
                r->out->known_mask|=bit;
            }
        } else if (status==0x0f) s->stale+=len;
        else s->pending+=len;
        off+=len;
    }
}
bool wf_storage_metadata_scan(wf_storage_read_t read, void *context, uint32_t size, wf_storage_metadata_t *out)
{
    if (!out) return false;
    memset(out,0,sizeof *out);
    if (!read || size!=WF_STORAGE_BYTES) return false;
    reader_t r={read,context,out};
    out->header=16*WF_STORAGE_SECTORS;
    bool normal=true;
    for (unsigned i=0; i<WF_STORAGE_SECTORS; ++i) {
        sector(&r,i);
        wf_storage_sector_t *s=&out->sectors[i];
        out->live+=s->live; out->stale+=s->stale; out->pending+=s->pending;
        out->tail+=s->tail; out->unknown+=s->unknown;
        if (!s->valid || s->dirty==0x1f) normal=false;
    }
    out->coherent=normal && !out->unknown && !out->pending && !out->bad_headers &&
        !out->bad_crc && !out->read_errors && !out->extra_nodes && !out->duplicate_nodes;
    return out->header+out->live+out->stale+out->pending+out->tail+out->unknown==size;
}
#endif
