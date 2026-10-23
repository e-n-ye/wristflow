#ifndef WRISTFLOW_AOD_VIEW_H
#define WRISTFLOW_AOD_VIEW_H

#include "ui_shell.h"

bool wristflow_aod_settings_surface(wristflow_surface_t surface);
lv_obj_t *wristflow_aod_settings_screen(wristflow_ui_shell_t *shell, wristflow_surface_t surface);
void wristflow_aod_settings_refresh(lv_obj_t *root, wristflow_surface_t surface, wristflow_ui_shell_t *shell);
/* The same XML content is used by the thumbnail and the resident display. */
void wristflow_aod_render(lv_obj_t *content, const wristflow_watch_snapshot_t *snapshot,
    wristflow_aod_style_t style, unsigned displacement);

#endif
