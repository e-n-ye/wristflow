#include "breathing_view.h"
#include "breathing.h"
#include "wristflow_ui.h"
#include <stdio.h>

enum { HOME, DURATION, TRAINING, CONTROLS, RESULT };
struct wristflow_breathing_view {
    wristflow_ui_shell_t *shell;
    lv_obj_t *root, *dialog;
    lv_timer_t *timer;
    wristflow_breathing_t session;
    wristflow_watch_snapshot_t clock;
    lv_point_t ring_center;
    unsigned page, draft;
    bool covered, holding;
    void (*commit_leave)(void *);
    void *leave_context;
};
static const char *const panels[] = {"breathing_home_panel", "breathing_duration_panel",
    "breathing_training_panel", "breathing_controls_panel", "breathing_result_panel"};

static void render(wristflow_breathing_view_t *v);

static void release_awake(wristflow_breathing_view_t *v)
{
    if (!v->holding) return;
    wristflow_ui_shell_release_awake(v->shell, v);
    v->holding = false;
}

static void expired(void *owner)
{
    wristflow_breathing_view_t *v = owner;
    wristflow_breathing_update(&v->session, lv_tick_get());
    /* The lease is a safety deadline even if the animation timer stops. */
    if (v->session.phase == WRISTFLOW_BREATHING_RUNNING) {
        wristflow_breathing_end(&v->session);
        v->page = HOME;
    }
    render(v);
}

static lv_obj_t *named(wristflow_breathing_view_t *v, const char *name)
{
    lv_obj_t *obj = lv_obj_find_by_name(v->root, name);
    LV_ASSERT(obj);
    return obj;
}

static void render(wristflow_breathing_view_t *v)
{
    if (v->session.phase != WRISTFLOW_BREATHING_RUNNING) release_awake(v);
    else if (!v->holding) {
        v->holding = wristflow_ui_shell_acquire_awake(v->shell, v, "breathing training",
            v->session.minutes * 60000U - v->session.elapsed_ms, expired);
        if (!v->holding) { wristflow_breathing_end(&v->session); v->page = HOME; }
    }
    if (!v->root) return;
    if (v->dialog) lv_obj_set_flag(v->dialog, LV_OBJ_FLAG_HIDDEN, v->covered);
    if (v->session.phase == WRISTFLOW_BREATHING_COMPLETE) v->page = RESULT;
    for (unsigned i = 0; i < 5; ++i)
        lv_obj_set_flag(named(v, panels[i]), LV_OBJ_FLAG_HIDDEN, i != v->page);
    const char *title = v->page == HOME ? "呼吸" : v->page == DURATION ? "时长" :
        v->page == RESULT ? "训练结果" : "呼吸训练";
    lv_label_set_text(named(v, "breathing_title"), title);
    char text[32];
    if (v->clock.time_unavailable) snprintf(text, sizeof text, "--:--");
    else snprintf(text, sizeof text, "%02u:%02u", v->clock.hour_24, v->clock.minute);
    lv_label_set_text(named(v, "breathing_clock"), text);
    snprintf(text, sizeof text, "%u分钟", v->session.minutes);
    lv_label_set_text(named(v, "breathing_duration_text"), text);
    snprintf(text, sizeof text, "呼吸%u次", v->draft * 7);
    lv_label_set_text(named(v, "breathing_cycles"), text);
    snprintf(text, sizeof text, "%u", v->session.minutes);
    lv_label_set_text(named(v, "breathing_result_minutes"), text);
    lv_label_set_text(named(v, "breathing_guide"), wristflow_breathing_inhaling(&v->session) ? "吸气" : "呼气");
    unsigned p = wristflow_breathing_expansion(&v->session);
    /* Smoothstep changes radius continuously, while the model owns phase timing. */
    unsigned eased = p * p / 1000U * (3000U - 2U * p) / 1000U;
    int size = 140 + (int)(80U * eased / 1000U);
    lv_obj_t *ring = named(v, "breathing_training_ring");
    lv_obj_set_size(ring, size, size);
    lv_obj_set_pos(ring, v->ring_center.x - size / 2, v->ring_center.y - size / 2);
    if (v->session.phase == WRISTFLOW_BREATHING_RUNNING) lv_timer_resume(v->timer);
    else lv_timer_pause(v->timer);
}

