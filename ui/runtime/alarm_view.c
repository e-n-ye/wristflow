#include "alarm_view.h"
#include "alarm.h"
#include "wristflow_ui.h"
#include "reminders.h"
#include <stdio.h>
#include <string.h>

typedef enum { ALARM_LIST, ALARM_TIME, ALARM_SETTINGS, ALARM_REPEAT, ALARM_DAYS } alarm_page_t;
struct wristflow_alarm_view {
    wristflow_alarms_t alarms;
    wristflow_alarm_t draft;
    wristflow_watch_snapshot_t clock;
    lv_obj_t *root, *confirmation;
    alarm_page_t page;
    uint8_t repeat_mask, day_mask;
    bool time_from_settings;
    wristflow_ui_shell_t *shell;
    wristflow_alarm_port_t backend;
    wristflow_reminders_t *reminders;
    lv_timer_t *timer;
    struct { uint32_t id; lv_obj_t *content; } alerts[WRISTFLOW_ALARM_CAPACITY];
    wristflow_alarm_command_t submitting;
    uint32_t request;
    bool failed, time_valid, storage_failed;
};

static const char *const panels[] = {"alarm_list_panel", "alarm_time_panel",
    "alarm_settings_panel", "alarm_repeat_panel", "alarm_weekdays_panel"};
static const char *const titles[] = {"闹钟", "时间设置", "设置", "重复", "自定义"};
static const char *const days[] = {"一", "二", "三", "四", "五", "六", "日"};
static const uint8_t presets[] = {0, WRISTFLOW_ALARM_DAILY, WRISTFLOW_ALARM_WORKDAYS, WRISTFLOW_ALARM_WEEKEND};

static lv_obj_t *named(wristflow_alarm_view_t *view, const char *name)
{
    lv_obj_t *obj = lv_obj_find_by_name(view->root, name);
    LV_ASSERT(obj); return obj;
}

static lv_obj_t *numbered(wristflow_alarm_view_t *view, const char *prefix, unsigned index)
{
    char name[32]; snprintf(name, sizeof name, "%s_%u", prefix, index);
    return named(view, name);
}

static void rule(char *text, size_t size, uint8_t mask)
{
    const char *preset = mask == 0 ? "仅一次" : mask == WRISTFLOW_ALARM_DAILY ? "每天" :
        mask == WRISTFLOW_ALARM_WORKDAYS ? "工作日（周一至五）" :
        mask == WRISTFLOW_ALARM_WEEKEND ? "周末（周六、日）" : NULL;
    if (preset) { snprintf(text, size, "%s", preset); return; }
    snprintf(text, size, "周");
    for (unsigned i = 0; i < 7; ++i) if (mask & (1U << i)) {
        size_t used = strlen(text);
        snprintf(text + used, size - used, "%s%s", used > strlen("周") ? "、" : "", days[i]);
    }
}

static void clock_text(wristflow_alarm_view_t *view)
{
    if (!view->root) return;
    char clock[8];
    if (view->clock.time_unavailable) snprintf(clock, sizeof clock, "--:--");
    else snprintf(clock, sizeof clock, "%02u:%02u", view->clock.hour_24, view->clock.minute);
    lv_label_set_text(named(view, "alarm_clock"), clock);
}

