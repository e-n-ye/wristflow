#include "watch_core.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

int main(void)
{
    wristflow_navigation_t navigation;
    assert(!wristflow_navigation_init(NULL, 4));
    assert(!wristflow_navigation_init(&navigation, 0));
    assert(wristflow_navigation_init(&navigation, 4));
    assert(navigation.page_index == 0 && navigation.surface == WRISTFLOW_SURFACE_HOME);
    for (unsigned int page = 1; page < 4; ++page) {
        assert(wristflow_navigation_commit_page(&navigation, page));
        assert(!wristflow_navigation_open_controls(&navigation));
        assert(navigation.page_index == page && navigation.surface == WRISTFLOW_SURFACE_HOME);
    }
    assert(!wristflow_navigation_commit_page(&navigation, 4));
    assert(navigation.page_index == 3);
    assert(wristflow_navigation_commit_page(&navigation, 0));
    assert(wristflow_navigation_open_controls(&navigation));
    assert(!wristflow_navigation_open_controls(&navigation));
    assert(!wristflow_navigation_commit_page(&navigation, 1));
    assert(navigation.page_index == 0 && navigation.surface == WRISTFLOW_SURFACE_CONTROLS);
    assert(wristflow_navigation_close_controls(&navigation));
    assert(!wristflow_navigation_close_controls(&navigation));
    assert(wristflow_navigation_init(&navigation, 1));
    assert(!wristflow_navigation_commit_page(&navigation, 1));

    wristflow_watch_snapshot_t snapshot = {0, 0, 0};
    assert(wristflow_snapshot_valid(&snapshot));
    snapshot = (wristflow_watch_snapshot_t){23, 59, 100};
    assert(wristflow_snapshot_valid(&snapshot));
    snapshot.hour_24 = 24;
    assert(!wristflow_snapshot_valid(&snapshot));
    snapshot = (wristflow_watch_snapshot_t){12, 60, 53};
    assert(!wristflow_snapshot_valid(&snapshot));
    snapshot = (wristflow_watch_snapshot_t){12, 0, 101};
    assert(!wristflow_snapshot_valid(&snapshot));
    assert(!wristflow_snapshot_valid(NULL));
    puts("PASS: committed navigation, overlay guards, one-page ring and snapshot bounds");
    return 0;
}
