#ifndef WRISTFLOW_POWER_STATE_H
#define WRISTFLOW_POWER_STATE_H

#include <stdbool.h>
#include <stdint.h>

/* Independent observations: neither PG nor VBAT proves battery presence/SOC. */
typedef struct {
    bool input_valid;
    bool input_good;
    bool voltage_valid;
    uint16_t millivolts;
} wristflow_power_snapshot_t;

bool wristflow_power_equal(const wristflow_power_snapshot_t *a, const wristflow_power_snapshot_t *b);
const char *wristflow_power_input_text(const wristflow_power_snapshot_t *power);

#endif
