#include "endurance_view.h"
#include "wristflow_ui.h"
#include <stdio.h>
#include <string.h>
static void text(lv_obj_t *root, const char *name, const char *value)
{
    lv_obj_t *label = lv_obj_find_by_name(root, name);
    if (strcmp(lv_label_get_text(label), value)) lv_label_set_text(label, value);
}
void wristflow_endurance_render(lv_obj_t *root, wristflow_ui_shell_t *shell)
{
    if (!root) return;
    const wristflow_watch_snapshot_t *s = wristflow_ui_shell_snapshot(shell);
    char value[80];
    if (s->time_unavailable) snprintf(value, sizeof value, "--:--");
    else snprintf(value, sizeof value, "%02u:%02u", s->hour_24, s->minute);
    text(root, "endurance_time", value);
    lv_obj_set_style_text_font(lv_obj_find_by_name(root, "endurance_time"),
        s->time_unavailable ? metric_56 : clock_80, 0);
    if (!s->time_unavailable && s->utc_seconds) {
        unsigned days = (unsigned)(((uint64_t)s->utc_seconds + 28800U) / 86400U);
        unsigned remaining = days, year = 1970, month = 1;
        for (;;) {
            unsigned length = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0) ? 366 : 365;
            if (remaining < length) break;
            remaining -= length; ++year;
        }
        static const unsigned lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        for (; month <= 12; ++month) {
            unsigned length = lengths[month - 1] + (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
            if (remaining < length) break;
            remaining -= length;
        }
        static const char *week[] = {"周四", "周五", "周六", "周日", "周一", "周二", "周三"};
        snprintf(value, sizeof value, "%02u/%02u %s", month, remaining + 1, week[days % 7]);
    } else snprintf(value, sizeof value, "--/--");
    text(root, "endurance_date", value);
    wristflow_battery_text(s, value, sizeof value);
    text(root, "endurance_battery", value);
    const wristflow_activity_metric_t *steps = &s->activity.metrics[WRISTFLOW_ACTIVITY_STEPS];
    if (steps->total_valid) snprintf(value, sizeof value, "步数 %lu", (unsigned long)steps->total);
    else snprintf(value, sizeof value, "步数 --");
    text(root, "endurance_steps", value);
    text(root, "endurance_status", wristflow_ui_shell_endurance_status(shell));
    wristflow_save_state_t state = wristflow_ui_shell_settings_save_state(shell);
    text(root, "endurance_save", state == WRISTFLOW_SAVE_PENDING ? "保存中" :
        state == WRISTFLOW_SAVE_FAILED ? "保存失败，退出后重试" : "");
}
void wristflow_endurance_progress(lv_obj_t *root, unsigned progress, bool showing)
{
    if (!root) return;
    lv_obj_set_flag(lv_obj_find_by_name(root, "endurance_exit"), LV_OBJ_FLAG_HIDDEN, !showing);
    lv_obj_t *arc = lv_obj_find_by_name(root, "endurance_ring");
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x008cff), LV_PART_INDICATOR);
    lv_arc_set_value(arc, (int32_t)(progress / 10));
}
