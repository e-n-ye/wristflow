#include "watch_core.h"
#include "stopwatch.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static void stack_limits(void)
{
    wristflow_navigation_t navigation;
    assert(wristflow_navigation_init(&navigation, 4));
    wristflow_navigation_key(&navigation);
    /* Seven app surfaces make it possible to reach and exceed eight levels. */
    for (unsigned int i = 0; i < WRISTFLOW_NAVIGATION_MAX_DEPTH - 2; ++i)
        assert(wristflow_navigation_open(&navigation, WRISTFLOW_SURFACE_STOPWATCH + i));
    assert(navigation.history_count == WRISTFLOW_NAVIGATION_MAX_DEPTH - 1);
    wristflow_navigation_t full = navigation;
    assert(!wristflow_navigation_open(&navigation, WRISTFLOW_SURFACE_SYSTEM));
    assert(!wristflow_navigation_open(&navigation, navigation.surface));
    assert(!wristflow_navigation_open(&navigation, WRISTFLOW_SURFACE_STOPWATCH));
    assert(!wristflow_navigation_open(&navigation, WRISTFLOW_SURFACE_COUNT));
    assert(!wristflow_navigation_open(&navigation, (wristflow_surface_t)-1));
    assert(memcmp(&full, &navigation, sizeof full) == 0);
    for (unsigned int depth = WRISTFLOW_NAVIGATION_MAX_DEPTH - 1; depth > 0; --depth) {
        wristflow_surface_t popped = navigation.surface;
        assert(wristflow_navigation_back(&navigation));
        assert(navigation.history_count == depth - 1);
        assert(navigation.surface == full.history[depth - 1]);
        assert(!wristflow_navigation_contains(&navigation, popped));
    }
    assert(navigation.surface == WRISTFLOW_SURFACE_HOME);
    assert(!wristflow_navigation_back(&navigation));
    assert(!wristflow_navigation_contains(NULL, WRISTFLOW_SURFACE_HOME));
    assert(!wristflow_navigation_contains(&navigation, WRISTFLOW_SURFACE_COUNT));
    /* KEY1 discards the entire path, including a retained parent app. */
    assert(wristflow_navigation_open_controls(&navigation));
    assert(wristflow_navigation_open(&navigation, WRISTFLOW_SURFACE_SETTINGS));
    assert(wristflow_navigation_open(&navigation, WRISTFLOW_SURFACE_SYSTEM));
    wristflow_navigation_key(&navigation);
    assert(navigation.surface == WRISTFLOW_SURFACE_HOME && navigation.history_count == 0);
    assert(!wristflow_navigation_back(&navigation));
    assert(!wristflow_navigation_contains(&navigation, WRISTFLOW_SURFACE_SETTINGS));
}

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

    assert(!wristflow_navigation_open(&navigation, WRISTFLOW_SURFACE_STOPWATCH));
    assert(wristflow_navigation_open(&navigation, WRISTFLOW_SURFACE_FACE_PICKER));
    assert(wristflow_navigation_back(&navigation));
    wristflow_navigation_key(&navigation);
    assert(navigation.surface == WRISTFLOW_SURFACE_LAUNCHER);
    assert(wristflow_navigation_open(&navigation, WRISTFLOW_SURFACE_STOPWATCH));
    assert(wristflow_navigation_open(&navigation, WRISTFLOW_SURFACE_SETTINGS));
    assert(!wristflow_navigation_open(&navigation, WRISTFLOW_SURFACE_STOPWATCH));
    assert(wristflow_navigation_back(&navigation));
    assert(navigation.surface == WRISTFLOW_SURFACE_STOPWATCH);
    assert(wristflow_navigation_back(&navigation));
    assert(navigation.surface == WRISTFLOW_SURFACE_LAUNCHER);
    wristflow_navigation_key(&navigation);
    assert(navigation.surface == WRISTFLOW_SURFACE_HOME);
    assert(wristflow_navigation_open_controls(&navigation));
    assert(wristflow_navigation_open(&navigation, WRISTFLOW_SURFACE_FLASHLIGHT));
    assert(wristflow_navigation_back(&navigation));
    assert(navigation.surface == WRISTFLOW_SURFACE_CONTROLS);
    wristflow_navigation_key(&navigation);
    assert(!wristflow_navigation_back(&navigation));
    stack_limits();

    wristflow_stopwatch_t watch = {0};
    wristflow_stopwatch_toggle(&watch, UINT32_MAX - 500);
    wristflow_stopwatch_update(&watch, 499);
    assert(watch.elapsed_ms == 1000 && watch.running);
    wristflow_stopwatch_toggle(&watch, 999);
    wristflow_stopwatch_update(&watch, 9999);
    assert(watch.elapsed_ms == 1500 && !watch.running);
    wristflow_stopwatch_toggle(&watch, 9999);
    wristflow_stopwatch_update(&watch, 10999);
    assert(watch.elapsed_ms == 2500);
    wristflow_stopwatch_reset(&watch, 12000);
    assert(watch.elapsed_ms == 0 && !watch.running);
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
    puts("PASS: bounded navigation stack, nested back, duplicate/overflow guards, key reset, carousel and snapshot bounds");
    return 0;
}
