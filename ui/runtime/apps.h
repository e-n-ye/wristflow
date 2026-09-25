#ifndef WRISTFLOW_APPS_H
#define WRISTFLOW_APPS_H
#include "ui_shell.h"

typedef struct wristflow_apps wristflow_apps_t;
wristflow_apps_t *wristflow_apps_create(wristflow_ui_shell_t *shell, lv_obj_t *controls,
                                      wristflow_brightness_cb_t brightness, void *context);
/* Screens are lazily created and owned by apps until destroy. */
lv_obj_t *wristflow_apps_screen(wristflow_apps_t *apps, wristflow_surface_t surface);
void wristflow_apps_activate(wristflow_apps_t *apps, wristflow_surface_t surface);
/* Internal adapter boundary: snapshot is validated and owned by ui_shell. */
void wristflow_apps_update(wristflow_apps_t *apps, const wristflow_watch_snapshot_t *snapshot);
void wristflow_apps_destroy(wristflow_apps_t *apps);
#endif
