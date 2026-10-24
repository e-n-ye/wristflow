#ifndef WRISTFLOW_COMPASS_VIEW_H
#define WRISTFLOW_COMPASS_VIEW_H
#include "lvgl.h"

typedef enum {
    WRISTFLOW_COMPASS_UNAVAILABLE,
    WRISTFLOW_COMPASS_HEADING,
    WRISTFLOW_COMPASS_CALIBRATING
} wristflow_compass_display_state_t;

/* UI-thread display input only. The future provider owns calibration and age. */
typedef struct {
    wristflow_compass_display_state_t state;
    unsigned heading_degrees; /* Clockwise from north, valid only in [0, 359]. */
} wristflow_compass_display_t;

void wristflow_compass_render(lv_obj_t *root, const wristflow_compass_display_t *display);
#endif
