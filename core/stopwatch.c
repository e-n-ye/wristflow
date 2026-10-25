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
    *watch = (wristflow_stopwatch_t){.last_tick = now};
}

bool wristflow_stopwatch_lap(wristflow_stopwatch_t *watch, uint32_t now)
{
    if (!watch->running || watch->lap_count >= WRISTFLOW_STOPWATCH_MAX_LAPS) return false;
    wristflow_stopwatch_update(watch, now);
    watch->laps[watch->lap_count++] = watch->elapsed_ms;
    return true;
}
