#ifndef WRISTFLOW_COUNTDOWN_VIEW_H
#define WRISTFLOW_COUNTDOWN_VIEW_H

#include "ui_shell.h"

typedef struct wristflow_countdown_view wristflow_countdown_view_t;
/* Demo adapter only: model and scheduling outlive individual screen instances.
 * Product scheduling, power policy and alert/session arbitration come later. */
wristflow_countdown_view_t *wristflow_countdown_view_create(wristflow_ui_shell_t *shell);
lv_obj_t *wristflow_countdown_view_screen(wristflow_countdown_view_t *view);
void wristflow_countdown_view_detach(wristflow_countdown_view_t *view);
void wristflow_countdown_view_clock(wristflow_countdown_view_t *view, const wristflow_watch_snapshot_t *snapshot);
void wristflow_countdown_view_bind_home(wristflow_countdown_view_t *view, lv_obj_t *home);
void wristflow_countdown_view_home_visible(wristflow_countdown_view_t *view, bool visible);
bool wristflow_countdown_view_back(wristflow_countdown_view_t *view);
void wristflow_countdown_view_destroy(wristflow_countdown_view_t *view);

#endif
