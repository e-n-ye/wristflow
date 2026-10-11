#ifndef WRISTFLOW_ALARM_VIEW_H
#define WRISTFLOW_ALARM_VIEW_H

#include "ui_shell.h"

typedef struct wristflow_alarm_view wristflow_alarm_view_t;
typedef struct wristflow_reminders wristflow_reminders_t;
/* NULL backend retains the local Demo model. */
wristflow_alarm_view_t *wristflow_alarm_view_create(void);
wristflow_alarm_view_t *wristflow_alarm_view_create_with_backend(wristflow_ui_shell_t *shell,
    const wristflow_alarm_port_t *backend, wristflow_reminders_t *reminders);
void wristflow_alarm_view_refresh(wristflow_alarm_view_t *view);
lv_obj_t *wristflow_alarm_view_screen(wristflow_alarm_view_t *view);
void wristflow_alarm_view_update(wristflow_alarm_view_t *view, const wristflow_watch_snapshot_t *clock);
bool wristflow_alarm_view_back(wristflow_alarm_view_t *view);
bool wristflow_alarm_view_editing(const wristflow_alarm_view_t *view);
/* Detach before the owning apps layer deletes the screen. Committed records
 * outlive the screen; uncommitted edits and confirmation are discarded. */
void wristflow_alarm_view_detach(wristflow_alarm_view_t *view);
void wristflow_alarm_view_destroy(wristflow_alarm_view_t *view);

#endif
