#include "watchface.h"
#include "mount_screen.h"
#include "wristflow_ui.h"
#include "product_state.h"
#include <stdio.h>

static lv_obj_t *create(lv_obj_t *parent)
{
    return wristflow_mount_screen(parent, screen_watchface_simple_create());
}

static void update(lv_obj_t *root, const wristflow_watch_snapshot_t *snapshot)
{
    lv_obj_set_style_text_font(lv_obj_find_by_name(root, "simple_time"),
        snapshot->time_unavailable ? LV_FONT_DEFAULT : clock_80, 0);
    lv_obj_set_style_text_font(lv_obj_find_by_name(root, "simple_battery"),
        snapshot->battery_unavailable ? LV_FONT_DEFAULT : body_20, 0);
    if (snapshot->time_unavailable)
        lv_label_set_text(lv_obj_find_by_name(root, "simple_time"), "--:--");
    else lv_label_set_text_fmt(lv_obj_find_by_name(root, "simple_time"), "%02u:%02u",
                         (unsigned)snapshot->hour_24, (unsigned)snapshot->minute);
    char text[16];
    wristflow_battery_text(snapshot, text, sizeof text);
    if (snapshot->battery_unavailable && (snapshot->power.charge == WF_POWER_CHARGE_ACTIVE ||
        snapshot->power.charge == WF_POWER_CHARGE_FULL || snapshot->power.charge == WF_POWER_CHARGE_FAULT)) {
        lv_obj_set_style_text_font(lv_obj_find_by_name(root, "simple_battery"), notification_22, 0);
        snprintf(text, sizeof text, "%s", wristflow_power_charge_text(&snapshot->power));
    }
    lv_obj_set_style_text_color(lv_obj_find_by_name(root, "simple_battery"),
        lv_color_hex(snapshot->power.charge == WF_POWER_CHARGE_FAULT ? 0xf87171 :
        snapshot->power.charge == WF_POWER_CHARGE_ACTIVE || snapshot->power.charge == WF_POWER_CHARGE_FULL ?
        0x00dfc0 : 0xa7adb5), 0);
    lv_label_set_text(lv_obj_find_by_name(root, "simple_battery"), text);
}

const wristflow_watchface_t wristflow_simple_watchface = {
    "simple", create, update, NULL, NULL
};
