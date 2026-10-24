#include "product_state.h"
#include <string.h>
#include <stdio.h>

wristflow_settings_t wristflow_settings_default(void)
{
    return (wristflow_settings_t){.brightness = 60, .face_id = "diffusion",
        .menu_layout = WRISTFLOW_MENU_LIST, .face_long_press = true,
        .screen_timeout = 10, .dnd_mode = WRISTFLOW_DND_OFF, .breathing_minutes = 1,
        .vibration_mode = WRISTFLOW_VIBRATION_STANDARD,
        .dnd_start_minute = 22 * 60, .dnd_end_minute = 8 * 60,
        .aod_start_minute = 8 * 60, .aod_end_minute = 22 * 60};
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
        settings->breathing_minutes >= 1 && settings->breathing_minutes <= 5 &&
        (unsigned)settings->vibration_mode < WRISTFLOW_VIBRATION_COUNT &&
        (unsigned)settings->dnd_mode < WRISTFLOW_DND_COUNT &&
        (unsigned)settings->aod_mode < WRISTFLOW_AOD_MODE_COUNT &&
        (unsigned)settings->aod_style < WRISTFLOW_AOD_STYLE_COUNT &&
        settings->aod_start_minute < 1440 && settings->aod_end_minute < 1440 &&
        settings->aod_start_minute != settings->aod_end_minute &&
        settings->dnd_start_minute < 1440 && settings->dnd_end_minute < 1440 &&
        (settings->dnd_mode != WRISTFLOW_DND_SCHEDULED ||
            settings->dnd_start_minute != settings->dnd_end_minute) &&
        (memcmp(settings->face_id, "diffusion\0", 10) == 0 ||
         memcmp(settings->face_id, "simple\0", 7) == 0);
}

bool wristflow_settings_equal(const wristflow_settings_t *a, const wristflow_settings_t *b)
{
    return wristflow_settings_valid(a) && wristflow_settings_valid(b) &&
        a->brightness == b->brightness && strcmp(a->face_id, b->face_id) == 0 &&
        a->menu_layout == b->menu_layout && a->face_long_press == b->face_long_press &&
        a->screen_timeout == b->screen_timeout && a->dnd_mode == b->dnd_mode &&
        a->breathing_minutes == b->breathing_minutes && a->vibration_mode == b->vibration_mode &&
        a->wrist_wake == b->wrist_wake && a->silent_mode == b->silent_mode &&
        a->endurance_mode == b->endurance_mode && a->auto_brightness == b->auto_brightness &&
        a->dnd_start_minute == b->dnd_start_minute && a->dnd_end_minute == b->dnd_end_minute &&
        a->aod_mode == b->aod_mode && a->aod_style == b->aod_style &&
        a->aod_start_minute == b->aod_start_minute && a->aod_end_minute == b->aod_end_minute;
}

bool wristflow_settings_dnd_active(const wristflow_settings_t *settings,
    const wristflow_watch_snapshot_t *clock)
{
    if (!wristflow_settings_valid(settings)) return false;
    if (settings->dnd_mode == WRISTFLOW_DND_ALL_DAY) return true;
    if (settings->dnd_mode != WRISTFLOW_DND_SCHEDULED || !clock ||
        clock->time_unavailable || clock->hour_24 > 23 || clock->minute > 59) return false;
    unsigned minute = clock->hour_24 * 60U + clock->minute;
    unsigned start = settings->dnd_start_minute, end = settings->dnd_end_minute;
    return start < end ? minute >= start && minute < end : minute >= start || minute < end;
}

bool wristflow_settings_aod_active(const wristflow_settings_t *settings,
    const wristflow_watch_snapshot_t *clock)
{
    if (!wristflow_settings_valid(settings) || settings->endurance_mode ||
        settings->aod_mode != WRISTFLOW_AOD_SCHEDULED || !clock ||
        clock->time_unavailable || clock->hour_24 > 23 || clock->minute > 59) return false;
    unsigned minute = clock->hour_24 * 60U + clock->minute;
    unsigned start = settings->aod_start_minute, end = settings->aod_end_minute;
    return start < end ? minute >= start && minute < end : minute >= start || minute < end;
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
    data[0] = 'W'; data[1] = 'F'; data[2] = 12; data[3] = settings->brightness;
    /* v11 uses an explicit face code (0 diffusion, 1 simple), freeing bytes
     * 5..12 for AOD while retaining the 24-byte record/checksum boundary.
     * v5 belongs to the separate wrist-wake draft and is never reinterpreted. */
    data[4] = strcmp(settings->face_id, "simple") == 0;
    data[5] = (uint8_t)settings->aod_start_minute;
    data[6] = (uint8_t)(settings->aod_start_minute >> 8);
    data[7] = (uint8_t)settings->aod_end_minute;
    data[8] = (uint8_t)(settings->aod_end_minute >> 8);
    data[9] = (uint8_t)(settings->aod_mode | settings->aod_style << 1);
    data[10] = settings->auto_brightness; /* v12; older versions require zero. */
    data[13] = (uint8_t)(settings->wrist_wake | settings->silent_mode << 1 |
        settings->face_long_press << 2 | settings->menu_layout << 3 | settings->dnd_mode << 4 |
        settings->endurance_mode << 6);
    data[14] = (uint8_t)(settings->vibration_mode | settings->breathing_minutes << 2);
    data[15] = settings->screen_timeout;
    data[16] = (uint8_t)settings->dnd_start_minute;
    data[17] = (uint8_t)(settings->dnd_start_minute >> 8);
    data[18] = (uint8_t)settings->dnd_end_minute;
    data[19] = (uint8_t)(settings->dnd_end_minute >> 8);
    uint32_t hash = checksum(data);
    for (unsigned i = 0; i < 4; ++i) data[20 + i] = (uint8_t)(hash >> (i * 8));
    return true;
}

