#include "product_motor.h"
#include <rtthread.h>
#include <rthw.h>
#include <rtdevice.h>
#include <string.h>

static wristflow_motor_io_t io;
static struct rt_event changed;
static wristflow_motor_snapshot_t state;
static rt_tick_t deadline;
static uint32_t stop_sequence, handled_stop;
static bool ready, idle_held;

static int32_t remaining(rt_tick_t now) { return (int32_t)(deadline - now); }
static uint32_t next_id(uint32_t id) { return ++id ? id : 1; }
static void hold(void)
{
#ifdef RT_USING_PM
    if (!idle_held) rt_pm_request(PM_SLEEP_MODE_IDLE);
#endif
    idle_held = true;
}
static void release(void)
{
#ifdef RT_USING_PM
    if (idle_held) rt_pm_release(PM_SLEEP_MODE_IDLE);
#endif
    idle_held = false;
}

static void turn_off(wristflow_motor_reason_t cause)
{
    rt_base_t level = rt_hw_interrupt_disable();
    uint32_t sequence = stop_sequence;
    state.state = WRISTFLOW_MOTOR_STOPPING;
    state.busy = true;
    rt_hw_interrupt_enable(level);
    hold();
    bool ok = io.set_enabled(io.context, false);
    if (ok) release();
    level = rt_hw_interrupt_disable();
    /* A stop arriving inside OFF I/O remains pending for another worker pass. */
    handled_stop = sequence;
    state.off_latched = ok;
    state.state = ok ? WRISTFLOW_MOTOR_OFF : WRISTFLOW_MOTOR_FAULT;
    state.reason = ok ? cause : WRISTFLOW_MOTOR_OFF_ERROR;
    state.busy = false;
    uint32_t id = state.request_id;
    wristflow_motor_reason_t reason = state.reason;
    rt_hw_interrupt_enable(level);
    rt_kprintf("[motor] request=%lu off_latch=%u reason=%u\n", (unsigned long)id, ok, (unsigned)reason);
}

static void step(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    bool stop = stop_sequence != handled_stop;
    if (stop || (state.state == WRISTFLOW_MOTOR_ACTIVE && remaining(rt_tick_get()) <= 0)) {
        state.busy = true;
        rt_hw_interrupt_enable(level);
        turn_off(stop ? WRISTFLOW_MOTOR_CANCELLED : WRISTFLOW_MOTOR_EXPIRED);
        return;
    }
    if (state.state != WRISTFLOW_MOTOR_QUEUED) { rt_hw_interrupt_enable(level); return; }
    uint32_t sequence = stop_sequence;
    state.busy = true;
    rt_hw_interrupt_enable(level);
    hold();
    level = rt_hw_interrupt_disable();
    bool cancelled = sequence != stop_sequence;
    bool expired = remaining(rt_tick_get()) <= 0;
    if (!cancelled && !expired) state.off_latched = false;
    rt_hw_interrupt_enable(level);
    if (cancelled || expired) {
        turn_off(cancelled ? WRISTFLOW_MOTOR_CANCELLED : WRISTFLOW_MOTOR_EXPIRED);
        return;
    }
    bool ok = io.prepare(io.context);
    level = rt_hw_interrupt_disable();
    cancelled = sequence != stop_sequence;
    expired = remaining(rt_tick_get()) <= 0;
    rt_hw_interrupt_enable(level);
    if (!ok || cancelled || expired) {
        turn_off(!ok ? WRISTFLOW_MOTOR_PREPARE_ERROR : cancelled ? WRISTFLOW_MOTOR_CANCELLED : WRISTFLOW_MOTOR_EXPIRED);
        return;
    }
    ok = io.set_enabled(io.context, true);
    level = rt_hw_interrupt_disable();
    cancelled = sequence != stop_sequence;
    expired = remaining(rt_tick_get()) <= 0;
    if (ok && !cancelled && !expired) {
        state.state = WRISTFLOW_MOTOR_ACTIVE;
        state.reason = WRISTFLOW_MOTOR_NONE;
        state.busy = false;
    }
    rt_hw_interrupt_enable(level);
    if (!ok || cancelled || expired)
        turn_off(!ok ? WRISTFLOW_MOTOR_ON_ERROR : cancelled ? WRISTFLOW_MOTOR_CANCELLED : WRISTFLOW_MOTOR_EXPIRED);
}

static int32_t wait_ticks(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    int32_t wait = RT_WAITING_FOREVER;
    if (stop_sequence != handled_stop || state.state == WRISTFLOW_MOTOR_QUEUED) wait = 0;
    else if (state.state == WRISTFLOW_MOTOR_ACTIVE) {
        wait = remaining(rt_tick_get());
        if (wait < 0) wait = 0;
    }
    rt_hw_interrupt_enable(level);
    return wait;
}
static void worker(void *context)
{
    (void)context;
    for (;;) {
        step();
        rt_uint32_t bits;
        rt_event_recv(&changed, 1, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, wait_ticks(), &bits);
    }
}

