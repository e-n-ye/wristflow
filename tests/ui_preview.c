#include "wristflow_ui.h"
#include "watchface_presenter.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void snapshot(lv_obj_t *screen, const char *directory, const char *name)
{
    lv_screen_load(screen);
    lv_obj_update_layout(screen);
    lv_draw_buf_t *buffer = lv_snapshot_take(screen, LV_COLOR_FORMAT_RGB888);
    assert(buffer && buffer->header.w == 390 && buffer->header.h == 450);
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.ppm", directory, name);
    FILE *file = fopen(path, "wb");
    assert(file);
    fprintf(file, "P6\n390 450\n255\n");
    for (unsigned int y = 0; y < 450; ++y) {
        const unsigned char *row = buffer->data + y * buffer->header.stride;
        for (unsigned int x = 0; x < 390; ++x) {
            unsigned char rgb[] = {row[x * 3 + 2], row[x * 3 + 1], row[x * 3]};
            assert(fwrite(rgb, 1, 3, file) == 3);
        }
    }
    assert(fclose(file) == 0);
    lv_draw_buf_destroy(buffer);
}

static void verify_slot(lv_obj_t *screen, unsigned int index, int x, int y, int width, int height)
{
    char path[40];
    snprintf(path, sizeof(path), "tile_slots/slot_%u", index);
    lv_obj_t *slot = lv_obj_get_child_by_name(screen, path);
    assert(slot);
    assert(lv_obj_get_x(slot) == x && lv_obj_get_y(slot) == y);
    assert(lv_obj_get_width(slot) == width && lv_obj_get_height(slot) == height);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    lv_init();
    lv_display_t *display = lv_display_create(390, 450);
    assert(display);
    lv_theme_t *theme = lv_theme_simple_init(display);
    lv_display_set_theme(display, theme);
    wristflow_ui_init("");
    lv_obj_t *face = screen_watchface_create();
    for (unsigned int time = 0; time < 1440; ++time) {
        char expected[3];
        snprintf(expected, sizeof(expected), "%02u", time % 60);
        assert(wristflow_watchface_set_time(face, time / 60, time % 60));
        assert(strcmp(lv_label_get_text(lv_obj_find_by_name(face, "minute_label")), expected) == 0);
    }
    for (unsigned int hour = 1; hour <= 12; ++hour) {
        char name[32];
        snprintf(name, sizeof(name), "hour_%02u", hour);
        assert(wristflow_watchface_set_time(face, hour, 48));
        snapshot(face, argv[1], name);
    }
    assert(!wristflow_watchface_set_time(face, 24, 0));
    assert(!wristflow_watchface_set_time(NULL, 12, 0));

    lv_obj_t *quarter = screen_tile_heart_rate_create();
    snapshot(quarter, argv[1], "quarters");
    for (unsigned int i = 0; i < 4; ++i)
        verify_slot(quarter, i, (i % 2) * 177, (i / 2) * 188, 165, 174);
    lv_obj_t *top = screen_tile_activity_create();
    snapshot(top, argv[1], "top_half");
    verify_slot(top, 0, 0, 0, 342, 174);
    verify_slot(top, 1, 0, 188, 165, 174);
    verify_slot(top, 2, 177, 188, 165, 174);
    lv_obj_t *bottom = screen_tile_system_create();
    snapshot(bottom, argv[1], "bottom_half");
    verify_slot(bottom, 0, 0, 0, 165, 174);
    verify_slot(bottom, 1, 177, 0, 165, 174);
    verify_slot(bottom, 2, 0, 188, 342, 174);
    lv_obj_t *full = screen_tile_full_create();
    snapshot(full, argv[1], "full");
    verify_slot(full, 0, 0, 0, 342, 362);
    snapshot(screen_layout_picker_create(), argv[1], "layout_picker");
    lv_obj_t *pickers[] = {screen_picker_quarter_create(), screen_picker_half_create(), screen_picker_full_create()};
    const char *names[] = {"picker_quarter", "picker_half", "picker_full"};
    for (unsigned int i = 0; i < 3; ++i) {
        snapshot(pickers[i], argv[1], names[i]);
        lv_obj_t *list = lv_obj_get_child_by_name(pickers[i], "choices");
        assert(list && lv_obj_get_scroll_bottom(list) > 0);
        lv_obj_scroll_to_y(list, lv_obj_get_scroll_bottom(list), LV_ANIM_OFF);
        assert(lv_obj_get_scroll_y(list) > 0);
        char name[48];
        snprintf(name, sizeof(name), "%s_scrolled", names[i]);
        snapshot(pickers[i], argv[1], name);
    }
    snapshot(screen_component_templates_create(), argv[1], "layout_templates_neutral");
    lv_obj_t *hourly = screen_weather_hourly_create();
    lv_obj_t *hour_pager = lv_obj_find_by_name(hourly, "weather_hourly_pager"); assert(hour_pager);
    for (unsigned i = 0; i < 24; ++i) {
        char name[32]; snprintf(name, sizeof name, "forecast_page_%u", i / 4);
        lv_obj_t *page = lv_obj_find_by_name(hour_pager, name); assert(page);
        snprintf(name, sizeof name, "column_%u", i % 4);
        lv_obj_t *column = lv_obj_find_by_name(page, name); assert(column);
        lv_obj_t *icon = lv_obj_find_by_name(column, "hour_wind_icon"); assert(icon);
        lv_font_glyph_dsc_t glyph;
        assert(!strcmp(lv_label_get_text(icon), "\xef\x87\x98"));
        assert(lv_font_get_glyph_dsc(lv_obj_get_style_text_font(icon, 0), &glyph, 0xf1d8, 0) && !glyph.is_placeholder);
    }
    snapshot(hourly, argv[1], "weather_hourly_xml");
    snapshot(screen_compass_calibration_create(), argv[1], "compass_calibration_neutral");
    snapshot(screen_breathing_create(), argv[1], "breathing_neutral_xml");
    lv_obj_t *battery = screen_battery_create();
    snapshot(battery, argv[1], "battery_neutral_xml");
    lv_obj_t *scroll = lv_obj_find_by_name(battery, "battery_scroll"); assert(scroll);
    lv_obj_scroll_to_y(scroll, 260, LV_ANIM_OFF);
    snapshot(battery, argv[1], "battery_neutral_cards");
    puts("PASS: generated LVGL UI, all time updates, four layouts and three scroll lists");
    return 0;
}
