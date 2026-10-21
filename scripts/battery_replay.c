/* Host-only policy experiments. The baseline calls the production estimator.
 * Nothing in this file is linked into Product or UI Demo. */
#include "battery_estimate.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    wristflow_battery_estimator_t state, saved;
    uint32_t invalid_since, pending_since, pending_sequence;
    unsigned pending_count;
    uint8_t pending_percent;
    bool suspended, pending;
} experiment_t;

static void evaluate(experiment_t *e, unsigned policy,
    wristflow_power_snapshot_t power, uint32_t now, unsigned index)
{
    bool fresh = power.voltage_valid && power.voltage_sequence &&
        now-power.voltage_sampled_ms <= WF_BATTERY_SAMPLE_MAX_AGE_MS &&
        power.millivolts >= 3000 && power.millivolts <= 4250;
    bool discharge = power.input_valid && !power.input_good &&
        (power.charge == WF_POWER_CHARGE_IDLE || power.charge == WF_POWER_CHARGE_STOPPED);
    /* Experimental 30s retention applies only to a missing sample, with a new
     * valid discharge sample on return. Stale/range/mode changes discard it. */
    bool gap_allowed = !power.input_good &&
        (power.charge == WF_POWER_CHARGE_IDLE || power.charge == WF_POWER_CHARGE_STOPPED);
    if ((policy & 1U) && !fresh && !power.voltage_sequence && e->state.seeded &&
        e->state.mode == 0 && gap_allowed) {
        e->saved = e->state;
        e->invalid_since = now;
        e->suspended = true;
    }
    if (e->suspended && fresh) {
        if (discharge && now-e->invalid_since <= 30000U &&
            power.voltage_sequence != e->saved.sequence)
            e->state = e->saved;
        e->suspended = false;
    }
    if (e->suspended && (!gap_allowed || (power.voltage_sequence && !fresh) ||
        now-e->invalid_since > 30000U)) e->suspended = false;

    wristflow_battery_diagnostic_t d;
    wristflow_battery_estimate_diagnosed(&e->state, &power, now, &d);
    /* Experimental convergence: an unchanged one-point candidate across at
     * least three distinct ADC samples AND 60s. Cached UI rows never count. */
    int difference = (int)d.candidate-(int)e->state.percent;
    if ((policy & 2U) && power.estimate_valid && d.decision == WF_BATTERY_NEW_SAMPLE &&
        (difference == 1 || difference == -1)) {
        if (!e->pending || e->pending_percent != d.candidate) {
            e->pending = true;
            e->pending_percent = d.candidate;
            e->pending_since = now;
            e->pending_count = 0;
            e->pending_sequence = 0;
        }
        if (e->pending_sequence != power.voltage_sequence) {
            e->pending_sequence = power.voltage_sequence;
            e->pending_count++;
        }
        if (e->pending_count >= 3 && now-e->pending_since >= 60000U) {
            e->state.percent = d.candidate;
            power.estimated_percent = d.candidate;
            e->pending = false;
        }
    } else if (!power.estimate_valid || !difference ||
        d.decision == WF_BATTERY_SEED || d.decision == WF_BATTERY_MODE_SEED) {
        e->pending = false;
    }
    printf("%u %u %u %u %" PRIu32 " %u %u %u %u\n", index, policy,
        power.estimate_valid, power.estimated_percent,
        d.filtered_decimillivolts, d.filter_valid, d.candidate, d.mode, d.decision);
}

int main(void)
{
    experiment_t experiments[4] = {0};
    unsigned seeded, mode, percent;
    uint32_t sequence, filtered;
    if (scanf("%u %u %u %" SCNu32 " %" SCNu32,
        &seeded, &mode, &percent, &sequence, &filtered) != 5) return 2;
    for (unsigned i=0; i<4; ++i)
        experiments[i].state = (wristflow_battery_estimator_t){
            .seeded=seeded != 0, .mode=(uint8_t)mode, .percent=(uint8_t)percent,
            .sequence=sequence, .filtered_decimillivolts=filtered};
    unsigned index, mv, raw, usb, good, charge;
    uint32_t now, adc;
    int fields;
    while ((fields=scanf("%u %" SCNu32 " %" SCNu32 " %" SCNu32 " %u %u %u %u %u",
        &index, &now, &adc, &sequence, &mv, &raw, &usb, &good, &charge)) == 9) {
        wristflow_power_snapshot_t power = {.voltage_valid=raw != 0,
            .millivolts=(uint16_t)mv, .voltage_sampled_ms=adc, .voltage_sequence=sequence,
            .input_valid=usb != 0, .input_good=good != 0,
            .charge=(wristflow_charge_display_t)charge};
        for (unsigned i=0; i<4; ++i) evaluate(&experiments[i], i, power, now, index);
    }
    return fields == EOF ? 0 : 2;
}
