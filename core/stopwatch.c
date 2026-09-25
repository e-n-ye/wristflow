#include "stopwatch.h"

void wristflow_stopwatch_update(wristflow_stopwatch_t *watch, uint32_t now)
{
    if (watch->running)
        watch->elapsed_ms += (uint32_t)(now - watch->last_tick);
    watch->last_tick = now;
}

void wristflow_stopwatch_toggle(wristflow_stopwatch_t *watch, uint32_t now)
{
    wristflow_stopwatch_update(watch, now);
    watch->running = !watch->running;
}

void wristflow_stopwatch_reset(wristflow_stopwatch_t *watch, uint32_t now)
{
    *watch = (wristflow_stopwatch_t){0, now, false};
}
