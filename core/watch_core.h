#ifndef WRISTFLOW_WATCH_CORE_H
#define WRISTFLOW_WATCH_CORE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t hour_24;
    uint8_t minute;
    uint8_t battery_percent;
} wristflow_watch_snapshot_t;

typedef enum {
    WRISTFLOW_SURFACE_HOME,
    WRISTFLOW_SURFACE_CONTROLS
} wristflow_surface_t;

typedef struct {
    unsigned int page_count;
    unsigned int page_index;
    wristflow_surface_t surface;
} wristflow_navigation_t;

bool wristflow_snapshot_valid(const wristflow_watch_snapshot_t *snapshot);
bool wristflow_navigation_init(wristflow_navigation_t *navigation, unsigned int page_count);
/* Called only once the UI has committed a page, never during a partial drag. */
bool wristflow_navigation_commit_page(wristflow_navigation_t *navigation, unsigned int page);
bool wristflow_navigation_open_controls(wristflow_navigation_t *navigation);
bool wristflow_navigation_close_controls(wristflow_navigation_t *navigation);

#endif
