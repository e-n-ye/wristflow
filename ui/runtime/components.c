#include "components.h"
#include "app_registry.h"
#include "wristflow_ui.h"
#include <string.h>
#include <stdio.h>

typedef struct {
    struct wristflow_components *owner;
    const wristflow_app_descriptor_t *app;
    uint8_t variant;
} choice_t;

struct wristflow_components {
    wristflow_ui_shell_t *shell;
    wristflow_layout_t layout;
    wristflow_layout_page_t draft_page;
    unsigned page, slot, insertion;
    bool draft, template_chosen, discard_home, deleting;
    bool dragged;
    lv_point_t press;
    lv_obj_t *screens[3], *confirmation, *failure;
    choice_t choices[32];
    wristflow_layout_request_cb_t request;
    wristflow_layout_status_cb_t status;
    void *context;
    uint32_t request_id;
    wristflow_save_state_t save_state;
    lv_timer_t *timer;
};

static const char *const slot_names[] = {"slot_0", "slot_1", "slot_2", "slot_3"};
static const char *const template_names[] = {"quarters", "top_half", "bottom_half", "full"};
static lv_obj_t *named(lv_obj_t *root, const char *name)
{
    lv_obj_t *obj = lv_obj_find_by_name(root, name);
    LV_ASSERT(obj);
    return obj;
}

static void touch(lv_event_t *event)
{
    wristflow_components_t *c = lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING) return;
    lv_indev_t *input = lv_indev_active(); if (!input) return;
    lv_point_t point; lv_indev_get_point(input, &point);
    if (code == LV_EVENT_PRESSED) { c->press = point; c->dragged = false; }
    else {
        int dx = point.x - c->press.x, dy = point.y - c->press.y;
        if (dx * dx + dy * dy > 100) c->dragged = true;
    }
}

bool wristflow_components_supported(const wristflow_layout_t *layout)
{
    if (!wristflow_layout_valid(layout)) return false;
    for (unsigned p = 0; p < layout->count; ++p)
        for (unsigned s = 0; s < wristflow_template_slots(layout->pages[p].template_id); ++s) {
            const wristflow_component_t *slot = &layout->pages[p].slots[s];
            const wristflow_app_descriptor_t *app = wristflow_app_find(slot->app_id);
            if (!app || !(app->card_sizes & slot->size)) return false;
        }
    return true;
}

static void paint(lv_obj_t *card, const wristflow_component_t *component,
                  const wristflow_watch_snapshot_t *snapshot)
{
    const wristflow_app_descriptor_t *app = wristflow_app_find(component->app_id);
    if (!app) return;
    wristflow_app_data_t data;
    if (!wristflow_app_read(app, snapshot, &data)) return;
    lv_label_set_text(named(card, "caption_label"), app->title);
    lv_label_set_text(named(card, "value_label"), data.value);
    lv_color_t fg = component->variant ? lv_color_white() : lv_color_hex(app->color);
    lv_obj_t *icon = lv_obj_find_by_name(card, "icon_label");
    if (icon) { lv_label_set_text(icon, app->icon); lv_obj_set_style_text_color(icon, fg, 0); }
    lv_obj_set_style_bg_color(card, lv_color_hex(component->variant ? app->color : 0x191c20), 0);
    lv_obj_set_style_text_color(named(card, "caption_label"),
        component->variant ? lv_color_white() : lv_color_hex(0xa7adb5), 0);
    lv_obj_t *detail = lv_obj_find_by_name(card, "detail_label");
    if (detail) {
        lv_label_set_text(detail, data.reason);
        lv_obj_set_style_text_color(detail, component->variant ? lv_color_white() : lv_color_hex(0xa7adb5), 0);
    }
    /* Activity's three missing metrics keep the same information in both colors. */
    if (!strcmp(app->id, "activity") && lv_obj_find_by_name(card, "activity_ring_0")) {
        for (unsigned i = 0; i < 3; ++i) {
            char name[32]; snprintf(name, sizeof name, "activity_ring_%u", i);
            lv_obj_set_style_arc_color(named(card, name),
                component->variant ? lv_color_hex(0xffffff) : lv_color_hex(i == 0 ? 0x593820 : i == 1 ? 0x4c491f : 0x183f50), LV_PART_MAIN);
        }
        lv_obj_set_style_text_color(named(card, "value_label"), fg, 0);
        lv_obj_set_style_text_color(named(card, "activity_value_1"), component->variant ? lv_color_white() : lv_color_hex(0xe2db48), 0);
        lv_obj_set_style_text_color(named(card, "activity_value_2"), component->variant ? lv_color_white() : lv_color_hex(0x38bdf8), 0);
    }
}

