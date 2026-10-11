#include "reminder_queue.h"
#include <string.h>
bool wristflow_reminder_key_equal(wristflow_reminder_key_t a, wristflow_reminder_key_t b)
{ return a.source == b.source && a.id == b.id; }
static bool before(wristflow_reminder_entry_t a, wristflow_reminder_entry_t b)
{
    return a.due_ms < b.due_ms || (a.due_ms == b.due_ms &&
        (a.key.source < b.key.source || (a.key.source == b.key.source && a.order < b.order)));
}
bool wristflow_reminder_enqueue(wristflow_reminder_queue_t *queue, wristflow_reminder_entry_t entry)
{
    if (!queue || entry.key.source > WF_REMINDER_COUNTDOWN ||
        (entry.key.source == WF_REMINDER_ALARM && !entry.key.id)) return false;
    for (unsigned i = 0; i < queue->count; ++i)
        if (wristflow_reminder_key_equal(queue->entries[i].key, entry.key)) return true;
    if (queue->count == WRISTFLOW_REMINDER_CAPACITY) { ++queue->overflow; return false; }
    unsigned at = queue->count, minimum = queue->active && queue->count ? 1U : 0U;
    while (at > minimum && before(entry, queue->entries[at - 1])) {
        queue->entries[at] = queue->entries[at - 1]; --at;
    }
    queue->entries[at] = entry; ++queue->count; return true;
}
bool wristflow_reminder_remove(wristflow_reminder_queue_t *queue, wristflow_reminder_key_t key)
{
    for (unsigned i = 0; queue && i < queue->count; ++i) if (wristflow_reminder_key_equal(queue->entries[i].key, key)) {
        memmove(queue->entries + i, queue->entries + i + 1, (--queue->count - i) * sizeof queue->entries[0]);
        if (!i || !queue->count) queue->active = false;
        return true;
    }
    return false;
}
const wristflow_reminder_entry_t *wristflow_reminder_front(const wristflow_reminder_queue_t *queue)
{ return queue && queue->count ? &queue->entries[0] : NULL; }
