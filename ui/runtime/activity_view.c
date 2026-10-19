#include "activity_view.h"
#include "wristflow_ui.h"
#include <stdio.h>
#include <string.h>

static const char *const ids[] = {"calories", "steps", "active_minutes", "standing"};
static const char *const cards[] = {"activity_calories", "activity_steps", "activity_minutes", "activity_standing"};
static const char *const values[] = {"overview_calories", "overview_steps", "overview_minutes"};
static const char *const units[] = {"千卡", "步", "分钟", "小时"};
static const uint32_t colors[] = {0xf97316, 0xe2db48, 0x38bdf8, 0x10d6b0};

const wristflow_activity_metric_t *wristflow_activity_metric(
    const wristflow_watch_snapshot_t *snapshot, const char *id)
{
    if (!snapshot || !id) return NULL;
    if (!strcmp(id, "activity")) return &snapshot->activity.metrics[0];
    for (unsigned i = 0; i < WRISTFLOW_ACTIVITY_METRICS; ++i)
        if (!strcmp(id, ids[i])) return &snapshot->activity.metrics[i];
    return NULL;
}

void wristflow_activity_value(const wristflow_activity_metric_t *metric, char *text, size_t capacity)
{
    if (metric && metric->total_valid) snprintf(text, capacity, "%lu", (unsigned long)metric->total);
    else snprintf(text, capacity, "--");
}

static lv_obj_t *named(lv_obj_t *root, const char *name)
{
    lv_obj_t *object = lv_obj_find_by_name(root, name);
    LV_ASSERT(object);
    return object;
}

static void ring(lv_obj_t *arc, const wristflow_activity_metric_t *metric, unsigned index, bool white)
{
    bool valid = metric->total_valid && metric->goal_valid && metric->goal;
    uint64_t percent = valid ? (uint64_t)metric->total * 100 / metric->goal : 0;
    lv_arc_set_value(arc, percent > 100 ? 100 : (int32_t)percent);
    lv_obj_set_style_arc_color(arc, lv_color_hex(white ? 0xffffff : colors[index]), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, valid ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, lv_obj_get_style_arc_width(arc, LV_PART_MAIN), LV_PART_INDICATOR);
}

static void rings(lv_obj_t *root, const wristflow_watch_snapshot_t *snapshot, bool white)
{
    for (unsigned i = 0; i < 3; ++i) {
        char name[32]; snprintf(name, sizeof name, "activity_ring_%u", i);
        ring(named(root, name), &snapshot->activity.metrics[i], i, white);
    }
}

static void metric_render(lv_obj_t *card, const wristflow_activity_metric_t *metric, unsigned index)
{
    char text[64]; wristflow_activity_value(metric, text, sizeof text);
    lv_obj_t *value = named(card, "metric_value");
    lv_label_set_text(value, text);
    lv_obj_set_style_text_font(value, metric->total_valid && metric->total > 999999 ? value_36 : metric_56, 0);
    if (metric->goal_valid && metric->goal)
        snprintf(text, sizeof text, "目标 %lu %s", (unsigned long)metric->goal, units[index]);
    else snprintf(text, sizeof text, "目标 -- %s", units[index]);
    lv_label_set_text(named(card, "metric_goal"), text);

    uint32_t mask = metric->hours_valid & 0xffffffU, maximum = 0;
    for (unsigned h = 0; h < 24; ++h)
        if ((mask & (1U << h)) && metric->hourly[h] > maximum) maximum = metric->hourly[h];
    if (mask) snprintf(text, sizeof text, "%lu", (unsigned long)maximum);
    else snprintf(text, sizeof text, "--");
    lv_label_set_text(named(card, "chart_max"), text);
    lv_obj_set_flag(named(card, "chart_empty"), LV_OBJ_FLAG_HIDDEN, mask != 0);
    lv_obj_t *plot = named(card, "metric_plot");
    int width = lv_obj_get_width(plot), height = lv_obj_get_height(plot);
    for (unsigned h = 0; h < 24; ++h) {
        char name[20]; snprintf(name, sizeof name, "hour_%u", h);
        lv_obj_t *bar = named(plot, name);
        bool show = (mask & (1U << h)) && metric->hourly[h] && maximum;
        lv_obj_set_flag(bar, LV_OBJ_FLAG_HIDDEN, !show);
        if (!show) continue;
        int bar_height = (int)((uint64_t)metric->hourly[h] * (height - 4) / maximum);
        if (bar_height < 2) bar_height = 2;
        lv_obj_set_size(bar, width / 24 - 4, bar_height);
        lv_obj_set_pos(bar, h * width / 24 + 2, height - bar_height);
        lv_obj_set_style_bg_color(bar, lv_color_hex(colors[index]), 0);
    }
}

void wristflow_activity_render(lv_obj_t *root, const wristflow_watch_snapshot_t *snapshot)
{
    if (!root || !snapshot) return;
    lv_obj_t *clock = named(root, "activity_time");
    if (snapshot->time_unavailable) lv_label_set_text(clock, "--:--");
    else lv_label_set_text_fmt(clock, "%02u:%02u", snapshot->hour_24, snapshot->minute);
    if (!lv_obj_find_by_name(root, "activity_overview")) return;
    lv_obj_update_layout(root);
    rings(root, snapshot, false);
    for (unsigned i = 0; i < WRISTFLOW_ACTIVITY_METRICS; ++i) {
        const wristflow_activity_metric_t *metric = &snapshot->activity.metrics[i];
        metric_render(named(root, cards[i]), metric, i);
        if (i >= 3) continue;
        char text[32], name[32]; wristflow_activity_value(metric, text, sizeof text);
        lv_obj_t *value = named(root, values[i]); lv_label_set_text(value, text);
        lv_obj_set_style_text_font(value, metric->total_valid && metric->total > 99999 ? title_24 : value_36, 0);
        snprintf(name, sizeof name, "overview_goal_%u", i);
        if (metric->goal_valid && metric->goal) snprintf(text, sizeof text, "/ %lu", (unsigned long)metric->goal);
        else snprintf(text, sizeof text, "/ --");
        lv_label_set_text(named(root, name), text);
    }
}

void wristflow_activity_card(lv_obj_t *card, const wristflow_component_t *component,
    const wristflow_watch_snapshot_t *snapshot)
{
    if (strcmp(component->app_id, "activity") || !lv_obj_find_by_name(card, "activity_ring_0")) return;
    rings(card, snapshot, component->variant != 0);
    static const uint32_t dim[] = {0x593820, 0x4c491f, 0x183f50};
    for (unsigned i = 0; i < 3; ++i) {
        char name[32], text[24];
        snprintf(name, sizeof name, "activity_ring_%u", i);
        lv_obj_set_style_arc_color(named(card, name), lv_color_hex(component->variant ? 0xffffff : dim[i]), LV_PART_MAIN);
        lv_obj_set_style_arc_opa(named(card, name), component->variant ? LV_OPA_30 : LV_OPA_COVER, LV_PART_MAIN);
        if (i) snprintf(name, sizeof name, "activity_value_%u", i);
        else snprintf(name, sizeof name, "value_label");
        wristflow_activity_value(&snapshot->activity.metrics[i], text, sizeof text);
        lv_label_set_text(named(card, name), text);
        lv_obj_set_style_text_color(named(card, name), lv_color_hex(component->variant ? 0xffffff : colors[i]), 0);
    }
}
