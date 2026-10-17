#include "charging_view.h"
#include "wristflow_ui.h"
#include <stdio.h>

struct wristflow_charging_view {
    lv_obj_t *root;
    uint32_t seen_cycle, cycle;
    bool active;
    bool time_unavailable;
    uint8_t hour, minute;
    wristflow_charge_display_t charge;
};

wristflow_charging_view_t *wristflow_charging_view_create(void)
{
    wristflow_charging_view_t *view = lv_malloc_zeroed(sizeof *view);
    LV_ASSERT_MALLOC(view);
    return view;
}

bool wristflow_charging_view_update(wristflow_charging_view_t *view,
    const wristflow_watch_snapshot_t *snapshot, bool allow_present)
{
    if (!view || !snapshot) return false;
    uint32_t cycle = snapshot->power.charge_cycle;
    if (view->active && view->cycle != cycle) wristflow_charging_view_dismiss(view);
    bool opened = false;
    if (!view->active && cycle && cycle != view->seen_cycle && allow_present &&
        snapshot->power.charge == WF_POWER_CHARGE_ACTIVE) {
        if (!view->root) {
            view->root = charging_panel_create(lv_layer_top());
            lv_obj_remove_flag(view->root, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
        }
        view->seen_cycle = view->cycle = cycle;
        view->active = opened = true;
    }
    if (view->active && (opened || view->time_unavailable != snapshot->time_unavailable ||
        view->hour != snapshot->hour_24 || view->minute != snapshot->minute || view->charge != snapshot->power.charge)) {
        view->time_unavailable = snapshot->time_unavailable;
        view->hour = snapshot->hour_24; view->minute = snapshot->minute;
        view->charge = snapshot->power.charge;
        lv_obj_t *clock = lv_obj_find_by_name(view->root, "charging_clock");
        char text[16];
        if (snapshot->time_unavailable) snprintf(text, sizeof text, "--:--");
        else snprintf(text, sizeof text, "%02u:%02u", snapshot->hour_24, snapshot->minute);
        lv_obj_set_style_text_font(clock, snapshot->time_unavailable ? metric_56 : clock_80, 0);
        lv_label_set_text(clock, text);
        lv_label_set_text(lv_obj_find_by_name(view->root, "charging_state"),
            wristflow_power_charge_text(&snapshot->power));
        bool full = snapshot->power.charge == WF_POWER_CHARGE_FULL;
        lv_obj_set_style_border_color(lv_obj_find_by_name(view->root, "charging_ring"),
            lv_color_hex(full ? 0x00dfc0 : 0x163b38), 0);
    }
    return opened;
}

void wristflow_charging_view_cover(wristflow_charging_view_t *view, bool covered)
{
    if (!view || !view->root) return;
    lv_obj_set_flag(view->root, LV_OBJ_FLAG_HIDDEN, !view->active || covered);
    if (view->active && !covered) lv_obj_move_foreground(view->root);
}

void wristflow_charging_view_dismiss(wristflow_charging_view_t *view)
{
    if (!view) return;
    view->active = false;
    if (view->root) lv_obj_add_flag(view->root, LV_OBJ_FLAG_HIDDEN);
}
bool wristflow_charging_view_active(const wristflow_charging_view_t *view)
{ return view && view->active; }
void wristflow_charging_view_destroy(wristflow_charging_view_t *view)
{
    if (!view) return;
    if (view->root) lv_obj_delete(view->root);
    lv_free(view);
}
