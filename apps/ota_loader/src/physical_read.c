#include "physical_read.h"
#include <string.h>

int wf_ota_physical_read(wf_ota_physical_reader_t *r, unsigned region,
                         uint32_t offset, void *out, size_t size)
{
    if (!out || !size || size > 256) return -1;
    memset(out, 0, size);
    if (!r || !r->ready || !r->read || region >= WF_OTA_READ_REGIONS) return -1;
    const wf_ota_read_region_t *span = &r->region[region];
    if (!span->bytes || span->address > UINT32_MAX - span->bytes ||
        offset >= span->bytes || size > span->bytes - offset) return -1;
    if (r->fault) return -1;
    if (!r->ready(r->context) ||
        r->read(r->context, span->address + offset, out, size) != (int)size ||
        !r->ready(r->context)) {
        memset(out, 0, size);
        r->fault = true;
        return -1;
    }
    return (int)size;
}
