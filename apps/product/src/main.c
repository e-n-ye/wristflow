#include <rtthread.h>
#include <rthw.h>
#include <rtdevice.h>
#include <board.h>
#include "button.h"
#include "littlevgl2rtt.h"
#include "wristflow_ui.h"
#include "product_ui.h"
#include "product_services.h"
#include "storage_notice.h"
#include "runtime_input.h"
#include "product_ble.h"
#include "product_pm.h"
#include "product_steps.h"
#include "product_wrist.h"
#include "product_light.h"
#include "product_compass.h"
#include "product_charger.h"
#include "product_power.h"
#include "battery_estimate.h"
#include "product_battery_trace.h"
#include "product_countdown.h"
#include "product_alarm.h"
#include "product_motor.h"
#include "weather_screen.h"
#include "aod_panel_probe.h"
#include "aod_display.h"
#include "aod_policy.h"
#include "aod_view.h"
#include "product_resource_probe.h"
#include "drv_lcd.h"
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
/* UI thread owns panel I/O. FinSH only reserves a request and copies status. */
static wf_aod_probe_t panel_probe, panel_probe_cache;
static unsigned probe_seconds;
static bool probe_reserved, probe_stop, probe_available;
static wf_aod_probe_input_t probe_input;
static wf_aod_display_t resident_cache;

static uint32_t panel_now(void *context)
{ (void)context; return (uint32_t)((uint64_t)rt_tick_get() * 1000U / RT_TICK_PER_SECOND); }

static void panel_publish(const wf_aod_probe_t *value, void *context)
{
    (void)context;
    rt_base_t level = rt_hw_interrupt_disable();
    panel_probe_cache = *value;
    rt_hw_interrupt_enable(level);
}

static void panel_unavailable(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    probe_available = false;
    /* A request accepted just before OFF must not wait for a later wake. */
    if (probe_seconds) {
        probe_seconds = 0; probe_stop = false; probe_reserved = false;
        panel_probe_cache = (wf_aod_probe_t){.phase = WF_AOD_PROBE_REJECTED};
    }
    rt_hw_interrupt_enable(level);
}

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
    bool flashlight=navigation && navigation->surface==WRISTFLOW_SURFACE_FLASHLIGHT &&
        !(record.context.flags&(WF_BATTERY_CHARGING_OVERLAY|WF_BATTERY_REMINDER_OVERLAY)) &&
        !wristflow_ui_shell_notification_active(product_shell) && brightness_valid && acknowledged_brightness==100;
    rt_base_t runtime_level=rt_hw_interrupt_disable();
    bool diagnostic=probe_reserved;
    rt_hw_interrupt_enable(runtime_level);
    wf_product_runtime_capture(&record,resident_cache.phase==WF_AOD_PANEL_RESIDENT,flashlight,diagnostic);
}

