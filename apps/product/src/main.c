#include <rtthread.h>
#include <rthw.h>
#include <rtdevice.h>
#include <board.h>
#include "button.h"
#include "littlevgl2rtt.h"
#include "wristflow_ui.h"
#include "product_ui.h"
#include "product_services.h"
#include "product_ble.h"
#include "product_pm.h"
#include "product_steps.h"
#include "product_wrist.h"
#include "product_charger.h"
#include "product_power.h"
#include "battery_estimate.h"
#include "product_battery_trace.h"
#include "product_countdown.h"
#include "product_alarm.h"
#include "product_motor.h"
#include "weather_screen.h"
#include <string.h>
#ifdef RT_USING_FINSH
#include <finsh.h>
#endif

static wristflow_ui_shell_t *product_shell;
static lv_indev_read_cb_t original_pointer_read;
static volatile bool waiting_for_wake;
static volatile bool suppress_wake_click;
static bool defer_brightness;
static uint8_t wake_brightness;
/* This bounded phone copy is too large for the main thread's stack. */
static wristflow_notifications_snapshot_t notifications;
static wristflow_power_snapshot_t power;
static rt_tick_t power_sampled;
static wristflow_battery_estimator_t battery_estimator;
static wristflow_battery_diagnostic_t battery_diagnostic;
static bool battery_adc_valid, brightness_valid, lcd_on;
static uint8_t requested_brightness, acknowledged_brightness;
static bool wrist_setting_known, wrist_setting;
static wristflow_carousel_snapshot_t carousel_current, carousel_last_mismatch;
static bool carousel_valid, carousel_mismatch_valid;

static void capture_battery(void)
{
    wristflow_battery_record_t record = {.observed_ms=(uint32_t)((uint64_t)rt_tick_get()*1000U/RT_TICK_PER_SECOND),
        .adc_valid=battery_adc_valid, .power=power, .diagnostic=battery_diagnostic};
    const wristflow_navigation_t *navigation = wristflow_ui_shell_navigation(product_shell);
    if (navigation) {
        record.context.surface = (uint16_t)navigation->surface;
        record.context.phase = (uint8_t)wristflow_ui_shell_display_phase(product_shell);
        record.context.flags |= WF_BATTERY_UI_VALID;
        if (wristflow_ui_shell_charging_active(product_shell)) record.context.flags |= WF_BATTERY_CHARGING_OVERLAY;
        if (wristflow_ui_shell_reminder_active(product_shell)) record.context.flags |= WF_BATTERY_REMINDER_OVERLAY;
    }
    record.context.requested_brightness=requested_brightness;
    record.context.acknowledged_brightness=acknowledged_brightness;
    if (brightness_valid) record.context.flags |= WF_BATTERY_BRIGHTNESS_VALID;
    if (lcd_on) record.context.flags |= WF_BATTERY_LCD_ON;
    if (defer_brightness) record.context.flags |= WF_BATTERY_BRIGHTNESS_DEFERRED;
    wristflow_product_battery_trace_observe(&record);
}

static void capture_carousel(void)
{
    wristflow_carousel_snapshot_t value;
    if (!wristflow_ui_shell_carousel_snapshot(product_shell, &value)) return;
    /* LVGL reads above run only on the UI thread; publish bounded copies. */
    rt_base_t level = rt_hw_interrupt_disable();
    carousel_current = value;
    carousel_valid = true;
    if (value.mismatch) {
        carousel_last_mismatch = value;
        carousel_mismatch_valid = true;
    }
    rt_hw_interrupt_enable(level);
}

