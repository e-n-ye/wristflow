#include "ui_shell.h"
#include "mount_screen.h"
#include "apps.h"

#define PAGE_WIDTH 390
#define PAGE_HEIGHT 450

typedef struct {
    lv_obj_t *panel;
    lv_obj_t *face;
    lv_obj_t *pending_face;
    bool visible;
} page_slot_t;

struct wristflow_ui_shell {
    lv_obj_t *home;
    lv_obj_t *carousel;
    lv_obj_t *controls;
    lv_obj_t *screens[WRISTFLOW_SURFACE_COUNT];
    wristflow_apps_t *apps;
    lv_obj_t **indicators;
    page_slot_t *slots;
    const wristflow_watchface_t *watchface;
    wristflow_navigation_t navigation;
    wristflow_watch_snapshot_t snapshot;
    bool recentering;
    bool transitioning;
    bool ready;
    bool edge_press;
};

static void gesture(lv_event_t *event);
static void face_long_press(lv_event_t *event);

static void bubble_events(lv_obj_t *obj)
{
    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i)
        bubble_events(lv_obj_get_child(obj, i));
}

static unsigned int slot_count(const wristflow_ui_shell_t *shell)
{
    return shell->navigation.page_count + 2;
}

static void set_visible(wristflow_ui_shell_t *shell, page_slot_t *slot, bool visible)
{
    if (slot->face && slot->visible != visible) {
        slot->visible = visible;
        if (shell->watchface->set_visible)
            shell->watchface->set_visible(slot->face, visible);
    }
}

static void sync_visibility(wristflow_ui_shell_t *shell)
{
    int32_t x = lv_obj_get_scroll_x(shell->carousel);
    bool home_visible = shell->ready && !shell->transitioning &&
                        shell->navigation.surface == WRISTFLOW_SURFACE_HOME;
    for (unsigned int i = 0; i < slot_count(shell); ++i) {
        int32_t left = (int32_t)i * PAGE_WIDTH - x;
        set_visible(shell, &shell->slots[i], home_visible && left < PAGE_WIDTH &&
                                            left + PAGE_WIDTH > 0);
    }
}

static void scroll_event(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    if (shell->recentering)
        return;
    if (lv_event_get_code(event) == LV_EVENT_SCROLL_END &&
        !lv_obj_is_scrolling(shell->carousel)) {
        int32_t x = lv_obj_get_scroll_x(shell->carousel);
        if (x >= 0 && x % PAGE_WIDTH == 0) {
            unsigned int slot = (unsigned int)(x / PAGE_WIDTH);
            if (slot >= slot_count(shell))
                return;
            if (slot == 0 || slot == shell->navigation.page_count + 1) {
                shell->recentering = true;
                slot = slot == 0 ? shell->navigation.page_count : 1;
                lv_obj_scroll_to_x(shell->carousel, (int32_t)slot * PAGE_WIDTH, LV_ANIM_OFF);
                shell->recentering = false;
            }
            wristflow_navigation_commit_page(&shell->navigation, slot - 1);
            for (unsigned int i = 0; i < shell->navigation.page_count; ++i)
                if (shell->indicators[i])
                    lv_obj_set_flag(shell->indicators[i], LV_OBJ_FLAG_HIDDEN,
                                    i != shell->navigation.page_index);
        }
    }
    sync_visibility(shell);
}

static void screen_loaded(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    shell->transitioning = false;
    sync_visibility(shell);
}

static void transition(wristflow_ui_shell_t *shell, lv_obj_t *screen, lv_screen_load_anim_t animation)
{
    wristflow_apps_activate(shell->apps, shell->navigation.surface);
    shell->transitioning = true;
    sync_visibility(shell);
    lv_screen_load_anim(screen, animation, 180, 0, false);
}

static void app_pressed(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    lv_indev_t *input = lv_indev_active();
    lv_point_t point;
    if (input) {
        lv_indev_get_point(input, &point);
        shell->edge_press = point.x <= 40;
    }
}

