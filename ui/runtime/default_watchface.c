#include "watchface.h"
#include "mount_screen.h"
#include "watchface_presenter.h"

static lv_obj_t *create(lv_obj_t *parent)
{
    return wristflow_mount_screen(parent, screen_watchface_create());
}

static void update(lv_obj_t *root, const wristflow_watch_snapshot_t *snapshot)
{
    bool updated = wristflow_watchface_set_time(root, snapshot->hour_24, snapshot->minute);
    LV_ASSERT(updated);
    lv_obj_t *battery = lv_obj_find_by_name(root, "battery_label");
    LV_ASSERT(battery);
    lv_label_set_text_fmt(battery, "%u%%", (unsigned int)snapshot->battery_percent);
}

const wristflow_watchface_t wristflow_default_watchface = {
    "diffusion", create, update, NULL, NULL
};
