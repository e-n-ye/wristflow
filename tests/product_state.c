#include "product_state.h"
#include <assert.h>
#include <string.h>

static void seal(uint8_t *record)
{
    uint32_t hash = 2166136261U;
    for (unsigned i = 0; i < 20; ++i) hash = (hash ^ record[i]) * 16777619U;
    for (unsigned i = 0; i < 4; ++i) record[20+i] = (uint8_t)(hash >> (i*8));
}

int main(void)
{
    wristflow_settings_t defaults = wristflow_settings_default();
    assert(defaults.brightness == 60 && strcmp(defaults.face_id, "diffusion") == 0);
    assert(defaults.menu_layout == WRISTFLOW_MENU_LIST);
    assert(defaults.face_long_press && defaults.screen_timeout == 10);
    assert(defaults.breathing_minutes == 1 && defaults.vibration_mode == WRISTFLOW_VIBRATION_STANDARD && !defaults.wrist_wake);
    const wristflow_settings_t chosen = {37, "simple", WRISTFLOW_MENU_GRID, false, 60, true, 5, WRISTFLOW_VIBRATION_ENHANCED, true};
    uint8_t record[WRISTFLOW_SETTINGS_BYTES];
    assert(wristflow_settings_encode(&chosen, record));
    wristflow_settings_t restored = defaults;
    assert(wristflow_settings_decode(&restored, record, sizeof record));
    assert(wristflow_settings_equal(&chosen, &restored));
    assert(restored.do_not_disturb);
    assert(restored.breathing_minutes == 5);
    assert(record[2] == 8 && record[13] == 1 && record[14] == WRISTFLOW_VIBRATION_ENHANCED);
    restored = chosen; restored.wrist_wake = false;
    assert(!wristflow_settings_equal(&chosen, &restored));
    assert(wristflow_settings_encode(&chosen, record));
    record[2] = 7; record[13] = 0; seal(record);
    assert(wristflow_settings_decode(&restored, record, sizeof record));
    wristflow_settings_t v7 = chosen; v7.wrist_wake = false;
    assert(wristflow_settings_equal(&restored, &v7));
    assert(wristflow_settings_encode(&defaults, record)); record[13] = 1; seal(record);
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
    assert(wristflow_settings_encode(&chosen, record));
    record[2] = 6; record[13] = 0; record[14] = 0; seal(record);
    assert(wristflow_settings_decode(&restored, record, sizeof record));
    wristflow_settings_t migrated = chosen; migrated.vibration_mode = WRISTFLOW_VIBRATION_STANDARD; migrated.wrist_wake = false;
    assert(wristflow_settings_equal(&restored, &migrated));
    for (unsigned mode = WRISTFLOW_VIBRATION_COUNT; mode <= 255; ++mode) {
        assert(wristflow_settings_encode(&chosen, record)); record[14] = (uint8_t)mode; seal(record);
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
    assert(!restored.do_not_disturb && restored.vibration_mode == WRISTFLOW_VIBRATION_STANDARD);
    /* The deployed P1 v3 format has no DND bit: retain its display preferences. */
    uint8_t v3[WRISTFLOW_SETTINGS_BYTES];
    memcpy(v3, v2, sizeof v3); v3[2] = 3; v3[17] = 0; v3[18] = 30;
    hash = 2166136261U;
    for (unsigned i = 0; i < 20; ++i) hash = (hash ^ v3[i]) * 16777619U;
    for (unsigned i = 0; i < 4; ++i) v3[20+i] = (uint8_t)(hash >> (i*8));
    assert(wristflow_settings_decode(&restored, v3, sizeof v3));
    assert(!restored.do_not_disturb && !restored.face_long_press && restored.screen_timeout == 30 && restored.brightness == 27);
    assert(restored.breathing_minutes == 1 && restored.vibration_mode == WRISTFLOW_VIBRATION_STANDARD);
    /* Deployed v4 retains DND and display settings, acquiring default duration. */
    uint8_t v4[WRISTFLOW_SETTINGS_BYTES];
    memcpy(v4, v3, sizeof v4); v4[2] = 4; v4[16] = 1;
    hash = 2166136261U;
    for (unsigned i = 0; i < 20; ++i) hash = (hash ^ v4[i]) * 16777619U;
    for (unsigned i = 0; i < 4; ++i) v4[20+i] = (uint8_t)(hash >> (i*8));
    assert(wristflow_settings_decode(&restored, v4, sizeof v4));
    assert(restored.do_not_disturb && restored.screen_timeout == 30 && restored.breathing_minutes == 1 &&
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
    assert(wristflow_settings_encode(&chosen, record)); record[15] = 6;
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
    return 0;
}
