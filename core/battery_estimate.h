#ifndef WRISTFLOW_BATTERY_ESTIMATE_H
#define WRISTFLOW_BATTERY_ESTIMATE_H
#include "power_state.h"

#define WF_BATTERY_SAMPLE_MAX_AGE_MS 15000U
#define WF_BATTERY_SETTLE_MS 60000U
#define WF_BATTERY_SETTLE_MIN_SAMPLES 3U
typedef struct {
    bool seeded;
    uint8_t mode, percent;
    uint32_t sequence, filtered_decimillivolts, sampled_ms;
    uint32_t settling_since_ms;
    uint8_t settling_candidate, settling_count;
} wristflow_battery_estimator_t;

typedef enum {
    WF_BATTERY_NEW_SAMPLE, WF_BATTERY_SEED, WF_BATTERY_MODE_SEED,
    WF_BATTERY_CACHED, WF_BATTERY_NO_SEQUENCE, WF_BATTERY_STALE,
    WF_BATTERY_NO_VOLTAGE, WF_BATTERY_RANGE, WF_BATTERY_MODE_UNKNOWN,
    WF_BATTERY_MODE_WAIT
} wristflow_battery_decision_t;
typedef struct {
    wristflow_battery_decision_t decision;
    uint32_t evaluated_ms, filtered_decimillivolts;
    uint8_t mode, candidate; /* mode: 0 discharge, 1 charging, 2 FULL, 255 unknown */
    bool filter_valid;
} wristflow_battery_diagnostic_t;

/* UI-thread owned. Consumes each ADC sequence once; no device access or clock
 * advancement. Reference voltage estimate, not a calibrated capacity gauge. */
void wristflow_battery_estimate(wristflow_battery_estimator_t *state,
    wristflow_power_snapshot_t *power, uint32_t now_ms);
/* Optional UI-thread diagnostic copy; identical estimate and state transitions. */
void wristflow_battery_estimate_diagnosed(wristflow_battery_estimator_t *state,
    wristflow_power_snapshot_t *power, uint32_t now_ms, wristflow_battery_diagnostic_t *diagnostic);
uint8_t wristflow_battery_reference_percent(uint16_t millivolts, bool charging);
#endif
