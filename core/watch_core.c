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
    *navigation = (wristflow_navigation_t){page_count, 0, WRISTFLOW_SURFACE_HOME};
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
