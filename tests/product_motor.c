#define RT_USING_PM
#include "../apps/product/src/product_motor.c"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>

static struct rt_thread fake_thread;
static rt_tick_t ticks;
static unsigned irq, pm, initializes, prepares, ons, offs, events_detached, threads_deleted;
static bool enabled, fail_init, fail_on, fail_off, fail_event, fail_create, fail_start, fail_send;
static bool stop_on_hold, expire_on_hold, stop_on_enable, stop_on_disable, replace_on_enable;
static bool slow_enable, zero_ticks;
static bool supply, fail_prepare, stop_on_prepare, expire_on_prepare, replace_on_prepare;
static unsigned prepare_delay, off_delay;
static bool cancel_preview_on_prepare;
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
        assert(supply);
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
    ticks += off_delay; off_delay = 0;
    enabled = supply = false; return true;
}
static bool prepare(void *context)
{
    assert(context == &ticks && !irq && pm == 1 && state.busy && !enabled && !state.off_latched);
    ++prepares; supply = true; /* Include partial preparation on failure. */
    ticks += prepare_delay; prepare_delay = 0;
    if (cancel_preview_on_prepare) { cancel_preview_on_prepare = false; assert(wristflow_product_motor_cancel_preview(state.request_id)); }
    if (stop_on_prepare) { stop_on_prepare = false; assert(wristflow_product_motor_stop()); }
    if (expire_on_prepare) { expire_on_prepare = false; ticks = deadline; }
    if (replace_on_prepare) { replace_on_prepare = false; assert(!wristflow_product_motor_pulse(250)); }
    return !fail_prepare;
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
    assert(s.state == WRISTFLOW_MOTOR_OFF && s.off_latched && !s.busy && !s.stop_pending && !enabled && !supply && !pm);
    assert(wait_ticks() == RT_WAITING_FOREVER);
}
static void fresh(void)
{
    assert(!pm && !irq);
    ready = idle_held = false;
    state = (wristflow_motor_snapshot_t){0};
    ticks = initializes = prepares = ons = offs = events_detached = threads_deleted = 0;
    enabled = fail_init = fail_on = fail_off = fail_event = fail_create = fail_start = fail_send = false;
    stop_on_hold = expire_on_hold = stop_on_enable = stop_on_disable = replace_on_enable = slow_enable = zero_ticks = false;
    supply = fail_prepare = stop_on_prepare = expire_on_prepare = replace_on_prepare = false;
    prepare_delay = off_delay = 0;
    cancel_preview_on_prepare = false;
    changed.bits = 0;
}
static const wristflow_motor_io_t backend = {&ticks, initialize_off, set_enabled, prepare};

