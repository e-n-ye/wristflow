#include "product_ui.h"
#include "wristflow_ui.h"
#include "ui_demo.h"
#include "app_registry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t ticks;
static uint8_t pixels[390 * 40 * 4];
static unsigned brightness;
static lv_indev_data_t pointer;
static void read_pointer(lv_indev_t *input, lv_indev_data_t *data) { (void)input; *data = pointer; }
static uint32_t tick(void) { return ticks; }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p)
{ (void)a; (void)p; lv_display_flush_ready(d); }
static void set_brightness(uint8_t value, void *context)
{ assert(context == &brightness); brightness = value; }
static void advance_ms(unsigned duration)
{
    for (unsigned i = 0; i < duration; i += 16) { ticks += 16; lv_timer_handler(); }
    lv_obj_update_layout(lv_screen_active());
}
static void advance(void) { advance_ms(480); }
static void sample(int x, int y, bool down)
{
    pointer.point = (lv_point_t){x, y};
    pointer.state = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    advance_ms(16);
}
static void click(lv_obj_t *obj)
{
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    int x = (a.x1 + a.x2) / 2, y = (a.y1 + a.y2) / 2;
    assert(x >= 0 && x < 390 && y >= 0 && y < 450);
    sample(x, y, true); sample(x, y, false); advance();
}
static void swipe(int x1, int y1, int x2, int y2)
{
    sample(x1, y1, true);
    for (int i = 1; i <= 12; ++i)
        sample(x1 + (x2 - x1) * i / 12, y1 + (y2 - y1) * i / 12, true);
    sample(x2, y2, false); advance_ms(800);
}
static lv_obj_t *named(lv_obj_t *root, const char *name)
{
    lv_obj_t *obj = lv_obj_find_by_name(root, name);
    assert(obj);
    return obj;
}
static void text(lv_obj_t *root, const char *name, const char *value)
{ assert(strcmp(lv_label_get_text(named(root, name)), value) == 0); }
static void glyphs(const lv_font_t *font, const char *string)
{
    const unsigned char *c = (const unsigned char *)string;
    while (*c) {
        uint32_t cp = *c++;
        if (cp >= 0xe0) { cp = ((cp & 15) << 12) | ((c[0] & 63) << 6) | (c[1] & 63); c += 2; }
        else if (cp >= 0xc0) { cp = ((cp & 31) << 6) | (*c++ & 63); }
        lv_font_glyph_dsc_t glyph;
        assert(lv_font_get_glyph_dsc(font, &glyph, cp, 0) && !glyph.is_placeholder);
    }
}
static void snapshot(const char *directory, const char *name)
{
    lv_draw_buf_t *buffer = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);
    assert(buffer);
    char path[1024];
    snprintf(path, sizeof path, "%s/%s.ppm", directory, name);
    FILE *file = fopen(path, "wb");
    assert(file);
    fprintf(file, "P6\n390 450\n255\n");
    for (unsigned y = 0; y < 450; ++y) {
        const unsigned char *row = buffer->data + y * buffer->header.stride;
        for (unsigned x = 0; x < 390; ++x) {
            unsigned char rgb[] = {row[x*3+2], row[x*3+1], row[x*3]};
            assert(fwrite(rgb, 1, 3, file) == 3);
        }
    }
    assert(fclose(file) == 0);
    lv_draw_buf_destroy(buffer);
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
    wristflow_settings_t settings = {37, "simple"}, chosen;
    wristflow_watch_snapshot_t state = wristflow_product_snapshot(false, 0);
    wristflow_ui_shell_t *shell = wristflow_product_ui_create(&state, &settings, set_brightness, &brightness);
    assert(shell && brightness == 37);
    assert(wristflow_ui_shell_navigation(shell)->page_count == 4);
    advance();
    text(lv_screen_active(), "simple_time", "--:--");
    text(lv_screen_active(), "simple_battery", "USB");
    snapshot(argv[1], "product_unsynced");
    lv_obj_t *home = lv_screen_active();
    lv_obj_t *carousel = named(home, "demo_carousel");
    const char *const expected[3][4] = {
        {"heart_rate", "blood_oxygen", "battery", "stress"},
        {"activity", "steps", "standing", NULL},
        {"weather", "sleep", "alarm", NULL}};
    const char *const slot_names[] = {"slot_0", "slot_1", "slot_2", "slot_3"};
    unsigned instances = 0;
    for (unsigned p = 0; p < 3; ++p) {
        const wristflow_card_page_t *page = wristflow_product_default_page(p);
        assert(page && page->count == (p == 0 ? 4 : 3));
        for (unsigned s = 0; s < page->count; ++s) {
            const wristflow_component_t *slot = &page->slots[s];
            assert(strcmp(slot->app_id, expected[p][s]) == 0);
            assert(slot->instance_id == ++instances);
            assert(slot->size == (p && !s ? WRISTFLOW_CARD_HALF : WRISTFLOW_CARD_QUARTER));
            const wristflow_app_descriptor_t *app = wristflow_app_find(slot->app_id);
            assert(app && app->capability == WRISTFLOW_CAPABILITY_PLACEHOLDER);
            assert(app->card_sizes & slot->size);
        }
    }
    assert(instances == 10 && wristflow_app_count() == 14);
    assert(!wristflow_product_default_page(3) && !wristflow_app_find("missing"));
    /* Every real/loop card has placeholders instead of demo measurements. */
    for (unsigned i = 0; i < 6; ++i) {
        unsigned index = (i + 3) % 4;
        lv_obj_t *panel = lv_obj_get_child(carousel, i);
        if (!index) continue;
        const wristflow_card_page_t *page = wristflow_product_default_page(index - 1);
        for (unsigned s = 0; s < page->count; ++s)
            text(named(panel, slot_names[s]), "value_label", index == 1 && s == 2 ? "USB" : "--");
    }
    state = wristflow_product_snapshot(true, WRISTFLOW_TIME_MIN + 60);
    state.uptime_seconds = 3720;
    assert(wristflow_ui_shell_update(shell, &state));
    text(home, "simple_time", "08:01");
    snapshot(argv[1], "product_time");
    /* Real pointer input: every default slot routes and returns to the retained page. */
    for (unsigned p = 0; p < 3; ++p) {
        swipe(320, 125, 60, 125);
        assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_HOME);
        assert(wristflow_ui_shell_navigation(shell)->page_index == p + 1);
        lv_obj_t *panel = lv_obj_get_child(carousel, p + 2);
        char name[40]; snprintf(name, sizeof name, "product_cards_%u", p + 1);
        snapshot(argv[1], name);
        const wristflow_card_page_t *page = wristflow_product_default_page(p);
        for (unsigned s = 0; s < page->count; ++s) {
            const wristflow_app_descriptor_t *app = wristflow_app_find(expected[p][s]);
            lv_obj_t *card = named(panel, slot_names[s]);
            click(card);
            assert(wristflow_ui_shell_navigation(shell)->surface == app->surface);
            text(lv_screen_active(), "app_title", app->title);
            text(lv_screen_active(), "placeholder_label", app->reason);
            if (!s) { snprintf(name, sizeof name, "product_placeholder_%u", p + 1); snapshot(argv[1], name); }
            if (s & 1) swipe(10, 230, 185, 230);
            else click(named(lv_screen_active(), "app_back"));
            assert(lv_screen_active() == home);
            assert(wristflow_ui_shell_navigation(shell)->page_index == p + 1);
            assert(lv_obj_get_scroll_x(carousel) == (int32_t)(p + 2) * 390);
            assert(named(panel, slot_names[s]) == card);
        }
    }
    /* Both loop clones recenter without losing routing; KEY1 discards the source. */
    swipe(320, 125, 60, 125);
    assert(wristflow_ui_shell_navigation(shell)->page_index == 0);
    swipe(60, 125, 320, 125);
    assert(wristflow_ui_shell_navigation(shell)->page_index == 3);
    click(named(lv_obj_get_child(carousel, 4), "slot_2"));
    assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_ALARM);
    assert(wristflow_ui_shell_key(shell)); advance();
    assert(wristflow_ui_shell_navigation(shell)->page_index == 0);
    assert(lv_obj_get_scroll_x(carousel) == 390);
    /* Drag away and return while held must not be mistaken for a click. */
    swipe(320, 125, 60, 125);
    sample(100, 125, true); sample(155, 125, true); sample(100, 125, true);
    sample(100, 125, false); advance_ms(800);
    assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_HOME);
    assert(wristflow_ui_shell_home(shell)); advance();
    /* Each registry icon and label has an actual glyph in the exported fonts. */
    for (size_t i = 0; i < wristflow_app_count(); ++i) {
        const wristflow_app_descriptor_t *app = wristflow_app_at(i);
        assert(wristflow_app_for_surface(app->surface) == app);
        for (size_t j = 0; j < i; ++j) assert(strcmp(wristflow_app_at(j)->id, app->id));
        const unsigned char *icon = (const unsigned char *)app->icon;
        uint32_t cp = ((icon[0] & 15) << 12) | ((icon[1] & 63) << 6) | (icon[2] & 63);
        lv_font_glyph_dsc_t glyph;
        assert(lv_font_get_glyph_dsc(icons_44, &glyph, cp, 0) && !glyph.is_placeholder);
        glyphs(body_20, app->title); glyphs(title_24, app->title);
        glyphs(body_20, app->reason);
    }
    assert(wristflow_ui_shell_key(shell)); advance();
    snapshot(argv[1], "product_launcher");
    lv_obj_t *launcher = lv_screen_active();
    for (size_t i = 0; i < wristflow_app_count(); ++i) {
        const wristflow_app_descriptor_t *app = wristflow_app_at(i);
        lv_obj_t *icon = named(launcher, app->launcher_name);
        for (unsigned tries = 0; tries < 8; ++tries) {
            lv_area_t a; lv_obj_get_coords(icon, &a);
            int x = (a.x1 + a.x2) / 2, y = (a.y1 + a.y2) / 2;
            if (x >= 45 && x <= 345 && y >= 45 && y <= 350) break;
            assert(tries < 7);
            int dx = x < 45 ? 150 : x > 345 ? -150 : 0;
            int dy = y < 45 ? 150 : y > 350 ? -150 : 0;
            swipe(195, 200, 195 + dx, 200 + dy);
            assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_LAUNCHER);
        }
        click(icon);
        assert(wristflow_ui_shell_navigation(shell)->surface == app->surface);
        if (app->capability == WRISTFLOW_CAPABILITY_PLACEHOLDER) text(lv_screen_active(), "app_title", app->title);
        assert(wristflow_ui_shell_back(shell)); advance();
        assert(lv_screen_active() == launcher);
    }
    assert(wristflow_ui_shell_key(shell)); advance();
    assert(wristflow_ui_shell_open_controls(shell));
    advance();
    assert(lv_slider_get_value(named(lv_screen_active(), "brightness_slider")) == 37);
    assert(wristflow_ui_shell_open(shell, WRISTFLOW_SURFACE_SETTINGS));
    advance();
    text(lv_screen_active(), "settings_battery", "USB");
    assert(wristflow_ui_shell_open(shell, WRISTFLOW_SURFACE_SYSTEM));
    advance();
    text(lv_screen_active(), "placeholder_label", "USB 供电，未接入电池");
    assert(wristflow_ui_shell_back(shell));
    advance();
    lv_obj_t *slider = named(lv_screen_active(), "settings_brightness");
    lv_slider_set_value(slider, 72, LV_ANIM_OFF);
    lv_obj_send_event(slider, LV_EVENT_VALUE_CHANGED, NULL);
    assert(brightness == 72);
    assert(wristflow_ui_shell_get_settings(shell, &chosen) && chosen.brightness == 72);
    assert(wristflow_ui_shell_open(shell, WRISTFLOW_SURFACE_FLASHLIGHT));
    advance();
    assert(brightness == 100);
    assert(wristflow_ui_shell_get_settings(shell, &chosen) && chosen.brightness == 72);
    state = wristflow_product_snapshot(true, WRISTFLOW_TIME_MIN + 120);
    assert(wristflow_ui_shell_update(shell, &state));
    assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_FLASHLIGHT);
    assert(wristflow_ui_shell_key(shell));
    advance();
    text(lv_screen_active(), "simple_time", "08:02");
    assert(wristflow_ui_shell_set_watchface(shell, &wristflow_default_watchface));
    assert(wristflow_ui_shell_get_settings(shell, &chosen));
    assert(strcmp(chosen.face_id, "diffusion") == 0);
    uint8_t record[WRISTFLOW_SETTINGS_BYTES];
    assert(wristflow_settings_encode(&chosen, record));
    wristflow_ui_shell_destroy(shell);
    /* Simulated restart: restore the exact committed UI preference blob. */
    settings = wristflow_settings_default();
    assert(wristflow_settings_decode(&settings, record, sizeof record));
    shell = wristflow_product_ui_create(&state, &settings, set_brightness, &brightness);
    advance();
    assert(shell && brightness == 72 && strcmp(wristflow_ui_shell_watchface_id(shell), "diffusion") == 0);
    text(lv_screen_active(), "battery_label", "USB");
    assert(!lv_obj_has_flag(named(lv_screen_active(), "hour_artwork"), LV_OBJ_FLAG_HIDDEN));
    snapshot(argv[1], "product_diffusion");
    state = wristflow_product_snapshot(false, 0);
    wristflow_ui_shell_update(shell, &state);
    assert(lv_obj_has_flag(named(lv_screen_active(), "hour_artwork"), LV_OBJ_FLAG_HIDDEN));
    text(lv_screen_active(), "minute_label", "--:--");
    wristflow_ui_shell_destroy(shell);
    /* The same binary's demo composition still has deterministic sample data. */
    wristflow_demo_start();
    advance();
    text(lv_screen_active(), "battery_label", "53%");
    text(lv_screen_active(), "minute_label", "48");
    assert(!lv_obj_has_flag(named(lv_screen_active(), "hour_artwork"), LV_OBJ_FLAG_HIDDEN));
    wristflow_demo_stop();
    lv_deinit();
    return 0;
}
