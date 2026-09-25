#include "product_ui.h"
#include "app_registry.h"
#include "wristflow_ui.h"
#include <string.h>

static const char *const slots[] = {"slot_0", "slot_1", "slot_2", "slot_3"};

static lv_obj_t *named(lv_obj_t *root, const char *name)
{
    lv_obj_t *obj = lv_obj_find_by_name(root, name);
    LV_ASSERT(obj);
    return obj;
}

static wristflow_surface_t card_target(unsigned index, unsigned slot)
{
    const wristflow_card_page_t *page = wristflow_product_default_page(index);
    if (!page || slot >= page->count) return WRISTFLOW_SURFACE_COUNT;
    const wristflow_app_descriptor_t *app = wristflow_app_find(page->slots[slot].app_id);
    return app ? app->surface : WRISTFLOW_SURFACE_COUNT;
}

static void update_card(lv_obj_t *root, unsigned index, const wristflow_watch_snapshot_t *snapshot)
{
    const wristflow_card_page_t *page = wristflow_product_default_page(index);
    if (!page) return;
    for (unsigned i = 0; i < page->count; ++i) {
        const wristflow_component_t *component = &page->slots[i];
        const wristflow_app_descriptor_t *app = wristflow_app_find(component->app_id);
        wristflow_app_data_t data;
        if (!app || !(app->card_sizes & component->size) || !wristflow_app_read(app, snapshot, &data)) continue;
        lv_obj_t *card = named(root, slots[i]);
        lv_label_set_text(named(card, "caption_label"), app->title);
        lv_label_set_text(named(card, "value_label"), data.value);
        lv_obj_t *icon = lv_obj_find_by_name(card, "icon_label");
        if (icon) {
            lv_label_set_text(icon, app->icon);
            lv_obj_set_style_text_color(icon,
                component->variant ? lv_color_white() : lv_color_hex(app->color), 0);
        }
        lv_obj_set_style_bg_color(card, lv_color_hex(component->variant ? app->color : 0x191c20), 0);
        lv_obj_set_style_text_color(named(card, "caption_label"),
            component->variant ? lv_color_white() : lv_color_hex(0xa7adb5), 0);
        lv_obj_t *detail = lv_obj_find_by_name(card, "detail_label");
        if (detail) lv_label_set_text(detail, data.reason);
    }
}

wristflow_ui_shell_t *wristflow_product_ui_create(const wristflow_watch_snapshot_t *snapshot,
    const wristflow_settings_t *settings, wristflow_brightness_cb_t brightness, void *context)
{
    if (!wristflow_snapshot_valid(snapshot) || !wristflow_settings_valid(settings)) return NULL;
    wristflow_screen_factory_t cards[WRISTFLOW_PRODUCT_DEFAULT_PAGES];
    for (unsigned i = 0; i < WRISTFLOW_PRODUCT_DEFAULT_PAGES; ++i)
        cards[i] = wristflow_product_default_page(i)->create;
    const wristflow_ui_shell_config_t config = {
        .watchface = strcmp(settings->face_id, "simple") == 0
            ? &wristflow_simple_watchface : &wristflow_default_watchface,
        .cards = cards, .card_count = WRISTFLOW_PRODUCT_DEFAULT_PAGES, .controls = screen_control_center_create,
        .initial_snapshot = *snapshot, .enable_apps = true,
        .set_brightness = brightness, .platform_context = context,
        .initial_settings = settings, .update_card = update_card,
        .card_target = card_target, .product_apps = true
    };
    return wristflow_ui_shell_create(&config);
}
