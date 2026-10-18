#ifndef WRISTFLOW_GAME_2048_VIEW_H
#define WRISTFLOW_GAME_2048_VIEW_H
#include "ui_shell.h"
typedef struct wristflow_2048_view wristflow_2048_view_t;
wristflow_2048_view_t *wristflow_2048_view_create(wristflow_ui_shell_t *shell);
lv_obj_t *wristflow_2048_view_screen(wristflow_2048_view_t *view);
bool wristflow_2048_view_gesture(wristflow_2048_view_t *view, lv_dir_t direction, bool edge, lv_point_t origin);
bool wristflow_2048_view_back(wristflow_2048_view_t *view);
void wristflow_2048_view_detach(wristflow_2048_view_t *view);
void wristflow_2048_view_destroy(wristflow_2048_view_t *view);
#endif
