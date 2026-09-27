#include "notification_view.h"
#include "wristflow_ui.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    struct wristflow_notification_view *view;
    lv_obj_t *row;
    int32_t id, pressed_id;
} message_link_t;
struct wristflow_notification_view {
    wristflow_ui_shell_t *shell;
    wristflow_notifications_snapshot_t snapshot;
    lv_obj_t *list, *detail, *rows[WF_PHONE_MESSAGES], *clear;
    lv_obj_t *banner, *preview;
    message_link_t links[WF_PHONE_MESSAGES];
    lv_timer_t *timer;
    uint32_t alert_at;
    int32_t selected, alert_id;
    wristflow_alert_t alert;
};
static lv_obj_t *named(lv_obj_t *root, const char *name)
{ lv_obj_t *o = lv_obj_find_by_name(root, name); LV_ASSERT(o); return o; }
static const wf_phone_message_t *find(const wristflow_notification_view_t *v, int32_t id)
{
    for (unsigned i = 0; i < v->snapshot.count; ++i)
        if (v->snapshot.messages[i].id == id) return &v->snapshot.messages[i];
    return NULL;
}
bool wristflow_notification_view_contains(const wristflow_notification_view_t *v, int32_t id)
{ return v && find(v, id); }
static void paint(lv_obj_t *root, const wf_phone_message_t *m)
{
    lv_label_set_text(named(root, "message_source"), m->source[0] ? m->source : "消息");
    lv_label_set_text(named(root, "message_title"), m->title);
    lv_label_set_text(named(root, "message_body"), m->body);
}
static void detail_refresh(wristflow_notification_view_t *v)
{
    if (!v->detail) return;
    const wf_phone_message_t *m = find(v, v->selected);
    if (m) paint(v->detail, m);
    else {
        lv_label_set_text(named(v->detail, "message_source"), "");
        lv_label_set_text(named(v->detail, "message_title"), "消息已移除");
        lv_label_set_text(named(v->detail, "message_body"), "");
    }
}
static void list_refresh(wristflow_notification_view_t *v)
{
    if (!v->list) return;
    for (unsigned i = 0; i < WF_PHONE_MESSAGES; ++i) {
        lv_obj_set_flag(v->rows[i], LV_OBJ_FLAG_HIDDEN, i >= v->snapshot.count);
        if (i < v->snapshot.count) {
            if (v->links[i].id != v->snapshot.messages[i].id)
                lv_obj_scroll_to_x(v->rows[i], 0, LV_ANIM_OFF);
            v->links[i].id = v->snapshot.messages[i].id;
            paint(v->rows[i], &v->snapshot.messages[i]);
        } else lv_obj_scroll_to_x(v->rows[i], 0, LV_ANIM_OFF);
    }
    lv_obj_set_flag(v->clear, LV_OBJ_FLAG_HIDDEN, !v->snapshot.count);
    lv_obj_set_flag(named(v->list, "notification_empty"), LV_OBJ_FLAG_HIDDEN, v->snapshot.count != 0);
}
static void row_event(lv_event_t *e)
{
    message_link_t *link = lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        link->pressed_id = link->id;
        /* Keep one exposed action, without replacing the touched row objects. */
        for (unsigned i = 0; i < WF_PHONE_MESSAGES; ++i)
            if (link->view->rows[i] != link->row)
                lv_obj_scroll_to_x(link->view->rows[i], 0, LV_ANIM_OFF);
    } else if (lv_event_get_code(e) == LV_EVENT_SHORT_CLICKED && link->pressed_id == link->id) {
        if (lv_event_get_current_target_obj(e) == named(link->row, "notification_row_delete"))
            wristflow_ui_shell_delete_notification(link->view->shell, false, link->id);
        else if (lv_obj_get_scroll_x(link->row) != 0)
            lv_obj_scroll_to_x(link->row, 0, LV_ANIM_ON);
        else wristflow_ui_shell_open_notification(link->view->shell, true, link->id);
    }
}
static void delete_message(lv_event_t *e)
{
    wristflow_notification_view_t *v = lv_event_get_user_data(e);
    wristflow_ui_shell_delete_notification(v->shell, true, 0);
}
static void alert_open(lv_event_t *e)
{
    wristflow_notification_view_t *v = lv_event_get_user_data(e);
    wristflow_ui_shell_open_notification(v->shell, v->alert == WF_ALERT_PREVIEW, v->alert_id);
}
static void preview_gesture(lv_event_t *e)
{
    wristflow_notification_view_t *v = lv_event_get_user_data(e);
    lv_indev_t *input = lv_indev_active();
    if (input && lv_indev_get_gesture_dir(input) == LV_DIR_TOP) {
        wristflow_ui_shell_notification_dismiss(v->shell, false);
        lv_indev_wait_release(input);
    }
}
static void tick(lv_timer_t *timer)
{
    wristflow_notification_view_t *v = lv_timer_get_user_data(timer);
    if (lv_tick_get() - v->alert_at >= (v->alert == WF_ALERT_PREVIEW ? 5000U : 3000U))
        wristflow_ui_shell_notification_dismiss(v->shell, true);
}
wristflow_notification_view_t *wristflow_notification_view_create(wristflow_ui_shell_t *shell)
{
    wristflow_notification_view_t *v = lv_malloc_zeroed(sizeof *v); LV_ASSERT_MALLOC(v);
    v->shell = shell;
    v->banner = notification_banner_create(lv_layer_top()); lv_obj_set_name(v->banner, "notification_banner");
    v->preview = notification_preview_create(lv_layer_top()); lv_obj_set_name(v->preview, "notification_preview");
    lv_obj_add_flag(v->banner, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(v->preview, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(v->banner, LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_remove_flag(v->preview, LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(v->banner, alert_open, LV_EVENT_SHORT_CLICKED, v);
    lv_obj_add_event_cb(named(v->preview, "preview_open"), alert_open, LV_EVENT_SHORT_CLICKED, v);
    lv_obj_add_event_cb(v->preview, preview_gesture, LV_EVENT_GESTURE, v);
    v->timer = lv_timer_create(tick, 50, v); lv_timer_pause(v->timer);
    return v;
}
void wristflow_notification_view_update(wristflow_notification_view_t *v, const wristflow_notifications_snapshot_t *s)
{
    v->snapshot = *s;
    list_refresh(v); detail_refresh(v);
    const wf_phone_message_t *m = find(v, v->alert_id);
    if (v->alert != WF_ALERT_NONE && m) paint(v->alert == WF_ALERT_PREVIEW ? v->preview : v->banner, m);
    else if (v->alert != WF_ALERT_NONE) wristflow_ui_shell_notification_dismiss(v->shell, true);
}
void wristflow_notification_view_select(wristflow_notification_view_t *v, int32_t id)
{
    v->selected = id; detail_refresh(v);
    if (v->detail) lv_obj_scroll_to_y(named(v->detail, "notification_scroll"), 0, LV_ANIM_OFF);
}
lv_obj_t *wristflow_notification_view_screen(wristflow_notification_view_t *v, wristflow_surface_t surface, bool *created)
{
    *created = false;
    if (surface == WRISTFLOW_SURFACE_NOTIFICATIONS) {
        if (v->list) return v->list;
        v->list = screen_notifications_create(); *created = true;
        lv_obj_t *list = named(v->list, "notification_list"); lv_obj_clean(list);
        lv_obj_remove_flag(list, LV_OBJ_FLAG_SCROLL_CHAIN); lv_obj_set_scroll_dir(list, LV_DIR_VER);
        for (unsigned i = 0; i < WF_PHONE_MESSAGES; ++i) {
            v->rows[i] = notification_row_create(list);
            char name[32]; snprintf(name, sizeof name, "notification_%u", i); lv_obj_set_name(v->rows[i], name);
            v->links[i].view = v; v->links[i].row = v->rows[i];
            lv_obj_set_scroll_dir(v->rows[i], LV_DIR_HOR);
            lv_obj_set_scroll_snap_x(v->rows[i], LV_SCROLL_SNAP_END);
            lv_obj_remove_flag(v->rows[i], LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
            lv_obj_add_event_cb(named(v->rows[i], "notification_row_body"), row_event, LV_EVENT_ALL, &v->links[i]);
            lv_obj_add_event_cb(named(v->rows[i], "notification_row_delete"), row_event, LV_EVENT_ALL, &v->links[i]);
        }
        v->clear = settings_choice_create(list, "清空全部"); lv_obj_set_name(v->clear, "notification_clear");
        lv_obj_add_event_cb(v->clear, delete_message, LV_EVENT_SHORT_CLICKED, v);
        list_refresh(v); return v->list;
    }
    if (surface != WRISTFLOW_SURFACE_NOTIFICATION_DETAIL) return NULL;
    if (!v->detail) {
        v->detail = screen_notification_detail_create(); *created = true;
        lv_obj_t *scroll = named(v->detail, "notification_scroll");
        lv_obj_remove_flag(scroll, LV_OBJ_FLAG_SCROLL_CHAIN); lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
        detail_refresh(v);
    }
    return v->detail;
}
void wristflow_notification_view_hide(wristflow_notification_view_t *v)
{
    if (!v) return;
    v->alert = WF_ALERT_NONE; lv_timer_pause(v->timer);
    lv_obj_add_flag(v->banner, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(v->preview, LV_OBJ_FLAG_HIDDEN);
}
void wristflow_notification_view_show(wristflow_notification_view_t *v, wristflow_alert_t kind, int32_t id)
{
    const wf_phone_message_t *m = find(v, id); if (!m) return;
    wristflow_notification_view_hide(v);
    v->alert = kind; v->alert_id = id; v->alert_at = lv_tick_get();
    lv_obj_t *root = kind == WF_ALERT_PREVIEW ? v->preview : v->banner;
    paint(root, m); lv_obj_remove_flag(root, LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(root);
    lv_timer_reset(v->timer); lv_timer_resume(v->timer);
}
wristflow_alert_t wristflow_notification_view_alert(const wristflow_notification_view_t *v)
{ return v ? v->alert : WF_ALERT_NONE; }
void wristflow_notification_view_activity(wristflow_notification_view_t *v)
{ if (v && v->alert == WF_ALERT_PREVIEW) v->alert_at = lv_tick_get(); }
void wristflow_notification_view_destroy(wristflow_notification_view_t *v)
{
    if (!v) return;
    lv_timer_delete(v->timer);
    if (v->list) lv_obj_delete(v->list);
    if (v->detail) lv_obj_delete(v->detail);
    lv_obj_delete(v->banner); lv_obj_delete(v->preview); lv_free(v);
}
