#include "weather_screen.h"
#include "weather_fixture.h"
#include "app_registry.h"
#include "product_state.h"
#include "wristflow_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t ticks;
static uint8_t pixels[390 * 40 * 4];
static lv_indev_data_t pointer;

static uint32_t tick(void) { return ticks; }
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *buffer)
{ (void)area; (void)buffer; lv_display_flush_ready(display); }
static void read_pointer(lv_indev_t *input, lv_indev_data_t *data)
{ (void)input; *data = pointer; }
static void advance_ms(unsigned duration)
{
    for (unsigned elapsed = 0; elapsed < duration; elapsed += 16) {
        ticks += 16;
        lv_timer_handler();
    }
    lv_obj_update_layout(lv_screen_active());
}
static void sample(int x, int y, bool pressed)
{
    pointer.point = (lv_point_t){x, y};
    pointer.state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    advance_ms(16);
}
static void swipe(int x1, int y1, int x2, int y2)
{
    sample(x1, y1, true);
    for (int i = 1; i <= 12; ++i)
        sample(x1 + (x2 - x1) * i / 12, y1 + (y2 - y1) * i / 12, true);
    sample(x2, y2, false);
    advance_ms(800);
}
static lv_obj_t *named(lv_obj_t *root, const char *name)
{
    lv_obj_t *object = lv_obj_find_by_name(root, name);
    assert(object);
    return object;
}
static void text(lv_obj_t *root, const char *name, const char *expected)
{ assert(strcmp(lv_label_get_text(named(root, name)), expected) == 0); }
static void click(lv_obj_t *object)
{
    lv_area_t area;
    lv_obj_get_coords(object, &area);
    int x = (area.x1 + area.x2) / 2;
    int y = (area.y1 + area.y2) / 2;
    sample(x, y, true);
    sample(x, y, false);
    advance_ms(480);
}
static void snapshot(const char *directory, const char *name)
{
    lv_draw_buf_t *buffer = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);
    assert(buffer);
    char path[1024];
    snprintf(path, sizeof path, "%s/%s.ppm", directory, name);
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fprintf(file, "P6\n390 450\n255\n") > 0);
    for (unsigned y = 0; y < 450; ++y) {
        const unsigned char *row = buffer->data + y * buffer->header.stride;
        for (unsigned x = 0; x < 390; ++x) {
            unsigned char rgb[] = {row[x * 3 + 2], row[x * 3 + 1], row[x * 3]};
            assert(fwrite(rgb, 1, 3, file) == 3);
        }
    }
    assert(fclose(file) == 0);
    lv_draw_buf_destroy(buffer);
}
static bool at_offset(lv_coord_t value, lv_coord_t expected)
{ return value == expected || value == -expected; }
static void glyph(const lv_font_t *font, const char *string)
{
    const unsigned char *bytes = (const unsigned char *)string;
    uint32_t codepoint = ((bytes[0] & 15U) << 12) | ((bytes[1] & 63U) << 6) | (bytes[2] & 63U);
    lv_font_glyph_dsc_t descriptor;
    assert(lv_font_get_glyph_dsc(font, &descriptor, codepoint, 0) && !descriptor.is_placeholder);
}
static void codepoint_glyph(const lv_font_t *font, uint32_t codepoint)
{
    lv_font_glyph_dsc_t descriptor;
    assert(lv_font_get_glyph_dsc(font, &descriptor, codepoint, 0) && !descriptor.is_placeholder);
}

