#include "product_ui.h"
#include "wristflow_ui.h"
#include "src/misc/lv_timer_private.h" /* Fault injection: stop only the animation timer. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

static wristflow_ui_shell_t *shell;
static uint32_t ticks;
static unsigned brightness;
static uint8_t draw[390 * 40 * 3], frame[390 * 450 * 3];
static lv_indev_data_t pointer;
static wristflow_countdown_t countdown;
static wristflow_alarm_snapshot_t alarms;
static const char *renders;
static uint32_t tick(void) { return ticks; }
static uint32_t now(void *context) { (void)context; return ticks; }
static uint64_t now64(void *context) { return now(context); }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p)
{
    unsigned width = a->x2 - a->x1 + 1;
    for (int y = a->y1; y <= a->y2; ++y)
        memcpy(frame + (y * 390 + a->x1) * 3, p + (y - a->y1) * width * 3, width * 3);
    lv_display_flush_ready(d);
}
static void bright(uint8_t value, void *context) { (void)context; brightness = value; }
static void read_pointer(lv_indev_t *input, lv_indev_data_t *data)
{
    (void)input; *data = pointer;
    if (!wristflow_ui_shell_filter_touch(shell, data->state == LV_INDEV_STATE_PRESSED))
        data->state = LV_INDEV_STATE_RELEASED;
}
static void read_countdown(void *context, wristflow_countdown_t *copy) { (void)context; *copy = countdown; }
static void sync_countdown(void *context) { wristflow_countdown_update(&countdown, now(context)); }
static bool command(void *context, wristflow_countdown_command_t op, uint32_t arg)
{
    switch (op) {
    case WF_COUNTDOWN_START: return wristflow_countdown_start(&countdown, arg, now(context));
    case WF_COUNTDOWN_ACK: return arg == countdown.generation && wristflow_countdown_take_expiry(&countdown);
    case WF_COUNTDOWN_DISMISS: case WF_COUNTDOWN_CANCEL: wristflow_countdown_cancel(&countdown); return true;
    case WF_COUNTDOWN_REPEAT: return wristflow_countdown_repeat(&countdown, now(context));
    default: return false;
    }
}
static void read_alarm(void *context, wristflow_alarm_snapshot_t *copy) { (void)context; *copy = alarms; }
static void sync_alarm(void *context) { (void)context; }
static uint32_t request(void *context, const wristflow_alarm_command_t *cmd) { (void)context; (void)cmd; return 0; }
static wristflow_save_state_t status(void *context, uint32_t id) { (void)context; (void)id; return WRISTFLOW_SAVE_FAILED; }
static bool ack(void *context, uint32_t id, uint32_t sequence)
{
    (void)context;
    for (unsigned i = 0; i < 5; ++i) if (alarms.pending[i].active &&
        alarms.pending[i].alarm.id == id && alarms.pending[i].sequence == sequence) {
        alarms.pending[i].active = false; return true;
    }
    return false;
}
static const wristflow_countdown_port_t countdown_port = {
    .command = command, .read = read_countdown, .sync = sync_countdown, .now = now, .now64 = now64};
static const wristflow_alarm_port_t alarm_port = {
    .request = request, .status = status, .read = read_alarm, .ack = ack, .sync = sync_alarm, .now64 = now64};