static void render(wristflow_alarm_view_t *view)
{
    if (!view->root) return;
    for (unsigned i = 0; i < 5; ++i)
        lv_obj_set_flag(named(view, panels[i]), LV_OBJ_FLAG_HIDDEN, i != (unsigned)view->page);
    lv_label_set_text(named(view, "alarm_title"), titles[view->page]);
    lv_obj_set_flag(named(view, "alarm_back_icon"), LV_OBJ_FLAG_HIDDEN, view->page == ALARM_LIST);
    lv_obj_set_x(named(view, "alarm_title"), view->page == ALARM_LIST ? 6 : 24);
    clock_text(view);
    bool empty = view->alarms.count == 0;
    bool full = view->alarms.count == WRISTFLOW_ALARM_CAPACITY;
    lv_obj_set_flag(named(view, "alarm_list"), LV_OBJ_FLAG_HIDDEN, empty);
    lv_obj_set_flag(named(view, "alarm_empty"), LV_OBJ_FLAG_HIDDEN, !empty);
    lv_obj_set_flag(named(view, "alarm_empty_icon"), LV_OBJ_FLAG_HIDDEN, !empty);
    bool error = view->failed || view->storage_failed;
    bool status = view->request || error || (view->backend.read && !view->time_valid);
    bool status_page = view->page == ALARM_LIST || view->page == ALARM_SETTINGS;
    lv_obj_set_flag(named(view, "alarm_limit"), LV_OBJ_FLAG_HIDDEN, !full || status);
    lv_obj_set_flag(named(view, "alarm_status"), LV_OBJ_FLAG_HIDDEN, !status || !status_page);
    lv_label_set_text(named(view, "alarm_status"), view->request ? "保存中" : error ? "保存失败，稍后重试" : "等待校时");
    lv_obj_set_style_text_color(named(view, "alarm_status"), lv_color_hex(error ? 0xef4770 : 0xb0b3b8), 0);
    lv_obj_set_state(named(view, "alarm_add"), LV_STATE_DISABLED, full || view->request);
    lv_obj_set_state(named(view, "app_back"), LV_STATE_DISABLED, view->request != 0);
    lv_obj_set_state(named(view, "alarm_save"), LV_STATE_DISABLED, view->request != 0);
    lv_obj_set_state(named(view, "alarm_delete"), LV_STATE_DISABLED, view->request != 0);
    for (unsigned i = 0; i < WRISTFLOW_ALARM_CAPACITY; ++i) {
        lv_obj_t *row = numbered(view, "alarm_row", i);
        bool present = i < view->alarms.count;
        lv_obj_set_flag(row, LV_OBJ_FLAG_HIDDEN, !present);
        if (!present) continue;
        const wristflow_alarm_t *alarm = &view->alarms.entries[i];
        lv_label_set_text_fmt(lv_obj_find_by_name(row, "alarm_time"), "%02u:%02u", alarm->hour, alarm->minute);
        char summary[96]; rule(summary, sizeof summary, alarm->weekdays);
        lv_label_set_text(lv_obj_find_by_name(row, "alarm_rule"), summary);
        lv_obj_set_state(lv_obj_find_by_name(row, "alarm_enabled"), LV_STATE_CHECKED, alarm->enabled);
        lv_obj_set_state(lv_obj_find_by_name(row, "alarm_enabled"), LV_STATE_DISABLED, view->request != 0);
    }
    lv_label_set_text_fmt(named(view, "alarm_draft_time"), "%02u:%02u", view->draft.hour, view->draft.minute);
    char summary[96]; rule(summary, sizeof summary, view->draft.weekdays);
    lv_label_set_text(named(view, "alarm_draft_rule"), summary);
    lv_obj_set_flag(named(view, "alarm_delete"), LV_OBJ_FLAG_HIDDEN, view->draft.id == 0);
    unsigned chosen = 4;
    for (unsigned i = 0; i < 4; ++i) if (presets[i] == view->repeat_mask) chosen = i;
    for (unsigned i = 0; i < 5; ++i)
        lv_obj_set_flag(lv_obj_find_by_name(numbered(view, "alarm_repeat", i), "option_check"), LV_OBJ_FLAG_HIDDEN, chosen != i);
    for (unsigned i = 0; i < 7; ++i)
        lv_obj_set_flag(lv_obj_find_by_name(numbered(view, "alarm_day", i), "option_check"), LV_OBJ_FLAG_HIDDEN, !(view->day_mask & (1U << i)));
    lv_obj_set_state(named(view, "alarm_days_accept"), LV_STATE_DISABLED, view->day_mask == 0);
}

