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
    WRISTFLOW_SURFACE_CONTROLS,
    WRISTFLOW_SURFACE_LAUNCHER,
    WRISTFLOW_SURFACE_STOPWATCH,
    WRISTFLOW_SURFACE_FACE_PICKER,
    WRISTFLOW_SURFACE_FLASHLIGHT,
    WRISTFLOW_SURFACE_SETTINGS,
    WRISTFLOW_SURFACE_ACTIVITY,
    WRISTFLOW_SURFACE_HEART,
    WRISTFLOW_SURFACE_SYSTEM,
    WRISTFLOW_SURFACE_COUNT
} wristflow_surface_t;

/* Includes the home root. One instance of each surface may be on the path. */
#define WRISTFLOW_NAVIGATION_MAX_DEPTH 8U

typedef struct {
    unsigned int page_count;
    unsigned int page_index;
    wristflow_surface_t surface;
    wristflow_surface_t history[WRISTFLOW_NAVIGATION_MAX_DEPTH - 1];
    unsigned int history_count;
} wristflow_navigation_t;

bool wristflow_snapshot_valid(const wristflow_watch_snapshot_t *snapshot);
bool wristflow_navigation_init(wristflow_navigation_t *navigation, unsigned int page_count);
/* Called only once the UI has committed a page, never during a partial drag. */
bool wristflow_navigation_commit_page(wristflow_navigation_t *navigation, unsigned int page);
bool wristflow_navigation_open_controls(wristflow_navigation_t *navigation);
bool wristflow_navigation_close_controls(wristflow_navigation_t *navigation);
bool wristflow_navigation_open(wristflow_navigation_t *navigation, wristflow_surface_t surface);
void wristflow_navigation_home(wristflow_navigation_t *navigation);
void wristflow_navigation_key(wristflow_navigation_t *navigation);
bool wristflow_navigation_back(wristflow_navigation_t *navigation);
bool wristflow_navigation_contains(const wristflow_navigation_t *navigation, wristflow_surface_t surface);

#endif
