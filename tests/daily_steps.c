#ifdef NDEBUG
#undef NDEBUG
#endif
#include "daily_steps.h"
#include "product_state.h"
#include <assert.h>

static wristflow_steps_clock_t clock_at(uint32_t utc, uint64_t ms)
{ return (wristflow_steps_clock_t){.utc=utc,.monotonic_ms=ms,.valid=true}; }
int main(void)
{
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
    assert(!state.snapshot.metric.goal_valid && !state.snapshot.metric.hours_valid);
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
    clock.valid=true; clock.utc+=2; clock.monotonic_ms+=1000;
    assert(wristflow_daily_steps_observe(&state,324,clock));
    assert(state.snapshot.metric.total_valid && !state.snapshot.metric.total);
    /* UTC16:00 is the existing fixed UTC+8 day boundary. */
    wristflow_daily_steps_init(&state);
    clock=clock_at(1792425599,1000);
    assert(wristflow_daily_steps_observe(&state,65000,clock));
    uint32_t old_day=state.snapshot.observation.local_day;
    clock.utc++; clock.monotonic_ms+=1000;
    assert(wristflow_daily_steps_observe(&state,65002,clock));
    assert(state.snapshot.observation.local_day==old_day+1 && !state.snapshot.metric.total);
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
    clock.utc+=301; clock.monotonic_ms+=300001;
    assert(!wristflow_daily_steps_observe(&state,603,clock));
    return 0;
}