static void completed(wristflow_alarm_view_t *view)
{
    const wristflow_alarm_command_t *cmd = &view->submitting;
    if (view->backend.read && (cmd->operation == WF_ALARM_REMOVE || (cmd->operation == WF_ALARM_ENABLE && !cmd->enabled)))
        wristflow_reminders_remove(view->reminders, (wristflow_reminder_key_t){WF_REMINDER_ALARM, cmd->id});
    if (cmd->operation != WF_ALARM_ENABLE) { view->draft = (wristflow_alarm_t){0}; view->page = ALARM_LIST; }
}

static void submit(wristflow_alarm_view_t *view, wristflow_alarm_command_t cmd)
{
    view->failed = false; view->submitting = cmd;
    if (view->backend.request) {
        view->request = view->backend.request(view->backend.context, &cmd);
        view->failed = !view->request;
        if (view->request) lv_timer_resume(view->timer);
    } else {
        bool accepted = false;
        switch (cmd.operation) {
        case WF_ALARM_ADD: accepted = wristflow_alarm_add(&view->alarms, &cmd.draft, NULL); break;
        case WF_ALARM_EDIT: accepted = wristflow_alarm_edit(&view->alarms, cmd.id, &cmd.draft); break;
        case WF_ALARM_ENABLE: accepted = wristflow_alarm_enable(&view->alarms, cmd.id, cmd.enabled); break;
        case WF_ALARM_REMOVE: accepted = wristflow_alarm_remove(&view->alarms, cmd.id); break;
        }
        if (accepted) completed(view); else view->failed = true;
    }
}

static void read_snapshot(wristflow_alarm_view_t *view, wristflow_alarm_snapshot_t *snapshot)
{
    view->backend.read(view->backend.context, snapshot);
    view->alarms = snapshot->alarms; view->time_valid = snapshot->time_valid;
    view->storage_failed = snapshot->storage_failed;
}

static void collect_state(wristflow_alarm_view_t *view, wristflow_alarm_snapshot_t *snapshot)
{
    if (view->request) {
        wristflow_save_state_t state = view->backend.status(view->backend.context, view->request);
        if (state != WRISTFLOW_SAVE_PENDING) {
            view->request = 0; view->failed = state != WRISTFLOW_SAVE_DONE;
            if (!view->failed) completed(view);
        }
    }
    read_snapshot(view, snapshot);
    if (!view->request) lv_timer_pause(view->timer);
    render(view);
}

static void save_tick(lv_timer_t *timer)
{
    wristflow_alarm_view_t *view = lv_timer_get_user_data(timer);
    wristflow_ui_shell_reminder_event(view->shell);
}

static void dismiss(lv_event_t *event)
{
    wristflow_alarm_view_t *view = lv_event_get_user_data(event);
    for (unsigned i = 0; i < WRISTFLOW_ALARM_CAPACITY; ++i)
        if (view->alerts[i].content && lv_event_get_current_target_obj(event) ==
            lv_obj_find_by_name(view->alerts[i].content, "alarm_reminder_close")) {
            wristflow_reminders_remove(view->reminders, (wristflow_reminder_key_t){WF_REMINDER_ALARM, view->alerts[i].id});
            break;
        }
    wristflow_ui_shell_reminder_event(view->shell);
}

static lv_obj_t *alert_content(wristflow_alarm_view_t *view, const wristflow_alarm_pending_t *pending)
{
    unsigned at = WRISTFLOW_ALARM_CAPACITY;
    for (unsigned i = 0; i < WRISTFLOW_ALARM_CAPACITY; ++i)
        if (view->alerts[i].id == pending->alarm.id) { at = i; break; }
    if (at == WRISTFLOW_ALARM_CAPACITY) for (unsigned i = 0; i < WRISTFLOW_ALARM_CAPACITY; ++i)
        if (!view->alerts[i].id || !wristflow_alarm_find(&view->alarms, view->alerts[i].id)) { at = i; break; }
    if (at == WRISTFLOW_ALARM_CAPACITY) return NULL;
    if (!view->alerts[at].content) {
        view->alerts[at].content = alarm_reminder_create(lv_layer_top());
        lv_obj_add_event_cb(lv_obj_find_by_name(view->alerts[at].content, "alarm_reminder_close"), dismiss, LV_EVENT_SHORT_CLICKED, view);
        lv_obj_add_flag(view->alerts[at].content, LV_OBJ_FLAG_HIDDEN);
    }
    /* A coalesced occurrence retains the first pending reminder's snapshot. */
    view->alerts[at].id = pending->alarm.id;
    if (!wristflow_reminders_contains(view->reminders, (wristflow_reminder_key_t){WF_REMINDER_ALARM, pending->alarm.id}))
        lv_label_set_text_fmt(lv_obj_find_by_name(view->alerts[at].content, "alarm_reminder_time"),
            "%02u:%02u", pending->alarm.hour, pending->alarm.minute);
    return view->alerts[at].content;
}

