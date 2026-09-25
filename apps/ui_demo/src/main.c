#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "button.h"
#include "littlevgl2rtt.h"
#include "wristflow_ui.h"
#include "ui_demo.h"

static struct rt_event key_events;

static void button_callback(int32_t pin, button_action_t action)
{
    (void)pin;
    /* The debounce callback runs outside the UI thread. */
    if (action == BUTTON_CLICKED) rt_event_send(&key_events, 1);
}

static void set_brightness(uint8_t brightness, void *context)
{
    RT_ASSERT(rt_device_control((rt_device_t)context, RTGRAPHIC_CTRL_SET_BRIGHTNESS, &brightness) == RT_EOK);
}

int main(void)
{
    RT_ASSERT(littlevgl2rtt_init("lcd") == RT_EOK);
    RT_ASSERT(lv_display_get_horizontal_resolution(NULL) == 390);
    RT_ASSERT(lv_display_get_vertical_resolution(NULL) == 450);
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
        rt_uint32_t events;
        if (rt_event_recv(&key_events, 1, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, 0, &events) == RT_EOK)
            wristflow_demo_key();
        lv_timer_handler();
        rt_thread_mdelay(10);
    }
}
