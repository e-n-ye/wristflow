#include "apps.h"
#include "stopwatch.h"
#include "wristflow_ui.h"
#include <stdint.h>
#include <string.h>

struct wristflow_apps {
    wristflow_ui_shell_t *shell;
    lv_obj_t *screens[WRISTFLOW_SURFACE_COUNT];
    lv_obj_t *controls;
    lv_timer_t *timer;
    wristflow_stopwatch_t stopwatch;
    wristflow_watch_snapshot_t snapshot;
    wristflow_brightness_cb_t brightness_cb;
    void *context;
    uint8_t brightness;
    unsigned int face_index;
    wristflow_surface_t active;
    lv_obj_t *menu_icons[7];
    lv_point_t menu_centers[7];
    lv_point_t menu_press, menu_origin, menu_start, menu_end;
    bool menu_dragged;
    unsigned int menu_nearest;
    lv_point_t picker_press;
    int32_t picker_origin_scroll;
    unsigned int picker_slot;
    bool picker_dragged;
    bool picker_settling;
};

static const wristflow_watchface_t *const faces[] = {
    &wristflow_default_watchface, &wristflow_simple_watchface
};
static const char *const face_names[] = {"扩散", "简洁"};
static const char *const picker_slots[] = {"face_slot_0", "face_slot_1", "face_slot_2", "face_slot_3"};
static const struct {
    const char *name;
    const char *title;
    wristflow_surface_t surface;
} launchers[] = {
    {"launch_stopwatch", "秒表", WRISTFLOW_SURFACE_STOPWATCH},
    {"launch_activity", "今日活动", WRISTFLOW_SURFACE_ACTIVITY},
    {"launch_heart", "心率", WRISTFLOW_SURFACE_HEART},
    {"launch_faces", "表盘", WRISTFLOW_SURFACE_FACE_PICKER},
    {"launch_system", "电池", WRISTFLOW_SURFACE_SYSTEM},
    {"launch_flashlight", "手电筒", WRISTFLOW_SURFACE_FLASHLIGHT},
    {"launch_settings", "设置", WRISTFLOW_SURFACE_SETTINGS}
};

static lv_obj_t *named(lv_obj_t *root, const char *name)
{
    lv_obj_t *obj = lv_obj_find_by_name(root, name);
    LV_ASSERT(obj);
    return obj;
}

static void back(lv_event_t *event)
{
    wristflow_apps_t *apps = lv_event_get_user_data(event);
    wristflow_ui_shell_back(apps->shell);
}

static void launch(lv_event_t *event)
{
    wristflow_apps_t *apps = lv_event_get_user_data(event);
    if (apps->menu_dragged) return;
    lv_obj_t *target = lv_event_get_current_target_obj(event);
    for (unsigned i = 0; i < sizeof(launchers) / sizeof(launchers[0]); ++i) {
        if (target != named(apps->screens[WRISTFLOW_SURFACE_LAUNCHER], launchers[i].name))
            continue;
        wristflow_ui_shell_open(apps->shell, launchers[i].surface);
        return;
    }
}

static void menu_position(wristflow_apps_t *apps, int x, int y)
{
    lv_obj_t *root = apps->screens[WRISTFLOW_SURFACE_LAUNCHER];
    lv_obj_set_pos(named(root, "launcher_canvas"), x, y);
    unsigned nearest = 0;
    int32_t nearest_distance = INT32_MAX;
    for (unsigned i = 0; i < sizeof(launchers) / sizeof(launchers[0]); ++i) {
        lv_obj_t *icon = apps->menu_icons[i];
        int dx = apps->menu_centers[i].x + x - 195;
        int dy = apps->menu_centers[i].y + y - 201;
        int32_t distance = dx * dx + dy * dy;
        if (distance < nearest_distance) {
            nearest_distance = distance;
            nearest = i;
        }
        /* Resize the circle directly: transformed widget trees allocate and
           composite an intermediate layer in the software renderer. */
        int size = 100 - distance / 1280;
        if (size < 64) size = 64;
        lv_obj_set_size(icon, size, size);
        lv_obj_set_pos(icon, apps->menu_centers[i].x - size / 2,
                            apps->menu_centers[i].y - size / 2);
    }
    if (apps->menu_nearest != nearest) {
        apps->menu_nearest = nearest;
        lv_label_set_text(named(root, "launcher_caption"), launchers[nearest].title);
    }
}

