#ifndef WRISTFLOW_ALARM_VIEW_H
#define WRISTFLOW_ALARM_VIEW_H

#include "ui_shell.h"

typedef struct wristflow_alarm_view wristflow_alarm_view_t;
/* First-round Demo session owner; no Product backend or expiry polling. */
wristflow_alarm_view_t *wristflow_alarm_view_create(void);
lv_obj_t *wristflow_alarm_view_screen(wristflow_alarm_view_t *view);
void wristflow_alarm_view_update(wristflow_alarm_view_t *view, const wristflow_watch_snapshot_t *clock);
bool wristflow_alarm_view_back(wristflow_alarm_view_t *view);
bool wristflow_alarm_view_editing(const wristflow_alarm_view_t *view);
/* Detach before the owning apps layer deletes the screen. Committed records
 * outlive the screen; uncommitted edits and confirmation are discarded. */
void wristflow_alarm_view_detach(wristflow_alarm_view_t *view);
void wristflow_alarm_view_destroy(wristflow_alarm_view_t *view);

#endif
