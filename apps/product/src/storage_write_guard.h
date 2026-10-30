#ifndef WF_STORAGE_WRITE_GUARD_H
#define WF_STORAGE_WRITE_GUARD_H
#include <fal.h>
#include <stdbool.h>
typedef struct {
    unsigned fault; /* 1 bounds, 2 write result, 3 readback, 4 erase result */
    uint32_t address;
    size_t bytes;
    uint32_t blocked;
} wf_storage_write_fault_t;
/* Startup-only binding, before FlashDB init. There is deliberately no runtime
 * reset API. All operations and status reads use the owner's DB mutex. */
bool wf_storage_write_guard_bind(const struct fal_partition *);
wf_storage_write_fault_t wf_storage_write_guard_status(void);
int __wrap_fal_partition_write(const struct fal_partition *, uint32_t, const uint8_t *, size_t);
int __wrap_fal_partition_erase(const struct fal_partition *, uint32_t, size_t);
#endif
