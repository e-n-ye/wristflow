#ifndef WF_FS_PROBE_H
#define WF_FS_PROBE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WF_FS_CHUNK 1024u
#define WF_FS_PREVIEW 16u
#define WF_FS_BUDGET_MS 12000u
typedef struct { uint32_t base, offset, bytes, next; } wf_fs_region_t;
typedef struct {
    uint32_t base, bytes, rdid, table_bytes, dual, erase;
    bool handles, nor, table_match;
} wf_fs_device_t;
enum { WF_FS_BAD_HANDLE=1, WF_FS_BAD_ID=2, WF_FS_BAD_GEOMETRY=4, WF_FS_BAD_RANGE=8 };
enum { WF_FS_OK=0, WF_FS_GUARD=1, WF_FS_READ=2, WF_FS_TIMEOUT=3,
       WF_FS_CHANGED=4, WF_FS_BUSY=5 };
typedef struct {
    uint32_t bytes, calls, ff, nonff, crc, ms;
    uint8_t head[WF_FS_PREVIEW], tail[WF_FS_PREVIEW];
} wf_fs_pass_t;
typedef struct {
    void *context;
    int (*read)(void *, uint32_t address, uint8_t *, size_t);
    uint32_t (*now)(void *);
    void (*yield)(void *);
} wf_fs_io_t;

/* Only observed SDK metadata: no physical capacity/erase or ownership claim. */
unsigned wf_fs_guard(const wf_fs_region_t *, const wf_fs_device_t *);
/* Caller serializes access. One pass, fixed 1KiB stack buffer, no write API.
 * Deadline checked around each call; a blocked SDK call cannot be preempted. */
unsigned wf_fs_scan(const wf_fs_region_t *, const wf_fs_device_t *, const wf_fs_io_t *, wf_fs_pass_t *);
bool wf_fs_equal(const wf_fs_pass_t *, const wf_fs_pass_t *);
#endif
