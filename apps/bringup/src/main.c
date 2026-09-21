#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "bf0_pm.h"
#include "button.h"
#include "drv_gpio.h"
#include "littlevgl2rtt.h"
#include "lvgl.h"

#define DIM_AFTER_MS       10000U
#define OFF_AFTER_MS       15000U
#define DIM_BRIGHTNESS     10U
#define DISPLAY_WAIT_MS    1000U
#define KEY_EVENT          1U

static struct rt_event key_events;
static rt_device_t lcd;
static rt_device_t touch;
static lv_obj_t *brightness_value;
static lv_obj_t *key_value;
static lv_obj_t *touch_value;
static uint8_t brightness = 60;
static bool dimmed;
static bool off_requested;
static unsigned int key_count;

static void require_ok(rt_err_t result, const char *operation)
{
    if (result != RT_EOK)
    {
        rt_kprintf("[bringup] ERROR %s: %d\n", operation, result);
        RT_ASSERT(result == RT_EOK);
    }
}

static void set_brightness(uint8_t value)
{
    require_ok(rt_device_control(lcd, RTGRAPHIC_CTRL_SET_BRIGHTNESS, &value),
               "brightness");
}

static void button_callback(int32_t pin, button_action_t action)
{
    (void)pin;
    /* The SDK debounce callback must not operate LVGL or change PM locks. */
    if (action == BUTTON_PRESSED)
        rt_event_send(&key_events, KEY_EVENT);
}

static void init_button(void)
{
    button_cfg_t config = {0};
    config.pin = BSP_KEY1_PIN;
#ifdef BSP_KEY1_ACTIVE_HIGH
    config.active_state = BUTTON_ACTIVE_HIGH;
#else
    config.active_state = BUTTON_ACTIVE_LOW;
#endif
    config.mode = PIN_MODE_INPUT;
    config.button_handler = button_callback;
    int32_t id = button_init(&config);
    RT_ASSERT(id >= 0);

    int8_t wake_pin = HAL_HPAON_QueryWakeupPin(GET_GPIO_INSTANCE(BSP_KEY1_PIN),
                                             GET_GPIOx_PIN(BSP_KEY1_PIN));
    RT_ASSERT(wake_pin >= 0);
    require_ok(pm_enable_pin_wakeup(wake_pin, AON_PIN_MODE_DOUBLE_EDGE), "KEY1 wake");
    RT_ASSERT(button_enable(id) == SF_EOK);
    rt_kprintf("[bringup] KEY1 pin=%d wake_index=%d active=%d\n",
               BSP_KEY1_PIN, wake_pin, config.active_state);
}

static bool take_key(rt_int32_t timeout)
{
    rt_uint32_t events;
    rt_err_t result = rt_event_recv(&key_events, KEY_EVENT,
                                    RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                                    timeout, &events);
    if (result == -RT_ETIMEOUT)
        return false;
    require_ok(result, "key event");
    return true;
}

static void key_activity(void)
{
    key_count++;
    lv_label_set_text_fmt(key_value, "KEY1  %u", key_count);
    lv_display_trigger_activity(NULL);
    off_requested = false;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, int x, int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    return label;
}

static void slider_changed(lv_event_t *event)
{
    brightness = (uint8_t)lv_slider_get_value(lv_event_get_target(event));
    lv_label_set_text_fmt(brightness_value, "%u%%", brightness);
    set_brightness(brightness);
    dimmed = false;
}

static void screen_off_clicked(lv_event_t *event)
{
    (void)event;
    off_requested = true;
}

static void create_screen(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x080c10), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xf2f4f5), 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(screen, 2, 0);
    lv_obj_set_style_border_color(screen, lv_color_hex(0x727c85), 0);

    lv_obj_t *title = make_label(screen, "WristFlow", 24, 22);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    static const uint32_t colors[] = {0xee4545, 0x36ce83, 0x428bea, 0xffffff};
    for (unsigned int i = 0; i < 4; i++)
    {
        lv_obj_t *swatch = lv_obj_create(screen);
        lv_obj_set_pos(swatch, 24 + (int)i * 88, 75);
        lv_obj_set_size(swatch, 78, 55);
        lv_obj_set_style_radius(swatch, 0, 0);
        lv_obj_set_style_border_width(swatch, 0, 0);
        lv_obj_set_style_bg_color(swatch, lv_color_hex(colors[i]), 0);
        lv_obj_remove_flag(swatch, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }
    make_label(screen, "Brightness", 24, 162);
    brightness_value = make_label(screen, "60%", 293, 162);
    lv_obj_t *slider = lv_slider_create(screen);
    lv_obj_set_pos(slider, 36, 213);
    lv_obj_set_size(slider, 318, 14);
    lv_slider_set_range(slider, 10, 100);
    lv_slider_set_value(slider, brightness, LV_ANIM_OFF);
    lv_obj_set_ext_click_area(slider, 20);
    lv_obj_add_event_cb(slider, slider_changed, LV_EVENT_VALUE_CHANGED, NULL);

    touch_value = make_label(screen, "X ---   Y ---", 24, 272);
    key_value = make_label(screen, "KEY1  0", 24, 307);
    lv_obj_t *off_button = lv_button_create(screen);
    lv_obj_set_size(off_button, 64, 56);
    lv_obj_set_pos(off_button, 302, 367);
    lv_obj_set_style_radius(off_button, 6, 0);
    lv_obj_t *off_icon = lv_label_create(off_button);
    lv_label_set_text(off_icon, LV_SYMBOL_POWER);
    lv_obj_center(off_icon);
    lv_obj_add_event_cb(off_button, screen_off_clicked, LV_EVENT_CLICKED, NULL);
}

