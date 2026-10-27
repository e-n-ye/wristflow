#include "settings_view.h"
#include "aod_view.h"
#include "wristflow_ui.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *named(lv_obj_t *root, const char *name)
{ lv_obj_t *obj = lv_obj_find_by_name(root, name); LV_ASSERT(obj); return obj; }

static void label(lv_obj_t *obj, const char *value)
{ if (strcmp(lv_label_get_text(obj), value)) lv_label_set_text(obj, value); }

static void open_page(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_current_target_obj(event);
    wristflow_ui_shell_open(lv_event_get_user_data(event), (wristflow_surface_t)(uintptr_t)lv_obj_get_user_data(target));
}

static void route(lv_obj_t *obj, wristflow_surface_t surface, wristflow_ui_shell_t *shell)
{
    lv_obj_set_user_data(obj, (void *)(uintptr_t)surface);
    lv_obj_add_event_cb(obj, open_page, LV_EVENT_SHORT_CLICKED, shell);
}

static lv_obj_t *row(lv_obj_t *list, const char *name, const char *title, const char *detail)
{
    lv_obj_t *obj = settings_row_create(list, title, detail);
    lv_obj_set_name(obj, name);
    return obj;
}

bool wristflow_settings_dismiss(lv_obj_t *root)
{
    lv_obj_t *dialog = lv_obj_find_by_name(root, "settings_confirm");
    if (!dialog) return false;
    lv_obj_delete(dialog);
    return true;
}

static void cancel(lv_event_t *event)
{ (void)event; wristflow_settings_dismiss(lv_screen_active()); }

static void accept_settings_confirmation(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    lv_obj_t *root = lv_screen_active();
    unsigned value = (unsigned)(uintptr_t)lv_obj_get_user_data(named(root, "settings_confirm"));
    if (value >= 1000) wristflow_ui_shell_keep_awake(shell, value - 1000);
    else {
        wristflow_settings_t settings;
        wristflow_ui_shell_get_settings(shell, &settings);
        settings.screen_timeout = (uint8_t)value;
        wristflow_ui_shell_configure(shell, &settings);
    }
    wristflow_settings_dismiss(root);
    wristflow_settings_refresh(root, wristflow_ui_shell_navigation(shell)->surface, shell);
}

