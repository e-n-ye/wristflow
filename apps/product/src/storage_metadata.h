#ifndef WF_STORAGE_METADATA_H
#define WF_STORAGE_METADATA_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The locked flash2 NOR FAL device advertises an 8KiB erase block. FlashDB
 * uses it as the default sector size; the application does not override it. */
enum { WF_STORAGE_BYTES = 16384, WF_STORAGE_SECTOR = 8192,
       WF_STORAGE_SECTORS = WF_STORAGE_BYTES / WF_STORAGE_SECTOR };
typedef bool (*wf_storage_read_t)(void *, uint32_t, uint8_t *, size_t);
typedef struct {
    uint32_t live, stale, pending, tail, unknown;
    uint8_t store, dirty;
    bool valid;
} wf_storage_sector_t;
typedef struct {
    wf_storage_sector_t sectors[WF_STORAGE_SECTORS];
    uint32_t header, live, stale, pending, tail, unknown;
    uint32_t read_bytes, read_calls, known_mask, duplicate_nodes, extra_nodes;
    uint32_t bad_headers, bad_crc, read_errors;
    bool coherent;
} wf_storage_metadata_t;

/* Locked FlashDB WRITE_GRAN=1, ARM little-endian format only. No writes,
 * repair, resynchronization, allocation or printing. Tail is physical erased
 * space, not an SDK allocation/GC guarantee. Unknown regions fail closed. */
bool wf_storage_metadata_scan(wf_storage_read_t, void *, uint32_t size, wf_storage_metadata_t *);
#endif