#ifdef RT_USING_FINSH
static void report_carousel(const char *label, const wristflow_carousel_snapshot_t *s)
{
    rt_kprintf("[ui-ring] %s ms=%lu surface=%u phase=%u page=%u/%u mismatch=%lu\n",
        label, (unsigned long)s->sampled_ms, s->surface, s->display_phase, s->page, s->pages,
        (unsigned long)s->mismatch);
    rt_kprintf("[ui-ring] %s x=%ld left=%ld right=%ld\n", label, (long)s->x, (long)s->left, (long)s->right);
    rt_kprintf("[ui-ring] %s settled=%u scrolling=%u settling=%u transition=%u recenter=%u dots=%u/%u selected=%u\n",
        label, s->settled, s->scrolling, s->settling, s->transitioning, s->recentering,
        s->dots_visible, s->dots_present, s->selected_dots);
    rt_kprintf("[ui-ring] %s first=%ld,%ld,0x%x last=%ld,%ld,0x%x; endpoint x,width,flags\n",
        label, (long)s->edge_x[0], (long)s->edge_width[0], s->edge_flags[0],
        (long)s->edge_x[1], (long)s->edge_width[1], s->edge_flags[1]);
}
static int wf_ui(int argc, char **argv)
{
    if (argc != 2 || strcmp(argv[1], "status")) {
        rt_kprintf("wf_ui status\n");
        return -1;
    }
    rt_base_t level = rt_hw_interrupt_disable();
    wristflow_carousel_snapshot_t current = carousel_current, last = carousel_last_mismatch;
    bool valid = carousel_valid, last_valid = carousel_mismatch_valid;
    rt_hw_interrupt_enable(level);
    rt_kprintf("[ui-ring] cache_valid=%u last_mismatch_valid=%u; cached only, ms is UI sample time\n", valid, last_valid);
    if (valid) report_carousel("current", &current);
    if (last_valid) report_carousel("last-mismatch", &last);
    return 0;
}
MSH_CMD_EXPORT(wf_ui, Read cached carousel state without changing UI);
#endif

static void sync_wrist_setting(void)
{
    wristflow_settings_t settings;
    if (wristflow_ui_shell_get_settings(product_shell, &settings) &&
        (!wrist_setting_known || settings.wrist_wake != wrist_setting)) {
        wrist_setting_known = true;
        wrist_setting = settings.wrist_wake;
        wristflow_product_wrist_enable(wrist_setting);
    }
}

static wristflow_watch_snapshot_t read_snapshot(bool force_power)
{
    if (force_power || rt_tick_get() - power_sampled >= rt_tick_from_millisecond(5000)) {
        wristflow_product_power_request();
        power_sampled = rt_tick_get();
    }
    wristflow_power_snapshot_t next = wristflow_product_power_snapshot();
    wristflow_product_charger_display(&next);
    battery_adc_valid = next.voltage_valid;
    wristflow_battery_estimate_diagnosed(&battery_estimator, &next,
        (uint32_t)((uint64_t)rt_tick_get()*1000U/RT_TICK_PER_SECOND), &battery_diagnostic);
    if (!wristflow_power_equal(&power, &next))
        rt_kprintf("[power] %s voltage_valid=%u VBAT=%umV; estimate_valid=%u SOC~%u%%\n",
                   wristflow_power_input_text(&next), next.voltage_valid, next.millivolts,
                   next.estimate_valid, next.estimated_percent);
    power = next;
    capture_battery();
    wristflow_watch_snapshot_t snapshot = wristflow_product_services_snapshot();
    wristflow_product_apply_power(&snapshot, &power);
    wristflow_steps_snapshot_t steps = wristflow_product_steps_snapshot();
    snapshot.activity.metrics[WRISTFLOW_ACTIVITY_STEPS] = steps.metric;
    snapshot.activity.steps_observation = steps.observation;
    return snapshot;
}

static bool sync_notifications(void)
{
    wristflow_product_ble_snapshot(&notifications);
    return wristflow_ui_shell_notifications(product_shell, &notifications);
}


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
        wristflow_product_event_send(WF_EVENT_KEY);
    } else if (action == BUTTON_CLICKED && !suppress_wake_click) {
        wristflow_product_event_send(WF_EVENT_KEY);
    } else if (action == BUTTON_RELEASED) {
        suppress_wake_click = false;
    }
}

