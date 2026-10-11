#include "alarm_schedule.h"
#include <string.h>
#include <limits.h>

#define DAY_SECONDS 86400U
#define LOCAL_OFFSET 28800U
#define MAX_WAIT_MS 300000U

uint32_t wristflow_alarm_local_day(uint32_t utc)
{ return (uint32_t)(((uint64_t)utc + LOCAL_OFFSET) / DAY_SECONDS); }

static bool valid_clock(wristflow_alarm_clock_t clock)
{ return clock.valid && clock.utc >= WRISTFLOW_TIME_MIN && clock.utc <= WRISTFLOW_TIME_MAX && clock.subsecond_ms < 1000; }

bool wristflow_alarm_data_valid(const wristflow_alarm_data_t *data)
{
    if (!data || data->alarms.count > WRISTFLOW_ALARM_CAPACITY) return false;
    uint32_t previous = 0;
    for (unsigned i = 0; i < data->alarms.count; ++i) {
        const wristflow_alarm_t *alarm = &data->alarms.entries[i];
        const wristflow_alarm_date_t *date = &data->dates[i];
        if (!wristflow_alarm_valid(alarm) || alarm->id <= previous ||
            (data->alarms.next_id && alarm->id >= data->alarms.next_id) ||
            (date->once_utc && (alarm->weekdays || date->once_utc < WRISTFLOW_TIME_MIN || date->once_utc > WRISTFLOW_TIME_MAX ||
                ((uint64_t)date->once_utc + LOCAL_OFFSET) % DAY_SECONDS != alarm->hour * 3600U + alarm->minute * 60U)) ||
            (date->last_fired_day && (date->last_fired_day < wristflow_alarm_local_day(WRISTFLOW_TIME_MIN) ||
                date->last_fired_day > wristflow_alarm_local_day(WRISTFLOW_TIME_MAX)))) return false;
        previous = alarm->id;
    }
    return true;
}

void wristflow_alarm_schedule_init(wristflow_alarm_schedule_t *state)
{
    memset(state, 0, sizeof *state);
    wristflow_alarms_init(&state->data.alarms);
}

static uint32_t next_time(const wristflow_alarm_t *alarm, uint32_t last_day, uint32_t utc)
{
    uint32_t day = wristflow_alarm_local_day(utc);
    if (alarm->weekdays && day <= last_day) day = last_day + 1;
    for (unsigned i = 0; i < 8; ++i, ++day) {
        /* Unix epoch day 0 was Thursday: Monday is bit 0. */
        if (alarm->weekdays && !(alarm->weekdays & (1U << ((day + 3U) % 7U)))) continue;
        uint64_t local = (uint64_t)day * DAY_SECONDS + alarm->hour * 3600U + alarm->minute * 60U;
        uint64_t candidate = local - LOCAL_OFFSET;
        if (candidate > utc && candidate <= WRISTFLOW_TIME_MAX) return (uint32_t)candidate;
    }
    return 0;
}

static void plan(wristflow_alarm_schedule_t *state, unsigned i, wristflow_alarm_clock_t clock)
{
    wristflow_alarm_t *alarm = &state->data.alarms.entries[i];
    wristflow_alarm_date_t *date = &state->data.dates[i];
    uint32_t next = 0;
    if (alarm->enabled && valid_clock(clock)) {
        if (alarm->weekdays) next = next_time(alarm, date->last_fired_day, clock.utc);
        else {
            if (!date->once_utc) {
                date->once_utc = next_time(alarm, 0, clock.utc);
                if (date->once_utc) ++state->version;
            }
            if (date->once_utc && date->once_utc <= clock.utc) {
                alarm->enabled = false; ++state->version;
            } else next = date->once_utc;
        }
    }
    state->next_utc[i] = next;
    state->due_ms[i] = next ? state->calendar_origin_ms + (uint64_t)(next - state->calendar_origin_utc) * 1000U : 0;
}

