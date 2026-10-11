#ifndef WRISTFLOW_ALARM_SCHEDULE_H
#define WRISTFLOW_ALARM_SCHEDULE_H
#include "alarm.h"
#include "product_state.h"

/* UTC calendar and extended monotonic milliseconds are separate clocks.
 * revision changes whenever RTC is explicitly set, even if readback fails. */
typedef struct { uint32_t utc, revision; uint64_t monotonic_ms; bool valid; uint16_t subsecond_ms; } wristflow_alarm_clock_t;
typedef struct { uint32_t once_utc, last_fired_day; } wristflow_alarm_date_t;
typedef struct {
    wristflow_alarms_t alarms;
    wristflow_alarm_date_t dates[WRISTFLOW_ALARM_CAPACITY];
} wristflow_alarm_data_t;
typedef struct {
    wristflow_alarm_t alarm;
    uint64_t due_ms;
    uint32_t sequence;
    bool active;
} wristflow_alarm_pending_t;
typedef enum { WF_ALARM_ADD, WF_ALARM_EDIT, WF_ALARM_ENABLE, WF_ALARM_REMOVE } wristflow_alarm_operation_t;
typedef struct {
    wristflow_alarm_operation_t operation;
    uint32_t id;
    wristflow_alarm_t draft;
    bool enabled;
} wristflow_alarm_command_t;
/* Single owner pure model. Pending arrivals are volatile, reconstructable until
 * conditional ACK; dates/configuration are the only persistent part. */
typedef struct {
    wristflow_alarm_data_t data;
    wristflow_alarm_pending_t pending[WRISTFLOW_ALARM_CAPACITY];
    uint32_t next_utc[WRISTFLOW_ALARM_CAPACITY], sequence;
    uint64_t due_ms[WRISTFLOW_ALARM_CAPACITY], version;
    /* One shared mapping per RTC revision. Same civil deadline always yields
     * the same monotonic timestamp, regardless of individual creation time. */
    uint64_t calendar_origin_ms;
    uint32_t calendar_origin_utc;
    wristflow_alarm_clock_t previous;
    bool observed, time_valid;
} wristflow_alarm_schedule_t;

uint32_t wristflow_alarm_local_day(uint32_t utc);
bool wristflow_alarm_data_valid(const wristflow_alarm_data_t *data);
void wristflow_alarm_schedule_init(wristflow_alarm_schedule_t *state);
/* First observation, RTC revision/validity changes and detected discontinuities
 * replan without replay. Returns true only for a NEW due arrival. */
bool wristflow_alarm_schedule_update(wristflow_alarm_schedule_t *state, wristflow_alarm_clock_t clock);
bool wristflow_alarm_schedule_command(wristflow_alarm_schedule_t *state,
    const wristflow_alarm_command_t *command, wristflow_alarm_clock_t clock);
bool wristflow_alarm_schedule_ack(wristflow_alarm_schedule_t *state, uint32_t id, uint32_t sequence);
uint32_t wristflow_alarm_schedule_wait(const wristflow_alarm_schedule_t *state, wristflow_alarm_clock_t clock);
#endif
