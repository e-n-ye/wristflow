#ifdef NDEBUG
#undef NDEBUG
#endif
#include "daily_steps.h"
#include "product_state.h"
#include <assert.h>

static wristflow_steps_clock_t clock_at(uint32_t utc, uint64_t ms)
{ return (wristflow_steps_clock_t){.utc=utc,.monotonic_ms=ms,.valid=true}; }
static void hourly_intervals(void)
{
    wristflow_daily_steps_t s;
    wristflow_daily_steps_init(&s);
    wristflow_steps_clock_t c=clock_at(1792414790,1000); /* UTC+8 20:59:50. */
    assert(wristflow_daily_steps_observe(&s,100,c) && !s.snapshot.metric.hours_valid);
    c.utc+=5; c.monotonic_ms+=5000;
    assert(wristflow_daily_steps_observe(&s,104,c));
    assert(s.snapshot.metric.total==4 && s.snapshot.metric.hourly[20]==4);
    c.utc+=10; c.monotonic_ms+=10000; /*20:59:55->21:00:05: six unassigned steps. */
    assert(wristflow_daily_steps_observe(&s,110,c));
    assert(s.snapshot.metric.total==10 && s.snapshot.metric.hours_valid==(1U<<20));
    c.utc+=5; c.monotonic_ms+=5000;
    assert(wristflow_daily_steps_observe(&s,112,c));
    assert(s.snapshot.metric.total==12 && s.snapshot.metric.hourly[21]==2);
    assert(s.snapshot.metric.hours_valid==((1U<<20)|(1U<<21)));
    ++c.revision; c.utc+=5; c.monotonic_ms+=5000;
    assert(wristflow_daily_steps_observe(&s,114,c) && s.snapshot.metric.total==12);
    assert(s.snapshot.metric.hourly[20]==4 && s.snapshot.metric.hourly[21]==2);
    c.utc+=5; c.monotonic_ms+=5000;
    assert(wristflow_daily_steps_observe(&s,116,c) && s.snapshot.metric.hourly[21]==4);
    c.utc=1792414795; ++c.revision; c.monotonic_ms+=1000; /* Same-day backward correction. */
    assert(wristflow_daily_steps_observe(&s,118,c) && s.snapshot.metric.total==14);
    ++c.utc; c.monotonic_ms+=1000;
    assert(wristflow_daily_steps_observe(&s,120,c));
    assert(s.snapshot.metric.total==16 && s.snapshot.metric.hourly[20]==6 && s.snapshot.metric.hourly[21]==4);
    wristflow_daily_steps_init(&s); c=clock_at(1792411200,1000);
    assert(wristflow_daily_steps_observe(&s,0,c)); ++c.utc; c.monotonic_ms+=1000;
    assert(wristflow_daily_steps_observe(&s,0,c));
    assert(!s.snapshot.metric.total && s.snapshot.metric.hours_valid==(1U<<20) && !s.snapshot.metric.hourly[20]);
    c.valid=false; c.monotonic_ms+=1000;
    assert(wristflow_daily_steps_observe(&s,2,c));
    assert(!s.snapshot.metric.total_valid && !s.snapshot.metric.hours_valid);
}
int main(void)
{
    hourly_intervals();
    wristflow_daily_steps_t state;
    wristflow_daily_steps_init(&state);
    assert(!state.snapshot.metric.total_valid);
    wristflow_steps_clock_t clock=clock_at(1792411200,1000);
    assert(wristflow_daily_steps_observe(&state,0,clock));
    assert(state.snapshot.metric.total_valid && !state.snapshot.metric.total);
    clock.utc+=10; clock.monotonic_ms+=10000;
    assert(wristflow_daily_steps_observe(&state,20,clock));
    assert(state.snapshot.metric.total==20);
    assert(state.snapshot.observation.native_source && state.snapshot.observation.partial_day);
    assert(!state.snapshot.metric.goal_valid && state.snapshot.metric.hours_valid==(1U<<20));
    assert(state.snapshot.metric.hourly[20]==20);
    uint32_t version=state.snapshot.observation.version;
    /* Screen-off samples extend the same total without depending on a UI tick. */
    clock.utc+=60; clock.monotonic_ms+=60000;
    assert(wristflow_daily_steps_observe(&state,120,clock));
    assert(state.snapshot.metric.total==120 && state.snapshot.observation.version>version);
    /* A same-day clock revision drops only the ambiguous interval, retaining
     * the earlier known portion. No source data gets duplicated at correction. */
    clock.utc+=60; clock.monotonic_ms+=60000; ++clock.revision;
    assert(wristflow_daily_steps_observe(&state,220,clock));
    assert(state.snapshot.metric.total==120);
    clock.utc+=60; clock.monotonic_ms+=60000;
    assert(wristflow_daily_steps_observe(&state,320,clock));
    assert(state.snapshot.metric.total==220);
    /* Invalid RTC never publishes a daily total, and first valid time starts
     * a new attributable baseline rather than importing unclocked steps. */
    clock.valid=false; clock.monotonic_ms+=1000;
    assert(wristflow_daily_steps_observe(&state,322,clock));
    assert(!state.snapshot.metric.total_valid);
    assert(!state.snapshot.metric.hours_valid);
    clock.valid=true; clock.utc+=2; clock.monotonic_ms+=1000;
    assert(wristflow_daily_steps_observe(&state,324,clock));
    assert(state.snapshot.metric.total_valid && !state.snapshot.metric.total);
    /* UTC16:00 is the existing fixed UTC+8 day boundary. */
    wristflow_daily_steps_init(&state);
    clock=clock_at(1792425597,1000);
    assert(wristflow_daily_steps_observe(&state,65000,clock));
    ++clock.utc; clock.monotonic_ms+=1000;
    assert(wristflow_daily_steps_observe(&state,65001,clock));
    assert(state.snapshot.metric.hourly[23]==1 && state.snapshot.metric.hours_valid==(1U<<23));
    uint32_t old_day=state.snapshot.observation.local_day;
    clock.utc+=2; clock.monotonic_ms+=2000;
    assert(wristflow_daily_steps_observe(&state,65002,clock));
    assert(state.snapshot.observation.local_day==old_day+1 && !state.snapshot.metric.total);
    assert(!state.snapshot.metric.hours_valid && !state.snapshot.metric.hourly[23]);
    clock.utc++; clock.monotonic_ms+=1000;
    assert(wristflow_daily_steps_observe(&state,65004,clock));
    assert(state.snapshot.metric.total==2);
    /* One native16-bit wrap in an observed interval is accumulated once. */
    clock.utc+=60; clock.monotonic_ms+=60000;
    assert(wristflow_daily_steps_observe(&state,100,clock));
    assert(state.snapshot.metric.total==634);
    /* Backward calendar change starts a fresh day baseline, never subtracts. */
    clock.utc-=86400; clock.monotonic_ms+=1000; ++clock.revision;
    assert(wristflow_daily_steps_observe(&state,102,clock));
    assert(!state.snapshot.metric.total);
    /* Counter reset or long/unbounded gap is not interpreted as huge steps. */
    clock.utc+=1; clock.monotonic_ms+=1000;
    assert(!wristflow_daily_steps_observe(&state,0,clock));
    assert(!state.snapshot.metric.total_valid);
    wristflow_daily_steps_init(&state); clock=clock_at(1792411200,1000);
    assert(wristflow_daily_steps_observe(&state,0,clock));
    clock.utc+=301; clock.monotonic_ms+=301000;
    assert(!wristflow_daily_steps_observe(&state,10,clock));
    assert(!state.snapshot.metric.total_valid);
    wristflow_daily_steps_init(&state); clock=clock_at(WRISTFLOW_TIME_MIN-1,1000);
    assert(wristflow_daily_steps_observe(&state,0,clock));
    assert(!state.snapshot.metric.total_valid);
    clock=clock_at(WRISTFLOW_TIME_MIN,500);
    assert(!wristflow_daily_steps_observe(&state,0,clock)); /* Monotonic rollback. */
    /* Same-day unannounced RTC jumps do not import an ambiguous delta. */
    wristflow_daily_steps_init(&state); clock=clock_at(1792411200,1000);
    assert(wristflow_daily_steps_observe(&state,0,clock));
    clock.utc+=100; clock.monotonic_ms+=1000;
    assert(wristflow_daily_steps_observe(&state,2,clock) && !state.snapshot.metric.total);
    /* The maximum gap is inclusive and the extended monotonic clock keeps
     * count continuity across the raw tick's32-bit wrap. */
    wristflow_daily_steps_init(&state); clock=clock_at(1792411200,UINT32_MAX-1000ULL);
    assert(wristflow_daily_steps_observe(&state,0,clock));
    clock.utc+=300; clock.monotonic_ms+=300000;
    assert(wristflow_daily_steps_observe(&state,600,clock) && state.snapshot.metric.total==600);
    state.snapshot.metric.total=UINT32_MAX-1;
    clock.utc+=1; clock.monotonic_ms+=1000;
    assert(wristflow_daily_steps_observe(&state,602,clock));
    assert(state.snapshot.metric.total==UINT32_MAX);
    assert(state.snapshot.metric.hourly[20]==601); /* Only the accepted final step fits the total. */
    clock.utc+=301; clock.monotonic_ms+=300001;
    assert(!wristflow_daily_steps_observe(&state,603,clock));
    return 0;
}
