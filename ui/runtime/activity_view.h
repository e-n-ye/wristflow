#ifndef WRISTFLOW_ACTIVITY_VIEW_H
#define WRISTFLOW_ACTIVITY_VIEW_H
#include "watch_core.h"
#include "component_layout.h"
#include "lvgl.h"
#include <stddef.h>

const wristflow_activity_metric_t *wristflow_activity_metric(
    const wristflow_watch_snapshot_t *snapshot, const char *component_id);
void wristflow_activity_value(const wristflow_activity_metric_t *metric, char *text, size_t capacity);
void wristflow_activity_render(lv_obj_t *root, const wristflow_watch_snapshot_t *snapshot);
void wristflow_activity_card(lv_obj_t *card, const wristflow_component_t *component,
    const wristflow_watch_snapshot_t *snapshot);
#endif
