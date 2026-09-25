#include "watchface.h"
#include "mount_screen.h"
#include "wristflow_ui.h"

static lv_obj_t *create(lv_obj_t *parent)
{
    return wristflow_mount_screen(parent, screen_watchface_simple_create());
}

static void update(lv_obj_t *root, const wristflow_watch_snapshot_t *snapshot)
{
    lv_label_set_text_fmt(lv_obj_find_by_name(root, "simple_time"), "%02u:%02u",
                         (unsigned)snapshot->hour_24, (unsigned)snapshot->minute);
    lv_label_set_text_fmt(lv_obj_find_by_name(root, "simple_battery"), "%u%%",
                         (unsigned)snapshot->battery_percent);
}

const wristflow_watchface_t wristflow_simple_watchface = {
    "simple", create, update, NULL, NULL
};
