#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../apps/product/src/product_battery_trace.c"

static unsigned locked;
static uint32_t ticks;
static bool restart_during_export;
static char output[24000];
static size_t used;
rt_base_t rt_hw_interrupt_disable(void) { unsigned old=locked; ++locked; return old; }
void rt_hw_interrupt_enable(rt_base_t old) { assert(locked==(unsigned)old+1U); locked=(unsigned)old; }
rt_tick_t rt_tick_get(void) { return ticks; }
void rt_kprintf(const char *format, ...)
{
    assert(!locked);
    char line[512]; va_list args; va_start(args,format);
    int size=vsnprintf(line,sizeof line,format,args); va_end(args);
    assert(size>=0 && size<128); /* Actual SDK console buffer, including maxima. */
    assert(used+(size_t)size<sizeof output);
    memcpy(output+used,line,(size_t)size+1U); used+=(size_t)size;
    if (restart_during_export && strstr(line,"row=")) {
        restart_during_export=false;
        rt_base_t level=rt_hw_interrupt_disable();
        assert(wristflow_battery_trace_start(&trace,ticks,10));
        rt_hw_interrupt_enable(level);
    }
}
static int command(int argc, const char **argv)
{ return wf_battery(argc,(char **)argv); }
static wristflow_battery_record_t record(uint32_t now, unsigned sequence)
{
    wristflow_battery_record_t r={.observed_ms=now,.adc_valid=true,
        .power={.voltage_sequence=sequence,.voltage_sampled_ms=now,.millivolts=4040,
            .voltage_valid=true,.input_valid=true,.charge=WF_POWER_CHARGE_IDLE}};
    wristflow_battery_estimator_t state={0};
    wristflow_battery_estimate_diagnosed(&state,&r.power,now,&r.diagnostic);
    return r;
}
static void observe(wristflow_battery_record_t *r)
{
    ticks=r->observed_ms;
    wristflow_product_battery_trace_observe(r);
}
int main(void)
{
    const char *start[]={"wf_battery","start","900"};
    const char *status[]={"wf_battery","status"};
    const char *stop[]={"wf_battery","stop"};
    const char *dump[]={"wf_battery","dump","1","0","2"};
    wristflow_battery_record_t r=record(100,1), read;
    wristflow_product_battery_trace_observe(&r);
    assert(trace.status.state==WF_BATTERY_TRACE_IDLE && !trace.status.count);
    const char *invalid[]={"wf_battery","start","42949672960"};
    assert(command(3,invalid)<0 && !trace.status.session);
    invalid[2]="-1"; assert(command(3,invalid)<0);
    invalid[2]="901"; assert(command(3,invalid)<0);
    invalid[2]="0"; assert(command(3,invalid)<0);
    invalid[2]="1x"; assert(command(3,invalid)<0);
    ticks=101; assert(!command(3,start));
    assert(command(3,start)<0 && trace.status.session==1);
    wristflow_product_battery_trace_observe(&r); /* Prepared before command. */
    assert(trace.status.state==WF_BATTERY_TRACE_RUNNING && !trace.status.count);
    r.observed_ms=102;
    observe(&r);
    assert(trace.status.count==1);
    r.observed_ms=200; r.diagnostic.evaluated_ms=200; r.diagnostic.decision=WF_BATTERY_CACHED;
    observe(&r);
    assert(trace.status.count==1); /* Cached polling does not exhaust capacity. */
    r.power.input_good=true;
    observe(&r);
    assert(trace.status.count==2); /* Same ADC, PG completion is meaningful. */
    assert(command(5,dump)<0); /* Export cannot race an active producer. */
    ticks=201; assert(!command(2,stop));
    assert(!command(5,dump));
    assert(strstr(output,"row=1/0") && strstr(output,"row=1/1") && strstr(output,"page=1/0/2 complete"));
    assert(wristflow_battery_trace_read(&trace,1,0,&read) && read.power.estimated_percent==91);
    assert(!wristflow_battery_trace_read(&trace,2,0,&read));
    assert(!wristflow_battery_trace_read(&trace,1,2,&read));
    restart_during_export=true; used=0; output[0]=0;
    assert(command(5,dump)<0 && trace.status.session==2 && !trace.status.count);
    assert(strstr(output,"aborted; session changed") && !strstr(output,"page="));

    /* UI events with an old evaluation timestamp remain separately dated. */
    r=record(202,10); observe(&r);
    r.observed_ms=203; r.context.surface=20; r.context.requested_brightness=100;
    r.context.flags=WF_BATTERY_UI_VALID|WF_BATTERY_LCD_ON;
    observe(&r);
    assert(trace.status.count==2 && trace.records[1].diagnostic.evaluated_ms==202);
    for (unsigned i=2;i<WF_BATTERY_TRACE_CAPACITY;++i) {
        r=record(203+i,10+i); observe(&r);
    }
    assert(trace.status.count==WF_BATTERY_TRACE_CAPACITY && trace.status.state==WF_BATTERY_TRACE_FULL);
    r=record(1000,999); observe(&r);
    assert(trace.records[WF_BATTERY_TRACE_CAPACITY-1].power.voltage_sequence!=999);

    /* Expiry across wrap and long sleep requires no timer/background work. */
    ticks=UINT32_MAX-500U;
    assert(wristflow_battery_trace_start(&trace,ticks,1));
    r=record(498,1); observe(&r);
    assert(trace.status.state==WF_BATTERY_TRACE_RUNNING && trace.status.count==1);
    r=record(499,2); observe(&r);
    assert(trace.status.state==WF_BATTERY_TRACE_EXPIRED && trace.status.count==1);
    ticks=10000; assert(!command(3,start));
    ticks=910000; assert(!command(2,status));
    assert(trace.status.state==WF_BATTERY_TRACE_EXPIRED && !trace.status.count);
    ticks=910001; assert(!command(3,start));
    r=record(ticks+0x80000000U,1); observe(&r);
    assert(trace.status.state==WF_BATTERY_TRACE_EXPIRED && !trace.status.count);
    trace.status.session=UINT32_MAX;
    ticks=910001; assert(!command(3,start) && trace.status.session==1);
    assert(!command(2,stop));

    /* Every numeric field at maximum still produces complete console lines. */
    r=record(UINT32_MAX,UINT32_MAX);
    r.power.voltage_sampled_ms=0; r.power.millivolts=UINT16_MAX;
    r.power.charge_cycle=UINT32_MAX; r.power.charge=WF_POWER_CHARGE_FAULT;
    r.diagnostic.filtered_decimillivolts=UINT32_MAX;
    r.context=(wristflow_battery_context_t){.surface=UINT16_MAX,.phase=255,.flags=255,
        .requested_brightness=255,.acknowledged_brightness=255};
    used=0; output[0]=0; report_record(UINT32_MAX,WF_BATTERY_TRACE_CAPACITY-1,&r);
    assert(!locked);
    return 0;
}
