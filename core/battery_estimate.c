#include "battery_estimate.h"
#include <stddef.h>

/* ZCLP500 usable-time model, not measured capacity or charging calibration.
 * Source: docs/evidence/2026-11-02/battery-curve-candidate.json, join3600.
 * Hundredths of a percent at 3300,3310,...,4140,4142mV, rounded once.
 * Both charge modes use this mapping by operator choice. Keep this separate
 * from charger control and protection thresholds; only FULL declares100%. */
static const uint16_t usable_percent_centi[] = {
    0,10,25,42,62,90,122,164,212,264,
    322,412,489,569,671,758,872,994,1136,1283,
    1452,1634,1848,2057,2301,2558,2834,3133,3457,3748,
    3984,4269,4490,4724,4928,5135,5291,5445,5592,5737,
    5851,5981,6104,6216,6330,6439,6541,6659,6756,6861,
    6965,7069,7178,7295,7392,7489,7600,7702,7788,7897,
    8001,8098,8205,8286,8385,8478,8570,8658,8750,8836,
    8928,9018,9101,9199,9291,9374,9459,9538,9618,9696,
    9775,9858,9912,9967,9998,10000
};
static uint8_t lookup(uint32_t voltage)
{
    if (voltage <= 33000U) return 0;
    if (voltage >= 41420U) return 100;
    unsigned i=(voltage-33000U)/100U;
    uint32_t span=i==84U ? 20U:100U; /* Last measured interval is only2mV. */
    uint32_t percent=usable_percent_centi[i]+(voltage-(33000U+i*100U))*
        (usable_percent_centi[i+1]-usable_percent_centi[i])/span;
    return (uint8_t)(percent/100U);
}
uint8_t wristflow_battery_reference_percent(uint16_t millivolts, bool charge)
{ (void)charge; return lookup((uint32_t)millivolts*10U); }

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
        state->settling_count = 0;
        report.decision = WF_BATTERY_MODE_UNKNOWN;
        goto done;
    }
    report.mode = mode;
    if (state->seeded && state->mode != mode &&
        state->sequence == power->voltage_sequence && mode != 2U) {
        state->settling_count = 0;
        report.decision = WF_BATTERY_MODE_WAIT;
        goto done;
    }
    bool discharge_mode = !mode && power->input_valid && !power->input_good &&
        (power->charge == WF_POWER_CHARGE_IDLE || power->charge == WF_POWER_CHARGE_STOPPED);
    if (!discharge_mode || (state->sequence == power->voltage_sequence &&
        state->sampled_ms != power->voltage_sampled_ms)) state->settling_count = 0;
    report.decision = WF_BATTERY_CACHED;
    if (!state->seeded || state->sequence != power->voltage_sequence || state->mode != mode) {
        uint32_t voltage = (uint32_t)power->millivolts*10U;
        bool reset = !state->seeded || state->mode != mode;
        report.decision = !state->seeded ? WF_BATTERY_SEED :
            reset ? WF_BATTERY_MODE_SEED : WF_BATTERY_NEW_SAMPLE;
        state->filtered_decimillivolts = reset ? voltage :
            (state->filtered_decimillivolts*3U+voltage+2U)/4U;
        uint8_t candidate = lookup(state->filtered_decimillivolts);
        if (candidate > 99U) candidate = 99U; /* Voltage alone never declares FULL. */
        int difference = (int)candidate-(int)state->percent;
        if (reset || difference >= 2 || difference <= -2 || candidate == 0 || candidate == 99) {
            state->percent = candidate;
            state->settling_count = 0;
        } else {
            uint32_t sequence_step = power->voltage_sequence-state->sequence;
            uint32_t sample_step = power->voltage_sampled_ms-state->sampled_ms;
            bool ordered = sequence_step && sequence_step < UINT32_C(0x80000000) &&
                sample_step && sample_step < UINT32_C(0x80000000);
            if (discharge_mode && (difference == 1 || difference == -1) && ordered) {
                /* A candidate needs fresh, ordered ADC support throughout the
                 * dwell. UI polling and delayed observations never advance it.
                 * Expiry cancels only the dwell; it never reseeds the filter. */
                if (!state->settling_count || state->settling_candidate != candidate ||
                    sample_step > WF_BATTERY_SAMPLE_MAX_AGE_MS) {
                    state->settling_since_ms = power->voltage_sampled_ms;
                    state->settling_candidate = candidate;
                    state->settling_count = 1;
                } else if (state->settling_count < WF_BATTERY_SETTLE_MIN_SAMPLES) {
                    ++state->settling_count;
                }
                if (state->settling_count >= WF_BATTERY_SETTLE_MIN_SAMPLES &&
                    power->voltage_sampled_ms-state->settling_since_ms >= WF_BATTERY_SETTLE_MS) {
                    state->percent = candidate;
                    state->settling_count = 0;
                }
            } else state->settling_count = 0;
        }
        if (mode == 2U) state->percent = 100;
        state->mode = mode;
        state->sequence = power->voltage_sequence;
        state->sampled_ms = power->voltage_sampled_ms;
        state->seeded = true;
    }
    power->estimate_valid = true;
    power->estimated_percent = state->percent;
    report.filter_valid = true;
    report.filtered_decimillivolts = state->filtered_decimillivolts;
    report.candidate = lookup(state->filtered_decimillivolts);
    if (report.candidate > 99U) report.candidate = 99U;
done:
    if (diagnostic) *diagnostic = report;
}