static void menu_snap(void *context, int32_t value)
{
    wristflow_apps_t *apps = context;
    menu_position(apps, apps->menu_start.x + (apps->menu_end.x - apps->menu_start.x) * value / 256,
                        apps->menu_start.y + (apps->menu_end.y - apps->menu_start.y) * value / 256);
}

static void menu_touch(lv_event_t *event)
{
    wristflow_apps_t *apps = lv_event_get_user_data(event);
    if (apps->active != WRISTFLOW_SURFACE_LAUNCHER) return;
    lv_indev_t *input = lv_indev_active();
    if (!input) return;
    lv_obj_t *canvas = named(apps->screens[WRISTFLOW_SURFACE_LAUNCHER], "launcher_canvas");
    lv_event_code_t code = lv_event_get_code(event);
    lv_point_t point;
    lv_indev_get_point(input, &point);
    if (code == LV_EVENT_PRESSED) {
        lv_anim_delete(apps, menu_snap);
        apps->menu_press = point;
        apps->menu_origin = (lv_point_t){lv_obj_get_x(canvas), lv_obj_get_y(canvas)};
        apps->menu_dragged = false;
    } else if (code == LV_EVENT_PRESSING) {
        int dx = point.x - apps->menu_press.x, dy = point.y - apps->menu_press.y;
        if (LV_ABS(dx) > 8 || LV_ABS(dy) > 8) apps->menu_dragged = true;
        if (!apps->menu_dragged) return;
        int x = LV_CLAMP(-330, apps->menu_origin.x + dx, 70);
        int y = LV_CLAMP(-360, apps->menu_origin.y + dy, 60);
        menu_position(apps, x, y);
    } else if ((code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) && apps->menu_dragged) {
        apps->menu_start = (lv_point_t){lv_obj_get_x(canvas), lv_obj_get_y(canvas)};
        apps->menu_end = (lv_point_t){195 - apps->menu_centers[apps->menu_nearest].x,
                                    201 - apps->menu_centers[apps->menu_nearest].y};
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, apps);
        lv_anim_set_exec_cb(&animation, menu_snap);
        lv_anim_set_values(&animation, 0, 256);
        lv_anim_set_duration(&animation, 180);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
    }
}

static void render_stopwatch(wristflow_apps_t *apps)
{
    lv_obj_t *root = apps->screens[WRISTFLOW_SURFACE_STOPWATCH];
    unsigned long seconds = (unsigned long)(apps->stopwatch.elapsed_ms / 1000);
    if (seconds < 3600)
        lv_label_set_text_fmt(named(root, "stopwatch_time"), "%02lu:%02lu", seconds / 60, seconds % 60);
    else
        lv_label_set_text_fmt(named(root, "stopwatch_time"), "%02lu:%02lu:%02lu",
                             seconds / 3600 % 100, seconds / 60 % 60, seconds % 60);
    lv_label_set_text_fmt(named(root, "stopwatch_fraction"), ".%02u",
                         (unsigned)(apps->stopwatch.elapsed_ms / 10 % 100));
    lv_label_set_text(named(root, "stopwatch_toggle_icon"), apps->stopwatch.running ? "\xef\x81\x8c" : "\xef\x81\x8b");
}

static void stopwatch_action(lv_event_t *event)
{
    wristflow_apps_t *apps = lv_event_get_user_data(event);
    if (lv_event_get_current_target_obj(event) == named(apps->screens[WRISTFLOW_SURFACE_STOPWATCH], "stopwatch_reset"))
        wristflow_stopwatch_reset(&apps->stopwatch, lv_tick_get());
    else
        wristflow_stopwatch_toggle(&apps->stopwatch, lv_tick_get());
    if (apps->stopwatch.running) lv_timer_resume(apps->timer);
    else lv_timer_pause(apps->timer);
    render_stopwatch(apps);
}

static void tick(lv_timer_t *timer)
{
    wristflow_apps_t *apps = lv_timer_get_user_data(timer);
    wristflow_stopwatch_update(&apps->stopwatch, lv_tick_get());
    if (apps->active == WRISTFLOW_SURFACE_STOPWATCH && apps->stopwatch.running)
        render_stopwatch(apps);
}

static void picker_apply(wristflow_apps_t *apps)
{
    if (wristflow_ui_shell_set_watchface(apps->shell, faces[apps->face_index]))
        wristflow_ui_shell_home(apps->shell);
}

