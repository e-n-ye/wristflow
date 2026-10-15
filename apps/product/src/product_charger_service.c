#include "product_charger.h"
#include "aw32001_manual.h"
#include "product_power.h"
#include <rtthread.h>
#include <rthw.h>
#include <rtdevice.h>
#include <string.h>

#ifndef RT_USING_PM
#error "Manual charge sessions require the Product PM constraint"
#endif
_Static_assert(RT_TICK_PER_SECOND == 1000, "manual charge clock requires millisecond ticks");

static struct rt_event commands;
static wristflow_charge_session_t session, cached;
static bool ready, busy, start_pending, stop_pending, status_pending, idle_held;
static unsigned start_seconds;
static uint32_t accepted_ms;

static uint32_t clock_ms(void *context) { (void)context; return rt_tick_get(); }
static bool cancelled(void *context)
{
    (void)context;
    rt_base_t level = rt_hw_interrupt_disable();
    bool value = stop_pending;
    rt_hw_interrupt_enable(level);
    return value;
}

/* The fixed RT mutex is recursive. Acquire it with a bounded wait before the
 * registered API's recursive FOREVER take. Preserve the SDK transfer timeout:
 * one HAL operation can still take ~5s (plus BUSY/recovery), not a hard cutoff. */
static bool transfer(bool write, uint8_t reg, uint8_t *value)
{
    struct rt_i2c_bus_device *bus = rt_i2c_bus_device_find("i2c2");
    if (!bus || rt_mutex_take(&bus->lock, rt_tick_from_millisecond(50)) != RT_EOK) return false;
    /* A stop/deadline can arrive while acquiring the bus. Check again before
     * dispatching enable; inhibition must always remain allowed. */
    if (write && reg == 1 && !(*value & 8) &&
        (cancelled(RT_NULL) || (int32_t)(clock_ms(RT_NULL) - session.deadline_ms) >= 0)) {
        rt_mutex_release(&bus->lock);
        return false;
    }
    bool ok = (write ? rt_i2c_mem_write(bus, 0x49, reg, 8, value, 1) :
                       rt_i2c_mem_read(bus, 0x49, reg, 8, value, 1)) == 1;
    rt_mutex_release(&bus->lock);
    return ok;
}
static bool read_reg(void *context, uint8_t reg, uint8_t *value)
{ (void)context; return transfer(false, reg, value); }
static bool write_reg(void *context, uint8_t reg, uint8_t value)
{ (void)context; return transfer(true, reg, &value); }
static bool voltage(void *context, uint16_t *mv)
{
    (void)context;
    wristflow_power_snapshot_t value = wristflow_product_power_voltage();
    *mv = value.millivolts;
    return value.voltage_valid;
}
static const wristflow_charge_port_t port = {
    { RT_NULL, read_reg, write_reg }, clock_ms, cancelled, voltage
};

static void hold(void)
{
    if (!idle_held) { rt_pm_request(PM_SLEEP_MODE_IDLE); idle_held = true; }
}
static void publish(void)
{
    bool retain = session.state == WF_CHARGE_ENABLED || session.state == WF_CHARGE_UNCONFIRMED;
    if (!retain && idle_held) { rt_pm_release(PM_SLEEP_MODE_IDLE); idle_held = false; }
    rt_base_t level = rt_hw_interrupt_disable();
    cached = session;
    busy = retain || start_pending || stop_pending;
    rt_hw_interrupt_enable(level);
}

static void report(void)
{
    static const char *states[] = { "idle", "enabled", "inhibited", "UNCONFIRMED" };
    static const char *reasons[] = { "none", "manual", "timeout", "chip_complete",
        "io", "profile", "input", "fault", "voltage", "regulation" };
    rt_base_t level = rt_hw_interrupt_disable();
    wristflow_charge_session_t value = cached;
    bool pending = busy;
    rt_hw_interrupt_enable(level);
    rt_kprintf("[charger] state=%s reason=%s busy=%u sampled=%u at_ms=%lu deadline_ms=%lu\n",
        states[value.state], reasons[value.reason], pending, value.sample_valid,
        (unsigned long)value.observed_ms, (unsigned long)value.deadline_ms);
    rt_kprintf("[charger] REG01=%02x REG05=%02x REG02=%02x REG04=%02x REG0b=%02x REG08=%02x REG09=%02x VBAT=%umV\n",
        value.power, value.timer, value.current, value.voltage, value.individual,
        value.status, value.fault, value.millivolts);
    rt_kprintf("[charger] cause=%u/REG%02x stop=%u/REG%02x; hold_required=%u\n",
        value.cause_error, value.cause_reg, value.stop_error, value.stop_reg,
        value.state == WF_CHARGE_ENABLED || value.state == WF_CHARGE_UNCONFIRMED);
    rt_kprintf("[charger] enabled means CEB readback, not measured current; no cell temperature or SOC\n");
}

