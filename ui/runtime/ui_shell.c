#include "ui_shell.h"
#include "mount_screen.h"
#include "apps.h"
#include "components.h"
#include "settings_view.h"
#include "wristflow_ui.h"
#include <string.h>

#define PAGE_WIDTH 390
#define PAGE_HEIGHT 450

typedef struct {
    wristflow_ui_shell_t *shell;
    wristflow_surface_t target;
    unsigned page;
    unsigned slot;
} card_link_t;

typedef struct {
    lv_obj_t *panel;
    lv_obj_t *face;
    lv_obj_t *pending_face;
    bool visible;
    card_link_t links[4];
} page_slot_t;

struct wristflow_ui_shell {
    lv_obj_t *home;
    lv_obj_t *carousel;
    lv_obj_t *controls;
    lv_timer_t *cleanup_timer;
    wristflow_apps_t *apps;
    wristflow_components_t *components;
    lv_timer_t *card_hold;
    card_link_t *pressed_card;
    lv_point_t card_press;
    lv_indev_t *card_input;
    lv_obj_t **indicators;
    page_slot_t *slots;
    const wristflow_watchface_t *watchface;
    wristflow_navigation_t navigation;
    wristflow_watch_snapshot_t snapshot;
    wristflow_card_update_cb_t update_card;
    wristflow_card_target_cb_t card_target;
    bool recentering;
    bool transitioning;
    bool ready;
    bool edge_press;
    wristflow_settings_t preferences;
    wristflow_display_policy_t display;
    lv_timer_t *display_timer;
    wristflow_brightness_cb_t brightness_cb;
    void *platform_context;
    uint8_t applied_brightness;
};

static void gesture(lv_event_t *event);
static void face_long_press(lv_event_t *event);

static void apply_brightness(wristflow_ui_shell_t *shell, uint8_t value)
{
    if (shell->applied_brightness == value) return;
    shell->applied_brightness = value;
    if (shell->brightness_cb) shell->brightness_cb(value, shell->platform_context);
}

static void display_tick(lv_timer_t *timer)
{
    wristflow_ui_shell_t *shell = lv_timer_get_user_data(timer);
    uint32_t keep_before = shell->display.keep_ms;
    wristflow_display_tick(&shell->display, lv_tick_get(), shell->preferences.screen_timeout,
        shell->navigation.surface == WRISTFLOW_SURFACE_FLASHLIGHT);
    apply_brightness(shell, wristflow_display_brightness(&shell->display, wristflow_apps_brightness(shell->apps)));
    if (keep_before != shell->display.keep_ms) wristflow_apps_update(shell->apps, &shell->snapshot);
}

static void brightness_changed(uint8_t value, void *context)
{
    wristflow_ui_shell_t *shell = context;
    if (shell->display_timer) {
        wristflow_display_activity(&shell->display, lv_tick_get());
        display_tick(shell->display_timer);
    } else apply_brightness(shell, value);
}

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

static void finish_transition(lv_timer_t *timer)
{
    wristflow_ui_shell_t *shell = lv_timer_get_user_data(timer);
    /* SCREEN_LOADED runs before LVGL clears prev_scr. Wait until that entire
       callback has returned before releasing any outgoing view. */
    if (lv_display_get_screen_prev(NULL)) return;
    lv_timer_pause(timer);
    wristflow_apps_collect(shell->apps, &shell->navigation);
    wristflow_components_collect(shell->components, &shell->navigation);
    shell->transitioning = false;
    sync_visibility(shell);
}

static void screen_loaded(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    if (!shell->ready) return;
    if (shell->transitioning) {
        lv_timer_reset(shell->cleanup_timer);
        lv_timer_resume(shell->cleanup_timer);
    } else {
        sync_visibility(shell);
    }
}

