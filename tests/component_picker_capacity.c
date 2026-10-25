#include "product_ui.h"
#include "app_registry.h"
#include "wristflow_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Host-only registry stress: every existing app supplies every card size.
   GNU link wrappers keep this fixture out of Product and its real registry. */
static wristflow_app_descriptor_t entries[64];
static size_t count;
static uint32_t ticks;
static lv_indev_data_t pointer;
static uint8_t pixels[390 * 40 * 4];
const wristflow_app_descriptor_t *__real_wristflow_app_at(size_t index);
const wristflow_app_descriptor_t *__real_wristflow_app_find(const char *id);
const wristflow_app_descriptor_t *__wrap_wristflow_app_at(size_t index)
{ return index < count ? &entries[index] : NULL; }
const wristflow_app_descriptor_t *__wrap_wristflow_app_find(const char *id)
{
    if (id) for (size_t i = 0; i < count; ++i)
        if (!strcmp(entries[i].id, id)) return &entries[i];
    return __real_wristflow_app_find(id);
}
static void read_fixture(const wristflow_watch_snapshot_t *snapshot, wristflow_app_data_t *data)
{ (void)snapshot; strcpy(data->value, "42"); }
static uint32_t tick(void) { return ticks; }
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{ (void)area; (void)data; lv_display_flush_ready(display); }
static void read_pointer(lv_indev_t *input, lv_indev_data_t *data)
{ (void)input; *data = pointer; }
static void advance(void)
{
    for (unsigned i = 0; i < 32; ++i) { ticks += 16; lv_timer_handler(); }
    lv_obj_update_layout(lv_screen_active());
}
static lv_obj_t *named(const char *name)
{ lv_obj_t *obj = lv_obj_find_by_name(lv_screen_active(), name); assert(obj); return obj; }
static void sample(int x, int y, bool down)
{
    pointer.point = (lv_point_t){x, y};
    pointer.state = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    ticks += 16; lv_timer_handler();
}
static void click(lv_obj_t *obj)
{
    lv_area_t area; lv_obj_get_coords(obj, &area);
    int x = (area.x1 + area.x2) / 2, y = (area.y1 + area.y2) / 2;
    assert(x >= 24 && x < 366 && y >= 80 && y < 430);
    sample(x, y, true); sample(x, y, false); advance();
}
static void enter_editor(void)
{
    sample(320,125,true);
    for (int i = 1; i <= 12; ++i) sample(320 - 260*i/12,125,true);
    sample(60,125,false); advance(); advance();
    sample(100,110,true); advance(); advance();
    sample(100,110,false); advance();
}

int main(void)
{
    count = wristflow_app_count(); assert(count <= 64 && count * 2 > 32);
    for (size_t i = 0; i < count; ++i) {
        entries[i] = *__real_wristflow_app_at(i);
        entries[i].card_sizes = WRISTFLOW_CARD_QUARTER | WRISTFLOW_CARD_HALF | WRISTFLOW_CARD_FULL;
        entries[i].read = read_fixture;
    }
    lv_init(); lv_tick_set_cb(tick);
    lv_display_t *display = lv_display_create(390, 450);
    lv_display_set_buffers(display, pixels, NULL, sizeof pixels, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush); wristflow_ui_init("");
    lv_indev_t *input = lv_indev_create(); lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(input, display); lv_indev_set_read_cb(input, read_pointer);
    lv_timer_set_period(lv_indev_get_read_timer(input), 16);
    const wristflow_template_t types[] = {WRISTFLOW_LAYOUT_QUARTERS, WRISTFLOW_LAYOUT_TOP_HALF, WRISTFLOW_LAYOUT_FULL};
    for (unsigned t = 0; t < 3; ++t) for (unsigned variant = 0; variant < 2; ++variant) {
        wristflow_layout_t layout = {.count = 1};
        layout.pages[0].template_id = types[t];
        for (unsigned s = 0; s < wristflow_template_slots(types[t]); ++s) {
            wristflow_component_t *slot = &layout.pages[0].slots[s];
            strcpy(slot->app_id, "heart_rate"); slot->instance_id = (uint16_t)(s + 1);
            slot->size = wristflow_template_size(types[t], s);
        }
        wristflow_watch_snapshot_t state = wristflow_product_snapshot(false, 0);
        wristflow_settings_t settings = wristflow_settings_default();
        wristflow_ui_shell_t *shell = wristflow_product_ui_create_with_layout(
            &state, &settings, NULL, NULL, &layout, NULL, NULL, NULL);
        assert(shell); advance();
        enter_editor();
        assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_COMPONENT_EDITOR);
        lv_obj_send_event(named("slot_0"), LV_EVENT_SHORT_CLICKED, NULL); advance();
        assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_COMPONENT_PICKER);
        for (size_t i = 0; i < count; ++i) for (unsigned v = 0; v < 2; ++v) {
            char name[48]; snprintf(name, sizeof name, "choose_%s_%u", entries[i].id, v);
            assert(lv_obj_get_user_data(named(name)) == &entries[i]);
        }
        wristflow_component_t original = wristflow_ui_shell_layout(shell)->pages[0].slots[0];
        wristflow_ui_shell_back(shell); advance();
        assert(!memcmp(&original, &wristflow_ui_shell_layout(shell)->pages[0].slots[0], sizeof original));
        click(named("slot_0"));
        assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_COMPONENT_PICKER);
        /* Compass is the 19th app: both choices are beyond the old 32 slots. */
        char name[48]; snprintf(name, sizeof name, "choose_compass_%u", variant);
        lv_obj_t *choice = named(name);
        lv_obj_scroll_to_view(choice, LV_ANIM_OFF); advance();
        click(choice);
        assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_COMPONENT_EDITOR);
        const wristflow_component_t *selected = &wristflow_ui_shell_layout(shell)->pages[0].slots[0];
        assert(!strcmp(selected->app_id, "compass") && selected->variant == variant);
        wristflow_ui_shell_destroy(shell); advance();
    }
    lv_deinit();
    puts("component picker: >32 choices, all sizes/variants, callback identity and repeated lifetime passed");
    return 0;
}
