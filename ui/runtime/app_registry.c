#include "app_registry.h"
#include "weather.h"
#include "weather_screen.h"
#include "wristflow_ui.h"
#include "product_state.h"
#include "activity_view.h"
#include <stdio.h>
#include <string.h>

static void missing(const wristflow_watch_snapshot_t *snapshot, wristflow_app_data_t *data)
{
    (void)snapshot;
    strcpy(data->value, "--");
}

static void battery(const wristflow_watch_snapshot_t *snapshot, wristflow_app_data_t *data)
{
    wristflow_battery_text(snapshot, data->value, sizeof data->value);
    snprintf(data->reason, sizeof data->reason, "%s", wristflow_power_charge_text(&snapshot->power));
}

static void calories(const wristflow_watch_snapshot_t *snapshot, wristflow_app_data_t *data)
{ wristflow_activity_value(wristflow_activity_metric(snapshot, "calories"), data->value, sizeof data->value); }
static void steps(const wristflow_watch_snapshot_t *snapshot, wristflow_app_data_t *data)
{ wristflow_activity_value(wristflow_activity_metric(snapshot, "steps"), data->value, sizeof data->value); }
static void minutes(const wristflow_watch_snapshot_t *snapshot, wristflow_app_data_t *data)
{ wristflow_activity_value(wristflow_activity_metric(snapshot, "active_minutes"), data->value, sizeof data->value); }
static void standing(const wristflow_watch_snapshot_t *snapshot, wristflow_app_data_t *data)
{ wristflow_activity_value(wristflow_activity_metric(snapshot, "standing"), data->value, sizeof data->value); }

static void weather(const wristflow_watch_snapshot_t *snapshot, wristflow_app_data_t *data)
{
    (void)snapshot;
    wristflow_weather_data_t current;
    strcpy(data->value, "--");
    if (!wristflow_weather_get_current(&current, NULL)) {
        snprintf(data->reason, sizeof data->reason, "%s", current.message);
        return;
    }
    if (current.temperature_valid)
        snprintf(data->value, sizeof data->value, "%d°", current.temperature);
    snprintf(data->reason, sizeof data->reason, "%s", current.expired ? current.updated : current.condition);
}

