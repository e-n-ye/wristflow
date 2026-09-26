#ifndef WRISTFLOW_COMPONENTS_H
#define WRISTFLOW_COMPONENTS_H
#include "ui_shell.h"
typedef struct wristflow_components wristflow_components_t;
wristflow_components_t *wristflow_components_create(wristflow_ui_shell_t *shell,
    const wristflow_layout_t *initial, wristflow_layout_request_cb_t request,
    wristflow_layout_status_cb_t status, void *context);
void wristflow_components_destroy(wristflow_components_t *components);
const wristflow_layout_t *wristflow_components_layout(const wristflow_components_t *components);
bool wristflow_components_supported(const wristflow_layout_t *layout);
lv_obj_t *wristflow_components_page(wristflow_components_t *components, unsigned index);
void wristflow_components_update(wristflow_components_t *components, lv_obj_t *root,
    unsigned page, const wristflow_watch_snapshot_t *snapshot);
wristflow_surface_t wristflow_components_target(wristflow_components_t *components, unsigned page, unsigned slot);
bool wristflow_components_enter(wristflow_components_t *components, unsigned page);
lv_obj_t *wristflow_components_screen(wristflow_components_t *components, wristflow_surface_t surface, bool *created);
bool wristflow_components_back(wristflow_components_t *components, bool home);
void wristflow_components_collect(wristflow_components_t *components, const wristflow_navigation_t *navigation);
#endif
