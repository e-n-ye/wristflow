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
static void visible(const char *name, bool show)
{ assert(lv_obj_is_visible(named(name)) == show); }
static void wheel(const char *name, unsigned value)
{
    lv_roller_set_selected(named(name), value, LV_ANIM_OFF);
    lv_obj_send_event(named(name), LV_EVENT_VALUE_CHANGED, NULL);
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
    assert(wristflow_ui_shell_open(shell, WRISTFLOW_SURFACE_COUNTDOWN)); advance(400);
    assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_COUNTDOWN);
}

int main(int argc, char **argv)
{
    assert(argc == 2); directory = argv[1];
    lv_init(); lv_tick_set_cb(tick);
    lv_display_t *display = lv_display_create(390, 450);
    lv_display_set_buffers(display, pixels, NULL, sizeof pixels, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush); wristflow_ui_init("");
    const uint32_t chinese[] = {0x8ba1, 0x65f6, 0x7ed3, 0x675f};
    for (unsigned i = 0; i < 4; ++i) {
        lv_font_glyph_dsc_t glyph;
        assert(lv_font_get_glyph_dsc(value_36, &glyph, chinese[i], 0) && !glyph.is_placeholder);
    }
    const uint32_t icons[] = {0xf04d, 0xf04c, 0xf04b, 0xf00c, 0xf00d, 0xf2f9};
    for (unsigned i = 0; i < 6; ++i) {
        lv_font_glyph_dsc_t glyph;
        assert(lv_font_get_glyph_dsc(icons_44, &glyph, icons[i], 0) && !glyph.is_placeholder);
    }
    lv_indev_t *input = lv_indev_create();
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER); lv_indev_set_display(input, display);
    lv_indev_set_read_cb(input, read_pointer); lv_timer_set_period(lv_indev_get_read_timer(input), 16);
    unsigned baseline_timers = timers();
    wristflow_ui_shell_t *shell = create(false);
    visible("countdown_indicator", false);
    assert(wristflow_ui_shell_key(shell)); advance(400);
    assert(lv_obj_find_by_name(lv_screen_active(), "launch_countdown"));
    open(shell); visible("countdown_picker", true); snapshot("countdown_picker");
    assert(strcmp(lv_label_get_text(named("countdown_clock")), "13:40") == 0);
    const char *const presets[] = {"countdown_preset_1", "countdown_preset_2", "countdown_preset_3",
        "countdown_preset_5", "countdown_preset_10", "countdown_preset_30"};
    const char *const values[] = {"00:01:00", "00:02:00", "00:03:00", "00:05:00", "00:10:00", "00:30:00"};
    for (unsigned i = 0; i < 6; ++i) {
        click(presets[i]); visible("countdown_active_panel", true);
        assert(strcmp(lv_label_get_text(named("countdown_remaining")), values[i]) == 0);
        click("countdown_cancel"); visible("countdown_picker", true);
    }
    click("countdown_custom"); snapshot("countdown_custom");
    lv_area_t wheel_area, separator_area;
    lv_obj_get_coords(named("countdown_hours"), &wheel_area);
    lv_obj_get_coords(named("countdown_separator_1"), &separator_area);
    assert(LV_ABS(wheel_area.y1 + wheel_area.y2 - separator_area.y1 - separator_area.y2) <= 2);
    assert(lv_obj_get_style_text_align(named("countdown_hours"), 0) == LV_TEXT_ALIGN_CENTER);
    swipe(85, 214, 85, 274); /* Actual pointer input scrolls the hours roller. */
    assert(lv_roller_get_selected(named("countdown_hours")) != 0);
    wheel("countdown_hours", 0); wheel("countdown_minutes", 0); wheel("countdown_seconds", 0);
    assert(lv_obj_has_state(named("countdown_start"), LV_STATE_DISABLED));
    click("countdown_start"); visible("countdown_custom_panel", true);
    wheel("countdown_hours", 23); wheel("countdown_minutes", 59); wheel("countdown_seconds", 59);
    assert(!lv_obj_has_state(named("countdown_start"), LV_STATE_DISABLED));
    snapshot("countdown_custom_max"); click("countdown_start");
    assert(strcmp(lv_label_get_text(named("countdown_remaining")), "23:59:59") == 0);
    click("countdown_cancel");
    click("countdown_custom"); swipe(15, 90, 260, 90);
    visible("countdown_picker", true); /* Custom back returns to duration selection. */
    click("countdown_preset_1"); advance(2600); snapshot("countdown_running");
    click("countdown_toggle"); visible("countdown_paused", true); snapshot("countdown_paused");
    char paused[20]; snprintf(paused, sizeof paused, "%s", lv_label_get_text(named("countdown_remaining")));
    advance(80000); assert(strcmp(paused, lv_label_get_text(named("countdown_remaining"))) == 0);
    assert(wristflow_ui_shell_key(shell)); advance(400);
    visible("countdown_indicator", true); snapshot("countdown_home_paused");
    lv_area_t indicator_area, battery_area;
    lv_obj_get_coords(named("countdown_indicator"), &indicator_area);
    lv_obj_get_coords(named("battery_label"), &battery_area);
    assert(indicator_area.x1 > battery_area.x2);
    click("countdown_indicator"); visible("countdown_paused", true);
    assert(strcmp(paused, lv_label_get_text(named("countdown_remaining"))) == 0);
    click("countdown_toggle"); visible("countdown_paused", false);
    swipe(15, 90, 260, 90); /* Edge Back leaves the running task intact. */
    assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_HOME);
    advance(2000); snapshot("countdown_home_running"); click("countdown_indicator");
    assert(strcmp(paused, lv_label_get_text(named("countdown_remaining"))) != 0);
    click("countdown_cancel"); assert(wristflow_ui_shell_key(shell)); advance(400);
    visible("countdown_indicator", false);

    /* Deadline while off-page opens one alert, then closes to the prior home. */
    open(shell); click("countdown_custom");
    wheel("countdown_hours", 0); wheel("countdown_minutes", 0); wheel("countdown_seconds", 3);
    click("countdown_start"); assert(wristflow_ui_shell_home(shell)); advance(400);
    advance(3200); visible("countdown_alert_panel", true); snapshot("countdown_expired");
    assert(strcmp(lv_label_get_text(named("countdown_original")), "00:00:03") == 0);
    unsigned history = wristflow_ui_shell_navigation(shell)->history_count;
    advance(10000); assert(wristflow_ui_shell_navigation(shell)->history_count == history);
    click("countdown_repeat"); visible("countdown_active_panel", true);
    assert(strcmp(lv_label_get_text(named("countdown_remaining")), "00:00:03") == 0);
    advance(3200); visible("countdown_alert_panel", true); click("countdown_close");
    assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_HOME);
    visible("countdown_indicator", false);
    wristflow_ui_shell_destroy(shell); advance(400); assert(timers() == baseline_timers);

    /* No menu entry, indicator or API shortcut in Product during round one. */
    shell = create(true); assert(!lv_obj_find_by_name(lv_screen_active(), "countdown_indicator"));
    assert(!wristflow_ui_shell_open(shell, WRISTFLOW_SURFACE_COUNTDOWN));
    assert(wristflow_ui_shell_key(shell)); advance(400);
    assert(!lv_obj_find_by_name(lv_screen_active(), "launch_countdown"));
    assert(!wristflow_ui_shell_open(shell, WRISTFLOW_SURFACE_COUNTDOWN));
    wristflow_ui_shell_destroy(shell); advance(400); assert(timers() == baseline_timers);
    puts("countdown UI: controls, rollers, page lifetime, hourglass and demo-only gate passed");
    return 0;
}
