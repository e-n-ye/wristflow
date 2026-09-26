#ifndef WRISTFLOW_APPS_H
#define WRISTFLOW_APPS_H
#include "ui_shell.h"

typedef struct wristflow_apps wristflow_apps_t;
wristflow_apps_t *wristflow_apps_create(wristflow_ui_shell_t *shell, lv_obj_t *controls,
                                      wristflow_brightness_cb_t brightness, void *context,
                                      uint8_t initial_brightness, bool product_mode,
                                      wristflow_menu_layout_t menu_layout);
uint8_t wristflow_apps_brightness(const wristflow_apps_t *apps);
wristflow_menu_layout_t wristflow_apps_menu_layout(const wristflow_apps_t *apps);
void wristflow_apps_set_brightness(wristflow_apps_t *apps, uint8_t brightness);
bool wristflow_apps_timer_running(const wristflow_apps_t *apps);
bool wristflow_apps_timer_has_data(const wristflow_apps_t *apps);
void wristflow_apps_timer_clear(wristflow_apps_t *apps);
/* Apps is the sole owner of app screens. The shell binds navigation events only
 * when created is true; callers must not retain a screen after it leaves the path. */
lv_obj_t *wristflow_apps_screen(wristflow_apps_t *apps, wristflow_surface_t surface, bool *created);
/* Leave the old view, then enter the new one as its transition starts. */
void wristflow_apps_activate(wristflow_apps_t *apps, wristflow_surface_t surface);
/* After LVGL finishes the transition, release views absent from navigation.
 * Launcher is resident to preserve its browse position; models outlive views. */
void wristflow_apps_collect(wristflow_apps_t *apps, const wristflow_navigation_t *navigation);
/* Internal adapter boundary: snapshot is validated and owned by ui_shell. */
void wristflow_apps_update(wristflow_apps_t *apps, const wristflow_watch_snapshot_t *snapshot);
void wristflow_apps_destroy(wristflow_apps_t *apps);
#endif