static bool load_surface(wristflow_ui_shell_t *shell)
{
    wristflow_surface_t surface = shell->navigation.surface;
    if (!shell->screens[surface]) {
        lv_obj_t *screen = wristflow_apps_screen(shell->apps, surface);
        if (!screen) return false;
        shell->screens[surface] = screen;
        bubble_events(screen);
        lv_obj_add_event_cb(screen, screen_loaded, LV_EVENT_SCREEN_LOADED, shell);
        lv_obj_add_event_cb(screen, app_pressed, LV_EVENT_PRESSED, shell);
        lv_obj_add_event_cb(screen, gesture, LV_EVENT_GESTURE, shell);
    }
    if (surface == WRISTFLOW_SURFACE_HOME) {
        shell->recentering = true;
        lv_obj_scroll_to_x(shell->carousel, PAGE_WIDTH, LV_ANIM_OFF);
        shell->recentering = false;
        for (unsigned i = 0; i < shell->navigation.page_count; ++i)
            if (shell->indicators[i]) lv_obj_add_flag(shell->indicators[i], LV_OBJ_FLAG_HIDDEN);
    }
    shell->edge_press = false;
    if (lv_screen_active() == shell->screens[surface]) {
        wristflow_apps_activate(shell->apps, surface);
        sync_visibility(shell);
    } else {
        transition(shell, shell->screens[surface], LV_SCR_LOAD_ANIM_FADE_IN);
    }
    return true;
}

bool wristflow_ui_shell_open(wristflow_ui_shell_t *shell, wristflow_surface_t surface)
{
    if (!shell || !shell->apps || shell->transitioning) return false;
    if (shell->navigation.surface == WRISTFLOW_SURFACE_HOME &&
        (lv_obj_is_scrolling(shell->carousel) || lv_obj_get_scroll_x(shell->carousel) != PAGE_WIDTH))
        return false;
    wristflow_navigation_t previous = shell->navigation;
    if (!wristflow_navigation_open(&shell->navigation, surface)) return false;
    if (load_surface(shell)) return true;
    shell->navigation = previous;
    return false;
}

bool wristflow_ui_shell_home(wristflow_ui_shell_t *shell)
{
    if (!shell || shell->transitioning) return false;
    wristflow_navigation_home(&shell->navigation);
    return load_surface(shell);
}

bool wristflow_ui_shell_key(wristflow_ui_shell_t *shell)
{
    if (!shell || !shell->apps || shell->transitioning || lv_obj_is_scrolling(shell->carousel)) return false;
    wristflow_navigation_key(&shell->navigation);
    return load_surface(shell);
}

bool wristflow_ui_shell_back(wristflow_ui_shell_t *shell)
{
    if (!shell || shell->transitioning || !wristflow_navigation_back(&shell->navigation)) return false;
    return load_surface(shell);
}

static void face_long_press(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    if (wristflow_ui_shell_open(shell, WRISTFLOW_SURFACE_FACE_PICKER)) {
        lv_indev_t *input = lv_indev_active();
        if (input) lv_indev_wait_release(input);
    }
}

bool wristflow_ui_shell_open_controls(wristflow_ui_shell_t *shell)
{
    if (!shell || shell->transitioning || lv_obj_is_scrolling(shell->carousel) ||
        lv_obj_get_scroll_x(shell->carousel) != PAGE_WIDTH ||
        !wristflow_navigation_open_controls(&shell->navigation))
        return false;
    transition(shell, shell->controls, LV_SCR_LOAD_ANIM_MOVE_TOP);
    return true;
}