static void advance(unsigned ms)
{
    for (unsigned i = 0; i < ms; i += 16) { ticks += 16; lv_timer_handler(); }
    lv_obj_update_layout(lv_screen_active()); lv_obj_update_layout(lv_layer_top());
}
static void jump(unsigned ms) { ticks += ms; lv_timer_handler(); advance(64); }
static lv_obj_t *named(lv_obj_t *root, const char *name)
{ lv_obj_t *obj = lv_obj_find_by_name(root, name); assert(obj); return obj; }
static bool visible(const char *name) { return lv_obj_is_visible(named(lv_screen_active(), name)); }
static void text(const char *name, const char *expected)
{ assert(!strcmp(lv_label_get_text(named(lv_screen_active(), name)), expected)); }
static void press_in(lv_obj_t *root, const char *name)
{
    lv_obj_t *obj = named(root, name);
    if (!lv_obj_is_visible(obj)) fprintf(stderr, "hidden press %s at tick=%u surface=%u\n", name, ticks,
        (unsigned)wristflow_ui_shell_navigation(shell)->surface);
    assert(lv_obj_is_visible(obj));
    lv_obj_update_layout(root); lv_area_t a; lv_obj_get_coords(obj, &a);
    pointer.point = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2};
    assert(pointer.point.x >= 0 && pointer.point.x < 390 && pointer.point.y >= 0 && pointer.point.y < 450);
    pointer.state = LV_INDEV_STATE_PRESSED; advance(48);
    pointer.state = LV_INDEV_STATE_RELEASED; advance(48); advance(240);
}
static void press(const char *name) { press_in(lv_screen_active(), name); }
static void choose(unsigned minutes, bool confirm)
{
    press("breathing_duration");
    lv_obj_t *wheel = named(lv_screen_active(), "breathing_minutes");
    lv_roller_set_selected(wheel, minutes - 1, LV_ANIM_OFF);
    lv_obj_send_event(wheel, LV_EVENT_VALUE_CHANGED, NULL);
    if (confirm) press("breathing_duration_confirm");
    else { assert(wristflow_ui_shell_back(shell)); advance(64); }
}
static wristflow_settings_t settings(void)
{ wristflow_settings_t copy; assert(wristflow_ui_shell_get_settings(shell, &copy)); return copy; }
static void open_breathing(void)
{
    if (wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_HOME) {
        assert(wristflow_ui_shell_key(shell)); advance(320);
    }
    assert(wristflow_ui_shell_open(shell, WRISTFLOW_SURFACE_BREATHING)); advance(320);
    assert(visible("breathing_home_panel"));
}
static void create(const wristflow_settings_t *saved)
{
    wristflow_watch_snapshot_t snapshot = {.time_unavailable = true, .battery_unavailable = true, .metrics_unavailable = true};
    shell = wristflow_product_ui_create_with_timers(&snapshot, saved, bright, NULL, NULL, NULL, NULL, NULL,
        &countdown_port, &alarm_port);
    assert(shell); advance(320); wristflow_ui_shell_enable_display_policy(shell);
}
static void capture(const char *name)
{
    lv_obj_invalidate(lv_screen_active()); lv_obj_invalidate(lv_layer_top()); lv_refr_now(NULL);
    char path[1024]; snprintf(path, sizeof path, "%s/breathing_product_%s.ppm", renders, name);
    FILE *file = fopen(path, "wb"); assert(file); fprintf(file, "P6\n390 450\n255\n");
    for (unsigned i = 0; i < 390 * 450; ++i) {
        uint8_t rgb[] = {frame[i * 3 + 2], frame[i * 3 + 1], frame[i * 3]};
        assert(fwrite(rgb, 1, 3, file) == 3);
    }
    assert(!fclose(file));
}
static lv_obj_t *current_alarm(void)
{
    lv_obj_t *host = named(lv_layer_top(), "reminder_host");
    for (unsigned i = 0; i < lv_obj_get_child_count(host); ++i) {
        lv_obj_t *child = lv_obj_get_child(host, i);
        if (lv_obj_is_visible(child) && lv_obj_find_by_name(child, "alarm_reminder_close")) return child;
    }
    return NULL;
}
static void queue_reminders(unsigned count, bool timer)
{
    for (unsigned i = 0; i < count; ++i) alarms.pending[i] = (wristflow_alarm_pending_t){
        .alarm = alarms.alarms.entries[i], .sequence = ticks + i + 1, .due_ms = ticks, .active = true};
    if (timer) { assert(wristflow_countdown_start(&countdown, 1, ticks)); countdown.remaining_ms = 1; ++ticks; }
    assert(wristflow_ui_shell_reminder_event(shell)); advance(64);
    assert(wristflow_ui_shell_reminder_active(shell));
}
static void close_reminders(unsigned count, bool timer)
{
    for (unsigned i = 0; i < count; ++i) {
        lv_obj_t *alarm = current_alarm(); assert(alarm);
        press_in(alarm, "alarm_reminder_close");
        if (i + 1 < count || timer) assert(wristflow_ui_shell_reminder_active(shell));
    }
    if (timer) press_in(named(lv_layer_top(), "countdown_reminder"), "countdown_close");
    assert(!wristflow_ui_shell_reminder_active(shell));
}
static unsigned timers(void)
{ unsigned count = 0; for (lv_timer_t *t = lv_timer_get_next(NULL); t; t = lv_timer_get_next(t)) ++count; return count; }

