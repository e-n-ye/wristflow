#include "reminder_host.h"

struct wristflow_reminder_host {
    lv_obj_t *root;
    uint32_t activity_at;
    bool active, swallow;
};

static void reset_input(void)
{
    for (lv_indev_t *input = lv_indev_get_next(NULL); input; input = lv_indev_get_next(input)) {
        lv_indev_reset(input, NULL);
        lv_indev_wait_release(input);
    }
}

wristflow_reminder_host_t *wristflow_reminder_host_create(void)
{
    wristflow_reminder_host_t *host = lv_malloc_zeroed(sizeof(*host));
    LV_ASSERT_MALLOC(host);
    host->root = lv_obj_create(lv_layer_top());
    lv_obj_set_name(host->root, "reminder_host");
    lv_obj_remove_style_all(host->root);
    lv_obj_set_size(host->root, 390, 450);
    lv_obj_set_style_bg_color(host->root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(host->root, LV_OPA_COVER, 0);
    lv_obj_remove_flag(host->root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(host->root, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_CLICKABLE);
    return host;
}

bool wristflow_reminder_host_present(wristflow_reminder_host_t *host, lv_obj_t *content, uint32_t now)
{
    if (!host || !content || host->active) return false;
    lv_obj_set_parent(content, host->root);
    lv_obj_set_pos(content, 0, 0);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_remove_flag(host->root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(host->root);
    host->active = true;
    host->activity_at = now;
    host->swallow = false;
    reset_input();
    return true;
}

void wristflow_reminder_host_close(wristflow_reminder_host_t *host)
{
    if (!host || !host->active) return;
    lv_obj_add_flag(host->root, LV_OBJ_FLAG_HIDDEN);
    host->active = false;
    reset_input();
}

bool wristflow_reminder_host_active(const wristflow_reminder_host_t *host)
{ return host && host->active; }

wristflow_display_phase_t wristflow_reminder_host_phase(const wristflow_reminder_host_t *host, uint32_t now)
{ return now - host->activity_at >= 30000U ? WRISTFLOW_DISPLAY_OFF : WRISTFLOW_DISPLAY_ACTIVE; }

void wristflow_reminder_host_wake(wristflow_reminder_host_t *host, uint32_t now)
{ if (host && host->active) host->activity_at = now; }

bool wristflow_reminder_host_touch(wristflow_reminder_host_t *host, uint32_t now, bool pressed)
{
    if (host->swallow) { if (!pressed) host->swallow = false; return false; }
    if (wristflow_reminder_host_phase(host, now) == WRISTFLOW_DISPLAY_OFF) {
        host->swallow = pressed;
        return false;
    }
    if (pressed) host->activity_at = now;
    return true;
}

void wristflow_reminder_host_destroy(wristflow_reminder_host_t *host)
{
    if (!host) return;
    lv_obj_delete(host->root);
    lv_free(host);
}
