#include "breathing.h"

void wristflow_breathing_init(wristflow_breathing_t *s)
{ if (s) *s = (wristflow_breathing_t){.minutes = 1}; }

bool wristflow_breathing_set_minutes(wristflow_breathing_t *s, unsigned minutes)
{
    if (!s || s->phase != WRISTFLOW_BREATHING_IDLE || minutes < 1 || minutes > 5) return false;
    s->minutes = (uint8_t)minutes;
    return true;
}

void wristflow_breathing_start(wristflow_breathing_t *s, uint32_t now)
{
    if (!s) return;
    s->phase = WRISTFLOW_BREATHING_RUNNING;
    s->elapsed_ms = s->rhythm_ms = 0;
    s->last_tick = now;
}

void wristflow_breathing_update(wristflow_breathing_t *s, uint32_t now)
{
    if (!s || s->phase != WRISTFLOW_BREATHING_RUNNING) return;
    uint32_t delta = now - s->last_tick; /* Unsigned subtraction supports tick wrap. */
    s->last_tick = now;
    uint32_t remaining = s->minutes * 60000U - s->elapsed_ms;
    if (delta >= remaining) {
        s->elapsed_ms += remaining;
        s->phase = WRISTFLOW_BREATHING_COMPLETE;
    } else {
        s->elapsed_ms += delta;
        s->rhythm_ms = (s->rhythm_ms + delta) % 60000U;
    }
}

void wristflow_breathing_pause(wristflow_breathing_t *s, uint32_t now)
{
    wristflow_breathing_update(s, now);
    if (s && s->phase == WRISTFLOW_BREATHING_RUNNING) s->phase = WRISTFLOW_BREATHING_PAUSED;
}

void wristflow_breathing_resume(wristflow_breathing_t *s, uint32_t now)
{
    if (!s || s->phase != WRISTFLOW_BREATHING_PAUSED) return;
    s->last_tick = now;
    s->rhythm_ms = 0; /* Resume visibly at the beginning of an inhale. */
    s->phase = WRISTFLOW_BREATHING_RUNNING;
}

void wristflow_breathing_end(wristflow_breathing_t *s)
{
    if (!s) return;
    s->phase = WRISTFLOW_BREATHING_IDLE;
    s->elapsed_ms = s->rhythm_ms = 0;
}

static unsigned cycle(const wristflow_breathing_t *s)
{ return s ? s->rhythm_ms * 7U % 60000U : 0; }

bool wristflow_breathing_inhaling(const wristflow_breathing_t *s)
{ return cycle(s) < 30000U; }

unsigned wristflow_breathing_expansion(const wristflow_breathing_t *s)
{
    unsigned value = cycle(s);
    return (value < 30000U ? value : 60000U - value) / 30U;
}