int main(int argc, char **argv)
{
    assert(argc == 2); renders = argv[1]; lv_init(); lv_tick_set_cb(tick);
    lv_display_t *display = lv_display_create(390, 450); lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(display, draw, NULL, sizeof draw, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush); wristflow_ui_init("");
    lv_indev_t *input = lv_indev_create(); lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input, read_pointer); lv_timer_set_period(lv_indev_get_read_timer(input), 16);
    unsigned baseline = timers();
    wristflow_countdown_init(&countdown);
    wristflow_alarms_init(&alarms.alarms);
    for (unsigned i = 0; i < 5; ++i) {
        wristflow_alarm_t draft = {.hour = 7, .enabled = true, .weekdays = WRISTFLOW_ALARM_DAILY};
        assert(wristflow_alarm_add(&alarms.alarms, &draft, NULL));
    }
    wristflow_settings_t saved = wristflow_settings_default(); saved.brightness = 27; saved.screen_timeout = 5;
    create(&saved); open_breathing(); text("breathing_duration_text", "1分钟");
    choose(5, false); assert(settings().breathing_minutes == 1);
    choose(4, true); assert(settings().breathing_minutes == 4);
    saved = settings(); uint8_t record[WRISTFLOW_SETTINGS_BYTES]; assert(wristflow_settings_encode(&saved, record));
    press("breathing_start"); wristflow_ui_shell_destroy(shell); assert(timers() == baseline);
    saved = wristflow_settings_default(); assert(wristflow_settings_decode(&saved, record, sizeof record));
    create(&saved); open_breathing(); text("breathing_duration_text", "4分钟");
    assert(visible("breathing_home_panel")); capture("restored");
    /* All durations keep the user's brightness through idle, then release. */
    for (unsigned minutes = 1; minutes <= 5; ++minutes) {
        choose(minutes, true); press("breathing_start"); jump(minutes * 60000U - 1600);
        assert(visible("breathing_training_panel") && brightness == 27);
        assert(wristflow_ui_shell_keep_minutes(shell) == 0); jump(1700);
        assert(visible("breathing_result_panel")); jump(5100);
        assert(wristflow_ui_shell_display_phase(shell) == WRISTFLOW_DISPLAY_OFF && brightness == 0);
        assert(wristflow_ui_shell_key(shell)); advance(64); press("breathing_done");
    }
    choose(1, true);
    assert(wristflow_ui_shell_keep_awake(shell, 10)); press("breathing_start"); jump(61000);
    assert(visible("breathing_result_panel") && wristflow_ui_shell_keep_minutes(shell) == 10 && brightness == 27);
    press("breathing_done"); assert(wristflow_ui_shell_keep_awake(shell, 0));
    /* Deadline callback must complete the owner even with its timer disabled. */
    press("breathing_start");
    for (lv_timer_t *t = lv_timer_get_next(NULL); t; t = lv_timer_get_next(t))
        if (t->period == 33) lv_timer_pause(t);
    jump(61000); assert(visible("breathing_result_panel")); press("breathing_done");
    /* Pause once across five alarms plus countdown; ordinary messages are retained. */
    press("breathing_start"); advance(5500); text("breathing_guide", "呼气");
    wristflow_notifications_snapshot_t phone = {.connected = true, .subscribed = true, .revision = 1,
        .alert_sequence = 1, .alert_id = 42, .count = 1};
    phone.messages[0].id = 42; strcpy(phone.messages[0].source, "QQ"); strcpy(phone.messages[0].title, "训练期间消息");
    assert(!wristflow_ui_shell_notifications(shell, &phone));
    assert(!lv_obj_is_visible(named(lv_layer_top(), "notification_banner")));
    queue_reminders(5, true); lv_obj_t *original = lv_screen_active(); capture("alarm_cover");
    jump(31000); assert(brightness == 0); jump(130000); assert(visible("breathing_training_panel"));
    assert(wristflow_ui_shell_key(shell)); advance(64); close_reminders(5, true);
    assert(lv_screen_active() == original && brightness == 27); text("breathing_guide", "吸气"); capture("resumed");
    assert(!wristflow_ui_shell_notifications(shell, &phone));
    jump(46000); assert(visible("breathing_training_panel")); jump(10000); assert(visible("breathing_result_panel"));
    press("breathing_done");
    /* Pending destination/confirmation survives a long reminder without approving it. */
    press("breathing_start"); advance(4500);
    assert(wristflow_ui_shell_open(shell, WRISTFLOW_SURFACE_SETTINGS)); advance(64);
    assert(visible("breathing_exit_confirm")); queue_reminders(1, false);
    assert(!visible("breathing_exit_confirm")); jump(180000);
    assert(wristflow_ui_shell_key(shell)); advance(64); close_reminders(1, false);
    assert(visible("breathing_exit_confirm")); capture("confirm_restored"); jump(70000);
    assert(!visible("breathing_result_panel")); assert(wristflow_ui_shell_key(shell)); advance(64);
    press("confirm_cancel"); text("breathing_guide", "吸气");
    /* Closing the last visible item must collect a newly pending producer first. */
    queue_reminders(1, false);
    alarms.pending[1] = (wristflow_alarm_pending_t){.alarm = alarms.alarms.entries[1],
        .sequence = ticks + 1, .due_ms = ticks, .active = true};
    press_in(current_alarm(), "alarm_reminder_close"); assert(wristflow_ui_shell_reminder_active(shell));
    jump(70000); assert(visible("breathing_training_panel"));
    assert(wristflow_ui_shell_key(shell)); advance(64); close_reminders(1, false); text("breathing_guide", "吸气");
    /* Notification navigation uses the same confirmation, and preserved detail. */
    assert(wristflow_ui_shell_open_notification(shell, true, 42)); advance(64);
    assert(visible("breathing_exit_confirm"));
    assert(!wristflow_ui_shell_open_notification(shell, false, 0));
    press("confirm_accept"); assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_NOTIFICATION_DETAIL);
    assert(wristflow_ui_shell_home(shell)); advance(320); open_breathing(); assert(visible("breathing_home_panel"));
    /* Unobserved completion consumes explicit notification navigation, as Back does. */
    press("breathing_start"); ticks += 61000;
    assert(wristflow_ui_shell_open_notification(shell, true, 42));
    assert(visible("breathing_result_panel") && wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_BREATHING);
    press("breathing_done");
    /* At the training deadline a result is an ordinary page, not a paused session. */
    press("breathing_start"); ticks += 61000; queue_reminders(1, false);
    assert(visible("breathing_result_panel")); jump(181000);
    assert(wristflow_ui_shell_key(shell)); advance(64); close_reminders(1, false); advance(320);
    assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_HOME); open_breathing();
    /* Restart/end and teardown under cover cannot retain a training lease. */
    press("breathing_start"); jump(40000);
    lv_obj_send_event(named(lv_screen_active(), "breathing_restart"), LV_EVENT_SHORT_CLICKED, NULL);
    jump(40000); assert(visible("breathing_training_panel"));
    lv_obj_send_event(named(lv_screen_active(), "breathing_end"), LV_EVENT_SHORT_CLICKED, NULL);
    assert(visible("breathing_home_panel") && !visible("breathing_result_panel")); jump(5100); assert(brightness == 0);
    assert(wristflow_ui_shell_key(shell)); advance(64); press("breathing_start"); queue_reminders(1, false);
    wristflow_ui_shell_destroy(shell); assert(timers() == baseline);
    lv_indev_delete(input); lv_display_delete(display); lv_deinit();
    puts("Product breathing: settings recreation, owned deadlines, six reminders, messages, confirmation and teardown passed");
    return 0;
}