void wristflow_alarm_view_refresh(wristflow_alarm_view_t *view, uint64_t cutoff_ms)
{
    if (!view || !view->backend.read) return;
    wristflow_alarm_snapshot_t snapshot;
    collect_state(view, &snapshot);
    for (unsigned i = 0; i < WRISTFLOW_ALARM_CAPACITY; ++i) if (snapshot.pending[i].active) {
        const wristflow_alarm_pending_t *pending = &snapshot.pending[i];
        if (pending->due_ms > cutoff_ms) continue;
        wristflow_reminder_entry_t entry = {.key = {WF_REMINDER_ALARM, pending->alarm.id},
            .due_ms = pending->due_ms, .order = pending->alarm.id};
        if (wristflow_reminders_submit(view->reminders, entry, alert_content(view, pending)))
            view->backend.ack(view->backend.context, pending->alarm.id, pending->sequence);
    }
}

static void show_time(wristflow_alarm_view_t *view, bool from_settings)
{
    view->time_from_settings = from_settings;
    lv_roller_set_selected(named(view, "alarm_hours"), view->draft.hour, LV_ANIM_OFF);
    lv_roller_set_selected(named(view, "alarm_minutes"), view->draft.minute, LV_ANIM_OFF);
    view->page = ALARM_TIME;
}

static void close_confirmation(wristflow_alarm_view_t *view)
{
    if (!view->confirmation) return;
    lv_obj_t *confirmation = view->confirmation;
    view->confirmation = NULL;
    lv_obj_delete(confirmation);
}

static void confirm(lv_event_t *event)
{
    wristflow_alarm_view_t *view = lv_event_get_user_data(event);
    if (!view->confirmation) return;
    bool accepted = lv_event_get_target(event) == lv_obj_find_by_name(view->confirmation, "confirm_accept");
    close_confirmation(view);
    if (accepted) submit(view, (wristflow_alarm_command_t){.operation = WF_ALARM_REMOVE, .id = view->draft.id});
    render(view);
}

