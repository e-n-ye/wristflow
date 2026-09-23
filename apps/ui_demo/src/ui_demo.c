#include "ui_demo.h"
#include "wristflow_ui.h"
#include "watchface_presenter.h"

/* A visual carousel only. Product navigation and power policy belong to watch_core. */
#define PAGE_COUNT 5U
#define PAGE_WIDTH 390
static lv_obj_t *home;
static lv_obj_t *carousel;
static lv_obj_t *control_center;
static lv_obj_t *indicators[PAGE_COUNT];
static unsigned int page_index;
static bool recentering;

static void scroll_finished(lv_event_t *event)
{
    (void)event;
    if (recentering || lv_obj_is_scrolling(carousel))
        return;
    int32_t x = lv_obj_get_scroll_x(carousel);
    if (x % PAGE_WIDTH != 0)
        return;
    unsigned int slot = (unsigned int)(x / PAGE_WIDTH);
    if (slot == 0 || slot == PAGE_COUNT + 1)
    {
        recentering = true;
        slot = slot == 0 ? PAGE_COUNT : 1;
        lv_obj_scroll_to_x(carousel, slot * PAGE_WIDTH, LV_ANIM_OFF);
        recentering = false;
    }
    page_index = slot - 1;
    for (unsigned int i = 0; i < PAGE_COUNT; ++i)
        if (indicators[i])
            lv_obj_set_flag(indicators[i], LV_OBJ_FLAG_HIDDEN, i != page_index);
}

static void gesture(lv_event_t *event)
{
    (void)event;
    lv_indev_t *input = lv_indev_active();
    if (!input)
        return;
    lv_dir_t direction = lv_indev_get_gesture_dir(input);
    lv_obj_t *active = lv_screen_active();
    if (active == control_center)
    {
        if (direction == LV_DIR_BOTTOM)
            lv_screen_load_anim(home, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 180, 0, false);
    }
    else if (page_index == 0U && direction == LV_DIR_TOP && !lv_obj_is_scrolling(carousel))
        lv_screen_load_anim(control_center, LV_SCR_LOAD_ANIM_MOVE_TOP, 180, 0, false);
    lv_indev_wait_release(input);
}

static void mount_page(lv_obj_t *screen, unsigned int slot, unsigned int index)
{
    /* LVGL screens cannot be reparented. Mount their generated child tree instead. */
    lv_obj_t *panel = lv_obj_create(carousel);
    lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel, slot * PAGE_WIDTH, 0);
    lv_obj_set_size(panel, PAGE_WIDTH, 450);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_SNAPPABLE);
    lv_obj_set_style_bg_color(panel, lv_obj_get_style_bg_color(screen, LV_PART_MAIN), 0);
    lv_obj_set_style_bg_opa(panel, lv_obj_get_style_bg_opa(screen, LV_PART_MAIN), 0);
    lv_obj_set_style_text_color(panel, lv_obj_get_style_text_color(screen, LV_PART_MAIN), 0);
    lv_obj_set_style_text_font(panel, lv_obj_get_style_text_font(screen, LV_PART_MAIN), 0);
    /* Keep XML-defined indicators on the stationary home layer, outside the carousel. */
    lv_obj_t *indicator = lv_obj_find_by_name(screen, "page_dots_0");
    if (indicator)
    {
        if (indicators[index])
            lv_obj_delete(indicator);
        else
        {
            indicators[index] = indicator;
            lv_obj_set_parent(indicator, home);
            lv_obj_add_flag(indicator, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(indicator, LV_OBJ_FLAG_CLICKABLE);
            for (uint32_t i = 0; i < lv_obj_get_child_count(indicator); ++i)
                lv_obj_remove_flag(lv_obj_get_child(indicator, i), LV_OBJ_FLAG_CLICKABLE);
        }
    }
    while (lv_obj_get_child_count(screen))
        lv_obj_set_parent(lv_obj_get_child(screen, 0), panel);
    lv_obj_delete(screen);
}

void wristflow_demo_start(void)
{
    page_index = 0;
    lv_obj_t *initial = lv_screen_active();
    home = lv_obj_create(NULL);
    lv_obj_remove_style_all(home);
    lv_obj_set_style_bg_color(home, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(home, LV_OPA_COVER, 0);
    lv_obj_remove_flag(home, LV_OBJ_FLAG_SCROLLABLE);
    carousel = lv_obj_create(home);
    lv_obj_set_name(carousel, "demo_carousel");
    lv_obj_remove_style_all(carousel);
    lv_obj_set_size(carousel, PAGE_WIDTH, 450);
    lv_obj_set_scroll_dir(carousel, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(carousel, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(carousel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(carousel, LV_OBJ_FLAG_SCROLL_ONE | LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_remove_flag(carousel, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_style_anim_duration(carousel, 220, 0);
    lv_obj_t *(*create_page[PAGE_COUNT])(void) = {
        screen_watchface_create, screen_tile_heart_rate_create,
        screen_tile_activity_create, screen_tile_system_create, screen_tile_full_create
    };
    for (unsigned int slot = 0; slot < PAGE_COUNT + 2; ++slot)
    {
        unsigned int index = (slot + PAGE_COUNT - 1) % PAGE_COUNT;
        lv_obj_t *page = create_page[index]();
        if (index == 0)
        {
            bool time_set = wristflow_watchface_set_time(page, 22, 48);
            LV_ASSERT(time_set);
        }
        mount_page(page, slot, index);
    }
    control_center = screen_control_center_create();
    lv_obj_add_event_cb(home, gesture, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(control_center, gesture, LV_EVENT_GESTURE, NULL);
    lv_obj_update_layout(home);
    lv_obj_scroll_to_x(carousel, PAGE_WIDTH, LV_ANIM_OFF);
    lv_obj_add_event_cb(carousel, scroll_finished, LV_EVENT_SCROLL_END, NULL);
    lv_screen_load(home);
    lv_obj_delete(initial);
}