static void capture_carousel(void)
{
    wristflow_carousel_snapshot_t value;
    if (!wristflow_ui_shell_carousel_snapshot(product_shell, &value)) return;
    /* LVGL reads above run only on the UI thread; publish bounded copies. */
    rt_base_t level = rt_hw_interrupt_disable();
    carousel_current = value;
    carousel_valid = true;
    probe_available = value.settled && value.surface == WRISTFLOW_SURFACE_HOME && value.page == 0 &&
        value.display_phase == WRISTFLOW_DISPLAY_ACTIVE && !wristflow_ui_shell_endurance(product_shell) &&
        !wristflow_ui_shell_charging_active(product_shell) && !wristflow_ui_shell_reminder_active(product_shell);
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

static int wf_aod(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "status")) {
        rt_base_t level = rt_hw_interrupt_disable();
        wf_aod_probe_t p = panel_probe_cache;
        wf_aod_display_t resident = resident_cache;
        bool reserved = probe_reserved, available = probe_available;
        unsigned queued = probe_seconds;
        rt_hw_interrupt_enable(level);
        rt_kprintf("[aod-probe] phase=%u reserved=%u queued=%u available=%u valid=%u restore_pending=%u restored=%u\n",
            p.phase, reserved, queued, available, p.valid, p.restore_pending, p.restored);
        rt_kprintf("[aod-probe] start=%lu elapsed=%lu duration=%lu events=0x%lx\n",
            (unsigned long)p.started_ms, (unsigned long)p.elapsed_ms,
            (unsigned long)p.duration_ms, (unsigned long)p.interrupted_events);
        rt_kprintf("[aod-probe] SDK-cache mode/brightness before=%u/%u entered=%u/%u after=%u/%u\n",
            p.before.mode, p.before.brightness, p.entered.mode, p.entered.brightness, p.after.mode, p.after.brightness);
        rt_kprintf("[aod-display] phase=%u owned=%u failed=%u minute=%lu; physical panel state, IDLE retained\n",
            resident.phase, resident.owned, resident.failed, (unsigned long)resident.frame_minute);
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "stop")) {
        rt_base_t level = rt_hw_interrupt_disable();
        if (probe_reserved) probe_stop = true;
        rt_hw_interrupt_enable(level);
        wristflow_product_event_send(WF_EVENT_AOD_PROBE);
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "start") && strlen(argv[2]) == 2 &&
        argv[2][0] >= '1' && argv[2][0] <= '6' && argv[2][1] >= '0' && argv[2][1] <= '9') {
        unsigned seconds = (unsigned)(argv[2][0] - '0') * 10U + (unsigned)(argv[2][1] - '0');
        rt_base_t level = rt_hw_interrupt_disable();
        bool accepted = seconds <= 60 && probe_available && !probe_reserved;
        accepted = accepted && !wf_product_resource_busy_locked();
        if (accepted) { probe_reserved = true; probe_seconds = seconds; probe_stop = false; }
        rt_hw_interrupt_enable(level);
        rt_kprintf("[aod-probe] start accepted=%u; diagnostic only, 5 percent request, UI holds IDLE\n", accepted);
        if (accepted) wristflow_product_event_send(WF_EVENT_AOD_PROBE);
        return accepted ? 0 : -1;
    }
    rt_kprintf("wf_aod status | start <10..60> | stop; requires settled HOME, awake, no overlays/endurance\n");
    return -1;
}
MSH_CMD_EXPORT(wf_aod, Bounded CO5300 Idle panel diagnostic);
#endif

static volatile bool endurance_key_enabled, key_down, key_mode_press;
static volatile uint32_t key_pressed_tick, key_press_id;
static wristflow_input_observation_t input_observation(void *context)
{
    (void)context;
    wristflow_power_snapshot_t cached = {0};
    wristflow_product_charger_display(&cached);
    return cached.input_observation;
}
static void sync_wrist_setting(void)
{
    wristflow_settings_t settings;
    if (wristflow_ui_shell_get_settings(product_shell, &settings) &&
        (!wrist_setting_known || (settings.wrist_wake && !settings.endurance_mode) != wrist_setting)) {
        wrist_setting_known = true;
        wrist_setting = settings.wrist_wake && !settings.endurance_mode;
        wristflow_product_wrist_enable(wrist_setting);
    }
}
static bool automatic_brightness(void *context)
{ (void)context; return wristflow_product_light_snapshot().enabled; }
static bool request_automatic_brightness(bool enabled, void *context)
{
    bool accepted = wristflow_product_light_enable(enabled);
    if (accepted) wristflow_ui_shell_auto_brightness_sample(context, false, 0);
    return accepted;
}
static void sync_automatic_brightness(void)
{
    wristflow_product_light_active(wristflow_ui_shell_auto_brightness_sampling(product_shell));
    wristflow_product_light_snapshot_t sample = wristflow_product_light_snapshot();
    wristflow_ui_shell_auto_brightness_sample(product_shell, sample.valid, sample.brightness);
}

static void sync_compass(void)
{
    bool active = wristflow_ui_shell_compass_sampling(product_shell);
    bool accepted = wristflow_product_compass_active(active);
    wf_compass_snapshot_t value = wristflow_product_compass_snapshot();
    if (active && !accepted) { value.valid = false; value.reason = WF_COMPASS_BUSY; }
    wristflow_ui_shell_compass_sample(product_shell, &value);
}
static void sync_endurance_services(void)
{
    bool enabled = wristflow_ui_shell_endurance(product_shell);
    endurance_key_enabled = enabled;
    wristflow_product_ble_enable(!enabled);
    sync_wrist_setting();
    if (enabled) {
        wf_radio_policy_t radio = wristflow_product_ble_radio();
        wristflow_ui_shell_endurance_services(product_shell,
            wf_radio_suspended(&radio) && wristflow_product_wrist_suspended(),
            radio.failed || wristflow_product_wrist_failed());
    }
}
static void sync_endurance_key(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    bool down = key_down && key_mode_press;
    uint32_t id = key_press_id, since = key_pressed_tick;
    rt_hw_interrupt_enable(level);
    uint32_t elapsed = (uint32_t)((uint64_t)(rt_tick_get() - since) * 1000U / RT_TICK_PER_SECOND);
    wristflow_ui_shell_endurance_key(product_shell, down, id, elapsed);
}