static bool picker_slot_simple(unsigned int slot)
{
    return (slot & 1U) == 0U;
}

static unsigned int picker_slot_face(unsigned int slot)
{
    return picker_slot_simple(slot) ? 1U : 0U;
}

static void picker_render(wristflow_apps_t *apps, unsigned int slot)
{
    lv_obj_t *root = apps->screens[WRISTFLOW_SURFACE_FACE_PICKER];
    if (!root) return;
    if (slot > 3U) slot = 3U;
    apps->picker_slot = slot;
    apps->face_index = picker_slot_face(slot);
    lv_label_set_text(named(root, "face_name"), face_names[apps->face_index]);
    lv_obj_set_flag(named(root, "face_selected"), LV_OBJ_FLAG_HIDDEN,
        strcmp(faces[apps->face_index]->id, wristflow_ui_shell_watchface_id(apps->shell)) != 0);
}

static void picker_snap(void *context, int32_t value)
{
    wristflow_apps_t *apps = context;
    lv_obj_t *carousel = named(apps->screens[WRISTFLOW_SURFACE_FACE_PICKER], "face_carousel");
    int32_t target = (int32_t)apps->picker_slot * 266;
    int32_t start = apps->picker_origin_scroll;
    lv_obj_scroll_to_x(carousel, start + (target - start) * value / 256, LV_ANIM_OFF);
}

static void picker_snap_ready(lv_anim_t *animation)
{
    wristflow_apps_t *apps = lv_anim_get_user_data(animation);
    apps->picker_settling = false;
    /* End clones make browsing wrap without exposing an empty edge. */
    if (apps->picker_slot == 0 || apps->picker_slot == 3) {
        apps->picker_slot = apps->picker_slot == 0 ? 2 : 1;
        lv_obj_scroll_to_x(named(apps->screens[WRISTFLOW_SURFACE_FACE_PICKER], "face_carousel"),
                           (int32_t)apps->picker_slot * 266, LV_ANIM_OFF);
    }
    picker_render(apps, apps->picker_slot);
}

static void picker_touch(lv_event_t *event)
{
    wristflow_apps_t *apps = lv_event_get_user_data(event);
    if (apps->active != WRISTFLOW_SURFACE_FACE_PICKER) return;
    lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING &&
        code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) return;
    lv_indev_t *input = lv_indev_active();
    if (!input) return;
    lv_point_t point;
    lv_indev_get_point(input, &point);
    lv_obj_t *carousel = named(apps->screens[WRISTFLOW_SURFACE_FACE_PICKER], "face_carousel");
    if (code == LV_EVENT_PRESSED) {
        bool interrupted = apps->picker_settling;
        lv_anim_delete(apps, picker_snap);
        apps->picker_settling = false;
        apps->picker_press = point;
        apps->picker_origin_scroll = lv_obj_get_scroll_x(carousel);
        apps->picker_dragged = interrupted;
    } else if (code == LV_EVENT_PRESSING) {
        if (LV_ABS(point.x - apps->picker_press.x) > 12)
            apps->picker_dragged = true;
        if (apps->picker_dragged) {
            int32_t scroll = LV_CLAMP(0, apps->picker_origin_scroll -
                                         (point.x - apps->picker_press.x), 798);
            lv_obj_scroll_to_x(carousel, scroll, LV_ANIM_OFF);
            unsigned int slot = (unsigned int)LV_CLAMP(0, (scroll + 133) / 266, 3);
            if (slot != apps->picker_slot) picker_render(apps, slot);
        }
    } else if ((code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) &&
               apps->picker_dragged) {
        int32_t scroll = lv_obj_get_scroll_x(carousel);
        apps->picker_slot = (unsigned int)LV_CLAMP(0, (scroll + 133) / 266, 3);
        apps->picker_origin_scroll = scroll;
        apps->picker_settling = true;
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, apps);
        lv_anim_set_user_data(&animation, apps);
        lv_anim_set_exec_cb(&animation, picker_snap);
        lv_anim_set_values(&animation, 0, 256);
        lv_anim_set_duration(&animation, 180);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_set_completed_cb(&animation, picker_snap_ready);
        lv_anim_start(&animation);
    } else if (code == LV_EVENT_RELEASED && !apps->picker_dragged &&
               !apps->picker_settling) {
        lv_area_t area;
        lv_obj_get_coords(named(apps->screens[WRISTFLOW_SURFACE_FACE_PICKER],
                                picker_slots[apps->picker_slot]), &area);
        if (point.x >= area.x1 && point.x <= area.x2 &&
            point.y >= area.y1 && point.y <= area.y2)
            picker_apply(apps);
    }
}