static void transition(wristflow_ui_shell_t *shell, lv_obj_t *screen, lv_screen_load_anim_t animation)
{
    /* Controls are retained outside the navigation stack; discard pending prompts on exit. */
    wristflow_settings_dismiss(lv_screen_active());
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

static void editor_back(lv_event_t *event)
{ wristflow_ui_shell_back(lv_event_get_user_data(event)); }

static bool load_surface(wristflow_ui_shell_t *shell)
{
    wristflow_surface_t surface = shell->navigation.surface;
    lv_obj_t *screen = surface == WRISTFLOW_SURFACE_HOME ? shell->home :
                       surface == WRISTFLOW_SURFACE_CONTROLS ? shell->controls : NULL;
    if (!screen) {
        bool created;
        screen = surface >= WRISTFLOW_SURFACE_COMPONENT_EDITOR
            ? wristflow_components_screen(shell->components, surface, &created)
            : wristflow_apps_screen(shell->apps, surface, &created);
        if (!screen) return false;
        if (created) {
            if (surface >= WRISTFLOW_SURFACE_COMPONENT_EDITOR)
                lv_obj_add_event_cb(lv_obj_find_by_name(screen, "app_back"), editor_back, LV_EVENT_SHORT_CLICKED, shell);
            bubble_events(screen);
            lv_obj_add_event_cb(screen, screen_loaded, LV_EVENT_SCREEN_LOADED, shell);
            lv_obj_add_event_cb(screen, app_pressed, LV_EVENT_PRESSED, shell);
            lv_obj_add_event_cb(screen, gesture, LV_EVENT_GESTURE, shell);
        }
    }
    if (surface == WRISTFLOW_SURFACE_HOME) {
        shell->recentering = true;
        lv_obj_scroll_to_x(shell->carousel, (int32_t)(shell->navigation.page_index + 1) * PAGE_WIDTH, LV_ANIM_OFF);
        shell->recentering = false;
        for (unsigned i = 0; i < shell->navigation.page_count; ++i)
            if (shell->indicators[i]) lv_obj_set_flag(shell->indicators[i], LV_OBJ_FLAG_HIDDEN,
                                                     i != shell->navigation.page_index);
    }
    shell->edge_press = false;
    if (lv_screen_active() == screen) {
        wristflow_apps_activate(shell->apps, surface);
        wristflow_apps_collect(shell->apps, &shell->navigation);
        wristflow_components_collect(shell->components, &shell->navigation);
        sync_visibility(shell);
    } else {
        transition(shell, screen, LV_SCR_LOAD_ANIM_FADE_IN);
    }
    return true;
}

bool wristflow_ui_shell_open(wristflow_ui_shell_t *shell, wristflow_surface_t surface)
{
    if (!shell || !shell->apps || shell->transitioning) return false;
    if (shell->navigation.surface == WRISTFLOW_SURFACE_HOME &&
        (lv_obj_is_scrolling(shell->carousel) || lv_obj_get_scroll_x(shell->carousel) !=
            (int32_t)(shell->navigation.page_index + 1) * PAGE_WIDTH))
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
    if (wristflow_components_back(shell->components, true)) return true;
    wristflow_navigation_home(&shell->navigation);
    return load_surface(shell);
}

bool wristflow_ui_shell_key(wristflow_ui_shell_t *shell)
{
    if (shell && shell->display_timer) {
        bool was_off = shell->display.phase == WRISTFLOW_DISPLAY_OFF;
        uint32_t elapsed = lv_tick_get() - shell->display.off_at;
        bool pass = wristflow_display_key(&shell->display, lv_tick_get());
        display_tick(shell->display_timer);
        if (!pass) {
            if (was_off && elapsed >= 120000U &&
                shell->navigation.surface < WRISTFLOW_SURFACE_COMPONENT_EDITOR &&
                !wristflow_apps_timer_running(shell->apps)) wristflow_ui_shell_home(shell);
            return true;
        }
    }
    if (!shell || !shell->apps || shell->transitioning || lv_obj_is_scrolling(shell->carousel)) return false;
    if (wristflow_components_back(shell->components, true)) return true;
    wristflow_navigation_t previous = shell->navigation;
    wristflow_navigation_key(&shell->navigation);
    if (load_surface(shell)) return true;
    shell->navigation = previous;
    return false;
}

bool wristflow_ui_shell_back(wristflow_ui_shell_t *shell)
{
    if (!shell || shell->transitioning) return false;
    if (wristflow_settings_dismiss(lv_screen_active())) return true;
    if (wristflow_components_back(shell->components, false)) return true;
    wristflow_navigation_t previous = shell->navigation;
    if (!wristflow_navigation_back(&shell->navigation)) return false;
    if (load_surface(shell)) return true;
    shell->navigation = previous;
    return false;
}

static void face_long_press(lv_event_t *event)
{
    wristflow_ui_shell_t *shell = lv_event_get_user_data(event);
    if (!shell->preferences.face_long_press) return;
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
    for (unsigned int i = 0; i < slot_count(shell); ++i) {
        if (shell->slots[i].face)
            shell->watchface->update(shell->slots[i].face, &shell->snapshot);
        else if (shell->components || shell->update_card) {
            unsigned index = (i + shell->navigation.page_count - 1) % shell->navigation.page_count;
            if (shell->components) wristflow_components_update(shell->components, shell->slots[i].panel, index - 1, &shell->snapshot);
            else shell->update_card(shell->slots[i].panel, index - 1, &shell->snapshot);
        }
    }
    return true;
}

static void card_clicked(lv_event_t *event)
{
    card_link_t *link = lv_event_get_user_data(event);
    if (link->shell->navigation.surface != WRISTFLOW_SURFACE_HOME ||
        link->shell->navigation.page_index != link->page) return;
    if (wristflow_ui_shell_open(link->shell, link->target)) lv_event_stop_bubbling(event);
}

static void card_hold_ready(lv_timer_t *timer)
{
    wristflow_ui_shell_t *shell = lv_timer_get_user_data(timer);
    lv_timer_pause(timer);
    card_link_t *link = shell->pressed_card;
    shell->pressed_card = NULL;
    if (!link || shell->transitioning || shell->navigation.surface != WRISTFLOW_SURFACE_HOME ||
        shell->navigation.page_index != link->page || lv_obj_is_scrolling(shell->carousel)) return;
    if (wristflow_components_enter(shell->components, link->page - 1)) {
        if (shell->card_input) lv_indev_wait_release(shell->card_input);
    }
}

static void card_touch(lv_event_t *event)
{
    card_link_t *link = lv_event_get_user_data(event);
    wristflow_ui_shell_t *shell = link->shell;
    lv_indev_t *input = lv_indev_active();
    if (!input || !shell->components) return;
    lv_event_code_t code = lv_event_get_code(event);
    lv_point_t point; lv_indev_get_point(input, &point);
    if (code == LV_EVENT_PRESSED) {
        shell->pressed_card = link; shell->card_press = point; shell->card_input = input;
        lv_timer_reset(shell->card_hold); lv_timer_resume(shell->card_hold);
    } else if (code == LV_EVENT_PRESSING) {
        int dx = point.x - shell->card_press.x, dy = point.y - shell->card_press.y;
        if (dx * dx + dy * dy > 100) {
            shell->pressed_card = NULL; lv_timer_pause(shell->card_hold);
        }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        shell->pressed_card = NULL; lv_timer_pause(shell->card_hold);
    }
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
            /* The carousel includes the face; the dots describe only card pages. */
            unsigned count = shell->navigation.page_count - 1;
            lv_obj_clean(indicator);
            int width = (int)(count - 1) * 18 + 6;
            lv_obj_set_width(indicator, width);
            lv_obj_set_x(indicator, (PAGE_WIDTH - width) / 2);
            for (unsigned i = 0; i < count; ++i) {
                lv_obj_t *dot = page_dot_create(indicator);
                lv_obj_set_x(dot, (int)i * 18);
                lv_obj_set_style_bg_color(dot, lv_color_hex(i == index - 1 ? 0xffffff : 0x334155), 0);
            }
            lv_obj_set_parent(indicator, shell->home);
            lv_obj_add_flag(indicator, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(indicator, LV_OBJ_FLAG_CLICKABLE);
            for (uint32_t i = 0; i < lv_obj_get_child_count(indicator); ++i)
                lv_obj_remove_flag(lv_obj_get_child(indicator, i), LV_OBJ_FLAG_CLICKABLE);
        }
    }
    wristflow_mount_screen(shell->slots[slot].panel, screen);
    if (shell->components) wristflow_components_update(shell->components, shell->slots[slot].panel, index - 1, &shell->snapshot);
    else if (shell->update_card) shell->update_card(shell->slots[slot].panel, index - 1, &shell->snapshot);
    if (shell->components || shell->card_target) {
        static const char *const names[] = {"slot_0", "slot_1", "slot_2", "slot_3"};
        for (unsigned i = 0; i < 4; ++i) {
            wristflow_surface_t target = shell->components
                ? wristflow_components_target(shell->components, index - 1, i)
                : shell->card_target(index - 1, i);
            if (target < WRISTFLOW_SURFACE_STOPWATCH || target >= WRISTFLOW_SURFACE_COUNT) continue;
            lv_obj_t *component = lv_obj_find_by_name(shell->slots[slot].panel, names[i]);
            LV_ASSERT(component);
            card_link_t *link = &shell->slots[slot].links[i];
            *link = (card_link_t){shell, target, index, i};
            lv_obj_add_flag(component, LV_OBJ_FLAG_CLICKABLE);
            bubble_events(component);
            lv_obj_add_event_cb(component, card_clicked, LV_EVENT_SHORT_CLICKED, link);
            if (shell->components) lv_obj_add_event_cb(component, card_touch, LV_EVENT_ALL, link);
        }
    }
}

