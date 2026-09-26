#ifndef WRISTFLOW_PRODUCT_UI_H
#define WRISTFLOW_PRODUCT_UI_H
#include "ui_shell.h"

/* Product composition uses the same shell and applications as ui_demo. */
wristflow_ui_shell_t *wristflow_product_ui_create(const wristflow_watch_snapshot_t *snapshot,
    const wristflow_settings_t *settings, wristflow_brightness_cb_t brightness, void *context);
wristflow_ui_shell_t *wristflow_product_ui_create_with_layout(const wristflow_watch_snapshot_t *snapshot,
    const wristflow_settings_t *settings, wristflow_brightness_cb_t brightness, void *context,
    const wristflow_layout_t *layout, wristflow_layout_request_cb_t request,
    wristflow_layout_status_cb_t status, void *layout_context);
#endif
