#ifndef WRISTFLOW_UI_SHELL_H
#define WRISTFLOW_UI_SHELL_H

#include "watchface.h"

typedef struct wristflow_ui_shell wristflow_ui_shell_t;
typedef lv_obj_t *(*wristflow_screen_factory_t)(void);
typedef void (*wristflow_brightness_cb_t)(uint8_t percent, void *context);

typedef struct {
    const wristflow_watchface_t *watchface;
    const wristflow_screen_factory_t *cards;
    uint8_t card_count;
    wristflow_screen_factory_t controls;
    wristflow_watch_snapshot_t initial_snapshot;
    bool enable_apps;
    wristflow_brightness_cb_t set_brightness;
    void *platform_context;
} wristflow_ui_shell_config_t;

/* One 390x450 default display, called on the LVGL thread after UI resource init.
 * The shell owns screen navigation while alive; do not change displays externally.
 * create activates home; the caller retains ownership of its previous screen.
 * destroy switches to a blank screen and cancels owned transitions. */
wristflow_ui_shell_t *wristflow_ui_shell_create(const wristflow_ui_shell_config_t *config);
void wristflow_ui_shell_destroy(wristflow_ui_shell_t *shell);
bool wristflow_ui_shell_update(wristflow_ui_shell_t *shell,
                              const wristflow_watch_snapshot_t *snapshot);
/* Replaces only face instances, preserving card objects and navigation state.
 * A failed create keeps all old instances and the selected face unchanged. */
bool wristflow_ui_shell_set_watchface(wristflow_ui_shell_t *shell,
                                    const wristflow_watchface_t *watchface);
bool wristflow_ui_shell_open_controls(wristflow_ui_shell_t *shell);
bool wristflow_ui_shell_close_controls(wristflow_ui_shell_t *shell);
bool wristflow_ui_shell_open(wristflow_ui_shell_t *shell, wristflow_surface_t surface);
bool wristflow_ui_shell_back(wristflow_ui_shell_t *shell);
bool wristflow_ui_shell_key(wristflow_ui_shell_t *shell);
bool wristflow_ui_shell_home(wristflow_ui_shell_t *shell);
const wristflow_navigation_t *wristflow_ui_shell_navigation(const wristflow_ui_shell_t *shell);
const char *wristflow_ui_shell_watchface_id(const wristflow_ui_shell_t *shell);

#endif
