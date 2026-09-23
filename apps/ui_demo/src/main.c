#include <rtthread.h>
#include <rtdevice.h>
#include "littlevgl2rtt.h"
#include "wristflow_ui.h"
#include "watchface_presenter.h"

/* A visual carousel only. Product navigation and power policy belong to watch_core. */
#define PAGE_COUNT 5U
static lv_obj_t *pages[PAGE_COUNT];
static lv_obj_t *control_center;
static unsigned int page_index;

static void gesture(lv_event_t *event)
{
    (void)event;
    lv_indev_t *input = lv_indev_active();
    if (!input)
        return;
    lv_dir_t direction = lv_indev_get_gesture_dir(input);
    lv_obj_t *active = lv_screen_active();
    if (active == control_center)
    {
        if (direction == LV_DIR_BOTTOM)
            lv_screen_load(pages[0]);
    }
    else if (direction == LV_DIR_LEFT || direction == LV_DIR_RIGHT)
    {
        page_index = (page_index + (direction == LV_DIR_LEFT ? 1U : PAGE_COUNT - 1U)) % PAGE_COUNT;
        lv_screen_load(pages[page_index]);
    }
    else if (page_index == 0U && direction == LV_DIR_TOP)
        lv_screen_load(control_center);
    lv_indev_wait_release(input);
}

int main(void)
{
    RT_ASSERT(littlevgl2rtt_init("lcd") == RT_EOK);
    RT_ASSERT(lv_display_get_horizontal_resolution(NULL) == 390);
    RT_ASSERT(lv_display_get_vertical_resolution(NULL) == 450);
    wristflow_ui_init("");
    pages[0] = screen_watchface_create();
    pages[1] = screen_tile_heart_rate_create();
    pages[2] = screen_tile_activity_create();
    pages[3] = screen_tile_system_create();
    pages[4] = screen_tile_full_create();
    RT_ASSERT(wristflow_watchface_set_time(pages[0], 22, 48));
    control_center = screen_control_center_create();
    for (unsigned int i = 0; i < PAGE_COUNT; i++)
        lv_obj_add_event_cb(pages[i], gesture, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(control_center, gesture, LV_EVENT_GESTURE, NULL);
    lv_obj_t *initial = lv_screen_active();
    lv_screen_load(pages[0]);
    lv_obj_delete(initial);
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