static void sync_brightness(wristflow_apps_t *apps)
{
    lv_slider_set_value(named(apps->controls, "brightness_slider"), apps->brightness, LV_ANIM_OFF);
    lv_obj_t *settings = apps->screens[WRISTFLOW_SURFACE_SETTINGS];
    if (settings) {
        lv_slider_set_value(named(settings, "settings_brightness"), apps->brightness, LV_ANIM_OFF);
        lv_label_set_text_fmt(named(settings, "settings_brightness_value"), "%u%%", (unsigned)apps->brightness);
        lv_obj_set_style_text_font(named(settings, "settings_battery"),
            apps->snapshot.battery_unavailable ? LV_FONT_DEFAULT : body_20, 0);
        if (apps->snapshot.battery_unavailable) lv_label_set_text(named(settings, "settings_battery"), "USB");
        else lv_label_set_text_fmt(named(settings, "settings_battery"), "%u%%", (unsigned)apps->snapshot.battery_percent);
    }
}

static void brightness_changed(lv_event_t *event)
{
    wristflow_apps_t *apps = lv_event_get_user_data(event);
    apps->brightness = (uint8_t)lv_slider_get_value(lv_event_get_current_target_obj(event));
    sync_brightness(apps);
    if (apps->brightness_cb) apps->brightness_cb(apps->brightness, apps->context);
}

static void control_action(lv_event_t *event)
{
    wristflow_apps_t *apps = lv_event_get_user_data(event);
    wristflow_surface_t destination = lv_event_get_current_target_obj(event) == named(apps->controls, "settings_button")
        ? WRISTFLOW_SURFACE_SETTINGS : WRISTFLOW_SURFACE_FLASHLIGHT;
    wristflow_ui_shell_open(apps->shell, destination);
}

static void open_battery(lv_event_t *event)
{
    wristflow_apps_t *apps = lv_event_get_user_data(event);
    wristflow_ui_shell_open(apps->shell, WRISTFLOW_SURFACE_SYSTEM);
}

static void bind_click(lv_obj_t *screen, const char *name, lv_event_cb_t callback, wristflow_apps_t *apps)
{
    lv_obj_add_event_cb(named(screen, name), callback, LV_EVENT_SHORT_CLICKED, apps);
}

wristflow_apps_t *wristflow_apps_create(wristflow_ui_shell_t *shell, lv_obj_t *controls,
                                      wristflow_brightness_cb_t brightness, void *context,
                                      uint8_t initial_brightness)
{
    wristflow_apps_t *apps = lv_malloc_zeroed(sizeof(*apps));
    LV_ASSERT_MALLOC(apps);
    apps->shell = shell;
    apps->controls = controls;
    apps->brightness = initial_brightness;
    apps->brightness_cb = brightness;
    apps->context = context;
    apps->timer = lv_timer_create(tick, 40, apps);
    lv_timer_pause(apps->timer);
    bind_click(controls, "flashlight_button", control_action, apps);
    bind_click(controls, "settings_button", control_action, apps);
    lv_obj_t *slider = named(controls, "brightness_slider");
    lv_slider_set_range(slider, 10, 100);
    lv_obj_add_event_cb(slider, brightness_changed, LV_EVENT_VALUE_CHANGED, apps);
    /* These two policies require the future notification/power services. */
    lv_obj_add_state(named(controls, "dnd_button"), LV_STATE_DISABLED);
    lv_obj_add_state(named(controls, "keep_awake_button"), LV_STATE_DISABLED);
    if (brightness) brightness(apps->brightness, context);
    return apps;
}

uint8_t wristflow_apps_brightness(const wristflow_apps_t *apps)
{
    return apps ? apps->brightness : 60;
}

