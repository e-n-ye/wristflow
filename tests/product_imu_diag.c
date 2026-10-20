#ifdef NDEBUG
#undef NDEBUG
#endif
#define RT_USING_PM
#include "../apps/product/src/product_imu_diag.c"
#include <assert.h>
#include <setjmp.h>

static uint8_t user_regs[256], bank_regs[256], bank;
static uint16_t native_count;
static rt_tick_t ticks;
static unsigned irq, idle, reads, writes, rounds, deleted, detached;
static int failing_reg = -1;
static bool fail_event, fail_create, fail_start;
static bool fail_bus_find, fail_bus_config;
static bool stop_in_prepare;
static bool stop_daily_in_prepare, retry_daily_in_prepare;
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
{ assert(!irq && idle && (pin == 39 || pin == 40) && pull == 1 && cpu == 1); (void)function; }
void BSP_GPIO_Set(unsigned pin, unsigned value, unsigned cpu)
{ assert(pin == 30 && value == 1 && cpu == 1 && idle && !irq); }
struct rt_i2c_bus_device *rt_i2c_bus_device_find(const char *name)
{ assert(!strcmp(name, "i2c3") && idle && !irq); return fail_bus_find ? NULL : &fake_bus; }
int rt_i2c_configure(struct rt_i2c_bus_device *b, struct rt_i2c_configuration *cfg)
{ assert(b == &fake_bus && cfg->timeout == 50 && cfg->max_hz == 100000); return fail_bus_config ? -1 : 0; }
size_t rt_i2c_mem_read(struct rt_i2c_bus_device *b, unsigned addr, unsigned reg,
    unsigned bits, void *out, size_t length)
{
    assert(b == &fake_bus && bits == 8 && length == 1 && !irq && idle); ++reads;
    if (addr != 0x6a) return 0;
    uint8_t value = bank ? bank_regs[reg] : user_regs[reg];
    if (reg == 1) value = bank;
    if (!bank && reg == 0x4c) value = (uint8_t)(native_count >> 8);
    if (!bank && reg == 0x4b) value = (uint8_t)native_count;
    *(uint8_t *)out = value; return 1;
}
size_t rt_i2c_mem_write(struct rt_i2c_bus_device *b, unsigned addr, unsigned reg,
    unsigned bits, void *input, size_t length)
{
    assert(b == &fake_bus && addr == 0x6a && bits == 8 && length == 1 && !irq && idle); ++writes;
    if ((int)reg == failing_reg) return 0;
    uint8_t value = *(uint8_t *)input;
    if (reg == 1) bank = value;
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
    return 0;
}