static void missing_forecasts(lv_obj_t *screen)
{
    for (unsigned i = 0; i < WRISTFLOW_WEATHER_HOURLY_COUNT; ++i) {
        char name[24];
        snprintf(name, sizeof name, "weather_hour_%u", i);
        lv_obj_t *host = named(screen, name);
        text(host, "hour_time", "--:--");
        text(host, "hour_temp", "--");
        text(host, "hour_icon", "");
        text(host, "hour_wind", "--");
        text(host, "hour_air", "--");
    }
    for (unsigned i = 0; i < WRISTFLOW_WEATHER_DAILY_COUNT; ++i) {
        char name[24];
        snprintf(name, sizeof name, "weather_day_%u", i);
        lv_obj_t *host = named(screen, name);
        text(host, "day_name", "--");
        text(host, "day_range", "--/--");
        text(host, "day_icon", "");
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t *display = lv_display_create(390, 450);
    lv_display_set_buffers(display, pixels, NULL, sizeof pixels, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    wristflow_ui_init("");
    lv_indev_t *input = lv_indev_create();
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(input, display);
    lv_indev_set_read_cb(input, read_pointer);
    lv_timer_set_period(lv_indev_get_read_timer(input), 16);

    glyph(icons_44, "\xef\x83\x82");
    glyph(icons_44, "\xef\x81\x83");
    glyph(icons_44, "\xef\x86\x85");
    glyph(icons_20, "\xef\x81\x83");
    glyph(icons_20, "\xef\x81\x93");
    glyph(icons_20, "\xef\x87\x98");
    glyph(icons_20, "\xef\x86\x86");
    codepoint_glyph(metric_56, 0x00b0);
    codepoint_glyph(title_24, 0x00b0);

    lv_obj_t *screen = screen_weather_create();
    lv_screen_load(screen);
    advance_ms(480);
    text(screen, "weather_temp", "--");
    text(screen, "weather_condition", "--");
    text(screen, "weather_state_label", "暂无天气数据");
    text(screen, "weather_sun_clock", "--:--");
    missing_forecasts(screen);
    assert(lv_obj_has_flag(named(screen, "weather_pager"), LV_OBJ_FLAG_HIDDEN));
    snapshot(argv[1], "weather_product_empty");

    wristflow_weather_screen_set_state(screen, WRISTFLOW_WEATHER_LOADING);
    text(screen, "weather_state_label", "正在同步天气");
    assert(lv_obj_has_flag(named(screen, "weather_retry"), LV_OBJ_FLAG_HIDDEN));
    wristflow_weather_screen_set_state(screen, WRISTFLOW_WEATHER_EMPTY);
    text(screen, "weather_state_label", "暂无天气数据");
    click(named(screen, "weather_retry"));
    text(screen, "weather_state_label", "天气更新失败");
    text(screen, "weather_temp", "--");
    wristflow_weather_screen_set_state(screen, WRISTFLOW_WEATHER_LOADING);
    text(screen, "weather_state_label", "正在同步天气");
    assert(lv_obj_has_flag(named(screen, "weather_retry"), LV_OBJ_FLAG_HIDDEN));
    wristflow_weather_screen_set_state(screen, WRISTFLOW_WEATHER_READY);
    text(screen, "weather_state_label", "暂无天气数据");
    text(screen, "weather_temp", "--");

    wristflow_phone_weather_t current = {.temp = -5, .code = -1};
    wristflow_weather_update(&current);
    wristflow_weather_screen_refresh(screen);
    text(screen, "weather_temp", "-5°");
    text(screen, "weather_range", "--/--");
    text(screen, "weather_condition", "--");
    text(screen, "weather_aqi", "--");
    text(screen, "weather_index_1_value", "--");
    text(screen, "weather_sunrise", "--:--");
    text(screen, "weather_sunset", "--:--");
    assert(lv_obj_has_flag(named(screen, "weather_sun_position"), LV_OBJ_FLAG_HIDDEN));
    missing_forecasts(screen);
    snapshot(argv[1], "weather_product_partial");

    current.temp = 0;
    current.humidity_valid = true;
    current.humidity = 0;
    current.high = 26;
    current.low = 18;
    current.range_valid = true;
    current.code = 800;
    strcpy(current.city, "杭州市");
    strcpy(current.condition, "晴");
    strcpy(current.wind, "12 km/h");
    wristflow_weather_update(&current);
    wristflow_weather_screen_refresh(screen);
    text(screen, "weather_temp", "0°");
    text(screen, "weather_range", "26°/18°");
    text(screen, "weather_index_1_value", "0");
    text(screen, "weather_index_2_value", "12 km/h");
    text(screen, "weather_index_3_value", "--");
    assert(lv_color_eq(lv_obj_get_style_bg_color(named(screen, "weather_page_sun"), 0), WEATHER_BLUE));
    snapshot(argv[1], "weather_product_current");
    lv_obj_scroll_to_y(named(screen, "weather_pager"), 3 * 450, LV_ANIM_OFF);
    advance_ms(32);
    snapshot(argv[1], "weather_product_indices");
    lv_obj_scroll_to_y(named(screen, "weather_pager"), 4 * 450, LV_ANIM_OFF);
    advance_ms(32);
    snapshot(argv[1], "weather_product_sun");

    current.range_valid = false;
    wristflow_weather_update(&current);
    wristflow_weather_screen_refresh(screen);
    text(screen, "weather_range", "--/--");

    wristflow_weather_reset();
    wristflow_weather_screen_refresh(screen);
    text(screen, "weather_state_label", "暂无天气数据");
    text(screen, "weather_temp", "--");
    missing_forecasts(screen);
    assert(lv_obj_has_flag(named(screen, "weather_pager"), LV_OBJ_FLAG_HIDDEN));
    lv_obj_delete(screen);
    screen = screen_weather_create();
    lv_screen_load(screen);
    text(screen, "weather_state_label", "暂无天气数据");
    text(screen, "weather_temp", "--");
    wristflow_app_data_t card;
    wristflow_watch_snapshot_t watch = wristflow_product_snapshot(false, 0);
    assert(wristflow_app_read(wristflow_app_find("weather"), &watch, &card));
    assert(!strcmp(card.value, "--") && !strcmp(card.reason, "暂无天气数据"));
    lv_obj_delete(screen);

    screen = screen_weather_demo_create(WRISTFLOW_WEATHER_THEME_CLOUDY);
    lv_screen_load(screen);
    advance_ms(480);
    text(screen, "weather_city", "乐清市");
    text(screen, "weather_temp", "30°");
    text(screen, "weather_condition", "多云");
    assert(lv_color_eq(lv_obj_get_style_bg_color(screen, 0), lv_color_hex(0x687f91)));
    assert(lv_color_eq(lv_obj_get_style_bg_color(named(screen, "weather_page_sun"), 0),
                       lv_color_hex(0x687f91)));
    assert(lv_color_eq(lv_obj_get_style_text_color(named(screen, "weather_temp"), 0), FG_PRIMARY));
    assert(!lv_obj_find_by_name(screen, "app_title"));
    assert(!lv_obj_find_by_name(screen, "weather_dots"));
    snapshot(argv[1], "weather_current");

    lv_obj_t *outer = named(screen, "weather_pager");
    swipe(195, 404, 195, 74);
    assert(at_offset(lv_obj_get_scroll_y(outer), 450));
    assert(wristflow_weather_screen_is_horizontal(screen));
    assert(!wristflow_weather_screen_edge_back_allowed(screen, (lv_point_t){10, 220}));
    assert(wristflow_weather_screen_edge_back_allowed(screen, (lv_point_t){10, 40}));
    text(screen, "weather_hourly_title", "天气预测");
    snapshot(argv[1], "weather_hourly");
    lv_obj_t *hourly = named(screen, "weather_hourly_pager");
    /* A single horizontal gesture advances one forecast page, even when it is fast. */
    swipe(350, 240, 20, 240);
    assert(lv_obj_get_scroll_x(hourly) == 390);
    assert(lv_color_eq(lv_obj_get_style_bg_color(
        named(screen, "weather_hourly_dots_1"), 0), FG_PRIMARY));
    swipe(20, 240, 350, 240);
    assert(lv_obj_get_scroll_x(hourly) == 0);
    lv_obj_scroll_to_x(hourly, 0, LV_ANIM_OFF);
    advance_ms(32);
    /* The middle of the forecast is allowed to hand a vertical gesture to the outer pager. */
    swipe(195, 240, 195, 70);
    assert(at_offset(lv_obj_get_scroll_y(outer), 2 * 450));
    assert(wristflow_weather_screen_is_horizontal(screen));
    text(screen, "weather_daily_title", "未来天气");
    lv_obj_t *daily = named(screen, "weather_daily_pager");
    swipe(350, 240, 20, 240);
    assert(lv_obj_get_scroll_x(daily) == 390);
    assert(lv_color_eq(lv_obj_get_style_bg_color(
        named(screen, "weather_daily_dots_1"), 0), FG_PRIMARY));
    lv_obj_scroll_to_x(daily, 0, LV_ANIM_OFF);
    advance_ms(32);
    /* The same middle-band vertical handoff works on the second forecast page. */
    swipe(195, 240, 195, 70);
    assert(at_offset(lv_obj_get_scroll_y(outer), 3 * 450));
    text(screen, "weather_indices_title", "天气指数");

    lv_obj_scroll_to_y(outer, 450, LV_ANIM_OFF);
    advance_ms(32);
    hourly = named(screen, "weather_hourly_pager");
    lv_obj_scroll_to_x(hourly, 5 * 390, LV_ANIM_OFF);
    advance_ms(32);
    lv_obj_send_event(hourly, LV_EVENT_SCROLL_END, NULL);
    assert(at_offset(lv_obj_get_scroll_x(hourly), 5 * 390));
    assert(lv_color_eq(lv_obj_get_style_bg_color(
        named(screen, "weather_hourly_dots_5"), 0), FG_PRIMARY));
    lv_obj_scroll_to_x(hourly, 0, LV_ANIM_OFF);
    advance_ms(32);
    assert(lv_obj_get_scroll_x(hourly) == 0);

    lv_obj_scroll_to_y(outer, 2 * 450, LV_ANIM_OFF);
    advance_ms(32);
    assert(at_offset(lv_obj_get_scroll_y(outer), 2 * 450));
    assert(wristflow_weather_screen_is_horizontal(screen));
    assert(!wristflow_weather_screen_edge_back_allowed(screen, (lv_point_t){10, 220}));
    text(screen, "weather_daily_title", "未来天气");
    snapshot(argv[1], "weather_daily");
    daily = named(screen, "weather_daily_pager");
    lv_obj_scroll_to_x(daily, 390, LV_ANIM_OFF);
    advance_ms(32);
    lv_obj_send_event(daily, LV_EVENT_SCROLL_END, NULL);
    assert(at_offset(lv_obj_get_scroll_x(daily), 390));
    assert(lv_color_eq(lv_obj_get_style_bg_color(
        named(screen, "weather_daily_dots_1"), 0), FG_PRIMARY));

    lv_obj_scroll_to_y(outer, 3 * 450, LV_ANIM_OFF);
    advance_ms(32);
    text(screen, "weather_indices_title", "天气指数");
    text(screen, "weather_index_3_value", "4");
    named(screen, "weather_index_0_icon");
    snapshot(argv[1], "weather_indices");
    assert(!wristflow_weather_screen_is_horizontal(screen));

    lv_obj_scroll_to_y(outer, 4 * 450, LV_ANIM_OFF);
    advance_ms(32);
    text(screen, "weather_sun_title", "日升日落");
    lv_obj_t *sun_track = named(screen, "weather_sun_track_image");
    lv_obj_t *sun_position = named(screen, "weather_sun_position");
    assert(lv_obj_get_x(sun_track) == 27 && lv_obj_get_y(sun_track) == 157);
    assert(lv_obj_get_width(sun_track) == 336 && lv_obj_get_height(sun_track) == 128);
    assert(lv_image_get_src(sun_track) == weather_sun_track);
    const lv_image_dsc_t *track_dsc = (const lv_image_dsc_t *)weather_sun_track;
    assert(track_dsc->header.cf == LV_COLOR_FORMAT_ARGB8888);
    assert(track_dsc->header.w == 336 && track_dsc->header.h == 128);
    assert(track_dsc->header.stride == 336 * 4);
    const lv_color32_t *track_px = (const lv_color32_t *)track_dsc->data;
    /* White daytime arc: apex (168,3) is high and opaque. */
    assert(track_px[3 * 336 + 168].red > 200 && track_px[3 * 336 + 168].alpha > 150);
    /* The dashed horizon crosses the arc endpoints at y=83. */
    assert(track_px[83 * 336 + 5].red > 200 && track_px[83 * 336 + 5].alpha > 100);
    assert(track_px[83 * 336 + 10].alpha == 0);
    /* Below-horizon extensions are dark translucent pixels, not white arc pixels. */
    assert(track_px[110 * 336 + 20].red < 100 && track_px[110 * 336 + 20].alpha > 0);
    assert(track_px[110 * 336 + 320].red < 100 && track_px[110 * 336 + 320].alpha > 0);
    int sun_center_x = lv_obj_get_x(sun_position) + lv_obj_get_width(sun_position) / 2;
    int sun_center_y = lv_obj_get_y(sun_position) + lv_obj_get_height(sun_position) / 2;
    /* Track image global position is x=27..363, y=157..285. The point is
     * on the descending daytime arc and above its y=240 horizon. */
    assert(sun_center_x == 272 && sun_center_y == 199);
    assert(sun_center_x > 27 + 0.70 * 336 && sun_center_x < 27 + 0.80 * 336);
    assert(sun_center_y < 157 + 83);
    snapshot(argv[1], "weather_sun");
    assert(!wristflow_weather_screen_is_horizontal(screen));
    assert(wristflow_weather_screen_edge_back_allowed(screen, (lv_point_t){10, 220}));

    /* A real current-only payload replaces every demonstration forecast. */
    wristflow_weather_update(&current);
    wristflow_weather_screen_refresh(screen);
    missing_forecasts(screen);
    text(screen, "weather_range", "--/--");
    assert(lv_obj_has_flag(named(screen, "weather_sun_position"), LV_OBJ_FLAG_HIDDEN));
    assert(wristflow_app_read(wristflow_app_find("weather"), &watch, &card));
    assert(!strcmp(card.value, "0°") && !strcmp(card.reason, "晴"));
    wristflow_weather_reset();
    lv_obj_delete(screen);

    lv_obj_t *sunny = screen_weather_demo_create(WRISTFLOW_WEATHER_THEME_SUNNY);
    lv_screen_load(sunny);
    advance_ms(480);
    text(sunny, "weather_condition", "晴");
    assert(lv_color_eq(lv_obj_get_style_bg_color(sunny, 0), lv_color_hex(0x0bb9f2)));
    assert(!lv_color_eq(lv_obj_get_style_bg_color(sunny, 0), lv_color_hex(0x687f91)));
    assert(lv_color_eq(lv_obj_get_style_bg_color(named(sunny, "weather_page_indices"), 0),
                       lv_color_hex(0x0bb9f2)));
    assert(lv_color_eq(lv_obj_get_style_text_color(named(sunny, "weather_condition"), 0), FG_PRIMARY));
    snapshot(argv[1], "weather_current_sunny");
    lv_obj_delete(sunny);
    lv_deinit();
    return 0;
}