lv_obj_t *wristflow_apps_screen(wristflow_apps_t *apps, wristflow_surface_t surface, bool *created)
{
    if (created) *created = false;
    if (!apps || surface < WRISTFLOW_SURFACE_LAUNCHER || surface >= WRISTFLOW_SURFACE_COUNT)
        return NULL;
    if (apps->screens[surface]) return apps->screens[surface];
    lv_obj_t *root = NULL;
    switch (surface) {
    case WRISTFLOW_SURFACE_LAUNCHER: root = screen_launcher_create(); break;
    case WRISTFLOW_SURFACE_STOPWATCH: root = screen_stopwatch_create(); break;
    case WRISTFLOW_SURFACE_FACE_PICKER: root = screen_face_picker_create(); break;
    case WRISTFLOW_SURFACE_FLASHLIGHT: root = screen_flashlight_create(); break;
    case WRISTFLOW_SURFACE_SETTINGS: root = screen_settings_create(); break;
    case WRISTFLOW_SURFACE_ACTIVITY: root = screen_app_placeholder_create(); break;
    case WRISTFLOW_SURFACE_HEART: root = screen_app_placeholder_create(); break;
    case WRISTFLOW_SURFACE_SYSTEM: root = screen_app_placeholder_create(); break;
    default: return NULL;
    }
    if (!root) return NULL;
    apps->screens[surface] = root;
    if (created) *created = true;
    lv_obj_t *header = lv_obj_find_by_name(root, "app_back");
    if (header) lv_obj_add_event_cb(header, back, LV_EVENT_SHORT_CLICKED, apps);
    if (surface == WRISTFLOW_SURFACE_LAUNCHER) {
        lv_obj_update_layout(root);
        for (unsigned i = 0; i < sizeof(launchers) / sizeof(launchers[0]); ++i) {
            lv_obj_t *icon = named(root, launchers[i].name);
            apps->menu_icons[i] = icon;
            apps->menu_centers[i] = (lv_point_t){lv_obj_get_x(icon) + 50, lv_obj_get_y(icon) + 50};
            lv_obj_add_flag(icon, LV_OBJ_FLAG_PRESS_LOCK);
            bind_click(root, launchers[i].name, launch, apps);
        }
        lv_obj_t *scroll = named(root, "launcher_scroll");
        lv_obj_remove_flag(scroll, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(scroll, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
        lv_obj_add_event_cb(scroll, menu_touch, LV_EVENT_ALL, apps);
        apps->menu_nearest = UINT32_MAX;
        menu_position(apps, -130, -124);
    } else if (surface == WRISTFLOW_SURFACE_STOPWATCH) {
        bind_click(root, "stopwatch_toggle", stopwatch_action, apps);
        bind_click(root, "stopwatch_reset", stopwatch_action, apps);
    } else if (surface == WRISTFLOW_SURFACE_FACE_PICKER) {
        lv_obj_t *carousel = named(root, "face_carousel");
        lv_obj_remove_flag(carousel, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(carousel, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
        for (unsigned int i = 0; i < 4; ++i) {
            lv_obj_t *thumbnail = named(root, picker_slots[i]);
            lv_obj_add_flag(thumbnail, LV_OBJ_FLAG_PRESS_LOCK);
            lv_obj_set_flag(named(thumbnail, "thumb_simple"), LV_OBJ_FLAG_HIDDEN,
                            !picker_slot_simple(i));
        }
        lv_obj_add_event_cb(carousel, picker_touch, LV_EVENT_ALL, apps);
        apps->face_index = strcmp(wristflow_ui_shell_watchface_id(apps->shell), "simple") == 0 ? 1 : 0;
        apps->picker_slot = apps->face_index == 1 ? 2 : 1;
    } else if (surface == WRISTFLOW_SURFACE_FLASHLIGHT) {
        lv_obj_add_event_cb(root, back, LV_EVENT_SHORT_CLICKED, apps);
    } else if (surface == WRISTFLOW_SURFACE_SETTINGS) {
        lv_obj_add_event_cb(named(root, "settings_brightness"), brightness_changed, LV_EVENT_VALUE_CHANGED, apps);
        lv_obj_add_flag(named(root, "settings_battery"), LV_OBJ_FLAG_CLICKABLE);
        bind_click(root, "settings_battery", open_battery, apps);
    } else {
        const char *title = surface == WRISTFLOW_SURFACE_ACTIVITY ? "今日活动" :
                            surface == WRISTFLOW_SURFACE_HEART ? "心率" : "电池";
        const char *icon = surface == WRISTFLOW_SURFACE_ACTIVITY ? "\xef\x95\x8b" :
                           surface == WRISTFLOW_SURFACE_HEART ? "\xef\x80\x84" : "\xef\x89\x80";
        lv_label_set_text(named(root, "app_title"), title);
        lv_label_set_text(named(root, "placeholder_icon"), icon);
        if (apps->snapshot.metrics_unavailable) {
            lv_obj_t *status = named(root, "placeholder_label");
            lv_obj_set_style_text_font(status, LV_FONT_DEFAULT, 0);
            lv_label_set_text(status, surface == WRISTFLOW_SURFACE_SYSTEM ? "USB / No battery" : "--");
        }
    }
    return root;
}

static void stop_view_effects(wristflow_apps_t *apps, wristflow_surface_t surface)
{
    if (surface == WRISTFLOW_SURFACE_LAUNCHER) lv_anim_delete(apps, menu_snap);
    if (surface == WRISTFLOW_SURFACE_FACE_PICKER) {
        lv_anim_delete(apps, picker_snap);
        apps->picker_settling = false;
    }
}

void wristflow_apps_activate(wristflow_apps_t *apps, wristflow_surface_t surface)
{
    if (!apps || apps->active == surface) return;
    stop_view_effects(apps, apps->active);
    if (apps->brightness_cb && (surface == WRISTFLOW_SURFACE_FLASHLIGHT || apps->active == WRISTFLOW_SURFACE_FLASHLIGHT))
        apps->brightness_cb(surface == WRISTFLOW_SURFACE_FLASHLIGHT ? 100 : apps->brightness, apps->context);
    apps->active = surface;
    lv_timer_set_period(apps->timer, surface == WRISTFLOW_SURFACE_STOPWATCH ? 40 : 1000);
    wristflow_stopwatch_update(&apps->stopwatch, lv_tick_get());
    if (surface == WRISTFLOW_SURFACE_STOPWATCH) {
        if (apps->stopwatch.running) lv_timer_resume(apps->timer);
        else lv_timer_pause(apps->timer);
        render_stopwatch(apps);
    } else {
        /* Keep the stopwatch model ticking once per second while hidden.  It
         * does not render outside its screen, and this keeps its last tick
         * current across long periods away from the app. */
        if (apps->stopwatch.running) lv_timer_resume(apps->timer);
        else lv_timer_pause(apps->timer);
    }
    if (surface == WRISTFLOW_SURFACE_SETTINGS) sync_brightness(apps);
    if (surface == WRISTFLOW_SURFACE_FACE_PICKER) {
        /* Resume a retained parent at the selected slot; a new view starts at
           the current face. A cancelled snap must not leave it between cards. */
        if (apps->picker_slot == 0 || apps->picker_slot == 3)
            apps->picker_slot = apps->picker_slot == 0 ? 2 : 1;
        apps->picker_settling = false;
        lv_obj_scroll_to_x(named(apps->screens[surface], "face_carousel"),
                           (int32_t)apps->picker_slot * 266, LV_ANIM_OFF);
        picker_render(apps, apps->picker_slot);
    }
}

void wristflow_apps_update(wristflow_apps_t *apps, const wristflow_watch_snapshot_t *snapshot)
{
    if (!apps) return;
    apps->snapshot = *snapshot;
    sync_brightness(apps);
}

static void release_screen(wristflow_apps_t *apps, wristflow_surface_t surface)
{
    lv_obj_t *root = apps->screens[surface];
    if (!root) return;
    stop_view_effects(apps, surface);
    apps->screens[surface] = NULL;
    if (surface == WRISTFLOW_SURFACE_LAUNCHER)
        memset(apps->menu_icons, 0, sizeof apps->menu_icons);
    lv_obj_delete(root);
}

void wristflow_apps_collect(wristflow_apps_t *apps, const wristflow_navigation_t *navigation)
{
    if (!apps) return;
    for (unsigned int i = WRISTFLOW_SURFACE_STOPWATCH; i < WRISTFLOW_SURFACE_COUNT; ++i)
        if (!wristflow_navigation_contains(navigation, (wristflow_surface_t)i))
            release_screen(apps, (wristflow_surface_t)i);
}

void wristflow_apps_destroy(wristflow_apps_t *apps)
{
    if (!apps) return;
    if (apps->active == WRISTFLOW_SURFACE_FLASHLIGHT && apps->brightness_cb)
        apps->brightness_cb(apps->brightness, apps->context);
    lv_timer_delete(apps->timer);
    for (unsigned i = 0; i < WRISTFLOW_SURFACE_COUNT; ++i)
        release_screen(apps, (wristflow_surface_t)i);
    lv_free(apps);
}
