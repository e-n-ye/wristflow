#ifndef WRISTFLOW_BREATHING_VIEW_H
#define WRISTFLOW_BREATHING_VIEW_H
#include "ui_shell.h"
typedef struct wristflow_breathing_view wristflow_breathing_view_t;
wristflow_breathing_view_t *wristflow_breathing_view_create(void);
lv_obj_t *wristflow_breathing_view_screen(wristflow_breathing_view_t *view);
void wristflow_breathing_view_clock(wristflow_breathing_view_t *view, const wristflow_watch_snapshot_t *clock);
bool wristflow_breathing_view_back(wristflow_breathing_view_t *view);
bool wristflow_breathing_view_gesture(wristflow_breathing_view_t *view, lv_dir_t direction, bool edge);
bool wristflow_breathing_view_confirm_leave(wristflow_breathing_view_t *view, void (*commit)(void *), void *context);
bool wristflow_breathing_view_cancel_leave(wristflow_breathing_view_t *view);
void wristflow_breathing_view_leave(wristflow_breathing_view_t *view);
void wristflow_breathing_view_detach(wristflow_breathing_view_t *view);
void wristflow_breathing_view_destroy(wristflow_breathing_view_t *view);
#endif