static void set_brightness(uint8_t brightness, void *context)
{
    requested_brightness = brightness;
    if (defer_brightness) { wake_brightness = brightness; return; }
    RT_ASSERT(rt_device_control((rt_device_t)context, RTGRAPHIC_CTRL_SET_BRIGHTNESS, &brightness) == RT_EOK);
    acknowledged_brightness = brightness;
    brightness_valid = true; /* Driver success, not measured luminance/PWM. */
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
    waiting_for_wake = true;
    RT_ASSERT(rt_device_control(touch, RTGRAPHIC_CTRL_POWEROFF, NULL) == RT_EOK);
    RT_ASSERT(rt_device_control(lcd, RTGRAPHIC_CTRL_POWEROFF, NULL) == RT_EOK);
    lcd_on = false;
    /* Touch power work is asynchronous and holds its own SDK idle request. */
    rt_kprintf("[product] screen off; notification events active\n");
    wristflow_product_power_screen(false);
    wristflow_product_steps_screen(false);
    wristflow_product_pm_screen(true);
    capture_battery();
    rt_uint32_t events;
    bool preview = false, reminder = false, charging = false, wrist = false;
    defer_brightness = true;
    for (;;) {
        events = wristflow_product_event_wait(RT_WAITING_FOREVER);
        /* Rebuild from service state on every wake: event bits can coalesce.
         * An acknowledged reminder stays pending without repeatedly waking. */
        reminder = wristflow_ui_shell_reminder_event(product_shell);
        wristflow_watch_snapshot_t charge_snapshot = read_snapshot(false);
        charging = wristflow_ui_shell_charging_event(product_shell, &charge_snapshot);
        wristflow_ui_shell_update(product_shell, &charge_snapshot);
        if (events & WF_EVENT_PHONE) {
            wristflow_product_pm_background();
            preview = sync_notifications();
        }
        if (events & WF_EVENT_PM_SAMPLE) wristflow_product_pm_report();
        /* KEY wins a coalesced batch: waking by wrist first would make the
         * same KEY look like an ACTIVE-screen navigation press. */
        if ((events & WF_EVENT_WRIST) && wristflow_product_wrist_take_wake() &&
            !(events & (WF_EVENT_KEY | WF_EVENT_TEST_WAKE)))
            wrist = wristflow_ui_shell_wrist_wake(product_shell);
        if (reminder || preview || charging || wrist || (events & (WF_EVENT_KEY | WF_EVENT_TEST_WAKE))) break;
    }
    wristflow_product_pm_screen(false); /* Hold idle before restoring the display. */
    waiting_for_wake = false;
    rt_kprintf("[product] wake event=%s\n", events & WF_EVENT_KEY ? "KEY1" : reminder ? "reminder" : preview ? "notification" : charging ? "charging" : wrist ? "wrist" : "diagnostic");
    wristflow_product_pm_report();
    /* Select the wake destination while dark; expose only its completed frame. */
    defer_brightness = true;
    if (!charging && (events & (WF_EVENT_KEY | WF_EVENT_TEST_WAKE))) wristflow_ui_shell_key(product_shell);
    wristflow_watch_snapshot_t wake_snapshot = read_snapshot(false);
    wristflow_ui_shell_update(product_shell, &wake_snapshot);
    RT_ASSERT(rt_device_control(lcd, RTGRAPHIC_CTRL_POWERON, NULL) == RT_EOK);
    lcd_on = true;
    RT_ASSERT(rt_device_control(touch, RTGRAPHIC_CTRL_POWERON, NULL) == RT_EOK);
    lv_indev_reset(pointer, NULL);
    lv_indev_wait_release(pointer);
    lv_timer_enable(true);
    lv_obj_invalidate(lv_screen_active());
    lv_obj_invalidate(lv_layer_top());
    lv_refr_now(NULL);
    started = rt_tick_get();
    do {
        RT_ASSERT(rt_device_control(lcd, RTGRAPHIC_CTRL_GET_BUSY, &busy) == RT_EOK);
        if (busy) rt_thread_mdelay(5);
    } while (busy && rt_tick_get() - started < rt_tick_from_millisecond(1000));
    if (busy) rt_kprintf("[product] wake frame incomplete: LCD busy after %lu ticks; brightness held at zero\n",
        (unsigned long)(rt_tick_get() - started));
    RT_ASSERT(!busy);
    defer_brightness = false;
    set_brightness(wake_brightness, lcd);
    rt_kprintf("[product] screen on\n");
    wristflow_product_power_screen(true); /* Start device work only after the first lit frame. */
    wristflow_product_steps_screen(true);
    capture_battery();
}

