/* Host-only policy experiments. Calls the frozen17a9de5 production estimator.
 * Nothing in this file is linked into Product or UI Demo. */
#include "battery_estimate.h"
#include "battery_policy.h"
#include <errno.h>
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

static bool parameter(const char *text, uint32_t *value)
{
    char *end;
    errno = 0;
    unsigned long parsed = strtoul(text, &end, 10);
    if (errno || !*text || *end || parsed > 900000UL) return false;
    *value = (uint32_t)parsed;
    return true;
}

int main(int argc, char **argv)
{
    bool guarded = argc != 1;
    battery_policy_config_t config = {0};
    uint32_t samples;
    if (guarded && (argc != 5 || !parameter(argv[1], &config.retain_ms) ||
        !parameter(argv[2], &config.settle_ms) || !parameter(argv[3], &samples) ||
        !parameter(argv[4], &config.max_sample_gap_ms) || !config.retain_ms ||
        !config.settle_ms || samples < 2U || samples > 256U || !config.max_sample_gap_ms)) return 2;
    config.settle_samples = guarded ? samples : 0U;
    experiment_t experiments[4] = {0};
    battery_policy_t bounded[4] = {0};
    unsigned seeded, mode, percent;
    uint32_t sequence, filtered;
    if (scanf("%u %u %u %" SCNu32 " %" SCNu32,
        &seeded, &mode, &percent, &sequence, &filtered) != 5) return 2;
    uint32_t last_now = 0, last_adc = 0, last_sequence = sequence;
    unsigned last_mv = 0;
    if (guarded && scanf("%" SCNu32 " %" SCNu32 " %u", &last_now, &last_adc, &last_mv) != 3) return 2;
    for (unsigned i=0; i<4; ++i) {
        experiments[i].state = (wristflow_battery_estimator_t){
            .seeded=seeded != 0, .mode=(uint8_t)mode, .percent=(uint8_t)percent,
            .sequence=sequence, .filtered_decimillivolts=filtered};
        bounded[i].state = experiments[i].state;
        bounded[i].validity_gate = experiments[i].state;
        bounded[i].last_adc_ms = last_adc;
        bounded[i].have_adc = seeded != 0;
    }
    unsigned index, mv, raw, usb, good, charge;
    uint32_t now, adc;
    int fields;
    while ((fields=scanf("%u %" SCNu32 " %" SCNu32 " %" SCNu32 " %u %u %u %u %u",
        &index, &now, &adc, &sequence, &mv, &raw, &usb, &good, &charge)) == 9) {
        wristflow_power_snapshot_t power = {.voltage_valid=raw != 0,
            .millivolts=(uint16_t)mv, .voltage_sampled_ms=adc, .voltage_sequence=sequence,
            .input_valid=usb != 0, .input_good=good != 0,
            .charge=(wristflow_charge_display_t)charge};
        if (guarded) {
            /* Bounded modular ordering accepts one ordinary uint32 wrap, but
             * rejects backwards observations, reused/changed ADCs and replayed
             * old identities. It cannot identify unseen complete counter wraps. */
            if (now-last_now >= UINT32_C(0x80000000) || (sequence &&
                ((sequence == last_sequence && (adc != last_adc || mv != last_mv)) ||
                 (sequence != last_sequence && (sequence-last_sequence >= UINT32_C(0x80000000) ||
                                               adc-last_adc >= UINT32_C(0x80000000)))))) {
                fputs("Rejected unordered observation/ADC identity\n", stderr);
                return 2;
            }
            last_now = now;
            if (sequence) { last_sequence = sequence; last_adc = adc; last_mv = mv; }
            for (unsigned i=0; i<4; ++i) {
                wristflow_power_snapshot_t copy = power;
                wristflow_battery_diagnostic_t d;
                battery_policy_events_t events;
                battery_policy_evaluate(&bounded[i], i, &config, &copy, now, &d, &events);
                printf("%u %u %u %u %" PRIu32 " %u %u %u %u %u %u %u\n", index, i,
                    copy.estimate_valid, copy.estimated_percent, d.filtered_decimillivolts,
                    d.filter_valid, d.candidate, d.mode, d.decision,
                    events.restored, events.history_reset, events.settled);
            }
        } else for (unsigned i=0; i<4; ++i) evaluate(&experiments[i], i, power, now, index);
    }
    return fields == EOF ? 0 : 2;
}