static void action(lv_event_t *event)
{
    wristflow_alarm_view_t *view = lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target(event);
    if (view->confirmation || view->request) return;
    view->failed = false;
    lv_obj_add_flag(named(view, "alarm_error"), LV_OBJ_FLAG_HIDDEN);
    if (target == named(view, "alarm_add") && view->page == ALARM_LIST) {
        if (view->alarms.count >= WRISTFLOW_ALARM_CAPACITY) return;
        view->draft = (wristflow_alarm_t){.hour = view->clock.time_unavailable ? 7 : view->clock.hour_24,
            .minute = view->clock.time_unavailable ? 0 : view->clock.minute, .enabled = true};
        show_time(view, false);
    } else if (target == named(view, "alarm_edit_time") && view->page == ALARM_SETTINGS) show_time(view, true);
    else if (target == named(view, "alarm_time_accept") && view->page == ALARM_TIME) {
        view->draft.hour = lv_roller_get_selected(named(view, "alarm_hours"));
        view->draft.minute = lv_roller_get_selected(named(view, "alarm_minutes"));
        view->page = ALARM_SETTINGS;
    } else if (target == named(view, "alarm_edit_repeat") && view->page == ALARM_SETTINGS) {
        view->repeat_mask = view->draft.weekdays;
        view->page = ALARM_REPEAT;
        lv_obj_scroll_to_y(named(view, "alarm_repeat_scroll"), 0, LV_ANIM_OFF);
    } else if (target == named(view, "alarm_repeat_accept") && view->page == ALARM_REPEAT) {
        view->draft.weekdays = view->repeat_mask; view->page = ALARM_SETTINGS;
    } else if (target == named(view, "alarm_days_accept") && view->page == ALARM_DAYS) {
        if (!view->day_mask) return;
        view->repeat_mask = view->day_mask; view->page = ALARM_REPEAT;
    } else if (target == named(view, "alarm_save") && view->page == ALARM_SETTINGS) {
        submit(view, (wristflow_alarm_command_t){.operation = view->draft.id ? WF_ALARM_EDIT : WF_ALARM_ADD,
            .id = view->draft.id, .draft = view->draft});
    } else if (target == named(view, "alarm_delete") && view->page == ALARM_SETTINGS) {
        if (!view->draft.id) return;
        view->confirmation = component_confirm_create(view->root);
        lv_obj_remove_flag(view->confirmation, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_set_name(view->confirmation, "alarm_delete_confirmation");
        lv_label_set_text(lv_obj_find_by_name(view->confirmation, "confirm_text"), "是否删除当前闹钟？");
        lv_obj_add_event_cb(lv_obj_find_by_name(view->confirmation, "confirm_cancel"), confirm, LV_EVENT_SHORT_CLICKED, view);
        lv_obj_add_event_cb(lv_obj_find_by_name(view->confirmation, "confirm_accept"), confirm, LV_EVENT_SHORT_CLICKED, view);
    } else {
        for (unsigned i = 0; i < WRISTFLOW_ALARM_CAPACITY; ++i) {
            lv_obj_t *row = numbered(view, "alarm_row", i);
            if (view->page == ALARM_LIST && target == lv_obj_find_by_name(row, "alarm_open") && i < view->alarms.count) {
                view->draft = view->alarms.entries[i]; view->page = ALARM_SETTINGS;
            }
            if (view->page == ALARM_REPEAT && target == numbered(view, "alarm_repeat", i)) {
                if (i < 4) view->repeat_mask = presets[i];
                else {
                    view->day_mask = view->repeat_mask; view->page = ALARM_DAYS;
                    lv_obj_scroll_to_y(named(view, "alarm_day_scroll"), 0, LV_ANIM_OFF);
                }
            }
        }
        for (unsigned i = 0; i < 7; ++i)
            if (view->page == ALARM_DAYS && target == numbered(view, "alarm_day", i)) view->day_mask ^= (uint8_t)(1U << i);
    }
    render(view);
}

static void toggle(lv_event_t *event)
{
    wristflow_alarm_view_t *view = lv_event_get_user_data(event);
    if (view->confirmation || view->request || view->page != ALARM_LIST) { render(view); return; }
    for (unsigned i = 0; i < view->alarms.count; ++i) {
        lv_obj_t *control = lv_obj_find_by_name(numbered(view, "alarm_row", i), "alarm_enabled");
        if (lv_event_get_target(event) == control)
            submit(view, (wristflow_alarm_command_t){.operation = WF_ALARM_ENABLE, .id = view->alarms.entries[i].id,
                .enabled = lv_obj_has_state(control, LV_STATE_CHECKED)});
    }
    render(view);
}

wristflow_alarm_view_t *wristflow_alarm_view_create(void)
{ return wristflow_alarm_view_create_with_backend(NULL, NULL, NULL); }

wristflow_alarm_view_t *wristflow_alarm_view_create_with_backend(wristflow_ui_shell_t *shell,
    const wristflow_alarm_port_t *backend, wristflow_reminders_t *reminders)
{
    if (backend && (!backend->request || !backend->status || !backend->read || !backend->ack || !reminders)) return NULL;
    wristflow_alarm_view_t *view = lv_malloc_zeroed(sizeof *view);
    LV_ASSERT_MALLOC(view);
    wristflow_alarms_init(&view->alarms);
    view->clock.time_unavailable = true;
    view->shell = shell; view->reminders = reminders;
    if (backend) {
        view->backend = *backend;
        view->timer = lv_timer_create(save_tick, 100, view); lv_timer_pause(view->timer);
    }
    return view;
}

lv_obj_t *wristflow_alarm_view_screen(wristflow_alarm_view_t *view)
{
    if (!view) return NULL;
    if (view->root) return view->root;
    view->root = screen_alarm_create();
    if (!view->request && !view->failed) view->page = ALARM_LIST;
    lv_obj_set_scroll_dir(named(view, "alarm_list"), LV_DIR_VER);
    lv_obj_set_scroll_dir(named(view, "alarm_repeat_scroll"), LV_DIR_VER);
    lv_obj_set_scroll_dir(named(view, "alarm_day_scroll"), LV_DIR_VER);
    const char *const buttons[] = {"alarm_add", "alarm_edit_time", "alarm_time_accept", "alarm_edit_repeat",
        "alarm_repeat_accept", "alarm_days_accept", "alarm_save", "alarm_delete"};
    for (unsigned i = 0; i < sizeof buttons / sizeof buttons[0]; ++i)
        lv_obj_add_event_cb(named(view, buttons[i]), action, LV_EVENT_SHORT_CLICKED, view);
    for (unsigned i = 0; i < WRISTFLOW_ALARM_CAPACITY; ++i) {
        lv_obj_t *row = numbered(view, "alarm_row", i);
        lv_obj_add_event_cb(lv_obj_find_by_name(row, "alarm_open"), action, LV_EVENT_SHORT_CLICKED, view);
        lv_obj_add_event_cb(lv_obj_find_by_name(row, "alarm_enabled"), toggle, LV_EVENT_VALUE_CHANGED, view);
        lv_obj_add_event_cb(numbered(view, "alarm_repeat", i), action, LV_EVENT_SHORT_CLICKED, view);
    }
    for (unsigned i = 0; i < 7; ++i)
        lv_obj_add_event_cb(numbered(view, "alarm_day", i), action, LV_EVENT_SHORT_CLICKED, view);
    /* Rebuilding a page only copies configuration. Completion/removal belongs
     * to the unified batch, so construction cannot leave a deferred host swap. */
    if (view->backend.read) { wristflow_alarm_snapshot_t snapshot; read_snapshot(view, &snapshot); }
    render(view); return view->root;
}

void wristflow_alarm_view_update(wristflow_alarm_view_t *view, const wristflow_watch_snapshot_t *clock)
{ if (view && clock) { view->clock = *clock; clock_text(view); } }

bool wristflow_alarm_view_back(wristflow_alarm_view_t *view)
{
    if (!view || !view->root) return false;
    if (view->request) return true;
    if (view->confirmation) close_confirmation(view);
    else switch (view->page) {
    case ALARM_LIST: return false;
    case ALARM_DAYS: view->page = ALARM_REPEAT; break;
    case ALARM_REPEAT: view->page = ALARM_SETTINGS; break;
    case ALARM_TIME:
        view->page = view->time_from_settings ? ALARM_SETTINGS : ALARM_LIST;
        if (!view->time_from_settings) view->draft = (wristflow_alarm_t){0};
        break;
    case ALARM_SETTINGS: view->draft = (wristflow_alarm_t){0}; view->page = ALARM_LIST; break;
    }
    render(view); return true;
}

bool wristflow_alarm_view_editing(const wristflow_alarm_view_t *view)
{ return view && view->root && view->page != ALARM_LIST; }

void wristflow_alarm_view_detach(wristflow_alarm_view_t *view)
{
    if (!view) return;
    view->root = NULL; view->confirmation = NULL;
    if (!view->request && !view->failed) { view->page = ALARM_LIST; view->draft = (wristflow_alarm_t){0}; }
}

void wristflow_alarm_view_destroy(wristflow_alarm_view_t *view)
{
    if (!view) return;
    if (view->timer) lv_timer_delete(view->timer);
    for (unsigned i = 0; i < WRISTFLOW_ALARM_CAPACITY; ++i) if (view->alerts[i].content) lv_obj_delete(view->alerts[i].content);
    lv_free(view);
}
