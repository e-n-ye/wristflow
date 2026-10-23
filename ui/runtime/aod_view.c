#include "aod_view.h"
#include "settings_view.h"
#include "wristflow_ui.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *named(lv_obj_t *root, const char *name)
{ lv_obj_t *obj = lv_obj_find_by_name(root, name); if (!obj) LV_LOG_ERROR("Missing AOD object: %s", name); LV_ASSERT(obj); return obj; }
static void text(lv_obj_t *obj, const char *value)
{ if (strcmp(lv_label_get_text(obj), value)) lv_label_set_text(obj, value); }
static void cancel(lv_event_t *event)
{ (void)event; wristflow_settings_dismiss(lv_screen_active()); }
static void open_aod_surface(lv_event_t *event)
{
    wristflow_ui_shell_open(lv_event_get_user_data(event),
        (wristflow_surface_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
}
static void route(lv_obj_t *obj, wristflow_surface_t surface, wristflow_ui_shell_t *shell)
{
    lv_obj_set_user_data(obj, (void *)(uintptr_t)surface);
    lv_obj_add_event_cb(obj, open_aod_surface, LV_EVENT_SHORT_CLICKED, shell);
}
static lv_obj_t *row(lv_obj_t *list, const char *name, const char *title, const char *detail)
{
    lv_obj_t *obj = settings_row_create(list, title, detail);
    lv_obj_set_name(obj, name);
    lv_obj_set_style_text_font(named(obj, "row_title"), notification_22, 0);
    lv_obj_set_style_text_font(named(obj, "row_detail"), notification_22, 0);
    return obj;
}
static lv_obj_t *note(lv_obj_t *list, const char *name, const char *message)
{
    lv_obj_t *obj = settings_note_create(list, message);
    lv_obj_set_name(obj, name);
    lv_obj_set_style_text_font(obj, notification_22, 0);
    return obj;
}

bool wristflow_aod_settings_surface(wristflow_surface_t surface)
{
    return surface == WRISTFLOW_SURFACE_AOD_SETTINGS || surface == WRISTFLOW_SURFACE_AOD_START ||
        surface == WRISTFLOW_SURFACE_AOD_END || surface == WRISTFLOW_SURFACE_AOD_INFO ||
        surface == WRISTFLOW_SURFACE_AOD_STYLES;
}

static void commit_mode(wristflow_ui_shell_t *shell, bool disable_wrist)
{
    wristflow_settings_t settings;
    if (!wristflow_ui_shell_get_settings(shell, &settings)) return;
    settings.aod_mode = WRISTFLOW_AOD_SCHEDULED;
    if (disable_wrist) settings.wrist_wake = false;
    wristflow_ui_shell_configure(shell, &settings);
    wristflow_settings_dismiss(lv_screen_active());
    wristflow_settings_refresh(lv_screen_active(), WRISTFLOW_SURFACE_AOD_SETTINGS, shell);
}
static void wrist_choice(lv_event_t *event)
{
    bool disable = (uintptr_t)lv_obj_get_user_data(lv_event_get_current_target_obj(event)) == 1;
    commit_mode(lv_event_get_user_data(event), disable);
}
static void accept_power(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    wristflow_settings_t settings;
    if (!wristflow_ui_shell_get_settings(shell, &settings)) return;
    wristflow_settings_dismiss(lv_screen_active());
    if (!settings.wrist_wake) { commit_mode(shell, false); return; }
    lv_obj_t *dialog = choice_confirm_create(lv_screen_active());
    lv_obj_set_name(dialog, "settings_confirm");
    lv_obj_set_user_data(named(dialog, "choice_first"), (void *)(uintptr_t)1);
    lv_obj_add_event_cb(named(dialog, "choice_first"), wrist_choice, LV_EVENT_SHORT_CLICKED, shell);
    lv_obj_add_event_cb(named(dialog, "choice_second"), wrist_choice, LV_EVENT_SHORT_CLICKED, shell);
    lv_obj_add_event_cb(named(dialog, "choice_back"), cancel, LV_EVENT_SHORT_CLICKED, shell);
    lv_obj_remove_flag(dialog, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
}
static void mode_choice(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    wristflow_settings_t settings;
    wristflow_ui_shell_get_settings(shell, &settings);
    if (!(uintptr_t)lv_obj_get_user_data(lv_event_get_current_target_obj(event))) {
        settings.aod_mode = WRISTFLOW_AOD_OFF;
        wristflow_ui_shell_configure(shell, &settings);
        wristflow_settings_refresh(lv_screen_active(), WRISTFLOW_SURFACE_AOD_SETTINGS, shell);
        return;
    }
    if (settings.aod_mode == WRISTFLOW_AOD_SCHEDULED ||
        lv_obj_find_by_name(lv_screen_active(), "settings_confirm")) return;
    lv_obj_t *dialog = component_confirm_create(lv_screen_active());
    lv_obj_set_name(dialog, "settings_confirm");
    lv_obj_set_style_text_font(named(dialog, "confirm_text"), notification_22, 0);
    text(named(dialog, "confirm_text"), "开启息屏显示会增加耗电，减少续航时间，确认开启？");
    lv_obj_add_event_cb(named(dialog, "confirm_cancel"), cancel, LV_EVENT_SHORT_CLICKED, shell);
    lv_obj_add_event_cb(named(dialog, "confirm_accept"), accept_power, LV_EVENT_SHORT_CLICKED, shell);
    lv_obj_remove_flag(dialog, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
}
static void time_accept(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    wristflow_settings_t settings;
    wristflow_ui_shell_get_settings(shell, &settings);
    unsigned minute = lv_roller_get_selected(named(lv_screen_active(), "time_hours")) * 60U +
        lv_roller_get_selected(named(lv_screen_active(), "time_minutes"));
    if (wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_AOD_START)
        settings.aod_start_minute = (uint16_t)minute;
    else settings.aod_end_minute = (uint16_t)minute;
    if (!wristflow_ui_shell_configure(shell, &settings)) {
        lv_obj_remove_flag(named(lv_screen_active(), "aod_time_error"), LV_OBJ_FLAG_HIDDEN);
        return;
    }
    wristflow_ui_shell_back(shell);
}
static void style_choice(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    wristflow_settings_t settings;
    wristflow_ui_shell_get_settings(shell, &settings);
    settings.aod_style = (wristflow_aod_style_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_current_target_obj(event));
    wristflow_ui_shell_configure(shell, &settings);
    wristflow_settings_refresh(lv_screen_active(), WRISTFLOW_SURFACE_AOD_STYLES, shell);
}
static void style_scroll(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    wristflow_aod_settings_refresh(lv_obj_get_screen(lv_event_get_current_target_obj(event)),
        WRISTFLOW_SURFACE_AOD_STYLES, shell);
}

lv_obj_t *wristflow_aod_settings_screen(wristflow_ui_shell_t *shell, wristflow_surface_t surface)
{
    if (!wristflow_aod_settings_surface(surface)) return NULL;
    wristflow_settings_t settings;
    wristflow_ui_shell_get_settings(shell, &settings);
    lv_obj_t *root;
    if (surface == WRISTFLOW_SURFACE_AOD_START || surface == WRISTFLOW_SURFACE_AOD_END) {
        root = screen_aod_time_create();
        bool start = surface == WRISTFLOW_SURFACE_AOD_START;
        unsigned minute = start ? settings.aod_start_minute : settings.aod_end_minute;
        text(named(root, "app_title"), start ? "开始时间" : "结束时间");
        lv_roller_set_selected(named(root, "time_hours"), minute / 60U, LV_ANIM_OFF);
        lv_roller_set_selected(named(root, "time_minutes"), minute % 60U, LV_ANIM_OFF);
        lv_obj_add_event_cb(named(root, "time_accept"), time_accept, LV_EVENT_SHORT_CLICKED, shell);
    } else if (surface == WRISTFLOW_SURFACE_AOD_STYLES) {
        root = screen_aod_styles_create();
        lv_obj_t *carousel = named(root, "aod_carousel");
        lv_obj_set_scroll_dir(carousel, LV_DIR_HOR);
        lv_obj_set_scroll_snap_x(carousel, LV_SCROLL_SNAP_CENTER);
        lv_obj_remove_flag(carousel, LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_SCROLL_MOMENTUM);
        lv_obj_remove_flag(named(root, "aod_carousel_pad"), LV_OBJ_FLAG_SNAPPABLE);
        for (unsigned i = 0; i < WRISTFLOW_AOD_STYLE_COUNT; ++i) {
            lv_obj_t *obj = named(root, i ? "aod_style_1" : "aod_style_0");
            lv_obj_set_user_data(obj, (void *)(uintptr_t)i);
            lv_obj_add_event_cb(obj, style_choice, LV_EVENT_SHORT_CLICKED, shell);
        }
        lv_obj_add_event_cb(carousel, style_scroll, LV_EVENT_SCROLL_END, shell);
        lv_obj_update_layout(root);
        lv_obj_scroll_to_x(carousel, settings.aod_style * 266, LV_ANIM_OFF);
    } else {
        root = screen_settings_list_create();
        lv_obj_t *list = named(root, "settings_scroll");
        lv_obj_remove_flag(list, LV_OBJ_FLAG_SCROLL_CHAIN);
        lv_obj_set_scroll_dir(list, LV_DIR_VER);
        text(named(root, "app_title"), surface == WRISTFLOW_SURFACE_AOD_INFO ? "说明" : "息屏显示");
        if (surface == WRISTFLOW_SURFACE_AOD_INFO) {
            note(list, "aod_info_text", "息屏显示\n手表休息时，屏幕仍显示时间等信息。\n\n定时开启\n仅在设定时段显示，支持跨夜。\n\n智能开启\n睡眠与离腕检测尚未接入，暂未开放。\n\n开启后会增加耗电，减少续航时间。");
        } else {
            static const char *names[] = {"关闭", "智能开启", "定时开启"};
            for (unsigned i = 0; i < 3; ++i) {
                lv_obj_t *obj = settings_choice_create(list, names[i]);
                lv_obj_set_name(obj, i == 0 ? "aod_off" : i == 1 ? "aod_smart" : "aod_scheduled");
                lv_obj_set_style_text_font(lv_obj_get_child(obj, 0), notification_22, 0);
                if (i == 1) {
                    lv_obj_add_state(obj, LV_STATE_DISABLED);
                    lv_obj_set_style_text_opa(lv_obj_get_child(obj, 0), LV_OPA_40, 0);
                }
                else {
                    lv_obj_set_user_data(obj, (void *)(uintptr_t)(i == 2));
                    lv_obj_add_event_cb(obj, mode_choice, LV_EVENT_SHORT_CLICKED, shell);
                }
            }
            route(row(list, "aod_start", "开始", "08:00"), WRISTFLOW_SURFACE_AOD_START, shell);
            route(row(list, "aod_end", "结束", "22:00"), WRISTFLOW_SURFACE_AOD_END, shell);
            note(list, "aod_clock_note", "");
            route(row(list, "aod_info", "说明", "智能开启暂未开放"), WRISTFLOW_SURFACE_AOD_INFO, shell);
        }
    }
    lv_obj_set_style_text_font(named(root, "app_title"), notification_22, 0);
    lv_obj_set_width(named(root, "app_title"), 230);
    wristflow_aod_settings_refresh(root, surface, shell);
    return root;
}

void wristflow_aod_settings_refresh(lv_obj_t *root, wristflow_surface_t surface, wristflow_ui_shell_t *shell)
{
    if (!root || !wristflow_aod_settings_surface(surface)) return;
    wristflow_settings_t settings;
    wristflow_ui_shell_get_settings(shell, &settings);
    lv_obj_t *clock = lv_obj_find_by_name(root, "settings_clock");
    if (clock) {
        const wristflow_watch_snapshot_t *snapshot = wristflow_ui_shell_snapshot(shell);
        char value[16];
        if (snapshot->time_unavailable) snprintf(value, sizeof value, "--:--");
        else snprintf(value, sizeof value, "%02u:%02u", snapshot->hour_24, snapshot->minute);
        text(clock, value);
    }
    if (surface == WRISTFLOW_SURFACE_AOD_SETTINGS) {
        bool enabled = settings.aod_mode == WRISTFLOW_AOD_SCHEDULED;
        lv_obj_set_flag(named(named(root, "aod_off"), "row_check"), LV_OBJ_FLAG_HIDDEN, enabled);
        lv_obj_add_flag(named(named(root, "aod_smart"), "row_check"), LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_flag(named(named(root, "aod_scheduled"), "row_check"), LV_OBJ_FLAG_HIDDEN, !enabled);
        lv_obj_set_flag(named(root, "aod_start"), LV_OBJ_FLAG_HIDDEN, !enabled);
        lv_obj_set_flag(named(root, "aod_end"), LV_OBJ_FLAG_HIDDEN, !enabled);
        char value[32];
        snprintf(value, sizeof value, "%02u:%02u", settings.aod_start_minute / 60U, settings.aod_start_minute % 60U);
        text(named(named(root, "aod_start"), "row_detail"), value);
        snprintf(value, sizeof value, "%s%02u:%02u", settings.aod_end_minute < settings.aod_start_minute ? "次日" : "",
            settings.aod_end_minute / 60U, settings.aod_end_minute % 60U);
        text(named(named(root, "aod_end"), "row_detail"), value);
        text(named(root, "aod_clock_note"), !enabled ? "" : wristflow_ui_shell_snapshot(shell)->time_unavailable ?
            "等待校时，定时息屏显示暂不生效。" : wristflow_settings_aod_active(&settings, wristflow_ui_shell_snapshot(shell)) ?
            "当前在息屏显示时段。" : "当前不在息屏显示时段。");
    } else if (surface == WRISTFLOW_SURFACE_AOD_STYLES) {
        unsigned current = (unsigned)LV_CLAMP(0, (lv_obj_get_scroll_x(named(root, "aod_carousel")) + 133) / 266, 1);
        char value[64];
        snprintf(value, sizeof value, "%s%s", current ? "左上数字" : "居中数字", current == (unsigned)settings.aod_style ? " · 已选" : " · 点击应用");
        text(named(root, "aod_style_name"), value);
    }
}

void wristflow_aod_render(lv_obj_t *content, const wristflow_watch_snapshot_t *snapshot,
    wristflow_aod_style_t style, unsigned displacement)
{
    lv_obj_t *center = named(content, "aod_center"), *corner = named(content, "aod_corner");
    lv_obj_set_flag(center, LV_OBJ_FLAG_HIDDEN, style != WRISTFLOW_AOD_CENTER);
    lv_obj_set_flag(corner, LV_OBJ_FLAG_HIDDEN, style != WRISTFLOW_AOD_CORNER);
    static const int8_t shifts[][2] = {{-2,-2}, {2,-2}, {2,2}, {-2,2}};
    lv_obj_set_pos(style == WRISTFLOW_AOD_CENTER ? center : corner,
        shifts[displacement % 4][0], shifts[displacement % 4][1]);
    char value[64];
    if (snapshot->time_unavailable) snprintf(value, sizeof value, "--:--");
    else snprintf(value, sizeof value, "%02u:%02u", snapshot->hour_24, snapshot->minute);
    lv_obj_t *clock = named(content, style == WRISTFLOW_AOD_CENTER ? "aod_center_time" : "aod_corner_time");
    lv_obj_set_style_text_font(clock, snapshot->time_unavailable || style == WRISTFLOW_AOD_CORNER ? metric_56 : clock_80, 0);
    text(clock, value);
    snprintf(value, sizeof value, "--/--");
    text(named(content, "aod_corner_weekday"), "");
    if (!snapshot->time_unavailable && snapshot->utc_seconds >= WRISTFLOW_TIME_MIN && snapshot->utc_seconds <= WRISTFLOW_TIME_MAX) {
        unsigned days = (unsigned)(((uint64_t)snapshot->utc_seconds + 28800U) / 86400U);
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
        if (style == WRISTFLOW_AOD_CENTER)
            snprintf(value, sizeof value, "%u月%u日 %s", month, remaining + 1, week[days % 7]);
        else snprintf(value, sizeof value, "%u/%u", month, remaining + 1);
        text(named(content, "aod_corner_weekday"), week[days % 7]);
    }
    lv_obj_t *date = named(content, style == WRISTFLOW_AOD_CENTER ? "aod_center_date" : "aod_corner_date");
    lv_obj_set_style_text_font(date, style == WRISTFLOW_AOD_CENTER ? notification_22 : metric_56, 0);
    if (snapshot->time_unavailable) text(named(content, "aod_corner_weekday"), "");
    text(date, value);
    if (snapshot->battery_unavailable) snprintf(value, sizeof value, "--%%");
    else snprintf(value, sizeof value, "%s%u%%", snapshot->power.estimate_valid ? "~" : "", snapshot->battery_percent);
    text(named(content, "aod_corner_battery"), value);
}
