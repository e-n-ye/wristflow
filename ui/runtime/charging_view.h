#ifndef WRISTFLOW_CHARGING_VIEW_H
#define WRISTFLOW_CHARGING_VIEW_H
#include "product_state.h"
#include "lvgl.h"

typedef struct wristflow_charging_view wristflow_charging_view_t;
/* LVGL-thread only. Presentation owns no charging controls or power lease. */
wristflow_charging_view_t *wristflow_charging_view_create(void);
void wristflow_charging_view_destroy(wristflow_charging_view_t *);
bool wristflow_charging_view_update(wristflow_charging_view_t *, const wristflow_watch_snapshot_t *, bool allow_present);
void wristflow_charging_view_cover(wristflow_charging_view_t *, bool covered);
void wristflow_charging_view_dismiss(wristflow_charging_view_t *);
bool wristflow_charging_view_active(const wristflow_charging_view_t *);
#endif
