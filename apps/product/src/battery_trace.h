#ifndef WRISTFLOW_BATTERY_TRACE_H
#define WRISTFLOW_BATTERY_TRACE_H
#include "battery_estimate.h"
#include <stddef.h>

#define WF_BATTERY_TRACE_CAPACITY 256U
#define WF_BATTERY_TRACE_MAX_SECONDS 900U
enum {
    WF_BATTERY_UI_VALID=1U, WF_BATTERY_BRIGHTNESS_VALID=2U,
    WF_BATTERY_LCD_ON=4U, WF_BATTERY_BRIGHTNESS_DEFERRED=8U,
    WF_BATTERY_CHARGING_OVERLAY=16U, WF_BATTERY_REMINDER_OVERLAY=32U
};
typedef struct {
    uint16_t surface;
    uint8_t phase, flags, requested_brightness, acknowledged_brightness;
} wristflow_battery_context_t;
typedef struct {
    uint32_t observed_ms;
    bool adc_valid; /* Before the estimator retracts stale display validity. */
    wristflow_power_snapshot_t power;
    wristflow_battery_diagnostic_t diagnostic;
    wristflow_battery_context_t context;
} wristflow_battery_record_t;
typedef enum {
    WF_BATTERY_TRACE_IDLE, WF_BATTERY_TRACE_RUNNING, WF_BATTERY_TRACE_STOPPED,
    WF_BATTERY_TRACE_EXPIRED, WF_BATTERY_TRACE_FULL
} wristflow_battery_trace_state_t;
typedef struct {
    uint32_t session, started_ms, duration_ms;
    uint16_t count;
    wristflow_battery_trace_state_t state;
} wristflow_battery_trace_status_t;
typedef struct {
    wristflow_battery_trace_status_t status;
    wristflow_battery_record_t records[WF_BATTERY_TRACE_CAPACITY];
} wristflow_battery_trace_t;

/* Caller synchronizes all access. No I/O, allocation, timer or wake request.
 * Deadline is logical: the next observe/status/control access notices expiry;
 * no record at/after the deadline is accepted, including after long sleep. */
bool wristflow_battery_trace_start(wristflow_battery_trace_t *trace, uint32_t now, unsigned seconds);
void wristflow_battery_trace_expire(wristflow_battery_trace_t *trace, uint32_t now);
void wristflow_battery_trace_stop(wristflow_battery_trace_t *trace, uint32_t now);
void wristflow_battery_trace_observe(wristflow_battery_trace_t *trace, const wristflow_battery_record_t *record,
    uint32_t now_at_lock);
bool wristflow_battery_trace_read(const wristflow_battery_trace_t *trace, uint32_t session,
    unsigned index, wristflow_battery_record_t *record);
#endif
