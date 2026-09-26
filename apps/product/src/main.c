#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "button.h"
#include "littlevgl2rtt.h"
#include "wristflow_ui.h"
#include "product_ui.h"
#include "product_services.h"

static struct rt_event key_events;

static void button_callback(int32_t pin, button_action_t action)
{
    (void)pin;
    if (action == BUTTON_CLICKED) rt_event_send(&key_events, 1);
}

static void set_brightness(uint8_t brightness, void *context)
{
    RT_ASSERT(rt_device_control((rt_device_t)context, RTGRAPHIC_CTRL_SET_BRIGHTNESS, &brightness) == RT_EOK);
}

int main(void)
{
    wristflow_settings_t settings;
    wristflow_layout_t layout;
    wristflow_product_services_start(&settings, &layout);
    wristflow_watch_snapshot_t snapshot = wristflow_product_services_snapshot();
    RT_ASSERT(littlevgl2rtt_init("lcd") == RT_EOK);
    RT_ASSERT(lv_display_get_horizontal_resolution(NULL) == 390);
    RT_ASSERT(lv_display_get_vertical_resolution(NULL) == 450);
    wristflow_ui_init("");
    rt_device_t lcd = rt_device_find("lcd");
    RT_ASSERT(lcd);
    lv_obj_t *initial = lv_screen_active();
    wristflow_ui_shell_t *shell = wristflow_product_ui_create_with_layout(&snapshot, &settings, set_brightness, lcd,
        &layout, wristflow_product_services_layout, wristflow_product_services_layout_status, NULL);
    RT_ASSERT(shell);
    lv_obj_delete(initial);
    RT_ASSERT(rt_event_init(&key_events, "wf_key", RT_IPC_FLAG_FIFO) == RT_EOK);
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
    rt_kprintf("[product] USB stage: RTC=%s, battery absent, BLE/PM pending\n",
               snapshot.time_unavailable ? "not synchronized" : "in range");
    uint32_t sampled = lv_tick_get();
    for (;;) {
        uint32_t wait_ms = lv_timer_handler();
        uint32_t now = lv_tick_get();
        if (now - sampled >= 250) {
            wristflow_watch_snapshot_t next = wristflow_product_services_snapshot();
            /* RTC polling and settings collection must not redraw static pages. */
            if (next.hour_24 != snapshot.hour_24 || next.minute != snapshot.minute ||
                next.time_unavailable != snapshot.time_unavailable ||
                next.uptime_seconds / 60 != snapshot.uptime_seconds / 60) {
                snapshot = next;
                wristflow_ui_shell_update(shell, &snapshot);
            }
            if (wristflow_ui_shell_get_settings(shell, &settings))
                wristflow_product_services_settings(&settings);
            sampled = now;
        }
        rt_uint32_t events;
        if (rt_event_recv(&key_events, 1, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
            rt_tick_from_millisecond(LV_CLAMP(1, wait_ms, 20)), &events) == RT_EOK)
            wristflow_ui_shell_key(shell);
    }
}
