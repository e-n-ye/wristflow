#include "battery_estimate.h"
#include <stddef.h>

/* Every fifth percentage point from SiFli-SDK @421126d9, Huangshan
 * sf32lb52-lchspi-ulp_base/battery_table.c. Values are 0.1mV: the locked SDK
 * battery_calculator.md example explicitly identifies 38000 as 3.8V.
 * These are vendor reference points, NOT measurements of our ZCLP500 cell.
 * Keep the reference data separate from charging/protection thresholds. */
static const uint16_t discharge[] = {
    35042,36813,37194,37605,37962,38198,38387,38517,38612,38748,39002,
    39390,39606,39778,39958,40086,40131,40195,40317,40647,41808
};
static const uint16_t charging[] = {
    35577,37276,37657,38068,38425,38661,38850,38980,39075,39211,39465,
    39853,40069,40241,40421,40549,40594,40658,40780,41110,41998
};
static uint8_t lookup(uint32_t voltage, bool charge)
{
    const uint16_t *table = charge ? charging : discharge;
    if (voltage <= table[0]) return 0;
    for (unsigned i=1; i<21; ++i) {
        if (voltage <= table[i])
            return (uint8_t)((i-1U)*5U + (voltage-table[i-1])*5U/(table[i]-table[i-1]));
    }
    return 100;
}
uint8_t wristflow_battery_reference_percent(uint16_t millivolts, bool charge)
{ return lookup((uint32_t)millivolts*10U, charge); }

void wristflow_battery_estimate(wristflow_battery_estimator_t *state,
    wristflow_power_snapshot_t *power, uint32_t now_ms)
{ wristflow_battery_estimate_diagnosed(state, power, now_ms, NULL); }

void wristflow_battery_estimate_diagnosed(wristflow_battery_estimator_t *state,
    wristflow_power_snapshot_t *power, uint32_t now_ms, wristflow_battery_diagnostic_t *diagnostic)
{
    wristflow_battery_diagnostic_t report = {.evaluated_ms=now_ms, .mode=255};
    power->estimate_valid = false;
    power->estimated_percent = 0;
    bool stale = !power->voltage_sequence ||
        now_ms-power->voltage_sampled_ms > WF_BATTERY_SAMPLE_MAX_AGE_MS;
    if (!power->voltage_valid || stale || power->millivolts < 3000U || power->millivolts > 4250U) {
        report.decision = !power->voltage_sequence ? WF_BATTERY_NO_SEQUENCE :
            stale ? WF_BATTERY_STALE : !power->voltage_valid ? WF_BATTERY_NO_VOLTAGE : WF_BATTERY_RANGE;
        *state = (wristflow_battery_estimator_t){0};
        if (stale) power->voltage_valid = false;
        goto done;
    }
    uint8_t mode = power->charge == WF_POWER_CHARGE_FULL ? 2U :
                   power->charge == WF_POWER_CHARGE_ACTIVE ? 1U : 0U;
    /* An independent ADC can publish before slow PG I/O. Do not guess the
     * curve or reset history in that gap; consume it after mode is known. */
    if (!mode && (!power->input_valid || power->charge == WF_POWER_CHARGE_UNKNOWN ||
        power->charge == WF_POWER_CHARGE_FAULT)) {
        report.decision = WF_BATTERY_MODE_UNKNOWN;
        goto done;
    }
    report.mode = mode;
    if (state->seeded && state->mode != mode &&
        state->sequence == power->voltage_sequence && mode != 2U) {
        report.decision = WF_BATTERY_MODE_WAIT;
        goto done;
    }
    report.decision = WF_BATTERY_CACHED;
    if (!state->seeded || state->sequence != power->voltage_sequence || state->mode != mode) {
        uint32_t voltage = (uint32_t)power->millivolts*10U;
        bool reset = !state->seeded || state->mode != mode;
        report.decision = !state->seeded ? WF_BATTERY_SEED :
            reset ? WF_BATTERY_MODE_SEED : WF_BATTERY_NEW_SAMPLE;
        state->filtered_decimillivolts = reset ? voltage :
            (state->filtered_decimillivolts*3U+voltage+2U)/4U;
        uint8_t candidate = lookup(state->filtered_decimillivolts, mode == 1U);
        if (candidate > 99U) candidate = 99U; /* Voltage alone never declares FULL. */
        int difference = (int)candidate-(int)state->percent;
        if (reset || difference >= 2 || difference <= -2 || candidate == 0 || candidate == 99)
            state->percent = candidate;
        if (mode == 2U) state->percent = 100;
        state->mode = mode;
        state->sequence = power->voltage_sequence;
        state->seeded = true;
    }
    power->estimate_valid = true;
    power->estimated_percent = state->percent;
    report.filter_valid = true;
    report.filtered_decimillivolts = state->filtered_decimillivolts;
    report.candidate = lookup(state->filtered_decimillivolts, mode == 1U);
    if (report.candidate > 99U) report.candidate = 99U;
done:
    if (diagnostic) *diagnostic = report;
}
