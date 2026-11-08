#include "physical_read.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { unsigned reads, checks, invalid_at; int short_read; } fake_t;
static bool ready(void *context)
{ fake_t *f = context; return ++f->checks != f->invalid_at; }
static int read_bytes(void *context, uint32_t address, void *out, size_t size)
{
    fake_t *f = context; ++f->reads;
    for (size_t i = 0; i < size; ++i) ((uint8_t *)out)[i] = (uint8_t)(address + i);
    return f->short_read ? (int)size - 1 : (int)size;
}
static wf_ota_physical_reader_t reader(fake_t *f)
{
    return (wf_ota_physical_reader_t){.context=f,.ready=ready,.read=read_bytes,
        .region={{0x12020000,0x400000},{0x12420000,0x400000},{0x12da0000,8192},{0x12da2000,8192}}};
}
static void cleared(const uint8_t *bytes, size_t size)
{ for (size_t i = 0; i < size; ++i) assert(bytes[i] == 0); }
int main(void)
{
    unsigned checks = 0;
    fake_t f = {0}; wf_ota_physical_reader_t r = reader(&f);
    uint8_t bytes[256];
    for (unsigned region = 0; region < WF_OTA_READ_REGIONS; ++region) {
        assert(wf_ota_physical_read(&r, region, r.region[region].bytes - sizeof bytes, bytes, sizeof bytes) == sizeof bytes);
        for (unsigned i = 0; i < sizeof bytes; ++i) assert(bytes[i] == (uint8_t)i);
        ++checks;
        unsigned reads = f.reads;
        const uint32_t offsets[] = {r.region[region].bytes - 255, r.region[region].bytes, UINT32_MAX};
        for (unsigned i = 0; i < 3; ++i) {
            memset(bytes, 0xa5, sizeof bytes);
            assert(wf_ota_physical_read(&r, region, offsets[i], bytes, sizeof bytes) < 0);
            cleared(bytes, sizeof bytes); assert(f.reads == reads); ++checks;
        }
    }
    for (unsigned invalid_at = 1; invalid_at <= 2; ++invalid_at) {
        f = (fake_t){.invalid_at=invalid_at}; r = reader(&f);
        assert(wf_ota_physical_read(&r, 0, 0, bytes, sizeof bytes) < 0 && r.fault);
        cleared(bytes, sizeof bytes);
        unsigned reads = f.reads;
        assert(wf_ota_physical_read(&r, 0, 0, bytes, sizeof bytes) < 0 && f.reads == reads);
        ++checks;
    }
    f = (fake_t){.short_read=1}; r = reader(&f);
    assert(wf_ota_physical_read(&r, 0, 0, bytes, sizeof bytes) < 0 && r.fault);
    cleared(bytes, sizeof bytes); ++checks;
    f = (fake_t){0}; r = reader(&f); r.region[0] = (wf_ota_read_region_t){UINT32_MAX - 127,256};
    assert(wf_ota_physical_read(&r, 0, 0, bytes, sizeof bytes) < 0 && f.reads == 0); ++checks;
    assert(wf_ota_physical_read(&r, WF_OTA_READ_REGIONS, 0, bytes, 8) < 0 && f.reads == 0); ++checks;
    printf("OTA physical reader: %u checks passed; callbacks only\n", checks);
    return 0;
}
