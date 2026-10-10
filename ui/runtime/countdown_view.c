#include "countdown_view.h"
#include "countdown.h"
#include "wristflow_ui.h"
#include "mount_screen.h"
#include <stdio.h>
#include <string.h>

struct wristflow_countdown_view {
    wristflow_ui_shell_t *shell;
    wristflow_countdown_t task;
    wristflow_countdown_port_t backend;
    lv_timer_t *timer;
    lv_obj_t *root, *indicator, *alert;
    wristflow_watch_snapshot_t clock;
    uint32_t custom_seconds;
    bool custom, home_visible;
};

static const unsigned presets[] = {1, 2, 3, 5, 10, 30};
static const char *const preset_names[] = {
    "countdown_preset_1", "countdown_preset_2", "countdown_preset_3",
    "countdown_preset_5", "countdown_preset_10", "countdown_preset_30"
};

static lv_obj_t *named(wristflow_countdown_view_t *view, const char *name)
{
    lv_obj_t *obj = lv_obj_find_by_name(view->root, name);
    LV_ASSERT(obj);
    return obj;
}

static void duration(lv_obj_t *label, uint32_t seconds)
{
    lv_label_set_text_fmt(label, "%02u:%02u:%02u", (unsigned)(seconds / 3600),
                         (unsigned)(seconds / 60 % 60), (unsigned)(seconds % 60));
}

static uint32_t now(wristflow_countdown_view_t *view)
{ return view->backend.now ? view->backend.now(view->backend.context) : lv_tick_get(); }

static void read_task(wristflow_countdown_view_t *view)
{
    if (view->backend.read) view->backend.read(view->backend.context, &view->task);
    else wristflow_countdown_update(&view->task, now(view));
}

static bool command(wristflow_countdown_view_t *view, wristflow_countdown_command_t op, uint32_t seconds)
{
    bool accepted = false;
    if (view->backend.command) accepted = view->backend.command(view->backend.context, op, seconds);
    else {
        uint32_t at = now(view);
        switch (op) {
        case WF_COUNTDOWN_START: accepted = wristflow_countdown_start(&view->task, seconds, at); break;
        case WF_COUNTDOWN_PAUSE: accepted = wristflow_countdown_pause(&view->task, at); break;
        case WF_COUNTDOWN_RESUME: accepted = wristflow_countdown_resume(&view->task, at); break;
        case WF_COUNTDOWN_CANCEL: case WF_COUNTDOWN_DISMISS:
            wristflow_countdown_cancel(&view->task); accepted = true; break;
        case WF_COUNTDOWN_REPEAT: accepted = wristflow_countdown_repeat(&view->task, at); break;
        case WF_COUNTDOWN_ACK: accepted = wristflow_countdown_take_expiry(&view->task); break;
        }
    }
    read_task(view);
    return accepted;
}

static void render_clock(wristflow_countdown_view_t *view, lv_obj_t *root)
{
    char time[8];
    if (view->clock.time_unavailable) snprintf(time, sizeof time, "--:--");
    else snprintf(time, sizeof time, "%02u:%02u", view->clock.hour_24, view->clock.minute);
    lv_label_set_text(lv_obj_find_by_name(root, "countdown_clock"), time);
}

static void render(wristflow_countdown_view_t *view)
{
    bool idle = view->task.phase == WRISTFLOW_COUNTDOWN_IDLE;
    bool expired = view->task.phase == WRISTFLOW_COUNTDOWN_EXPIRED;
    if (view->indicator)
        lv_obj_set_flag(view->indicator, LV_OBJ_FLAG_HIDDEN, !view->home_visible || idle);
    if (view->alert) {
        duration(lv_obj_find_by_name(view->alert, "countdown_original"), view->task.duration_ms / 1000U);
        render_clock(view, view->alert);
    }
    if (!view->root) return;
    bool paused = view->task.phase == WRISTFLOW_COUNTDOWN_PAUSED;
    lv_obj_set_flag(named(view, "countdown_picker"), LV_OBJ_FLAG_HIDDEN, !idle || view->custom);
    lv_obj_set_flag(named(view, "countdown_custom_panel"), LV_OBJ_FLAG_HIDDEN, !idle || !view->custom);
    lv_obj_set_flag(named(view, "countdown_active_panel"), LV_OBJ_FLAG_HIDDEN, idle || expired);
    lv_obj_set_flag(named(view, "countdown_alert_panel"), LV_OBJ_FLAG_HIDDEN, !expired);
    lv_label_set_text(named(view, "countdown_title"), idle && view->custom ? "自定义" : "倒计时");
    lv_obj_set_flag(named(view, "countdown_paused"), LV_OBJ_FLAG_HIDDEN, !paused);
    lv_label_set_text(named(view, "countdown_toggle_icon"), paused ? "\xef\x81\x8b" : "\xef\x81\x8c");
    uint32_t remaining = wristflow_countdown_remaining(&view->task, now(view));
    duration(named(view, "countdown_remaining"), (remaining + 999U) / 1000U);
    duration(named(view, "countdown_original"), view->task.duration_ms / 1000U);
    render_clock(view, view->root);
    lv_obj_set_state(named(view, "countdown_start"), LV_STATE_DISABLED, view->custom_seconds == 0);
    lv_obj_set_style_opa(named(view, "countdown_start"), view->custom_seconds ? LV_OPA_COVER : LV_OPA_40, 0);
}

