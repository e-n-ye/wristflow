#ifndef WRISTFLOW_MOUNT_SCREEN_H
#define WRISTFLOW_MOUNT_SCREEN_H

#include "lvgl.h"

/* Consume a generated top-level screen and move its visual tree into parent. */
lv_obj_t *wristflow_mount_screen(lv_obj_t *parent, lv_obj_t *screen);

#endif
