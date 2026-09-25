#ifndef WRISTFLOW_WATCHFACE_H
#define WRISTFLOW_WATCHFACE_H

#include "lvgl.h"
#include "watch_core.h"

/* Descriptors must outlive the shell. Each create returns an independent child
 * of parent, initially inactive. On failure, create cleans up its own work.
 * update receives a validated snapshot on the LVGL thread, including when hidden.
 * set_visible controls timers/animations; destroy releases non-object resources.
 * The shell deletes the root and its children after destroy returns.
 * Callbacks must not reenter shell APIs or retain the snapshot pointer. */
typedef struct {
    const char *id;
    lv_obj_t *(*create)(lv_obj_t *parent);
    void (*update)(lv_obj_t *root, const wristflow_watch_snapshot_t *snapshot);
    void (*set_visible)(lv_obj_t *root, bool visible);
    void (*destroy)(lv_obj_t *root);
} wristflow_watchface_t;

extern const wristflow_watchface_t wristflow_default_watchface;
extern const wristflow_watchface_t wristflow_simple_watchface;

#endif
