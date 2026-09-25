#include "apps.h"
#include "stopwatch.h"
#include "wristflow_ui.h"
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
};

static const wristflow_watchface_t *const faces[] = {
    &wristflow_default_watchface, &wristflow_simple_watchface
};
static const char *const face_names[] = {"扩散", "简洁"};
static const struct {
    const char *name;
    const char *title;
    wristflow_surface_t surface;
} launchers[] = {
    {"launch_stopwatch", "秒表", WRISTFLOW_SURFACE_STOPWATCH},
    {"launch_activity", "今日活动", WRISTFLOW_SURFACE_ACTIVITY},
    {"launch_heart", "心率", WRISTFLOW_SURFACE_HEART},
    {"launch_faces", "表盘", WRISTFLOW_SURFACE_FACE_PICKER},
    {"launch_system", "系统状态", WRISTFLOW_SURFACE_SYSTEM},
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
    lv_obj_t *target = lv_event_get_current_target_obj(event);
    for (unsigned i = 0; i < sizeof(launchers) / sizeof(launchers[0]); ++i) {
        if (target != named(apps->screens[WRISTFLOW_SURFACE_LAUNCHER], launchers[i].name))
            continue;
        wristflow_ui_shell_open(apps->shell, launchers[i].surface);
        return;
    }
}

