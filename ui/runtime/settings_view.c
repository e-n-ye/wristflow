#include "settings_view.h"
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

static void accept(lv_event_t *event)
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
    lv_obj_add_event_cb(named(dialog, "confirm_accept"), accept, LV_EVENT_SHORT_CLICKED, shell);
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
    } else if (value == 60) confirm(lv_screen_active(), shell, value);
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

static void unavailable(lv_obj_t *list, const char *name, const char *title, const char *detail)
{ lv_obj_add_state(row(list, name, title, detail), LV_STATE_DISABLED); }

static const unsigned timeouts[] = {5, 10, 20, 30, 60};
static const unsigned awake_times[] = {0, 5, 10, 15, 20};

lv_obj_t *wristflow_settings_screen(wristflow_ui_shell_t *shell, wristflow_surface_t surface)
{
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
    if (surface < WRISTFLOW_SURFACE_FACE_MANAGEMENT || surface > WRISTFLOW_SURFACE_WAKE_SETTINGS) return NULL;
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
        route(row(list, "display_wrist", "抬腕亮屏", "暂未开放"), WRISTFLOW_SURFACE_WAKE_SETTINGS, shell);
        route(row(list, "display_keep", "持续亮屏", "关闭"), WRISTFLOW_SURFACE_KEEP_AWAKE, shell);
        break;
    case WRISTFLOW_SURFACE_BRIGHTNESS_SETTINGS:
        title = "屏幕亮度";
        unavailable(list, "auto_brightness", "自动亮度", "暂未开放");
        settings_note_create(list, "自动亮度功能待接入。");
        route(row(list, "custom_brightness", "自定义亮度", "60%"), WRISTFLOW_SURFACE_BRIGHTNESS_ADJUST, shell);
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
        settings_note_create(list, "尚未接入振动硬件，声音提醒功能待接入。");
        unavailable(list, "sound_disabled", "声音提醒", "暂未开放");
        unavailable(list, "vibration_disabled", "振动提醒", "未接入硬件");
        break;
    case WRISTFLOW_SURFACE_DND_SETTINGS:
        title = "勿扰模式";
        settings_note_create(list, "等待手机通知功能，暂不可设置勿扰模式。");
        unavailable(list, "dnd_disabled", "勿扰模式", "暂未开放"); break;
    case WRISTFLOW_SURFACE_NOTIFICATION_SETTINGS:
        title = "消息通知";
        settings_note_create(list, "等待手机连接与通知同步，当前暂无消息通知。");
        unavailable(list, "notifications_disabled", "消息提醒", "暂未开放"); break;
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
        row(list, "about_product", "WristFlow", "Product / 设置与显示");
        row(list, "about_firmware", "固件版本", "2026.09.26-settings");
        row(list, "about_board", "目标设备", "黄山派 / SF32LB52");
        row(list, "about_sdk", "SDK", "2.5.1 / 421126d9");
        row(list, "about_display", "屏幕", "390 x 450");
        row(list, "about_power", "供电", "USB / 未接入电池");
        row(list, "about_time", "时间状态", "未校时");
        settings_note_create(list, "硬件版本与实际容量尚未核实。"); break;
    case WRISTFLOW_SURFACE_WAKE_SETTINGS:
        title = "抬腕亮屏";
        settings_note_create(list, "抬腕识别功能待接入，当前可使用按键亮屏。");
        unavailable(list, "wrist_disabled", "抬腕亮屏", "暂未开放"); break;
    default: break;
    }
    lv_label_set_text(named(root, "app_title"), title);
    wristflow_settings_refresh(root, surface, shell);
    return root;
}

void wristflow_settings_refresh(lv_obj_t *root, wristflow_surface_t surface, wristflow_ui_shell_t *shell)
{
    if (!root) return;
    wristflow_settings_t settings;
    if (!wristflow_ui_shell_get_settings(shell, &settings)) return;
    char value[40];
    unsigned minutes = wristflow_ui_shell_keep_minutes(shell);
    if (surface == WRISTFLOW_SURFACE_FACE_MANAGEMENT)
        lv_obj_set_state(named(root, "row_switch"), LV_STATE_CHECKED, settings.face_long_press);
    else if (surface == WRISTFLOW_SURFACE_DISPLAY_SETTINGS) {
        snprintf(value, sizeof value, "%u%%", settings.brightness);
        label(named(named(root, "display_brightness"), "row_detail"), value);
        snprintf(value, sizeof value, "%u秒", settings.screen_timeout);
        label(named(named(root, "display_timeout"), "row_detail"), value);
        if (minutes) snprintf(value, sizeof value, "%u分钟", minutes);
        else snprintf(value, sizeof value, "关闭");
        label(named(named(root, "display_keep"), "row_detail"), value);
    } else if (surface == WRISTFLOW_SURFACE_BRIGHTNESS_SETTINGS) {
        snprintf(value, sizeof value, "%u%%", settings.brightness);
        label(named(named(root, "custom_brightness"), "row_detail"), value);
    } else if (surface == WRISTFLOW_SURFACE_SCREEN_TIMEOUT || surface == WRISTFLOW_SURFACE_KEEP_AWAKE) {
        unsigned selected = surface == WRISTFLOW_SURFACE_SCREEN_TIMEOUT ? settings.screen_timeout : minutes;
        for (unsigned i = 0; i < 5; ++i) {
            unsigned option = surface == WRISTFLOW_SURFACE_SCREEN_TIMEOUT ? timeouts[i] : awake_times[i];
            snprintf(value, sizeof value, "choice_%u", option);
            lv_obj_set_flag(named(named(root, value), "row_check"), LV_OBJ_FLAG_HIDDEN, option != selected);
        }
    } else if (surface == WRISTFLOW_SURFACE_ABOUT) {
        label(named(named(root, "about_time"), "row_detail"),
            wristflow_ui_shell_snapshot(shell)->time_unavailable ? "未校时" : "UTC+8");
    }
}