bool wristflow_alarm_schedule_update(wristflow_alarm_schedule_t *state, wristflow_alarm_clock_t clock)
{
    bool valid = valid_clock(clock), jump = !state->observed || valid != state->time_valid ||
        clock.revision != state->previous.revision;
    if (!jump && valid) {
        uint64_t elapsed = clock.monotonic_ms - state->previous.monotonic_ms;
        int64_t drift = (int64_t)clock.utc - state->previous.utc - (int64_t)(elapsed / 1000U);
        if (clock.monotonic_ms < state->previous.monotonic_ms || drift < -2 || drift > 2) jump = true;
    }
    if (jump) {
        ++state->version;
        state->calendar_origin_utc = clock.utc + 1U;
        state->calendar_origin_ms = clock.monotonic_ms + 1000U - clock.subsecond_ms;
    }
    state->observed = true; state->time_valid = valid; state->previous = clock;
    bool arrived = false;
    for (unsigned i = 0; i < state->data.alarms.count; ++i) {
        if (jump) { plan(state, i, clock); continue; }
        if (!valid || !state->next_utc[i] || clock.utc < state->next_utc[i]) continue;
        wristflow_alarm_t *alarm = &state->data.alarms.entries[i];
        wristflow_alarm_pending_t *pending = &state->pending[i];
        uint32_t due = state->next_utc[i];
        if (!pending->active) {
            pending->alarm = *alarm;
            pending->due_ms = state->due_ms[i] < clock.monotonic_ms ? state->due_ms[i] : clock.monotonic_ms;
            pending->active = true;
        }
        if (++state->sequence == 0) ++state->sequence;
        pending->sequence = state->sequence;
        state->data.dates[i].last_fired_day = wristflow_alarm_local_day(due);
        if (!alarm->weekdays) alarm->enabled = false;
        ++state->version; arrived = true;
        plan(state, i, clock);
    }
    return arrived;
}

bool wristflow_alarm_schedule_command(wristflow_alarm_schedule_t *state,
    const wristflow_alarm_command_t *command, wristflow_alarm_clock_t clock)
{
    if (!command || command->operation > WF_ALARM_REMOVE || (command->operation == WF_ALARM_ADD && command->id)) return false;
    if (!state->observed) wristflow_alarm_schedule_update(state, clock);
    unsigned index = state->data.alarms.count;
    for (unsigned i = 0; i < state->data.alarms.count; ++i)
        if (state->data.alarms.entries[i].id == command->id) { index = i; break; }
    if (command->operation != WF_ALARM_ADD && index == state->data.alarms.count) return false;
    bool accepted = false;
    switch (command->operation) {
    case WF_ALARM_ADD:
        accepted = wristflow_alarm_add(&state->data.alarms, &command->draft, NULL);
        if (accepted) state->data.dates[index] = (wristflow_alarm_date_t){0};
        break;
    case WF_ALARM_EDIT:
        accepted = wristflow_alarm_edit(&state->data.alarms, command->id, &command->draft);
        if (accepted) state->data.dates[index].once_utc = 0;
        break;
    case WF_ALARM_ENABLE:
        accepted = wristflow_alarm_enable(&state->data.alarms, command->id, command->enabled);
        if (accepted) {
            if (command->enabled) state->data.dates[index].once_utc = 0;
            else state->pending[index].active = false;
        }
        break;
    case WF_ALARM_REMOVE:
        accepted = wristflow_alarm_remove(&state->data.alarms, command->id);
        if (accepted) {
            for (unsigned i = index; i < state->data.alarms.count; ++i) {
                state->data.dates[i] = state->data.dates[i + 1];
                state->pending[i] = state->pending[i + 1];
                state->next_utc[i] = state->next_utc[i + 1];
                state->due_ms[i] = state->due_ms[i + 1];
            }
            unsigned tail = state->data.alarms.count;
            state->data.dates[tail] = (wristflow_alarm_date_t){0};
            state->pending[tail] = (wristflow_alarm_pending_t){0};
            state->next_utc[tail] = 0; state->due_ms[tail] = 0;
        }
        break;
    }
    if (!accepted) return false;
    ++state->version;
    if (command->operation != WF_ALARM_REMOVE) plan(state, index, clock);
    return true;
}

bool wristflow_alarm_schedule_ack(wristflow_alarm_schedule_t *state, uint32_t id, uint32_t sequence)
{
    for (unsigned i = 0; i < state->data.alarms.count; ++i)
        if (state->data.alarms.entries[i].id == id && state->pending[i].active && state->pending[i].sequence == sequence) {
            state->pending[i].active = false; ++state->version; return true;
        }
    return false;
}

uint32_t wristflow_alarm_schedule_wait(const wristflow_alarm_schedule_t *state, wristflow_alarm_clock_t clock)
{
    uint32_t wait = MAX_WAIT_MS;
    if (!valid_clock(clock)) return wait;
    for (unsigned i = 0; i < state->data.alarms.count; ++i) if (state->next_utc[i]) {
        uint64_t delay = state->next_utc[i] > clock.utc ? (uint64_t)(state->next_utc[i] - clock.utc) * 1000U : 1U;
        if (state->due_ms[i] > clock.monotonic_ms && state->due_ms[i] - clock.monotonic_ms < delay)
            delay = state->due_ms[i] - clock.monotonic_ms;
        if (delay < wait) wait = (uint32_t)delay;
    }
    return wait;
}