static void launcher_scroll(lv_event_t *event)
{
    wristflow_apps_t *apps = lv_event_get_user_data(event);
    lv_obj_t *root = apps->screens[WRISTFLOW_SURFACE_LAUNCHER];
    lv_obj_t *scroll = named(root, "launcher_scroll");
    lv_area_t viewport;
    lv_obj_get_coords(scroll, &viewport);
    int cx = (viewport.x1 + viewport.x2) / 2;
    int cy = (viewport.y1 + viewport.y2) / 2;
    unsigned nearest = 0;
    int32_t nearest_distance = INT32_MAX;
    for (unsigned i = 0; i < sizeof(launchers) / sizeof(launchers[0]); ++i) {
        lv_obj_t *icon = named(root, launchers[i].name);
        lv_area_t area;
        lv_obj_get_coords(icon, &area);
        int dx = (area.x1 + area.x2) / 2 - cx;
        int dy = (area.y1 + area.y2) / 2 - cy;
        int32_t distance = dx * dx + dy * dy;
        if (distance < nearest_distance) {
            nearest_distance = distance;
            nearest = i;
        }
        int scale = 256 - distance / 500;
        if (scale < 154) scale = 154;
        lv_obj_set_style_transform_scale_x(icon, scale, 0);
        lv_obj_set_style_transform_scale_y(icon, scale, 0);
    }
    lv_label_set_text(named(root, "launcher_caption"), launchers[nearest].title);
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

static void render_picker(wristflow_apps_t *apps)
{
    lv_obj_t *root = apps->screens[WRISTFLOW_SURFACE_FACE_PICKER];
    bool simple = apps->face_index == 1;
    lv_obj_set_flag(named(root, "face_preview_hour"), LV_OBJ_FLAG_HIDDEN, simple);
    lv_obj_set_flag(named(root, "face_preview_minute"), LV_OBJ_FLAG_HIDDEN, simple);
    lv_obj_set_flag(named(root, "face_preview_simple"), LV_OBJ_FLAG_HIDDEN, !simple);
    lv_label_set_text(named(root, "face_name"), face_names[apps->face_index]);
    lv_obj_set_flag(named(root, "face_selected"), LV_OBJ_FLAG_HIDDEN,
        strcmp(faces[apps->face_index]->id, wristflow_ui_shell_watchface_id(apps->shell)) != 0);
}

static void picker_action(lv_event_t *event)
{
    wristflow_apps_t *apps = lv_event_get_user_data(event);
    if (lv_event_get_current_target_obj(event) == named(apps->screens[WRISTFLOW_SURFACE_FACE_PICKER], "face_apply")) {
        if (wristflow_ui_shell_set_watchface(apps->shell, faces[apps->face_index]))
            wristflow_ui_shell_home(apps->shell);
    } else {
        apps->face_index = (apps->face_index + 1) % 2;
        render_picker(apps);
    }
}

static void sync_brightness(wristflow_apps_t *apps)
{
    lv_slider_set_value(named(apps->controls, "brightness_slider"), apps->brightness, LV_ANIM_OFF);
    lv_obj_t *settings = apps->screens[WRISTFLOW_SURFACE_SETTINGS];
    if (settings) {
        lv_slider_set_value(named(settings, "settings_brightness"), apps->brightness, LV_ANIM_OFF);
        lv_label_set_text_fmt(named(settings, "settings_brightness_value"), "%u%%", (unsigned)apps->brightness);
        lv_label_set_text_fmt(named(settings, "settings_battery"), "%u%%", (unsigned)apps->snapshot.battery_percent);
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

static void bind_click(lv_obj_t *screen, const char *name, lv_event_cb_t callback, wristflow_apps_t *apps)
{
    lv_obj_add_event_cb(named(screen, name), callback, LV_EVENT_SHORT_CLICKED, apps);
}

wristflow_apps_t *wristflow_apps_create(wristflow_ui_shell_t *shell, lv_obj_t *controls,
                                      wristflow_brightness_cb_t brightness, void *context)
{
    wristflow_apps_t *apps = lv_malloc_zeroed(sizeof(*apps));
    LV_ASSERT_MALLOC(apps);
    apps->shell = shell;
    apps->controls = controls;
    apps->brightness = 60;
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

lv_obj_t *wristflow_apps_screen(wristflow_apps_t *apps, wristflow_surface_t surface)
{
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
    case WRISTFLOW_SURFACE_ACTIVITY: root = screen_tile_activity_create(); break;
    case WRISTFLOW_SURFACE_HEART: root = screen_tile_heart_rate_create(); break;
    case WRISTFLOW_SURFACE_SYSTEM: root = screen_tile_system_create(); break;
    default: return NULL;
    }
    apps->screens[surface] = root;
    lv_obj_t *header = lv_obj_find_by_name(root, "app_back");
    if (header) lv_obj_add_event_cb(header, back, LV_EVENT_SHORT_CLICKED, apps);
    if (surface == WRISTFLOW_SURFACE_LAUNCHER) {
        for (unsigned i = 0; i < sizeof(launchers) / sizeof(launchers[0]); ++i) {
            lv_obj_t *icon = named(root, launchers[i].name);
            lv_obj_set_style_transform_pivot_x(icon, 50, 0);
            lv_obj_set_style_transform_pivot_y(icon, 50, 0);
            bind_click(root, launchers[i].name, launch, apps);
        }
        lv_obj_t *scroll = named(root, "launcher_scroll");
        lv_obj_set_scroll_dir(scroll, LV_DIR_ALL);
        lv_obj_remove_flag(scroll, LV_OBJ_FLAG_SCROLL_ELASTIC);
        lv_obj_update_layout(root);
        lv_obj_add_event_cb(scroll, launcher_scroll, LV_EVENT_SCROLL, apps);
        lv_obj_scroll_to(scroll, 130, 124, LV_ANIM_OFF);
    } else if (surface == WRISTFLOW_SURFACE_STOPWATCH) {
        bind_click(root, "stopwatch_toggle", stopwatch_action, apps);
        bind_click(root, "stopwatch_reset", stopwatch_action, apps);
    } else if (surface == WRISTFLOW_SURFACE_FACE_PICKER) {
        bind_click(root, "face_previous", picker_action, apps);
        bind_click(root, "face_next", picker_action, apps);
        bind_click(root, "face_apply", picker_action, apps);
    } else if (surface == WRISTFLOW_SURFACE_FLASHLIGHT) {
        lv_obj_add_event_cb(root, back, LV_EVENT_SHORT_CLICKED, apps);
    } else if (surface == WRISTFLOW_SURFACE_SETTINGS) {
        lv_obj_add_event_cb(named(root, "settings_brightness"), brightness_changed, LV_EVENT_VALUE_CHANGED, apps);
    } else {
        lv_obj_t *dots = lv_obj_find_by_name(root, "page_dots_0");
        if (dots) lv_obj_add_flag(dots, LV_OBJ_FLAG_HIDDEN);
    }
    return root;
}

void wristflow_apps_activate(wristflow_apps_t *apps, wristflow_surface_t surface)
{
    if (!apps) return;
    if (apps->brightness_cb && (surface == WRISTFLOW_SURFACE_FLASHLIGHT || apps->active == WRISTFLOW_SURFACE_FLASHLIGHT))
        apps->brightness_cb(surface == WRISTFLOW_SURFACE_FLASHLIGHT ? 100 : apps->brightness, apps->context);
    apps->active = surface;
    lv_timer_set_period(apps->timer, surface == WRISTFLOW_SURFACE_STOPWATCH ? 40 : 1000);
    wristflow_stopwatch_update(&apps->stopwatch, lv_tick_get());
    if (surface == WRISTFLOW_SURFACE_STOPWATCH) render_stopwatch(apps);
    if (surface == WRISTFLOW_SURFACE_SETTINGS) sync_brightness(apps);
    if (surface == WRISTFLOW_SURFACE_FACE_PICKER) {
        apps->face_index = strcmp(wristflow_ui_shell_watchface_id(apps->shell), "simple") == 0 ? 1 : 0;
        render_picker(apps);
    }
}

void wristflow_apps_update(wristflow_apps_t *apps, const wristflow_watch_snapshot_t *snapshot)
{
    if (!apps) return;
    apps->snapshot = *snapshot;
    sync_brightness(apps);
}

void wristflow_apps_destroy(wristflow_apps_t *apps)
{
    if (!apps) return;
    if (apps->active == WRISTFLOW_SURFACE_FLASHLIGHT && apps->brightness_cb)
        apps->brightness_cb(apps->brightness, apps->context);
    lv_timer_delete(apps->timer);
    for (unsigned i = 0; i < WRISTFLOW_SURFACE_COUNT; ++i)
        if (apps->screens[i]) lv_obj_delete(apps->screens[i]);
    lv_free(apps);
}
