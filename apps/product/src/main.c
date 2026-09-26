#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "button.h"
#include "littlevgl2rtt.h"
#include "wristflow_ui.h"
#include "product_ui.h"
#include "product_services.h"

static struct rt_event key_events;
static wristflow_ui_shell_t *product_shell;
static lv_indev_read_cb_t original_pointer_read;
static volatile bool waiting_for_wake;
static volatile bool suppress_wake_click;

static void read_pointer(lv_indev_t *input, lv_indev_data_t *data)
{
    original_pointer_read(input, data);
    if (!wristflow_ui_shell_filter_touch(product_shell, data->state == LV_INDEV_STATE_PRESSED))
        data->state = LV_INDEV_STATE_RELEASED;
}

static void button_callback(int32_t pin, button_action_t action)
{
    (void)pin;
    if (action == BUTTON_PRESSED && waiting_for_wake) {
        suppress_wake_click = true;
        rt_event_send(&key_events, 1);
    } else if (action == BUTTON_CLICKED && !suppress_wake_click) {
        rt_event_send(&key_events, 1);
    } else if (action == BUTTON_RELEASED) {
        suppress_wake_click = false;
    }
}

static void set_brightness(uint8_t brightness, void *context)
{
    RT_ASSERT(rt_device_control((rt_device_t)context, RTGRAPHIC_CTRL_SET_BRIGHTNESS, &brightness) == RT_EOK);
}

static void screen_off(rt_device_t lcd, rt_device_t touch, lv_indev_t *pointer)
{
    lv_timer_enable(false);
    rt_tick_t started = rt_tick_get();
    bool busy = true;
    while (busy) {
        if (rt_device_control(lcd, RTGRAPHIC_CTRL_GET_BUSY, &busy) != RT_EOK ||
            rt_tick_get() - started >= rt_tick_from_millisecond(1000)) {
            rt_kprintf("[product] screen off deferred: LCD busy\n");
            wristflow_ui_shell_display_retry(product_shell);
            lv_timer_enable(true);
            return;
        }
        if (busy) rt_thread_mdelay(5);
    }
    RT_ASSERT(rt_device_control(touch, RTGRAPHIC_CTRL_POWEROFF, NULL) == RT_EOK);
    RT_ASSERT(rt_device_control(lcd, RTGRAPHIC_CTRL_POWEROFF, NULL) == RT_EOK);
    rt_kprintf("[product] screen off; KEY1 wake (PM unchanged)\n");
    rt_uint32_t events;
    waiting_for_wake = true;
    RT_ASSERT(rt_event_recv(&key_events, 1, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
        RT_WAITING_FOREVER, &events) == RT_EOK);
    waiting_for_wake = false;
    rt_kprintf("[product] KEY1 wake event received\n");
    RT_ASSERT(rt_device_control(lcd, RTGRAPHIC_CTRL_POWERON, NULL) == RT_EOK);
    RT_ASSERT(rt_device_control(touch, RTGRAPHIC_CTRL_POWERON, NULL) == RT_EOK);
    lv_indev_reset(pointer, NULL);
    lv_indev_wait_release(pointer);
    wristflow_ui_shell_key(product_shell); /* Consume the completed wake key. */
    lv_timer_enable(true);
    lv_obj_invalidate(lv_screen_active());
    rt_kprintf("[product] screen on\n");
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
    product_shell = shell;
    rt_device_t touch = rt_device_find("touch");
    RT_ASSERT(touch);
    lv_indev_t *pointer = lv_indev_get_next(NULL);
    while (pointer && lv_indev_get_type(pointer) != LV_INDEV_TYPE_POINTER) pointer = lv_indev_get_next(pointer);
    RT_ASSERT(pointer);
    original_pointer_read = lv_indev_get_read_cb(pointer);
    RT_ASSERT(original_pointer_read);
    lv_indev_set_read_cb(pointer, read_pointer);
    wristflow_ui_shell_enable_display_policy(shell);
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
        if (wristflow_ui_shell_display_phase(shell) == WRISTFLOW_DISPLAY_OFF) {
            if (wristflow_ui_shell_get_settings(shell, &settings))
                wristflow_product_services_settings(&settings);
            screen_off(lcd, touch, pointer);
            continue;
        }
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
