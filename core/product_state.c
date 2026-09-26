#include "product_state.h"
#include <string.h>

wristflow_settings_t wristflow_settings_default(void)
{
    return (wristflow_settings_t){60, "diffusion", WRISTFLOW_MENU_LIST, true, 10};
}

bool wristflow_screen_timeout_valid(unsigned seconds)
{
    return seconds == 5 || seconds == 10 || seconds == 20 || seconds == 30 || seconds == 60;
}

bool wristflow_settings_valid(const wristflow_settings_t *settings)
{
    return settings && settings->brightness >= 10 && settings->brightness <= 100 &&
        (unsigned)settings->menu_layout < WRISTFLOW_MENU_COUNT &&
        wristflow_screen_timeout_valid(settings->screen_timeout) &&
        (memcmp(settings->face_id, "diffusion\0", 10) == 0 ||
         memcmp(settings->face_id, "simple\0", 7) == 0);
}

bool wristflow_settings_equal(const wristflow_settings_t *a, const wristflow_settings_t *b)
{
    return wristflow_settings_valid(a) && wristflow_settings_valid(b) &&
        a->brightness == b->brightness && strcmp(a->face_id, b->face_id) == 0 &&
        a->menu_layout == b->menu_layout && a->face_long_press == b->face_long_press &&
        a->screen_timeout == b->screen_timeout;
}

static uint32_t checksum(const uint8_t *data)
{
    uint32_t value = 2166136261U;
    for (unsigned i = 0; i < 20; ++i) value = (value ^ data[i]) * 16777619U;
    return value;
}

bool wristflow_settings_encode(const wristflow_settings_t *settings, uint8_t data[WRISTFLOW_SETTINGS_BYTES])
{
    if (!data || !wristflow_settings_valid(settings)) return false;
    memset(data, 0, WRISTFLOW_SETTINGS_BYTES);
    data[0] = 'W'; data[1] = 'F'; data[2] = 3; data[3] = settings->brightness;
    memcpy(data + 4, settings->face_id, strlen(settings->face_id));
    /* v3 reserves two more bytes; both supported face IDs fit in 13 bytes. */
    data[17] = settings->face_long_press;
    data[18] = settings->screen_timeout;
    data[19] = (uint8_t)settings->menu_layout;
    uint32_t hash = checksum(data);
    for (unsigned i = 0; i < 4; ++i) data[20 + i] = (uint8_t)(hash >> (i * 8));
    return true;
}

bool wristflow_settings_decode(wristflow_settings_t *settings, const uint8_t *data, size_t size)
{
    if (!settings || !data || size != WRISTFLOW_SETTINGS_BYTES ||
        data[0] != 'W' || data[1] != 'F' || data[2] < 1 || data[2] > 3) return false;
    uint32_t hash = checksum(data);
    for (unsigned i = 0; i < 4; ++i)
        if (data[20 + i] != (uint8_t)(hash >> (i * 8))) return false;
    wristflow_settings_t decoded = wristflow_settings_default();
    decoded.brightness = data[3];
    memset(decoded.face_id, 0, sizeof decoded.face_id);
    memcpy(decoded.face_id, data + 4, data[2] == 3 ? 13 : 15);
    decoded.menu_layout = data[2] == 1 ? WRISTFLOW_MENU_LIST : (wristflow_menu_layout_t)data[19];
    if (data[2] == 3) {
        if (data[17] > 1) return false;
        decoded.face_long_press = data[17] != 0;
        decoded.screen_timeout = data[18];
    }
    if (!wristflow_settings_valid(&decoded)) return false;
    *settings = decoded;
    return true;
}

wristflow_watch_snapshot_t wristflow_product_snapshot(bool rtc_read, uint32_t utc_seconds)
{
    wristflow_watch_snapshot_t snapshot = {
        .time_unavailable = true, .battery_unavailable = true, .metrics_unavailable = true
    };
    if (rtc_read && utc_seconds >= WRISTFLOW_TIME_MIN && utc_seconds <= WRISTFLOW_TIME_MAX) {
        uint32_t minute = (uint32_t)(((uint64_t)utc_seconds + 8U * 3600U) / 60U % 1440U);
        snapshot.hour_24 = (uint8_t)(minute / 60U);
        snapshot.minute = (uint8_t)(minute % 60U);
        snapshot.time_unavailable = false;
    }
    return snapshot;
}
