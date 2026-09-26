#include "product_ui.h"
#include "wristflow_ui.h"
#include <string.h>

wristflow_ui_shell_t *wristflow_product_ui_create_with_layout(const wristflow_watch_snapshot_t *snapshot,
    const wristflow_settings_t *settings, wristflow_brightness_cb_t brightness, void *context,
    const wristflow_layout_t *layout, wristflow_layout_request_cb_t request,
    wristflow_layout_status_cb_t status, void *layout_context)
{
    if (!wristflow_snapshot_valid(snapshot) || !wristflow_settings_valid(settings)) return NULL;
    const wristflow_ui_shell_config_t config = {
        .watchface = strcmp(settings->face_id, "simple") == 0
            ? &wristflow_simple_watchface : &wristflow_default_watchface,
        .controls = screen_control_center_create,
        .initial_snapshot = *snapshot, .enable_apps = true,
        .set_brightness = brightness, .platform_context = context,
        .initial_settings = settings, .product_apps = true, .initial_layout = layout,
        .save_layout = request, .layout_status = status, .layout_context = layout_context
    };
    return wristflow_ui_shell_create(&config);
}

wristflow_ui_shell_t *wristflow_product_ui_create(const wristflow_watch_snapshot_t *snapshot,
    const wristflow_settings_t *settings, wristflow_brightness_cb_t brightness, void *context)
{
    return wristflow_product_ui_create_with_layout(snapshot, settings, brightness, context, NULL, NULL, NULL, NULL);
}
