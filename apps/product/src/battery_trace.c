#include "battery_trace.h"

void wristflow_battery_trace_expire(wristflow_battery_trace_t *trace, uint32_t now)
{
    if (trace->status.state == WF_BATTERY_TRACE_RUNNING &&
        now-trace->status.started_ms >= trace->status.duration_ms)
        trace->status.state = WF_BATTERY_TRACE_EXPIRED;
}
bool wristflow_battery_trace_start(wristflow_battery_trace_t *trace, uint32_t now, unsigned seconds)
{
    if (!seconds || seconds > WF_BATTERY_TRACE_MAX_SECONDS) return false;
    wristflow_battery_trace_expire(trace, now);
    if (trace->status.state == WF_BATTERY_TRACE_RUNNING) return false;
    uint32_t session = trace->status.session+1U;
    if (!session) session = 1;
    /* Metadata only: never clear the whole buffer inside the caller's lock. */
    trace->status = (wristflow_battery_trace_status_t){.session=session,
        .started_ms=now, .duration_ms=seconds*1000U, .state=WF_BATTERY_TRACE_RUNNING};
    return true;
}
void wristflow_battery_trace_stop(wristflow_battery_trace_t *trace, uint32_t now)
{
    wristflow_battery_trace_expire(trace, now);
    if (trace->status.state == WF_BATTERY_TRACE_RUNNING)
        trace->status.state = WF_BATTERY_TRACE_STOPPED;
}
static bool same(const wristflow_battery_record_t *a, const wristflow_battery_record_t *b)
{
    const wristflow_power_snapshot_t *p=&a->power, *q=&b->power;
    const wristflow_battery_diagnostic_t *d=&a->diagnostic, *e=&b->diagnostic;
    const wristflow_battery_context_t *c=&a->context, *f=&b->context;
    /* Completion identity, not observation age, distinguishes ADC samples.
     * NEW/SEED -> CACHED alone is not another data point. Invalid decisions
     * remain significant, as do PG completion on an existing ADC identity. */
    bool decision_same = d->decision == e->decision ||
        (d->filter_valid && e->filter_valid && e->decision == WF_BATTERY_CACHED);
    return a->adc_valid==b->adc_valid && p->voltage_sequence==q->voltage_sequence && p->voltage_sampled_ms==q->voltage_sampled_ms &&
        p->voltage_valid==q->voltage_valid && p->millivolts==q->millivolts &&
        p->input_valid==q->input_valid && p->input_good==q->input_good &&
        p->charge==q->charge && p->charge_reason==q->charge_reason && p->charge_cycle==q->charge_cycle &&
        p->estimate_valid==q->estimate_valid && p->estimated_percent==q->estimated_percent &&
        decision_same && d->filter_valid==e->filter_valid && d->mode==e->mode &&
        d->candidate==e->candidate && d->filtered_decimillivolts==e->filtered_decimillivolts &&
        c->surface==f->surface && c->phase==f->phase && c->flags==f->flags &&
        c->requested_brightness==f->requested_brightness && c->acknowledged_brightness==f->acknowledged_brightness;
}
void wristflow_battery_trace_observe(wristflow_battery_trace_t *trace, const wristflow_battery_record_t *record,
    uint32_t now_at_lock)
{
    wristflow_battery_trace_expire(trace, now_at_lock);
    /* A command can interrupt a UI copy prepared just before a new session. */
    if (trace->status.state == WF_BATTERY_TRACE_RUNNING &&
        (int32_t)(record->observed_ms-trace->status.started_ms)<0) return;
    if (trace->status.state != WF_BATTERY_TRACE_RUNNING) return;
    unsigned count=trace->status.count;
    if (count && same(&trace->records[count-1], record)) return;
    trace->records[count] = *record;
    trace->status.count = (uint16_t)(count+1U);
    if (trace->status.count == WF_BATTERY_TRACE_CAPACITY) trace->status.state=WF_BATTERY_TRACE_FULL;
}
bool wristflow_battery_trace_read(const wristflow_battery_trace_t *trace, uint32_t session,
    unsigned index, wristflow_battery_record_t *record)
{
    if (!session || trace->status.session != session || trace->status.state == WF_BATTERY_TRACE_RUNNING ||
        index >= trace->status.count) return false;
    *record = trace->records[index];
    return true;
}
