#define RT_USING_PM
#include "../apps/product/src/product_motor.c"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>

static struct rt_thread fake_thread;
static rt_tick_t ticks;
static unsigned irq, pm, initializes, ons, offs, events_detached, threads_deleted;
static bool enabled, fail_init, fail_on, fail_off, fail_event, fail_create, fail_start, fail_send;
static bool stop_on_hold, expire_on_hold, stop_on_enable, stop_on_disable, replace_on_enable;
static bool slow_enable, zero_ticks;
static int32_t recorded_wait;
static jmp_buf worker_exit;
rt_base_t rt_hw_interrupt_disable(void) { return irq++; }
void rt_hw_interrupt_enable(rt_base_t previous) { assert(irq); irq = (unsigned)previous; }
rt_tick_t rt_tick_get(void) { return ticks; }
rt_tick_t rt_tick_from_millisecond(unsigned ms) { return zero_ticks ? 0 : ms; }
void rt_kprintf(const char *format, ...) { (void)format; }
void rt_pm_request(unsigned mode)
{
    assert(mode == PM_SLEEP_MODE_IDLE && !irq && !pm); ++pm;
    if (stop_on_hold) { stop_on_hold = false; assert(wristflow_product_motor_stop()); }
    if (expire_on_hold) { expire_on_hold = false; ticks = deadline; }
}
void rt_pm_release(unsigned mode) { assert(mode == PM_SLEEP_MODE_IDLE && !irq && pm == 1); --pm; }
static bool initialize_off(void *context)
{
    assert(context == &ticks && !irq); ++initializes;
    if (fail_init) return false;
    enabled = false; return true;
}
static bool set_enabled(void *context, bool on)
{
    assert(context == &ticks && !irq);
    if (ready) assert(pm == 1 && state.busy);
    else assert(!on); /* Startup cleanup never requests ON. */
    if (on) {
        assert(!state.off_latched); ++ons;
        enabled = true; /* Exercise a partial ON even when the callback fails. */
        if (replace_on_enable) {
            replace_on_enable = false;
            assert(!wristflow_product_motor_pulse(250));
        }
        if (stop_on_enable) { stop_on_enable = false; assert(wristflow_product_motor_stop()); }
        if (slow_enable) { slow_enable = false; ticks = deadline + 1; }
        return !fail_on;
    }
    ++offs;
    if (stop_on_disable) { stop_on_disable = false; assert(wristflow_product_motor_stop()); }
    if (fail_off) return false;
    enabled = false; return true;
}
int rt_event_init(struct rt_event *event, const char *name, unsigned flags)
{
    assert(!irq && !strcmp(name, "wf_mtr") && flags == RT_IPC_FLAG_PRIO);
    event->bits = 0; return fail_event ? -1 : 0;
}
int rt_event_detach(struct rt_event *event) { assert(event == &changed && !irq); ++events_detached; return 0; }
int rt_event_send(struct rt_event *event, unsigned bits)
{ assert(event == &changed && irq); if (fail_send) return -1; event->bits |= bits; return 0; }
int rt_event_recv(struct rt_event *event, unsigned mask, unsigned flags, int wait, rt_uint32_t *bits)
{
    assert(event == &changed && mask == 1 && flags == (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR) && !irq);
    *bits = event->bits & mask; event->bits &= ~mask; recorded_wait = wait;
    longjmp(worker_exit, 1);
}
rt_thread_t rt_thread_create(const char *name, void (*entry)(void *), void *context,
                            unsigned stack, unsigned priority, unsigned slice)
{
    assert(!irq && !strcmp(name, "wf_motor") && stack == 2048 && priority == 18 && slice == 10);
    fake_thread = (struct rt_thread){entry, context}; return fail_create ? NULL : &fake_thread;
}
int rt_thread_startup(rt_thread_t thread) { assert(thread == &fake_thread && !irq); return fail_start ? -1 : 0; }
int rt_thread_delete(rt_thread_t thread) { assert(thread == &fake_thread && !irq); ++threads_deleted; return 0; }
static void background(void) { changed.bits = 0; step(); assert(!irq); }
static int command(const char *action, const char *value)
{ char *args[] = {"wf_motor", (char *)action, (char *)value}; return wf_motor(value ? 3 : 2, args); }
static void check_off(void)
{
    wristflow_motor_snapshot_t s = wristflow_product_motor_snapshot();
    assert(s.state == WRISTFLOW_MOTOR_OFF && s.off_latched && !s.busy && !s.stop_pending && !enabled && !pm);
    assert(wait_ticks() == RT_WAITING_FOREVER);
}
static void fresh(void)
{
    assert(!pm && !irq);
    ready = idle_held = false;
    state = (wristflow_motor_snapshot_t){0};
    ticks = initializes = ons = offs = events_detached = threads_deleted = 0;
    enabled = fail_init = fail_on = fail_off = fail_event = fail_create = fail_start = fail_send = false;
    stop_on_hold = expire_on_hold = stop_on_enable = stop_on_disable = replace_on_enable = slow_enable = zero_ticks = false;
    changed.bits = 0;
}
static const wristflow_motor_io_t backend = {&ticks, initialize_off, set_enabled};
int main(void)
{
    assert(!wristflow_product_motor_start(NULL));
    wristflow_motor_io_t missing = {0}; assert(!wristflow_product_motor_start(&missing));
    assert(!wristflow_product_motor_pulse(100) && !wristflow_product_motor_stop() && !ons);
    fresh(); fail_init = true;
    assert(!wristflow_product_motor_start(&backend) && initializes == 1 && offs == 1 && !ons);
    assert(wristflow_product_motor_snapshot().state == WRISTFLOW_MOTOR_UNAVAILABLE);
    fresh(); fail_event = true; assert(!wristflow_product_motor_start(&backend) && offs == 1 && !ons);
    fresh(); fail_create = true;
    assert(!wristflow_product_motor_start(&backend) && events_detached == 1 && offs == 1 && !ons);
    fresh(); fail_start = true;
    assert(!wristflow_product_motor_start(&backend) && threads_deleted == 1 && events_detached == 1 && offs == 1 && !ons);
    for (unsigned failure = 0; failure < 4; ++failure) {
        fresh();
        fail_init = failure == 0; fail_event = failure == 1;
        fail_create = failure == 2; fail_start = failure == 3; fail_off = true;
        assert(!wristflow_product_motor_start(&backend) && !ons && pm == 1);
        assert(state.state == WRISTFLOW_MOTOR_UNAVAILABLE && !state.off_latched);
        assert(!wristflow_product_motor_pulse(10) && !wristflow_product_motor_stop());
        fail_init = fail_event = fail_create = fail_start = fail_off = false;
        assert(wristflow_product_motor_start(&backend)); check_off();
    }
    fresh(); assert(wristflow_product_motor_start(&backend)); check_off();
    unsigned calls = initializes + offs + ons;
    if (!setjmp(worker_exit)) fake_thread.entry(fake_thread.context);
    assert(recorded_wait == RT_WAITING_FOREVER && initializes + offs + ons == calls);
    ticks += 3600000; background(); check_off(); assert(initializes + offs + ons == calls);
    assert(!wristflow_product_motor_pulse(0) && !wristflow_product_motor_pulse(251));
    const char *bad[] = {"", "0", "251", "-1", "+1", "1x", "999999999999999999999"};
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; ++i) assert(command("pulse", bad[i]) == -RT_EINVAL);
    assert(!ons && command("status", NULL) == RT_EOK);

    fail_send = true; assert(!wristflow_product_motor_pulse(100)); fail_send = false;
    background(); check_off(); assert(!ons);
    uint32_t id = wristflow_product_motor_pulse(100); assert(id && wait_ticks() == 0);
    rt_tick_t limit = deadline;
    assert(!wristflow_product_motor_pulse(250) && deadline == limit);
    replace_on_enable = true; background();
    assert(enabled && pm == 1 && state.state == WRISTFLOW_MOTOR_ACTIVE && deadline == limit && wait_ticks() == 100);
    assert(wristflow_product_motor_start(&backend) && initializes == 1 && enabled);
    assert(!wristflow_product_motor_pulse(250) && deadline == limit);
    ticks = limit - 1; background(); assert(enabled && wait_ticks() == 1);
    ticks = limit; background(); check_off(); assert(state.reason == WRISTFLOW_MOTOR_EXPIRED);

    calls = ons; assert(wristflow_product_motor_pulse(10)); ticks = deadline;
    background(); check_off(); assert(ons == calls); /* Already expired before dispatch. */
    assert(wristflow_product_motor_pulse(10)); expire_on_hold = true;
    background(); check_off(); assert(ons == calls); /* Expires while acquiring PM. */
    assert(wristflow_product_motor_pulse(10)); assert(wristflow_product_motor_stop());
    background(); check_off(); assert(ons == calls);
    assert(wristflow_product_motor_pulse(10)); stop_on_hold = true;
    background(); check_off(); assert(ons == calls);

    assert(wristflow_product_motor_pulse(10)); stop_on_enable = true;
    background(); check_off(); assert(state.reason == WRISTFLOW_MOTOR_CANCELLED);
    assert(wristflow_product_motor_pulse(10)); slow_enable = true;
    background(); check_off(); assert(state.reason == WRISTFLOW_MOTOR_EXPIRED);
    fail_on = true; assert(wristflow_product_motor_pulse(10)); background();
    check_off(); assert(state.reason == WRISTFLOW_MOTOR_ON_ERROR); fail_on = false;

    assert(wristflow_product_motor_pulse(10)); background();
    fail_off = true; assert(wristflow_product_motor_stop()); background();
    assert(enabled && pm == 1 && state.state == WRISTFLOW_MOTOR_FAULT && !state.off_latched);
    assert(wait_ticks() == RT_WAITING_FOREVER && !wristflow_product_motor_pulse(10));
    calls = offs; ticks += 1000; background(); assert(offs == calls && pm == 1);
    fail_off = false; assert(wristflow_product_motor_stop()); background(); check_off();

    assert(wristflow_product_motor_pulse(50)); background();
    assert(wristflow_product_motor_stop()); stop_on_disable = true;
    background(); assert(!enabled && !pm && wristflow_product_motor_snapshot().stop_pending);
    assert(wait_ticks() == 0 && !wristflow_product_motor_pulse(50));
    background(); check_off();
    assert(wristflow_product_motor_pulse(50)); background(); limit = deadline;
    fail_send = true; assert(!wristflow_product_motor_stop() && deadline == limit); fail_send = false;
    ticks = limit; background(); check_off();
    fail_send = true; assert(!wristflow_product_motor_stop()); fail_send = false;
    assert(!wristflow_product_motor_pulse(50)); assert(wristflow_product_motor_stop()); background(); check_off();

    state.request_id = UINT32_MAX; stop_sequence = handled_stop = UINT32_MAX;
    ticks = UINT32_MAX - 10; assert(wristflow_product_motor_pulse(20) == 1);
    background(); assert(wait_ticks() == 20); ticks = deadline - 1;
    assert(wristflow_product_motor_snapshot().remaining_ms == 1);
    ticks = deadline; background(); check_off();
    assert(wristflow_product_motor_stop() && stop_sequence == 1); background(); check_off();
    zero_ticks = true; assert(wristflow_product_motor_pulse(1)); background();
    assert(wait_ticks() == 1); ticks = deadline; background(); check_off(); zero_ticks = false;
    assert(command("pulse", "250") == RT_EOK); background();
    assert(command("stop", NULL) == RT_EOK); background(); check_off();
    puts("product motor deadlines, cancellation, failures, PM and idle wait passed");
    return 0;
}
