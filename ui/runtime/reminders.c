#include "reminders.h"
struct wristflow_reminders {
    wristflow_ui_shell_t *shell;
    wristflow_reminder_queue_t queue;
    struct { wristflow_reminder_key_t key; lv_obj_t *content; } slots[WRISTFLOW_REMINDER_CAPACITY];
    lv_obj_t *current;
    bool batching, arrived;
};
static void present(wristflow_reminders_t *owner)
{
    const wristflow_reminder_entry_t *front = wristflow_reminder_front(&owner->queue);
    lv_obj_t *content = NULL;
    if (front) for (unsigned i = 0; i < WRISTFLOW_REMINDER_CAPACITY; ++i)
        if (owner->slots[i].content && wristflow_reminder_key_equal(owner->slots[i].key, front->key))
            content = owner->slots[i].content;
    if (content && (content != owner->current || !owner->queue.active)) {
        bool accepted = owner->current ? wristflow_ui_shell_replace_reminder(owner->shell, content) :
            wristflow_ui_shell_present_reminder(owner->shell, content);
        if (accepted) { owner->current = content; owner->queue.active = true; }
    } else if (!content && owner->current) {
        wristflow_ui_shell_close_reminder(owner->shell); owner->current = NULL;
    }
}
wristflow_reminders_t *wristflow_reminders_create(wristflow_ui_shell_t *shell)
{
    wristflow_reminders_t *owner = lv_malloc_zeroed(sizeof *owner);
    LV_ASSERT_MALLOC(owner); owner->shell = shell; return owner;
}
void wristflow_reminders_begin(wristflow_reminders_t *owner)
{ owner->batching = true; owner->arrived = false; }
bool wristflow_reminders_submit(wristflow_reminders_t *owner, wristflow_reminder_entry_t entry, lv_obj_t *content)
{
    if (!owner || !content) return false;
    unsigned slot = WRISTFLOW_REMINDER_CAPACITY;
    for (unsigned i = 0; i < WRISTFLOW_REMINDER_CAPACITY; ++i)
        if (owner->slots[i].content && wristflow_reminder_key_equal(owner->slots[i].key, entry.key)) { slot = i; break; }
    if (slot == WRISTFLOW_REMINDER_CAPACITY) for (unsigned i = 0; i < WRISTFLOW_REMINDER_CAPACITY; ++i)
        if (!owner->slots[i].content) { slot = i; break; }
    if (slot == WRISTFLOW_REMINDER_CAPACITY || !wristflow_reminder_enqueue(&owner->queue, entry)) return false;
    owner->slots[slot].key = entry.key; owner->slots[slot].content = content;
    owner->arrived = true;
    if (!owner->batching) { present(owner); wristflow_ui_shell_wake_reminder(owner->shell); }
    return true;
}
bool wristflow_reminders_end(wristflow_reminders_t *owner)
{
    owner->batching = false; present(owner);
    if (owner->arrived) wristflow_ui_shell_wake_reminder(owner->shell);
    return owner->arrived;
}
void wristflow_reminders_remove(wristflow_reminders_t *owner, wristflow_reminder_key_t key)
{
    if (!owner || !wristflow_reminder_remove(&owner->queue, key)) return;
    for (unsigned i = 0; i < WRISTFLOW_REMINDER_CAPACITY; ++i)
        if (owner->slots[i].content && wristflow_reminder_key_equal(owner->slots[i].key, key)) owner->slots[i].content = NULL;
    /* The caller reconciles all producers before end advances the host. An
     * unseen next item must not be pinned ahead of backend-pending arrivals. */
}
void wristflow_reminders_destroy(wristflow_reminders_t *owner) { lv_free(owner); }
bool wristflow_reminders_contains(const wristflow_reminders_t *owner, wristflow_reminder_key_t key)
{
    for (unsigned i = 0; owner && i < owner->queue.count; ++i)
        if (wristflow_reminder_key_equal(owner->queue.entries[i].key, key)) return true;
    return false;
}
