#include "ui_shell.h"
#include "wristflow_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t ticks;
static unsigned int alive, visible, timer_calls, creates, fail_on_create;
static uint8_t draw_buffer[390 * 40 * 4];

typedef struct {
    lv_timer_t *timer;
    bool visible;
} face_context_t;

static uint32_t tick(void) { return ticks; }
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    (void)area;
    (void)pixels;
    lv_display_flush_ready(display);
}
static void settle(void)
{
    for (int i = 0; i < 40; ++i) {
        ticks += 16;
        lv_timer_handler();
    }
}
static void animate(lv_timer_t *timer)
{
    (void)timer;
    ++timer_calls;
}
static lv_obj_t *create_alternate(lv_obj_t *parent)
{
    if (++creates == fail_on_create)
        return NULL;
    lv_obj_t *root = lv_label_create(parent);
    lv_obj_set_name(root, "alternate_time");
    face_context_t *context = lv_malloc_zeroed(sizeof(*context));
    assert(context);
    context->timer = lv_timer_create(animate, 16, root);
    lv_timer_pause(context->timer);
    lv_obj_set_user_data(root, context);
    ++alive;
    return root;
}
static void update_alternate(lv_obj_t *root, const wristflow_watch_snapshot_t *snapshot)
{
    lv_label_set_text_fmt(root, "%02u:%02u / %u", (unsigned int)snapshot->hour_24,
                         (unsigned int)snapshot->minute, (unsigned int)snapshot->battery_percent);
}
static void show_alternate(lv_obj_t *root, bool show)
{
    face_context_t *context = lv_obj_get_user_data(root);
    if (show != context->visible) {
        if (show) {
            ++visible;
            lv_timer_resume(context->timer);
        }
        else {
            assert(visible > 0);
            --visible;
            lv_timer_pause(context->timer);
        }
        context->visible = show;
    }
}
static void destroy_alternate(lv_obj_t *root)
{
    face_context_t *context = lv_obj_get_user_data(root);
    assert(!context->visible && alive > 0);
    lv_timer_delete(context->timer);
    lv_free(context);
    --alive;
}
static const wristflow_watchface_t alternate = {
    "test-text", create_alternate, update_alternate, show_alternate, destroy_alternate
};

static void check_copies(lv_obj_t *carousel, const char *text)
{
    unsigned int count = 0;
    for (uint32_t i = 0; i < lv_obj_get_child_count(carousel); ++i) {
        lv_obj_t *root = lv_obj_find_by_name(lv_obj_get_child(carousel, i), "alternate_time");
        if (root) {
            ++count;
            assert(strcmp(lv_label_get_text(root), text) == 0);
        }
    }
    assert(count == alive);
}

