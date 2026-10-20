#include "daily_steps.h"
#include "product_state.h"
#include <limits.h>
#include <string.h>

void wristflow_daily_steps_init(wristflow_daily_steps_t *state)
{ *state = (wristflow_daily_steps_t){0}; }

bool wristflow_steps_checkpoint_valid(const wristflow_steps_checkpoint_t *c)
{
    if (!c || c->hours_valid & 0xff000000U) return false;
    uint64_t sum = 0;
    for (unsigned h = 0; h < 24; ++h) {
        if (!(c->hours_valid & (1U << h)) && c->hourly[h]) return false;
        sum += c->hourly[h];
    }
    return sum <= c->total && c->saved_utc >= WRISTFLOW_TIME_MIN && c->saved_utc <= WRISTFLOW_TIME_MAX &&
        c->coverage_start_utc >= WRISTFLOW_TIME_MIN && c->coverage_start_utc <= c->saved_utc &&
        ((uint64_t)c->saved_utc + 28800U) / 86400U == c->local_day &&
        ((uint64_t)c->coverage_start_utc + 28800U) / 86400U == c->local_day;
}
bool wristflow_daily_steps_seed(wristflow_daily_steps_t *s, const wristflow_steps_checkpoint_t *c)
{
    if (!s || s->baseline || !wristflow_steps_checkpoint_valid(c)) return false;
    s->seed = *c; s->seed_pending = true;
    return true;
}

void wristflow_daily_steps_invalidate(wristflow_daily_steps_t *state)
{
    state->snapshot.metric.total_valid = false;
    state->snapshot.metric.hours_valid = 0;
    state->baseline = false;
    ++state->snapshot.observation.version;
}

bool wristflow_daily_steps_observe(wristflow_daily_steps_t *s, uint16_t native,
    wristflow_steps_clock_t clock)
{
    wristflow_steps_observation_t *o = &s->snapshot.observation;
    uint32_t day = ((uint64_t)clock.utc + 28800U) / 86400U;
    bool calendar = clock.valid && clock.utc >= WRISTFLOW_TIME_MIN && clock.utc <= WRISTFLOW_TIME_MAX;
    bool continuous = s->baseline && clock.monotonic_ms >= s->previous.monotonic_ms;
    uint64_t elapsed = continuous ? clock.monotonic_ms - s->previous.monotonic_ms : 0;
    uint16_t delta = (uint16_t)(native - s->previous_native);
    if (s->baseline && (!continuous || elapsed > WRISTFLOW_STEPS_MAX_GAP_MS ||
        delta > (elapsed * 26U + 999U) / 1000U + 32U)) {
        wristflow_daily_steps_invalidate(s);
        return false;
    }
    bool same_day = s->baseline && s->previous.valid && calendar && o->local_day == day;
    int64_t drift = (int64_t)((int64_t)clock.utc - s->previous.utc) * 1000 - (int64_t)elapsed;
    bool same_clock = same_day && clock.revision == s->previous.revision && drift >= -2000 && drift <= 2000;
    bool restored = same_day && o->restored;
    if (!calendar) {
        s->snapshot.metric.total_valid = false;
        s->snapshot.metric.hours_valid = 0;
    } else if (!same_day) {
        restored = s->seed_pending && s->seed.local_day == day && s->seed.saved_utc <= clock.utc;
        s->snapshot.metric = (wristflow_activity_metric_t){
            .total = restored ? s->seed.total : 0, .total_valid = true,
            .hours_valid = restored ? s->seed.hours_valid : 0
        };
        if (restored) memcpy(s->snapshot.metric.hourly, s->seed.hourly, sizeof s->seed.hourly);
        o->coverage_start_utc = restored ? s->seed.coverage_start_utc : clock.utc;
    } else {
        if (same_clock) {
            uint32_t total = s->snapshot.metric.total;
            uint32_t accepted = UINT32_MAX - total < delta ? UINT32_MAX - total : delta;
            s->snapshot.metric.total = total + accepted;
            /* A cross-hour interval has a known daily delta but no known split.
             * A valid bucket means observed partial intervals, not a full hour. */
            if (clock.utc / 3600U == s->previous.utc / 3600U) {
                unsigned hour = ((uint64_t)clock.utc + 28800U) / 3600U % 24U;
                s->snapshot.metric.hourly[hour] += accepted;
                s->snapshot.metric.hours_valid |= 1U << hour;
            }
        }
        s->snapshot.metric.total_valid = true;
    }
    /* A same-day RTC correction can put the old coverage timestamp in the
     * future. Keep the known count, but rebase that uncertain metadata. */
    if (calendar && o->coverage_start_utc > clock.utc) o->coverage_start_utc = clock.utc;
    if (calendar) s->seed_pending = false;
    /* Neither a missed boundary nor a clock revision invents a per-hour split. */
    *o = (wristflow_steps_observation_t){
        .sampled_ms = clock.monotonic_ms, .version = o->version + 1,
        .utc = calendar ? clock.utc : 0, .local_day = calendar ? day : 0,
        .clock_revision = clock.revision,
        .coverage_start_utc = o->coverage_start_utc, .native_count = native,
        .native_source = true, .partial_day = true, .restored = restored
    };
    s->previous = clock;
    s->previous.valid = calendar;
    s->previous_native = native;
    s->baseline = true;
    return true;
}