static lv_obj_t *make_card(lv_obj_t *parent, const wristflow_component_t *slot,
                          const wristflow_watch_snapshot_t *snapshot)
{
    lv_obj_t *card;
    if (slot->size == WRISTFLOW_CARD_QUARTER)
        card = card_container_create(parent, "", "--", "", lv_color_hex(0x191c20),
            lv_color_white(), lv_color_white(), lv_color_white());
    else if (slot->size == WRISTFLOW_CARD_FULL) card = product_metric_full_create(parent);
    else if (!strcmp(slot->app_id, "activity")) card = activity_summary_create(parent);
    else card = metric_half_create(parent, "", "--", "", "", lv_color_hex(0x191c20), lv_color_white(), lv_color_white());
    paint(card, slot, snapshot);
    return card;
}

static void place(lv_obj_t *card, wristflow_template_t type, unsigned slot)
{
    unsigned col = 0, row = 0;
    wristflow_card_size_t size = wristflow_template_size(type, slot);
    if (type == WRISTFLOW_LAYOUT_QUARTERS || type == WRISTFLOW_LAYOUT_BOTTOM_HALF) {
        col = slot % 2; row = slot / 2;
    } else if (type == WRISTFLOW_LAYOUT_TOP_HALF && slot) { col = slot - 1; row = 1; }
    lv_obj_set_grid_cell(card, LV_GRID_ALIGN_START, col, size == WRISTFLOW_CARD_QUARTER ? 1 : 2,
        LV_GRID_ALIGN_START, row, size == WRISTFLOW_CARD_FULL ? 2 : 1);
}

lv_obj_t *wristflow_components_page(wristflow_components_t *c, unsigned index)
{
    lv_obj_t *root = screen_component_page_create();
    const wristflow_layout_page_t *page = &c->layout.pages[index];
    for (unsigned s = 0; s < wristflow_template_slots(page->template_id); ++s) {
        lv_obj_t *card = make_card(named(root, "tile_slots"), &page->slots[s], wristflow_ui_shell_snapshot(c->shell));
        lv_obj_set_name(card, slot_names[s]); place(card, page->template_id, s);
    }
    return root;
}

void wristflow_components_update(wristflow_components_t *c, lv_obj_t *root,
    unsigned index, const wristflow_watch_snapshot_t *snapshot)
{
    const wristflow_layout_page_t *page = &c->layout.pages[index];
    for (unsigned s = 0; s < wristflow_template_slots(page->template_id); ++s)
        paint(named(root, slot_names[s]), &page->slots[s], snapshot);
}

wristflow_surface_t wristflow_components_target(wristflow_components_t *c, unsigned page, unsigned slot)
{
    if (slot >= wristflow_template_slots(c->layout.pages[page].template_id)) return WRISTFLOW_SURFACE_COUNT;
    return wristflow_app_find(c->layout.pages[page].slots[slot].app_id)->surface;
}

static void save_status(wristflow_components_t *c)
{
    const char *message = c->save_state == WRISTFLOW_SAVE_PENDING ? "保存中" :
        c->save_state == WRISTFLOW_SAVE_DONE ? "已保存" :
        c->save_state == WRISTFLOW_SAVE_FAILED ? "保存失败，稍后重试" : "";
    if (c->screens[0]) lv_label_set_text(named(c->screens[0], "edit_save"), c->draft ? "" : message);
    lv_obj_set_flag(c->failure, LV_OBJ_FLAG_HIDDEN, c->save_state != WRISTFLOW_SAVE_FAILED);
}

static void poll(lv_timer_t *timer)
{
    wristflow_components_t *c = lv_timer_get_user_data(timer);
    if (c->request_id && c->status) {
        wristflow_save_state_t state = c->status(c->request_id, c->context);
        if (state != c->save_state) { c->save_state = state; save_status(c); }
        if (state == WRISTFLOW_SAVE_DONE) lv_timer_pause(timer);
    }
}

static void persist(wristflow_components_t *c)
{
    c->request_id = c->request ? c->request(&c->layout, c->context) : 0;
    c->save_state = c->request_id ? WRISTFLOW_SAVE_PENDING : WRISTFLOW_SAVE_FAILED;
    if (c->request_id) { lv_timer_reset(c->timer); lv_timer_resume(c->timer); }
    save_status(c);
}

static void render_editor(wristflow_components_t *c);
static void choose_slot(lv_event_t *event);