int main(void)
{
    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t *display = lv_display_create(390, 450);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof draw_buffer, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    wristflow_ui_init("");
    const wristflow_screen_factory_t cards[] = {screen_tile_heart_rate_create, screen_tile_activity_create};
    wristflow_ui_shell_config_t config = {
        &wristflow_default_watchface, cards, 2, screen_control_center_create, {22, 48, 53}
    };
    assert(!wristflow_ui_shell_create(NULL));
    wristflow_ui_shell_t *shell = wristflow_ui_shell_create(&config);
    assert(shell);
    lv_obj_t *home = lv_screen_active();
    lv_obj_t *carousel = lv_obj_find_by_name(home, "demo_carousel");
    lv_obj_t *card = lv_obj_get_child(carousel, 2);
    assert(wristflow_ui_shell_set_watchface(shell, &alternate));
    assert(!lv_obj_find_by_name(home, "hour_artwork"));
    assert(alive == 2 && visible == 1);
    check_copies(carousel, "22:48 / 53");
    settle();
    assert(timer_calls > 0);

    /* A partial drag changes visibility, but does not commit navigation. */
    lv_obj_scroll_to_x(carousel, 585, LV_ANIM_OFF);
    assert(wristflow_ui_shell_navigation(shell)->page_index == 0 && visible == 1);
    assert(!wristflow_ui_shell_open_controls(shell));
    lv_obj_scroll_to_x(carousel, 780, LV_ANIM_OFF);
    assert(wristflow_ui_shell_navigation(shell)->page_index == 1 && visible == 0);
    unsigned int before = timer_calls;
    settle();
    assert(timer_calls == before);
    assert(!wristflow_ui_shell_open_controls(shell));
    assert(wristflow_ui_shell_set_watchface(shell, &alternate));
    assert(visible == 0 && wristflow_ui_shell_navigation(shell)->page_index == 1);
    assert(lv_obj_get_child(carousel, 2) == card);
    /* The trailing mirror loops back to the real home instance. */
    lv_obj_scroll_to_x(carousel, 1560, LV_ANIM_OFF);
    assert(lv_obj_get_scroll_x(carousel) == 390 && visible == 1);
    assert(wristflow_ui_shell_open_controls(shell));
    assert(!wristflow_ui_shell_close_controls(shell));
    assert(visible == 0);
    settle();
    lv_obj_t *controls = lv_screen_active();
    lv_obj_t *slider = lv_obj_find_by_name(controls, "brightness_slider");
    assert(slider);
    lv_slider_set_value(slider, 71, LV_ANIM_OFF);
    wristflow_watch_snapshot_t snapshot = {23, 59, 0};
    assert(wristflow_ui_shell_update(shell, &snapshot));
    check_copies(carousel, "23:59 / 0");
    snapshot.minute = 60;
    assert(!wristflow_ui_shell_update(shell, &snapshot));
    check_copies(carousel, "23:59 / 0");

    /* Fail on the mirror: the old primary/mirror pair and card state survive. */
    lv_obj_t *old_face = lv_obj_find_by_name(home, "alternate_time");
    fail_on_create = creates + 2;
    assert(!wristflow_ui_shell_set_watchface(shell, &alternate));
    assert(alive == 2 && visible == 0);
    assert(lv_obj_find_by_name(home, "alternate_time") == old_face);
    fail_on_create = 0;
    assert(wristflow_ui_shell_set_watchface(shell, &wristflow_default_watchface));
    assert(alive == 0 && visible == 0);
    assert(strcmp(wristflow_ui_shell_watchface_id(shell), "diffusion") == 0);
    assert(strcmp(lv_label_get_text(lv_obj_find_by_name(home, "minute_label")), "59") == 0);
    assert(strcmp(lv_label_get_text(lv_obj_find_by_name(home, "battery_label")), "0%") == 0);
    assert(wristflow_ui_shell_set_watchface(shell, &alternate));
    check_copies(carousel, "23:59 / 0");
    assert(lv_screen_active() == controls && lv_slider_get_value(slider) == 71);
    assert(lv_obj_get_child(carousel, 2) == card && visible == 0);
    assert(wristflow_ui_shell_close_controls(shell));
    settle();
    assert(lv_screen_active() == home && visible == 1);
    assert(wristflow_ui_shell_open_controls(shell));
    /* Teardown while a screen transition is still pending. */
    wristflow_ui_shell_destroy(shell);
    assert(alive == 0 && visible == 0);
    before = timer_calls;
    settle();
    assert(timer_calls == before);

    config.watchface = &alternate;
    config.card_count = 0;
    config.cards = NULL;
    for (int i = 0; i < 12; ++i) {
        lv_obj_t *previous = lv_screen_active();
        shell = wristflow_ui_shell_create(&config);
        assert(shell && alive == 3 && visible == 1);
        lv_obj_delete(previous);
        if (i % 2) {
            assert(wristflow_ui_shell_open_controls(shell));
            settle();
            assert(wristflow_ui_shell_close_controls(shell));
            ticks += 32;
            lv_timer_handler();
        }
        wristflow_ui_shell_destroy(shell);
        assert(alive == 0 && visible == 0);
        settle();
    }
    lv_obj_t *previous = lv_screen_active();
    fail_on_create = creates + 2;
    assert(!wristflow_ui_shell_create(&config));
    assert(alive == 0 && visible == 0 && lv_screen_active() == previous);
    puts("PASS: independent face trees, hidden updates/timers, mirrors, rollback, retained card/controls, transition teardown and restart");
    lv_deinit();
    return 0;
}
