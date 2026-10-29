#ifndef WF_STORAGE_SNAPSHOT_H
#define WF_STORAGE_SNAPSHOT_H
#include "storage_metadata.h"
typedef struct {
    void *context;
    bool (*lock)(void *);
    bool (*unlock)(void *);
    int (*read)(void *, uint32_t, uint8_t *, size_t);
    uint32_t (*now)(void *);
} wf_storage_copy_io_t;
typedef struct { uint32_t bytes, calls, elapsed_ms; } wf_storage_copy_stats_t;
enum { WF_STORAGE_COPY_OK, WF_STORAGE_COPY_ARGUMENT, WF_STORAGE_COPY_UNAVAILABLE,
    WF_STORAGE_COPY_LOCK, WF_STORAGE_COPY_READ, WF_STORAGE_COPY_TIMEOUT, WF_STORAGE_COPY_UNLOCK };
/* 128-byte reads; finite lock supplied by owner; 2s checked between reads.
 * An individual platform driver read can still exceed that budget. */
unsigned wf_storage_copy(const wf_storage_copy_io_t *, uint8_t *, size_t, wf_storage_copy_stats_t *);
#endif
