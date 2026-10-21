#include "product_battery_trace.h"
#include <rtthread.h>
#include <rthw.h>
#include <string.h>
#include <limits.h>
#ifdef RT_USING_FINSH
#include <finsh.h>
#endif

static wristflow_battery_trace_t trace;
static uint32_t now_ms(void)
{ return (uint32_t)((uint64_t)rt_tick_get()*1000U/RT_TICK_PER_SECOND); }

void wristflow_product_battery_trace_observe(const wristflow_battery_record_t *record)
{
    rt_base_t level=rt_hw_interrupt_disable();
    wristflow_battery_trace_observe(&trace, record, now_ms());
    rt_hw_interrupt_enable(level);
}

/* Decimal-only, overflow-checked command arguments; no signs or suffixes. */
static bool number(const char *text, unsigned *value)
{
    unsigned n=0;
    if (!*text) return false;
    for (; *text; ++text) {
        if (*text<'0' || *text>'9' || n>(UINT_MAX-(unsigned)(*text-'0'))/10U) return false;
        n=n*10U+(unsigned)(*text-'0');
    }
    *value=n;
    return true;
}
static void report_status(wristflow_battery_trace_status_t s)
{
    rt_kprintf("[bat-trace] schema=1 session=%lu state=%u count=%u cap=%u\n",
        (unsigned long)s.session, s.state, s.count, WF_BATTERY_TRACE_CAPACITY);
    rt_kprintf("[bat-trace] start=%lu duration=%lu now=%lu; RAM only, no off-screen samples\n",
        (unsigned long)s.started_ms, (unsigned long)s.duration_ms, (unsigned long)now_ms());
}
static void report_record(uint32_t session, unsigned index, const wristflow_battery_record_t *r)
{
    const wristflow_power_snapshot_t *p=&r->power;
    const wristflow_battery_diagnostic_t *d=&r->diagnostic;
    const wristflow_battery_context_t *c=&r->context;
    /* Each labelled line fits the SDK's 128-byte console buffer at maxima. */
    rt_kprintf("[bat-trace] row=%lu/%u obs=%lu eval=%lu seq=%lu\n", (unsigned long)session, index,
        (unsigned long)r->observed_ms, (unsigned long)d->evaluated_ms, (unsigned long)p->voltage_sequence);
    rt_kprintf("[bat-trace] adc=%lu age=%lu mv=%u raw_valid=%u display_valid=%u\n",
        (unsigned long)p->voltage_sampled_ms, (unsigned long)(d->evaluated_ms-p->voltage_sampled_ms),
        p->millivolts, r->adc_valid, p->voltage_valid);
    rt_kprintf("[bat-trace] usb=%u/%u\n",p->input_valid,p->input_good);
    rt_kprintf("[bat-trace] charge=%u reason=%u cycle=%lu mode=%u decision=%u\n",
        p->charge, p->charge_reason, (unsigned long)p->charge_cycle, d->mode, d->decision);
    rt_kprintf("[bat-trace] filter=%lu valid=%u candidate=%u percent=%u valid=%u\n",
        (unsigned long)d->filtered_decimillivolts, d->filter_valid, d->candidate, p->estimated_percent, p->estimate_valid);
    rt_kprintf("[bat-trace] surface=%u phase=%u flags=%u requested=%u ack=%u\n",
        c->surface, c->phase, c->flags, c->requested_brightness, c->acknowledged_brightness);
    rt_kprintf("[bat-trace] end=%lu/%u\n", (unsigned long)session, index);
}
static int wf_battery(int argc, char **argv)
{
    unsigned seconds=0, session=0, offset=0, count=0;
    bool start=argc==3 && !strcmp(argv[1],"start") && number(argv[2],&seconds) &&
        seconds && seconds<=WF_BATTERY_TRACE_MAX_SECONDS;
    bool stop=argc==2 && !strcmp(argv[1],"stop");
    bool status=argc==2 && !strcmp(argv[1],"status");
    bool dump=argc==5 && !strcmp(argv[1],"dump") && number(argv[2],&session) &&
        number(argv[3],&offset) && number(argv[4],&count) && session && count && count<=16U;
    if (!start && !stop && !status && !dump) {
        rt_kprintf("wf_battery start <1..900> | stop | status | dump <session> <offset> <1..16>\n");
        return -1;
    }
    rt_base_t level=rt_hw_interrupt_disable();
    uint32_t now=now_ms();
    wristflow_battery_trace_expire(&trace,now);
    bool ok=!start || wristflow_battery_trace_start(&trace,now,seconds);
    if (stop) wristflow_battery_trace_stop(&trace,now);
    wristflow_battery_trace_status_t s=trace.status;
    rt_hw_interrupt_enable(level);
    report_status(s);
    if (!ok) { rt_kprintf("[bat-trace] busy; stop before starting another session\n"); return -1; }
    if (!dump) return 0;
    if (s.state==WF_BATTERY_TRACE_RUNNING || session!=s.session || offset>=s.count || count>s.count-offset) {
        rt_kprintf("[bat-trace] rejected; require stopped same session and complete valid range\n");
        return -1;
    }
    for (unsigned i=offset; i<offset+count; ++i) {
        wristflow_battery_record_t record;
        level=rt_hw_interrupt_disable();
        ok=wristflow_battery_trace_read(&trace,session,i,&record);
        rt_hw_interrupt_enable(level);
        if (!ok) { rt_kprintf("[bat-trace] aborted; session changed during export\n"); return -1; }
        report_record(session,i,&record);
    }
    rt_kprintf("[bat-trace] page=%lu/%u/%u complete\n", (unsigned long)session,offset,count);
    return 0;
}
#ifdef RT_USING_FINSH
MSH_CMD_EXPORT(wf_battery, Record bounded battery estimate diagnostics without device IO);
#endif
