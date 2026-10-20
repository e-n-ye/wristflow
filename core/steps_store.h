#ifndef WRISTFLOW_STEPS_STORE_H
#define WRISTFLOW_STEPS_STORE_H
#include "daily_steps.h"
#include <stddef.h>
#define WRISTFLOW_STEPS_LEGACY_BYTES 32U
#define WRISTFLOW_STEPS_RECORD_BYTES 132U
#define WRISTFLOW_STEPS_SAVE_MS 60000U
/* Return the actual stored length, or zero on failure/truncation. */
typedef size_t (*wristflow_steps_read_t)(unsigned, uint8_t *, size_t, void *);
typedef bool (*wristflow_steps_write_t)(unsigned, const uint8_t *, void *);
/* Single worker owns this state and all I/O. No native counter is persisted. */
typedef struct {
    int active;
    uint32_t generation;
    wristflow_steps_checkpoint_t saved, latest;
    uint64_t due_ms, next_save_ms;
    unsigned attempts;
    bool saved_valid, latest_valid, dirty, failed;
} wristflow_steps_store_t;
bool wristflow_steps_encode(const wristflow_steps_checkpoint_t *, uint32_t, uint8_t *);
bool wristflow_steps_decode(wristflow_steps_checkpoint_t *, uint32_t *, const uint8_t *, size_t);
bool wristflow_steps_store_restore(wristflow_steps_store_t *, wristflow_steps_read_t, void *);
void wristflow_steps_store_offer(wristflow_steps_store_t *, const wristflow_steps_snapshot_t *, uint64_t);
/* Returns -1 when no deadline remains; failed budgets require explicit flush. */
int32_t wristflow_steps_store_wait(const wristflow_steps_store_t *, uint64_t);
void wristflow_steps_store_flush(wristflow_steps_store_t *, uint64_t);
bool wristflow_steps_store_process(wristflow_steps_store_t *, uint64_t,
    wristflow_steps_read_t, wristflow_steps_write_t, void *);
#endif
