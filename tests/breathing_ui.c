#include "ui_shell.h"
#include "wristflow_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t ticks;
static lv_indev_data_t pointer;
static uint8_t pixels[390 * 40 * 4];
static const char *directory;
static uint32_t tick(void) { return ticks; }
static void read_pointer(lv_indev_t *input, lv_indev_data_t *data) { (void)input; *data = pointer; }
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{ (void)area; (void)data; lv_display_flush_ready(display); }
static void advance(unsigned ms)
{
    for (unsigned i = 0; i < ms; i += 16) { ticks += 16; lv_timer_handler(); }
    lv_obj_update_layout(lv_screen_active());
}
static void sample(int x, int y, bool pressed)
{
    pointer.point = (lv_point_t){x, y};
    pointer.state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    advance(16);
}
static lv_obj_t *named(const char *name)
{
    lv_obj_t *obj = lv_obj_find_by_name(lv_screen_active(), name);
    assert(obj); return obj;
}
static void click(const char *name)
{
    lv_area_t a; lv_obj_get_coords(named(name), &a);
    int x = (a.x1 + a.x2) / 2, y = (a.y1 + a.y2) / 2;
    assert(x >= 0 && x < 390 && y >= 0 && y < 450);
    sample(x, y, true); sample(x, y, false); advance(400);
}
static void swipe(int x1, int y1, int x2, int y2)
{
    sample(x1, y1, true);
    for (int i = 1; i <= 12; ++i)
        sample(x1 + (x2 - x1) * i / 12, y1 + (y2 - y1) * i / 12, true);
    sample(x2, y2, false); advance(600);
}
static void snapshot(const char *name)
{
    lv_draw_buf_t *buf = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);
    assert(buf && buf->header.w == 390 && buf->header.h == 450);
    char path[1024]; snprintf(path, sizeof path, "%s/%s.ppm", directory, name);
    FILE *file = fopen(path, "wb"); assert(file);
    fprintf(file, "P6\n390 450\n255\n");
    for (unsigned y = 0; y < 450; ++y) {
        const unsigned char *row = buf->data + y * buf->header.stride;
        for (unsigned x = 0; x < 390; ++x) {
            unsigned char rgb[] = {row[3*x+2], row[3*x+1], row[3*x]};
            assert(fwrite(rgb, 1, 3, file) == 3);
        }
    }
    assert(fclose(file) == 0); lv_draw_buf_destroy(buf);
}
static void visible_at(const char *name, bool show, unsigned line)
{
    bool actual = lv_obj_is_visible(named(name));
    if (actual != show) fprintf(stderr, "visibility %s: expected=%u actual=%u at tick=%u test line=%u\n", name, show, actual, ticks, line);
    assert(actual == show);
}
#define visible(name, show) visible_at(name, show, __LINE__)
static void wheel(unsigned value)
{
    lv_roller_set_selected(named("breathing_minutes"), value - 1, LV_ANIM_OFF);
    lv_obj_send_event(named("breathing_minutes"), LV_EVENT_VALUE_CHANGED, NULL);
    advance(32);
}
static unsigned timers(void)
{
    unsigned count = 0;
    for (lv_timer_t *t = lv_timer_get_next(NULL); t; t = lv_timer_get_next(t)) ++count;
    return count;
}
static wristflow_ui_shell_t *create(bool product)
{
    wristflow_ui_shell_config_t config = {
        .watchface = &wristflow_default_watchface, .controls = screen_control_center_create,
        .initial_snapshot = {.hour_24=13, .minute=40, .battery_percent=53},
        .enable_apps = true, .product_apps = product
    };
    wristflow_ui_shell_t *shell = wristflow_ui_shell_create(&config);
    assert(shell); advance(400); return shell;
}
static void open(wristflow_ui_shell_t *shell)
{
    if (wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_HOME) {
        assert(wristflow_ui_shell_key(shell)); advance(400);
    }
    assert(wristflow_ui_shell_open(shell, WRISTFLOW_SURFACE_BREATHING)); advance(400);
    assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_BREATHING);
    visible("breathing_home_panel", true);
}
static void glyphs(const lv_font_t *font, const uint32_t *chars, unsigned count)
{
    for (unsigned i = 0; i < count; ++i) {
        lv_font_glyph_dsc_t glyph;
        assert(lv_font_get_glyph_dsc(font, &glyph, chars[i], 0) && !glyph.is_placeholder);
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2); directory = argv[1];
    lv_init(); lv_tick_set_cb(tick);
    lv_display_t *display = lv_display_create(390, 450);
    lv_display_set_buffers(display, pixels, NULL, sizeof pixels, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush); wristflow_ui_init("");
    const uint32_t guide[] = {0x5438, 0x547c, 0x6c14};
    glyphs(guide_40, guide, 3);
    const uint32_t titles[] = {0x547c, 0x5438, 0x8bad, 0x7ec3, 0x7ed3, 0x679c, 0x524d, 0x540e,
                               0x675f, 0x91cd, 0x65b0, 0x5f00, 0x59cb};
    glyphs(title_24, titles, sizeof titles / sizeof titles[0]);
    const uint32_t confirmation[] = {0x7ed3, 0x675f, 0x672c, 0x6b21, 0x547c, 0x5438, 0x8bad, 0x7ec3, 0xff1f};
    glyphs(body_20, confirmation, sizeof confirmation / sizeof confirmation[0]);
    const uint32_t values[] = {0x5206, 0x949f, 0x5f00, 0x59cb, 0x5b8c, 0x6210};
    glyphs(value_36, values, 6);
    const uint32_t icons[] = {0xf192, 0xf04d, 0xf2f9, 0xf00c};
    glyphs(icons_44, icons, 4);
    lv_indev_t *input = lv_indev_create();
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER); lv_indev_set_display(input, display);
    lv_indev_set_read_cb(input, read_pointer); lv_timer_set_period(lv_indev_get_read_timer(input), 16);
    unsigned baseline = timers();
    wristflow_ui_shell_t *shell = create(false);
    assert(wristflow_ui_shell_key(shell)); advance(400);
    /* Verify registry -> launcher binding; content pages use real pointer input. */
    lv_obj_send_event(named("launch_breathing"), LV_EVENT_SHORT_CLICKED, NULL); advance(400);
    assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_BREATHING);
    assert(strcmp(lv_label_get_text(named("breathing_duration_text")), "1分钟") == 0);
    snapshot("breathing_home");
    click("breathing_duration"); visible("breathing_duration_panel", true);
    assert(lv_roller_get_option_count(named("breathing_minutes")) == 5);
    assert(lv_obj_get_style_text_align(named("breathing_minutes"), 0) == LV_TEXT_ALIGN_CENTER);
    swipe(172, 247, 172, 310); assert(lv_roller_get_selected(named("breathing_minutes")) != 0);
    wheel(5); snapshot("breathing_duration_5");
    assert(strcmp(lv_label_get_text(named("breathing_cycles")), "呼吸35次") == 0);
    swipe(15, 90, 270, 90); visible("breathing_home_panel", true);
    assert(strcmp(lv_label_get_text(named("breathing_duration_text")), "1分钟") == 0);
    for (unsigned minutes = 1; minutes <= 5; ++minutes) {
        click("breathing_duration"); wheel(minutes); click("breathing_duration_confirm");
        char text[24]; snprintf(text, sizeof text, "%u分钟", minutes);
        assert(strcmp(lv_label_get_text(named("breathing_duration_text")), text) == 0);
    }
    assert(wristflow_ui_shell_home(shell)); advance(400); open(shell);
    assert(strcmp(lv_label_get_text(named("breathing_duration_text")), "5分钟") == 0);
    click("breathing_start"); visible("breathing_training_panel", true);
    assert(strcmp(lv_label_get_text(named("breathing_guide")), "吸气") == 0);
    int small = lv_obj_get_width(named("breathing_training_ring"));
    lv_area_t ring_area;
    lv_obj_get_coords(named("breathing_training_ring"), &ring_area);
    assert(ring_area.x1 + ring_area.x2 >= 389 && ring_area.x1 + ring_area.x2 <= 390);
    assert(ring_area.y1 + ring_area.y2 >= 439 && ring_area.y1 + ring_area.y2 <= 440);
    snapshot("breathing_inhale_small"); advance(3000);
    assert(lv_obj_get_width(named("breathing_training_ring")) > small + 40);
    lv_obj_get_coords(named("breathing_training_ring"), &ring_area);
    assert(ring_area.x1 + ring_area.x2 >= 389 && ring_area.x1 + ring_area.x2 <= 390);
    assert(ring_area.y1 + ring_area.y2 >= 439 && ring_area.y1 + ring_area.y2 <= 440);
    advance(800); /* Sample close to the largest radius, before the phase boundary. */
    lv_obj_get_coords(named("breathing_training_ring"), &ring_area);
    lv_area_t guide_area;
    lv_obj_get_coords(named("breathing_guide"), &guide_area);
    assert(lv_obj_get_width(named("breathing_training_ring")) >= 218);
    assert(guide_area.y1 - ring_area.y2 >= 18);
    snapshot("breathing_inhale_large"); advance(2200);
    assert(strcmp(lv_label_get_text(named("breathing_guide")), "呼气") == 0);
    snapshot("breathing_exhale");
    swipe(290, 300, 90, 300); visible("breathing_controls_panel", true); snapshot("breathing_controls");
    swipe(41, 300, 280, 300); visible("breathing_training_panel", true);
    assert(!lv_obj_find_by_name(lv_screen_active(), "breathing_exit_confirm"));
    for (unsigned edge = 15; edge <= 40; edge += 25) {
        swipe(290, 300, 90, 300); visible("breathing_controls_panel", true);
        swipe((int)edge, 300, 280, 300); visible("breathing_exit_confirm", true);
        snapshot("breathing_exit_confirm");
        swipe(290, 200, 90, 200); visible("breathing_exit_confirm", true);
        advance(310000); /* Hidden time must not finish a five-minute session. */
        click("confirm_cancel"); visible("breathing_controls_panel", true);
        assert(strcmp(lv_label_get_text(named("breathing_guide")), "吸气") == 0);
        swipe(100, 300, 290, 300);
    }
    swipe(290, 300, 90, 300); click("breathing_restart");
    visible("breathing_training_panel", true);
    assert(strcmp(lv_label_get_text(named("breathing_guide")), "吸气") == 0);
    advance(61000); visible("breathing_training_panel", true); /* Retains five-minute duration. */
    swipe(290, 300, 90, 300); click("breathing_end");
    visible("breathing_home_panel", true); visible("breathing_result_panel", false);
    click("breathing_duration"); wheel(1); snapshot("breathing_duration_1"); click("breathing_duration_confirm");
    click("breathing_start"); advance(2500);
    assert(wristflow_ui_shell_key(shell)); advance(400); visible("breathing_exit_confirm", true);
    advance(65000); assert(wristflow_ui_shell_key(shell)); advance(400);
    visible("breathing_training_panel", true); /* KEY1 cancels the pending confirmation. */
    advance(50000); visible("breathing_training_panel", true);
    advance(10000); visible("breathing_result_panel", true);
    assert(strcmp(lv_label_get_text(named("breathing_before")), "--次/分") == 0);
    assert(strcmp(lv_label_get_text(named("breathing_after")), "--") == 0);
    assert(strcmp(lv_label_get_text(named("breathing_result_minutes")), "1") == 0);
    snapshot("breathing_result"); click("breathing_done"); visible("breathing_home_panel", true);
    click("breathing_start");
    uint32_t started = ticks - 400;
    advance(20000); swipe(290, 300, 90, 300); advance(20000);
    swipe(100, 300, 290, 300); advance(15000); swipe(290, 300, 90, 300);
    visible("breathing_controls_panel", true);
    /* Absolute start time, not time since the last page switch, owns completion. */
    ticks = started + 60064; lv_timer_handler(); advance(32);
    visible("breathing_result_panel", true); click("breathing_done");
    for (unsigned action = 0; action < 3; ++action) {
        click("breathing_start");
        ticks += 60000; /* No timer sampling before the user requests navigation. */
        bool handled = action == 0 ? wristflow_ui_shell_back(shell) : action == 1 ?
            wristflow_ui_shell_home(shell) : wristflow_ui_shell_open(shell, WRISTFLOW_SURFACE_SETTINGS);
        assert(handled); visible("breathing_result_panel", true);
        assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_BREATHING);
        click("breathing_done");
    }
    click("breathing_start"); assert(wristflow_ui_shell_home(shell)); advance(400);
    visible("breathing_exit_confirm", true); click("confirm_cancel");
    click("app_back"); visible("breathing_exit_confirm", true); click("confirm_accept");
    assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_LAUNCHER);
    open(shell); click("breathing_start");
    assert(wristflow_ui_shell_open(shell, WRISTFLOW_SURFACE_SETTINGS)); advance(400);
    visible("breathing_exit_confirm", true);
    /* A second Open cannot replace the first pending destination. */
    assert(wristflow_ui_shell_open(shell, WRISTFLOW_SURFACE_STOPWATCH));
    click("confirm_accept"); assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_SETTINGS);
    assert(wristflow_ui_shell_back(shell)); advance(400); visible("breathing_home_panel", true);
    assert(wristflow_ui_shell_home(shell)); advance(400); open(shell);
    assert(strcmp(lv_label_get_text(named("breathing_duration_text")), "1分钟") == 0);
    click("breathing_start"); assert(wristflow_ui_shell_home(shell)); advance(32);
    visible("breathing_exit_confirm", true); click("confirm_accept");
    assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_HOME);
    open(shell); click("breathing_start");
    wristflow_ui_shell_destroy(shell); assert(timers() == baseline);
    shell = create(false); open(shell); click("breathing_start");
    assert(wristflow_ui_shell_home(shell)); advance(32); visible("breathing_exit_confirm", true);
    wristflow_ui_shell_destroy(shell); assert(timers() == baseline);
    shell = create(true);
    assert(wristflow_ui_shell_key(shell)); advance(400);
    assert(lv_obj_find_by_name(lv_screen_active(), "launch_breathing"));
    open(shell);
    wristflow_ui_shell_destroy(shell); assert(timers() == baseline);
    lv_indev_delete(input); lv_display_delete(display); lv_deinit();
    return 0;
}
