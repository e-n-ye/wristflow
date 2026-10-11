#ifndef WRISTFLOW_REMINDER_QUEUE_H
#define WRISTFLOW_REMINDER_QUEUE_H
#include <stdbool.h>
#include <stdint.h>
#define WRISTFLOW_REMINDER_CAPACITY 6U
typedef enum { WF_REMINDER_ALARM, WF_REMINDER_COUNTDOWN } wristflow_reminder_source_t;
typedef struct { wristflow_reminder_source_t source; uint32_t id; } wristflow_reminder_key_t;
typedef struct { wristflow_reminder_key_t key; uint64_t due_ms; uint32_t order; } wristflow_reminder_entry_t;
typedef struct {
    wristflow_reminder_entry_t entries[WRISTFLOW_REMINDER_CAPACITY];
    unsigned count;
    bool active;
    uint32_t overflow;
} wristflow_reminder_queue_t;
bool wristflow_reminder_key_equal(wristflow_reminder_key_t a, wristflow_reminder_key_t b);
/* Batch all producer snapshots before start. Once active, the front is pinned;
 * new entries sort behind it and existing source/ID entries coalesce. */
bool wristflow_reminder_enqueue(wristflow_reminder_queue_t *queue, wristflow_reminder_entry_t entry);
bool wristflow_reminder_remove(wristflow_reminder_queue_t *queue, wristflow_reminder_key_t key);
const wristflow_reminder_entry_t *wristflow_reminder_front(const wristflow_reminder_queue_t *queue);
#endif