uint32_t wristflow_product_motor_pulse(uint32_t milliseconds)
{
    if (!milliseconds || milliseconds > WRISTFLOW_MOTOR_MAX_MS) return 0;
    rt_tick_t ticks = rt_tick_from_millisecond(milliseconds);
    if (!ticks) ticks = 1;
    rt_base_t level = rt_hw_interrupt_disable();
    if (!ready || state.state != WRISTFLOW_MOTOR_OFF || state.busy || stop_sequence != handled_stop) {
        rt_hw_interrupt_enable(level); return 0;
    }
    state.request_id = next_id(state.request_id);
    state.requested_ms = milliseconds;
    deadline = rt_tick_get() + ticks;
    state.state = WRISTFLOW_MOTOR_QUEUED;
    state.reason = WRISTFLOW_MOTOR_NONE;
    uint32_t id = state.request_id;
    if (rt_event_send(&changed, 1) != RT_EOK) {
        state.state = WRISTFLOW_MOTOR_OFF;
        state.reason = WRISTFLOW_MOTOR_WAKE_ERROR;
        id = 0;
    }
    rt_hw_interrupt_enable(level);
    return id;
}
bool wristflow_product_motor_stop(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    if (!ready) { rt_hw_interrupt_enable(level); return false; }
    stop_sequence = next_id(stop_sequence);
    /* Keep cancellation even if waking fails; active deadline remains unchanged. */
    bool ok = rt_event_send(&changed, 1) == RT_EOK;
    rt_hw_interrupt_enable(level);
    return ok;
}
wristflow_motor_snapshot_t wristflow_product_motor_snapshot(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    wristflow_motor_snapshot_t result = state;
    int32_t ticks = remaining(rt_tick_get());
    result.remaining_ms = (state.state == WRISTFLOW_MOTOR_QUEUED || state.state == WRISTFLOW_MOTOR_ACTIVE) && ticks > 0
        ? (uint32_t)((uint64_t)(uint32_t)ticks * 1000 / RT_TICK_PER_SECOND) : 0;
    result.stop_pending = stop_sequence != handled_stop;
    rt_hw_interrupt_enable(level);
    return result;
}
static bool unavailable(wristflow_motor_reason_t reason)
{
    bool off = io.set_enabled(io.context, false);
    if (off) release();
    else hold();
    rt_base_t level = rt_hw_interrupt_disable();
    state.off_latched = off;
    state.state = WRISTFLOW_MOTOR_UNAVAILABLE;
    state.reason = reason;
    rt_hw_interrupt_enable(level);
    return false;
}
bool wristflow_product_motor_start(const wristflow_motor_io_t *backend)
{
    if (ready) return true;
    if (!backend || !backend->initialize_off || !backend->set_enabled || !backend->prepare) return false;
    io = *backend;
    rt_base_t level = rt_hw_interrupt_disable();
    state = (wristflow_motor_snapshot_t){.state = WRISTFLOW_MOTOR_UNAVAILABLE};
    stop_sequence = handled_stop = 0;
    rt_hw_interrupt_enable(level);
    if (!io.initialize_off(io.context)) return unavailable(WRISTFLOW_MOTOR_INIT_ERROR);
    release();
    level = rt_hw_interrupt_disable();
    state.off_latched = true;
    rt_hw_interrupt_enable(level);
    if (rt_event_init(&changed, "wf_mtr", RT_IPC_FLAG_PRIO) != RT_EOK)
        return unavailable(WRISTFLOW_MOTOR_RESOURCE_ERROR);
    rt_thread_t thread = rt_thread_create("wf_motor", worker, NULL, 2048, 18, 10);
    if (!thread) {
        rt_event_detach(&changed);
        return unavailable(WRISTFLOW_MOTOR_RESOURCE_ERROR);
    }
    level = rt_hw_interrupt_disable();
    state.state = WRISTFLOW_MOTOR_OFF;
    rt_hw_interrupt_enable(level);
    if (rt_thread_startup(thread) != RT_EOK) {
        rt_thread_delete(thread);
        rt_event_detach(&changed);
        return unavailable(WRISTFLOW_MOTOR_RESOURCE_ERROR);
    }
    level = rt_hw_interrupt_disable();
    ready = true;
    rt_hw_interrupt_enable(level);
    return true;
}

static int wf_motor(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "status")) {
        static const char *names[] = {"unavailable", "off", "queued", "active", "stopping", "fault"};
        wristflow_motor_snapshot_t s = wristflow_product_motor_snapshot();
        rt_kprintf("[motor] state=%s reason=%u request=%lu busy=%u stop_pending=%u\n",
            names[s.state], (unsigned)s.reason, (unsigned long)s.request_id, s.busy, s.stop_pending);
        rt_kprintf("[motor] requested=%lums remaining=%lums off_latch=%u; LOW/supply checks, not measured current\n",
            (unsigned long)s.requested_ms, (unsigned long)s.remaining_ms, s.off_latched);
        return RT_EOK;
    }
    if (argc == 2 && !strcmp(argv[1], "stop")) return wristflow_product_motor_stop() ? RT_EOK : -RT_ERROR;
    if (argc == 3 && !strcmp(argv[1], "pulse")) {
        uint32_t value = 0;
        if (!argv[2][0]) return -RT_EINVAL;
        for (const char *p = argv[2]; *p; ++p) {
            if (*p < '0' || *p > '9' || value > WRISTFLOW_MOTOR_MAX_MS) return -RT_EINVAL;
            value = value * 10 + (unsigned)(*p - '0');
            if (value > WRISTFLOW_MOTOR_MAX_MS) return -RT_EINVAL;
        }
        if (!value) return -RT_EINVAL;
        return wristflow_product_motor_pulse(value) ? RT_EOK : -RT_ERROR;
    }
    rt_kprintf("wf_motor status | pulse <1..250 ms> | stop; manual diagnostic only\n");
    return -RT_EINVAL;
}
MSH_CMD_EXPORT(wf_motor, Bounded manual motor pulse and digital latch status);
