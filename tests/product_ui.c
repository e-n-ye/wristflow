#include "product_ui.h"
#include "wristflow_ui.h"
#include "ui_demo.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t ticks;
static uint8_t pixels[390 * 40 * 4];
static unsigned brightness;
static uint32_t tick(void) { return ticks; }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p)
{ (void)a; (void)p; lv_display_flush_ready(d); }
static void set_brightness(uint8_t value, void *context)
{ assert(context == &brightness); brightness = value; }
static void advance(void)
{
    for (unsigned i = 0; i < 30; ++i) { ticks += 16; lv_timer_handler(); }
    lv_obj_update_layout(lv_screen_active());
}
static lv_obj_t *named(lv_obj_t *root, const char *name)
{
    lv_obj_t *obj = lv_obj_find_by_name(root, name);
    assert(obj);
    return obj;
}
static void text(lv_obj_t *root, const char *name, const char *value)
{ assert(strcmp(lv_label_get_text(named(root, name)), value) == 0); }
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
    /* Every real/loop card has placeholders instead of demo measurements. */
    for (unsigned i = 0; i < 6; ++i) {
        unsigned index = (i + 3) % 4;
        lv_obj_t *panel = lv_obj_get_child(carousel, i);
        if (!index) continue;
        text(named(panel, "slot_0"), "value_label", index == 3 ? "USB" : "--");
        text(named(panel, "slot_1"), "value_label", index == 3 ? "OFF" : "--");
    }
    state = wristflow_product_snapshot(true, WRISTFLOW_TIME_MIN + 60);
    state.uptime_seconds = 3720;
    assert(wristflow_ui_shell_update(shell, &state));
    text(home, "simple_time", "08:01");
    text(named(lv_obj_get_child(carousel, 0), "slot_2"), "value_label", "1:02");
    snapshot(argv[1], "product_time");
    assert(wristflow_ui_shell_open_controls(shell));
    advance();
    assert(lv_slider_get_value(named(lv_screen_active(), "brightness_slider")) == 37);
    assert(wristflow_ui_shell_open(shell, WRISTFLOW_SURFACE_SETTINGS));
    advance();
    text(lv_screen_active(), "settings_battery", "USB");
    assert(wristflow_ui_shell_open(shell, WRISTFLOW_SURFACE_SYSTEM));
    advance();
    text(lv_screen_active(), "placeholder_label", "USB / No battery");
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
