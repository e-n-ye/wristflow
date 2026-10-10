#ifndef WRISTFLOW_COUNTDOWN_VIEW_H
#define WRISTFLOW_COUNTDOWN_VIEW_H

#include "ui_shell.h"

typedef struct wristflow_countdown_view wristflow_countdown_view_t;
/* NULL backend selects the local demonstration model. Product supplies an
 * independent service; this adapter never advances that service's deadline. */
wristflow_countdown_view_t *wristflow_countdown_view_create(wristflow_ui_shell_t *shell,
    const wristflow_countdown_port_t *backend);
bool wristflow_countdown_view_refresh(wristflow_countdown_view_t *view);
bool wristflow_countdown_view_active(const wristflow_countdown_view_t *view);
lv_obj_t *wristflow_countdown_view_screen(wristflow_countdown_view_t *view);
void wristflow_countdown_view_detach(wristflow_countdown_view_t *view);
void wristflow_countdown_view_clock(wristflow_countdown_view_t *view, const wristflow_watch_snapshot_t *snapshot);
void wristflow_countdown_view_bind_home(wristflow_countdown_view_t *view, lv_obj_t *home);
void wristflow_countdown_view_home_visible(wristflow_countdown_view_t *view, bool visible);
bool wristflow_countdown_view_back(wristflow_countdown_view_t *view);
void wristflow_countdown_view_destroy(wristflow_countdown_view_t *view);

#endif
