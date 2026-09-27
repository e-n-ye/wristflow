/* Product PM with bounded diagnostics and a runtime rollback gate. */
#include "product_pm.h"
#include <rtdevice.h>
#include <board.h>
#include <string.h>
#ifdef RT_USING_PM
#include "bf0_pm.h"
#include "drv_gpio.h"
#endif

typedef struct {
    uint32_t enter, exit, deep_elapsed, deep_ticks, max_deep_ticks;
    uint32_t phone, phone_off, background, screen_off, screen_on;
    uint32_t last_wake_source;
} pm_stats_t;
static volatile pm_stats_t stats;
static struct rt_event events;
static struct rt_timer sample_timer;
static bool ready, display_off;
static bool allowed = true;
#ifdef RT_USING_PM
static bool idle_held;
static rt_tick_t entered_at;

/* Called with interrupts disabled by RT-Thread: counters only, never log. */
static void pm_notify(uint8_t event, uint8_t mode, void *context)
{
    (void)context;
    if (event == RT_PM_ENTER_SLEEP) {
        ++stats.enter;
        entered_at = rt_tick_get();
    } else if (event == RT_PM_EXIT_SLEEP) {
        ++stats.exit;
        uint32_t elapsed = rt_tick_get() - entered_at;
        /* Positive compensated ticks distinguish useful returns from immediate vetoes.
         * This observes the SDK path, not rail current or exact hardware residency. */
        if (mode == PM_SLEEP_MODE_DEEP && elapsed && pm_get_power_mode() == PM_SLEEP_MODE_DEEP) {
            ++stats.deep_elapsed;
            stats.deep_ticks += elapsed;
            if (elapsed > stats.max_deep_ticks) stats.max_deep_ticks = elapsed;
            stats.last_wake_source = pm_get_wakeup_src();
        }
    }
}

/* Both the command thread and UI thread use this under the IRQ lock. */
static void update_gate(void)
{
    bool hold = !allowed || !display_off;
    if (hold == idle_held) return;
    if (hold) rt_pm_request(PM_SLEEP_MODE_IDLE);
    else rt_pm_release(PM_SLEEP_MODE_IDLE);
    idle_held = hold;
}
#endif

void wristflow_product_event_send(rt_uint32_t mask)
{
    if (ready) rt_event_send(&events, mask);
}

rt_uint32_t wristflow_product_event_wait(rt_int32_t timeout)
{
    rt_uint32_t mask = 0;
    rt_err_t result = rt_event_recv(&events, WF_EVENT_ALL,
        RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, timeout, &mask);
    RT_ASSERT(result == RT_EOK || result == -RT_ETIMEOUT);
    return result == RT_EOK ? mask : 0;
}

static void sample_expired(void *context)
{
    (void)context;
    wristflow_product_event_send(WF_EVENT_PM_SAMPLE);
}

void wristflow_product_pm_start(void)
{
    RT_ASSERT(!ready);
    RT_ASSERT(rt_event_init(&events, "wf_evt", RT_IPC_FLAG_FIFO) == RT_EOK);
    rt_timer_init(&sample_timer, "wf_probe", sample_expired, NULL,
        rt_tick_from_millisecond(5000), RT_TIMER_FLAG_ONE_SHOT | RT_TIMER_FLAG_SOFT_TIMER);
#ifdef RT_USING_PM
    rt_base_t level = rt_hw_interrupt_disable();
    update_gate(); /* Initialization and every lit frame hold idle; only OFF releases it. */
    rt_pm_notify_set(pm_notify, NULL);
    rt_hw_interrupt_enable(level);
    int8_t pin = HAL_HPAON_QueryWakeupPin(GET_GPIO_INSTANCE(BSP_KEY1_PIN), GET_GPIOx_PIN(BSP_KEY1_PIN));
    RT_ASSERT(pin >= 0);
    RT_ASSERT(pm_enable_pin_wakeup(pin, AON_PIN_MODE_DOUBLE_EDGE) == RT_EOK);
    rt_kprintf("[pm-p1] KEY1 pin=%d aon=%d; auto sleep when off; wf_pm hold to inhibit\n", BSP_KEY1_PIN, pin);
#else
    rt_kprintf("[pm-p1] PM disabled at build time\n");
#endif
    ready = true;
}

void wristflow_product_pm_screen(bool off)
{
    rt_base_t level = rt_hw_interrupt_disable();
    if (off != display_off) {
        display_off = off;
        if (off) ++stats.screen_off;
        else ++stats.screen_on;
    }
#ifdef RT_USING_PM
    update_gate();
#endif
    rt_hw_interrupt_enable(level);
}

void wristflow_product_pm_phone_event(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    ++stats.phone;
    if (display_off) ++stats.phone_off;
    rt_hw_interrupt_enable(level);
    wristflow_product_event_send(WF_EVENT_PHONE);
}

void wristflow_product_pm_background(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    ++stats.background;
    rt_hw_interrupt_enable(level);
}

void wristflow_product_pm_report(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    pm_stats_t s = stats;
    bool off = display_off, allow = allowed;
    unsigned idle = 0, held = 0;
#ifdef RT_USING_PM
    held = idle_held;
    idle = rt_pm_sleep_mode_state_get(PM_SLEEP_MODE_IDLE);
#endif
    rt_hw_interrupt_enable(level);
    /* Each call stays below the SDK's 128-byte console buffer at uint32 maxima. */
    rt_kprintf("[pm-p1] tick=%u allow=%u off=%u own_idle=%u sdk_idle=%u\n",
        rt_tick_get(), allow, off, held, idle);
    rt_kprintf("[pm-p1] enter=%u exit=%u deep_elapsed=%u\n", s.enter, s.exit, s.deep_elapsed);
    rt_kprintf("[pm-p1] deep_ticks=%u max_ticks=%u wake=0x%x\n", s.deep_ticks, s.max_deep_ticks, s.last_wake_source);
    rt_kprintf("[pm-p1] screen_off=%u screen_on=%u tick_hz=%u\n", s.screen_off, s.screen_on, RT_TICK_PER_SECOND);
    rt_kprintf("[pm-p1] phone=%u phone_off=%u background=%u\n", s.phone, s.phone_off, s.background);
}

static int wf_pm(int argc, char **argv)
{
    if (!ready) return -RT_ERROR;
    if (argc != 2) {
        rt_kprintf("wf_pm status | hold | allow | sample | wake (sample is one shot after 5s)\n");
        return -RT_EINVAL;
    }
    if (!strcmp(argv[1], "status")) wristflow_product_pm_report();
    else if (!strcmp(argv[1], "wake")) wristflow_product_event_send(WF_EVENT_TEST_WAKE);
    else if (!strcmp(argv[1], "sample")) rt_timer_start(&sample_timer);
    else if (!strcmp(argv[1], "hold") || !strcmp(argv[1], "allow")) {
#ifdef RT_USING_PM
        bool enable = !strcmp(argv[1], "allow");
        /* Arm a bounded observation before releasing the gate. */
        if (enable) rt_timer_start(&sample_timer);
        rt_base_t level = rt_hw_interrupt_disable();
        allowed = enable;
        update_gate();
        rt_hw_interrupt_enable(level);
        wristflow_product_pm_report();
#else
        return -RT_ENOSYS;
#endif
    } else return -RT_EINVAL;
    return RT_EOK;
}
MSH_CMD_EXPORT(wf_pm, Product PM P1 diagnostics and runtime rollback);