wristflow_ui_shell_t *wristflow_ui_shell_create(const wristflow_ui_shell_config_t *config)
{
    if (!config || !config->controls || !wristflow_snapshot_valid(&config->initial_snapshot) ||
        (config->card_count && !config->cards))
        return NULL;
    for (unsigned int i = 0; i < config->card_count; ++i)
        if (!config->cards[i])
            return NULL;
    if (config->initial_settings && !wristflow_settings_valid(config->initial_settings)) return NULL;
    wristflow_ui_shell_t *shell = lv_malloc_zeroed(sizeof(*shell));
    LV_ASSERT_MALLOC(shell);
    wristflow_navigation_init(&shell->navigation, (unsigned int)config->card_count + 1);
    shell->snapshot = config->initial_snapshot;
    shell->preferences = config->initial_settings ? *config->initial_settings : wristflow_settings_default();
    shell->brightness_cb = config->set_brightness;
    shell->platform_context = config->platform_context;
    shell->applied_brightness = 255;
    wristflow_display_init(&shell->display, lv_tick_get());
    if (config->product_apps) {
        shell->components = wristflow_components_create(shell, config->initial_layout,
            config->save_layout, config->layout_status, config->layout_context);
        shell->navigation.page_count = wristflow_components_layout(shell->components)->count + 1;
    }
    shell->card_hold = lv_timer_create(card_hold_ready, 700, shell);
    lv_timer_pause(shell->card_hold);
    shell->update_card = config->update_card;
    shell->card_target = config->card_target;
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
            mount_card(shell, shell->components ? wristflow_components_page(shell->components, index - 1)
                : config->cards[index - 1](), slot, index);
    }
    if (!wristflow_ui_shell_set_watchface(shell, config->watchface)) {
        wristflow_components_destroy(shell->components);
        lv_timer_delete(shell->card_hold);
        lv_obj_delete(shell->home);
        lv_free(shell->slots);
        lv_free(shell->indicators);
        lv_free(shell);
        return NULL;
    }
    shell->controls = config->controls();
    if (config->enable_apps) {
        shell->apps = wristflow_apps_create(shell, shell->controls, brightness_changed, shell,
            config->initial_settings ? config->initial_settings->brightness : 60, config->product_apps,
            config->initial_settings ? config->initial_settings->menu_layout : WRISTFLOW_MENU_LIST);
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
    shell->cleanup_timer = lv_timer_create(finish_transition, 1, shell);
    lv_timer_pause(shell->cleanup_timer);
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
    lv_timer_delete(shell->cleanup_timer);
    lv_timer_delete(shell->card_hold);
    if (shell->display_timer) lv_timer_delete(shell->display_timer);
    shell->display_timer = NULL;
    /* Loading without animation completes/cancels any pending LVGL screen load. */
    lv_screen_load(lv_obj_create(NULL));
    wristflow_apps_destroy(shell->apps);
    wristflow_components_destroy(shell->components);
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

bool wristflow_ui_shell_get_settings(const wristflow_ui_shell_t *shell, wristflow_settings_t *settings)
{
    if (!shell || !shell->apps || !settings || strlen(shell->watchface->id) >= sizeof settings->face_id)
        return false;
    wristflow_settings_t current = shell->preferences;
    current.brightness = wristflow_apps_brightness(shell->apps);
    current.menu_layout = wristflow_apps_menu_layout(shell->apps);
    strcpy(current.face_id, shell->watchface->id);
    if (!wristflow_settings_valid(&current)) return false;
    *settings = current;
    return true;
}

const wristflow_layout_t *wristflow_ui_shell_layout(const wristflow_ui_shell_t *shell)
{ return shell ? wristflow_components_layout(shell->components) : NULL; }

const wristflow_watch_snapshot_t *wristflow_ui_shell_snapshot(const wristflow_ui_shell_t *shell)
{ return &shell->snapshot; }

bool wristflow_ui_shell_configure(wristflow_ui_shell_t *shell, const wristflow_settings_t *settings)
{
    if (!shell || !wristflow_settings_valid(settings)) return false;
    shell->preferences = *settings;
    wristflow_apps_set_brightness(shell->apps, settings->brightness);
    wristflow_display_activity(&shell->display, lv_tick_get());
    return true;
}

void wristflow_ui_shell_enable_display_policy(wristflow_ui_shell_t *shell)
{
    if (!shell || shell->display_timer) return;
    wristflow_display_init(&shell->display, lv_tick_get());
    shell->display_timer = lv_timer_create(display_tick, 50, shell);
}

bool wristflow_ui_shell_filter_touch(wristflow_ui_shell_t *shell, bool pressed)
{
    if (!shell || !shell->display_timer) return true;
    bool pass = wristflow_display_touch(&shell->display, lv_tick_get(), pressed);
    display_tick(shell->display_timer);
    return pass;
}

wristflow_display_phase_t wristflow_ui_shell_display_phase(const wristflow_ui_shell_t *shell)
{ return shell ? shell->display.phase : WRISTFLOW_DISPLAY_ACTIVE; }

bool wristflow_ui_shell_keep_awake(wristflow_ui_shell_t *shell, unsigned minutes)
{
    if (!shell || !wristflow_display_keep(&shell->display, lv_tick_get(), minutes)) return false;
    wristflow_apps_update(shell->apps, &shell->snapshot);
    if (shell->display_timer) display_tick(shell->display_timer);
    return true;
}

unsigned wristflow_ui_shell_keep_minutes(const wristflow_ui_shell_t *shell)
{ return shell ? shell->display.keep_ms / 60000U : 0; }

void wristflow_ui_shell_display_retry(wristflow_ui_shell_t *shell)
{
    wristflow_display_activity(&shell->display, lv_tick_get());
    if (shell->display_timer) display_tick(shell->display_timer);
}

void wristflow_ui_shell_rebuild_components(wristflow_ui_shell_t *shell, unsigned selected)
{
    shell->ready = false; shell->recentering = true;
    shell->pressed_card = NULL; lv_timer_pause(shell->card_hold);
    sync_visibility(shell);
    for (unsigned i = 0; i < slot_count(shell); ++i)
        if (shell->slots[i].face) destroy_face(shell->watchface, shell->slots[i].face);
    lv_obj_clean(shell->carousel);
    for (unsigned i = 0; i < shell->navigation.page_count; ++i)
        if (shell->indicators[i]) lv_obj_delete(shell->indicators[i]);
    lv_free(shell->slots); lv_free(shell->indicators);
    shell->navigation.page_count = wristflow_components_layout(shell->components)->count + 1;
    shell->navigation.page_index = selected;
    shell->slots = lv_malloc_zeroed(slot_count(shell) * sizeof(*shell->slots));
    shell->indicators = lv_malloc_zeroed(shell->navigation.page_count * sizeof(*shell->indicators));
    LV_ASSERT_MALLOC(shell->slots); LV_ASSERT_MALLOC(shell->indicators);
    for (unsigned i = 0; i < slot_count(shell); ++i) {
        lv_obj_t *panel = lv_obj_create(shell->carousel); shell->slots[i].panel = panel;
        lv_obj_remove_style_all(panel); lv_obj_set_pos(panel, (int32_t)i * PAGE_WIDTH, 0);
        lv_obj_set_size(panel, PAGE_WIDTH, PAGE_HEIGHT);
        lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_flag(panel, LV_OBJ_FLAG_SNAPPABLE);
        unsigned index = (i + shell->navigation.page_count - 1) % shell->navigation.page_count;
        if (index) mount_card(shell, wristflow_components_page(shell->components, index - 1), i, index);
    }
    wristflow_ui_shell_set_watchface(shell, shell->watchface);
    lv_obj_update_layout(shell->home);
    lv_obj_scroll_to_x(shell->carousel, (int32_t)(selected + 1) * PAGE_WIDTH, LV_ANIM_OFF);
    shell->recentering = false; shell->ready = true;
}

bool wristflow_ui_shell_finish_edit(wristflow_ui_shell_t *shell, unsigned selected)
{
    if (shell->transitioning || selected >= shell->navigation.page_count) return false;
    wristflow_navigation_home(&shell->navigation);
    shell->navigation.page_index = selected;
    return load_surface(shell);
}
