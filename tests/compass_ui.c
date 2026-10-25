#include "product_ui.h"
#include "compass_view.h"
#include "app_registry.h"
#include "wristflow_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t ticks;
static uint8_t pixels[390 * 40 * 4];
static lv_indev_data_t pointer;
static uint32_t tick(void) { return ticks; }
static void read_pointer(lv_indev_t *input, lv_indev_data_t *data)
{ (void)input; *data = pointer; }
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{ (void)area; (void)data; lv_display_flush_ready(display); }
static void advance(void)
{ for (unsigned i = 0; i < 32; ++i) { ticks += 16; lv_timer_handler(); } lv_obj_update_layout(lv_screen_active()); }
static lv_obj_t *named(lv_obj_t *root, const char *name)
{ lv_obj_t *object = lv_obj_find_by_name(root, name); assert(object); return object; }
static void text(lv_obj_t *root, const char *name, const char *expected)
{ assert(!strcmp(lv_label_get_text(named(root, name)), expected)); }
static void click(lv_obj_t *object)
{
    lv_area_t a; lv_obj_get_coords(object, &a);
    pointer.point = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2};
    assert(pointer.point.x >= 0 && pointer.point.x < 390 && pointer.point.y >= 0 && pointer.point.y < 450);
    pointer.state = LV_INDEV_STATE_PRESSED; ticks += 32; lv_timer_handler();
    pointer.state = LV_INDEV_STATE_RELEASED; ticks += 32; lv_timer_handler(); advance();
}
static void edge_back(void)
{
    pointer.point = (lv_point_t){10, 220}; pointer.state = LV_INDEV_STATE_PRESSED;
    ticks += 32; lv_timer_handler();
    for (int x = 30; x <= 290; x += 20) { pointer.point.x = x; ticks += 16; lv_timer_handler(); }
    pointer.state = LV_INDEV_STATE_RELEASED; ticks += 32; lv_timer_handler(); advance();
}
static void glyphs(lv_obj_t *object)
{
    if (lv_obj_check_type(object, &lv_label_class)) {
        const lv_font_t *font = lv_obj_get_style_text_font(object, 0);
        const unsigned char *value = (const unsigned char *)lv_label_get_text(object);
        while (*value) {
            uint32_t cp = *value++;
            if (cp >= 0xe0) { cp = ((cp & 15) << 12) | ((value[0] & 63) << 6) | (value[1] & 63); value += 2; }
            else if (cp >= 0xc0) { cp = ((cp & 31) << 6) | (*value++ & 63); }
            if (cp == '\n') continue;
            lv_font_glyph_dsc_t glyph;
            assert(lv_font_get_glyph_dsc(font, &glyph, cp, 0) && !glyph.is_placeholder);
        }
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(object); ++i) glyphs(lv_obj_get_child(object, i));
}
static void snapshot(const char *directory, const char *name)
{
    advance();
    lv_draw_buf_t *buffer = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888); assert(buffer);
    char path[1024]; snprintf(path, sizeof path, "%s/%s.ppm", directory, name);
    FILE *file = fopen(path, "wb"); assert(file); fprintf(file, "P6\n390 450\n255\n");
    unsigned nonblack = 0;
    for (unsigned y = 0; y < 450; ++y) for (unsigned x = 0; x < 390; ++x) {
        const uint8_t *p = buffer->data + y * buffer->header.stride + 3 * x;
        uint8_t rgb[] = {p[2], p[1], p[0]};
        nonblack += (p[0] | p[1] | p[2]) != 0;
        assert(fwrite(rgb, 1, 3, file) == 3);
    }
    assert(nonblack > 3000); assert(!fclose(file)); lv_draw_buf_destroy(buffer);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    lv_init(); lv_tick_set_cb(tick);
    lv_display_t *display = lv_display_create(390, 450);
    lv_display_set_buffers(display, pixels, NULL, sizeof pixels, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush); wristflow_ui_init("");
    lv_indev_t *input = lv_indev_create(); lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(input, display); lv_indev_set_read_cb(input, read_pointer);
    lv_timer_set_period(lv_indev_get_read_timer(input), 16);
    lv_obj_t *root = screen_compass_create(); lv_screen_load(root); advance();
    wristflow_compass_display_t value = {WRISTFLOW_COMPASS_HEADING, 305};
    wristflow_compass_render(root, &value); advance(); glyphs(root);
    text(root, "compass_heading", "305°"); text(root, "compass_direction", "西北");
    assert(lv_obj_get_style_text_color(named(root, "compass_heading"), 0).red > 0);
    snapshot(argv[1], "compass_305");
    static const unsigned degrees[] = {0, 22, 23, 67, 68, 90, 113, 158, 203, 248, 293, 337, 338, 359};
    static const char *const directions[] = {"北", "北", "东北", "东北", "东", "东", "东南", "南", "西南", "西", "西北", "西北", "北", "北"};
    for (unsigned i = 0; i < sizeof degrees / sizeof degrees[0]; ++i) {
        value.heading_degrees = degrees[i]; wristflow_compass_render(root, &value); advance(); glyphs(root);
        text(root, "compass_direction", directions[i]);
        const char *const marks[] = {"compass_n", "compass_e", "compass_s", "compass_w", "compass_north_pointer", "compass_south_pointer"};
        for (unsigned j = 0; j < sizeof marks / sizeof marks[0]; ++j) {
            lv_area_t a; lv_obj_get_coords(named(root, marks[j]), &a);
            assert(a.x1 >= 20 && a.x2 < 370 && a.y1 >= 66 && a.y2 < 416);
        }
        if (degrees[i] == 0) snapshot(argv[1], "compass_north");
        if (degrees[i] == 90) snapshot(argv[1], "compass_east");
        if (degrees[i] == 359) snapshot(argv[1], "compass_359");
    }
    value.heading_degrees = 360; wristflow_compass_render(root, &value); advance();
    text(root, "compass_heading", "--°"); text(root, "compass_direction", "--");
    assert(lv_obj_has_flag(named(root, "compass_n"), LV_OBJ_FLAG_HIDDEN));
    wristflow_compass_render(root, NULL); snapshot(argv[1], "compass_unknown"); glyphs(root);
    value.state = WRISTFLOW_COMPASS_CALIBRATING; wristflow_compass_render(root, &value); advance(); glyphs(root);
    assert(lv_obj_has_flag(named(root, "compass_dial"), LV_OBJ_FLAG_HIDDEN));
    assert(!lv_obj_has_flag(named(root, "compass_calibration_panel"), LV_OBJ_FLAG_HIDDEN));
    snapshot(argv[1], "compass_calibrating");
    value = (wristflow_compass_display_t){WRISTFLOW_COMPASS_HEADING, 305};
    wristflow_compass_render(root, &value); advance();
    assert(!lv_obj_has_flag(named(root, "compass_dial"), LV_OBJ_FLAG_HIDDEN));
    text(root, "compass_heading", "305°");

    value.uncalibrated = true;
    wristflow_compass_render(root, &value); advance(); glyphs(root);
    text(root, "compass_notice", "未校准，请保持水平");
    lv_area_t notice; lv_obj_get_coords(named(root, "compass_notice"), &notice);
    assert(notice.y1 >= 416 && notice.y2 < 450);
    snapshot(argv[1], "compass_uncalibrated");
    value.state = WRISTFLOW_COMPASS_UNAVAILABLE;
    value.reason = WF_COMPASS_IO_FAILURE;
    wristflow_compass_render(root, &value); advance(); glyphs(root);
    text(root, "compass_heading", "--°"); text(root, "compass_status", "测量失败");
    value.reason = WF_COMPASS_BUSY;
    wristflow_compass_render(root, &value); advance(); glyphs(root);
    text(root, "compass_status", "暂不可用");

    wristflow_watch_snapshot_t state = wristflow_product_snapshot(false, 0);
    wristflow_settings_t settings = wristflow_settings_default();
    settings.screen_timeout = 5;
    wristflow_ui_shell_t *shell = wristflow_product_ui_create(&state, &settings, NULL, NULL); assert(shell);
    wristflow_ui_shell_key(shell); advance();
    lv_obj_t *menu = lv_screen_active();
    lv_obj_t *list = named(menu, "launcher_list");
    lv_obj_t *entry = named(list, "launch_compass");
    lv_obj_scroll_to_view(entry, LV_ANIM_OFF); advance(); glyphs(menu);
    snapshot(argv[1], "compass_menu_list");
    assert(wristflow_app_find("compass")->icon_image == &compass_icon);
    click(entry);
    assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_COMPASS);
    text(lv_screen_active(), "compass_heading", "--°");
    assert(wristflow_ui_shell_compass_sampling(shell));
    wf_compass_snapshot_t sample = {.valid = true, .heading_degrees = 90};
    wristflow_ui_shell_compass_sample(shell, &sample); advance();
    text(lv_screen_active(), "compass_heading", "90°");
    text(lv_screen_active(), "compass_notice", "未校准，请保持水平");
    sample.valid = false; sample.reason = WF_COMPASS_STALE;
    wristflow_ui_shell_compass_sample(shell, &sample); advance();
    text(lv_screen_active(), "compass_heading", "--°");
    lv_obj_t *reminder = lv_obj_create(lv_layer_top());
    assert(wristflow_ui_shell_present_reminder(shell, reminder));
    assert(!wristflow_ui_shell_compass_sampling(shell));
    wristflow_ui_shell_close_reminder(shell); advance();
    assert(wristflow_ui_shell_compass_sampling(shell));
    lv_obj_delete(reminder);
    wristflow_ui_shell_enable_display_policy(shell);
    ticks += 5100; lv_timer_handler();
    assert(wristflow_ui_shell_display_phase(shell) == WRISTFLOW_DISPLAY_OFF);
    assert(!wristflow_ui_shell_compass_sampling(shell));
    sample.valid = true;
    wristflow_ui_shell_compass_sample(shell, &sample);
    assert(wristflow_ui_shell_key(shell)); advance();
    assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_COMPASS);
    assert(wristflow_ui_shell_compass_sampling(shell));
    text(lv_screen_active(), "compass_heading", "--°");
    snapshot(argv[1], "compass_product");
    edge_back();
    assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_LAUNCHER);
    assert(!wristflow_ui_shell_compass_sampling(shell));
    assert(wristflow_ui_shell_open(shell, WRISTFLOW_SURFACE_COMPASS)); advance();
    text(lv_screen_active(), "compass_heading", "--°");
    wristflow_ui_shell_key(shell); advance();
    assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_HOME);
    assert(!wristflow_ui_shell_compass_sampling(shell));
    wristflow_ui_shell_destroy(shell);
    settings.menu_layout = WRISTFLOW_MENU_GRID;
    shell = wristflow_product_ui_create(&state, &settings, NULL, NULL); assert(shell);
    wristflow_ui_shell_key(shell); advance();
    menu = lv_screen_active();
    assert(!lv_obj_has_flag(named(menu, "launcher_grid"), LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(named(menu, "launcher_list"), LV_OBJ_FLAG_HIDDEN));
    entry = named(named(menu, "launcher_grid"), "launch_compass");
    lv_obj_scroll_to_view(entry, LV_ANIM_OFF); advance();
    snapshot(argv[1], "compass_menu_grid");
    click(entry);
    assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_COMPASS);
    text(lv_screen_active(), "compass_heading", "--°");
    wristflow_ui_shell_destroy(shell);
    lv_obj_delete(root); lv_deinit();
    return 0;
}