static void tick(lv_timer_t *timer)
{
    wristflow_breathing_view_t *v = lv_timer_get_user_data(timer);
    wristflow_breathing_update(&v->session, lv_tick_get());
    render(v);
}

static void wheel_changed(lv_event_t *event)
{
    wristflow_breathing_view_t *v = lv_event_get_user_data(event);
    v->draft = lv_roller_get_selected(lv_event_get_target_obj(event)) + 1;
    render(v);
}

static void action(lv_event_t *event)
{
    wristflow_breathing_view_t *v = lv_event_get_user_data(event);
    lv_obj_t *button = lv_event_get_target_obj(event);
    if (button == named(v, "breathing_duration")) {
        v->draft = v->session.minutes;
        lv_roller_set_selected(named(v, "breathing_minutes"), v->draft - 1, LV_ANIM_OFF);
        v->page = DURATION;
    } else if (button == named(v, "breathing_duration_confirm")) {
        wristflow_breathing_set_minutes(&v->session, v->draft);
        wristflow_ui_shell_set_breathing_minutes(v->shell, v->session.minutes);
        v->page = HOME;
    } else if (button == named(v, "breathing_start") || button == named(v, "breathing_restart")) {
        wristflow_ui_shell_notification_dismiss(v->shell, true);
        release_awake(v);
        wristflow_breathing_start(&v->session, lv_tick_get());
        v->page = TRAINING;
        lv_timer_reset(v->timer);
    } else {
        wristflow_breathing_end(&v->session); /* End and Done both return to this app's home. */
        v->page = HOME;
    }
    render(v);
}

wristflow_breathing_view_t *wristflow_breathing_view_create(wristflow_ui_shell_t *shell)
{
    wristflow_breathing_view_t *v = lv_malloc_zeroed(sizeof(*v));
    LV_ASSERT_MALLOC(v);
    v->shell = shell;
    wristflow_breathing_init(&v->session);
    wristflow_settings_t settings;
    if (wristflow_ui_shell_get_settings(shell, &settings)) v->session.minutes = settings.breathing_minutes;
    v->draft = v->session.minutes;
    v->timer = lv_timer_create(tick, 33, v);
    lv_timer_pause(v->timer);
    return v;
}

lv_obj_t *wristflow_breathing_view_screen(wristflow_breathing_view_t *v)
{
    if (!v) return NULL;
    if (v->root) return v->root;
    v->root = screen_breathing_create();
    lv_obj_update_layout(v->root);
    lv_obj_t *ring = named(v, "breathing_training_ring");
    v->ring_center = (lv_point_t){lv_obj_get_x(ring) + lv_obj_get_width(ring) / 2,
        lv_obj_get_y(ring) + lv_obj_get_height(ring) / 2};
    const char *const buttons[] = {"breathing_duration", "breathing_duration_confirm", "breathing_start",
        "breathing_end", "breathing_restart", "breathing_done"};
    for (unsigned i = 0; i < sizeof buttons / sizeof buttons[0]; ++i)
        lv_obj_add_event_cb(named(v, buttons[i]), action, LV_EVENT_SHORT_CLICKED, v);
    lv_obj_add_event_cb(named(v, "breathing_minutes"), wheel_changed, LV_EVENT_VALUE_CHANGED, v);
    render(v);
    return v->root;
}

void wristflow_breathing_view_clock(wristflow_breathing_view_t *v, const wristflow_watch_snapshot_t *clock)
{
    if (!v) return;
    v->clock = *clock;
    wristflow_settings_t settings;
    if (v->session.phase == WRISTFLOW_BREATHING_IDLE && wristflow_ui_shell_get_settings(v->shell, &settings))
        v->session.minutes = settings.breathing_minutes;
    render(v);
}

bool wristflow_breathing_view_active(const wristflow_breathing_view_t *v)
{ return v && (v->session.phase == WRISTFLOW_BREATHING_RUNNING || v->session.phase == WRISTFLOW_BREATHING_PAUSED); }

