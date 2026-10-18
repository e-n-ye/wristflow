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
    bool percent_valid;
    uint8_t percent;
};

static void progress_line(lv_layer_t *layer, lv_point_t a, lv_point_t b,
    int width, uint32_t *remaining)
{
    uint32_t length = (uint32_t)(LV_ABS(b.x-a.x)+LV_ABS(b.y-a.y));
    if (!*remaining || !length) return;
    uint32_t used = LV_MIN(*remaining, length);
    lv_draw_line_dsc_t line; lv_draw_line_dsc_init(&line);
    line.p1 = (lv_point_precise_t){a.x,a.y};
    line.p2 = (lv_point_precise_t){a.x+(b.x-a.x)*(int32_t)used/(int32_t)length,
        a.y+(b.y-a.y)*(int32_t)used/(int32_t)length};
    line.width = width; line.color = lv_color_hex(0x00dfc0);
    line.round_start = line.round_end = true;
    lv_draw_line(layer, &line); *remaining -= used;
}
static void progress_arc(lv_layer_t *layer, lv_point_t center, int radius,
    int width, unsigned start, uint32_t length, uint32_t *remaining)
{
    if (!*remaining || !length) return;
    uint32_t used = LV_MIN(*remaining, length);
    lv_draw_arc_dsc_t arc; lv_draw_arc_dsc_init(&arc);
    arc.center = center; arc.radius = (uint16_t)(radius+width/2);
    arc.width = width; arc.color = lv_color_hex(0x00dfc0); arc.rounded = true;
    arc.start_angle = start; arc.end_angle = start+used*90U/length;
    if (arc.end_angle > arc.start_angle) lv_draw_arc(layer, &arc);
    *remaining -= used;
}
static void draw_progress(lv_event_t *event)
{
    wristflow_charging_view_t *view = lv_event_get_user_data(event);
    if (!view->percent_valid || !view->percent || view->charge == WF_POWER_CHARGE_FULL) return;
    lv_obj_t *ring = lv_event_get_target_obj(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t area; lv_obj_get_coords(ring, &area);
    int width = lv_obj_get_style_border_width(ring, 0);
    int radius = lv_obj_get_style_radius(ring, 0)-width/2;
    int left=area.x1+width/2, right=area.x2-width/2;
    int top=area.y1+width/2, bottom=area.y2-width/2, middle=(left+right)/2;
    if (radius <= 0 || right-left <= radius*2 || bottom-top <= radius*2) return;
    uint32_t quarter = (uint32_t)radius*355U/226U; /* pi*r/2, integer pixels. */
    uint32_t total = (uint32_t)(2*(right-left-2*radius)+2*(bottom-top-2*radius))+4*quarter;
    uint32_t remaining = total*LV_MIN(view->percent,100U)/100U;
    progress_line(layer,(lv_point_t){middle,top},(lv_point_t){right-radius,top},width,&remaining);
    progress_arc(layer,(lv_point_t){right-radius,top+radius},radius,width,270,quarter,&remaining);
    progress_line(layer,(lv_point_t){right,top+radius},(lv_point_t){right,bottom-radius},width,&remaining);
    progress_arc(layer,(lv_point_t){right-radius,bottom-radius},radius,width,0,quarter,&remaining);
    progress_line(layer,(lv_point_t){right-radius,bottom},(lv_point_t){left+radius,bottom},width,&remaining);
    progress_arc(layer,(lv_point_t){left+radius,bottom-radius},radius,width,90,quarter,&remaining);
    progress_line(layer,(lv_point_t){left,bottom-radius},(lv_point_t){left,top+radius},width,&remaining);
    progress_arc(layer,(lv_point_t){left+radius,top+radius},radius,width,180,quarter,&remaining);
    progress_line(layer,(lv_point_t){left+radius,top},(lv_point_t){middle,top},width,&remaining);
}

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
            lv_obj_add_event_cb(lv_obj_find_by_name(view->root,"charging_ring"), draw_progress, LV_EVENT_DRAW_MAIN, view);
        }
        view->seen_cycle = view->cycle = cycle;
        view->active = opened = true;
    }
    if (view->active && (opened || view->time_unavailable != snapshot->time_unavailable ||
        view->hour != snapshot->hour_24 || view->minute != snapshot->minute || view->charge != snapshot->power.charge ||
        view->percent_valid != snapshot->power.estimate_valid || view->percent != snapshot->power.estimated_percent)) {
        view->time_unavailable = snapshot->time_unavailable;
        view->hour = snapshot->hour_24; view->minute = snapshot->minute;
        view->charge = snapshot->power.charge;
        view->percent_valid = snapshot->power.estimate_valid;
        view->percent = snapshot->power.estimated_percent;
        lv_obj_t *clock = lv_obj_find_by_name(view->root, "charging_clock");
        char text[16];
        if (snapshot->time_unavailable) snprintf(text, sizeof text, "--:--");
        else snprintf(text, sizeof text, "%02u:%02u", snapshot->hour_24, snapshot->minute);
        lv_obj_set_style_text_font(clock, snapshot->time_unavailable ? metric_56 : clock_80, 0);
        lv_label_set_text(clock, text);
        lv_label_set_text(lv_obj_find_by_name(view->root, "charging_state"),
            wristflow_power_charge_text(&snapshot->power));
        lv_obj_t *percent = lv_obj_find_by_name(view->root,"charging_percent");
        if (view->percent_valid) lv_label_set_text_fmt(percent,"~%u%%",view->percent);
        else lv_label_set_text(percent,"--%");
        lv_label_set_text(lv_obj_find_by_name(view->root,"charging_hint"),
            view->percent_valid ? "估算电量" : "电量未知");
        bool full = view->percent_valid && snapshot->power.charge == WF_POWER_CHARGE_FULL;
        lv_obj_set_style_border_color(lv_obj_find_by_name(view->root, "charging_ring"),
            lv_color_hex(full ? 0x00dfc0 : 0x163b38), 0);
        lv_obj_invalidate(lv_obj_find_by_name(view->root,"charging_ring"));
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