static void raw_snapshot(void)
{
    uint8_t values[13];
    /* Only while inactive. Active status uses the monitoring sample, so it
     * cannot add a second fault-register reader or delay cutoff by a full dump. */
    for (unsigned reg = 0; reg < sizeof values; ++reg) {
        if (cancelled(RT_NULL)) return;
        if (!read_reg(RT_NULL, reg, &values[reg])) {
            rt_kprintf("[charger] raw REG%02x read failed; snapshot incomplete\n", reg);
            return;
        }
    }
    for (unsigned reg = 0; reg < sizeof values; ++reg)
        rt_kprintf("[charger] REG%02x=0x%02x\n", reg, values[reg]);
    rt_kprintf("[charger] sequential raw snapshot; REG09 may clear exited fault latches\n");
}

static void step(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    bool stop = stop_pending, start = start_pending, status = status_pending;
    unsigned seconds = start_seconds;
    uint32_t accepted = accepted_ms;
    start_pending = status_pending = false;
    /* Stop requests coalesce; no queued start survives a stop. */
    if (stop) stop_pending = false;
    rt_hw_interrupt_enable(level);
    if (stop) {
        hold();
        wristflow_charge_stop(&session, &port, WF_CHARGE_MANUAL);
    } else if (start) {
        hold();
        wristflow_charge_begin(&session, &port, seconds, accepted);
        if (session.reason == WF_CHARGE_MANUAL) {
            level = rt_hw_interrupt_disable();
            stop_pending = false; /* This coalesced request already attempted inhibition. */
            rt_hw_interrupt_enable(level);
        }
    } else if (session.state == WF_CHARGE_ENABLED) {
        wristflow_charge_poll(&session, &port);
        if (session.reason == WF_CHARGE_MANUAL) {
            level = rt_hw_interrupt_disable();
            stop_pending = false;
            rt_hw_interrupt_enable(level);
        }
    }
    if (status && session.state != WF_CHARGE_ENABLED && !stop && !start) {
        hold();
        raw_snapshot();
    }
    publish();
    if (stop || start || status || session.state != WF_CHARGE_ENABLED) report();
}

static void worker(void *context)
{
    (void)context;
    for (;;) {
        rt_uint32_t bits;
        rt_int32_t wait = RT_WAITING_FOREVER;
        if (session.state == WF_CHARGE_ENABLED) wait = (rt_int32_t)rt_tick_from_millisecond(1000);
        rt_event_recv(&commands, 1, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
            wait, &bits);
        step();
    }
}

static bool request_start(unsigned seconds)
{
    if (!ready || !seconds || seconds > WF_CHARGE_MAX_SECONDS) return false;
    rt_base_t level = rt_hw_interrupt_disable();
    if (busy) { rt_hw_interrupt_enable(level); return false; }
    busy = start_pending = true;
    start_seconds = seconds;
    accepted_ms = clock_ms(RT_NULL);
    rt_hw_interrupt_enable(level);
    if (rt_event_send(&commands, 1) == RT_EOK) return true;
    level = rt_hw_interrupt_disable();
    start_pending = false;
    busy = stop_pending;
    rt_hw_interrupt_enable(level);
    return false;
}
static bool request_stop(void)
{
    if (!ready) return false;
    rt_base_t level = rt_hw_interrupt_disable();
    stop_pending = busy = true;
    start_pending = false;
    rt_hw_interrupt_enable(level);
    return rt_event_send(&commands, 1) == RT_EOK;
}

static int wf_charger(int argc, char **argv)
{
    if (!ready) { rt_kprintf("[charger] service unavailable; no charge command accepted\n"); return -RT_ERROR; }
    if (argc == 2 && !strcmp(argv[1], "status")) {
        report();
        rt_base_t level = rt_hw_interrupt_disable();
        status_pending = true;
        rt_hw_interrupt_enable(level);
        return rt_event_send(&commands, 1);
    }
    if (argc == 2 && !strcmp(argv[1], "stop")) return request_stop() ? RT_EOK : -RT_ERROR;
    if (argc == 3 && !strcmp(argv[1], "start")) {
        unsigned seconds = 0;
        const char *p = argv[2];
        if (!*p) return -RT_EINVAL;
        for (; *p; ++p) {
            if (*p < '0' || *p > '9' || seconds > WF_CHARGE_MAX_SECONDS) return -RT_EINVAL;
            seconds = seconds * 10 + (unsigned)(*p - '0');
        }
        if (!request_start(seconds)) { rt_kprintf("[charger] start rejected: busy/unconfirmed or duration outside 1..60s\n"); return -RT_ERROR; }
        rt_kprintf("[charger] start queued; supervised experiment only, nominal 4170mV/80mA; await readback\n");
        return RT_EOK;
    }
    rt_kprintf("Usage: wf_charger status | stop | start <1..60 seconds>\n");
    return -RT_EINVAL;
}
MSH_CMD_EXPORT(wf_charger, supervised short charge control and readback);

bool wristflow_product_charger_service_start(void)
{
    if (ready) return true;
    if (!wristflow_product_power_adc_init()) return false;
    if (rt_event_init(&commands, "wf_chg", RT_IPC_FLAG_PRIO) != RT_EOK) return false;
    rt_thread_t thread = rt_thread_create("wf_charge", worker, RT_NULL, 4096, 23, 10);
    if (!thread) { rt_event_detach(&commands); return false; }
    if (rt_thread_startup(thread) != RT_EOK) {
        rt_thread_delete(thread);
        rt_event_detach(&commands);
        return false;
    }
    ready = true;
    return true;
}
