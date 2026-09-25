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
    *navigation = (wristflow_navigation_t){.page_count = page_count, .surface = WRISTFLOW_SURFACE_HOME};
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

bool wristflow_navigation_contains(const wristflow_navigation_t *navigation, wristflow_surface_t surface)
{
    if (!navigation || !navigation->page_count || surface < WRISTFLOW_SURFACE_HOME ||
        surface >= WRISTFLOW_SURFACE_COUNT) return false;
    if (navigation->surface == surface) return true;
    for (unsigned int i = 0; i < navigation->history_count; ++i)
        if (navigation->history[i] == surface) return true;
    return false;
}

static bool push(wristflow_navigation_t *navigation, wristflow_surface_t surface)
{
    if (navigation->history_count >= WRISTFLOW_NAVIGATION_MAX_DEPTH - 1 ||
        wristflow_navigation_contains(navigation, surface)) return false;
    navigation->history[navigation->history_count++] = navigation->surface;
    navigation->surface = surface;
    return true;
}

bool wristflow_navigation_open_controls(wristflow_navigation_t *navigation)
{
    if (navigation == NULL || navigation->page_count == 0 || navigation->page_index != 0 ||
        navigation->surface != WRISTFLOW_SURFACE_HOME)
        return false;
    return push(navigation, WRISTFLOW_SURFACE_CONTROLS);
}

bool wristflow_navigation_close_controls(wristflow_navigation_t *navigation)
{
    if (navigation == NULL || navigation->surface != WRISTFLOW_SURFACE_CONTROLS)
        return false;
    return wristflow_navigation_back(navigation);
}

void wristflow_navigation_home(wristflow_navigation_t *navigation)
{
    if (navigation == NULL || navigation->page_count == 0) return;
    navigation->page_index = 0;
    navigation->surface = WRISTFLOW_SURFACE_HOME;
    navigation->history_count = 0;
}

void wristflow_navigation_key(wristflow_navigation_t *navigation)
{
    if (navigation == NULL || navigation->page_count == 0) return;
    if (navigation->surface == WRISTFLOW_SURFACE_HOME && navigation->page_index == 0)
        push(navigation, WRISTFLOW_SURFACE_LAUNCHER);
    else
        wristflow_navigation_home(navigation);
}

bool wristflow_navigation_open(wristflow_navigation_t *navigation, wristflow_surface_t surface)
{
    if (navigation == NULL || navigation->page_count == 0) return false;
    bool from_launcher = navigation->surface == WRISTFLOW_SURFACE_LAUNCHER;
    bool from_app = navigation->surface >= WRISTFLOW_SURFACE_STOPWATCH &&
                    navigation->surface < WRISTFLOW_SURFACE_COUNT;
    bool from_controls = navigation->surface == WRISTFLOW_SURFACE_CONTROLS &&
                         (surface == WRISTFLOW_SURFACE_SETTINGS || surface == WRISTFLOW_SURFACE_FLASHLIGHT);
    bool from_face = navigation->surface == WRISTFLOW_SURFACE_HOME && navigation->page_index == 0 &&
                     surface == WRISTFLOW_SURFACE_FACE_PICKER;
    if (surface < WRISTFLOW_SURFACE_STOPWATCH || surface >= WRISTFLOW_SURFACE_COUNT ||
        !(from_launcher || from_controls || from_face || from_app))
        return false;
    return push(navigation, surface);
}

bool wristflow_navigation_back(wristflow_navigation_t *navigation)
{
    if (navigation == NULL || navigation->page_count == 0) return false;
    if (navigation->history_count == 0)
        return false;
    navigation->surface = navigation->history[--navigation->history_count];
    return true;
}