static void confirm(lv_obj_t *root, wristflow_ui_shell_t *shell, unsigned value)
{
    if (lv_obj_find_by_name(root, "settings_confirm")) return;
    lv_obj_t *dialog = component_confirm_create(root);
    lv_obj_set_name(dialog, "settings_confirm");
    lv_obj_set_user_data(dialog, (void *)(uintptr_t)value);
    char message[180];
    if (value >= 1000) snprintf(message, sizeof message,
        "本次持续亮屏%u分钟，会减少续航时间，确定使用？", value - 1000);
    else snprintf(message, sizeof message, "亮屏时间过长，会减少续航时间，确定使用？");
    lv_label_set_text(named(dialog, "confirm_text"), message);
    lv_obj_add_event_cb(named(dialog, "confirm_cancel"), cancel, LV_EVENT_SHORT_CLICKED, shell);
    lv_obj_add_event_cb(named(dialog, "confirm_accept"), accept_settings_confirmation, LV_EVENT_SHORT_CLICKED, shell);
    /* Modal stops gestures reaching the underlying settings list. KEY1 remains Home. */
    lv_obj_remove_flag(dialog, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
}

void wristflow_settings_keep_prompt(lv_obj_t *root, wristflow_ui_shell_t *shell)
{
    if (wristflow_ui_shell_keep_minutes(shell)) wristflow_ui_shell_keep_awake(shell, 0);
    else confirm(root, shell, 1005);
}

static void choose(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    unsigned value = (unsigned)(uintptr_t)lv_obj_get_user_data(lv_event_get_current_target_obj(event));
    wristflow_surface_t surface = wristflow_ui_shell_navigation(shell)->surface;
    if (surface == WRISTFLOW_SURFACE_KEEP_AWAKE) {
        if (value) confirm(lv_screen_active(), shell, 1000 + value);
        else wristflow_ui_shell_keep_awake(shell, 0);
    } else if (surface == WRISTFLOW_SURFACE_VIBRATION_SETTINGS)
        wristflow_ui_shell_set_vibration_mode(shell, (wristflow_vibration_mode_t)value);
    else if (value == 60) confirm(lv_screen_active(), shell, value);
    else {
        wristflow_settings_t settings;
        wristflow_ui_shell_get_settings(shell, &settings);
        settings.screen_timeout = (uint8_t)value;
        wristflow_ui_shell_configure(shell, &settings);
    }
    wristflow_settings_refresh(lv_screen_active(), surface, shell);
}

static void long_press_changed(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    wristflow_settings_t settings;
    wristflow_ui_shell_get_settings(shell, &settings);
    settings.face_long_press = lv_obj_has_state(lv_event_get_current_target_obj(event), LV_STATE_CHECKED);
    wristflow_ui_shell_configure(shell, &settings);
}

static void long_press_row(lv_event_t *event)
{
    lv_obj_t *toggle = named(lv_event_get_current_target_obj(event), "row_switch");
    if (lv_event_get_target_obj(event) == toggle) return;
    lv_obj_set_state(toggle, LV_STATE_CHECKED, !lv_obj_has_state(toggle, LV_STATE_CHECKED));
    lv_obj_send_event(toggle, LV_EVENT_VALUE_CHANGED, NULL);
}

static void silent_changed(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    wristflow_settings_t settings;
    wristflow_ui_shell_get_settings(shell, &settings);
    settings.silent_mode = lv_obj_has_state(lv_event_get_current_target_obj(event), LV_STATE_CHECKED);
    wristflow_ui_shell_configure(shell, &settings);
}

static void dnd_choice(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    wristflow_settings_t settings;
    wristflow_ui_shell_get_settings(shell, &settings);
    settings.dnd_mode = (wristflow_dnd_mode_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_current_target_obj(event));
    if (!wristflow_ui_shell_configure(shell, &settings)) return;
    wristflow_settings_refresh(lv_screen_active(), WRISTFLOW_SURFACE_DND_SETTINGS, shell);
}

static void dnd_time_accept(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    lv_obj_t *root = lv_screen_active();
    wristflow_settings_t settings;
    wristflow_ui_shell_get_settings(shell, &settings);
    unsigned minute = lv_roller_get_selected(named(root, "time_hours")) * 60U +
        lv_roller_get_selected(named(root, "time_minutes"));
    if (wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_DND_START)
        settings.dnd_start_minute = (uint16_t)minute;
    else settings.dnd_end_minute = (uint16_t)minute;
    if (settings.dnd_start_minute == settings.dnd_end_minute || !wristflow_ui_shell_configure(shell, &settings)) {
        lv_obj_remove_flag(named(root, "dnd_time_error"), LV_OBJ_FLAG_HIDDEN);
        return;
    }
    wristflow_ui_shell_back(shell);
}

static void wrist_changed(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    wristflow_settings_t settings;
    wristflow_ui_shell_get_settings(shell, &settings);
    settings.wrist_wake = lv_obj_has_state(lv_event_get_current_target_obj(event), LV_STATE_CHECKED);
    wristflow_ui_shell_configure(shell, &settings);
}

static void auto_brightness_changed(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    bool automatic = lv_obj_has_state(lv_event_get_current_target_obj(event), LV_STATE_CHECKED);
    wristflow_ui_shell_request_auto_brightness(shell, automatic);
}

static void notification_center(lv_event_t *event)
{ wristflow_ui_shell_open_notification(lv_event_get_user_data(event), false, 0); }

static void unavailable(lv_obj_t *list, const char *name, const char *title, const char *detail)
{ lv_obj_add_state(row(list, name, title, detail), LV_STATE_DISABLED); }

static void enable_endurance(lv_event_t *event)
{ wristflow_ui_shell_endurance_prompt(lv_event_get_user_data(event)); }
static void optimize_wrist(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    wristflow_settings_t settings; wristflow_ui_shell_get_settings(shell, &settings);
    settings.wrist_wake = false;
    wristflow_ui_shell_configure(shell, &settings);
    wristflow_settings_refresh(lv_screen_active(), WRISTFLOW_SURFACE_BATTERY_OPTIMIZATION, shell);
}
static void optimize_awake(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    wristflow_ui_shell_keep_awake(shell, 0);
    wristflow_settings_refresh(lv_screen_active(), WRISTFLOW_SURFACE_BATTERY_OPTIMIZATION, shell);
}
void wristflow_settings_battery_bind(lv_obj_t *root, wristflow_ui_shell_t *shell)
{
    lv_obj_t *scroll = named(root, "battery_scroll");
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
    lv_obj_remove_flag(scroll, LV_OBJ_FLAG_SCROLL_CHAIN);
    route(named(root, "battery_endurance"), WRISTFLOW_SURFACE_ENDURANCE_SETTINGS, shell);
    route(named(root, "battery_optimize"), WRISTFLOW_SURFACE_BATTERY_OPTIMIZATION, shell);
    route(named(root, "battery_info"), WRISTFLOW_SURFACE_BATTERY_INFO, shell);
}

static const unsigned timeouts[] = {5, 10, 20, 30, 60};
static const unsigned awake_times[] = {0, 5, 10, 15, 20};

static void retry_save(lv_event_t *event)
{ wristflow_ui_shell_retry_settings_save(lv_event_get_user_data(event)); }

static void refresh_save_note(lv_obj_t *root, wristflow_ui_shell_t *shell)
{
    if (!wristflow_ui_shell_has_settings_store(shell)) return;
    lv_obj_t *list = lv_obj_find_by_name(root, "settings_scroll");
    if (!list) return;
    lv_obj_t *note = lv_obj_find_by_name(list, "settings_save_note");
    if (!note) {
        note = settings_note_create(list, "");
        lv_obj_set_style_text_font(note, notification_22, 0); /* Shared common-Chinese font. */
        lv_obj_set_name(note, "settings_save_note");
        lv_obj_move_to_index(note, 0);
        lv_obj_add_flag(note, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(note, retry_save, LV_EVENT_SHORT_CLICKED, shell);
    }
    wristflow_save_state_t state = wristflow_ui_shell_settings_save_state(shell);
    label(note, state == WRISTFLOW_SAVE_PENDING ? "保存中" :
        state == WRISTFLOW_SAVE_DONE ? "已保存" : state == WRISTFLOW_SAVE_FAILED ? "保存失败，点击重试" : "");
    lv_obj_set_flag(note, LV_OBJ_FLAG_HIDDEN, state == WRISTFLOW_SAVE_IDLE);
}

lv_obj_t *wristflow_settings_screen(wristflow_ui_shell_t *shell, wristflow_surface_t surface)
{
    if (wristflow_aod_settings_surface(surface)) return wristflow_aod_settings_screen(shell, surface);
    if (surface == WRISTFLOW_SURFACE_SETTINGS) {
        lv_obj_t *root = screen_product_settings_create();
        static const struct { const char *name; wristflow_surface_t surface; } destinations[] = {
            {"settings_faces", WRISTFLOW_SURFACE_FACE_MANAGEMENT}, {"settings_display", WRISTFLOW_SURFACE_DISPLAY_SETTINGS},
            {"settings_sound", WRISTFLOW_SURFACE_SOUND_SETTINGS}, {"settings_dnd", WRISTFLOW_SURFACE_DND_SETTINGS},
            {"settings_notifications", WRISTFLOW_SURFACE_NOTIFICATION_SETTINGS}, {"settings_keys", WRISTFLOW_SURFACE_KEY_SETTINGS},
            {"settings_layout", WRISTFLOW_SURFACE_MENU_LAYOUT}, {"settings_battery", WRISTFLOW_SURFACE_SYSTEM},
            {"settings_system", WRISTFLOW_SURFACE_SYSTEM_SETTINGS}, {"settings_about", WRISTFLOW_SURFACE_ABOUT}};
        for (unsigned i = 0; i < sizeof destinations / sizeof destinations[0]; ++i)
            route(named(root, destinations[i].name), destinations[i].surface, shell);
        lv_obj_remove_flag(named(root, "settings_scroll"), LV_OBJ_FLAG_SCROLL_CHAIN);
        lv_obj_set_scroll_dir(named(root, "settings_scroll"), LV_DIR_VER);
        return root;
    }
    if (surface == WRISTFLOW_SURFACE_BRIGHTNESS_ADJUST) return screen_brightness_adjust_create();
    if (surface == WRISTFLOW_SURFACE_DND_START || surface == WRISTFLOW_SURFACE_DND_END) {
        lv_obj_t *root = screen_dnd_time_create();
        wristflow_settings_t settings;
        wristflow_ui_shell_get_settings(shell, &settings);
        bool start = surface == WRISTFLOW_SURFACE_DND_START;
        unsigned minute = start ? settings.dnd_start_minute : settings.dnd_end_minute;
        lv_label_set_text(named(root, "app_title"), start ? "开始时间" : "结束时间");
        lv_roller_set_selected(named(root, "time_hours"), minute / 60U, LV_ANIM_OFF);
        lv_roller_set_selected(named(root, "time_minutes"), minute % 60U, LV_ANIM_OFF);
        lv_obj_add_event_cb(named(root, "time_accept"), dnd_time_accept, LV_EVENT_SHORT_CLICKED, shell);
        return root;
    }
    if ((surface < WRISTFLOW_SURFACE_FACE_MANAGEMENT || surface > WRISTFLOW_SURFACE_WAKE_SETTINGS) &&
        surface != WRISTFLOW_SURFACE_VIBRATION_SETTINGS && surface != WRISTFLOW_SURFACE_ENDURANCE_SETTINGS &&
        surface != WRISTFLOW_SURFACE_BATTERY_OPTIMIZATION && surface != WRISTFLOW_SURFACE_BATTERY_INFO) return NULL;
    lv_obj_t *root = screen_settings_list_create();
    lv_obj_t *list = named(root, "settings_scroll");
    lv_obj_remove_flag(list, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    const char *title = "设置";
    lv_obj_t *obj;
    switch (surface) {
    case WRISTFLOW_SURFACE_FACE_MANAGEMENT:
        title = "表盘管理";
        obj = row(list, "face_hold_row", "长按表盘", "允许更换表盘");
        lv_obj_add_event_cb(obj, long_press_row, LV_EVENT_SHORT_CLICKED, shell);
        obj = named(obj, "row_switch");
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(obj, long_press_changed, LV_EVENT_VALUE_CHANGED, shell);
        settings_note_create(list, "开启后可长按表盘进行更换。");
        route(row(list, "face_change", "表盘更换", "扩散 / 简洁"), WRISTFLOW_SURFACE_FACE_PICKER, shell);
        break;
    case WRISTFLOW_SURFACE_DISPLAY_SETTINGS:
        title = "显示与亮度";
        route(row(list, "display_brightness", "屏幕亮度", "自定义"), WRISTFLOW_SURFACE_BRIGHTNESS_SETTINGS, shell);
        route(row(list, "display_timeout", "熄屏时间", "10秒"), WRISTFLOW_SURFACE_SCREEN_TIMEOUT, shell);
        route(row(list, "display_wrist", "抬腕亮屏", "关闭"), WRISTFLOW_SURFACE_WAKE_SETTINGS, shell);
        route(row(list, "display_keep", "持续亮屏", "关闭"), WRISTFLOW_SURFACE_KEEP_AWAKE, shell);
        obj = row(list, "display_aod", "息屏显示", "关闭");
        lv_obj_set_style_text_font(named(obj, "row_title"), notification_22, 0);
        route(obj, WRISTFLOW_SURFACE_AOD_SETTINGS, shell);
        obj = row(list, "display_aod_styles", "默认息屏样式", "居中数字");
        lv_obj_set_style_text_font(named(obj, "row_title"), notification_22, 0);
        lv_obj_set_style_text_font(named(obj, "row_detail"), notification_22, 0);
        route(obj, WRISTFLOW_SURFACE_AOD_STYLES, shell);
        obj = settings_note_create(list, "开启后使用所选的默认息屏样式。");
        lv_obj_set_style_text_font(obj, notification_22, 0);
        break;
    case WRISTFLOW_SURFACE_BRIGHTNESS_SETTINGS:
        title = "屏幕亮度";
        obj = settings_choice_create(list, "自动亮度");
        lv_obj_set_name(obj, "auto_brightness");
        lv_obj_add_event_cb(obj, long_press_row, LV_EVENT_SHORT_CLICKED, shell);
        obj = settings_switch_create(obj);
        lv_obj_set_name(obj, "row_switch");
        lv_obj_add_event_cb(obj, auto_brightness_changed, LV_EVENT_VALUE_CHANGED, shell);
        obj = settings_choice_create(list, "自定义亮度");
        lv_obj_set_name(obj, "custom_brightness");
        route(obj, WRISTFLOW_SURFACE_BRIGHTNESS_ADJUST, shell);
        obj = settings_note_create(list, "自动亮度暂不可用");
        lv_obj_set_name(obj, "auto_brightness_unavailable");
        lv_obj_set_style_text_font(obj, notification_22, 0);
        break;
    case WRISTFLOW_SURFACE_SCREEN_TIMEOUT:
    case WRISTFLOW_SURFACE_KEEP_AWAKE:
        title = surface == WRISTFLOW_SURFACE_SCREEN_TIMEOUT ? "熄屏时间" : "持续亮屏";
        for (unsigned i = 0; i < 5; ++i) {
            unsigned value = surface == WRISTFLOW_SURFACE_SCREEN_TIMEOUT ? timeouts[i] : awake_times[i];
            char text[32], name[32];
            if (!value) snprintf(text, sizeof text, "关闭");
            else snprintf(text, sizeof text, "%u%s", value, surface == WRISTFLOW_SURFACE_SCREEN_TIMEOUT ? "秒" : "分钟");
            snprintf(name, sizeof name, "choice_%u", value);
            obj = settings_choice_create(list, text); lv_obj_set_name(obj, name);
            lv_obj_set_user_data(obj, (void *)(uintptr_t)value);
            lv_obj_add_event_cb(obj, choose, LV_EVENT_SHORT_CLICKED, shell);
        }
        if (surface == WRISTFLOW_SURFACE_KEEP_AWAKE) settings_note_create(list, "仅本次生效，到时恢复自动熄屏。");
        break;
    case WRISTFLOW_SURFACE_SOUND_SETTINGS:
        title = "声音与振动";
        obj = row(list, "silent_row", "静音模式", "");
        lv_obj_add_event_cb(obj, long_press_row, LV_EVENT_SHORT_CLICKED, shell);
        obj = named(obj, "row_switch"); lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(obj, silent_changed, LV_EVENT_VALUE_CHANGED, shell);
        unavailable(list, "sound_disabled", "声音提醒", "暂未开放");
        route(row(list, "vibration_mode", "振动模式", "标准"), WRISTFLOW_SURFACE_VIBRATION_SETTINGS, shell);
        obj = settings_note_create(list, wristflow_ui_shell_has_reminder_vibration(shell) ?
            "静音偏好可保存；音频待接入。静音不关闭振动，闹钟和倒计时声音独立。当前提醒按振动模式执行。" :
            "静音偏好可保存，音频待接入。提醒仅通过屏幕显示。");
        lv_obj_set_style_text_font(obj, notification_22, 0);
        break;
    case WRISTFLOW_SURFACE_VIBRATION_SETTINGS: {
        title = "振动模式";
        static const char *names[] = {"关闭", "标准", "增强"};
        for (unsigned i = 0; i < WRISTFLOW_VIBRATION_COUNT; ++i) {
            char name[32]; snprintf(name, sizeof name, "vibration_%u", i);
            obj = settings_choice_create(list, names[i]); lv_obj_set_name(obj, name);
            lv_obj_set_style_text_font(lv_obj_get_child(obj, 0), notification_22, 0);
            lv_obj_set_user_data(obj, (void *)(uintptr_t)i);
            lv_obj_add_event_cb(obj, choose, LV_EVENT_SHORT_CLICKED, shell);
        }
        obj = settings_note_create(list, wristflow_ui_shell_has_vibration_preview(shell) ?
            (wristflow_ui_shell_has_reminder_vibration(shell) ? "改选后体验振动；闹钟与倒计时使用此模式。" :
            "改选后体验振动；增强通过节奏更明显。应用提醒待接入。") :
            "当前仅保存设置，振动体验暂不可用。应用提醒待接入。");
        lv_obj_set_style_text_font(obj, notification_22, 0);
        break;
    }
    case WRISTFLOW_SURFACE_DND_SETTINGS:
        title = "勿扰模式";
        for (unsigned i = 0; i < WRISTFLOW_DND_COUNT; ++i) {
            static const char *names[] = {"关闭", "全天开启", "定时开启"};
            char name[24]; snprintf(name, sizeof name, "dnd_mode_%u", i);
            obj = settings_choice_create(list, names[i]); lv_obj_set_name(obj, name);
            lv_obj_set_user_data(obj, (void *)(uintptr_t)i);
            lv_obj_add_event_cb(obj, dnd_choice, LV_EVENT_SHORT_CLICKED, shell);
        }
        route(row(list, "dnd_start", "开始", "22:00"), WRISTFLOW_SURFACE_DND_START, shell);
        route(row(list, "dnd_end", "结束", "次日08:00"), WRISTFLOW_SURFACE_DND_END, shell);
        obj = settings_note_create(list, ""); lv_obj_set_name(obj, "dnd_clock_note");
        lv_obj_set_style_text_font(obj, notification_22, 0);
        obj = settings_note_create(list, "开启后消息不亮屏、不显示预览。闹钟和倒计时不受影响。");
        lv_obj_set_style_text_font(obj, notification_22, 0);
        break;
    case WRISTFLOW_SURFACE_ENDURANCE_SETTINGS:
        title = "长续航模式";
        obj = row(list, "endurance_enable", "长续航模式", "开启前确认");
        lv_obj_set_style_text_font(named(obj, "row_title"), notification_22, 0);
        lv_obj_set_style_text_font(named(obj, "row_detail"), notification_22, 0);
        lv_obj_remove_flag(named(obj, "row_switch"), LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(named(obj, "row_switch"), LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(obj, enable_endurance, LV_EVENT_SHORT_CLICKED, shell);
        obj = settings_note_create(list, "开启后保留时间、计步、闹钟与倒计时。暂停蓝牙、抬腕和临时常亮。按住按键3秒或新接USB退出。重启后仍保持此模式。");
        lv_obj_set_style_text_font(obj, notification_22, 0);
        break;
    case WRISTFLOW_SURFACE_BATTERY_OPTIMIZATION:
        title = "续航优化";
        obj = row(list, "optimize_wrist", "抬腕亮屏", "点击关闭");
        lv_obj_set_style_text_font(named(obj, "row_detail"), notification_22, 0);
        lv_obj_add_event_cb(obj, optimize_wrist, LV_EVENT_SHORT_CLICKED, shell);
        obj = row(list, "optimize_awake", "持续亮屏", "点击关闭");
        lv_obj_set_style_text_font(named(obj, "row_detail"), notification_22, 0);
        lv_obj_add_event_cb(obj, optimize_awake, LV_EVENT_SHORT_CLICKED, shell);
        obj = settings_note_create(list, ""); lv_obj_set_name(obj, "optimize_note");
        lv_obj_set_style_text_font(obj, notification_22, 0);
        break;
    case WRISTFLOW_SURFACE_BATTERY_INFO:
        title = "说明";
        obj = settings_note_create(list, "电量为电压估算，仅供参考。尚无本电池可用时长与节省小时数的测量依据。\n\n续航优化只显示当前已开启的相关功能，可逐项关闭；心率与血氧后台尚未接入。\n\n长续航模式保留已有提醒，暂停蓝牙连接和广播，不代表蓝牙芯片已断电。实际省电效果需独立测量。");
        lv_obj_set_style_text_font(obj, notification_22, 0);
        break;
    case WRISTFLOW_SURFACE_NOTIFICATION_SETTINGS:
        title = "消息通知";
        obj = row(list, "notifications_open", "消息通知", "查看全部消息");
        lv_obj_add_event_cb(obj, notification_center, LV_EVENT_SHORT_CLICKED, shell);
        route(row(list, "notifications_dnd", "勿扰模式", "关闭"), WRISTFLOW_SURFACE_DND_SETTINGS, shell);
        obj = settings_note_create(list, "最多保留10条消息。在手表删除或清空，仅移除本地记录，不影响手机通知。");
        lv_obj_set_style_text_font(obj, notification_22, 0);
        break;
    case WRISTFLOW_SURFACE_KEY_SETTINGS:
        title = "按键设置";
        settings_note_create(list, "短按按键可打开应用列表或返回表盘。变暗或熄屏时，首次短按仅亮屏。");
        unavailable(list, "assistant_disabled", "长按唤出助手", "暂未开放"); break;
    case WRISTFLOW_SURFACE_SYSTEM_SETTINGS:
        title = "系统操作";
        settings_note_create(list, "以下系统操作暂未开放。");
        unavailable(list, "restart_disabled", "重启", "暂未开放");
        unavailable(list, "shutdown_disabled", "关机", "暂未开放");
        unavailable(list, "phone_disabled", "连接新手机", "暂未开放");
        unavailable(list, "reset_disabled", "恢复出厂", "暂未开放"); break;
    case WRISTFLOW_SURFACE_ABOUT:
        title = "关于";
        row(list, "about_product", "WristFlow", "Product / 消息通知");
        row(list, "about_firmware", "固件版本", "2026.09.27-notify-b1");
        row(list, "about_board", "目标设备", "黄山派 / SF32LB52");
        row(list, "about_sdk", "SDK", "2.5.1 / 421126d9");
        row(list, "about_display", "屏幕", "390 x 450");
        row(list, "about_power", "供电", "-- / USB ?");
        row(list, "about_time", "时间状态", "未校时");
        settings_note_create(list, "硬件版本与实际容量尚未核实。"); break;
    case WRISTFLOW_SURFACE_WAKE_SETTINGS:
        title = "抬腕亮屏";
        obj = row(list, "wrist_row", "抬腕亮屏", "");
        lv_obj_add_event_cb(obj, long_press_row, LV_EVENT_SHORT_CLICKED, shell);
        obj = named(obj, "row_switch");
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(obj, wrist_changed, LV_EVENT_VALUE_CHANGED, shell);
        break;
    default: break;
    }
    lv_label_set_text(named(root, "app_title"), title);
    if (surface == WRISTFLOW_SURFACE_ENDURANCE_SETTINGS || surface == WRISTFLOW_SURFACE_BATTERY_OPTIMIZATION ||
        surface == WRISTFLOW_SURFACE_BATTERY_INFO) lv_obj_set_style_text_font(named(root, "app_title"), notification_22, 0);
    lv_obj_set_width(named(root, "app_title"), 175);
    wristflow_settings_refresh(root, surface, shell);
    return root;
}

void wristflow_settings_refresh(lv_obj_t *root, wristflow_surface_t surface, wristflow_ui_shell_t *shell)
{
    if (!root) return;
    if (wristflow_aod_settings_surface(surface)) {
        wristflow_aod_settings_refresh(root, surface, shell);
        refresh_save_note(root, shell);
        return;
    }
    refresh_save_note(root, shell);
    wristflow_settings_t settings;
    if (!wristflow_ui_shell_get_settings(shell, &settings)) return;
    char value[40];
    if (surface == WRISTFLOW_SURFACE_BATTERY_OPTIMIZATION) {
        bool wrist = settings.wrist_wake, awake = wristflow_ui_shell_keep_minutes(shell) != 0;
        lv_obj_set_flag(named(root, "optimize_wrist"), LV_OBJ_FLAG_HIDDEN, !wrist);
        lv_obj_set_flag(named(root, "optimize_awake"), LV_OBJ_FLAG_HIDDEN, !awake);
        label(named(root, "optimize_note"), wrist || awake ? "关闭以下功能可减少相关活动。" : "暂无可关闭的优化项。");
    }
    unsigned minutes = wristflow_ui_shell_keep_minutes(shell);
    lv_obj_t *clock = lv_obj_find_by_name(root, "settings_clock");
    if (clock) {
        const wristflow_watch_snapshot_t *snapshot = wristflow_ui_shell_snapshot(shell);
        if (snapshot->time_unavailable) snprintf(value, sizeof value, "--:--");
        else snprintf(value, sizeof value, "%02u:%02u", snapshot->hour_24, snapshot->minute);
        label(clock, value);
    }
    if (surface == WRISTFLOW_SURFACE_FACE_MANAGEMENT)
        lv_obj_set_state(named(root, "row_switch"), LV_STATE_CHECKED, settings.face_long_press);
    else if (surface == WRISTFLOW_SURFACE_DND_SETTINGS) {
        for (unsigned i = 0; i < WRISTFLOW_DND_COUNT; ++i) {
            snprintf(value, sizeof value, "dnd_mode_%u", i);
            lv_obj_set_flag(named(named(root, value), "row_check"), LV_OBJ_FLAG_HIDDEN, i != (unsigned)settings.dnd_mode);
        }
        bool scheduled = settings.dnd_mode == WRISTFLOW_DND_SCHEDULED;
        lv_obj_set_flag(named(root, "dnd_start"), LV_OBJ_FLAG_HIDDEN, !scheduled);
        lv_obj_set_flag(named(root, "dnd_end"), LV_OBJ_FLAG_HIDDEN, !scheduled);
        snprintf(value, sizeof value, "%02u:%02u", settings.dnd_start_minute / 60U, settings.dnd_start_minute % 60U);
        label(named(named(root, "dnd_start"), "row_detail"), value);
        snprintf(value, sizeof value, "%s%02u:%02u", settings.dnd_end_minute < settings.dnd_start_minute ? "次日" : "",
            settings.dnd_end_minute / 60U, settings.dnd_end_minute % 60U);
        label(named(named(root, "dnd_end"), "row_detail"), value);
        label(named(root, "dnd_clock_note"), !scheduled ? "" :
            wristflow_ui_shell_snapshot(shell)->time_unavailable ? "等待校时，定时勿扰暂不生效。" :
            wristflow_ui_shell_dnd(shell) ? "定时勿扰生效中。" : "当前不在勿扰时段。");
    }
    else if (surface == WRISTFLOW_SURFACE_WAKE_SETTINGS)
        lv_obj_set_state(named(root, "row_switch"), LV_STATE_CHECKED, settings.wrist_wake);
    else if (surface == WRISTFLOW_SURFACE_NOTIFICATION_SETTINGS)
        label(named(named(root, "notifications_dnd"), "row_detail"), settings.dnd_mode == WRISTFLOW_DND_SCHEDULED ? "定时开启" :
            settings.dnd_mode == WRISTFLOW_DND_ALL_DAY ? "全天开启" : "关闭");
    else if (surface == WRISTFLOW_SURFACE_SOUND_SETTINGS) {
        lv_obj_set_state(named(named(root, "silent_row"), "row_switch"), LV_STATE_CHECKED, settings.silent_mode);
        static const char *names[] = {"关闭", "标准", "增强"};
        label(named(named(root, "vibration_mode"), "row_detail"), names[settings.vibration_mode]);
        lv_obj_set_style_text_font(named(named(root, "vibration_mode"), "row_detail"), notification_22, 0);
    } else if (surface == WRISTFLOW_SURFACE_VIBRATION_SETTINGS) {
        for (unsigned i = 0; i < WRISTFLOW_VIBRATION_COUNT; ++i) {
            snprintf(value, sizeof value, "vibration_%u", i);
            lv_obj_set_flag(named(named(root, value), "row_check"), LV_OBJ_FLAG_HIDDEN, i != (unsigned)settings.vibration_mode);
        }
    }
    else if (surface == WRISTFLOW_SURFACE_DISPLAY_SETTINGS) {
        label(named(named(root, "display_aod"), "row_detail"), settings.aod_mode == WRISTFLOW_AOD_SCHEDULED ? "定时开启" : "关闭");
        label(named(named(root, "display_aod_styles"), "row_detail"), settings.aod_style == WRISTFLOW_AOD_CENTER ? "居中数字" : "左上数字");
        label(named(named(root, "display_wrist"), "row_detail"), settings.wrist_wake ? "开启" : "关闭");
        snprintf(value, sizeof value, "%u%%", settings.brightness);
        label(named(named(root, "display_brightness"), "row_detail"), wristflow_ui_shell_auto_brightness(shell) ? "自动亮度" : value);
        snprintf(value, sizeof value, "%u秒", settings.screen_timeout);
        label(named(named(root, "display_timeout"), "row_detail"), value);
        if (minutes) snprintf(value, sizeof value, "%u分钟", minutes);
        else snprintf(value, sizeof value, "关闭");
        label(named(named(root, "display_keep"), "row_detail"), value);
    } else if (surface == WRISTFLOW_SURFACE_BRIGHTNESS_SETTINGS) {
        bool available = wristflow_ui_shell_has_auto_brightness(shell);
        bool automatic = wristflow_ui_shell_auto_brightness(shell);
        lv_obj_set_state(named(root, "auto_brightness"), LV_STATE_DISABLED, !available);
        lv_obj_set_state(named(root, "row_switch"), LV_STATE_DISABLED, !available);
        lv_obj_set_state(named(root, "row_switch"), LV_STATE_CHECKED, automatic);
        lv_obj_set_flag(named(root, "custom_brightness"), LV_OBJ_FLAG_HIDDEN, automatic);
        lv_obj_set_flag(named(root, "auto_brightness_unavailable"), LV_OBJ_FLAG_HIDDEN, available);
    } else if (surface == WRISTFLOW_SURFACE_SCREEN_TIMEOUT || surface == WRISTFLOW_SURFACE_KEEP_AWAKE) {
        unsigned selected = surface == WRISTFLOW_SURFACE_SCREEN_TIMEOUT ? settings.screen_timeout : minutes;
        for (unsigned i = 0; i < 5; ++i) {
            unsigned option = surface == WRISTFLOW_SURFACE_SCREEN_TIMEOUT ? timeouts[i] : awake_times[i];
            snprintf(value, sizeof value, "choice_%u", option);
            lv_obj_set_flag(named(named(root, value), "row_check"), LV_OBJ_FLAG_HIDDEN, option != selected);
        }
    } else if (surface == WRISTFLOW_SURFACE_ABOUT) {
        const wristflow_watch_snapshot_t *snapshot = wristflow_ui_shell_snapshot(shell);
        char battery[16];
        wristflow_battery_text(snapshot, battery, sizeof battery);
        snprintf(value, sizeof value, "%s / %s", battery, wristflow_power_input_text(&snapshot->power));
        label(named(named(root, "about_power"), "row_detail"), value);
        label(named(named(root, "about_time"), "row_detail"),
            wristflow_ui_shell_snapshot(shell)->time_unavailable ? "未校时" : "UTC+8");
    }
}
