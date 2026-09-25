#include "watch_core.h"
#include <stddef.h>

bool wristflow_snapshot_valid(const wristflow_watch_snapshot_t *snapshot)
{
    return snapshot != NULL && snapshot->hour_24 < 24 && snapshot->minute < 60 &&
           snapshot->battery_percent <= 100;
}

bool wristflow_navigation_init(wristflow_navigation_t *navigation, unsigned int page_count)
{
    if (navigation == NULL || page_count == 0)
        return false;
    *navigation = (wristflow_navigation_t){page_count, 0, WRISTFLOW_SURFACE_HOME, WRISTFLOW_SURFACE_HOME};
    return true;
}

bool wristflow_navigation_commit_page(wristflow_navigation_t *navigation, unsigned int page)
{
    if (navigation == NULL || navigation->surface != WRISTFLOW_SURFACE_HOME ||
        page >= navigation->page_count)
        return false;
    navigation->page_index = page;
    return true;
}

bool wristflow_navigation_open_controls(wristflow_navigation_t *navigation)
{
    if (navigation == NULL || navigation->page_count == 0 || navigation->page_index != 0 ||
        navigation->surface != WRISTFLOW_SURFACE_HOME)
        return false;
    navigation->surface = WRISTFLOW_SURFACE_CONTROLS;
    return true;
}

bool wristflow_navigation_close_controls(wristflow_navigation_t *navigation)
{
    if (navigation == NULL || navigation->surface != WRISTFLOW_SURFACE_CONTROLS)
        return false;
    navigation->surface = WRISTFLOW_SURFACE_HOME;
    return true;
}

void wristflow_navigation_home(wristflow_navigation_t *navigation)
{
    if (navigation == NULL || navigation->page_count == 0) return;
    navigation->page_index = 0;
    navigation->surface = WRISTFLOW_SURFACE_HOME;
    navigation->return_surface = WRISTFLOW_SURFACE_HOME;
}

void wristflow_navigation_key(wristflow_navigation_t *navigation)
{
    if (navigation == NULL || navigation->page_count == 0) return;
    if (navigation->surface == WRISTFLOW_SURFACE_HOME && navigation->page_index == 0)
        navigation->surface = WRISTFLOW_SURFACE_LAUNCHER;
    else
        wristflow_navigation_home(navigation);
}

bool wristflow_navigation_open(wristflow_navigation_t *navigation, wristflow_surface_t surface)
{
    if (navigation == NULL || navigation->page_count == 0) return false;
    bool from_launcher = navigation->surface == WRISTFLOW_SURFACE_LAUNCHER;
    bool from_controls = navigation->surface == WRISTFLOW_SURFACE_CONTROLS &&
                         (surface == WRISTFLOW_SURFACE_SETTINGS || surface == WRISTFLOW_SURFACE_FLASHLIGHT);
    bool from_face = navigation->surface == WRISTFLOW_SURFACE_HOME && navigation->page_index == 0 &&
                     surface == WRISTFLOW_SURFACE_FACE_PICKER;
    if (surface < WRISTFLOW_SURFACE_STOPWATCH || surface >= WRISTFLOW_SURFACE_COUNT ||
        !(from_launcher || from_controls || from_face))
        return false;
    navigation->return_surface = navigation->surface;
    navigation->surface = surface;
    return true;
}

bool wristflow_navigation_back(wristflow_navigation_t *navigation)
{
    if (navigation == NULL || navigation->page_count == 0) return false;
    if (navigation->surface == WRISTFLOW_SURFACE_HOME)
        return false;
    if (navigation->surface == WRISTFLOW_SURFACE_LAUNCHER ||
        navigation->surface == WRISTFLOW_SURFACE_CONTROLS)
        wristflow_navigation_home(navigation);
    else
        navigation->surface = navigation->return_surface;
    return true;
}