bool wristflow_ui_shell_close_controls(wristflow_ui_shell_t *shell)
{
    if (!shell || shell->transitioning || !wristflow_navigation_close_controls(&shell->navigation))
        return false;
    transition(shell, shell->home, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
    return true;
}

static void gesture(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    lv_indev_t *input = lv_indev_active();
    if (!input)
        return;
    lv_dir_t direction = lv_indev_get_gesture_dir(input);
    /* Browsers own their drag. Unhandled gestures must not suppress a slider
       or menu pointer until release. */
    if (shell->navigation.surface == WRISTFLOW_SURFACE_LAUNCHER ||
        shell->navigation.surface == WRISTFLOW_SURFACE_FACE_PICKER)
        return;
    bool handled = false;
    if (shell->navigation.surface >= WRISTFLOW_SURFACE_STOPWATCH) {
        if (direction == LV_DIR_RIGHT && shell->edge_press)
            handled = wristflow_ui_shell_back(shell);
    } else if (direction == LV_DIR_BOTTOM)
        handled = wristflow_ui_shell_close_controls(shell);
    else if (direction == LV_DIR_TOP)
        handled = wristflow_ui_shell_open_controls(shell);
    if (handled) lv_indev_wait_release(input);
}

static void destroy_face(const wristflow_watchface_t *watchface, lv_obj_t *root)
{
    if (watchface->destroy)
        watchface->destroy(root);
    lv_obj_delete(root);
}

bool wristflow_ui_shell_set_watchface(wristflow_ui_shell_t *shell,
                                    const wristflow_watchface_t *watchface)
{
    if (!shell || !watchface || !watchface->id || !watchface->id[0] ||
        !watchface->create || !watchface->update)
        return false;
    /* Stage every loop copy before touching the current face. */
    for (unsigned int i = 0; i < slot_count(shell); ++i) {
        if ((i + shell->navigation.page_count - 1) % shell->navigation.page_count != 0)
            continue;
        page_slot_t *slot = &shell->slots[i];
        slot->pending_face = watchface->create(slot->panel);
        if (!slot->pending_face) {
            for (unsigned int j = 0; j < i; ++j) {
                if (shell->slots[j].pending_face) {
                    destroy_face(watchface, shell->slots[j].pending_face);
                    shell->slots[j].pending_face = NULL;
                }
            }
            return false;
        }
        watchface->update(slot->pending_face, &shell->snapshot);
        bubble_events(slot->pending_face);
        lv_obj_add_event_cb(slot->pending_face, face_long_press, LV_EVENT_LONG_PRESSED, shell);
        if (watchface->set_visible)
            watchface->set_visible(slot->pending_face, false);
    }
    for (unsigned int i = 0; i < slot_count(shell); ++i) {
        page_slot_t *slot = &shell->slots[i];
        if (!slot->pending_face)
            continue;
        if (slot->face) {
            set_visible(shell, slot, false);
            destroy_face(shell->watchface, slot->face);
        }
        slot->face = slot->pending_face;
        slot->pending_face = NULL;
    }
    shell->watchface = watchface;
    sync_visibility(shell);
    return true;
}

bool wristflow_ui_shell_update(wristflow_ui_shell_t *shell,
                              const wristflow_watch_snapshot_t *snapshot)
{
    if (!shell || !wristflow_snapshot_valid(snapshot))
        return false;
    shell->snapshot = *snapshot;
    wristflow_apps_update(shell->apps, snapshot);
    for (unsigned int i = 0; i < slot_count(shell); ++i)
        if (shell->slots[i].face)
            shell->watchface->update(shell->slots[i].face, &shell->snapshot);
    return true;
}

static void mount_card(wristflow_ui_shell_t *shell, lv_obj_t *screen,
                       unsigned int slot, unsigned int index)
{
    lv_obj_t *indicator = lv_obj_find_by_name(screen, "page_dots_0");
    if (indicator) {
        if (shell->indicators[index])
            lv_obj_delete(indicator);
        else {
            shell->indicators[index] = indicator;
            lv_obj_set_parent(indicator, shell->home);
            lv_obj_add_flag(indicator, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(indicator, LV_OBJ_FLAG_CLICKABLE);
            for (uint32_t i = 0; i < lv_obj_get_child_count(indicator); ++i)
                lv_obj_remove_flag(lv_obj_get_child(indicator, i), LV_OBJ_FLAG_CLICKABLE);
        }
    }
    wristflow_mount_screen(shell->slots[slot].panel, screen);
}

wristflow_ui_shell_t *wristflow_ui_shell_create(const wristflow_ui_shell_config_t *config)
{
    if (!config || !config->controls || !wristflow_snapshot_valid(&config->initial_snapshot) ||
        (config->card_count && !config->cards))
        return NULL;
    for (unsigned int i = 0; i < config->card_count; ++i)
        if (!config->cards[i])
            return NULL;
    wristflow_ui_shell_t *shell = lv_malloc_zeroed(sizeof(*shell));
    LV_ASSERT_MALLOC(shell);
    wristflow_navigation_init(&shell->navigation, (unsigned int)config->card_count + 1);
    shell->snapshot = config->initial_snapshot;
    shell->slots = lv_malloc_zeroed(slot_count(shell) * sizeof(*shell->slots));
    shell->indicators = lv_malloc_zeroed(shell->navigation.page_count * sizeof(*shell->indicators));
    LV_ASSERT_MALLOC(shell->slots);
    LV_ASSERT_MALLOC(shell->indicators);
    shell->home = lv_obj_create(NULL);
    lv_obj_remove_style_all(shell->home);
    lv_obj_set_style_bg_color(shell->home, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(shell->home, LV_OPA_COVER, 0);
    lv_obj_remove_flag(shell->home, LV_OBJ_FLAG_SCROLLABLE);
    shell->carousel = lv_obj_create(shell->home);
    lv_obj_set_name(shell->carousel, "demo_carousel");
    lv_obj_remove_style_all(shell->carousel);
    lv_obj_set_size(shell->carousel, PAGE_WIDTH, PAGE_HEIGHT);
    lv_obj_set_scroll_dir(shell->carousel, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(shell->carousel, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(shell->carousel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(shell->carousel, LV_OBJ_FLAG_SCROLL_ONE | LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_remove_flag(shell->carousel, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_style_anim_duration(shell->carousel, 220, 0);
    for (unsigned int slot = 0; slot < slot_count(shell); ++slot) {
        lv_obj_t *panel = lv_obj_create(shell->carousel);
        shell->slots[slot].panel = panel;
        lv_obj_remove_style_all(panel);
        lv_obj_set_pos(panel, (int32_t)slot * PAGE_WIDTH, 0);
        lv_obj_set_size(panel, PAGE_WIDTH, PAGE_HEIGHT);
        lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(panel, LV_OBJ_FLAG_SNAPPABLE);
        unsigned int index = (slot + shell->navigation.page_count - 1) % shell->navigation.page_count;
        if (index)
            mount_card(shell, config->cards[index - 1](), slot, index);
    }
    if (!wristflow_ui_shell_set_watchface(shell, config->watchface)) {
        lv_obj_delete(shell->home);
        lv_free(shell->slots);
        lv_free(shell->indicators);
        lv_free(shell);
        return NULL;
    }
    shell->controls = config->controls();
    shell->screens[WRISTFLOW_SURFACE_HOME] = shell->home;
    shell->screens[WRISTFLOW_SURFACE_CONTROLS] = shell->controls;
    if (config->enable_apps) {
        shell->apps = wristflow_apps_create(shell, shell->controls, config->set_brightness, config->platform_context);
        wristflow_apps_update(shell->apps, &shell->snapshot);
    }
    lv_obj_add_event_cb(shell->home, gesture, LV_EVENT_GESTURE, shell);
    lv_obj_add_event_cb(shell->controls, gesture, LV_EVENT_GESTURE, shell);
    lv_obj_add_event_cb(shell->home, screen_loaded, LV_EVENT_SCREEN_LOADED, shell);
    lv_obj_add_event_cb(shell->controls, screen_loaded, LV_EVENT_SCREEN_LOADED, shell);
    lv_obj_update_layout(shell->home);
    lv_obj_scroll_to_x(shell->carousel, PAGE_WIDTH, LV_ANIM_OFF);
    lv_obj_add_event_cb(shell->carousel, scroll_event, LV_EVENT_SCROLL, shell);
    lv_obj_add_event_cb(shell->carousel, scroll_event, LV_EVENT_SCROLL_END, shell);
    shell->ready = true;
    lv_screen_load(shell->home);
    sync_visibility(shell);
    return shell;
}

void wristflow_ui_shell_destroy(wristflow_ui_shell_t *shell)
{
    if (!shell)
        return;
    shell->ready = false;
    sync_visibility(shell);
    /* Loading without animation completes/cancels any pending LVGL screen load. */
    lv_screen_load(lv_obj_create(NULL));
    wristflow_apps_destroy(shell->apps);
    for (unsigned int i = 0; i < slot_count(shell); ++i)
        if (shell->slots[i].face)
            destroy_face(shell->watchface, shell->slots[i].face);
    lv_obj_delete(shell->controls);
    lv_obj_delete(shell->home);
    lv_free(shell->slots);
    lv_free(shell->indicators);
    lv_free(shell);
}

const wristflow_navigation_t *wristflow_ui_shell_navigation(const wristflow_ui_shell_t *shell)
{
    return shell ? &shell->navigation : NULL;
}

const char *wristflow_ui_shell_watchface_id(const wristflow_ui_shell_t *shell)
{
    return shell ? shell->watchface->id : NULL;
}