bool wristflow_settings_decode(wristflow_settings_t *settings, const uint8_t *data, size_t size)
{
    if (!settings || !data || size != WRISTFLOW_SETTINGS_BYTES ||
        data[0] != 'W' || data[1] != 'F' || data[2] < 1 || data[2] > 12 || data[2] == 5) return false;
    uint32_t hash = checksum(data);
    for (unsigned i = 0; i < 4; ++i)
        if (data[20 + i] != (uint8_t)(hash >> (i * 8))) return false;
    wristflow_settings_t decoded = wristflow_settings_default();
    if (data[2] >= 9) {
        if ((data[13] & (data[2] == 9 ? 0xc0U : 0x80U)) || (data[14] & 0xe0U)) return false;
        decoded.endurance_mode = data[2] >= 10 && (data[13] & 0x40U);
        decoded.brightness = data[3];
        memset(decoded.face_id, 0, sizeof decoded.face_id);
        if (data[2] >= 11) {
            if (data[4] > 1 || (data[9] & ~3U) || data[10] > (data[2] >= 12 ? 1U : 0U) || data[11] || data[12]) return false;
            decoded.auto_brightness = data[2] >= 12 && data[10];
            strcpy(decoded.face_id, data[4] ? "simple" : "diffusion");
            decoded.aod_start_minute = data[5] | (uint16_t)data[6] << 8;
            decoded.aod_end_minute = data[7] | (uint16_t)data[8] << 8;
            decoded.aod_mode = (wristflow_aod_mode_t)(data[9] & 1U);
            decoded.aod_style = (wristflow_aod_style_t)((data[9] >> 1) & 1U);
        } else memcpy(decoded.face_id, data + 4, 9);
        decoded.wrist_wake = (data[13] & 1U) != 0;
        decoded.silent_mode = (data[13] & 2U) != 0;
        decoded.face_long_press = (data[13] & 4U) != 0;
        decoded.menu_layout = (wristflow_menu_layout_t)((data[13] >> 3) & 1U);
        decoded.dnd_mode = (wristflow_dnd_mode_t)((data[13] >> 4) & 3U);
        decoded.vibration_mode = (wristflow_vibration_mode_t)(data[14] & 3U);
        decoded.breathing_minutes = (data[14] >> 2) & 7U;
        decoded.screen_timeout = data[15];
        decoded.dnd_start_minute = data[16] | (uint16_t)data[17] << 8;
        decoded.dnd_end_minute = data[18] | (uint16_t)data[19] << 8;
        if (!wristflow_settings_valid(&decoded)) return false;
        *settings = decoded;
        return true;
    }
    decoded.brightness = data[3];
    memset(decoded.face_id, 0, sizeof decoded.face_id);
    memcpy(decoded.face_id, data + 4, data[2] == 8 ? 9 : data[2] == 7 ? 10 : data[2] == 6 ? 11 : data[2] == 4 ? 12 : data[2] == 3 ? 13 : 15);
    decoded.menu_layout = data[2] == 1 ? WRISTFLOW_MENU_LIST : (wristflow_menu_layout_t)data[19];
    if (data[2] >= 3) {
        if (data[17] > 1) return false;
        decoded.face_long_press = data[17] != 0;
        decoded.screen_timeout = data[18];
    }
    if (data[2] >= 4) {
        if (data[16] > 1) return false;
        decoded.dnd_mode = data[16] != 0;
    }
    if (data[2] >= 6) decoded.breathing_minutes = data[15];
    if (data[2] >= 7) decoded.vibration_mode = (wristflow_vibration_mode_t)data[14];
    if (data[2] == 8) {
        if (data[13] > 1) return false;
        decoded.wrist_wake = data[13] != 0;
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
        snapshot.utc_seconds = utc_seconds;
    }
    return snapshot;
}

void wristflow_product_apply_power(wristflow_watch_snapshot_t *snapshot,
    const wristflow_power_snapshot_t *power)
{
    snapshot->power = *power;
    snapshot->battery_unavailable = !power->estimate_valid;
    snapshot->battery_percent = power->estimate_valid ? power->estimated_percent : 0;
}

void wristflow_battery_text(const wristflow_watch_snapshot_t *snapshot, char *text, size_t size)
{
    if (!snapshot->battery_unavailable)
        snprintf(text, size, "%s%u%%", snapshot->power.estimate_valid ? "~" : "",
            (unsigned)snapshot->battery_percent);
    else if (snapshot->power.voltage_valid) {
        unsigned centivolts = (snapshot->power.millivolts + 5U) / 10U;
        snprintf(text, size, "%u.%02uV", centivolts / 100U, centivolts % 100U);
    } else snprintf(text, size, "--");
}
