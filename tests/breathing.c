#include "breathing.h"
#include <assert.h>
#include <stdint.h>

int main(void)
{
    wristflow_breathing_t s;
    wristflow_breathing_init(&s);
    assert(s.minutes == 1 && s.phase == WRISTFLOW_BREATHING_IDLE);
    assert(!wristflow_breathing_set_minutes(&s, 0));
    assert(!wristflow_breathing_set_minutes(&s, 6));
    for (unsigned minutes = 1; minutes <= 5; ++minutes) {
        assert(wristflow_breathing_set_minutes(&s, minutes));
        uint32_t base = UINT32_MAX - 2000U;
        wristflow_breathing_start(&s, base);
        assert(!wristflow_breathing_set_minutes(&s, 2));
        unsigned switches = 0;
        bool inhale = true;
        for (unsigned ms = 1; ms < minutes * 60000U; ++ms) {
            wristflow_breathing_update(&s, base + ms);
            bool current = wristflow_breathing_inhaling(&s);
            if (current != inhale) { ++switches; inhale = current; }
            assert(wristflow_breathing_expansion(&s) <= 1000);
        }
        assert(switches == minutes * 14U - 1U);
        assert(s.phase == WRISTFLOW_BREATHING_RUNNING);
        wristflow_breathing_update(&s, base + minutes * 60000U);
        assert(s.phase == WRISTFLOW_BREATHING_COMPLETE && s.elapsed_ms == minutes * 60000U);
        wristflow_breathing_update(&s, base + minutes * 60000U + 100000);
        assert(s.elapsed_ms == minutes * 60000U);
        wristflow_breathing_end(&s);
    }
    assert(wristflow_breathing_set_minutes(&s, 1));
    wristflow_breathing_start(&s, 100);
    wristflow_breathing_update(&s, 4385);
    assert(wristflow_breathing_inhaling(&s));
    wristflow_breathing_update(&s, 4386);
    assert(!wristflow_breathing_inhaling(&s));
    wristflow_breathing_pause(&s, 10100);
    wristflow_breathing_pause(&s, 20100);
    wristflow_breathing_update(&s, 30100);
    assert(s.elapsed_ms == 10000 && s.phase == WRISTFLOW_BREATHING_PAUSED);
    wristflow_breathing_resume(&s, 100000);
    assert(wristflow_breathing_inhaling(&s) && wristflow_breathing_expansion(&s) == 0);
    wristflow_breathing_resume(&s, 101000); /* Idempotent: no rhythm or time reset. */
    wristflow_breathing_update(&s, 149999);
    assert(s.elapsed_ms == 59999 && s.phase == WRISTFLOW_BREATHING_RUNNING);
    wristflow_breathing_pause(&s, 150000); /* Deadline wins over a simultaneous pause. */
    assert(s.phase == WRISTFLOW_BREATHING_COMPLETE);
    wristflow_breathing_start(&s, 200000); /* Immediate restart also works after completion. */
    assert(s.elapsed_ms == 0 && s.phase == WRISTFLOW_BREATHING_RUNNING);
    wristflow_breathing_update(&s, 500000); /* Delayed sampling clamps at the deadline. */
    assert(s.elapsed_ms == 60000 && s.phase == WRISTFLOW_BREATHING_COMPLETE);
    wristflow_breathing_end(&s);
    assert(s.minutes == 1 && s.elapsed_ms == 0 && s.phase == WRISTFLOW_BREATHING_IDLE);
    return 0;
}
