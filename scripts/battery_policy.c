/* This wraps production C without changing its reference curves or decisions. */
#include "battery_policy.h"

void battery_policy_evaluate(battery_policy_t *e, unsigned policy,
    const battery_policy_config_t *c, wristflow_power_snapshot_t *p,
    uint32_t now, wristflow_battery_diagnostic_t *d, battery_policy_events_t *events)
{
    *events = (battery_policy_events_t){0};
    if (!policy) { wristflow_battery_estimate_diagnosed(&e->state, p, now, d); return; }
    /* Resetting experiment history must not bypass production's same-ADC
     * mode-change wait. Its independent state controls validity only. */
    wristflow_power_snapshot_t gate = *p;
    wristflow_battery_diagnostic_t gate_d;
    wristflow_battery_estimate_diagnosed(&e->validity_gate, &gate, now, &gate_d);
    bool fresh = p->voltage_valid && p->voltage_sequence &&
        now-p->voltage_sampled_ms <= WF_BATTERY_SAMPLE_MAX_AGE_MS &&
        p->millivolts >= 3000U && p->millivolts <= 4250U;
    bool discharge = p->input_valid && !p->input_good &&
        (p->charge == WF_POWER_CHARGE_IDLE || p->charge == WF_POWER_CHARGE_STOPPED);
    /* Zeroed PG during a missing-sequence gap is not continuity evidence.
     * Keep only a tentative discharge copy; a fresh trusted return is required.
     * The deadline is anchored to the LAST ACTUAL ADC, never to gap callbacks. */
    bool tentative_gap = !p->voltage_sequence && !p->input_good &&
        (p->charge == WF_POWER_CHARGE_IDLE || p->charge == WF_POWER_CHARGE_STOPPED);
    /* A convergence-support timeout cancels the dwell only. It must not
     * reseed a still-fresh cached ADC and thereby bypass that dwell. */
    uint32_t history_limit = (policy & 1U) ? c->retain_ms : WF_BATTERY_SAMPLE_MAX_AGE_MS;
    bool within = e->have_adc && now-e->last_adc_ms <= history_limit;
    bool ordinary_mode = p->charge != WF_POWER_CHARGE_ACTIVE && p->charge != WF_POWER_CHARGE_FULL;
    bool unknown = ordinary_mode && (!p->input_valid || p->charge == WF_POWER_CHARGE_UNKNOWN ||
                                    p->charge == WF_POWER_CHARGE_FAULT);
    if ((policy & 1U) && tentative_gap && within && e->state.seeded && e->state.mode == 0U) {
        e->saved = e->state;
        e->suspended = true;
    }
    if (e->suspended && fresh && discharge && within && p->voltage_sequence != e->saved.sequence) {
        e->state = e->saved;
        e->suspended = false;
        events->restored = true;
    } else if (e->suspended && (!tentative_gap || !within)) {
        e->suspended = false;
    }
    if ((!within && e->state.seeded) || (unknown && !tentative_gap) ||
        (!fresh && !tentative_gap)) {
        events->history_reset = e->state.seeded;
        e->state = (wristflow_battery_estimator_t){0};
        e->suspended = false;
    }
    if (!gate.estimate_valid) {
        e->state = (wristflow_battery_estimator_t){0};
        e->pending = false;
        p->estimate_valid = false;
        p->estimated_percent = 0;
        p->voltage_valid = gate.voltage_valid;
        *d = gate_d;
        return;
    }
    wristflow_battery_estimate_diagnosed(&e->state, p, now, d);
    bool new_adc = d->decision == WF_BATTERY_NEW_SAMPLE || d->decision == WF_BATTERY_SEED ||
        d->decision == WF_BATTERY_MODE_SEED;
    if (p->estimate_valid && new_adc) {
        e->last_adc_ms = p->voltage_sampled_ms;
        e->have_adc = true;
    }
    int difference = (int)d->candidate-(int)e->state.percent;
    bool eligible = (policy & 2U) && discharge && p->estimate_valid &&
        (difference == 1 || difference == -1);
    if (!eligible || d->decision == WF_BATTERY_SEED || d->decision == WF_BATTERY_MODE_SEED ||
        (e->pending && now-e->pending_last_ms > c->max_sample_gap_ms)) e->pending = false;
    if (eligible && d->decision == WF_BATTERY_NEW_SAMPLE) {
        if (!e->pending || e->pending_percent != d->candidate) {
            e->pending = true;
            e->pending_percent = d->candidate;
            e->pending_first_ms = p->voltage_sampled_ms;
            e->pending_count = 0;
        }
        e->pending_last_ms = p->voltage_sampled_ms;
        e->pending_count++;
        if (e->pending_count >= c->settle_samples &&
            p->voltage_sampled_ms-e->pending_first_ms >= c->settle_ms) {
            e->state.percent = d->candidate;
            p->estimated_percent = d->candidate;
            e->pending = false;
            events->settled = true;
        }
    }
}