static void preview_tests(void)
{
    unsigned before = ons;
    uint32_t id = wristflow_product_motor_preview(false);
    assert(id && state.preview && state.segments == 1 && state.requested_ms == 100);
    background(); assert(enabled); ticks = deadline; background(); check_off(); assert(ons == before+1);

    ticks = 5000; before = ons; id = wristflow_product_motor_preview(true);
    assert(id && group_deadline == 5350 && deadline == 5100);
    background(); ticks = deadline; background();
    assert(state.state == WRISTFLOW_MOTOR_GAP && state.off_latched && !enabled && !supply && pm == 1);
    assert(wait_ticks() == 150 && gap_until == 5250);
    assert(!wristflow_product_motor_preview(false) && !wristflow_product_motor_pulse(100));
    ticks = 5249; background(); assert(ons == before+1);
    ticks = 5250; background(); assert(enabled && state.segment == 2 && deadline == 5350);
    ticks = deadline; background(); check_off(); assert(ons == before+2);

    /* Actual successful LOW starts the minimum gap, without moving the group end. */
    ticks = 6000; id = wristflow_product_motor_preview(true); background();
    ticks = deadline; off_delay = 15; background();
    assert(gap_until == 6265 && group_deadline == 6350 && wait_ticks() == 150);
    ticks = gap_until; background(); assert(enabled && wait_ticks() == 85);
    ticks = deadline; background(); check_off();

    /* Missing a window never replays or grants another full100ms. */
    before = ons; id = wristflow_product_motor_preview(true); ticks = deadline;
    background(); check_off(); assert(ons == before);
    id = wristflow_product_motor_preview(true); prepare_delay = 100;
    background(); check_off(); assert(ons == before);
    id = wristflow_product_motor_preview(true); background();
    ticks = group_deadline - 50; background(); check_off(); assert(ons == before+1);
    before = ons; id = wristflow_product_motor_preview(true); background(); ticks = deadline; background();
    ticks = group_deadline; background(); check_off(); assert(ons == before+1);
    before = ons; id = wristflow_product_motor_preview(true); background(); ticks = deadline; background();
    ticks = gap_until; prepare_delay = 100; background(); check_off(); assert(ons == before+1);

    /* Matched cancellation covers QUEUED, ACTIVE, GAP and second preparation. */
    for (unsigned phase = 0; phase < 4; ++phase) {
        before = ons; id = wristflow_product_motor_preview(true);
        if (phase) background();
        if (phase >= 2) { ticks = deadline; background(); }
        if (phase == 3) { ticks = gap_until; cancel_preview_on_prepare = true; }
        else assert(wristflow_product_motor_cancel_preview(id));
        background(); check_off(); assert(ons == before + (phase != 0));
        ticks += 1000; background(); assert(ons == before + (phase != 0));
    }
    uint32_t old = id;
    id = wristflow_product_motor_preview(true);
    assert(id != old && !wristflow_product_motor_cancel_preview(old));
    assert(wristflow_product_motor_cancel_preview(id)); background(); check_off();
    id = wristflow_product_motor_pulse(100); background();
    assert(!wristflow_product_motor_cancel_preview(id) && !wristflow_product_motor_cancel_preview(old));
    assert(enabled && !state.stop_pending); ticks = deadline; background(); check_off();

    /* Any failed segment cleanup discards the remaining sequence and retains IDLE. */
    for (unsigned segment = 1; segment <= 2; ++segment) {
        before = ons; id = wristflow_product_motor_preview(true); background();
        if (segment == 2) { ticks = deadline; background(); ticks = gap_until; background(); }
        fail_off = true; ticks = deadline; background();
        assert(state.state == WRISTFLOW_MOTOR_FAULT && pm == 1 && !state.off_latched);
        ticks += 1000; background(); assert(ons == before+segment);
        assert(!wristflow_product_motor_preview(true));
        fail_off = false; assert(wristflow_product_motor_stop()); background(); check_off();
    }
    for (unsigned failure = 0; failure < 2; ++failure) {
        id = wristflow_product_motor_preview(true); background(); ticks = deadline; background();
        ticks = gap_until; fail_prepare = failure == 0; fail_on = failure == 1;
        background(); check_off();
        assert(state.reason == (failure ? WRISTFLOW_MOTOR_ON_ERROR : WRISTFLOW_MOTOR_PREPARE_ERROR));
        fail_prepare = fail_on = false;
    }
    id = wristflow_product_motor_preview(true); background(); ticks = deadline; stop_on_disable = true;
    before = ons; background(); assert(state.state == WRISTFLOW_MOTOR_OFF && stop_sequence != handled_stop);
    background(); check_off(); assert(ons == before);

    fail_send = true; assert(!wristflow_product_motor_preview(true)); fail_send = false;
    background(); check_off();
    id = wristflow_product_motor_preview(true); background(); ticks = deadline; background();
    rt_tick_t end = group_deadline; fail_send = true;
    assert(!wristflow_product_motor_cancel_preview(id) && group_deadline == end); fail_send = false;
    ticks = gap_until; background(); check_off();
    state.request_id = UINT32_MAX; ticks = UINT32_MAX-50;
    assert(wristflow_product_motor_preview(true) == 1); background();
    ticks = deadline; background(); ticks = gap_until; background();
    assert(enabled && state.segment == 2); ticks = deadline; background(); check_off();
    assert(command("pulse", "350") == -RT_EINVAL);
}
int main(void)
{
    assert(!wristflow_product_motor_start(NULL));
    wristflow_motor_io_t missing = {0}; assert(!wristflow_product_motor_start(&missing));
    missing = backend; missing.prepare = NULL; assert(!wristflow_product_motor_start(&missing));
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

    assert(wristflow_product_motor_pulse(20)); stop_on_prepare = true;
    background(); check_off(); assert(ons == calls && state.reason == WRISTFLOW_MOTOR_CANCELLED);
    assert(wristflow_product_motor_pulse(20)); expire_on_prepare = true;
    background(); check_off(); assert(ons == calls && state.reason == WRISTFLOW_MOTOR_EXPIRED);
    fail_prepare = true; assert(wristflow_product_motor_pulse(20)); background();
    check_off(); assert(ons == calls && state.reason == WRISTFLOW_MOTOR_PREPARE_ERROR);
    fail_prepare = false;
    assert(wristflow_product_motor_pulse(20)); replace_on_prepare = true;
    limit = deadline; background();
    assert(enabled && supply && deadline == limit && state.state == WRISTFLOW_MOTOR_ACTIVE);
    assert(wristflow_product_motor_stop()); background(); check_off();
    fail_prepare = fail_off = true; calls = ons;
    assert(wristflow_product_motor_pulse(20)); background();
    assert(!enabled && supply && pm == 1 && state.state == WRISTFLOW_MOTOR_FAULT && !state.off_latched && ons == calls);
    assert(!wristflow_product_motor_pulse(20));
    fail_prepare = fail_off = false;
    assert(wristflow_product_motor_stop()); background(); check_off();

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
    preview_tests();
    puts("product motor preparation, deadlines, cancellation, failures, PM and idle wait passed");
    return 0;
}
