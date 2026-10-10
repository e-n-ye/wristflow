#include "countdown_view.h"
#include "countdown.h"
#include "wristflow_ui.h"
#include <stdio.h>

struct wristflow_countdown_view {
    wristflow_ui_shell_t *shell;
    wristflow_countdown_t task;
    lv_timer_t *timer;
    lv_obj_t *root, *indicator;
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

static void render(wristflow_countdown_view_t *view)
{
    bool idle = view->task.phase == WRISTFLOW_COUNTDOWN_IDLE;
    bool expired = view->task.phase == WRISTFLOW_COUNTDOWN_EXPIRED;
    if (view->indicator)
        lv_obj_set_flag(view->indicator, LV_OBJ_FLAG_HIDDEN, !view->home_visible || idle);
    if (!view->root) return;
    bool paused = view->task.phase == WRISTFLOW_COUNTDOWN_PAUSED;
    lv_obj_set_flag(named(view, "countdown_picker"), LV_OBJ_FLAG_HIDDEN, !idle || view->custom);
    lv_obj_set_flag(named(view, "countdown_custom_panel"), LV_OBJ_FLAG_HIDDEN, !idle || !view->custom);
    lv_obj_set_flag(named(view, "countdown_active_panel"), LV_OBJ_FLAG_HIDDEN, idle || expired);
    lv_obj_set_flag(named(view, "countdown_alert_panel"), LV_OBJ_FLAG_HIDDEN, !expired);
    lv_label_set_text(named(view, "countdown_title"), idle && view->custom ? "自定义" : "倒计时");
    lv_obj_set_flag(named(view, "countdown_paused"), LV_OBJ_FLAG_HIDDEN, !paused);
    lv_label_set_text(named(view, "countdown_toggle_icon"), paused ? "\xef\x81\x8b" : "\xef\x81\x8c");
    uint32_t remaining = wristflow_countdown_remaining(&view->task, lv_tick_get());
    duration(named(view, "countdown_remaining"), (remaining + 999U) / 1000U);
    duration(named(view, "countdown_original"), view->task.duration_ms / 1000U);
    char time[8];
    if (view->clock.time_unavailable) snprintf(time, sizeof time, "--:--");
    else snprintf(time, sizeof time, "%02u:%02u", view->clock.hour_24, view->clock.minute);
    lv_label_set_text(named(view, "countdown_clock"), time);
    lv_obj_set_state(named(view, "countdown_start"), LV_STATE_DISABLED, view->custom_seconds == 0);
    lv_obj_set_style_opa(named(view, "countdown_start"), view->custom_seconds ? LV_OPA_COVER : LV_OPA_40, 0);
}

static void observe(wristflow_countdown_view_t *view)
{
    wristflow_countdown_update(&view->task, lv_tick_get());
    render(view);
    if (view->task.expiry_pending) {
        const wristflow_navigation_t *navigation = wristflow_ui_shell_navigation(view->shell);
        if (navigation->surface == WRISTFLOW_SURFACE_COUNTDOWN ||
            wristflow_ui_shell_open(view->shell, WRISTFLOW_SURFACE_COUNTDOWN))
            wristflow_countdown_take_expiry(&view->task);
    }
    if (view->task.phase == WRISTFLOW_COUNTDOWN_RUNNING || view->task.expiry_pending)
        lv_timer_resume(view->timer);
    else lv_timer_pause(view->timer);
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
    lv_obj_t *target = lv_event_get_current_target_obj(event);
    uint32_t now = lv_tick_get();
    /* Reconcile expiry before accepting a late pause/cancel click. */
    wristflow_countdown_update(&view->task, now);
    for (unsigned i = 0; i < sizeof presets / sizeof presets[0]; ++i)
        if (target == named(view, preset_names[i])) {
            wristflow_countdown_start(&view->task, presets[i] * 60U, now);
            view->custom = false;
        }
    if (target == named(view, "countdown_custom")) view->custom = true;
    if (target == named(view, "countdown_start")) {
        if (wristflow_countdown_start(&view->task, view->custom_seconds, now)) view->custom = false;
    }
    if (target == named(view, "countdown_cancel") &&
        view->task.phase != WRISTFLOW_COUNTDOWN_EXPIRED)
        wristflow_countdown_cancel(&view->task);
    if (target == named(view, "countdown_toggle")) {
        if (view->task.phase == WRISTFLOW_COUNTDOWN_PAUSED) wristflow_countdown_resume(&view->task, now);
        else wristflow_countdown_pause(&view->task, now);
    }
    if (target == named(view, "countdown_repeat")) wristflow_countdown_repeat(&view->task, now);
    if (target == named(view, "countdown_close")) {
        wristflow_countdown_cancel(&view->task);
        render(view);
        if (!wristflow_ui_shell_back(view->shell)) wristflow_ui_shell_home(view->shell);
    }
    observe(view);
}

static void open_countdown(lv_event_t *event)
{
    wristflow_countdown_view_t *view = lv_event_get_user_data(event);
    wristflow_ui_shell_open(view->shell, WRISTFLOW_SURFACE_COUNTDOWN);
}

wristflow_countdown_view_t *wristflow_countdown_view_create(wristflow_ui_shell_t *shell)
{
    wristflow_countdown_view_t *view = lv_malloc_zeroed(sizeof(*view));
    LV_ASSERT_MALLOC(view);
    view->shell = shell;
    view->custom_seconds = 60;
    wristflow_countdown_init(&view->task);
    view->timer = lv_timer_create(tick, 250, view);
    lv_timer_pause(view->timer);
    return view;
}

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
    wristflow_countdown_update(&view->task, lv_tick_get());
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
    lv_free(view);
}
