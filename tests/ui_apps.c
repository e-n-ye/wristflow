#include "wristflow_ui.h"
#include "ui_shell.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t ticks;
static lv_indev_data_t pointer;
static unsigned brightness;
static uint8_t pixels[390 * 40 * 4];
static const char *directory;

static uint32_t tick(void) { return ticks; }
static void read_pointer(lv_indev_t *input, lv_indev_data_t *data) { (void)input; *data = pointer; }
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{ (void)area; (void)data; lv_display_flush_ready(display); }
static void set_brightness(uint8_t value, void *context)
{ assert(context == &brightness); brightness = value; }
static void advance(unsigned duration)
{
    for (unsigned i = 0; i < duration; i += 16) {
        ticks += 16;
        lv_timer_handler();
    }
    lv_obj_update_layout(lv_screen_active());
}
static void sample(int x, int y, bool down)
{
    pointer.point = (lv_point_t){x, y};
    pointer.state = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    advance(16);
}
static lv_obj_t *named(const char *name)
{
    lv_obj_t *obj = lv_obj_find_by_name(lv_screen_active(), name);
    assert(obj);
    return obj;
}
static void click(const char *name)
{
    lv_area_t area;
    lv_obj_get_coords(named(name), &area);
    int x = (area.x1 + area.x2) / 2, y = (area.y1 + area.y2) / 2;
    assert(x >= 0 && x < 390 && y >= 0 && y < 450);
    sample(x, y, true);
    sample(x, y, false);
    advance(400);
}
static void swipe(int x1, int y1, int x2, int y2)
{
    sample(x1, y1, true);
    for (int i = 1; i <= 12; ++i)
        sample(x1 + (x2 - x1) * i / 12, y1 + (y2 - y1) * i / 12, true);
    sample(x2, y2, false);
    advance(800);
}
static void snapshot(const char *name)
{
    lv_draw_buf_t *buffer = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);
    assert(buffer && buffer->header.w == 390 && buffer->header.h == 450);
    char path[1024];
    snprintf(path, sizeof path, "%s/%s.ppm", directory, name);
    FILE *file = fopen(path, "wb");
    assert(file);
    fprintf(file, "P6\n390 450\n255\n");
    unsigned nonblack = 0;
    for (unsigned y = 0; y < 450; ++y) {
        const unsigned char *row = buffer->data + y * buffer->header.stride;
        for (unsigned x = 0; x < 390; ++x) {
            unsigned char rgb[] = {row[x * 3 + 2], row[x * 3 + 1], row[x * 3]};
            if (rgb[0] || rgb[1] || rgb[2]) nonblack++;
            assert(fwrite(rgb, 1, 3, file) == 3);
        }
    }
    assert(nonblack > 1000);
    assert(fclose(file) == 0);
    lv_draw_buf_destroy(buffer);
}
static void surface(wristflow_ui_shell_t *shell, wristflow_surface_t expected)
{ assert(wristflow_ui_shell_navigation(shell)->surface == expected); }

