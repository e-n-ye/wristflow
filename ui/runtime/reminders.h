#ifndef WRISTFLOW_REMINDERS_H
#define WRISTFLOW_REMINDERS_H
#include "ui_shell.h"
#include "reminder_queue.h"
typedef struct wristflow_reminders wristflow_reminders_t;
wristflow_reminders_t *wristflow_reminders_create(wristflow_ui_shell_t *shell);
void wristflow_reminders_begin(wristflow_reminders_t *owner);
/* Accepted entries, including coalesced entries, may be ACKed by the producer.
 * Content remains adapter-owned. Collect every source before end presents. */
bool wristflow_reminders_submit(wristflow_reminders_t *owner, wristflow_reminder_entry_t entry, lv_obj_t *content);
bool wristflow_reminders_end(wristflow_reminders_t *owner);
void wristflow_reminders_remove(wristflow_reminders_t *owner, wristflow_reminder_key_t key);
bool wristflow_reminders_contains(const wristflow_reminders_t *owner, wristflow_reminder_key_t key);
void wristflow_reminders_destroy(wristflow_reminders_t *owner);
#endif
