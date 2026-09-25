#include "product_ui.h"
#include "wristflow_ui.h"
#include <string.h>

static lv_obj_t *named(lv_obj_t *root, const char *name)
{
    lv_obj_t *obj = lv_obj_find_by_name(root, name);
    LV_ASSERT(obj);
    return obj;
}

static void value(lv_obj_t *root, const char *slot, const char *text)
{
    lv_obj_t *label = named(named(root, slot), "value_label");
    /* Numeric metric subsets do not contain the USB/OFF glyphs. */
    lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, 0);
    lv_label_set_text(label, text);
}

static void update_card(lv_obj_t *root, unsigned index, const wristflow_watch_snapshot_t *snapshot)
{
    if (index == 0) {
        value(root, "slot_0", "--");
        value(root, "slot_1", "--");
        value(root, "slot_2", "USB");
        value(root, "slot_3", "OFF");
    } else if (index == 1) {
        value(root, "slot_0", "--");
        value(root, "slot_1", "--");
        value(root, "slot_2", "--");
        lv_label_set_text(named(named(root, "slot_0"), "detail_label"), "--");
    } else {
        value(root, "slot_0", "USB");
        value(root, "slot_1", "OFF");
        lv_obj_t *uptime = named(root, "slot_2");
        unsigned minutes = snapshot->uptime_seconds / 60;
        lv_label_set_text_fmt(named(uptime, "value_label"), "%u:%02u", minutes / 60, minutes % 60);
        lv_obj_t *detail = named(uptime, "detail_label");
        lv_obj_set_style_text_font(detail, LV_FONT_DEFAULT, 0);
        lv_label_set_text(detail, "h:mm");
    }
}

wristflow_ui_shell_t *wristflow_product_ui_create(const wristflow_watch_snapshot_t *snapshot,
    const wristflow_settings_t *settings, wristflow_brightness_cb_t brightness, void *context)
{
    if (!wristflow_snapshot_valid(snapshot) || !wristflow_settings_valid(settings)) return NULL;
    const wristflow_screen_factory_t cards[] = {
        screen_tile_heart_rate_create, screen_tile_activity_create, screen_tile_system_create
    };
    const wristflow_ui_shell_config_t config = {
        .watchface = strcmp(settings->face_id, "simple") == 0
            ? &wristflow_simple_watchface : &wristflow_default_watchface,
        .cards = cards, .card_count = 3, .controls = screen_control_center_create,
        .initial_snapshot = *snapshot, .enable_apps = true,
        .set_brightness = brightness, .platform_context = context,
        .initial_settings = settings, .update_card = update_card
    };
    return wristflow_ui_shell_create(&config);
}
