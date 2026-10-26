#ifdef NDEBUG
#undef NDEBUG
#endif
#define RT_USING_PM
#include "../apps/product/src/product_imu_diag.c"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include "support/mmc5603_fake.h"

static uint8_t user_regs[256], bank_regs[256], bank_b_regs[256], bank;
static uint8_t light_regs[256];
static unsigned light_reads, light_writes;
static int light_failing_read = -1;
static bool light_fail_restore, stop_light_in_prepare, duplicate_light_in_prepare;
static bool pause_auto_in_light_read;
static unsigned light_restore_attempts;
static fake_mmc5603_t mag_fake;
static bool stop_mag_in_prepare, duplicate_mag_in_prepare;
static bool stop_compass_in_prepare;
static char mag_log[16384];
static uint16_t native_count;
static unsigned native_resets;
static int imu_failing_read = -1, imu_read_failures;
static unsigned imu_read_skip;
static uint16_t retry_native_increment;
static bool stop_during_retry;
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
static unsigned store_delay_ms;
static bool storage_available = true, fail_storage;
static char imu_log[2048];
static jmp_buf yield;
rt_base_t rt_hw_interrupt_disable(void) { return irq++; }
void rt_hw_interrupt_enable(rt_base_t previous) { irq = (unsigned)previous; }
rt_tick_t rt_tick_get(void) { return ticks; }
rt_tick_t rt_tick_from_millisecond(unsigned ms) { return ms; }
int rt_thread_mdelay(rt_int32_t ms)
{
    ticks += ms;
    if (ms == 20) {
        native_count += retry_native_increment;
        if (stop_during_retry) {
            stop_during_retry = false;
            char *args[] = {"wf_steps", "stop"}; wf_steps(2, args);
        }
    }
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
    if (stop_light_in_prepare) {
        stop_light_in_prepare = false;
        char *args[] = {"wf_light", "stop"}; wf_light(2, args);
    }
    if (duplicate_light_in_prepare) {
        duplicate_light_in_prepare = false;
        char *args[] = {"wf_light", "start", "120"}; wf_light(3, args);
    }
    if (stop_mag_in_prepare) {
        stop_mag_in_prepare=false;
        char *args[]={"wf_mag","stop"}; wf_mag(2,args);
    }
    if (duplicate_mag_in_prepare) {
        duplicate_mag_in_prepare=false;
        char *args[]={"wf_mag","start","120"}; wf_mag(3,args);
    }
    if (stop_compass_in_prepare) {
        stop_compass_in_prepare=false;
        assert(wristflow_product_compass_active(false));
    }
    return 0;
}
void rt_kprintf(const char *format, ...)
{
    char *log = !strncmp(format,"[mag",4) ? mag_log : !strncmp(format,"[imu",4) ? imu_log : NULL;
    if (!log) return;
    size_t capacity = log == mag_log ? sizeof mag_log : sizeof imu_log;
    size_t used=strlen(log);
    va_list args; va_start(args,format);
    (void)vsnprintf(log+used,capacity-used,format,args);
    va_end(args);
}
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
    assert(b == &fake_bus && bits == 8 && !irq && idle); ++reads;
    if (addr==0x30) {
        assert(!bank); mag_fake.ms=ticks;
        return fake_mag_read(&mag_fake,(uint8_t)reg,out,length) ? length : 0;
    }
    assert(length == 1 || (reg==0x28 && length==6));
    if (addr == 0x29) {
        assert(!bank && length == 1); ++light_reads;
        if (reg == 0x88 && pause_auto_in_light_read) {
            pause_auto_in_light_read=false;
            wristflow_product_light_active(false);
        }
        if ((int)reg == light_failing_read) return 0;
        *(uint8_t *)out = light_regs[reg];
        if (reg == 0x8b) light_regs[0x8c] &= (uint8_t)~4U;
        return 1;
    }
    if (addr != 0x6a) return 0;
    if ((int)reg == imu_failing_read) {
        if (imu_read_skip) --imu_read_skip;
        else if (imu_read_failures) {
            if (imu_read_failures > 0) --imu_read_failures;
            ticks += 50; /* Transfer timeout, before the SDK returns failure. */
            return 0;
        }
    }
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
    assert(b == &fake_bus && bits == 8 && length == 1 && !irq && idle); ++writes;
    if (addr==0x30) {
        assert(!bank); mag_fake.ms=ticks;
        return fake_mag_write(&mag_fake,(uint8_t)reg,*(uint8_t *)input) ? 1 : 0;
    }
    if (addr == 0x29) {
        assert(!bank && (reg == 0x80 || reg == 0x85)); ++light_writes;
        uint8_t value = *(uint8_t *)input;
        if (reg == 0x80 && value == 0x0c) ++light_restore_attempts;
        if (light_fail_restore && reg == 0x80 && value == 0x0c) return 0;
        light_regs[reg] = value; return 1;
    }
    assert(addr == 0x6a);
    if ((int)reg == failing_reg) return 0;
    uint8_t value = *(uint8_t *)input;
    if (reg == 1) bank = value;
    else if (bank == 0xa0) bank_b_regs[reg] = value;
    else if (bank) bank_regs[reg] = value;
    else { user_regs[reg] = value; if (reg == 0x19 && (value & 2)) { native_count = 0; ++native_resets; } }
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
static void light_cmd(const char *action, const char *span)
{ char *args[] = {"wf_light", (char *)action, (char *)span}; wf_light(span ? 3 : 2, args); }
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
    ticks += store_delay_ms;
    step_lengths[key]=WRISTFLOW_STEPS_RECORD_BYTES; return !fail_storage;
}
static void recovery_integration(void)
{
    assert(wristflow_product_light_enable(false)); wristflow_product_wrist_enable(false); cycle();
    assert(!idle && !light.snapshot.owned && !mag.snapshot.owned && !wrist.changed);
    wristflow_steps_checkpoint_t seed = {.local_day=20745, .total=313,
        .coverage_start_utc=1792407600, .saved_utc=1792411260,
        .hourly={[19]=50,[20]=80}, .hours_valid=(1U<<19)|(1U<<20)};
    assert(wristflow_steps_encode(&seed,23,step_records[0]));
    step_lengths[0]=WRISTFLOW_STEPS_RECORD_BYTES; step_lengths[1]=0;
    ready=false; cancel_requested=false; ticks=0; daily_lit=true; storage_available=true; native_resets=0;
    max_wait=60000; service_clock=(wristflow_alarm_clock_t){.utc=1792411300,.valid=true};
    assert(wristflow_product_steps_start()); cycle(); native_count=17; cycle();
    assert(daily_cached.metric.total==330 && daily_cached.metric.hourly[20]==97);
    wristflow_product_wrist_enable(true); cycle();
    assert(wrist_runtime && irq_adapter.armed);
    light_regs[0x8c]=4; assert(wristflow_product_light_enable(true));
    wristflow_product_light_active(true); cycle();
    for (unsigned i=0;i<10 && !auto_cached.valid;++i) { light_regs[0x8c]=4; cycle(); }
    assert(auto_cached.valid && auto_requested && auto_runtime);
    const uint8_t regs[] = {1,0x10,0x19,0x4c,0x4b,0x4c};
    for (unsigned i=0;i<sizeof regs;++i) {
        wristflow_steps_snapshot_t before=daily_cached;
        wristflow_steps_store_t saved=checkpoint_cached;
        unsigned before_writes=writes, resets=native_resets;
        uint32_t light_generation=auto_generation;
        imu_failing_read=regs[i]; imu_read_failures=1; imu_read_skip=i==5 ? 1 : 0;
        retry_native_increment=3; light_regs[0x8c]=4; cycle(); retry_native_increment=0;
        assert(cached.phase==WF_IMU_RUNNING && daily_cached.metric.total_valid && daily.baseline);
        assert(daily_cached.metric.total==before.metric.total+3);
        assert(daily_cached.metric.hourly[20]==before.metric.hourly[20]+3 && daily_cached.metric.hourly[19]==50);
        assert(daily_cached.metric.hours_valid==seed.hours_valid && daily.previous_native==native_count);
        assert(checkpoint_cached.generation==saved.generation && checkpoint_cached.attempts==saved.attempts);
        assert(checkpoint_cached.dirty);
        assert(writes==before_writes);
        assert(native_resets==resets);
        assert(cached.poll_retries==i+1 && cached.poll_recoveries==i+1);
        assert(wrist_requested && wrist_runtime && irq_adapter.armed && !wrist_cached.observations);
        assert(auto_requested && auto_runtime && !auto_fault && auto_generation==light_generation);
        assert(io_failure_cached.failures==i+1 && !io_failure_cached.write && io_failure_cached.address==0x6a);
        assert(io_failure_cached.reg==regs[i] && io_failure_cached.requested==1 && !io_failure_cached.returned);
        assert(io_failure_cached.completed_ms-io_failure_cached.started_ms==50);
    }
    imu_log[0]=0; unsigned before=reads+writes+store_reads+store_writes;
    daily_cmd("status"); assert(before==reads+writes+store_reads+store_writes);
    assert(strstr(imu_log,"retries=6 recovered=6") && strstr(imu_log,"write=0 addr=0x6a reg=0x4c requested=1 returned=0"));
    daily_cmd("save"); cycle();
    assert(checkpoint_cached.saved_valid && checkpoint_cached.saved.total==348 && checkpoint_cached.generation==24);
    assert(checkpoint_cached.saved.hourly[19]==50 && checkpoint_cached.saved.hourly[20]==115);
    /* Recovered daily delta across an hour has no invented per-hour split. */
    ticks += 1000U-ticks%1000U; service_clock.utc=1792414799U-ticks/1000U; ++service_clock.revision;
    wristflow_product_steps_clock_changed(); cycle();
    assert(daily_cached.observation.utc==1792414799U);
    ticks+=950; imu_failing_read=0x4b; imu_read_failures=1; retry_native_increment=3;
    wristflow_product_steps_clock_changed(); cycle(); retry_native_increment=0;
    assert(daily_cached.observation.utc>=1792414800U && daily_cached.metric.total==351);
    assert(daily_cached.metric.hourly[20]==115 && !(daily_cached.metric.hours_valid&(1U<<21)));
    daily_cmd("save"); cycle();
    uint32_t generation=checkpoint_cached.generation;
    /* Exhaustion preserves the last verified checkpoint and does not loop. */
    imu_failing_read=0x10; imu_read_failures=2; cycle();
    assert(cached.phase==WF_IMU_FAULT && cached.stop_verified && !daily_cached.metric.total_valid);
    assert(checkpoint_cached.saved_valid && checkpoint_cached.saved.total==351);
    assert(checkpoint_cached.generation==generation && native_resets==1);
    assert(!wrist_runtime && !irq_adapter.owned && auto_fault && !auto_requested && !idle);
    assert(io_failure_cached.reg==0x10 && !io_failure_cached.write && io_failure_cached.failures==9);
    before=reads+writes; cycle(); assert(reads+writes==before && !idle);
    imu_failing_read=-1;
    /* A user stop racing with backoff cannot recover/restart the service. */
    daily_cmd("retry"); cycle();
    unsigned resets_before_stop=native_resets;
    imu_failing_read=0x4b; imu_read_failures=1; stop_during_retry=true;
    cycle(); cycle();
    assert(cached.phase==WF_IMU_READY && cached.stop_verified && !cancel_requested && !idle);
    assert(!daily_cached.metric.total_valid && !wrist_runtime && !irq_adapter.owned);
    assert(native_resets==resets_before_stop);
    imu_failing_read=-1;
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
    /* LTR uses the same worker but a fixed, separate address. Default/status
     * do no LTR I/O; the persistent wrist preference is never cancelled. */
    assert(!light_reads && !light_writes && light_cached.phase == WF_LIGHT_IDLE);
    before = reads + writes; light_cmd("status", NULL); assert(reads + writes == before);
    daily_cmd("retry"); cycle(); wristflow_product_steps_screen(false); cycle();
    wristflow_product_wrist_enable(true); cycle();
    assert(wrist_runtime && irq_adapter.armed && !idle);
    light_regs[0x86] = 0xa0; light_regs[0x87] = 5;
    light_regs[0x80] = 0x0c; light_regs[0x85] = 4;
    light_regs[0x88] = 20; light_regs[0x8a] = 100;
    native_count = 17;
    before = reads + writes;
    light_cmd("start", "2"); light_cmd("start", "120");
    assert(light_pending && light_seconds == 2 && reads + writes == before);
    wrist_cmd("start", "2", "xneg"); assert(!wrist_pending);
    duplicate_light_in_prepare = true; cycle();
    assert(light_cached.phase == WF_LIGHT_RUNNING && !light_pending && light_cached.duration_ms == 2000 && idle);
    before = reads + writes; light_cmd("status", NULL); assert(reads + writes == before);
    cycle(); assert(last_wait == 100 && light_cached.entered_valid && light_regs[0x80] == 1);
    light_regs[0x8c] = 4; cycle();
    assert(last_wait == 200 && light_cached.samples == 1 && light_cached.sample_valid && light_cached.ch0 == 100);
    assert(native_count == 17 && daily_cached.metric.total == 17 && wrist_requested && wrist_runtime && irq_adapter.armed);
    light_cmd("stop", NULL); cycle();
    assert(light_cached.restored && !light_cached.owned && !idle && light_regs[0x80] == 0x0c && light_regs[0x85] == 4);
    assert(cached.phase == WF_IMU_RUNNING && native_count == 17 && wrist_runtime && irq_adapter.armed);
    before = light_reads + light_writes; cycle();
    assert(last_wait == 60000 && light_reads + light_writes == before);
    light_cmd("start", "1"); stop_light_in_prepare = true; cycle(); cycle();
    assert(light_cached.ended == WF_LIGHT_CANCELLED && !light_cached.owned && !idle && light_regs[0x80] == 0x0c);
    light_cmd("start", "1"); cycle(); cycle();
    ticks += 5000; cycle();
    assert(light_cached.ended == WF_LIGHT_DEADLINE && light_cached.restored && !idle);
    light_cmd("start", "2"); cycle(); cycle();
    light_failing_read = 0x8c; cycle(); light_failing_read = -1;
    assert(light_cached.phase == WF_LIGHT_FAULT && light_cached.restored && !light_cached.owned && !idle);
    assert(cached.phase == WF_IMU_RUNNING && native_count == 17 && wrist_runtime && irq_adapter.armed);
    light_cmd("start", "2"); cycle(); cycle();
    light_fail_restore = true; light_cmd("stop", NULL); cycle();
    assert(light_cached.phase == WF_LIGHT_FAULT && light_cached.owned && !light_cached.restored && idle);
    before = light_reads + light_writes; cycle(); assert(light_reads + light_writes == before && idle);
    light_cmd("start", "120"); wrist_cmd("start", "2", "xneg");
    assert(!light_pending && !wrist_pending);
    light_fail_restore = false; light_cmd("stop", NULL); cycle();
    assert(light_cached.restored && !light_cached.owned && !idle && wrist_runtime && native_count == 17);
    light_cmd("start", "2"); cycle(); cycle(); daily_cmd("stop"); cycle();
    assert(light_cached.restored && !light_cached.owned && cached.stop_verified && !idle && !wrist_runtime);
    daily_cmd("retry"); cycle();
    wrist_cmd("start", "2", "xneg"); assert(wrist_pending);
    light_cmd("start", "2"); assert(!light_pending); cycle();
    wrist_cmd("stop", NULL, NULL); cycle();
    /* Fail only the LTR preparation, after the daily counter's preparation
     * succeeded. Its uncertain cleanup must not fault a healthy counter. */
    for (unsigned failure = 0; failure < 2; ++failure) {
        before = light_reads + light_writes;
        light_cmd("start", "2");
        if (failure) bus_config_fail_after = 2; else bus_find_fail_after = 2;
        cycle();
        assert(light_cached.phase == WF_LIGHT_FAULT && !light_cached.owned && !idle);
        assert(light_reads + light_writes == before && cached.phase == WF_IMU_RUNNING);
        light_cmd("start", "2"); cycle(); cycle();
        if (failure) bus_config_fail_after = 2; else bus_find_fail_after = 2;
        light_cmd("stop", NULL); cycle();
        assert(light_cached.phase == WF_LIGHT_FAULT && light_cached.owned && idle);
        assert(cached.phase == WF_IMU_RUNNING);
        before = light_reads + light_writes; cycle();
        assert(before == light_reads + light_writes && idle);
        light_cmd("stop", NULL); cycle();
        assert(light_cached.restored && !light_cached.owned && !idle);
    }
    /* Flash runs after light_work. A slow save may pass the deadline, but the
     * next iteration stops using the original time instead of taking a sample. */
    ready = false; storage_available = true; cancel_requested = false;
    assert(wristflow_product_steps_start()); cycle();
    light_cmd("start", "2"); cycle(); cycle();
    native_count = 1; store_delay_ms = 5000; daily_cmd("save"); cycle(); store_delay_ms = 0;
    assert(ticks - light_cached.started_ms >= light_cached.duration_ms && light_cached.phase == WF_LIGHT_RUNNING);
    before = light_cached.samples; cycle();
    assert(light_cached.phase == WF_LIGHT_DONE && light_cached.ended == WF_LIGHT_DEADLINE);
    assert(light_cached.samples == before && light_cached.restored && !light_cached.owned && !idle);
    /* Product mode is a command/cache interface on the SAME sensor owner.
     * Saved mode can be on while OFF/flashlight/AOD qualification is false. */
    assert(!wristflow_product_light_snapshot().enabled);
    before=light_reads+light_writes;
    assert(wristflow_product_light_enable(true));
    assert(wristflow_product_light_snapshot().enabled && !wristflow_product_light_snapshot().valid);
    cycle(); assert(light_reads+light_writes==before && !idle);
    wristflow_product_light_active(true); cycle(); cycle();
    assert(light.continuous && light_cached.entered_valid && idle);
    light_regs[0x8c]=4; cycle();
    wristflow_product_light_snapshot_t automatic=wristflow_product_light_snapshot();
    assert(automatic.enabled && automatic.valid && automatic.brightness>=10 && automatic.brightness<=100);
    /* Device shade regression: native STATUS04 frames continue, but 0/1 has
     * ratio1. Appendix ELSE estimates zero; it must not trip the stale timer. */
    uint32_t shade_session=automatic.session;
    light_regs[0x88]=1; light_regs[0x89]=light_regs[0x8a]=light_regs[0x8b]=0;
    for (unsigned i=0;i<12;++i) {
        uint32_t prior_sequence=automatic.sequence, prior_ms=automatic.sampled_ms;
        light_regs[0x8c]=4; cycle(); automatic=wristflow_product_light_snapshot();
        assert(automatic.enabled && automatic.valid && !automatic.fault && automatic.session==shade_session);
        assert(automatic.sequence>prior_sequence && automatic.sampled_ms!=prior_ms);
    }
    assert(automatic.brightness>=10 && automatic.brightness<=12 && automatic.millilux==0); /* two-point deadband */
    light_regs[0x88]=20; light_regs[0x8a]=100; light_regs[0x8c]=4; cycle();
    before=light_reads+light_writes;
    (void)wristflow_product_light_snapshot(); assert(before==light_reads+light_writes);
    light_cmd("start","1"); wrist_cmd("start","1","xneg");
    assert(!light_pending && !wrist_pending);
    wristflow_product_light_active(false);
    assert(!wristflow_product_light_snapshot().valid);
    cycle(); assert(!idle && light_cached.restored && !light_cached.owned);
    assert(wristflow_product_light_snapshot().enabled); /* pause does not overwrite mode */
    before=light_reads+light_writes; cycle(); assert(light_reads+light_writes==before);
    wristflow_product_light_active(true); cycle(); cycle(); light_regs[0x8c]=4; cycle();
    assert(wristflow_product_light_snapshot().valid);
    ticks+=121000; light_regs[0x8c]=4; cycle();
    assert(light.continuous && light_cached.phase==WF_LIGHT_RUNNING && wristflow_product_light_snapshot().valid);
    /* No NEW data makes the public cache stale and closes mode, without a
     * fabricated zero sample or automatic sensor retry. */
    ticks+=5000; assert(!wristflow_product_light_snapshot().valid); cycle();
    assert(!wristflow_product_light_snapshot().enabled && wristflow_product_light_snapshot().fault && !idle);
    before=light_reads+light_writes; cycle(); cycle(); assert(before==light_reads+light_writes);
    assert(wristflow_product_light_enable(true)); cycle(); cycle(); light_regs[0x8c]=4; cycle();
    assert(wristflow_product_light_snapshot().valid);
    /* A policy change inside DATA I/O must not publish the old frame or retry
     * failed restore twice. Ownership/IDLE stays until an explicit command. */
    unsigned restores=light_restore_attempts;
    pause_auto_in_light_read=true; light_fail_restore=true; light_regs[0x8c]=4; cycle();
    automatic=wristflow_product_light_snapshot();
    assert(!automatic.enabled && !automatic.valid && automatic.fault && automatic.cleanup_pending && idle);
    assert(light_restore_attempts==restores+1);
    before=light_reads+light_writes; cycle(); assert(before==light_reads+light_writes && idle);
    light_fail_restore=false; assert(wristflow_product_light_enable(true)); cycle();
    assert(!light_cached.owned && !idle); /* still paused; explicit cleanup only */
    wristflow_product_light_active(true); cycle(); cycle(); light_regs[0x8c]=4; cycle();
    assert(wristflow_product_light_snapshot().valid);
    light_failing_read=0x8c; cycle(); light_failing_read=-1;
    assert(!wristflow_product_light_snapshot().enabled && wristflow_product_light_snapshot().fault && !idle);
    /* Restored-on startup request may race the first daily START event. */
    ready=false; storage_available=false; cancel_requested=false;
    assert(wristflow_product_steps_start());
    assert(wristflow_product_light_enable(true)); wristflow_product_light_active(true); cycle(); cycle();
    light_regs[0x8c]=4; cycle();
    assert(cached.phase==WF_IMU_RUNNING && wristflow_product_light_snapshot().enabled && wristflow_product_light_snapshot().valid);
    assert(wristflow_product_light_enable(false)); cycle(); assert(!idle && light_cached.restored);
    /* MMC has no boot/status I/O. Its independent on-demand address shares
     * the existing worker while daily steps and native wrist keep ownership. */
    assert(!mag_fake.operations && mag_cached.phase==WF_MAG_IDLE);
    char *mag_status[]={"wf_mag","status"};
    before=reads+writes; wf_mag(2,mag_status); assert(reads+writes==before);
    fake_mag_init(&mag_fake);
    char *mag_start[]={"wf_mag","start","2"};
    char *mag_stop[]={"wf_mag","stop"};
    char *mag_duplicate[]={"wf_mag","start","120"};
    native_count=9;
    wristflow_product_wrist_enable(true); cycle();
    assert(wrist_runtime && irq_adapter.armed);
    wf_mag(3,mag_start); wf_mag(3,mag_duplicate);
    assert(mag_pending && mag_seconds==2 && !mag_fake.operations);
    light_cmd("start","2"); assert(!light_pending);
    duplicate_mag_in_prepare=true; cycle();
    assert(mag_cached.phase==WF_MAG_RUNNING && mag_cached.duration_ms==2000 && idle);
    for (unsigned i=0;i<20 && !mag_cached.samples;++i) cycle();
    assert(mag_cached.selftest_valid && mag_cached.selftest_passed && mag_cached.samples==1 && mag_cached.sample_valid);
    assert(mag_cached.selftest_trace==0x7f && mag_cached.reset_status==0x10 && mag_cached.selftest_post_status==0x10);
    assert(!memcmp(mag_cached.selftest_raw,mag_fake.data,9) && mag_cached.selftest_polls==1);
    assert(mag_cached.x==12345 && mag_cached.y==-23456 && mag_cached.z==34567);
    assert(native_count==9 && daily_cached.metric.total==9 && wrist_runtime && irq_adapter.armed);
    before=mag_fake.operations; wf_mag(2,mag_status); assert(mag_fake.operations==before);
    wf_mag(2,mag_stop); cycle();
    assert(mag_cached.stopped && !mag_cached.owned && !idle && wrist_runtime);
    /* Auto can already be running, or qualify during MMC. Neither mode nor
     * generation is mutated by the magnetic diagnostic. */
    assert(wristflow_product_light_enable(true)); wristflow_product_light_active(true);
    cycle(); cycle(); light_regs[0x8c]=4; cycle();
    assert(wristflow_product_light_snapshot().valid);
    uint32_t light_generation=auto_generation;
    wf_mag(3,mag_start); cycle();
    for (unsigned i=0;i<20 && !mag_cached.samples;++i) { light_regs[0x8c]=4; cycle(); }
    assert(mag_cached.samples && mag_cached.sample_valid && auto_generation==light_generation && auto_requested);
    assert(light.continuous && light.snapshot.phase==WF_LIGHT_RUNNING && wrist_runtime && native_count==9);
    wf_mag(2,mag_stop); cycle();
    wristflow_product_light_active(false); cycle();
    wf_mag(3,mag_start); cycle();
    wristflow_product_light_active(true);
    for (unsigned i=0;i<20 && !mag_cached.samples;++i) { light_regs[0x8c]=4; cycle(); }
    assert(mag_cached.samples && auto_requested && light.continuous);
    wf_mag(2,mag_stop); cycle();
    assert(wristflow_product_light_enable(false)); cycle();
    wristflow_product_wrist_enable(false); cycle();
    wrist_cmd("start","2","xneg"); assert(wrist_pending);
    wf_mag(3,mag_start); assert(!mag_pending); cycle();
    wrist_cmd("stop",NULL,NULL); cycle();
    light_cmd("start","2"); assert(light_pending);
    wf_mag(3,mag_start); assert(!mag_pending); cycle();
    light_cmd("stop",NULL); cycle();
    /* Failed cleanup holds the single PM responsibility and never retries in
     * the background. Only explicit stop can release this uncertain state. */
    wf_mag(3,mag_start); cycle(); cycle();
    mag_fake.fail_reset=true; wf_mag(2,mag_stop); cycle();
    assert(mag_cached.phase==WF_MAG_FAULT && mag_cached.owned && !mag_cached.stopped && idle);
    before=mag_fake.operations; cycle(); assert(mag_fake.operations==before && idle);
    wf_mag(3,mag_start); light_cmd("start","2"); assert(!mag_pending && !light_pending);
    mag_fake.fail_reset=false; wf_mag(2,mag_stop); cycle();
    assert(mag_cached.stopped && !mag_cached.owned && !idle);
    wf_mag(3,mag_start); stop_mag_in_prepare=true; cycle(); cycle();
    assert(mag_cached.ended==WF_MAG_CANCELLED && !mag_cached.owned && !idle);
    wf_mag(3,mag_start); cycle(); cycle(); daily_cmd("stop"); cycle();
    assert(mag_cached.stopped && !mag_cached.owned && cached.stop_verified && !idle);
    daily_cmd("retry"); cycle();
    wf_mag(3,mag_start); bus_find_fail_after=2; cycle();
    assert(mag_cached.phase==WF_MAG_FAULT && !mag_cached.owned && !idle && cached.phase==WF_IMU_RUNNING);
    wf_mag(3,mag_start); cycle(); cycle();
    before=mag_cached.samples; ticks+=5000; cycle();
    assert(mag_cached.ended==WF_MAG_DEADLINE && mag_cached.stopped && mag_cached.samples==before && !idle);
    /* Only an explicit accepted command may select the experimental decoder.
     * Rejected duplicate/default/invalid requests cannot alter that selection. */
    char *selftest_encoded[]={"wf_mag","selftest","encoded"};
    char *selftest_raw[]={"wf_mag","selftest","raw80"};
    char *selftest_invalid[]={"wf_mag","selftest","Encoded"};
    char *selftest_extra[]={"wf_mag","selftest","encoded","10"};
    wf_mag(3,selftest_invalid); wf_mag(4,selftest_extra);
    assert(!mag_pending);
    fake_mag_init(&mag_fake); mag_fake.factory[0]=0x63; mag_fake.factory[1]=0x7a; mag_fake.factory[2]=0x72;
    mag_log[0]=0;
    wf_mag(3,selftest_encoded); wf_mag(3,selftest_raw); wf_mag(3,mag_start);
    assert(mag_pending && mag_selftest_only && mag_seconds==10 && mag_threshold_mode==WF_MAG_THRESHOLD_ENCODED_EXPERIMENTAL);
    duplicate_mag_in_prepare=true; cycle();
    for (unsigned i=0;i<30 && mag_cached.phase==WF_MAG_RUNNING;++i) cycle();
    assert(mag_cached.phase==WF_MAG_DONE && mag_cached.ended==WF_MAG_SELFTEST_COMPLETE);
    assert(mag_cached.selftest_only && mag_cached.threshold_mode==WF_MAG_THRESHOLD_ENCODED_EXPERIMENTAL);
    assert(mag_cached.duration_ms==10000 && mag_cached.selftest_valid && mag_cached.selftest_passed);
    assert(mag_cached.selftest_threshold[0]==0x5c && mag_cached.selftest_threshold[1]==0x13 && mag_cached.selftest_threshold[2]==0x2c);
    assert(!mag_cached.samples && !mag_cached.sample_valid && mag_cached.stopped && !mag_cached.owned && !idle);
    assert(mag_fake.triggers==1 && mag_fake.resets==2 && !strstr(mag_log,"[mag-data]"));
    assert(strstr(mag_log,"selftest_only=1 threshold=encoded experimental=1"));
    before=mag_fake.operations; wf_mag(2,mag_status); assert(mag_fake.operations==before);
    assert(strstr(mag_log,"[mag-selftest-raw]") && strstr(mag_log,"[mag-mode]"));
    /* The raw80 selftest-only control coexists with normal auto-light/wrist. */
    uint16_t compare_native=native_count;
    wristflow_product_wrist_enable(true);
    assert(wristflow_product_light_enable(true)); wristflow_product_light_active(true);
    cycle(); light_regs[0x8c]=4; cycle();
    light_generation=auto_generation;
    wf_mag(3,selftest_raw); cycle();
    for (unsigned i=0;i<30 && mag_cached.phase==WF_MAG_RUNNING;++i) { light_regs[0x8c]=4; cycle(); }
    assert(mag_cached.ended==WF_MAG_SELFTEST_COMPLETE && mag_cached.threshold_mode==WF_MAG_THRESHOLD_RAW80 && mag_cached.selftest_only);
    assert(mag_cached.selftest_threshold[0]==0x4f && mag_cached.selftest_threshold[1]==0x61 && mag_cached.selftest_threshold[2]==0x5b);
    assert(!mag_cached.samples && mag_cached.stopped && !mag_cached.owned && !mag_busy);
    assert(auto_requested && auto_generation==light_generation && light.continuous && wrist_runtime);
    assert(native_count==compare_native);
    assert(strstr(mag_log,"selftest_only=1 threshold=raw80 experimental=0"));
    assert(wristflow_product_light_enable(false)); wristflow_product_wrist_enable(false); cycle();
    wf_mag(3,mag_start); cycle();
    for (unsigned i=0;i<20 && !mag_cached.samples;++i) cycle();
    assert(!mag_cached.selftest_only && mag_cached.threshold_mode==WF_MAG_THRESHOLD_RAW80 && mag_cached.samples);
    wf_mag(2,mag_stop); cycle();
    /* Native FAIL is still a fault, not a completed comparison/PASS. */
    fake_mag_init(&mag_fake); mag_fake.selftest_fail=true;
    wf_mag(3,selftest_encoded); cycle();
    for (unsigned i=0;i<30 && mag_cached.phase==WF_MAG_RUNNING;++i) cycle();
    assert(mag_cached.ended==WF_MAG_SELFTEST_FAILED && mag_cached.phase==WF_MAG_FAULT);
    assert(mag_cached.selftest_valid && !mag_cached.selftest_passed && !mag_cached.samples && mag_cached.stopped && !idle);
    /* A native PASS followed by failed cleanup keeps ownership and PM duty. */
    fake_mag_init(&mag_fake); wf_mag(3,selftest_encoded); cycle();
    for (unsigned i=0;i<20 && mag_fake.triggers==0;++i) cycle();
    assert(mag_fake.triggers==1 && mag_cached.phase==WF_MAG_RUNNING);
    mag_fake.fail_reset=true;
    for (unsigned i=0;i<30 && mag_cached.phase==WF_MAG_RUNNING;++i) cycle();
    assert(mag_cached.selftest_valid && mag_cached.selftest_passed && mag_cached.ended==WF_MAG_IO_FAILURE);
    assert(mag_cached.owned && !mag_cached.stopped && !mag_cached.samples && idle && mag_busy);
    before=mag_fake.operations; cycle(); assert(mag_fake.operations==before);
    wf_mag(3,selftest_raw); assert(!mag_pending);
    mag_fake.fail_reset=false; wf_mag(2,mag_stop); cycle(); assert(!idle && mag_cached.stopped);
    /* Cancellation during the daily bus preparation can precede begin_session.
     * Preserve the old result under its old session; never invent a new run. */
    uint32_t previous_session=mag_cached.session;
    fake_mag_init(&mag_fake); wf_mag(3,selftest_encoded); stop_mag_in_prepare=true; cycle(); cycle();
    assert(mag_cached.ended==WF_MAG_CANCELLED && mag_cached.session==previous_session);
    assert(mag_cached.selftest_valid && !mag_fake.triggers && !mag_cached.samples && !idle);
    fake_mag_init(&mag_fake); wf_mag(3,selftest_encoded); cycle(); cycle();
    ticks+=10000; cycle();
    assert(mag_cached.ended==WF_MAG_DEADLINE && mag_cached.stopped && !mag_cached.selftest_valid && !mag_cached.samples && !idle);
    char *sample_encoded[]={"wf_mag","sample","encoded","2"};
    char *sample_invalid[]={"wf_mag","sample","encoded","0"};
    const char *bad_seconds[]={"0","121","-1","+1","2x",""};
    for (unsigned i=0;i<sizeof bad_seconds/sizeof bad_seconds[0];++i) {
        sample_invalid[3]=(char *)bad_seconds[i]; wf_mag(4,sample_invalid); assert(!mag_pending);
    }
    sample_invalid[2]="raw80"; sample_invalid[3]="2"; wf_mag(4,sample_invalid); assert(!mag_pending);
    /* The candidate can now sample, but only through its explicit entry point.
     * A duplicate selftest or old start cannot replace the queued request. */
    fake_mag_init(&mag_fake); mag_fake.factory[0]=0x63; mag_fake.factory[1]=0x7a; mag_fake.factory[2]=0x72;
    wristflow_product_wrist_enable(true);
    assert(wristflow_product_light_enable(true)); wristflow_product_light_active(true);
    cycle(); light_regs[0x8c]=4; cycle(); light_generation=auto_generation;
    uint16_t sample_native=native_count;
    mag_log[0]=0;
    wf_mag(4,sample_encoded); wf_mag(3,selftest_raw); wf_mag(3,mag_start);
    assert(mag_pending && !mag_selftest_only && mag_threshold_mode==WF_MAG_THRESHOLD_ENCODED_EXPERIMENTAL && mag_seconds==2);
    duplicate_mag_in_prepare=true; cycle();
    for (unsigned i=0;i<30 && !mag_cached.samples;++i) { light_regs[0x8c]=4; cycle(); }
    assert(mag_cached.phase==WF_MAG_RUNNING && mag_cached.selftest_passed && mag_cached.samples==1 && mag_cached.sample_valid);
    assert(mag_cached.threshold_mode==WF_MAG_THRESHOLD_ENCODED_EXPERIMENTAL && !mag_cached.selftest_only && mag_cached.duration_ms==2000);
    assert(mag_cached.selftest_threshold[0]==0x5c && mag_cached.selftest_threshold[1]==0x13 && mag_cached.selftest_threshold[2]==0x2c);
    assert(mag_cached.x==12345 && mag_cached.y==-23456 && mag_cached.z==34567);
    assert(strstr(mag_log,"selftest_only=0 threshold=encoded experimental=1") && strstr(mag_log,"[mag-data]"));
    assert(auto_requested && auto_generation==light_generation && light.continuous && wrist_runtime && native_count==sample_native);
    before=mag_fake.operations; wf_mag(2,mag_status); assert(mag_fake.operations==before);
    wf_mag(2,mag_stop); cycle();
    assert(mag_cached.ended==WF_MAG_CANCELLED && mag_cached.stopped && !mag_cached.owned && !mag_busy);
    assert(wristflow_product_light_enable(false)); wristflow_product_wrist_enable(false); cycle();
    wf_mag(3,mag_start); cycle();
    for (unsigned i=0;i<30 && !mag_cached.samples;++i) cycle();
    assert(mag_cached.threshold_mode==WF_MAG_THRESHOLD_RAW80 && !mag_cached.selftest_only && mag_cached.samples);
    wf_mag(2,mag_stop); cycle();
    fake_mag_init(&mag_fake); mag_fake.selftest_fail=true;
    wf_mag(4,sample_encoded); cycle();
    for (unsigned i=0;i<30 && mag_cached.phase==WF_MAG_RUNNING;++i) cycle();
    assert(mag_cached.ended==WF_MAG_SELFTEST_FAILED && !mag_cached.samples && mag_cached.stopped && !idle);
    fake_mag_init(&mag_fake); wf_mag(4,sample_encoded); cycle();
    for (unsigned i=0;i<30 && !mag_cached.samples;++i) cycle();
    before=mag_cached.samples; ticks+=2000; cycle();
    assert(mag_cached.ended==WF_MAG_DEADLINE && mag_cached.samples==before && mag_cached.stopped && !idle);
    fake_mag_init(&mag_fake); wf_mag(4,sample_encoded); cycle();
    for (unsigned i=0;i<30 && !mag_cached.samples;++i) cycle();
    mag_fake.fail_reset=true; wf_mag(2,mag_stop); cycle();
    assert(mag_cached.owned && !mag_cached.stopped && mag_cached.ended==WF_MAG_IO_FAILURE && idle);
    before=mag_fake.operations; cycle(); assert(mag_fake.operations==before);
    wf_mag(4,sample_encoded); assert(!mag_pending);
    mag_fake.fail_reset=false; wf_mag(2,mag_stop); cycle(); assert(!idle && mag_cached.stopped);
    /* Product foreground owns the existing worker, and rejects bounded
     * diagnostics even before the first worker turn publishes mag_busy. */
    fake_mag_init(&mag_fake);
    assert(wristflow_product_compass_active(true));
    wf_mag(3,mag_start); assert(!mag_pending);
    char *compass_light[]={"wf_light","start","2"}; wf_light(3,compass_light); assert(!light_pending);
    char *compass_wrist[]={"wf_wrist","start","2","xpos"}; wf_wrist(4,compass_wrist); assert(!wrist_pending);
    assert(!wristflow_product_compass_snapshot().valid);
    cycle();
    for (unsigned i=0;i<30 && !compass_cached.valid;++i) cycle();
    wf_compass_snapshot_t cv=wristflow_product_compass_snapshot();
    assert(cv.requested && cv.active && cv.valid && cv.selftest_passed && !cv.fault && cv.sequence==1);
    assert(cv.x==12345 && cv.y==-23456 && mag.snapshot.foreground && idle);
    uint32_t compass_session=cv.session, compass_seq=cv.sequence;
    uint32_t saved_ticks=ticks; ticks+=1000;
    assert(!wristflow_product_compass_snapshot().valid && wristflow_product_compass_snapshot().reason==WF_COMPASS_STALE);
    ticks=saved_ticks;
    fake_mag_counts(&mag_fake,10000,10000,5000);
    for (unsigned i=0;i<5 && compass_cached.sequence==compass_seq;++i) cycle();
    cv=wristflow_product_compass_snapshot(); assert(cv.valid && cv.heading_degrees==45 && cv.sequence>compass_seq);
    assert(wristflow_product_compass_active(false));
    assert(!wristflow_product_compass_snapshot().valid && !wristflow_product_compass_snapshot().active);
    cycle(); assert(!compass_owner && mag_cached.stopped && !mag_cached.owned && !idle);
    /* Rapid leave/re-enter must never replay the old cached sample. */
    assert(wristflow_product_compass_active(true));
    assert(!wristflow_product_compass_snapshot().valid); cycle();
    for (unsigned i=0;i<30 && !compass_cached.valid;++i) cycle();
    assert(compass_cached.valid && compass_cached.session>compass_session);
    mag_fake.fail_reset=true; assert(wristflow_product_compass_active(false)); cycle();
    assert(compass_cached.cleanup_pending && compass_owner && mag_cached.owned && !mag_cached.stopped && idle);
    before=mag_fake.operations; cycle(); cycle(); assert(mag_fake.operations==before);
    assert(wristflow_product_compass_active(false)); cycle(); assert(mag_fake.operations==before);
    assert(!wristflow_product_compass_active(true));
    /* Only the explicit stop command retries failed hardware cleanup. */
    mag_fake.fail_reset=false; wf_mag(2,mag_stop); cycle();
    assert(!compass_owner && mag_cached.stopped && !idle);
    assert(wristflow_product_compass_active(true)); cycle();
    for (unsigned i=0;i<30 && !compass_cached.valid;++i) cycle();
    assert(compass_cached.valid && !compass_cached.cleanup_pending);
    assert(wristflow_product_compass_active(false)); cycle();
    /* Deactivation while preparing I2C cannot start a hidden session. */
    fake_mag_init(&mag_fake); assert(wristflow_product_compass_active(true));
    stop_compass_in_prepare=true; cycle(); cycle();
    assert(!compass_requested && !compass_owner && !mag_fake.triggers && !idle);
    fake_mag_init(&mag_fake); mag_fake.selftest_fail=true;
    assert(wristflow_product_compass_active(true)); cycle();
    for (unsigned i=0;i<30 && !compass_cached.fault;++i) cycle();
    assert(compass_cached.fault && compass_cached.reason==WF_COMPASS_SELFTEST_FAILURE && !compass_cached.valid);
    before=mag_fake.operations;
    assert(wristflow_product_compass_active(true)); cycle(); assert(mag_fake.operations==before);
    assert(wristflow_product_compass_active(false)); cycle();
    /* Normal automatic brightness and persistent wrist policy survive a run. */
    fake_mag_init(&mag_fake); wristflow_product_wrist_enable(true);
    assert(wristflow_product_light_enable(true)); wristflow_product_light_active(true); cycle();
    light_regs[0x8c]=4; cycle(); light_generation=auto_generation;
    assert(wristflow_product_compass_active(true)); cycle();
    for (unsigned i=0;i<30 && !compass_cached.valid;++i) { light_regs[0x8c]=4; cycle(); }
    assert(compass_cached.valid && auto_generation==light_generation && auto_requested && wrist_runtime);
    assert(wristflow_product_compass_active(false)); cycle();
    assert(!compass_owner && mag_cached.stopped && auto_requested && wrist_runtime);
    recovery_integration();
    return 0;
}