static wristflow_watch_snapshot_t read_snapshot(bool force_power)
{
    if (!waiting_for_wake && (force_power || rt_tick_get() - power_sampled >= rt_tick_from_millisecond(5000))) {
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
    if (action == BUTTON_PRESSED) {
        key_down = true; key_pressed_tick = rt_tick_get(); ++key_press_id;
        key_mode_press = endurance_key_enabled;
        if (key_mode_press) wristflow_product_event_send(WF_EVENT_KEY_STATE);
    }
    rt_base_t level = rt_hw_interrupt_disable();
    bool pass = wf_aod_probe_filter_key(&probe_input, action == BUTTON_PRESSED, action == BUTTON_RELEASED);
    rt_hw_interrupt_enable(level);
    if (!pass) {
        if (action == BUTTON_PRESSED) wristflow_product_event_send(WF_EVENT_AOD_PROBE);
        if (action == BUTTON_RELEASED) key_down = false;
        return;
    }
    if (action == BUTTON_PRESSED && waiting_for_wake) {
        suppress_wake_click = true;
        wristflow_product_event_send(WF_EVENT_KEY);
    } else if (action == BUTTON_CLICKED && !suppress_wake_click && !key_mode_press) {
        wristflow_product_event_send(WF_EVENT_KEY);
    } else if (action == BUTTON_RELEASED) {
        key_down = false;
        if (key_mode_press) wristflow_product_event_send(WF_EVENT_KEY_STATE);
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

static bool panel_observe(wf_panel_observation_t *out, void *context)
{
    LCD_DrvStatusTypeDef state = LCD_STATUS_NONE;
    uint8_t brightness = 255;
    if (rt_device_control(context, RTGRAPHIC_CTRL_GET_STATE, &state) != RT_EOK ||
        rt_device_control(context, RTGRAPHIC_CTRL_GET_BRIGHTNESS, &brightness) != RT_EOK) return false;
    *out = (wf_panel_observation_t){.brightness = brightness,
        .mode = state == LCD_STATUS_IDLE_MODE ? WF_PANEL_IDLE :
            state == LCD_STATUS_DISPLAY_ON ? WF_PANEL_NORMAL : WF_PANEL_OTHER};
    return true;
}
static bool panel_brightness(uint8_t value, void *context)
{
    requested_brightness = value;
    bool ok = rt_device_control(context, RTGRAPHIC_CTRL_SET_BRIGHTNESS, &value) == RT_EOK;
    brightness_valid = ok;
    if (ok) acknowledged_brightness = value;
    return ok;
}
static bool panel_idle(bool enabled, void *context)
{
    uint8_t mode = enabled;
    return rt_device_control(context, RTGRAPHIC_CTRL_SET_MODE, &mode) == RT_EOK;
}
static uint32_t panel_wait(uint32_t remaining, bool *cancelled, void *context)
{
    (void)context;
    rt_base_t level = rt_hw_interrupt_disable();
    *cancelled = probe_stop || (remaining && probe_input.cancel);
    probe_stop = false;
    probe_input.cancel = false;
    rt_hw_interrupt_enable(level);
    if (*cancelled) return 0;
    rt_int32_t timeout = remaining ? (rt_int32_t)rt_tick_from_millisecond(remaining) : RT_WAITING_FOREVER;
    uint32_t events = wristflow_product_event_wait(timeout);
    level = rt_hw_interrupt_disable();
    *cancelled = probe_stop || (remaining && probe_input.cancel);
    probe_stop = false;
    probe_input.cancel = false;
    rt_hw_interrupt_enable(level);
    return events & ~WF_EVENT_AOD_PROBE;
}
static bool panel_drain(rt_device_t lcd)
{
    uint32_t start = panel_now(NULL);
    for (;;) {
        bool busy = true;
        if (rt_device_control(lcd, RTGRAPHIC_CTRL_GET_BUSY, &busy) != RT_EOK) return false;
        if (!busy) return true;
        if (panel_now(NULL) - start >= 1000U) return false;
        rt_thread_mdelay(5);
    }
}

typedef struct {
    rt_device_t lcd;
    lv_obj_t *cover;
    wristflow_watch_snapshot_t snapshot;
    wristflow_aod_style_t style;
} resident_context_t;
static bool resident_observe(wf_panel_observation_t *value, void *context)
{ return panel_observe(value, ((resident_context_t *)context)->lcd); }
static bool resident_brightness(uint8_t value, void *context)
{ return panel_brightness(value, ((resident_context_t *)context)->lcd); }
static bool resident_idle(bool value, void *context)
{ return panel_idle(value, ((resident_context_t *)context)->lcd); }
static void resident_hide(void *context)
{
    resident_context_t *resident = context;
    if (resident->cover) lv_obj_add_flag(resident->cover, LV_OBJ_FLAG_HIDDEN);
}
static bool resident_draw(void *context)
{
    resident_context_t *resident = context;
    if (!resident->cover) resident->cover = aod_content_create(lv_layer_top(), false, true);
    lv_obj_remove_flag(resident->cover, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(resident->cover);
    wristflow_aod_render(resident->cover, &resident->snapshot, resident->style,
        resident->snapshot.utc_seconds / 60U % 4U);
    lv_refr_now(NULL);
    return panel_drain(resident->lcd);
}
static const wf_aod_display_io_t resident_io = {
    resident_observe, resident_brightness, resident_idle, resident_draw, resident_hide
};
static void resident_publish(const wf_aod_display_t *state)
{
    rt_base_t level = rt_hw_interrupt_disable();
    resident_cache = *state;
    rt_hw_interrupt_enable(level);
    capture_battery(); /* Actual resident/restore result, after releasing the IRQ guard. */
}
static uint32_t resident_restore(wf_aod_display_t *state, resident_context_t *context)
{
    uint32_t deferred = 0;
    while (state->owned) {
        if (state->phase == WF_AOD_PANEL_FAULT) {
            resident_publish(state);
            rt_kprintf("[aod-display] restore failed; LCD retained, next KEY retries\n");
            uint32_t events;
            do {
                events = wristflow_product_event_wait(RT_WAITING_FOREVER);
                deferred |= events;
            } while (!(events & (WF_EVENT_KEY | WF_EVENT_TEST_WAKE)));
        }
        if (wf_aod_display_restore(state, &resident_io, context)) break;
    }
    resident_publish(state);
    return deferred;
}

static void run_panel_probe(rt_device_t lcd, lv_indev_t *pointer)
{
    rt_base_t level = rt_hw_interrupt_disable();
    unsigned seconds = probe_seconds;
    bool stop = probe_stop;
    probe_seconds = 0;
    probe_stop = false;
    rt_hw_interrupt_enable(level);
    if (!seconds) return;
    wristflow_carousel_snapshot_t current;
    bool eligible = wristflow_ui_shell_carousel_snapshot(product_shell, &current) && current.settled &&
        current.surface == WRISTFLOW_SURFACE_HOME && current.page == 0 &&
        current.display_phase == WRISTFLOW_DISPLAY_ACTIVE &&
        !wristflow_ui_shell_endurance(product_shell) && !wristflow_ui_shell_charging_active(product_shell) &&
        !wristflow_ui_shell_reminder_active(product_shell);
    if (stop || !eligible) {
        panel_probe = (wf_aod_probe_t){.phase = stop ? WF_AOD_PROBE_CANCELLED : WF_AOD_PROBE_REJECTED};
        panel_publish(&panel_probe, NULL);
    } else {
        const wf_aod_probe_io_t io = {panel_now, panel_observe, panel_brightness, panel_idle, panel_wait, panel_publish};
        level = rt_hw_interrupt_disable();
        probe_input.active = true; probe_input.cancel = false;
        rt_hw_interrupt_enable(level);
        capture_battery(); /* Diagnostic residency is explicitly unclassified. */
        lv_timer_enable(false);
        if (panel_drain(lcd)) {
            wf_aod_probe_run(&panel_probe, &io, lcd, seconds);
            if (panel_probe.restore_pending) {
                rt_kprintf("[aod-probe] RESTORE FAILED: UI frozen, LCD exclusively owned; wf_aod stop retries\n");
                wf_aod_probe_await_restore(&panel_probe, &io, lcd);
            }
        } else {
            panel_probe = (wf_aod_probe_t){.phase = WF_AOD_PROBE_REJECTED};
            panel_publish(&panel_probe, NULL);
        }
        level = rt_hw_interrupt_disable();
        probe_input.active = false; probe_input.cancel = false;
        rt_hw_interrupt_enable(level);
        lv_indev_reset(pointer, NULL);
        lv_indev_wait_release(pointer);
        lv_timer_enable(true);
        wristflow_ui_shell_display_retry(product_shell);
        if (panel_probe.restored) {
            lv_obj_invalidate(lv_screen_active());
            lv_obj_invalidate(lv_layer_top());
        }
        /* One KEY cancels the diagnostic. All other service events are handed
         * back to normal arbitration; a mode/brightness error never eats them. */
        uint32_t replay = panel_probe.interrupted_events &
            ~(WF_EVENT_KEY | WF_EVENT_TEST_WAKE | WF_EVENT_KEY_STATE);
        if (replay) wristflow_product_event_send(replay);
    }
    level = rt_hw_interrupt_disable();
    probe_reserved = panel_probe.restore_pending;
    probe_stop = false;
    probe_available = false;
    rt_hw_interrupt_enable(level);
    rt_kprintf("[aod-probe] ended phase=%u restore_pending=%u restored=%u; SDK cache only\n",
        panel_probe.phase, panel_probe.restore_pending, panel_probe.restored);
}

static void screen_off(rt_device_t lcd, rt_device_t touch, lv_indev_t *pointer)
{
    wristflow_product_compass_active(false);
    wf_compass_snapshot_t compass = wristflow_product_compass_snapshot();
    wristflow_ui_shell_compass_sample(product_shell, &compass);
    wristflow_product_light_active(false);
    wristflow_ui_shell_auto_brightness_sample(product_shell, false, 0);
    panel_unavailable();
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
    /* Touch power work is asynchronous and holds its own SDK idle request. */
    rt_kprintf("[product] display sleeping; notification events active\n");
    wristflow_product_power_screen(false);
    wristflow_product_steps_screen(false);
    static resident_context_t resident;
    resident.lcd = lcd;
    wf_aod_display_t aod = {0};
    uint32_t power_minute = UINT32_MAX;
    bool power_updated = false;
    bool rendered_battery_unknown = true, rendered_estimate = false;
    uint8_t rendered_battery = 0;
    rt_uint32_t events;
    bool preview = false, reminder = false, charging = false, wrist = false;
    defer_brightness = true;
    for (;;) {
        wristflow_settings_t settings;
        wristflow_ui_shell_get_settings(product_shell, &settings);
        resident.snapshot = read_snapshot(false);
        wristflow_alarm_clock_t clock = wristflow_product_services_time();
        wristflow_watch_snapshot_t wall = wristflow_product_snapshot(clock.valid, clock.utc);
        resident.snapshot.time_unavailable = wall.time_unavailable;
        resident.snapshot.hour_24 = wall.hour_24;
        resident.snapshot.minute = wall.minute;
        resident.snapshot.utc_seconds = wall.utc_seconds;
        resident.style = settings.aod_style;
        events = 0;
        bool show_aod = !aod.failed && wristflow_settings_aod_active(&settings, &resident.snapshot);
        /* Only the corner style consumes battery data. Neither this request
         * nor its POWER completion creates a timer or another request. */
        bool needs_power = show_aod && settings.aod_style == WRISTFLOW_AOD_CORNER;
        wristflow_product_power_aod(needs_power);
        uint32_t minute = resident.snapshot.utc_seconds / 60U;
        if (!needs_power) power_minute = UINT32_MAX;
        else if (power_minute != minute) {
            power_minute = minute;
            rt_kprintf("[aod-power] request minute=%lu\n", (unsigned long)minute);
            wristflow_product_power_request();
        }
        if (show_aod) {
            wristflow_product_pm_screen(false); /* AOD retains the known LCD IDLE guard. */
            if (!lcd_on) {
                RT_ASSERT(rt_device_control(lcd, RTGRAPHIC_CTRL_POWERON, NULL) == RT_EOK);
                lcd_on = true;
            }
            bool content_changed = needs_power && power_updated &&
                (rendered_battery_unknown != resident.snapshot.battery_unavailable ||
                 rendered_battery != resident.snapshot.battery_percent ||
                 rendered_estimate != resident.snapshot.power.estimate_valid);
            bool changed = aod.phase != WF_AOD_PANEL_RESIDENT || aod.frame_minute != minute || content_changed;
            if (!wf_aod_display_frame(&aod, &resident_io, &resident, minute, content_changed)) {
                rt_kprintf("[aod-display] frame failed; AOD disabled for this sleep session\n");
                wristflow_product_power_aod(false);
                if (aod.owned) events |= resident_restore(&aod, &resident);
                show_aod = false;
            } else if (changed) {
                rendered_battery_unknown = resident.snapshot.battery_unavailable;
                rendered_battery = resident.snapshot.battery_percent;
                rendered_estimate = resident.snapshot.power.estimate_valid;
                rt_kprintf("[aod-display] minute=%lu style=%u brightness=%u battery_valid=%u percent=%u; IDLE retained\n",
                    (unsigned long)minute, settings.aod_style, WF_AOD_BRIGHTNESS,
                    !resident.snapshot.battery_unavailable, resident.snapshot.battery_percent);
            }
            resident_publish(&aod);
        }
        if (!show_aod) {
            wristflow_product_power_aod(false);
            if (aod.owned) events |= resident_restore(&aod, &resident);
            if (lcd_on) {
                RT_ASSERT(rt_device_control(lcd, RTGRAPHIC_CTRL_POWEROFF, NULL) == RT_EOK);
                lcd_on = false;
                rt_kprintf("[product] screen off; notification events active\n");
            }
            wristflow_product_pm_screen(true);
        }
        capture_battery();
        power_updated = false;
        clock = wristflow_product_services_time();
        wall = wristflow_product_snapshot(clock.valid, clock.utc);
        uint32_t wait_ms = aod.failed ? UINT32_MAX : wristflow_aod_wait_ms(&settings, &wall, clock.subsecond_ms);
        if (wall.time_unavailable != resident.snapshot.time_unavailable ||
            wall.utc_seconds / 60U != resident.snapshot.utc_seconds / 60U) wait_ms = 1;
        if (!events) events = wristflow_product_event_wait(wait_ms == UINT32_MAX ? RT_WAITING_FOREVER :
            (rt_int32_t)rt_tick_from_millisecond(wait_ms));
arbitrate_sleep_events:
        /* Rebuild from service state on every wake: event bits can coalesce.
         * An acknowledged reminder stays pending without repeatedly waking. */
        reminder = wristflow_ui_shell_reminder_event(product_shell) || reminder;
        wristflow_watch_snapshot_t charge_snapshot = read_snapshot(false);
        bool endurance_before = wristflow_ui_shell_endurance(product_shell);
        charging = wristflow_ui_shell_charging_event(product_shell, &charge_snapshot) || charging;
        wristflow_ui_shell_update(product_shell, &charge_snapshot);
        sync_endurance_services();
        if (events & WF_EVENT_PHONE) {
            wristflow_product_pm_background();
            preview = sync_notifications() || preview;
        }
        if (events & WF_EVENT_PM_SAMPLE) wristflow_product_pm_report();
        if (events & WF_EVENT_POWER) power_updated = true;
        /* KEY wins a coalesced batch: waking by wrist first would make the
         * same KEY look like an ACTIVE-screen navigation press. */
        if ((events & WF_EVENT_WRIST) && wristflow_product_wrist_take_wake() &&
            !(events & (WF_EVENT_KEY | WF_EVENT_TEST_WAKE | WF_EVENT_KEY_STATE)))
            wrist = wristflow_ui_shell_wrist_wake(product_shell) || wrist;
        if (reminder || preview || charging || wrist ||
            (endurance_before && !wristflow_ui_shell_endurance(product_shell)) ||
            (events & (WF_EVENT_KEY | WF_EVENT_TEST_WAKE)) ||
            ((events & WF_EVENT_KEY_STATE) && key_down && wristflow_ui_shell_endurance(product_shell))) {
            wristflow_product_power_aod(false); /* Cancel before any recovery wait. */
            if (aod.owned) {
                uint32_t deferred = resident_restore(&aod, &resident);
                if (deferred) {
                    events |= deferred;
                    /* Recovery can consume an alarm/phone/charger event. Rebuild
                     * authority before choosing the first visible normal frame. */
                    goto arbitrate_sleep_events;
                }
            }
            break;
        }
    }
    wristflow_product_pm_screen(false); /* Hold idle before restoring the display. */
    waiting_for_wake = false;
    rt_kprintf("[product] wake event=%s\n", events & WF_EVENT_KEY ? "KEY1" : reminder ? "reminder" : preview ? "notification" : charging ? "charging" : wrist ? "wrist" : "diagnostic");
    wristflow_product_pm_report();
    /* Select the wake destination while dark; expose only its completed frame. */
    defer_brightness = true;
    if (!charging && !wrist && (events & (WF_EVENT_KEY | WF_EVENT_TEST_WAKE))) wristflow_ui_shell_key(product_shell);
    if (wristflow_ui_shell_endurance(product_shell)) sync_endurance_key();
    wristflow_watch_snapshot_t wake_snapshot = read_snapshot(false);
    wristflow_ui_shell_update(product_shell, &wake_snapshot);
    if (!lcd_on) RT_ASSERT(rt_device_control(lcd, RTGRAPHIC_CTRL_POWERON, NULL) == RT_EOK);
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
    wristflow_product_ble_start(!settings.endurance_mode);
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
    wristflow_ui_shell_bind_input_observation(shell, input_observation, NULL);
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
    wf_product_resource_init(shell, pointer, &probe_reserved);
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
    else {
        const wristflow_brightness_mode_port_t light_port = {automatic_brightness, request_automatic_brightness, shell};
        (void)wristflow_product_light_enable(settings.auto_brightness);
        wristflow_ui_shell_bind_brightness_mode(shell, &light_port);
    }
    sync_wrist_setting();
    sync_endurance_services();
    if (!wristflow_product_charger_service_start()) rt_kprintf("[charger] control service unavailable; no manual start\n");
    rt_kprintf("[product] RTC=%s, %s, voltage_valid=%u VBAT=%umV, BLE active, PM auto when off\n",
               snapshot.time_unavailable ? "not synchronized" : "in range",
               wristflow_power_input_text(&snapshot.power), snapshot.power.voltage_valid,
               snapshot.power.millivolts);
    uint32_t sampled = lv_tick_get();
    for (;;) {
        /* The bounded panel diagnostic owns LCD state while this loop blocks. */
        rt_base_t light_level = rt_hw_interrupt_disable();
        bool probing = probe_reserved;
        rt_hw_interrupt_enable(light_level);
        if (probing) {
            wristflow_product_compass_active(false);
            wristflow_product_light_active(false);
            wristflow_ui_shell_auto_brightness_sample(shell, false, 0);
        }
        run_panel_probe(lcd, pointer);
        if (wristflow_ui_shell_endurance(shell)) sync_endurance_key();
        sync_endurance_services();
        sync_automatic_brightness();
        sync_compass();
        wf_product_storage_notice_poll();
        uint32_t wait_ms = lv_timer_handler();
        wf_product_resource_poll();
        sync_compass();
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
            /* Evaluate scheduled DND using fresh wall time before any alert. */
            sync_notifications();
            wristflow_ui_shell_sync_settings_save(shell);
            sync_wrist_setting();
            sampled = now;
            capture_carousel();
        }
        rt_uint32_t events = wristflow_product_event_wait(rt_tick_from_millisecond(LV_CLAMP(1, wait_ms, 20)));
        events = wf_product_resource_events(events);
        if (events & WF_EVENT_PM_SAMPLE) wristflow_product_pm_report();
        if ((events & WF_EVENT_WRIST) && wristflow_product_wrist_take_wake() &&
            !(events & (WF_EVENT_KEY | WF_EVENT_TEST_WAKE | WF_EVENT_KEY_STATE)))
            wristflow_ui_shell_wrist_wake(shell);
        if (events & (WF_EVENT_ALARM | WF_EVENT_COUNTDOWN)) wristflow_ui_shell_reminder_event(shell);
        if (events & WF_EVENT_CHARGER) {
            snapshot = read_snapshot(false);
            wristflow_ui_shell_charging_event(shell, &snapshot);
            wristflow_ui_shell_update(shell, &snapshot);
        }
        if (events & WF_EVENT_PHONE) {
            snapshot = read_snapshot(false);
            wristflow_ui_shell_update(shell, &snapshot);
            sync_notifications();
            lv_obj_t *active = lv_screen_active();
            if (active && lv_obj_get_name(active) && !strcmp(lv_obj_get_name(active), "screen_weather")) {
                wristflow_weather_screen_refresh(active);
            }
        }
        if (events & (WF_EVENT_KEY | WF_EVENT_TEST_WAKE)) wristflow_ui_shell_key(shell);
        if (events & WF_EVENT_KEY_STATE) sync_endurance_key();
    }
}
