#include "product_state.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    wristflow_settings_t defaults = wristflow_settings_default();
    assert(defaults.brightness == 60 && strcmp(defaults.face_id, "diffusion") == 0);
    assert(defaults.menu_layout == WRISTFLOW_MENU_LIST);
    const wristflow_settings_t chosen = {37, "simple", WRISTFLOW_MENU_GRID};
    uint8_t record[WRISTFLOW_SETTINGS_BYTES];
    assert(wristflow_settings_encode(&chosen, record));
    wristflow_settings_t restored = defaults;
    assert(wristflow_settings_decode(&restored, record, sizeof record));
    assert(wristflow_settings_equal(&chosen, &restored));
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
           restored.menu_layout == WRISTFLOW_MENU_LIST);
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
