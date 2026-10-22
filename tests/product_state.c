#include "product_state.h"
#include <assert.h>
#include <string.h>

static void seal(uint8_t *record)
{
    uint32_t hash = 2166136261U;
    for (unsigned i = 0; i < 20; ++i) hash = (hash ^ record[i]) * 16777619U;
    for (unsigned i = 0; i < 4; ++i) record[20+i] = (uint8_t)(hash >> (i*8));
}

/* Build deployed v6-v8 bytes independently of the current encoder. */
static void legacy_record(const wristflow_settings_t *s, unsigned version, uint8_t *record)
{
    memset(record, 0, WRISTFLOW_SETTINGS_BYTES);
    record[0] = 'W'; record[1] = 'F'; record[2] = (uint8_t)version; record[3] = s->brightness;
    memcpy(record + 4, s->face_id, strlen(s->face_id));
    if (version == 8) record[13] = s->wrist_wake;
    if (version >= 7) record[14] = (uint8_t)s->vibration_mode;
    record[15] = s->breathing_minutes; record[16] = s->dnd_mode == WRISTFLOW_DND_ALL_DAY;
    record[17] = s->face_long_press; record[18] = s->screen_timeout; record[19] = (uint8_t)s->menu_layout;
    seal(record);
}

int main(void)
{
    wristflow_settings_t defaults = wristflow_settings_default();
    assert(defaults.brightness == 60 && strcmp(defaults.face_id, "diffusion") == 0);
    assert(defaults.menu_layout == WRISTFLOW_MENU_LIST);
    assert(defaults.face_long_press && defaults.screen_timeout == 10);
    assert(defaults.breathing_minutes == 1 && defaults.vibration_mode == WRISTFLOW_VIBRATION_STANDARD && !defaults.wrist_wake);
    wristflow_settings_t chosen = defaults;
    chosen.brightness = 37; strcpy(chosen.face_id, "simple"); chosen.menu_layout = WRISTFLOW_MENU_GRID;
    chosen.face_long_press = false; chosen.screen_timeout = 60; chosen.dnd_mode = WRISTFLOW_DND_ALL_DAY;
    chosen.breathing_minutes = 5; chosen.vibration_mode = WRISTFLOW_VIBRATION_ENHANCED; chosen.wrist_wake = true;
    uint8_t record[WRISTFLOW_SETTINGS_BYTES];
    assert(wristflow_settings_encode(&chosen, record));
    wristflow_settings_t restored = defaults;
    assert(wristflow_settings_decode(&restored, record, sizeof record));
    assert(wristflow_settings_equal(&chosen, &restored));
    assert(restored.dnd_mode);
    assert(restored.breathing_minutes == 5);
    assert(record[2] == 9 && (record[13] & 1U) && (record[14] & 3U) == WRISTFLOW_VIBRATION_ENHANCED);
    restored = chosen; restored.wrist_wake = false;
    assert(!wristflow_settings_equal(&chosen, &restored));
    legacy_record(&chosen, 7, record);
    assert(wristflow_settings_decode(&restored, record, sizeof record));
    wristflow_settings_t v7 = chosen; v7.wrist_wake = false;
    assert(wristflow_settings_equal(&restored, &v7));
    legacy_record(&defaults, 8, record); record[13] = 1; seal(record);
    assert(wristflow_settings_decode(&restored, record, sizeof record));
    assert(!strcmp(restored.face_id, "diffusion") && restored.wrist_wake);
    for (unsigned flag = 2; flag <= 255; ++flag) {
        record[13] = (uint8_t)flag; seal(record); restored = defaults;
        assert(!wristflow_settings_decode(&restored, record, sizeof record));
        assert(wristflow_settings_equal(&restored, &defaults));
    }
    for (unsigned mode = 0; mode < WRISTFLOW_VIBRATION_COUNT; ++mode) {
        restored = chosen; restored.vibration_mode = (wristflow_vibration_mode_t)mode;
        assert(wristflow_settings_encode(&restored, record));
        wristflow_settings_t decoded = defaults;
        assert(wristflow_settings_decode(&decoded, record, sizeof record));
        assert(wristflow_settings_equal(&decoded, &restored));
        assert(!wristflow_settings_equal(&decoded, &defaults));
    }
    /* v6 preserves all seven old preferences and acquires only the new default. */
    legacy_record(&chosen, 6, record);
    assert(wristflow_settings_decode(&restored, record, sizeof record));
    wristflow_settings_t migrated = chosen; migrated.vibration_mode = WRISTFLOW_VIBRATION_STANDARD; migrated.wrist_wake = false;
    assert(wristflow_settings_equal(&restored, &migrated));
    for (unsigned mode = WRISTFLOW_VIBRATION_COUNT; mode <= 255; ++mode) {
        legacy_record(&chosen, 7, record); record[14] = (uint8_t)mode; seal(record);
        restored = defaults;
        assert(!wristflow_settings_decode(&restored, record, sizeof record));
        assert(wristflow_settings_equal(&restored, &defaults));
    }
    restored = chosen; restored.vibration_mode = (wristflow_vibration_mode_t)-1;
    assert(!wristflow_settings_encode(&restored, record));
    assert(wristflow_settings_encode(&defaults, record));
    assert(wristflow_settings_decode(&restored, record, sizeof record) && !strcmp(restored.face_id, "diffusion"));
    assert(wristflow_settings_encode(&chosen, record));
    for (unsigned byte = 0; byte < sizeof record; ++byte) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            record[byte] ^= 1U << bit;
            restored = defaults;
            assert(!wristflow_settings_decode(&restored, record, sizeof record));
            assert(wristflow_settings_equal(&restored, &defaults));
            record[byte] ^= 1U << bit;
        }
    }
    for (unsigned size = 0; size < sizeof record; ++size)
        assert(!wristflow_settings_decode(&restored, record, size));
    assert(!wristflow_settings_decode(&restored, record, sizeof record + 1));
    /* A deployed v1 record must retain brightness/face and acquire the new default. */
    const uint8_t legacy[] = {0x57,0x46,0x01,0x1b,0x73,0x69,0x6d,0x70,0x6c,0x65,
        0,0,0,0,0,0,0,0,0,0,0xd2,0xe0,0x68,0x9d};
    assert(wristflow_settings_decode(&restored, legacy, sizeof legacy));
    assert(restored.brightness == 27 && strcmp(restored.face_id, "simple") == 0 &&
           restored.menu_layout == WRISTFLOW_MENU_LIST && restored.face_long_press && restored.screen_timeout == 10);
    assert(restored.vibration_mode == WRISTFLOW_VIBRATION_STANDARD);
    /* Deployed v2 adds layout but has no display preferences. */
    uint8_t v2[WRISTFLOW_SETTINGS_BYTES];
    memcpy(v2, legacy, sizeof v2); v2[2] = 2; v2[19] = WRISTFLOW_MENU_GRID;
    uint32_t hash = 2166136261U;
    for (unsigned i = 0; i < 20; ++i) hash = (hash ^ v2[i]) * 16777619U;
    for (unsigned i = 0; i < 4; ++i) v2[20+i] = (uint8_t)(hash >> (i*8));
    assert(wristflow_settings_decode(&restored, v2, sizeof v2));
    assert(restored.brightness == 27 && !strcmp(restored.face_id, "simple") &&
        restored.menu_layout == WRISTFLOW_MENU_GRID && restored.face_long_press && restored.screen_timeout == 10);
    assert(!restored.dnd_mode && restored.vibration_mode == WRISTFLOW_VIBRATION_STANDARD);
    /* The deployed P1 v3 format has no DND bit: retain its display preferences. */
    uint8_t v3[WRISTFLOW_SETTINGS_BYTES];
    memcpy(v3, v2, sizeof v3); v3[2] = 3; v3[17] = 0; v3[18] = 30;
    hash = 2166136261U;
    for (unsigned i = 0; i < 20; ++i) hash = (hash ^ v3[i]) * 16777619U;
    for (unsigned i = 0; i < 4; ++i) v3[20+i] = (uint8_t)(hash >> (i*8));
    assert(wristflow_settings_decode(&restored, v3, sizeof v3));
    assert(!restored.dnd_mode && !restored.face_long_press && restored.screen_timeout == 30 && restored.brightness == 27);
    assert(restored.breathing_minutes == 1 && restored.vibration_mode == WRISTFLOW_VIBRATION_STANDARD);
    /* Deployed v4 retains DND and display settings, acquiring default duration. */
    uint8_t v4[WRISTFLOW_SETTINGS_BYTES];
    memcpy(v4, v3, sizeof v4); v4[2] = 4; v4[16] = 1;
    hash = 2166136261U;
    for (unsigned i = 0; i < 20; ++i) hash = (hash ^ v4[i]) * 16777619U;
    for (unsigned i = 0; i < 4; ++i) v4[20+i] = (uint8_t)(hash >> (i*8));
    assert(wristflow_settings_decode(&restored, v4, sizeof v4));
    assert(restored.dnd_mode && restored.screen_timeout == 30 && restored.breathing_minutes == 1 &&
        restored.vibration_mode == WRISTFLOW_VIBRATION_STANDARD);
    /* Never reuse the deployed C2 draft's v5 ID/flag semantics for breathing. */
    v4[2] = 5; v4[16] = 3;
    hash = 2166136261U;
    for (unsigned i = 0; i < 20; ++i) hash = (hash ^ v4[i]) * 16777619U;
    for (unsigned i = 0; i < 4; ++i) v4[20+i] = (uint8_t)(hash >> (i*8));
    restored = defaults;
    assert(!wristflow_settings_decode(&restored, v4, sizeof v4));
    assert(wristflow_settings_equal(&restored, &defaults));
    for (unsigned minutes = 0; minutes <= 6; ++minutes) {
        restored = chosen; restored.breathing_minutes = minutes;
        assert(wristflow_settings_encode(&restored, record) == (minutes >= 1 && minutes <= 5));
    }
    /* A checksummed but out-of-range duration must preserve caller defaults. */
    assert(wristflow_settings_encode(&chosen, record)); record[14] = (6U << 2) | WRISTFLOW_VIBRATION_ENHANCED;
    hash = 2166136261U;
    for (unsigned i = 0; i < 20; ++i) hash = (hash ^ record[i]) * 16777619U;
    for (unsigned i = 0; i < 4; ++i) record[20+i] = (uint8_t)(hash >> (i*8));
    restored = defaults;
    assert(!wristflow_settings_decode(&restored, record, sizeof record));
    assert(wristflow_settings_equal(&restored, &defaults));
    restored.screen_timeout = 6;
    assert(!wristflow_settings_encode(&restored, record));
    restored = chosen;
    restored.menu_layout = WRISTFLOW_MENU_COUNT;
    assert(!wristflow_settings_encode(&restored, record));
    restored = chosen;
    restored.brightness = 0;
    assert(!wristflow_settings_encode(&restored, record));
    restored.brightness = 101;
    assert(!wristflow_settings_encode(&restored, record));
    memset(restored.face_id, 'x', sizeof restored.face_id);
    restored.brightness = 60;
    assert(!wristflow_settings_encode(&restored, record));
    wristflow_watch_snapshot_t snapshot = wristflow_product_snapshot(false, WRISTFLOW_TIME_MIN);
    assert(wristflow_snapshot_valid(&snapshot) && snapshot.time_unavailable && snapshot.battery_unavailable);
    snapshot = wristflow_product_snapshot(true, 0);
    assert(snapshot.time_unavailable);
    snapshot = wristflow_product_snapshot(true, WRISTFLOW_TIME_MIN - 1);
    assert(snapshot.time_unavailable);
    snapshot = wristflow_product_snapshot(true, WRISTFLOW_TIME_MIN);
    assert(!snapshot.time_unavailable && snapshot.hour_24 == 8 && snapshot.minute == 0);
    snapshot = wristflow_product_snapshot(true, WRISTFLOW_TIME_MIN + 16 * 3600 - 1);
    assert(snapshot.hour_24 == 23 && snapshot.minute == 59);
    snapshot = wristflow_product_snapshot(true, WRISTFLOW_TIME_MIN + 16 * 3600);
    assert(snapshot.hour_24 == 0 && snapshot.minute == 0);
    snapshot = wristflow_product_snapshot(true, WRISTFLOW_TIME_MAX);
    assert(!snapshot.time_unavailable && snapshot.hour_24 == 7 && snapshot.minute == 59);
    snapshot = wristflow_product_snapshot(true, WRISTFLOW_TIME_MAX + 1);
    assert(snapshot.time_unavailable);

    /* New fields round-trip together, affect equality, and reject sealed garbage. */
    chosen.silent_mode = true; chosen.dnd_mode = WRISTFLOW_DND_SCHEDULED;
    assert(wristflow_settings_encode(&chosen, record));
    assert(wristflow_settings_decode(&restored, record, sizeof record));
    assert(wristflow_settings_equal(&restored, &chosen) && restored.silent_mode);
    restored.silent_mode = false; assert(!wristflow_settings_equal(&restored, &chosen));
    const unsigned endpoints[][2] = {{0,1439},{1439,0},{7*60+13,18*60+47}};
    for (unsigned i = 0; i < 3; ++i) {
        restored = chosen;
        restored.dnd_start_minute = endpoints[i][0]; restored.dnd_end_minute = endpoints[i][1];
        assert(wristflow_settings_encode(&restored, record));
        wristflow_settings_t decoded = defaults;
        assert(wristflow_settings_decode(&decoded, record, sizeof record));
        assert(wristflow_settings_equal(&restored, &decoded));
    }
    assert(wristflow_settings_encode(&chosen, record));
    for (unsigned byte = 0; byte < sizeof record; ++byte) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            record[byte] ^= 1U << bit;
            restored = defaults;
            assert(!wristflow_settings_decode(&restored, record, sizeof record));
            assert(wristflow_settings_equal(&restored, &defaults));
            record[byte] ^= 1U << bit;
        }
    }
    const unsigned bad_bytes[] = {13, 13, 14, 14, 16, 18};
    const unsigned bad_values[] = {0xc0, 0x30, 0xe0, 3, 0xff, 0xff};
    for (unsigned i = 0; i < 6; ++i) {
        assert(wristflow_settings_encode(&chosen, record));
        record[bad_bytes[i]] = (uint8_t)bad_values[i];
        if (i == 4) record[17] = 0xff;
        if (i == 5) record[19] = 0xff;
        seal(record); restored = defaults;
        assert(!wristflow_settings_decode(&restored, record, sizeof record));
        assert(wristflow_settings_equal(&restored, &defaults));
    }
    /* Full-day and overnight intervals are checked for every minute, including
     * inclusive starts/exclusive ends. No clock means scheduled waiting. */
    for (unsigned crossing = 0; crossing < 2; ++crossing) {
        chosen.dnd_start_minute = crossing ? 1320 : 480;
        chosen.dnd_end_minute = crossing ? 480 : 1320;
        for (unsigned minute = 0; minute < 1440; ++minute) {
            snapshot = (wristflow_watch_snapshot_t){.hour_24 = minute / 60, .minute = minute % 60};
            bool expected = crossing ? minute >= 1320 || minute < 480 : minute >= 480 && minute < 1320;
            assert(wristflow_settings_dnd_active(&chosen, &snapshot) == expected);
        }
    }
    snapshot.time_unavailable = true;
    assert(!wristflow_settings_dnd_active(&chosen, &snapshot));
    assert(!wristflow_settings_dnd_active(&chosen, NULL));
    chosen.dnd_mode = WRISTFLOW_DND_ALL_DAY;
    assert(wristflow_settings_dnd_active(&chosen, &snapshot));
    chosen.dnd_mode = WRISTFLOW_DND_OFF;
    assert(!wristflow_settings_dnd_active(&chosen, &snapshot));
    chosen.dnd_mode = WRISTFLOW_DND_SCHEDULED;
    chosen.dnd_start_minute = chosen.dnd_end_minute;
    assert(!wristflow_settings_encode(&chosen, record));
    return 0;
}