static void action(lv_event_t *event);

static lv_obj_t *alert_content(wristflow_countdown_view_t *view)
{
    if (!view->alert) {
        view->alert = wristflow_mount_screen(lv_layer_top(), screen_countdown_create());
        lv_obj_set_name(view->alert, "countdown_reminder");
        lv_obj_add_flag(view->alert, LV_OBJ_FLAG_HIDDEN);
        const char *const hidden[] = {"countdown_picker", "countdown_custom_panel", "countdown_active_panel"};
        for (unsigned i = 0; i < 3; ++i)
            lv_obj_add_flag(lv_obj_find_by_name(view->alert, hidden[i]), LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(lv_obj_find_by_name(view->alert, "countdown_alert_panel"), LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(lv_obj_find_by_name(view->alert, "countdown_close"), action, LV_EVENT_SHORT_CLICKED, view);
        lv_obj_add_event_cb(lv_obj_find_by_name(view->alert, "countdown_repeat"), action, LV_EVENT_SHORT_CLICKED, view);
    }
    render(view);
    return view->alert;
}

static bool observe(wristflow_countdown_view_t *view)
{
    read_task(view);
    render(view);
    bool presented = false;
    if (view->task.expiry_pending) {
        if (view->backend.read) {
            presented = wristflow_ui_shell_present_reminder(view->shell, alert_content(view));
            if (presented) command(view, WF_COUNTDOWN_ACK, 0);
        } else {
            wristflow_ui_shell_open(view->shell, WRISTFLOW_SURFACE_COUNTDOWN);
            /* Accepted open can mean a pending stopwatch exit confirmation. */
            if (wristflow_ui_shell_navigation(view->shell)->surface == WRISTFLOW_SURFACE_COUNTDOWN)
                command(view, WF_COUNTDOWN_ACK, 0);
        }
    }
    if (view->task.phase == WRISTFLOW_COUNTDOWN_RUNNING || view->task.expiry_pending)
        lv_timer_resume(view->timer);
    else lv_timer_pause(view->timer);
    return presented;
}

static void tick(lv_timer_t *timer)
{ observe(lv_timer_get_user_data(timer)); }

static void wheel_changed(lv_event_t *event)
{
    wristflow_countdown_view_t *view = lv_event_get_user_data(event);
    view->custom_seconds = lv_roller_get_selected(named(view, "countdown_hours")) * 3600U +
        lv_roller_get_selected(named(view, "countdown_minutes")) * 60U +
        lv_roller_get_selected(named(view, "countdown_seconds"));
    render(view);
}

static void action(lv_event_t *event)
{
    wristflow_countdown_view_t *view = lv_event_get_user_data(event);
    const char *target = lv_obj_get_name(lv_event_get_current_target_obj(event));
    /* Reconcile expiry before commands; explicit Cancel still wins its race. */
    read_task(view);
    for (unsigned i = 0; i < sizeof presets / sizeof presets[0]; ++i)
        if (!strcmp(target, preset_names[i])) {
            command(view, WF_COUNTDOWN_START, presets[i] * 60U);
            view->custom = false;
        }
    if (!strcmp(target, "countdown_custom")) view->custom = true;
    if (!strcmp(target, "countdown_start")) {
        if (command(view, WF_COUNTDOWN_START, view->custom_seconds)) view->custom = false;
    }
    if (!strcmp(target, "countdown_cancel")) {
        command(view, WF_COUNTDOWN_CANCEL, 0);
        if (view->backend.read) wristflow_ui_shell_close_reminder(view->shell);
    }
    if (!strcmp(target, "countdown_toggle")) {
        command(view, view->task.phase == WRISTFLOW_COUNTDOWN_PAUSED ? WF_COUNTDOWN_RESUME : WF_COUNTDOWN_PAUSE, 0);
    }
    bool repeat = !strcmp(target, "countdown_repeat"), close = !strcmp(target, "countdown_close");
    if (repeat || close) {
        bool accepted = command(view, repeat ? WF_COUNTDOWN_REPEAT : WF_COUNTDOWN_DISMISS, 0);
        render(view);
        if (accepted && view->backend.read) wristflow_ui_shell_close_reminder(view->shell);
        else if (close && accepted && !wristflow_ui_shell_back(view->shell)) wristflow_ui_shell_home(view->shell);
    }
    observe(view);
}

static void open_countdown(lv_event_t *event)
{
    wristflow_countdown_view_t *view = lv_event_get_user_data(event);
    wristflow_ui_shell_open(view->shell, WRISTFLOW_SURFACE_COUNTDOWN);
}

wristflow_countdown_view_t *wristflow_countdown_view_create(wristflow_ui_shell_t *shell,
    const wristflow_countdown_port_t *backend)
{
    if (backend && (!backend->read || !backend->command || !backend->now)) return NULL;
    wristflow_countdown_view_t *view = lv_malloc_zeroed(sizeof(*view));
    LV_ASSERT_MALLOC(view);
    view->shell = shell;
    if (backend) view->backend = *backend;
    view->custom_seconds = 60;
    wristflow_countdown_init(&view->task);
    view->timer = lv_timer_create(tick, 250, view);
    lv_timer_pause(view->timer);
    return view;
}

bool wristflow_countdown_view_refresh(wristflow_countdown_view_t *view)
{ return view && observe(view); }

bool wristflow_countdown_view_active(const wristflow_countdown_view_t *view)
{ return view && view->task.phase != WRISTFLOW_COUNTDOWN_IDLE; }

lv_obj_t *wristflow_countdown_view_screen(wristflow_countdown_view_t *view)
{
    if (!view) return NULL;
    if (view->root) return view->root;
    view->root = screen_countdown_create();
    view->custom = false;
    const char *const buttons[] = {"countdown_custom", "countdown_start", "countdown_cancel",
        "countdown_toggle", "countdown_close", "countdown_repeat"};
    for (unsigned i = 0; i < sizeof buttons / sizeof buttons[0]; ++i)
        lv_obj_add_event_cb(named(view, buttons[i]), action, LV_EVENT_SHORT_CLICKED, view);
    for (unsigned i = 0; i < sizeof presets / sizeof presets[0]; ++i)
        lv_obj_add_event_cb(named(view, preset_names[i]), action, LV_EVENT_SHORT_CLICKED, view);
    const char *const wheels[] = {"countdown_hours", "countdown_minutes", "countdown_seconds"};
    uint32_t values[] = {view->custom_seconds / 3600U, view->custom_seconds / 60U % 60U,
                         view->custom_seconds % 60U};
    for (unsigned i = 0; i < 3; ++i) {
        lv_roller_set_selected(named(view, wheels[i]), values[i], LV_ANIM_OFF);
        lv_obj_add_event_cb(named(view, wheels[i]), wheel_changed, LV_EVENT_VALUE_CHANGED, view);
    }
    read_task(view);
    render(view);
    return view->root;
}

void wristflow_countdown_view_detach(wristflow_countdown_view_t *view)
{ if (view) view->root = NULL; }

void wristflow_countdown_view_clock(wristflow_countdown_view_t *view, const wristflow_watch_snapshot_t *snapshot)
{ if (view) { view->clock = *snapshot; render(view); } }

void wristflow_countdown_view_bind_home(wristflow_countdown_view_t *view, lv_obj_t *home)
{
    if (!view) return;
    view->indicator = countdown_indicator_create(home);
    lv_obj_set_name(view->indicator, "countdown_indicator");
    lv_obj_add_event_cb(view->indicator, open_countdown, LV_EVENT_SHORT_CLICKED, view);
    render(view);
}

void wristflow_countdown_view_home_visible(wristflow_countdown_view_t *view, bool visible)
{ if (view) { view->home_visible = visible; render(view); } }

bool wristflow_countdown_view_back(wristflow_countdown_view_t *view)
{
    if (!view || !view->root || !view->custom || view->task.phase != WRISTFLOW_COUNTDOWN_IDLE)
        return false;
    view->custom = false;
    render(view);
    return true;
}

void wristflow_countdown_view_destroy(wristflow_countdown_view_t *view)
{
    if (!view) return;
    lv_timer_delete(view->timer);
    if (view->indicator) lv_obj_delete(view->indicator);
    if (view->alert) lv_obj_delete(view->alert);
    lv_free(view);
}
