#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "button.h"
#include "littlevgl2rtt.h"
#include "wristflow_ui.h"
#include "ui_demo.h"

static struct rt_event key_events;
/* Diagnostic totals are reported once per 2 seconds, never per frame. */
typedef struct { uint32_t count, total, maximum; } timing_t;
static timing_t handler_time, render_time, flush_time, brightness_time;
static uint32_t render_started, flush_started;

static uint32_t milliseconds(void)
{
    return (uint32_t)((uint64_t)rt_tick_get() * 1000 / RT_TICK_PER_SECOND);
}

static void sample_time(timing_t *timing, uint32_t elapsed)
{
    timing->count++;
    timing->total += elapsed;
    if (elapsed > timing->maximum) timing->maximum = elapsed;
}

static void display_timing(lv_event_t *event)
{
    uint32_t now = milliseconds();
    switch (lv_event_get_code(event)) {
    case LV_EVENT_RENDER_START: render_started = now; break;
    case LV_EVENT_RENDER_READY: sample_time(&render_time, now - render_started); break;
    case LV_EVENT_FLUSH_START: flush_started = now; break;
    case LV_EVENT_FLUSH_FINISH: sample_time(&flush_time, now - flush_started); break;
    default: break;
    }
}

static unsigned average(timing_t *timing)
{
    return timing->count ? timing->total / timing->count : 0;
}

static void report_timing(uint32_t now)
{
    static uint32_t reported;
    if (now - reported < 2000) return;
    if (render_time.count || brightness_time.count) {
        rt_kprintf("[ui_perf] window=%u surface=%u frames=%u handler=%u/%u render=%u/%u flush=%u/%u brightness=%u/%u n=%u (avg/max ms)\n",
            now - reported, wristflow_demo_surface(), render_time.count,
            average(&handler_time), handler_time.maximum, average(&render_time), render_time.maximum,
            average(&flush_time), flush_time.maximum, average(&brightness_time), brightness_time.maximum,
            brightness_time.count);
    }
    handler_time = render_time = flush_time = brightness_time = (timing_t){0};
    reported = now;
}

static void button_callback(int32_t pin, button_action_t action)
{
    (void)pin;
    /* The debounce callback runs outside the UI thread. */
    if (action == BUTTON_CLICKED) rt_event_send(&key_events, 1);
}

static void set_brightness(uint8_t brightness, void *context)
{
    uint32_t started = milliseconds();
    RT_ASSERT(rt_device_control((rt_device_t)context, RTGRAPHIC_CTRL_SET_BRIGHTNESS, &brightness) == RT_EOK);
    sample_time(&brightness_time, milliseconds() - started);
}

int main(void)
{
    RT_ASSERT(littlevgl2rtt_init("lcd") == RT_EOK);
    RT_ASSERT(lv_display_get_horizontal_resolution(NULL) == 390);
    RT_ASSERT(lv_display_get_vertical_resolution(NULL) == 450);
    lv_display_add_event_cb(lv_display_get_default(), display_timing, LV_EVENT_ALL, NULL);
    wristflow_ui_init("");
    rt_device_t lcd = rt_device_find("lcd");
    RT_ASSERT(lcd);
    wristflow_demo_set_platform(set_brightness, lcd);
    wristflow_demo_start();
    RT_ASSERT(rt_event_init(&key_events, "ui_key", RT_IPC_FLAG_FIFO) == RT_EOK);
    button_cfg_t key = {0};
    key.pin = BSP_KEY1_PIN;
#ifdef BSP_KEY1_ACTIVE_HIGH
    key.active_state = BUTTON_ACTIVE_HIGH;
#else
    key.active_state = BUTTON_ACTIVE_LOW;
#endif
    key.mode = PIN_MODE_INPUT;
    key.button_handler = button_callback;
    int32_t key_id = button_init(&key);
    RT_ASSERT(key_id >= 0);
    RT_ASSERT(button_enable(key_id) == SF_EOK);
    rt_kprintf("[ui_demo] Static sample data; no sensors, BLE or power-policy validation\n");
    for (;;)
    {
        uint32_t started = milliseconds();
        uint32_t wait_ms = lv_timer_handler();
        sample_time(&handler_time, milliseconds() - started);
        report_timing(milliseconds());
        /* Sleep until LVGL's next deadline; the key event wakes us earlier. */
        wait_ms = LV_CLAMP(1, wait_ms, 20);
        rt_uint32_t events;
        if (rt_event_recv(&key_events, 1, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                          rt_tick_from_millisecond(wait_ms), &events) == RT_EOK)
            wristflow_demo_key();
    }
}
