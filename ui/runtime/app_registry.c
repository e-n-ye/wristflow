#include "app_registry.h"
#include "wristflow_ui.h"
#include <stdio.h>
#include <string.h>

static void missing(const wristflow_watch_snapshot_t *snapshot, wristflow_app_data_t *data)
{
    (void)snapshot;
    strcpy(data->value, "--");
}

static void battery(const wristflow_watch_snapshot_t *snapshot, wristflow_app_data_t *data)
{
    if (snapshot->battery_unavailable) strcpy(data->value, "USB");
    else snprintf(data->value, sizeof data->value, "%u%%", snapshot->battery_percent);
}

/* One identity feeds the menu, the card and its destination. XML owns geometry. */
static const wristflow_app_descriptor_t apps[] = {
    {"stopwatch", "秒表", "\xef\x8b\xb2", "launch_stopwatch", WRISTFLOW_SURFACE_STOPWATCH,
        WRISTFLOW_CAPABILITY_READY, 0, 0xf2a33b, "", screen_stopwatch_create, NULL},
    {"activity", "活力指标", "\xef\x88\x81", "launch_activity", WRISTFLOW_SURFACE_ACTIVITY,
        WRISTFLOW_CAPABILITY_PLACEHOLDER, WRISTFLOW_CARD_HALF, 0xf97316,
        "活力指标功能待接入", screen_app_placeholder_create, missing},
    {"heart_rate", "心率", "\xef\x80\x84", "launch_heart", WRISTFLOW_SURFACE_HEART,
        WRISTFLOW_CAPABILITY_PLACEHOLDER, WRISTFLOW_CARD_QUARTER, 0xef4770,
        "未接入心率传感器", screen_app_placeholder_create, missing},
    {"watchfaces", "表盘", "\xef\x80\x97", "launch_faces", WRISTFLOW_SURFACE_FACE_PICKER,
        WRISTFLOW_CAPABILITY_READY, 0, 0x616c77, "", screen_face_picker_create, NULL},
    {"battery", "电量", "\xef\x89\x80", "launch_system", WRISTFLOW_SURFACE_SYSTEM,
        WRISTFLOW_CAPABILITY_PLACEHOLDER, WRISTFLOW_CARD_QUARTER, 0x10b981,
        "USB 供电，未接入电池", screen_app_placeholder_create, battery},
    {"flashlight", "手电筒", "\xef\x83\xab", "launch_flashlight", WRISTFLOW_SURFACE_FLASHLIGHT,
        WRISTFLOW_CAPABILITY_READY, 0, 0x8b71bf, "", screen_flashlight_create, NULL},
    {"settings", "设置", "\xef\x80\x93", "launch_settings", WRISTFLOW_SURFACE_SETTINGS,
        WRISTFLOW_CAPABILITY_READY, 0, 0x647783, "", screen_settings_create, NULL},
    {"blood_oxygen", "血氧", "\xef\x81\x83", "launch_blood_oxygen", WRISTFLOW_SURFACE_BLOOD_OXYGEN,
        WRISTFLOW_CAPABILITY_PLACEHOLDER, WRISTFLOW_CARD_QUARTER, 0xef4770,
        "未接入血氧传感器", screen_app_placeholder_create, missing},
    {"stress", "压力", "\xef\x84\x98", "launch_stress", WRISTFLOW_SURFACE_STRESS,
        WRISTFLOW_CAPABILITY_PLACEHOLDER, WRISTFLOW_CARD_QUARTER, 0x38bdf8,
        "压力功能待接入", screen_app_placeholder_create, missing},
    {"steps", "计步", "\xef\x9c\x8c", "launch_steps", WRISTFLOW_SURFACE_STEPS,
        WRISTFLOW_CAPABILITY_PLACEHOLDER, WRISTFLOW_CARD_QUARTER, 0xe2db48,
        "计步功能待接入", screen_app_placeholder_create, missing},
    {"standing", "站立", "\xef\x86\x83", "launch_standing", WRISTFLOW_SURFACE_STANDING,
        WRISTFLOW_CAPABILITY_PLACEHOLDER, WRISTFLOW_CARD_QUARTER, 0x10d6b0,
        "站立功能待接入", screen_app_placeholder_create, missing},
    {"weather", "天气", "\xef\x83\x82", "launch_weather", WRISTFLOW_SURFACE_WEATHER,
        WRISTFLOW_CAPABILITY_PLACEHOLDER, WRISTFLOW_CARD_HALF, 0x38bdf8,
        "等待手机同步天气", screen_app_placeholder_create, missing},
    {"sleep", "睡眠", "\xef\x88\xb6", "launch_sleep", WRISTFLOW_SURFACE_SLEEP,
        WRISTFLOW_CAPABILITY_PLACEHOLDER, WRISTFLOW_CARD_QUARTER, 0x388bfa,
        "睡眠功能待接入", screen_app_placeholder_create, missing},
    {"alarm", "闹钟", "\xef\x83\xb3", "launch_alarm", WRISTFLOW_SURFACE_ALARM,
        WRISTFLOW_CAPABILITY_PLACEHOLDER, WRISTFLOW_CARD_QUARTER, 0x38bdf8,
        "闹钟功能待接入", screen_app_placeholder_create, missing}
};

static const wristflow_card_page_t pages[] = {
    {screen_product_health_create, 4, {
        {"heart_rate", 1, WRISTFLOW_CARD_QUARTER, 1},
        {"blood_oxygen", 2, WRISTFLOW_CARD_QUARTER, 0},
        {"battery", 3, WRISTFLOW_CARD_QUARTER, 0},
        {"stress", 4, WRISTFLOW_CARD_QUARTER, 1}}},
    {screen_product_activity_create, 3, {
        {"activity", 5, WRISTFLOW_CARD_HALF, 0},
        {"steps", 6, WRISTFLOW_CARD_QUARTER, 0},
        {"standing", 7, WRISTFLOW_CARD_QUARTER, 0}}},
    {screen_product_daily_create, 3, {
        {"weather", 8, WRISTFLOW_CARD_HALF, 0},
        {"sleep", 9, WRISTFLOW_CARD_QUARTER, 1},
        {"alarm", 10, WRISTFLOW_CARD_QUARTER, 0}}}
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
    *data = (wristflow_app_data_t){.reason = app->reason};
    app->read(snapshot, data);
    return true;
}
const wristflow_card_page_t *wristflow_product_default_page(unsigned index)
{ return index < WRISTFLOW_PRODUCT_DEFAULT_PAGES ? &pages[index] : NULL; }