int main(int argc, char **argv)
{
    assert(argc == 2);
    directory = argv[1];
    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t *display = lv_display_create(390, 450);
    lv_display_set_buffers(display, pixels, NULL, sizeof pixels, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    wristflow_ui_init("");
    const uint32_t caption_glyphs[] = {0x8868, 0x76d8};
    for (unsigned i = 0; i < sizeof(caption_glyphs) / sizeof(caption_glyphs[0]); ++i) {
        lv_font_glyph_dsc_t glyph;
        assert(lv_font_get_glyph_dsc(body_20, &glyph, caption_glyphs[i], 0));
        assert(!glyph.is_placeholder);
    }
    const uint32_t placeholder_glyphs[] = {0x5373, 0x5c06, 0x63a8, 0x51fa};
    for (unsigned i = 0; i < sizeof(placeholder_glyphs) / sizeof(placeholder_glyphs[0]); ++i) {
        lv_font_glyph_dsc_t glyph;
        assert(lv_font_get_glyph_dsc(title_24, &glyph, placeholder_glyphs[i], 0));
        assert(!glyph.is_placeholder);
    }
    lv_indev_t *input = lv_indev_create();
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(input, display);
    lv_indev_set_read_cb(input, read_pointer);
    lv_timer_set_period(lv_indev_get_read_timer(input), 16);
    const wristflow_screen_factory_t cards[] = {screen_tile_heart_rate_create};
    wristflow_ui_shell_config_t config = {&wristflow_default_watchface, cards, 1,
        screen_control_center_create, {22, 48, 53}, true, set_brightness, &brightness};
    wristflow_ui_shell_t *shell = wristflow_ui_shell_create(&config);
    assert(shell && brightness == 60);
    advance(400);
    snapshot("apps_diffusion");
    assert(wristflow_ui_shell_key(shell));
    assert(!wristflow_ui_shell_key(shell));
    advance(400);
    surface(shell, WRISTFLOW_SURFACE_LAUNCHER);
    snapshot("apps_launcher");
    lv_obj_t *launcher = lv_screen_active();
    lv_obj_t *scroll = named("launcher_canvas");
    sample(195, 201, true);
    sample(230, 225, true);
    assert(lv_obj_get_x(scroll) == -95 && lv_obj_get_y(scroll) == -100);
    /* Change direction without lifting: both axes must keep following. */
    sample(195, 260, true);
    assert(lv_obj_get_x(scroll) == -130 && lv_obj_get_y(scroll) == -65);
    sample(195, 201, true);
    sample(195, 201, false);
    advance(400);
    surface(shell, WRISTFLOW_SURFACE_LAUNCHER);
    int32_t scroll_x = lv_obj_get_x(scroll), scroll_y = lv_obj_get_y(scroll);
    click("launch_stopwatch");
    surface(shell, WRISTFLOW_SURFACE_STOPWATCH);
    assert(strcmp(lv_label_get_text(named("stopwatch_time")), "00:00") == 0);
    click("stopwatch_toggle");
    advance(1200);
    snapshot("apps_stopwatch");
    click("app_back");
    assert(lv_screen_active() == launcher);
    assert(lv_obj_get_x(scroll) == scroll_x && lv_obj_get_y(scroll) == scroll_y);
    advance(2000);
    click("launch_stopwatch");
    click("stopwatch_toggle");
    char paused[32];
    snprintf(paused, sizeof paused, "%s", lv_label_get_text(named("stopwatch_time")));
    assert(strcmp(paused, "00:00") != 0);
    advance(2200);
    assert(strcmp(paused, lv_label_get_text(named("stopwatch_time"))) == 0);
    click("stopwatch_reset");
    assert(strcmp(lv_label_get_text(named("stopwatch_time")), "00:00") == 0);
    swipe(10, 220, 290, 220);
    surface(shell, WRISTFLOW_SURFACE_LAUNCHER);

    assert(wristflow_ui_shell_home(shell));
    advance(400);
    sample(195, 170, true);
    advance(600);
    surface(shell, WRISTFLOW_SURFACE_FACE_PICKER);
    sample(195, 170, false);
    advance(400);
    surface(shell, WRISTFLOW_SURFACE_FACE_PICKER);
    snapshot("apps_picker_diffusion");
    sample(300, 220, true);
    sample(220, 220, true);
    assert(lv_obj_get_scroll_x(named("face_carousel")) > 266);
    sample(90, 220, true);
    sample(90, 220, false);
    /* Interrupt a snap with a stationary press: never apply a moving face. */
    sample(195, 220, true);
    sample(195, 220, false);
    advance(400);
    surface(shell, WRISTFLOW_SURFACE_FACE_PICKER);
    assert(strcmp(wristflow_ui_shell_watchface_id(shell), "diffusion") == 0);
    assert(lv_obj_get_scroll_x(named("face_carousel")) == 532);
    swipe(300, 220, 90, 220);
    assert(lv_obj_get_scroll_x(named("face_carousel")) == 266);
    /* Left-edge dragging belongs to the picker, not the app Back gesture. */
    swipe(10, 220, 280, 220);
    surface(shell, WRISTFLOW_SURFACE_FACE_PICKER);
    assert(lv_obj_get_scroll_x(named("face_carousel")) == 532);
    swipe(90, 220, 300, 220);
    swipe(300, 220, 90, 220);
    snapshot("apps_picker_simple");
    assert(lv_obj_find_by_name(lv_screen_active(), "face_previous") == NULL);
    assert(lv_obj_find_by_name(lv_screen_active(), "face_next") == NULL);
    assert(lv_obj_find_by_name(lv_screen_active(), "face_apply") == NULL);
    assert(lv_obj_get_scroll_x(named("face_carousel")) == 532);
    wristflow_ui_shell_back(shell);
    assert(strcmp(wristflow_ui_shell_watchface_id(shell), "diffusion") == 0);
    advance(400);
    sample(195, 170, true);
    advance(600);
    sample(195, 170, false);
    advance(400);
    swipe(300, 220, 90, 220);
    click("face_slot_2");
    surface(shell, WRISTFLOW_SURFACE_HOME);
    assert(strcmp(wristflow_ui_shell_watchface_id(shell), "simple") == 0);
    assert(strcmp(lv_label_get_text(named("simple_time")), "22:48") == 0);
    snapshot("apps_simple");

    swipe(195, 330, 195, 80);
    surface(shell, WRISTFLOW_SURFACE_CONTROLS);
    click("settings_button");
    surface(shell, WRISTFLOW_SURFACE_SETTINGS);
    sample(80, 191, true);
    sample(160, 191, true);
    unsigned midway = brightness;
    sample(250, 191, true);
    assert(brightness > midway + 10);
    sample(342, 191, true);
    assert(brightness >= 98);
    sample(342, 191, false);
    advance(400);
    unsigned chosen = brightness;
    assert(chosen != 60 && chosen >= 10 && chosen <= 100);
    snapshot("apps_settings");
    click("app_back");
    surface(shell, WRISTFLOW_SURFACE_CONTROLS);
    assert(lv_slider_get_value(named("brightness_slider")) == (int)chosen);
    click("flashlight_button");
    surface(shell, WRISTFLOW_SURFACE_FLASHLIGHT);
    assert(brightness == 100);
    assert(lv_color_eq(lv_obj_get_style_bg_color(lv_screen_active(), 0), lv_color_white()));
    snapshot("apps_flashlight");
    sample(195, 220, true);
    sample(195, 220, false);
    advance(400);
    surface(shell, WRISTFLOW_SURFACE_CONTROLS);
    assert(brightness == chosen);
    click("flashlight_button");
    assert(wristflow_ui_shell_key(shell));
    advance(400);
    assert(brightness == chosen);
    surface(shell, WRISTFLOW_SURFACE_HOME);
    assert(strcmp(wristflow_ui_shell_watchface_id(shell), "simple") == 0);

    for (unsigned i = 0; i < 8; ++i) {
        assert(wristflow_ui_shell_key(shell));
        advance(240);
        assert(wristflow_ui_shell_open(shell, WRISTFLOW_SURFACE_HEART));
        advance(240);
        assert(strcmp(lv_label_get_text(named("placeholder_label")), "即将推出") == 0);
        assert(lv_obj_find_by_name(lv_screen_active(), "tile_slots") == NULL);
        if (i == 0) snapshot("apps_heart_placeholder");
        assert(wristflow_ui_shell_back(shell));
        advance(240);
        assert(wristflow_ui_shell_key(shell));
        advance(240);
    }
    const wristflow_surface_t placeholders[] = {WRISTFLOW_SURFACE_ACTIVITY, WRISTFLOW_SURFACE_SYSTEM};
    for (unsigned i = 0; i < 2; ++i) {
        assert(wristflow_ui_shell_key(shell));
        advance(240);
        assert(wristflow_ui_shell_open(shell, placeholders[i]));
        advance(240);
        assert(lv_obj_find_by_name(lv_screen_active(), "tile_slots") == NULL);
        snapshot(i == 0 ? "apps_activity_placeholder" : "apps_battery_placeholder");
        assert(wristflow_ui_shell_key(shell));
        advance(240);
    }
    wristflow_ui_shell_destroy(shell);
    advance(400);
    shell = wristflow_ui_shell_create(&config);
    assert(shell && brightness == 60);
    assert(strcmp(wristflow_ui_shell_watchface_id(shell), "diffusion") == 0);
    wristflow_ui_shell_destroy(shell);
    lv_deinit();
    puts("PASS: pointer launcher, drag guard, retained position, stopwatch background/pause/reset, long press, picker cancel/apply, brightness and flashlight restore, key/back, recreation");
    return 0;
}
