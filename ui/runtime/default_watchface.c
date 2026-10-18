#include "watchface.h"
#include "mount_screen.h"
#include "watchface_presenter.h"
#include "product_state.h"
#include <stdio.h>

static lv_obj_t *create(lv_obj_t *parent)
{
    return wristflow_mount_screen(parent, screen_watchface_create());
}

static void update(lv_obj_t *root, const wristflow_watch_snapshot_t *snapshot)
{
    lv_obj_t *hour = lv_obj_find_by_name(root, "hour_artwork");
    lv_obj_t *minutes = lv_obj_find_by_name(root, "minute_label");
    lv_obj_remove_flag(hour, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(minutes, metric_56, 0);
    if (snapshot->time_unavailable) {
        lv_image_set_src(hour, hour_unknown);
        lv_label_set_text(minutes, "--");
    }
    else {
        bool updated = wristflow_watchface_set_time(root, snapshot->hour_24, snapshot->minute);
        LV_ASSERT(updated);
    }
    lv_obj_t *battery = lv_obj_find_by_name(root, "battery_label");
    LV_ASSERT(battery);
    lv_obj_set_flag(lv_obj_find_by_name(root, "battery_icon"), LV_OBJ_FLAG_HIDDEN, snapshot->battery_unavailable);
    lv_obj_set_style_text_font(battery, snapshot->battery_unavailable ? LV_FONT_DEFAULT : body_20, 0);
    char text[16];
    wristflow_battery_text(snapshot, text, sizeof text);
    if (snapshot->battery_unavailable && (snapshot->power.charge == WF_POWER_CHARGE_ACTIVE ||
        snapshot->power.charge == WF_POWER_CHARGE_FULL || snapshot->power.charge == WF_POWER_CHARGE_FAULT)) {
        lv_obj_set_style_text_font(battery, notification_22, 0);
        snprintf(text, sizeof text, "%s", snapshot->power.charge == WF_POWER_CHARGE_ACTIVE ? "充电" :
            snapshot->power.charge == WF_POWER_CHARGE_FULL ? "已满" : "异常");
    }
    lv_obj_set_style_text_color(battery, lv_color_hex(snapshot->power.charge == WF_POWER_CHARGE_FAULT ?
        0xf87171 : snapshot->power.charge == WF_POWER_CHARGE_ACTIVE ||
        snapshot->power.charge == WF_POWER_CHARGE_FULL ? 0x00dfc0 : 0xffffff), 0);
    lv_label_set_text(battery, text);
}

const wristflow_watchface_t wristflow_default_watchface = {
    "diffusion", create, update, NULL, NULL
};
