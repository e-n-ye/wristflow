#include "product_power.h"
#include <rtthread.h>
#include <rthw.h>
#include <rtdevice.h>

static struct rt_event requests;
static wristflow_power_snapshot_t cached;
static bool ready, lit;
static uint32_t epoch;

/* Eight-byte copy on this HCPU only; never hold this critical section over I/O. */
wristflow_power_snapshot_t wristflow_product_power_snapshot(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    wristflow_power_snapshot_t value = cached;
    rt_hw_interrupt_enable(level);
    return value;
}

static bool publish(const wristflow_power_snapshot_t *value, uint32_t generation)
{
    rt_base_t level = rt_hw_interrupt_disable();
    bool current = lit && epoch == generation;
    if (current) cached = *value;
    rt_hw_interrupt_enable(level);
    return current;
}

static void sample_once(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    bool active = lit;
    uint32_t generation = epoch;
    rt_hw_interrupt_enable(level);
    if (!active) return;
#ifdef RT_USING_PM
    rt_pm_request(PM_SLEEP_MODE_IDLE);
#endif
    rt_tick_t started = rt_tick_get();
    wristflow_power_snapshot_t next = wristflow_product_power_voltage();
    if (publish(&next, generation)) {
        wristflow_product_power_input(&next);
        publish(&next, generation);
    }
    rt_tick_t elapsed = rt_tick_get() - started;
    if (elapsed >= rt_tick_from_millisecond(1000))
        rt_kprintf("[power] background read_ms=%lu input_valid=%u; UI did not wait\n",
            (unsigned long)((uint64_t)elapsed * 1000U / RT_TICK_PER_SECOND), next.input_valid);
#ifdef RT_USING_PM
    rt_pm_release(PM_SLEEP_MODE_IDLE);
#endif
}

static void worker(void *context)
{
    (void)context;
    for (;;) {
        rt_uint32_t bits = 0;
        if (rt_event_recv(&requests, 1, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                          RT_WAITING_FOREVER, &bits) == RT_EOK) sample_once();
    }
}

void wristflow_product_power_request(void)
{
    if (ready && lit) rt_event_send(&requests, 1); /* Coalesced, never a sample queue. */
}

void wristflow_product_power_screen(bool visible)
{
    rt_base_t level = rt_hw_interrupt_disable();
    lit = visible;
    ++epoch;
    cached = (wristflow_power_snapshot_t){0};
    rt_hw_interrupt_enable(level);
    if (visible) wristflow_product_power_request();
}

bool wristflow_product_power_start(void)
{
    if (ready) return true;
    if (!wristflow_product_power_adc_init()) return false;
    if (rt_event_init(&requests, "wf_pwr", RT_IPC_FLAG_PRIO) != RT_EOK) return false;
    rt_thread_t thread = rt_thread_create("wf_power", worker, RT_NULL, 4096, 24, 10);
    if (!thread) { rt_event_detach(&requests); return false; }
    ready = true;
    if (rt_thread_startup(thread) != RT_EOK) {
        ready = false;
        rt_thread_delete(thread);
        rt_event_detach(&requests);
        return false;
    }
    wristflow_product_power_screen(true);
    return true;
}
