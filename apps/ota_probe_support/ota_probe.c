/* Pure framing, bounded transport policy and exact test-image comparison. */
#include "ota_probe.h"
uint32_t wf_ota_probe_crc(const void *data, size_t length)
{
    const uint8_t *p=data;
    uint32_t crc=UINT32_MAX;
    for (size_t i=0;i<length;++i) {
        crc^=p[i];
        for (unsigned bit=0;bit<8;++bit) crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);
    }
    return ~crc;
}
void wf_ota_probe_frame_init(wf_ota_probe_frame_t *f, unsigned kind, const uint32_t tag[4])
{
    volatile uint32_t *words=(volatile uint32_t *)f;
    for (unsigned i=0;i<WF_OTA_PROBE_WORDS;++i) words[i]=0;
    f->magic=WF_OTA_PROBE_MAGIC;
    f->schema_kind=0x10000u|kind;
    for (unsigned i=0;i<4;++i) f->tag[i]=tag[i];
}
void wf_ota_probe_frame_seal(wf_ota_probe_frame_t *f)
{
    f->crc=wf_ota_probe_crc(f, sizeof(*f)-sizeof(f->crc));
}
static bool ready(const wf_ota_probe_tx_t *tx, uint32_t mask, uint32_t *polls)
{
    while (*polls) {
        --*polls;
        if (tx->flags(tx->context)&mask) return true;
    }
    return false;
}
bool wf_ota_probe_send(const wf_ota_probe_frame_t *f, const wf_ota_probe_tx_t *tx, uint32_t polls)
{
    if (!f || !tx || !tx->flags || !tx->put || !polls || polls>2000000u ||
        f->magic!=WF_OTA_PROBE_MAGIC || (f->schema_kind>>16)!=1 ||
        !(f->schema_kind&0xffffu) || (f->schema_kind&0xffffu)>WF_OTA_PROBE_ERROR ||
        f->crc!=wf_ota_probe_crc(f,sizeof(*f)-4)) return false;
    const uint8_t *p=(const uint8_t *)f;
    for (unsigned i=0;i<sizeof(*f);++i) {
        if (!ready(tx,WF_OTA_PROBE_TXE,&polls)) return false;
        tx->put(tx->context,p[i]);
    }
    return ready(tx,WF_OTA_PROBE_TC,&polls);
}
bool wf_ota_probe_match(const uint8_t *expected, uint32_t bytes,
                       int (*read)(uint32_t,uint8_t *,uint32_t,void *), void *context)
{
    if (!expected || !read || bytes<=1024 || bytes>4096) return false;
    uint8_t block[256];
    for (uint32_t offset=0;offset<bytes;) {
        uint32_t n=bytes-offset;
        if (n>sizeof(block)) n=sizeof(block);
        if (read(offset,block,n,context)!=(int)n) return false;
        for (uint32_t i=0;i<n;++i) if (block[i]!=expected[offset+i]) return false;
        offset+=n;
    }
    return true;
}
