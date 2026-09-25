#ifndef WRISTFLOW_APP_REGISTRY_H
#define WRISTFLOW_APP_REGISTRY_H

#include "watch_core.h"
#include "lvgl.h"
#include <stddef.h>

typedef enum {
    WRISTFLOW_CAPABILITY_READY,
    WRISTFLOW_CAPABILITY_PLACEHOLDER,
    WRISTFLOW_CAPABILITY_UNAVAILABLE
} wristflow_capability_t;

typedef enum {
    WRISTFLOW_CARD_QUARTER = 1,
    WRISTFLOW_CARD_HALF = 2,
    WRISTFLOW_CARD_FULL = 4
} wristflow_card_size_t;

typedef struct {
    char value[24];
    const char *reason;
} wristflow_app_data_t;

typedef struct wristflow_app_descriptor {
    const char *id;
    const char *title;
    const char *icon;
    const char *launcher_name;
    wristflow_surface_t surface;
    wristflow_capability_t capability;
    unsigned card_sizes;
    uint32_t color;
    const char *reason;
    lv_obj_t *(*create)(void);
    void (*read)(const wristflow_watch_snapshot_t *, wristflow_app_data_t *);
} wristflow_app_descriptor_t;

typedef struct {
    const char *app_id;
    uint16_t instance_id;
    wristflow_card_size_t size;
    uint8_t variant; /* 0: dark background; 1: application color background. */
} wristflow_component_t;

typedef struct {
    lv_obj_t *(*create)(void);
    unsigned count;
    wristflow_component_t slots[4];
} wristflow_card_page_t;

size_t wristflow_app_count(void);
const wristflow_app_descriptor_t *wristflow_app_at(size_t index);
const wristflow_app_descriptor_t *wristflow_app_find(const char *id);
const wristflow_app_descriptor_t *wristflow_app_for_surface(wristflow_surface_t surface);
bool wristflow_app_read(const wristflow_app_descriptor_t *app,
    const wristflow_watch_snapshot_t *snapshot, wristflow_app_data_t *data);
const wristflow_card_page_t *wristflow_product_default_page(unsigned index);
#define WRISTFLOW_PRODUCT_DEFAULT_PAGES 3U
#endif
