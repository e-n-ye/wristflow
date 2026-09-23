#include "wristflow_ui.h"
#include "ui_demo.h"
#include <assert.h>
#include <stdio.h>

static lv_indev_data_t pointer_data;
static uint32_t ticks;
static lv_obj_t *carousel;
static uint8_t draw_buffer[390 * 40 * 4];
static const char *render_directory;

static void snapshot(const char *name)
{
    lv_draw_buf_t *buffer = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);
    assert(buffer && buffer->header.w == 390 && buffer->header.h == 450);
    char path[1024];
    snprintf(path, sizeof path, "%s/%s.ppm", render_directory, name);
    FILE *file = fopen(path, "wb");
    assert(file);
    fprintf(file, "P6\n390 450\n255\n");
    for (unsigned int y = 0; y < 450; ++y)
    {
        const unsigned char *row = buffer->data + y * buffer->header.stride;
        for (unsigned int x = 0; x < 390; ++x)
        {
            unsigned char rgb[] = {row[x * 3 + 2], row[x * 3 + 1], row[x * 3]};
            assert(fwrite(rgb, 1, 3, file) == 3);
        }
    }
    assert(fclose(file) == 0);
    lv_draw_buf_destroy(buffer);
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    (void)area;
    (void)pixels;
    lv_display_flush_ready(display);
}

static uint32_t tick(void) { return ticks; }
static void read_pointer(lv_indev_t *input, lv_indev_data_t *data)
{
    (void)input;
    *data = pointer_data;
}

static void sample(lv_indev_t *input, int x, int y, lv_indev_state_t state)
{
    (void)input;
    pointer_data.point.x = x;
    pointer_data.point.y = y;
    pointer_data.state = state;
    ticks += 16;
    lv_timer_handler();
    lv_obj_update_layout(lv_screen_active());
}

static void settle(void)
{
    for (int i = 0; i < 60; ++i)
    {
        ticks += 16;
        lv_timer_handler();
    }
}

static unsigned int page_index(void)
{
    int32_t x = lv_obj_get_scroll_x(carousel);
    assert(x % 390 == 0 && x >= 390 && x <= 1950);
    return (unsigned int)(x / 390 - 1);
}

static void check_indicator(unsigned int page)
{
    assert(!lv_obj_find_by_name(carousel, "page_dots_0"));
    unsigned int visible = 0;
    for (unsigned int i = 0; i < 4; ++i)
    {
        char name[32];
        snprintf(name, sizeof name, "page_dots_%u", i);
        lv_obj_t *indicator = lv_obj_find_by_name(lv_obj_get_parent(carousel), name);
        assert(indicator);
        if (lv_obj_has_flag(indicator, LV_OBJ_FLAG_HIDDEN))
            continue;
        ++visible;
        lv_area_t bounds;
        lv_obj_get_coords(indicator, &bounds);
        assert(bounds.x1 == 165 && bounds.y1 == 423 && bounds.x2 == 224 && bounds.y2 == 428);
        unsigned int active = 0;
        for (uint32_t j = 0; j < lv_obj_get_child_count(indicator); ++j)
        {
            lv_obj_t *dot = lv_obj_get_child(indicator, j);
            if (lv_color_eq(lv_obj_get_style_bg_color(dot, 0), PAGER_ACTIVE))
            {
                ++active;
                assert(page > 0 && lv_obj_get_x(dot) == (int32_t)(page - 1) * 18);
            }
        }
        assert(active == 1);
    }
    assert(visible == (page ? 1U : 0U));
}

