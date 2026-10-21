/* Guarded HOST experiments only; never linked into firmware. */
#ifndef WRISTFLOW_HOST_BATTERY_POLICY_H
#define WRISTFLOW_HOST_BATTERY_POLICY_H
#include "battery_estimate.h"

typedef struct {
    uint32_t retain_ms, settle_ms, max_sample_gap_ms;
    unsigned settle_samples;
} battery_policy_config_t;
typedef struct {
    wristflow_battery_estimator_t state, saved, validity_gate;
    uint32_t last_adc_ms, pending_first_ms, pending_last_ms;
    unsigned pending_count;
    uint8_t pending_percent;
    bool have_adc, suspended, pending;
} battery_policy_t;
typedef struct { bool restored, history_reset, settled; } battery_policy_events_t;

/* Caller must validate ordered observation/ADC identities as the guarded
 * battery_replay CLI does. All elapsed intervals must be less than 2^31ms. */
void battery_policy_evaluate(battery_policy_t *experiment, unsigned policy,
    const battery_policy_config_t *config, wristflow_power_snapshot_t *power,
    uint32_t now, wristflow_battery_diagnostic_t *diagnostic, battery_policy_events_t *events);
#endif
