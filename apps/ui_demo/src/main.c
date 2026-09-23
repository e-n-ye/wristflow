#include <rtthread.h>
#include <rtdevice.h>
#include "littlevgl2rtt.h"
#include "wristflow_ui.h"
#include "ui_demo.h"

int main(void)
{
    RT_ASSERT(littlevgl2rtt_init("lcd") == RT_EOK);
    RT_ASSERT(lv_display_get_horizontal_resolution(NULL) == 390);
    RT_ASSERT(lv_display_get_vertical_resolution(NULL) == 450);
    wristflow_ui_init("");
    wristflow_demo_start();
    rt_device_t lcd = rt_device_find("lcd");
    uint8_t brightness = 60;
    RT_ASSERT(lcd);
    RT_ASSERT(rt_device_control(lcd, RTGRAPHIC_CTRL_SET_BRIGHTNESS, &brightness) == RT_EOK);
    rt_kprintf("[ui_demo] Static sample data; no sensors, BLE or power-policy validation\n");
    for (;;)
    {
        lv_timer_handler();
        rt_thread_mdelay(10);
    }
}
