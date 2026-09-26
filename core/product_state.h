#ifndef WRISTFLOW_PRODUCT_STATE_H
#define WRISTFLOW_PRODUCT_STATE_H

#include "watch_core.h"
#include <stddef.h>

#define WRISTFLOW_SETTINGS_BYTES 24U
#define WRISTFLOW_TIME_MIN 1767225600U /* 2026-01-01 UTC */
#define WRISTFLOW_TIME_MAX 4102444799U /* 2099-12-31 UTC */

typedef enum {
    WRISTFLOW_MENU_LIST,
    WRISTFLOW_MENU_GRID,
    WRISTFLOW_MENU_COUNT
} wristflow_menu_layout_t;

typedef struct {
    uint8_t brightness;
    char face_id[16];
    wristflow_menu_layout_t menu_layout;
    bool face_long_press;
    uint8_t screen_timeout;
} wristflow_settings_t;

bool wristflow_screen_timeout_valid(unsigned seconds);

wristflow_settings_t wristflow_settings_default(void);
bool wristflow_settings_valid(const wristflow_settings_t *settings);
bool wristflow_settings_equal(const wristflow_settings_t *a, const wristflow_settings_t *b);
bool wristflow_settings_encode(const wristflow_settings_t *settings, uint8_t data[WRISTFLOW_SETTINGS_BYTES]);
/* Invalid records leave the caller's defaults intact. No raw C structs on flash. */
bool wristflow_settings_decode(wristflow_settings_t *settings, const uint8_t *data, size_t size);
/* RTC stores UTC. Product display currently uses a fixed UTC+8 offset. */
wristflow_watch_snapshot_t wristflow_product_snapshot(bool rtc_read, uint32_t utc_seconds);

#endif