static void select_component(lv_event_t *event)
{
    choice_t *choice = lv_event_get_user_data(event);
    wristflow_components_t *c = choice->owner;
    if (c->dragged) return;
    if (c->draft) {
        wristflow_component_t *slot = &c->draft_page.slots[c->slot];
        strcpy(slot->app_id, choice->app->id); slot->variant = choice->variant;
        slot->size = wristflow_template_size(c->draft_page.template_id, c->slot);
    } else {
        if (!wristflow_layout_replace(&c->layout, c->page, c->slot, choice->app->id, choice->variant)) return;
        persist(c);
        wristflow_ui_shell_rebuild_components(c->shell, c->page + 1);
    }
    render_editor(c);
    wristflow_ui_shell_back(c->shell);
}

static void populate_picker(wristflow_components_t *c, lv_obj_t *root)
{
    lv_obj_t *list = named(root, "component_choices");
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    wristflow_layout_page_t *page = c->draft ? &c->draft_page : &c->layout.pages[c->page];
    wristflow_card_size_t size = wristflow_template_size(page->template_id, c->slot);
    int y = 0; unsigned count = 0;
    for (size_t i = 0; i < wristflow_app_count(); ++i) {
        const wristflow_app_descriptor_t *app = wristflow_app_at(i);
        if (!(app->card_sizes & size)) continue;
        lv_obj_t *heading = component_group_create(list, app->title);
        lv_obj_set_y(heading, y); y += 36;
        for (unsigned variant = 0; variant < 2; ++variant) {
            wristflow_component_t slot = {.size = size, .variant = (uint8_t)variant};
            strcpy(slot.app_id, app->id);
            lv_obj_t *card = make_card(list, &slot, wristflow_ui_shell_snapshot(c->shell));
            lv_obj_set_pos(card, size == WRISTFLOW_CARD_QUARTER ? variant * 177 : 0, y);
            char name[40]; snprintf(name, sizeof name, "choose_%s_%u", app->id, variant);
            lv_obj_set_name(card, name);
            lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
            LV_ASSERT(count < sizeof c->choices / sizeof c->choices[0]);
            c->choices[count] = (choice_t){c, app, (uint8_t)variant};
            lv_obj_add_event_cb(card, select_component, LV_EVENT_SHORT_CLICKED, &c->choices[count++]);
            if (size != WRISTFLOW_CARD_QUARTER) y += (size == WRISTFLOW_CARD_FULL ? 362 : 174) + 14;
        }
        if (size == WRISTFLOW_CARD_QUARTER) y += 188;
        y += 16;
    }
}

static void choose_slot(lv_event_t *event)
{
    wristflow_components_t *c = lv_event_get_user_data(event);
    if (c->dragged) return;
    lv_obj_t *card = lv_event_get_current_target_obj(event);
    for (unsigned s = 0; s < 4; ++s) if (!strcmp(lv_obj_get_name(card), slot_names[s])) c->slot = s;
    wristflow_ui_shell_open(c->shell, WRISTFLOW_SURFACE_COMPONENT_PICKER);
}

