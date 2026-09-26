#ifndef WRISTFLOW_SETTINGS_VIEW_H
#define WRISTFLOW_SETTINGS_VIEW_H
#include "ui_shell.h"
lv_obj_t *wristflow_settings_screen(wristflow_ui_shell_t *shell, wristflow_surface_t surface);
void wristflow_settings_refresh(lv_obj_t *root, wristflow_surface_t surface, wristflow_ui_shell_t *shell);
bool wristflow_settings_dismiss(lv_obj_t *root);
void wristflow_settings_keep_prompt(lv_obj_t *root, wristflow_ui_shell_t *shell);
#endif