/* One identity feeds the menu, the card and its destination. XML owns geometry. */
static const wristflow_app_descriptor_t apps[] = {
    {"breathing", "呼吸训练", "\xef\x86\x92", "launch_breathing", WRISTFLOW_SURFACE_BREATHING,
        WRISTFLOW_CAPABILITY_READY, 0, 0x008cff, "", screen_breathing_create, NULL, false, false, NULL},
    {"countdown", "倒计时", "\xef\x89\x92", "launch_countdown", WRISTFLOW_SURFACE_COUNTDOWN,
        WRISTFLOW_CAPABILITY_READY, 0, 0x008cff, "", screen_countdown_create, NULL, true, false, NULL},
    {"stopwatch", "秒表", "\xef\x8b\xb2", "launch_stopwatch", WRISTFLOW_SURFACE_STOPWATCH,
        WRISTFLOW_CAPABILITY_READY, 0, 0xf2a33b, "", screen_stopwatch_create, NULL, false, false, NULL},
    {"activity", "活力指标", "\xef\x88\x81", "launch_activity", WRISTFLOW_SURFACE_ACTIVITY,
        WRISTFLOW_CAPABILITY_READY, WRISTFLOW_CARD_HALF | WRISTFLOW_CARD_FULL, 0xf97316,
        "", screen_activity_create, calories, false, false, NULL},
    {"heart_rate", "心率", "\xef\x80\x84", "launch_heart", WRISTFLOW_SURFACE_HEART,
        WRISTFLOW_CAPABILITY_PLACEHOLDER, WRISTFLOW_CARD_QUARTER, 0xef4770,
        "未接入心率传感器", screen_app_placeholder_create, missing, false, false, NULL},
    {"watchfaces", "表盘", "\xef\x80\x97", "launch_faces", WRISTFLOW_SURFACE_FACE_PICKER,
        WRISTFLOW_CAPABILITY_READY, 0, 0x616c77, "", screen_face_picker_create, NULL, false, false, NULL},
    {"battery", "电池", "\xef\x89\x80", "launch_system", WRISTFLOW_SURFACE_SYSTEM,
        WRISTFLOW_CAPABILITY_READY, WRISTFLOW_CARD_QUARTER, 0x10b981,
        "", screen_battery_create, battery, false, false, NULL},
    {"flashlight", "手电筒", "\xef\x83\xab", "launch_flashlight", WRISTFLOW_SURFACE_FLASHLIGHT,
        WRISTFLOW_CAPABILITY_READY, 0, 0x8b71bf, "", screen_flashlight_create, NULL, false, false, NULL},
    {"settings", "设置", "\xef\x80\x93", "launch_settings", WRISTFLOW_SURFACE_SETTINGS,
        WRISTFLOW_CAPABILITY_READY, 0, 0x647783, "", screen_settings_create, NULL, false, false, NULL},
    {"blood_oxygen", "血氧", "\xef\x81\x83", "launch_blood_oxygen", WRISTFLOW_SURFACE_BLOOD_OXYGEN,
        WRISTFLOW_CAPABILITY_PLACEHOLDER, WRISTFLOW_CARD_QUARTER, 0xef4770,
        "未接入血氧传感器", screen_app_placeholder_create, missing, false, false, NULL},
    {"stress", "压力", "\xef\x84\x98", "launch_stress", WRISTFLOW_SURFACE_STRESS,
        WRISTFLOW_CAPABILITY_PLACEHOLDER, WRISTFLOW_CARD_QUARTER, 0x38bdf8,
        "压力功能待接入", screen_app_placeholder_create, missing, false, false, NULL},
    {"steps", "步数", "\xef\x9c\x8c", "launch_steps", WRISTFLOW_SURFACE_ACTIVITY,
        WRISTFLOW_CAPABILITY_READY, WRISTFLOW_CARD_QUARTER, 0xe2db48,
        "", screen_activity_create, steps, false, true, NULL},
    {"standing", "站立", "\xef\x86\x83", "launch_standing", WRISTFLOW_SURFACE_ACTIVITY,
        WRISTFLOW_CAPABILITY_READY, WRISTFLOW_CARD_QUARTER, 0x10d6b0,
        "", screen_activity_create, standing, false, true, NULL},
    {"calories", "卡路里", "\xef\x81\xad", "launch_calories", WRISTFLOW_SURFACE_ACTIVITY,
        WRISTFLOW_CAPABILITY_READY, WRISTFLOW_CARD_QUARTER, 0xf97316,
        "", screen_activity_create, calories, false, true, NULL},
    {"active_minutes", "中高强度活动", "\xef\x80\x97", "launch_active_minutes", WRISTFLOW_SURFACE_ACTIVITY,
        WRISTFLOW_CAPABILITY_READY, WRISTFLOW_CARD_QUARTER, 0x38bdf8,
        "", screen_activity_create, minutes, false, true, NULL},
    {"weather", "天气", "\xef\x83\x82", "launch_weather", WRISTFLOW_SURFACE_WEATHER,
        WRISTFLOW_CAPABILITY_READY, WRISTFLOW_CARD_HALF | WRISTFLOW_CARD_FULL, 0x38bdf8,
        "暂无天气数据", screen_weather_create, weather, false, false, NULL},
    {"sleep", "睡眠", "\xef\x88\xb6", "launch_sleep", WRISTFLOW_SURFACE_SLEEP,
        WRISTFLOW_CAPABILITY_PLACEHOLDER, WRISTFLOW_CARD_QUARTER, 0x388bfa,
        "睡眠功能待接入", screen_app_placeholder_create, missing, false, false, NULL},
    {"alarm", "闹钟", "\xef\x83\xb3", "launch_alarm", WRISTFLOW_SURFACE_ALARM,
        WRISTFLOW_CAPABILITY_READY, WRISTFLOW_CARD_QUARTER, 0x38bdf8,
        "", screen_alarm_create, missing, true, false, NULL},
    {"compass", "指南针", "", "launch_compass", WRISTFLOW_SURFACE_COMPASS,
        WRISTFLOW_CAPABILITY_READY, 0, 0x173a69, "", screen_compass_create, NULL, false, false, &compass_icon},
    {"game_2048", "2048", "\xef\x80\x8a", "launch_game_2048", WRISTFLOW_SURFACE_GAME_2048,
        WRISTFLOW_CAPABILITY_READY, 0, 0xd8ad3e, "", screen_game_2048_create, NULL, false, false, NULL}
};

size_t wristflow_app_count(void) { return sizeof apps / sizeof apps[0]; }
const wristflow_app_descriptor_t *wristflow_app_at(size_t index)
{ return index < wristflow_app_count() ? &apps[index] : NULL; }
const wristflow_app_descriptor_t *wristflow_app_find(const char *id)
{
    if (!id) return NULL;
    for (size_t i = 0; i < wristflow_app_count(); ++i)
        if (strcmp(apps[i].id, id) == 0) return &apps[i];
    return NULL;
}
const wristflow_app_descriptor_t *wristflow_app_for_surface(wristflow_surface_t surface)
{
    for (size_t i = 0; i < wristflow_app_count(); ++i)
        if (apps[i].surface == surface) return &apps[i];
    return NULL;
}
bool wristflow_app_read(const wristflow_app_descriptor_t *app,
    const wristflow_watch_snapshot_t *snapshot, wristflow_app_data_t *data)
{
    if (!app || !data || !wristflow_snapshot_valid(snapshot) || !app->read) return false;
    *data = (wristflow_app_data_t){0};
    snprintf(data->reason, sizeof data->reason, "%s", app->reason);
    app->read(snapshot, data);
    return true;
}
const wristflow_card_page_t *wristflow_product_default_page(unsigned index)
{
    /* Retained XML preview factories share the actual persisted default model. */
    static wristflow_card_page_t pages[WRISTFLOW_PRODUCT_DEFAULT_PAGES];
    static lv_obj_t *(*const factories[])(void) = {
        screen_product_health_create, screen_product_activity_create, screen_product_daily_create};
    if (index >= WRISTFLOW_PRODUCT_DEFAULT_PAGES) return NULL;
    wristflow_layout_t layout = wristflow_layout_default();
    pages[index].create = factories[index];
    pages[index].count = wristflow_template_slots(layout.pages[index].template_id);
    memcpy(pages[index].slots, layout.pages[index].slots, sizeof pages[index].slots);
    return &pages[index];
}
