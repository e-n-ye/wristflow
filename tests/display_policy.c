#include "display_policy.h"
#include <assert.h>
#include <limits.h>

int main(void)
{
    wristflow_display_policy_t p;
    const unsigned timeouts[] = {5, 10, 20, 30, 60};
    for (unsigned i = 0; i < 5; ++i) {
        for (unsigned wrap = 0; wrap < 2; ++wrap) {
            uint32_t start = wrap ? UINT_MAX - 2000U : 100U;
            unsigned dim = timeouts[i] == 5 ? 4000 : (timeouts[i] - 5) * 1000;
            wristflow_display_init(&p, start);
            wristflow_display_tick(&p, start + dim - 1, timeouts[i], false);
            assert(p.phase == WRISTFLOW_DISPLAY_ACTIVE);
            wristflow_display_tick(&p, start + dim, timeouts[i], false);
            assert(p.phase == WRISTFLOW_DISPLAY_DIM && wristflow_display_brightness(&p, 27) == 5);
            wristflow_display_tick(&p, start + timeouts[i] * 1000 - 1, timeouts[i], false);
            assert(p.phase == WRISTFLOW_DISPLAY_DIM);
            wristflow_display_tick(&p, start + timeouts[i] * 1000, timeouts[i], false);
            assert(p.phase == WRISTFLOW_DISPLAY_OFF && wristflow_display_brightness(&p, 27) == 0);
            assert(!wristflow_display_touch(&p, start + 61000, true));
            assert(!wristflow_display_touch(&p, start + 61001, false));
            assert(p.phase == WRISTFLOW_DISPLAY_OFF);
            assert(!wristflow_display_key(&p, start + 62000));
            assert(p.phase == WRISTFLOW_DISPLAY_ACTIVE && wristflow_display_brightness(&p, 27) == 27);
            assert(wristflow_display_key(&p, start + 62001));
        }
    }
    wristflow_display_init(&p, 0);
    wristflow_display_tick(&p, 5000, 10, false);
    assert(!wristflow_display_touch(&p, 5001, true));
    assert(p.phase == WRISTFLOW_DISPLAY_ACTIVE);
    assert(!wristflow_display_touch(&p, 6000, true));
    assert(!wristflow_display_touch(&p, 6001, false));
    assert(wristflow_display_touch(&p, 6100, true));
    assert(wristflow_display_touch(&p, 6101, false));
    for (unsigned minutes = 5; minutes <= 20; minutes += 5) {
        wristflow_display_init(&p, UINT_MAX - 1000U);
        uint32_t start = p.activity_at;
        assert(wristflow_display_keep(&p, start, minutes));
        wristflow_display_touch(&p, start + minutes * 60000U - 1, true);
        wristflow_display_tick(&p, start + minutes * 60000U, 5, false);
        assert(!p.keep_ms && p.phase == WRISTFLOW_DISPLAY_ACTIVE);
        wristflow_display_tick(&p, start + minutes * 60000U + 3999, 5, false);
        assert(p.phase == WRISTFLOW_DISPLAY_ACTIVE);
        wristflow_display_tick(&p, start + minutes * 60000U + 4000, 5, false);
        assert(p.phase == WRISTFLOW_DISPLAY_DIM);
    }
    assert(!wristflow_display_keep(&p, 0, 1));
    wristflow_display_init(&p, 0);
    assert(wristflow_display_keep(&p, 0, 5));
    assert(wristflow_display_keep(&p, 10000, 0));
    wristflow_display_tick(&p, 13999, 5, false); assert(p.phase == WRISTFLOW_DISPLAY_ACTIVE);
    wristflow_display_tick(&p, 14000, 5, true);
    wristflow_display_tick(&p, 90000, 5, true);
    assert(p.phase == WRISTFLOW_DISPLAY_ACTIVE && wristflow_display_brightness(&p, 27) == 100);
    wristflow_display_tick(&p, 90001, 5, false);
    assert(wristflow_display_brightness(&p, 27) == 27);
    wristflow_display_tick(&p, 94000, 5, false); assert(p.phase == WRISTFLOW_DISPLAY_ACTIVE);
    wristflow_display_tick(&p, 94001, 5, false); assert(p.phase == WRISTFLOW_DISPLAY_DIM);
    return 0;
}
