#ifndef WF_OTA_PHYSICAL_READ_H
#define WF_OTA_PHYSICAL_READ_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { WF_OTA_READ_A, WF_OTA_READ_B, WF_OTA_READ_META0, WF_OTA_READ_META1, WF_OTA_READ_REGIONS };
typedef struct { uint32_t address, bytes; } wf_ota_read_region_t;
typedef struct {
    void *context;
    bool (*ready)(void *); /* Exact NOR geometry, inhibited startup and no alias. */
    int (*read)(void *, uint32_t address, void *, size_t);
    wf_ota_read_region_t region[WF_OTA_READ_REGIONS];
    bool fault;
} wf_ota_physical_reader_t;

/* Caller serializes all access and prevents alias/write operations throughout.
 * At most 256 bytes; checks readiness before/after the platform read. A failed
 * operation clears its output and freezes this reader for the boot session. */
int wf_ota_physical_read(wf_ota_physical_reader_t *, unsigned region,
                         uint32_t offset, void *, size_t);
#endif
