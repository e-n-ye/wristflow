#ifdef NDEBUG
#undef NDEBUG
#endif
#define RT_USING_PM
#include "../apps/product/src/product_imu_diag.c"
#include <assert.h>
#include <setjmp.h>

static uint8_t user_regs[256], bank_regs[256], bank_b_regs[256], bank;
static uint16_t native_count;
static rt_tick_t ticks;
static unsigned irq, idle, reads, writes, rounds, deleted, detached;
static int failing_reg = -1;
static bool fail_event, fail_create, fail_start;
static bool fail_bus_find, fail_bus_config;
static bool fail_pin_device;
static unsigned pin_modes, pin_reads;
static wristflow_wrist_irq_snapshot_t irq_adapter;
static void (*irq_signal)(void);
static bool fail_irq_start, fail_irq_stop;
static unsigned ui_events;
void wristflow_product_event_send(rt_uint32_t events)
{ assert(events == WF_EVENT_WRIST && !(user_regs[0x54]&1)); ui_events |= events; }
bool wristflow_product_wrist_irq_start(void (*signal)(void))
{
    assert(!irq && idle && !irq_adapter.owned);
    if (fail_irq_start) return false;
    irq_adapter.armed=irq_adapter.owned=true; irq_signal=signal; ++pin_modes; return true;
}
bool wristflow_product_wrist_irq_stop(void)
{
    irq_adapter.armed=false; irq_signal=NULL;
    if (fail_irq_stop) return false;
    irq_adapter.owned=false; return true;
}
wristflow_wrist_irq_snapshot_t wristflow_product_wrist_irq_snapshot(void) { return irq_adapter; }
static unsigned bus_find_fail_after, bus_config_fail_after;
static bool stop_in_prepare;
static bool stop_daily_in_prepare, retry_daily_in_prepare;
static bool stop_wrist_in_delay;
static bool disable_wrist_in_delay;
static void wf_wrist(int, char **);
static unsigned max_wait=1000, last_wait;
static wristflow_alarm_clock_t service_clock;
static struct rt_thread thread;
static struct rt_i2c_bus_device fake_bus;
static uint8_t step_records[2][WRISTFLOW_STEPS_RECORD_BYTES];
static size_t step_lengths[2];
static unsigned store_reads, store_writes;
static bool storage_available = true, fail_storage;
static jmp_buf yield;
rt_base_t rt_hw_interrupt_disable(void) { return irq++; }
void rt_hw_interrupt_enable(rt_base_t previous) { irq = (unsigned)previous; }
rt_tick_t rt_tick_get(void) { return ticks; }
rt_tick_t rt_tick_from_millisecond(unsigned ms) { return ms; }
int rt_thread_mdelay(rt_int32_t ms)
{
    ticks += ms;
    if (ms == 50 && stop_wrist_in_delay) {
        stop_wrist_in_delay = false;
        char *args[] = {"wf_wrist", "stop"}; wf_wrist(2, args);
    }
    if (ms == 50 && disable_wrist_in_delay) {
        disable_wrist_in_delay=false;
        wristflow_product_wrist_enable(false);
    }
    if (stop_in_prepare) {
        stop_in_prepare = false;
        char *args[] = {"wf_imu", "stop"}; wf_imu(2, args);
    }
    if (stop_daily_in_prepare) {
        stop_daily_in_prepare=false;
        char *args[]={"wf_steps","stop"}; wf_steps(2,args);
    }
    if (retry_daily_in_prepare) {
        retry_daily_in_prepare=false;
        char *args[]={"wf_steps","retry"}; wf_steps(2,args);
    }
    return 0;
}
void rt_kprintf(const char *format, ...) { (void)format; }
void rt_pm_request(unsigned mode) { assert(mode == PM_SLEEP_MODE_IDLE && !irq && !idle); ++idle; }
void rt_pm_release(unsigned mode) { assert(mode == PM_SLEEP_MODE_IDLE && !irq && idle == 1); --idle; }
int rt_event_init(struct rt_event *e, const char *name, unsigned flags)
{ (void)name; (void)flags; e->bits = 0; return fail_event ? -1 : 0; }
int rt_event_detach(struct rt_event *e) { (void)e; ++detached; return 0; }
int rt_event_send(struct rt_event *e, unsigned bits) { e->bits |= bits; return 0; }
int rt_event_recv(struct rt_event *e, unsigned mask, unsigned flags, int wait, rt_uint32_t *bits)
{
    (void)flags;
    if (!rounds--) longjmp(yield, 1);
    *bits = e->bits & mask; e->bits &= ~mask;
    if (!*bits) {
        if (wait == RT_WAITING_FOREVER) longjmp(yield, 1);
        assert(wait > 0 && (unsigned)wait <= max_wait); ticks += (unsigned)wait;
    }
    last_wait=(unsigned)wait;
    return *bits ? 0 : -1;
}
rt_thread_t rt_thread_create(const char *name, void (*entry)(void *), void *context,
    unsigned stack, unsigned priority, unsigned slice)
{
    assert(!strcmp(name, "wf_imu") && stack == 6144 && priority == 24 && slice == 10);
    thread = (struct rt_thread){entry, context}; return fail_create ? NULL : &thread;
}
int rt_thread_startup(rt_thread_t t) { assert(t == &thread); return fail_start ? -1 : 0; }
int rt_thread_delete(rt_thread_t t) { assert(t == &thread); ++deleted; return 0; }
void HAL_PIN_Set(unsigned pin, unsigned function, unsigned pull, unsigned cpu)
{
    assert(!irq && idle && cpu == 1);
    if (pin==31) assert(function==GPIO_A31 && pull==PIN_NOPULL);
    else assert((pin==39 || pin==40) && pull==1);
}
void *rt_device_find(const char *name)
{ assert(!strcmp(name,"pin") && idle && !irq); return fail_pin_device ? NULL : &fake_bus; }
void rt_pin_mode(int pin, int mode)
{ assert(pin==31 && mode==PIN_MODE_INPUT && idle && !irq); ++pin_modes; }
int rt_pin_read(int pin)
{ assert(pin==31 && pin_modes && idle && !irq); ++pin_reads; return !!((user_regs[0x13]&0x20) && (user_regs[0x54]&1)); }
void BSP_GPIO_Set(unsigned pin, unsigned value, unsigned cpu)
{ assert(pin == 30 && value == 1 && cpu == 1 && idle && !irq); }
struct rt_i2c_bus_device *rt_i2c_bus_device_find(const char *name)
{
    assert(!strcmp(name, "i2c3") && idle && !irq);
    return fail_bus_find || (bus_find_fail_after && !--bus_find_fail_after) ? NULL : &fake_bus;
}
int rt_i2c_configure(struct rt_i2c_bus_device *b, struct rt_i2c_configuration *cfg)
{
    assert(b == &fake_bus && cfg->timeout == 50 && cfg->max_hz == 100000);
    return fail_bus_config || (bus_config_fail_after && !--bus_config_fail_after) ? -1 : 0;
}
size_t rt_i2c_mem_read(struct rt_i2c_bus_device *b, unsigned addr, unsigned reg,
    unsigned bits, void *out, size_t length)
{
    assert(b == &fake_bus && bits == 8 && (length == 1 || (reg==0x28 && length==6)) && !irq && idle); ++reads;
    if (addr != 0x6a) return 0;
    if (length==6) { assert(!bank); memcpy(out,user_regs+reg,length); return length; }
    uint8_t value = bank == 0xa0 ? bank_b_regs[reg] : bank ? bank_regs[reg] : user_regs[reg];
    if (reg == 1) value = bank;
    if (!bank && reg == 0x4c) value = (uint8_t)(native_count >> 8);
    if (!bank && reg == 0x4b) value = (uint8_t)native_count;
    *(uint8_t *)out = value;
    if (!bank && reg == 0x54) user_regs[0x54] = user_regs[0x55] = 0;
    return 1;
}
size_t rt_i2c_mem_write(struct rt_i2c_bus_device *b, unsigned addr, unsigned reg,
    unsigned bits, void *input, size_t length)
{
    assert(b == &fake_bus && addr == 0x6a && bits == 8 && length == 1 && !irq && idle); ++writes;
    if ((int)reg == failing_reg) return 0;
    uint8_t value = *(uint8_t *)input;
    if (reg == 1) bank = value;
    else if (bank == 0xa0) bank_b_regs[reg] = value;
    else if (bank) bank_regs[reg] = value;
    else { user_regs[reg] = value; if (reg == 0x19 && (value & 2)) native_count = 0; }
    return 1;
}
static void cycle(void)
{
    rounds = 1;
    if (!setjmp(yield)) thread.entry(thread.context);
    assert(!irq);
}
static void cmd(const char *action, const char *argument)
{
    char *args[] = {"wf_imu", (char *)action, (char *)argument};
    wf_imu(argument ? 3 : 2, args);
}
wristflow_alarm_clock_t wristflow_product_services_time(void)
{
    wristflow_alarm_clock_t value=service_clock;
    value.monotonic_ms=ticks;
    value.utc += ticks/1000;
    return value;
}
static void daily_cmd(const char *action)
{ char *args[]={"wf_steps",(char *)action}; wf_steps(2,args); }
static void wrist_cmd(const char *action, const char *span, const char *axis)
{
    char *args[] = {"wf_wrist", (char *)action, (char *)span, (char *)axis};
    wf_wrist(span ? 4 : 2, args);
}
bool wristflow_product_services_storage_ready(void) { return storage_available; }
size_t wristflow_product_services_steps_read(unsigned key, uint8_t *record, size_t capacity, void *context)
{
    (void)context; assert(key<2 && idle && !irq); ++store_reads;
    if(fail_storage || step_lengths[key]>capacity) return 0;
    memcpy(record,step_records[key],step_lengths[key]); return step_lengths[key];
}
bool wristflow_product_services_steps_write(unsigned key, const uint8_t *record, void *context)
{
    (void)context; assert(key<2 && idle && !irq); ++store_writes;
    memcpy(step_records[key],record,WRISTFLOW_STEPS_RECORD_BYTES);
    step_lengths[key]=WRISTFLOW_STEPS_RECORD_BYTES; return !fail_storage;
}
int main(void)
{
    fail_event = true; assert(!wristflow_product_imu_diag_start()); fail_event = false;
    fail_create = true; assert(!wristflow_product_imu_diag_start()); fail_create = false;
    fail_start = true; assert(!wristflow_product_imu_diag_start()); fail_start = false;
    assert(detached == 2 && deleted == 1);
    assert(wristflow_product_imu_diag_start()); cycle();
    assert(!reads && !writes && !idle && cached.phase == WF_IMU_UNPROBED);
    cmd("steps", "2"); assert(pending == CMD_NONE);
    cmd("probe", NULL); cmd("stop", NULL); cmd("probe", NULL);
    assert(pending == CMD_NONE); cycle(); assert(!reads && !writes && !idle);
    fail_bus_find = true; cmd("probe", NULL); cycle();
    assert(cached.phase == WF_IMU_UNPROBED && !reads && !writes && !idle);
    fail_bus_find = false; fail_bus_config = true; cmd("probe", NULL); cycle();
    assert(cached.phase == WF_IMU_UNPROBED && !reads && !writes && !idle);
    fail_bus_config = false;
    user_regs[0x0f] = 0x6a;
    cmd("probe", NULL); cycle();
    assert(cached.phase == WF_IMU_READY && !writes && !idle);
    stop_in_prepare = true;
    cmd("steps", "2"); cycle();
    assert(cached.phase != WF_IMU_RUNNING && user_regs[0x10] == 0 && user_regs[0x19] == 0);
    cycle(); assert(!idle); /* Drain the cancellation without enabling hardware. */
    cmd("steps", "121"); assert(pending == CMD_NONE);
    cmd("steps", "2"); cycle();
    assert(cached.phase == WF_IMU_RUNNING && idle == 1);
    uint32_t started = cached.started_ms;
    unsigned before = reads + writes;
    cmd("status", NULL); cmd("steps", "120");
    assert(before == reads + writes && pending == CMD_NONE && cached.started_ms == started);
    native_count = 12; cycle(); assert(cached.steps == 12 && cached.phase == WF_IMU_RUNNING);
    cycle(); assert(cached.phase == WF_IMU_DONE && cached.stop_verified && !idle);
    before = reads + writes; cycle(); assert(before == reads + writes); /* No idle polling. */
    cmd("steps", "2"); cycle();
    failing_reg = 0x19; cmd("stop", NULL); cycle();
    assert(cached.phase == WF_IMU_FAULT && !cached.stop_verified && idle == 1);
    cmd("probe", NULL); cmd("steps", "2"); assert(pending == CMD_NONE);
    failing_reg = -1; cmd("stop", NULL); cycle();
    assert(cached.phase == WF_IMU_READY && cached.stop_verified && !idle);
    cmd("stop", NULL); cycle(); assert(!idle); /* Repeated stop also protects I/O. */
    /* Production daily mode reuses this owner and never holds IDLE between
     * observations. Diagnostic reset/stop commands cannot alter its state. */
    ready=false; cancel_requested=false; pending=CMD_NONE; native_count=0; ticks=0;
    service_clock=(wristflow_alarm_clock_t){.utc=1792411200,.valid=true};
    assert(wristflow_product_steps_start()); cycle();
    assert(daily_cached.metric.total_valid && !daily_cached.metric.total && !idle);
    assert(device.continuous && device.snapshot.phase==WF_IMU_RUNNING);
    before=reads+writes; cmd("probe",NULL); cmd("steps","2"); cmd("stop",NULL); daily_cmd("status");
    assert(before==reads+writes && !cancel_requested && !work.bits);
    native_count=12; cycle(); assert(daily_cached.metric.total==12 && !idle);
    assert(daily_cached.metric.hourly[20]==12 && daily_cached.metric.hours_valid==(1U<<20));
    wristflow_product_steps_screen(false); cycle(); max_wait=60000;
    native_count=112; cycle(); assert(last_wait>0 && last_wait<=60000 && daily_cached.metric.total==112 && !idle);
    wristflow_product_steps_screen(true); cycle();
    native_count=114; cycle(); assert(last_wait==1000 && daily_cached.metric.total==114 && !idle);
    /* RTC failures and clock-change notification affect only daily validity. */
    service_clock.valid=false; wristflow_product_steps_clock_changed(); cycle();
    assert(!daily_cached.metric.total_valid && device.snapshot.phase==WF_IMU_RUNNING && !idle);
    assert(!daily_cached.metric.hours_valid);
    service_clock.valid=true; ++service_clock.revision;
    wristflow_product_steps_clock_changed(); cycle();
    assert(daily_cached.metric.total_valid && daily_cached.metric.total==0);
    service_clock.valid=false;
    assert(!wristflow_product_steps_snapshot().metric.total_valid);
    assert(!wristflow_product_steps_snapshot().metric.hours_valid);
    service_clock.valid=true; ++service_clock.revision;
    assert(!wristflow_product_steps_snapshot().metric.total_valid);
    wristflow_product_steps_clock_changed(); cycle();
    service_clock.utc+=86400;
    assert(!wristflow_product_steps_snapshot().metric.total_valid);
    wristflow_product_steps_clock_changed(); cycle();
    assert(wristflow_product_steps_snapshot().metric.total_valid);
    /* Lost native configuration is a fault, not a65536-step wrap. */
    user_regs[0x10]=0; cycle();
    assert(cached.phase==WF_IMU_FAULT && cached.stop_verified && !daily_cached.metric.total_valid && !idle);
    daily_cmd("retry"); retry_daily_in_prepare=true; cycle();
    assert(device.continuous && cached.phase==WF_IMU_RUNNING && !work.bits && !idle);
    /* Failed cleanup retains the resource responsibility until explicit retry. */
    failing_reg=0x19; daily_cmd("stop"); cycle();
    assert(cached.phase==WF_IMU_FAULT && !cached.stop_verified && idle==1);
    failing_reg=-1; daily_cmd("retry"); stop_daily_in_prepare=true; cycle();
    cycle(); assert(cached.phase!=WF_IMU_RUNNING && cached.stop_verified && !idle);
    daily_cmd("retry"); cycle(); assert(cached.phase==WF_IMU_RUNNING && !idle);
    ticks+=WRISTFLOW_STEPS_MAX_GAP_MS+1;
    assert(!wristflow_product_steps_snapshot().metric.total_valid);
    cycle(); assert(cached.phase==WF_IMU_FAULT && !daily_cached.metric.total_valid && !idle);
    /* A cold application start imports only the same-day daily checkpoint.
     * Native begin resets its counter; the saved113 must not be counted twice. */
    wristflow_steps_checkpoint_t seed={.local_day=20745,.total=113,.coverage_start_utc=1792411200,
        .saved_utc=1792411260,.hourly={[20]=80},.hours_valid=1U<<20};
    memset(step_records,0,sizeof step_records);
    assert(wristflow_steps_encode(&seed,7,step_records[0]));
    step_lengths[0]=WRISTFLOW_STEPS_RECORD_BYTES; step_lengths[1]=0;
    ready=false; cancel_requested=false; ticks=0; daily_lit=true;
    service_clock=(wristflow_alarm_clock_t){.utc=1792411300,.valid=true};
    assert(wristflow_product_steps_start()); cycle();
    assert(daily_cached.metric.total==113 && daily_cached.observation.restored && native_count==0 && !idle);
    assert(daily_cached.metric.hourly[20]==80 && daily_cached.metric.hours_valid==(1U<<20));
    before=store_reads+store_writes; daily_cmd("status");
    assert(before==store_reads+store_writes); /* Status performs no Flash I/O. */
    native_count=2; cycle(); assert(daily_cached.metric.total==115);
    assert(daily_cached.metric.hourly[20]==82);
    unsigned saved_writes=store_writes;
    ticks+=30000; wristflow_product_steps_screen(false); cycle();
    cycle(); /* Checkpoint deadline occurs before the next60-second sample. */
    assert(last_wait>28000 && last_wait<31000);
    assert(store_writes==saved_writes+1 && checkpoint_cached.saved_valid && checkpoint_cached.saved.total==115 && !idle);
    assert(checkpoint_cached.saved.hourly[20]==82 && checkpoint_cached.saved.hours_valid==(1U<<20));
    wristflow_product_steps_screen(true); cycle(); native_count=4; cycle();
    daily_cmd("stop"); cycle(); saved_writes=store_writes; cycle();
    assert(store_writes==saved_writes+1 && checkpoint_cached.saved.total==117 && !idle);
    daily_cmd("retry"); cycle();
    assert(daily_cached.metric.total==0 && !daily_cached.observation.restored);
    assert(!daily_cached.metric.hours_valid);
    daily_cmd("save"); cycle(); assert(checkpoint_cached.saved.total==0);
    native_count=2; cycle(); user_regs[0x10]=0; cycle();
    assert(cached.phase==WF_IMU_FAULT && !daily_cached.metric.total_valid && !idle);
    saved_writes=store_writes; cycle();
    assert(store_writes==saved_writes+1 && checkpoint_cached.saved.total==2 && !idle);
    /* The common RTS/unknown-RTC startup must wait before using its seed. */
    memset(step_records,0,sizeof step_records); assert(wristflow_steps_encode(&seed,9,step_records[0]));
    step_lengths[0]=WRISTFLOW_STEPS_RECORD_BYTES; step_lengths[1]=0;
    ready=false; cancel_requested=false; ticks=0; service_clock.valid=false;
    assert(wristflow_product_steps_start()); cycle(); native_count=5; cycle();
    assert(!daily_cached.metric.total_valid && daily.seed_pending);
    service_clock.valid=true; wristflow_product_steps_clock_changed(); cycle();
    assert(daily_cached.metric.total==113 && daily_cached.observation.restored && !daily.seed_pending);
    /* Storage unavailable leaves native counting operational and uncheckpointed. */
    ready=false; cancel_requested=false; storage_available=false;
    assert(wristflow_product_steps_start()); cycle(); native_count=2; cycle();
    assert(daily_cached.metric.total==2 && !checkpoint_enabled && !idle);
    /* A bounded AWT observation borrows this same device/worker. It preserves
     * the existing native count and daily/hourly values (storage is off here). */
    ready=false; cancel_requested=false; ticks=0; daily_lit=true;
    assert(wristflow_product_steps_start()); cycle(); native_count=5;
    wrist_cmd("start","2","xneg"); cycle();
    assert(wrist_cached.phase==WF_IMU_RUNNING && wrist_cached.configured && idle==1);
    assert(device.wrist_enabled && user_regs[0x19]==0x94 && bank_b_regs[0x59]==0x40);
    assert(native_count==5 && daily_cached.metric.total==5 && daily_cached.metric.hourly[20]==5);
    assert(wrist_cached.before.valid==WF_WRIST_ALL && wrist_cached.before.ctrl10==0x14);
    before=reads+writes; wrist_cmd("status",NULL,NULL); wrist_cmd("start","120","xpos");
    assert(before==reads+writes && !wrist_pending);
    native_count=6; user_regs[0x54]=1; user_regs[0x55]=0x40; cycle();
    assert(wrist_cached.observations==1 && wrist_cached.source==1 && wrist_cached.axes==0x40);
    assert(wrist_cached.running.valid==WF_WRIST_ALL && wrist_cached.running.xyz_valid && wrist_cached.running.ctrl10==0x94);
    assert(daily_cached.metric.total==6 && cached.phase==WF_IMU_RUNNING && idle==1);
    wrist_cmd("stop",NULL,NULL); cycle();
    assert(wrist_cached.stop_verified && !device.wrist_enabled && user_regs[0x19]==0x14 && !idle);
    assert(wrist_cached.after.valid==WF_WRIST_ALL && wrist_cached.after.ctrl10==0x14 && !wrist_cached.after.source);
    assert(native_count==6 && daily_cached.metric.total==6 && cached.phase==WF_IMU_RUNNING);
    wrist_cmd("start","1","xpos"); cycle(); ticks+=1000; cycle();
    assert(wrist_cached.phase==WF_IMU_DONE && wrist_cached.stop_verified && !idle);
    assert(cached.phase==WF_IMU_RUNNING && native_count==6);
    assert(!pin_modes && !pin_reads); /* Legacy start never configures PA31. */
    stop_wrist_in_delay=true; wrist_cmd("start","2","xneg"); cycle(); cycle();
    assert(wrist_cached.ended==WF_IMU_USER_STOP && wrist_cached.stop_verified && !idle);
    assert(cached.phase==WF_IMU_RUNNING && user_regs[0x19]==0x14 && native_count==6);
    failing_reg=0x59; wrist_cmd("start","2","xneg"); cycle();
    assert(wrist_cached.phase==WF_IMU_FAULT && wrist_cached.stop_verified && !idle);
    assert(cached.phase==WF_IMU_RUNNING && user_regs[0x19]==0x14 && native_count==6);
    failing_reg=-1; wrist_cmd("stop",NULL,NULL); cycle();
    wrist_cmd("start","2","xneg"); cycle(); daily_cmd("stop"); cycle();
    assert(wrist_cached.stop_verified && !wrist.changed && cached.stop_verified && !idle);
    assert(!user_regs[0x19] && !user_regs[0x10] && !daily_cached.metric.total_valid);
    daily_cmd("retry"); cycle();
    failing_reg=1; wrist_cmd("start","2","xneg"); cycle();
    assert(wrist_cached.phase==WF_IMU_FAULT && !wrist_cached.stop_verified && idle==1);
    assert(cached.phase==WF_IMU_FAULT && !cached.counter_valid && !daily_cached.metric.total_valid);
    failing_reg=-1; wrist_cmd("stop",NULL,NULL); cycle();
    assert(wrist_cached.stop_verified && cached.stop_verified && !idle && !user_regs[0x10]);
    daily_cmd("retry"); cycle(); assert(cached.phase==WF_IMU_RUNNING && !idle);
    /* Loss of bus preparation during an active wrist session faults both
     * borrowers and retains ownership until explicit cleanup succeeds. */
    for (unsigned failure=0; failure<2; ++failure) {
        wrist_cmd("start","2","xneg"); cycle();
        /* The daily read succeeds; fail the wrist preparation in this round. */
        if (failure) bus_config_fail_after=2; else bus_find_fail_after=2;
        cycle();
        assert(wrist_cached.phase==WF_IMU_FAULT && wrist_cached.ended==WF_IMU_IO_FAILURE);
        assert(!wrist_cached.configured && !wrist_cached.source_valid && !wrist_cached.stop_verified);
        assert(cached.phase==WF_IMU_FAULT && !cached.counter_valid && !daily_cached.metric.total_valid && idle==1);
        wrist_cmd("stop",NULL,NULL); cycle();
        assert(wrist_cached.stop_verified && cached.stop_verified && !idle && !user_regs[0x10]);
        daily_cmd("retry"); cycle(); assert(cached.phase==WF_IMU_RUNNING && !idle);
    }
    user_regs[0x13]=0x08;
    before=writes; fail_pin_device=true; wrist_cmd("route","2","xneg"); cycle();
    assert(writes==before && !pin_modes && !wrist.changed && !idle);
    fail_pin_device=false; wrist_cmd("route","2","xneg"); cycle();
    assert(wrist_cached.route_int1 && wrist_cached.configured && pin_modes==1 && idle==1);
    assert(user_regs[0x13]==0x28 && wrist_cached.before.ctrl4==0x08);
    native_count=9; user_regs[0x54]=0x85; cycle();
    assert(wrist_cached.event.source==0x85 && wrist_cached.event.pin_before==1 && !wrist_cached.event.pin_after);
    assert(wrist_cached.event.pin_valid==3 && daily_cached.metric.total==9);
    before=reads+writes+pin_reads; wrist_cmd("status",NULL,NULL);
    assert(reads+writes+pin_reads==before); /* All status fields are cached. */
    cycle(); assert(!wrist_cached.running.pin_before && wrist_cached.event.pin_before==1);
    failing_reg=0x13; wrist_cmd("stop",NULL,NULL); cycle();
    assert(wrist.changed && !wrist_cached.stop_verified && cached.phase==WF_IMU_FAULT && idle==1);
    failing_reg=-1; wrist_cmd("stop",NULL,NULL); cycle();
    assert(!wrist.changed && wrist_cached.stop_verified && cached.stop_verified && !idle && user_regs[0x13]==0x08);
    daily_cmd("retry"); cycle(); wrist_cmd("route","1","xneg"); cycle(); ticks+=1000; cycle();
    assert(wrist_cached.phase==WF_IMU_DONE && wrist_cached.stop_verified && !idle && user_regs[0x13]==0x08);
    wrist_cmd("route","2","xneg"); cycle(); daily_cmd("stop"); cycle();
    assert(wrist_cached.stop_verified && !wrist.changed && cached.stop_verified && !idle && user_regs[0x13]==0x08);
    /* Persistent native service has no 100ms poll/IDLE hold. IRQ only schedules
     * this worker; UI wake is emitted after source clear and consumed once. */
    daily_cmd("retry"); cycle(); native_count=11;
    wristflow_product_wrist_enable(true); cycle();
    assert(wrist_runtime && wrist_cached.continuous && irq_adapter.armed && !idle);
    assert(native_count==11 && daily_cached.metric.total==11);
    wrist_cmd("route","1","xpos"); assert(!wrist_pending);
    wristflow_product_steps_screen(false); cycle(); max_wait=60000;
    cycle(); assert(last_wait==60000 && !idle && wrist_cached.phase==WF_IMU_RUNNING);
    user_regs[0x54]=0x85; irq_signal(); irq_signal(); cycle();
    assert(ui_events==WF_EVENT_WRIST && wrist_wakes==1 && wrist_cached.observations==1 && !idle);
    assert(wristflow_product_wrist_take_wake() && !wristflow_product_wrist_take_wake());
    cycle(); assert(wrist_wakes==1); /* A coalesced callback cannot replay a cleared source. */
    user_regs[0x54]=0x85; irq_signal(); cycle();
    wristflow_product_wrist_enable(false); assert(!wristflow_product_wrist_take_wake());
    cycle(); assert(!wrist_runtime && !irq_adapter.owned && !idle && user_regs[0x19]==0x14);
    rt_event_send(&work,EV_WRIST_IRQ); cycle(); assert(wrist_wakes==2 && !wristflow_product_wrist_take_wake());
    wristflow_product_wrist_enable(true); cycle();
    fail_irq_stop=true; wristflow_product_wrist_enable(false); cycle();
    assert(irq_adapter.owned && !irq_adapter.armed && idle && wrist_cached.phase==WF_IMU_FAULT);
    fail_irq_stop=false; wrist_cmd("stop",NULL,NULL); cycle(); assert(!irq_adapter.owned && !idle);
    fail_irq_start=true; wristflow_product_wrist_enable(true); cycle();
    assert(!wrist_runtime && wrist_cached.phase==WF_IMU_FAULT && !idle);
    fail_irq_start=false; wristflow_product_wrist_enable(false); cycle();
    wristflow_product_wrist_enable(true); cycle(); assert(wrist_runtime && !idle);
    /* Disable racing with the 50ms prepare cancels the configuration. */
    wristflow_product_wrist_enable(false); cycle();
    disable_wrist_in_delay=true; wristflow_product_wrist_enable(true); cycle(); cycle();
    assert(!wrist_runtime && !irq_adapter.owned && !wrist_requested && !idle);
    assert(native_count==11 && daily_cached.metric.total==11);
    wristflow_product_wrist_enable(true); cycle();
    user_regs[0x13]=0x08; irq_signal(); cycle();
    assert(wrist_cached.phase==WF_IMU_FAULT && wrist_cached.stop_verified && !wrist_runtime && !irq_adapter.owned && !idle);
    assert(cached.phase==WF_IMU_RUNNING && native_count==11);
    wristflow_product_wrist_enable(false); cycle();
    wristflow_product_wrist_enable(true); cycle();
    fail_bus_find=true; irq_signal(); cycle();
    assert(wrist_cached.phase==WF_IMU_FAULT && wrist.changed && !irq_adapter.owned && idle);
    assert(cached.phase==WF_IMU_FAULT && !daily_cached.metric.total_valid);
    fail_bus_find=false; wrist_cmd("stop",NULL,NULL); cycle();
    assert(wrist_cached.stop_verified && cached.stop_verified && !idle);
    return 0;
}
