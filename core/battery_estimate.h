#ifndef WRISTFLOW_BATTERY_ESTIMATE_H
#define WRISTFLOW_BATTERY_ESTIMATE_H
#include "power_state.h"

#define WF_BATTERY_SAMPLE_MAX_AGE_MS 15000U
typedef struct {
    bool seeded;
    uint8_t mode, percent;
    uint32_t sequence, filtered_decimillivolts;
} wristflow_battery_estimator_t;

/* UI-thread owned. Consumes each ADC sequence once; no device access or clock
 * advancement. Reference voltage estimate, not a calibrated capacity gauge. */
void wristflow_battery_estimate(wristflow_battery_estimator_t *state,
    wristflow_power_snapshot_t *power, uint32_t now_ms);
uint8_t wristflow_battery_reference_percent(uint16_t millivolts, bool charging);
#endif