static uint32_t vibration_preview(wristflow_vibration_mode_t mode, void *context)
{
    (void)context;
    return wristflow_product_motor_preview(mode == WRISTFLOW_VIBRATION_ENHANCED);
}
static void vibration_cancel(uint32_t request, void *context)
{ (void)context; (void)wristflow_product_motor_cancel_preview(request); }
static uint32_t reminder_vibration(wristflow_vibration_mode_t mode, void *context)
{ (void)context; return wristflow_product_motor_reminder(mode == WRISTFLOW_VIBRATION_ENHANCED); }
static void reminder_vibration_cancel(uint32_t request, void *context)
{ (void)context; (void)wristflow_product_motor_cancel_reminder(request); }

int main(void)
{
    wristflow_product_charger_report();
    wristflow_product_pm_start();
    bool motor_ready = wristflow_product_motor_start(wristflow_product_motor_board_io());
    if (!motor_ready)
        rt_kprintf("[motor] unavailable; manual pulse disabled\n");
    const wristflow_countdown_port_t *countdown = wristflow_product_countdown_start();
    wristflow_settings_t settings;
    wristflow_layout_t layout;
    wristflow_product_services_start(&settings, &layout);
    const wristflow_alarm_port_t *alarm = wristflow_product_alarm_start();
    wristflow_product_ble_start();
    wristflow_watch_snapshot_t snapshot = read_snapshot(true);
    RT_ASSERT(littlevgl2rtt_init("lcd") == RT_EOK);
    RT_ASSERT(lv_display_get_horizontal_resolution(NULL) == 390);
    RT_ASSERT(lv_display_get_vertical_resolution(NULL) == 450);
    wristflow_ui_init("");
    rt_device_t lcd = rt_device_find("lcd");
    RT_ASSERT(lcd);
    lcd_on = true;
    lv_obj_t *initial = lv_screen_active();
    wristflow_ui_shell_t *shell = wristflow_product_ui_create_with_timers(&snapshot, &settings, set_brightness, lcd,
        &layout, wristflow_product_services_layout, wristflow_product_services_layout_status, NULL, countdown, alarm);
    RT_ASSERT(shell);
    product_shell = shell;
    wristflow_ui_shell_bind_settings_store(shell, wristflow_product_services_settings,
        wristflow_product_services_settings_status, NULL);
    wristflow_ui_shell_bind_notification_delete(shell, wristflow_product_ble_delete, NULL);
    if (motor_ready) wristflow_ui_shell_bind_vibration_preview(shell, vibration_preview, vibration_cancel, NULL);
    if (motor_ready) wristflow_ui_shell_bind_reminder_vibration(shell, reminder_vibration, reminder_vibration_cancel, NULL);
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
    if (!wristflow_product_power_start()) rt_kprintf("[power] service unavailable; display unknown\n");
    if (!wristflow_product_steps_start()) rt_kprintf("[steps] native daily service unavailable\n");
    sync_wrist_setting();
    if (!wristflow_product_charger_service_start()) rt_kprintf("[charger] control service unavailable; no manual start\n");
    rt_kprintf("[product] RTC=%s, %s, voltage_valid=%u VBAT=%umV, BLE active, PM auto when off\n",
               snapshot.time_unavailable ? "not synchronized" : "in range",
               wristflow_power_input_text(&snapshot.power), snapshot.power.voltage_valid,
               snapshot.power.millivolts);
    uint32_t sampled = lv_tick_get();
    for (;;) {
        uint32_t wait_ms = lv_timer_handler();
        uint32_t now = lv_tick_get();
        capture_battery();
        if (wristflow_ui_shell_display_phase(shell) == WRISTFLOW_DISPLAY_OFF) {
            capture_carousel();
            wristflow_ui_shell_sync_settings_save(shell);
            sync_wrist_setting();
            screen_off(lcd, touch, pointer);
            continue;
        }
        if (now - sampled >= 250) {
            sync_notifications();
            wristflow_watch_snapshot_t next = read_snapshot(false);
            wristflow_ui_shell_charging_event(shell, &next);
            lv_obj_t *active = lv_screen_active();
            if (active && lv_obj_get_name(active) && !strcmp(lv_obj_get_name(active), "screen_weather"))
                wristflow_weather_screen_refresh(active);
            /* RTC polling and settings collection must not redraw static pages. */
            if (next.hour_24 != snapshot.hour_24 || next.minute != snapshot.minute ||
                next.time_unavailable != snapshot.time_unavailable ||
                !wristflow_power_equal(&next.power, &snapshot.power) ||
                next.activity.metrics[WRISTFLOW_ACTIVITY_STEPS].total != snapshot.activity.metrics[WRISTFLOW_ACTIVITY_STEPS].total ||
                next.activity.metrics[WRISTFLOW_ACTIVITY_STEPS].total_valid != snapshot.activity.metrics[WRISTFLOW_ACTIVITY_STEPS].total_valid ||
                next.activity.metrics[WRISTFLOW_ACTIVITY_STEPS].hours_valid != snapshot.activity.metrics[WRISTFLOW_ACTIVITY_STEPS].hours_valid ||
                memcmp(next.activity.metrics[WRISTFLOW_ACTIVITY_STEPS].hourly,
                    snapshot.activity.metrics[WRISTFLOW_ACTIVITY_STEPS].hourly,
                    sizeof next.activity.metrics[WRISTFLOW_ACTIVITY_STEPS].hourly) ||
                next.uptime_seconds / 60 != snapshot.uptime_seconds / 60) {
                snapshot = next;
                wristflow_ui_shell_update(shell, &snapshot);
            }
            wristflow_ui_shell_sync_settings_save(shell);
            sync_wrist_setting();
            sampled = now;
            capture_carousel();
        }
        rt_uint32_t events = wristflow_product_event_wait(rt_tick_from_millisecond(LV_CLAMP(1, wait_ms, 20)));
        if (events & WF_EVENT_PM_SAMPLE) wristflow_product_pm_report();
        if ((events & WF_EVENT_WRIST) && wristflow_product_wrist_take_wake() &&
            !(events & (WF_EVENT_KEY | WF_EVENT_TEST_WAKE)))
            wristflow_ui_shell_wrist_wake(shell);
        if (events & (WF_EVENT_ALARM | WF_EVENT_COUNTDOWN)) wristflow_ui_shell_reminder_event(shell);
        if (events & WF_EVENT_CHARGER) {
            snapshot = read_snapshot(false);
            wristflow_ui_shell_charging_event(shell, &snapshot);
            wristflow_ui_shell_update(shell, &snapshot);
        }
        if (events & WF_EVENT_PHONE) {
            sync_notifications();
            snapshot = read_snapshot(false);
            wristflow_ui_shell_update(shell, &snapshot);
            lv_obj_t *active = lv_screen_active();
            if (active && lv_obj_get_name(active) && !strcmp(lv_obj_get_name(active), "screen_weather")) {
                wristflow_weather_screen_refresh(active);
            }
        }
        if (events & (WF_EVENT_KEY | WF_EVENT_TEST_WAKE))

            wristflow_ui_shell_key(shell);
    }
}