void wristflow_breathing_view_cover(wristflow_breathing_view_t *v, bool covered)
{
    if (!v || v->covered == covered) return;
    v->covered = covered;
    if (covered) wristflow_breathing_pause(&v->session, lv_tick_get());
    else if (!v->dialog) wristflow_breathing_resume(&v->session, lv_tick_get());
    render(v);
}

bool wristflow_breathing_view_cancel_leave(wristflow_breathing_view_t *v)
{
    if (!v || !v->dialog || v->covered) return false;
    lv_obj_delete(v->dialog);
    v->dialog = NULL;
    v->commit_leave = NULL;
    v->leave_context = NULL;
    wristflow_breathing_resume(&v->session, lv_tick_get());
    render(v);
    return true;
}

static void cancel(lv_event_t *event)
{ wristflow_breathing_view_cancel_leave(lv_event_get_user_data(event)); }

static void accept(lv_event_t *event)
{
    wristflow_breathing_view_t *v = lv_event_get_user_data(event);
    if (v->covered) return;
    void (*commit)(void *) = v->commit_leave;
    void *context = v->leave_context;
    wristflow_breathing_view_leave(v);
    if (commit) commit(context);
}

bool wristflow_breathing_view_confirm_leave(wristflow_breathing_view_t *v, void (*commit)(void *), void *context)
{
    if (!v || !v->root) return false;
    if (v->dialog) return true; /* One request owns the pending destination. */
    bool running = v->session.phase == WRISTFLOW_BREATHING_RUNNING;
    wristflow_breathing_pause(&v->session, lv_tick_get());
    render(v);
    /* A deadline not yet sampled by the UI timer consumes this leave request. */
    if (running && v->session.phase == WRISTFLOW_BREATHING_COMPLETE) return true;
    if (v->session.phase != WRISTFLOW_BREATHING_PAUSED) return false;
    v->commit_leave = commit;
    v->leave_context = context;
    v->dialog = component_confirm_create(v->root);
    lv_obj_set_name(v->dialog, "breathing_exit_confirm");
    lv_label_set_text(lv_obj_find_by_name(v->dialog, "confirm_text"), "结束本次呼吸训练？");
    lv_obj_add_event_cb(lv_obj_find_by_name(v->dialog, "confirm_cancel"), cancel, LV_EVENT_SHORT_CLICKED, v);
    lv_obj_add_event_cb(lv_obj_find_by_name(v->dialog, "confirm_accept"), accept, LV_EVENT_SHORT_CLICKED, v);
    lv_obj_remove_flag(v->dialog, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    return true;
}

bool wristflow_breathing_view_back(wristflow_breathing_view_t *v)
{
    if (!v || !v->root) return false;
    if (wristflow_breathing_view_cancel_leave(v)) return true;
    if (v->page == DURATION || v->page == RESULT) {
        wristflow_breathing_end(&v->session);
        v->page = HOME;
        render(v);
        return true;
    }
    return false;
}

bool wristflow_breathing_view_gesture(wristflow_breathing_view_t *v, lv_dir_t direction, bool edge)
{
    if (!v || v->dialog || (v->page != TRAINING && v->page != CONTROLS)) return false;
    if (direction == LV_DIR_RIGHT && edge) return false;
    if (direction != LV_DIR_LEFT && direction != LV_DIR_RIGHT) return false;
    v->page = direction == LV_DIR_LEFT ? CONTROLS : TRAINING;
    render(v);
    return true;
}

void wristflow_breathing_view_leave(wristflow_breathing_view_t *v)
{
    if (!v) return;
    if (v->dialog) lv_obj_delete(v->dialog);
    v->dialog = NULL;
    v->commit_leave = NULL;
    v->leave_context = NULL;
    v->covered = false;
    wristflow_breathing_end(&v->session);
    v->page = HOME;
    lv_timer_pause(v->timer);
    render(v);
}

void wristflow_breathing_view_detach(wristflow_breathing_view_t *v)
{ if (v) { wristflow_breathing_view_leave(v); v->root = NULL; } }

void wristflow_breathing_view_destroy(wristflow_breathing_view_t *v)
{ if (v) { wristflow_breathing_view_leave(v); lv_timer_delete(v->timer); lv_free(v); } }