static void render_editor(wristflow_components_t *c)
{
    lv_obj_t *root = c->screens[0];
    if (!root) return;
    lv_obj_t *canvas = named(root, "edit_canvas"); lv_obj_clean(canvas);
    const wristflow_layout_page_t *page = c->draft ? &c->draft_page : &c->layout.pages[c->page];
    for (unsigned s = 0; s < wristflow_template_slots(page->template_id); ++s) {
        lv_obj_t *card = editor_card_create(canvas);
        lv_obj_add_flag(card, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_name(card, slot_names[s]);
        wristflow_card_size_t size = wristflow_template_size(page->template_id, s);
        lv_obj_set_size(card, size == WRISTFLOW_CARD_QUARTER ? 126 : 264, size == WRISTFLOW_CARD_FULL ? 266 : 126);
        place(card, page->template_id, s);
        if (page->slots[s].app_id[0]) paint(card, &page->slots[s], wristflow_ui_shell_snapshot(c->shell));
        else { lv_label_set_text(named(card, "icon_label"), "+"); lv_obj_set_style_text_font(named(card, "icon_label"), body_20, 0); }
        lv_obj_add_event_cb(card, choose_slot, LV_EVENT_SHORT_CLICKED, c);
    }
    bool can_add = !c->draft && c->layout.count < WRISTFLOW_LAYOUT_MAX_PAGES;
    lv_obj_set_state(named(root, "edit_left"), LV_STATE_DISABLED, !can_add);
    lv_obj_set_state(named(root, "edit_right"), LV_STATE_DISABLED, !can_add);
    lv_label_set_text(named(root, "edit_action_icon"), c->draft ? "\xef\x80\x8c" : "\xef\x87\xb8");
    lv_obj_set_state(named(root, "edit_action"), LV_STATE_DISABLED,
        c->draft ? !wristflow_layout_page_complete(page) : c->layout.count == 1);
    save_status(c);
}

static void hide_confirmation(wristflow_components_t *c)
{
    if (c->confirmation) lv_obj_add_flag(c->confirmation, LV_OBJ_FLAG_HIDDEN);
}

static void confirmed(lv_event_t *event)
{
    wristflow_components_t *c = lv_event_get_user_data(event);
    if (c->dragged) return;
    bool accept = lv_event_get_current_target_obj(event) == named(c->confirmation, "confirm_accept");
    hide_confirmation(c);
    if (!accept) return;
    if (c->deleting) {
        if (!wristflow_layout_delete(&c->layout, c->page)) return;
        c->page = c->page ? c->page - 1 : 0;
        persist(c); wristflow_ui_shell_rebuild_components(c->shell, c->page + 1);
    }
    c->draft = false;
    wristflow_ui_shell_finish_edit(c->shell, c->discard_home ? 0 : c->page + 1);
}

static void confirm(wristflow_components_t *c, bool deleting, bool home)
{
    c->deleting = deleting; c->discard_home = home;
    /* A top-layer modal also covers a selector when KEY1 requests home. */
    if (!c->confirmation) {
        c->confirmation = component_confirm_create(lv_layer_top());
        lv_obj_add_flag(named(c->confirmation, "confirm_accept"), LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_flag(named(c->confirmation, "confirm_cancel"), LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_event_cb(c->confirmation, touch, LV_EVENT_ALL, c);
        lv_obj_add_event_cb(named(c->confirmation, "confirm_accept"), confirmed, LV_EVENT_SHORT_CLICKED, c);
        lv_obj_add_event_cb(named(c->confirmation, "confirm_cancel"), confirmed, LV_EVENT_SHORT_CLICKED, c);
    }
    lv_label_set_text(named(c->confirmation, "confirm_text"), deleting ? "是否删除当前小组件？" :
        "组件编辑未完成，退出将不再保存，确认退出？");
    lv_obj_remove_flag(c->confirmation, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(c->confirmation);
}

static void add_page(lv_event_t *event)
{
    wristflow_components_t *c = lv_event_get_user_data(event);
    if (c->dragged || c->draft || c->layout.count >= WRISTFLOW_LAYOUT_MAX_PAGES) return;
    c->insertion = c->page + (lv_event_get_current_target_obj(event) == named(c->screens[0], "edit_right"));
    c->draft = true; c->template_chosen = false;
    memset(&c->draft_page, 0, sizeof c->draft_page);
    wristflow_ui_shell_open(c->shell, WRISTFLOW_SURFACE_COMPONENT_TEMPLATES);
}

static void choose_template(lv_event_t *event)
{
    wristflow_components_t *c = lv_event_get_user_data(event);
    if (c->dragged) return;
    for (unsigned i = 0; i < WRISTFLOW_LAYOUT_COUNT; ++i)
        if (lv_event_get_current_target_obj(event) == named(c->screens[2], template_names[i]))
            c->draft_page.template_id = (wristflow_template_t)i;
    c->template_chosen = true;
    render_editor(c);
    wristflow_ui_shell_back(c->shell);
}

static void action(lv_event_t *event)
{
    wristflow_components_t *c = lv_event_get_user_data(event);
    if (c->dragged) return;
    if (!c->draft) { if (c->layout.count > 1) confirm(c, true, false); return; }
    if (!wristflow_layout_insert(&c->layout, c->insertion, &c->draft_page)) return;
    c->draft = false; c->page = c->insertion;
    persist(c); wristflow_ui_shell_rebuild_components(c->shell, c->page + 1);
    wristflow_ui_shell_finish_edit(c->shell, c->page + 1);
}

static void blank(lv_event_t *event)
{
    wristflow_components_t *c = lv_event_get_user_data(event);
    if (c->dragged) return;
    lv_obj_t *target = lv_event_get_target_obj(event);
    if (target == c->screens[0] || target == named(c->screens[0], "edit_canvas"))
        wristflow_ui_shell_back(c->shell);
}

lv_obj_t *wristflow_components_screen(wristflow_components_t *c, wristflow_surface_t surface, bool *created)
{
    *created = false;
    if (!c || surface < WRISTFLOW_SURFACE_COMPONENT_EDITOR || surface > WRISTFLOW_SURFACE_COMPONENT_TEMPLATES) return NULL;
    unsigned index = surface - WRISTFLOW_SURFACE_COMPONENT_EDITOR;
    if (c->screens[index]) return c->screens[index];
    lv_obj_t *root = index == 0 ? screen_component_editor_create() :
        index == 1 ? screen_component_picker_create() : screen_component_templates_create();
    c->screens[index] = root; *created = true;
    lv_obj_add_event_cb(root, touch, LV_EVENT_ALL, c);
    if (!index) {
        lv_obj_set_style_opa(named(root, "edit_left"), LV_OPA_30, LV_STATE_DISABLED);
        lv_obj_set_style_opa(named(root, "edit_right"), LV_OPA_30, LV_STATE_DISABLED);
        lv_obj_set_style_opa(named(root, "edit_action"), LV_OPA_30, LV_STATE_DISABLED);
        render_editor(c);
        lv_obj_add_event_cb(named(root, "edit_left"), add_page, LV_EVENT_SHORT_CLICKED, c);
        lv_obj_add_event_cb(named(root, "edit_right"), add_page, LV_EVENT_SHORT_CLICKED, c);
        lv_obj_add_event_cb(named(root, "edit_action"), action, LV_EVENT_SHORT_CLICKED, c);
        lv_obj_add_event_cb(root, blank, LV_EVENT_SHORT_CLICKED, c);
    } else if (index == 1) populate_picker(c, root);
    else for (unsigned i = 0; i < WRISTFLOW_LAYOUT_COUNT; ++i) {
        lv_obj_t *option = named(root, template_names[i]);
        lv_obj_add_flag(option, LV_OBJ_FLAG_CLICKABLE);
        /* Diagram blocks must not steal taps from the enclosing option. */
        for (unsigned j = 0; j < lv_obj_get_child_count(option); ++j)
            lv_obj_remove_flag(lv_obj_get_child(option, j), LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(option, choose_template, LV_EVENT_SHORT_CLICKED, c);
    }
    return root;
}

bool wristflow_components_enter(wristflow_components_t *c, unsigned page)
{
    if (!c || page >= c->layout.count) return false;
    c->page = page; c->draft = false;
    return wristflow_ui_shell_open(c->shell, WRISTFLOW_SURFACE_COMPONENT_EDITOR);
}

bool wristflow_components_back(wristflow_components_t *c, bool home)
{
    if (!c) return false;
    wristflow_surface_t surface = wristflow_ui_shell_navigation(c->shell)->surface;
    if (surface < WRISTFLOW_SURFACE_COMPONENT_EDITOR) return false;
    if (c->confirmation && !lv_obj_has_flag(c->confirmation, LV_OBJ_FLAG_HIDDEN)) {
        hide_confirmation(c); return true;
    }
    if (c->draft && (home || surface == WRISTFLOW_SURFACE_COMPONENT_EDITOR ||
        (surface == WRISTFLOW_SURFACE_COMPONENT_TEMPLATES && !c->template_chosen))) {
        confirm(c, false, home); return true;
    }
    if (home || surface == WRISTFLOW_SURFACE_COMPONENT_EDITOR)
        return wristflow_ui_shell_finish_edit(c->shell, home ? 0 : c->page + 1);
    return false;
}

void wristflow_components_collect(wristflow_components_t *c, const wristflow_navigation_t *navigation)
{
    if (!c) return;
    for (unsigned i = 0; i < 3; ++i)
        if (c->screens[i] && !wristflow_navigation_contains(navigation, WRISTFLOW_SURFACE_COMPONENT_EDITOR + i)) {
            lv_obj_delete(c->screens[i]); c->screens[i] = NULL;
        }
}

wristflow_components_t *wristflow_components_create(wristflow_ui_shell_t *shell,
    const wristflow_layout_t *initial, wristflow_layout_request_cb_t request,
    wristflow_layout_status_cb_t status, void *context)
{
    wristflow_components_t *c = lv_malloc_zeroed(sizeof(*c)); LV_ASSERT_MALLOC(c);
    c->shell = shell;
    c->layout = wristflow_components_supported(initial) ? *initial : wristflow_layout_default();
    c->request = request; c->status = status; c->context = context;
    c->failure = layout_save_status_create(lv_layer_top());
    c->timer = lv_timer_create(poll, 200, c);
    lv_timer_pause(c->timer);
    return c;
}

const wristflow_layout_t *wristflow_components_layout(const wristflow_components_t *c)
{ return c ? &c->layout : NULL; }

void wristflow_components_destroy(wristflow_components_t *c)
{
    if (!c) return;
    lv_timer_delete(c->timer);
    for (unsigned i = 0; i < 3; ++i) if (c->screens[i]) lv_obj_delete(c->screens[i]);
    if (c->confirmation) lv_obj_delete(c->confirmation);
    lv_obj_delete(c->failure); lv_free(c);
}
