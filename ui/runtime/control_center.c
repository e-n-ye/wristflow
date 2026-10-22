#include "control_center.h"
#include "wristflow_ui.h"
#include <string.h>
#include <stdio.h>

struct wristflow_control_center {
    lv_obj_t *root, *toast;
    lv_timer_t *toast_timer;
    wristflow_ui_shell_t *shell;
};

static lv_obj_t *named(lv_obj_t *root, const char *name)
{ lv_obj_t *obj = lv_obj_find_by_name(root, name); LV_ASSERT(obj); return obj; }

static void label(lv_obj_t *obj, const char *text)
{ if (strcmp(lv_label_get_text(obj), text)) lv_label_set_text(obj, text); }

static void hide_toast(lv_timer_t *timer)
{
    wristflow_control_center_t *view = lv_timer_get_user_data(timer);
    lv_obj_add_flag(view->toast, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(timer);
}

static void toast(wristflow_control_center_t *view, const char *text)
{
    if (wristflow_ui_shell_navigation(view->shell)->surface != WRISTFLOW_SURFACE_CONTROLS ||
        wristflow_ui_shell_reminder_active(view->shell) || wristflow_ui_shell_charging_active(view->shell)) return;
    label(named(view->toast, "toast_text"), text);
    lv_obj_remove_flag(view->toast, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(view->toast);
    lv_timer_reset(view->toast_timer);
    lv_timer_resume(view->toast_timer);
}

void wristflow_control_center_leave(wristflow_control_center_t *view)
{ if (view) hide_toast(view->toast_timer); }

static void cancel_endurance(lv_event_t *event)
{
    wristflow_control_center_t *view = lv_event_get_user_data(event);
    lv_obj_t *dialog = lv_obj_find_by_name(view->root, "settings_confirm");
    if (dialog) lv_obj_delete(dialog);
}

static void confirm_endurance(lv_event_t *event)
{
    wristflow_control_center_t *view = lv_event_get_user_data(event);
    cancel_endurance(event);
    toast(view, "长续航模式待实现");
}

static void endurance_prompt(wristflow_control_center_t *view)
{
    if (lv_obj_find_by_name(view->root, "settings_confirm")) return;
    wristflow_control_center_leave(view);
    lv_obj_t *dialog = component_confirm_create(view->root);
    lv_obj_set_name(dialog, "settings_confirm");
    lv_label_set_text(named(dialog, "confirm_text"), "是否进入长续航模式？\n功能待实现，本次不会切换模式。");
    lv_obj_remove_flag(dialog, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(named(dialog, "confirm_cancel"), cancel_endurance, LV_EVENT_SHORT_CLICKED, view);
    lv_obj_add_event_cb(named(dialog, "confirm_accept"), confirm_endurance, LV_EVENT_SHORT_CLICKED, view);
}

static void action(lv_event_t *event)
{
    wristflow_control_center_t *view = lv_event_get_user_data(event);
    if (wristflow_ui_shell_navigation(view->shell)->surface != WRISTFLOW_SURFACE_CONTROLS ||
        wristflow_ui_shell_reminder_active(view->shell) || wristflow_ui_shell_charging_active(view->shell) ||
        lv_obj_find_by_name(view->root, "settings_confirm")) return;
    lv_obj_t *target = lv_event_get_current_target_obj(event);
    const char *name = lv_obj_get_name(target);
    bool long_press = lv_event_get_code(event) == LV_EVENT_LONG_PRESSED;
    wristflow_surface_t destination = WRISTFLOW_SURFACE_COUNT;
    wristflow_settings_t settings;
    if (!wristflow_ui_shell_get_settings(view->shell, &settings)) return;
    if (!strcmp(name, "settings_button")) destination = WRISTFLOW_SURFACE_SETTINGS;
    else if (!strcmp(name, "flashlight_button")) destination = WRISTFLOW_SURFACE_FLASHLIGHT;
    else if (!strcmp(name, "dnd_button")) {
        if (long_press) destination = WRISTFLOW_SURFACE_DND_SETTINGS;
        else {
            bool enabled = !wristflow_ui_shell_dnd(view->shell);
            wristflow_ui_shell_set_dnd(view->shell, enabled);
            toast(view, enabled ? "勿扰模式已开启" : "勿扰模式已关闭");
        }
    } else if (!strcmp(name, "silent_button")) {
        if (long_press) destination = WRISTFLOW_SURFACE_SOUND_SETTINGS;
        else {
            settings.silent_mode = !settings.silent_mode;
            if (wristflow_ui_shell_configure(view->shell, &settings))
                toast(view, settings.silent_mode ? "静音模式已开启" : "静音模式已关闭");
        }
    } else if (!strcmp(name, "wrist_button")) {
        if (long_press) destination = WRISTFLOW_SURFACE_WAKE_SETTINGS;
        else {
            settings.wrist_wake = !settings.wrist_wake;
            if (wristflow_ui_shell_configure(view->shell, &settings))
                toast(view, settings.wrist_wake ? "抬腕亮屏已开启" : "抬腕亮屏已关闭");
        }
    } else if (!strcmp(name, "keep_awake_button")) {
        if (long_press) destination = WRISTFLOW_SURFACE_KEEP_AWAKE;
        else {
            bool enabled = wristflow_ui_shell_keep_minutes(view->shell) == 0;
            if (wristflow_ui_shell_keep_awake(view->shell, enabled ? 5 : 0))
                toast(view, enabled ? "持续亮屏5分钟" : "持续亮屏已关闭");
        }
    } else if (!strcmp(name, "endurance_button")) {
        if (long_press) destination = WRISTFLOW_SURFACE_ENDURANCE_SETTINGS;
        else endurance_prompt(view);
    }
    if (destination != WRISTFLOW_SURFACE_COUNT)
        wristflow_ui_shell_open(view->shell, destination);
    wristflow_control_center_update(view, wristflow_ui_shell_snapshot(view->shell));
}

wristflow_control_center_t *wristflow_control_center_create(lv_obj_t *root,
    wristflow_ui_shell_t *shell, bool product_mode)
{
    wristflow_control_center_t *view = lv_malloc_zeroed(sizeof *view);
    LV_ASSERT_MALLOC(view);
    view->root = root; view->shell = shell;
    view->toast = status_toast_create(root);
    lv_obj_set_name(view->toast, "control_toast");
    view->toast_timer = lv_timer_create(hide_toast, 1800, view);
    lv_timer_pause(view->toast_timer);
    static const char *names[] = {"dnd_button", "silent_button", "wrist_button", "settings_button",
        "flashlight_button", "endurance_button", "keep_awake_button"};
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; ++i) {
        lv_obj_t *button = named(root, names[i]);
        if (!product_mode && strcmp(names[i], "settings_button") && strcmp(names[i], "flashlight_button")) {
            lv_obj_add_state(button, LV_STATE_DISABLED);
            continue;
        }
        lv_obj_add_event_cb(button, action, LV_EVENT_SHORT_CLICKED, view);
        lv_obj_add_event_cb(button, action, LV_EVENT_LONG_PRESSED, view);
    }
    return view;
}

void wristflow_control_center_update(wristflow_control_center_t *view, const wristflow_watch_snapshot_t *snapshot)
{
    if (!view || !snapshot) return;
    if (wristflow_ui_shell_reminder_active(view->shell) || wristflow_ui_shell_charging_active(view->shell) ||
        wristflow_ui_shell_display_phase(view->shell) == WRISTFLOW_DISPLAY_OFF)
        wristflow_control_center_leave(view);
    char text[24];
    if (snapshot->battery_unavailable) snprintf(text, sizeof text, "--");
    else snprintf(text, sizeof text, "%s%u%%", snapshot->power.estimate_valid ? "~" : "", snapshot->battery_percent);
    label(named(view->root, "control_battery_value"), text);
    if (snapshot->time_unavailable) snprintf(text, sizeof text, "--:--");
    else snprintf(text, sizeof text, "%02u:%02u", snapshot->hour_24, snapshot->minute);
    label(named(view->root, "control_clock"), text);
    lv_obj_set_style_text_color(named(view->root, "connection_status"),
        lv_color_hex(wristflow_ui_shell_phone_connected(view->shell) ? 0x55baff : 0x64748b), 0);
    wristflow_settings_t settings;
    if (!wristflow_ui_shell_get_settings(view->shell, &settings)) return;
    const char *names[] = {"dnd_button", "silent_button", "wrist_button", "keep_awake_button"};
    bool active[] = {wristflow_ui_shell_dnd(view->shell), settings.silent_mode,
        settings.wrist_wake, wristflow_ui_shell_keep_minutes(view->shell) != 0};
    for (unsigned i = 0; i < 4; ++i)
        lv_obj_set_style_bg_color(named(view->root, names[i]), lv_color_hex(active[i] ? 0x008cff : 0x183453), 0);
}

void wristflow_control_center_destroy(wristflow_control_center_t *view)
{
    if (!view) return;
    lv_timer_delete(view->toast_timer);
    lv_free(view);
}
