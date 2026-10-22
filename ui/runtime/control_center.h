#ifndef WRISTFLOW_CONTROL_CENTER_H
#define WRISTFLOW_CONTROL_CENTER_H
#include "ui_shell.h"

typedef struct wristflow_control_center wristflow_control_center_t;
wristflow_control_center_t *wristflow_control_center_create(lv_obj_t *root,
    wristflow_ui_shell_t *shell, bool product_mode);
void wristflow_control_center_update(wristflow_control_center_t *, const wristflow_watch_snapshot_t *);
void wristflow_control_center_leave(wristflow_control_center_t *);
void wristflow_control_center_destroy(wristflow_control_center_t *);
#endif