static void swipe(lv_indev_t *input, int x1, int y1, int x2, int y2)
{
    sample(input, x1, y1, LV_INDEV_STATE_PRESSED);
    for (int i = 1; i <= 12; ++i)
        sample(input, x1 + (x2 - x1) * i / 12, y1 + (y2 - y1) * i / 12,
               LV_INDEV_STATE_PRESSED);
    sample(input, x2, y2, LV_INDEV_STATE_RELEASED);
    settle();
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    render_directory = argv[1];
    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t *display = lv_display_create(390, 450);
    assert(display);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof draw_buffer, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    wristflow_ui_init("");
    wristflow_demo_start();
    lv_obj_update_layout(lv_screen_active());
    lv_indev_t *input = lv_indev_create();
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(input, display);
    lv_indev_set_read_cb(input, read_pointer);
    lv_timer_set_period(lv_indev_get_read_timer(input), 16);
    lv_obj_t *face = lv_screen_active();
    carousel = lv_obj_find_by_name(face, "demo_carousel");
    assert(carousel && page_index() == 0);
    assert(lv_obj_find_by_name(face, "minute_label"));
    check_indicator(0);
    snapshot("navigation_watchface");

    /* Real pointer samples exercise LVGL gesture recognition and child bubbling. */
    for (int i = 1; i <= 10; ++i)
    {
        swipe(input, 330, 180, 60, 180);
        assert(lv_screen_active() == face && page_index() == (unsigned int)i % 5);
        check_indicator((unsigned int)i % 5);
    }
    for (int i = 9; i >= 0; --i)
    {
        swipe(input, 60, 180, 330, 180);
        assert(lv_screen_active() == face && page_index() == (unsigned int)i % 5);
        check_indicator((unsigned int)i % 5);
    }

    /* The indicator stays fixed while content moves and does not intercept a drag. */
    swipe(input, 330, 180, 60, 180);
    check_indicator(1);
    snapshot("indicator_before_drag");
    sample(input, 300, 180, LV_INDEV_STATE_PRESSED);
    for (int x = 295; x >= 210; x -= 5)
    {
        sample(input, x, 180, LV_INDEV_STATE_PRESSED);
        check_indicator(1);
    }
    assert(lv_obj_get_scroll_x(carousel) > 780 && lv_obj_get_scroll_x(carousel) < 975);
    snapshot("indicator_mid_drag");
    for (int i = 0; i < 12; ++i)
        sample(input, 210, 180, LV_INDEV_STATE_PRESSED);
    sample(input, 210, 180, LV_INDEV_STATE_RELEASED);
    settle();
    assert(page_index() == 1);
    check_indicator(1);
    swipe(input, 220, 426, 0, 426);
    assert(page_index() == 2);
    check_indicator(2);
    snapshot("indicator_next_page");
    swipe(input, 60, 180, 330, 180);
    swipe(input, 60, 180, 330, 180);

    /* A slow short drag follows the pointer, then cancels below half a page. */
    sample(input, 300, 180, LV_INDEV_STATE_PRESSED);
    for (int x = 295; x >= 210; x -= 5)
        sample(input, x, 180, LV_INDEV_STATE_PRESSED);
    assert(lv_obj_get_scroll_x(carousel) > 390 && lv_obj_get_scroll_x(carousel) < 585);
    snapshot("navigation_mid_drag");
    for (int i = 0; i < 12; ++i)
        sample(input, 210, 180, LV_INDEV_STATE_PRESSED);
    sample(input, 210, 180, LV_INDEV_STATE_RELEASED);
    settle();
    assert(page_index() == 0);
    snapshot("navigation_cancelled");
    /* A slow drag beyond half a page commits without needing a fling. */
    sample(input, 330, 180, LV_INDEV_STATE_PRESSED);
    for (int x = 325; x >= 90; x -= 5)
        sample(input, x, 180, LV_INDEV_STATE_PRESSED);
    for (int i = 0; i < 12; ++i)
        sample(input, 90, 180, LV_INDEV_STATE_PRESSED);
    sample(input, 90, 180, LV_INDEV_STATE_RELEASED);
    settle();
    assert(page_index() == 1);
    swipe(input, 60, 180, 330, 180);
    /* Less than half a page, but enough release velocity for native LVGL momentum. */
    swipe(input, 300, 180, 120, 180);
    assert(page_index() == 1);
    swipe(input, 60, 180, 330, 180);
    swipe(input, 330, 180, 60, 180);
    swipe(input, 195, 330, 195, 80);
    assert(lv_screen_active() == face && page_index() == 1);
    swipe(input, 60, 180, 330, 180);
    swipe(input, 195, 100, 195, 340);
    assert(lv_screen_active() == face);
    sample(input, 190, 180, LV_INDEV_STATE_PRESSED);
    sample(input, 190, 180, LV_INDEV_STATE_RELEASED);
    assert(lv_screen_active() == face);

    for (int cycle = 0; cycle < 3; ++cycle)
    {
        swipe(input, 195, 330, 195, 80);
        lv_obj_t *control = lv_screen_active();
        lv_obj_t *slider = lv_obj_find_by_name(control, "brightness_slider");
        assert(control != face && slider);
        if (cycle == 0)
            snapshot("navigation_control_center");
        int before = lv_slider_get_value(slider);
        swipe(input, 100, 383, 330, 383);
        assert(lv_screen_active() == control);
        assert(lv_slider_get_value(slider) != before || cycle > 0);
        /* Start on the button to verify its child gesture reaches the screen. */
        swipe(input, 100, 140, 100, 340);
        assert(lv_screen_active() == face);
    }
    puts("PASS: fixed indicators, live drag, short-drag cancellation, half-page commit, fling, both loop boundaries, control center and slider");
    lv_deinit();
    return 0;
}
