#ifndef WRISTFLOW_POWER_STATE_H
#define WRISTFLOW_POWER_STATE_H

#include <stdbool.h>
#include <stdint.h>

/* Independent observations: neither PG nor VBAT proves battery presence/SOC. */
typedef enum {
    WF_POWER_CHARGE_UNKNOWN, WF_POWER_CHARGE_IDLE, WF_POWER_CHARGE_ACTIVE,
    WF_POWER_CHARGE_FULL, WF_POWER_CHARGE_STOPPED, WF_POWER_CHARGE_FAULT
} wristflow_charge_display_t;
typedef enum {
    WF_POWER_CHARGE_REASON_NONE, WF_POWER_CHARGE_REASON_MANUAL, WF_POWER_CHARGE_REASON_TIMEOUT,
    WF_POWER_CHARGE_REASON_COMPLETE, WF_POWER_CHARGE_REASON_IO, WF_POWER_CHARGE_REASON_PROFILE,
    WF_POWER_CHARGE_REASON_INPUT, WF_POWER_CHARGE_REASON_FAULT, WF_POWER_CHARGE_REASON_VOLTAGE,
    WF_POWER_CHARGE_REASON_REGULATION
} wristflow_charge_display_reason_t;
/* Charger worker's debounced PG observation, independent of screen/ADC. Only
 * a confirmed low followed by high advances insertions; boot-high does not. */
typedef struct {
    bool valid, good;
    uint32_t insertions;
} wristflow_input_observation_t;
typedef struct {
    bool input_valid;
    bool input_good;
    bool voltage_valid;
    uint16_t millivolts;
    wristflow_charge_display_t charge;
    wristflow_charge_display_reason_t charge_reason;
    /* First confirmed automatic ACTIVE in an input cycle; zero after confirmed
     * unplug. Status IRQs never allocate a new identity. Not persistent. */
    uint32_t charge_cycle;
    wristflow_input_observation_t input_observation;
    /* Identity/time of an actual ADC completion, not of a UI read or PG update. */
    uint32_t voltage_sequence, voltage_sampled_ms;
    bool estimate_valid;
    uint8_t estimated_percent;
} wristflow_power_snapshot_t;

bool wristflow_power_equal(const wristflow_power_snapshot_t *a, const wristflow_power_snapshot_t *b);
const char *wristflow_power_input_text(const wristflow_power_snapshot_t *power);
const char *wristflow_power_charge_text(const wristflow_power_snapshot_t *power);
const char *wristflow_power_charge_reason_text(const wristflow_power_snapshot_t *power);

#endif