static void update_touch_position(void)
{
    lv_indev_t *input = lv_indev_get_next(NULL);
    while (input)
    {
        if (lv_indev_get_type(input) == LV_INDEV_TYPE_POINTER &&
            lv_indev_get_state(input) == LV_INDEV_STATE_PRESSED)
        {
            lv_point_t point;
            lv_indev_get_point(input, &point);
            lv_label_set_text_fmt(touch_value, "X %03d   Y %03d", (int)point.x, (int)point.y);
            break;
        }
        input = lv_indev_get_next(input);
    }
}

static bool wait_display_idle(void)
{
    rt_tick_t started = rt_tick_get();
    for (;;)
    {
        bool busy = true;
        require_ok(rt_device_control(lcd, RTGRAPHIC_CTRL_GET_BUSY, &busy), "LCD busy");
        if (!busy)
            return true;
        if ((rt_tick_t)(rt_tick_get() - started) >= rt_tick_from_millisecond(DISPLAY_WAIT_MS))
            return false;
        rt_thread_mdelay(5);
    }
}

static void suspend_screen(void)
{
    lv_timer_enable(false);
    if (!wait_display_idle())
    {
        rt_kprintf("[bringup] OFF aborted: LCD busy timeout\n");
        lv_timer_enable(true);
        lv_display_trigger_activity(NULL);
        off_requested = false;
        return;
    }
    require_ok(rt_device_control(touch, RTGRAPHIC_CTRL_POWEROFF, NULL), "touch off");
    require_ok(rt_device_control(lcd, RTGRAPHIC_CTRL_POWEROFF, NULL), "LCD off");
    rt_kprintf("[bringup] OFF requested; waiting for KEY1 (not a current measurement)\n");

    /* One owner balances this lock. Events received during shutdown remain queued. */
    rt_pm_release(PM_SLEEP_MODE_IDLE);
    take_key(RT_WAITING_FOREVER);
    rt_pm_request(PM_SLEEP_MODE_IDLE);

    require_ok(rt_device_control(lcd, RTGRAPHIC_CTRL_POWERON, NULL), "LCD on");
    require_ok(rt_device_control(touch, RTGRAPHIC_CTRL_POWERON, NULL), "touch on");
    set_brightness(brightness);
    dimmed = false;
    lv_timer_enable(true);
    lv_obj_invalidate(lv_screen_active());
    key_activity();
    rt_kprintf("[bringup] ACTIVE brightness=%u\n", brightness);
}

int main(void)
{
    rt_pm_request(PM_SLEEP_MODE_IDLE);
    /* No BLE in this experiment; use the official non-BLE LCPU sleep path. */
    HAL_LPAON_Sleep();
    require_ok(rt_event_init(&key_events, "wf_key", RT_IPC_FLAG_FIFO), "key init");
    require_ok(littlevgl2rtt_init("lcd"), "LVGL init");
    lcd = rt_device_find("lcd");
    touch = rt_device_find("touch");
    RT_ASSERT(lcd && touch);
    RT_ASSERT(lv_display_get_horizontal_resolution(NULL) == 390);
    RT_ASSERT(lv_display_get_vertical_resolution(NULL) == 450);
    init_button();
    create_screen();
    set_brightness(brightness);
    lv_display_trigger_activity(NULL);
    rt_kprintf("[bringup] ACTIVE 390x450 dim=%u ms off=%u ms brightness=%u\n",
               DIM_AFTER_MS, OFF_AFTER_MS, brightness);

    for (;;)
    {
        if (take_key(0))
            key_activity();
        lv_timer_handler();
        update_touch_position();
        uint32_t inactive = lv_display_get_inactive_time(NULL);
        if (off_requested || inactive >= OFF_AFTER_MS)
        {
            suspend_screen();
            continue;
        }
        if (inactive >= DIM_AFTER_MS && !dimmed)
        {
            set_brightness(DIM_BRIGHTNESS);
            dimmed = true;
            rt_kprintf("[bringup] DIM brightness=%u\n", DIM_BRIGHTNESS);
        }
        else if (inactive < DIM_AFTER_MS && dimmed)
        {
            set_brightness(brightness);
            dimmed = false;
            rt_kprintf("[bringup] ACTIVE brightness=%u\n", brightness);
        }
        rt_thread_mdelay(10);
    }
}
