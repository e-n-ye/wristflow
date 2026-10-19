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
static struct rt_thread thread;
static struct rt_i2c_bus_device fake_bus;
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
        assert(wait > 0 && wait <= 1000); ticks += (unsigned)wait;
    }
    return *bits ? 0 : -1;
}
rt_thread_t rt_thread_create(const char *name, void (*entry)(void *), void *context,
    unsigned stack, unsigned priority, unsigned slice)
{
    assert(!strcmp(name, "wf_imu") && stack == 2048 && priority == 24 && slice == 10);
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
    return 0;
}
