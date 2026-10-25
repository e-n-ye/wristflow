#include "weather_screen.h"
#include "mount_screen.h"
#include "wristflow_ui.h"
#include <stdio.h>
#include <string.h>

#define WEATHER_WIDTH 390
#define WEATHER_HEIGHT 450
#define WEATHER_PAGE_HEIGHT WEATHER_HEIGHT
#define WEATHER_PAGE_COUNT 5U
#define WEATHER_INNER_WIDTH WEATHER_WIDTH
#define WEATHER_FORECAST_TOP 76
#define WEATHER_FORECAST_HEIGHT 320

typedef struct {
    lv_obj_t *root;
    lv_obj_t *outer;
    lv_obj_t *hourly;
    lv_obj_t *daily;
    lv_obj_t *state_panel;
    lv_obj_t *state_label;
    lv_obj_t *state_retry;
    lv_obj_t *contents[WEATHER_PAGE_COUNT];
    unsigned outer_page;
    unsigned hourly_page;
    unsigned daily_page;
    unsigned hourly_start_page;
    unsigned daily_start_page;
    bool hourly_gesture;
    bool daily_gesture;
    bool hourly_settling;
    bool daily_settling;
    wristflow_weather_state_t state;
    wristflow_weather_theme_t theme;
    lv_color_t background;
    wristflow_weather_data_t data;
} weather_view_t;

static lv_color_t weather_background(wristflow_weather_theme_t theme)
{
    return theme == WRISTFLOW_WEATHER_THEME_SUNNY ? WEATHER_BLUE : WEATHER_CLOUDY;
}

static lv_color_t weather_dot_dim(wristflow_weather_theme_t theme)
{
    return lv_color_mix(FG_PRIMARY, weather_background(theme), 48);
}

static lv_obj_t *named(lv_obj_t *root, const char *name)
{
    lv_obj_t *obj = lv_obj_find_by_name(root, name);
    LV_ASSERT(obj);
    return obj;
}
static void update_dots(lv_obj_t *host, unsigned count, unsigned active, lv_color_t dim)
{
    if (!host) return;
    for (unsigned i = 0; i < count; ++i) {
        lv_obj_t *dot = lv_obj_get_child(host, i);
        if (dot)
            lv_obj_set_style_bg_color(dot, i == active ? FG_PRIMARY : dim, 0);
    }
}

static void configure_scroll(lv_obj_t *obj, lv_dir_t direction, lv_scroll_snap_t snap,
                             bool chain_vertical, bool momentum)
{
    lv_obj_set_scroll_dir(obj, direction);
    if (direction == LV_DIR_HOR) lv_obj_set_scroll_snap_x(obj, snap);
    else lv_obj_set_scroll_snap_y(obj, snap);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ONE);
    if (momentum) lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    else lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_CHAIN);
    if (chain_vertical) lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    lv_obj_set_style_anim_duration(obj, 220, 0);
}

static void set_text(lv_obj_t *root, const char *name, const char *text)
{
    lv_obj_t *obj = lv_obj_find_by_name(root, name);
    if (obj) lv_label_set_text(obj, text ? text : "");
}

static unsigned page_from_scroll(lv_obj_t *obj, lv_coord_t page_size, unsigned max_page)
{
    lv_coord_t offset = lv_obj_get_scroll_y(obj);
    if (lv_obj_get_scroll_dir(obj) == LV_DIR_HOR) offset = lv_obj_get_scroll_x(obj);
    if (offset < 0) offset = -offset;
    unsigned page = page_size > 0 ? (unsigned)((offset + page_size / 2) / page_size) : 0;
    return page > max_page ? max_page : page;
}

static unsigned horizontal_page_for_gesture(unsigned start, unsigned max_page, lv_coord_t offset)
{
    lv_coord_t base = (lv_coord_t)start * WEATHER_INNER_WIDTH;
    if (offset > base + WEATHER_INNER_WIDTH / 3 && start < max_page) return start + 1U;
    if (offset + WEATHER_INNER_WIDTH / 3 < base && start > 0U) return start - 1U;
    return start;
}

static const char *weather_icon(const char *condition)
{
    if (!condition[0]) return "";
    if (strcmp(condition, "sun") == 0) return "\xef\x86\x85";
    /* The generated icon font contains tint (F043), but not the newer cloud-rain glyph. */
    if (strcmp(condition, "rain") == 0) return "\xef\x81\x83";
    return "\xef\x83\x82";
}

static void render_ready(weather_view_t *view)
{
    char text[32];
    if (view->data.temperature_valid) snprintf(text, sizeof text, "%d°", view->data.temperature);
    else snprintf(text, sizeof text, "--");
    set_text(view->root, "weather_city", view->data.city[0] ? view->data.city : "--");
    const char *updated = view->data.message[0] ? view->data.message : view->data.updated;
    if (view->data.expired) {
        updated = view->data.request_state == WRISTFLOW_WEATHER_REQUEST_PENDING ? "已过期 · 同步中" :
            view->data.request_state == WRISTFLOW_WEATHER_REQUEST_FAILED ? "已过期 · 更新失败" :
            view->data.request_state == WRISTFLOW_WEATHER_REQUEST_TIMEOUT ? "已过期 · 同步超时" : view->data.updated;
    }
    set_text(view->root, "weather_update", updated);
    set_text(view->root, "weather_temp", text);
    set_text(view->root, "weather_condition", view->data.condition[0] ? view->data.condition : "--");
    if (view->data.range_valid) snprintf(text, sizeof text, "%d°/%d°", view->data.high, view->data.low);
    else snprintf(text, sizeof text, "--/--");
    set_text(view->root, "weather_range", text);
    set_text(view->root, "weather_aqi", view->data.aqi[0] ? view->data.aqi : "--");
    set_text(view->root, "weather_sunrise", view->data.sunrise[0] ? view->data.sunrise : "--:--");
    set_text(view->root, "weather_sunset", view->data.sunset[0] ? view->data.sunset : "--:--");
    lv_obj_set_flag(lv_obj_find_by_name(view->root, "weather_sun_position"), LV_OBJ_FLAG_HIDDEN,
                    !view->data.sun_position_valid);
    for (unsigned i = 0; i < WRISTFLOW_WEATHER_INDEX_COUNT; ++i) {
        char name[40];
        snprintf(name, sizeof name, "weather_index_%u_value", i);
        const char *value = view->data.indices[i].value;
        set_text(view->root, name, value[0] ? value : "--");
        lv_obj_set_style_text_font(lv_obj_find_by_name(view->root, name),
                                  strlen(value) > 2 ? notification_22 : metric_56, 0);
        lv_label_set_long_mode(lv_obj_find_by_name(view->root, name), LV_LABEL_LONG_MODE_WRAP);
        snprintf(name, sizeof name, "weather_index_%u_label", i);
        set_text(view->root, name, view->data.indices[i].label);
    }
    for (unsigned i = 0; i < WRISTFLOW_WEATHER_HOURLY_COUNT; ++i) {
        char name[24];
        snprintf(name, sizeof name, "weather_hour_%u", i);
        lv_obj_t *host = lv_obj_find_by_name(view->root, name);
        const wristflow_weather_hour_t *hour = &view->data.hourly[i];
        if (hour->valid) snprintf(text, sizeof text, "%d°", hour->temperature);
        else snprintf(text, sizeof text, "--");
        set_text(host, "hour_temp", text);
        set_text(host, "hour_time", hour->valid ? hour->time : "--:--");
        set_text(host, "hour_icon", hour->valid ? weather_icon(hour->icon) : "");
        set_text(host, "hour_wind", hour->valid ? hour->wind : "--");
        set_text(host, "hour_air", hour->valid ? hour->air : "--");
    }
    for (unsigned i = 0; i < WRISTFLOW_WEATHER_DAILY_COUNT; ++i) {
        char name[24];
        snprintf(name, sizeof name, "weather_day_%u", i);
        lv_obj_t *host = lv_obj_find_by_name(view->root, name);
        const wristflow_weather_day_t *day = &view->data.daily[i];
        if (day->valid) snprintf(text, sizeof text, "%d°/%d°", day->high, day->low);
        else snprintf(text, sizeof text, "--/--");
        set_text(host, "day_range", text);
        set_text(host, "day_name", day->valid ? day->day : "--");
        set_text(host, "day_icon", day->valid ? weather_icon(day->icon) : "");
    }
}

static void render_state(weather_view_t *view)
{
    view->theme = view->data.theme;
    view->background = weather_background(view->theme);
    lv_obj_set_style_bg_color(view->root, view->background, 0);
    lv_obj_set_style_bg_color(view->state_panel, view->background, 0);
    for (unsigned i = 0; i < WEATHER_PAGE_COUNT; ++i) {
        lv_obj_set_style_bg_color(lv_obj_get_child(view->outer, i), view->background, 0);
        lv_obj_set_style_bg_color(view->contents[i], view->background, 0);
    }
    update_dots(lv_obj_find_by_name(view->root, "weather_hourly_dots"), 6,
                view->hourly_page, weather_dot_dim(view->theme));
    update_dots(lv_obj_find_by_name(view->root, "weather_daily_dots"), 2,
                view->daily_page, weather_dot_dim(view->theme));
    bool ready = view->state == WRISTFLOW_WEATHER_READY || view->state == WRISTFLOW_WEATHER_STALE;
    lv_obj_set_flag(view->outer, LV_OBJ_FLAG_HIDDEN, !ready);
    lv_obj_set_flag(view->state_panel, LV_OBJ_FLAG_HIDDEN, ready);
    lv_obj_t *retry_parent = ready ? lv_obj_find_by_name(view->root, "weather_page_current") : view->state_panel;
    if (lv_obj_get_parent(view->state_retry) != retry_parent) lv_obj_set_parent(view->state_retry, retry_parent);
    lv_obj_set_pos(view->state_retry, 134, ready ? 310 : 276);
    lv_obj_set_flag(view->state_retry, LV_OBJ_FLAG_HIDDEN,
        view->state == WRISTFLOW_WEATHER_LOADING || view->data.request_state == WRISTFLOW_WEATHER_REQUEST_PENDING ||
        (ready && !view->data.expired && view->data.request_state == WRISTFLOW_WEATHER_REQUEST_IDLE));
    lv_label_set_text(view->state_label, view->data.message);
    render_ready(view);
}

static weather_view_t *view_from(lv_obj_t *root)
{
    return root ? (weather_view_t *)lv_obj_get_user_data(root) : NULL;
}

static void retry(lv_event_t *event)
{
    weather_view_t *view = lv_event_get_user_data(event);
    if (!view) return;
    wristflow_weather_request_sync();
    wristflow_weather_screen_refresh(view->root);
}


static void scroll_begin(lv_event_t *event)
{
    weather_view_t *view = lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target_obj(event);
    if (!view || lv_indev_active() == NULL) return;
    if (target == view->hourly && !view->hourly_settling) {
        view->hourly_start_page = page_from_scroll(target, WEATHER_INNER_WIDTH, 5);
        view->hourly_gesture = true;
    } else if (target == view->daily && !view->daily_settling) {
        view->daily_start_page = page_from_scroll(target, WEATHER_INNER_WIDTH, 1);
        view->daily_gesture = true;
    }
}

static void scroll_end(lv_event_t *event)
{
    weather_view_t *view = lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target_obj(event);
    bool active_inner_gesture = view && (target == view->hourly || target == view->daily) &&
                                lv_indev_active() != NULL;
    if (!view || (lv_obj_is_scrolling(target) && !active_inner_gesture)) {
        return;
    }
    if (target == view->outer) {
        view->outer_page = page_from_scroll(target, WEATHER_PAGE_HEIGHT, WEATHER_PAGE_COUNT - 1U);
    } else if (target == view->hourly) {
        if (view->hourly_gesture && !view->hourly_settling) {
            unsigned page = horizontal_page_for_gesture(
                view->hourly_start_page, 5, lv_obj_get_scroll_x(target));
            view->hourly_gesture = false;
            view->hourly_settling = true;
            lv_obj_scroll_to_x(target, (lv_coord_t)page * WEATHER_INNER_WIDTH, LV_ANIM_OFF);
            view->hourly_settling = false;
        }
        view->hourly_page = page_from_scroll(target, WEATHER_INNER_WIDTH, 5);
        update_dots(lv_obj_find_by_name(view->root, "weather_hourly_dots"), 6,
                    view->hourly_page, weather_dot_dim(view->theme));
    } else if (target == view->daily) {
        if (view->daily_gesture && !view->daily_settling) {
            unsigned page = horizontal_page_for_gesture(
                view->daily_start_page, 1, lv_obj_get_scroll_x(target));
            view->daily_gesture = false;
            view->daily_settling = true;
            lv_obj_scroll_to_x(target, (lv_coord_t)page * WEATHER_INNER_WIDTH, LV_ANIM_OFF);
            view->daily_settling = false;
        }
        view->daily_page = page_from_scroll(target, WEATHER_INNER_WIDTH, 1);
        update_dots(lv_obj_find_by_name(view->root, "weather_daily_dots"), 2,
                    view->daily_page, weather_dot_dim(view->theme));
    }
}

static void deleted(lv_event_t *event)
{
    lv_obj_t *root = lv_event_get_target_obj(event);
    weather_view_t *view = view_from(root);
    if (view) {
        lv_obj_set_user_data(root, NULL);
        lv_free(view);
    }
}

static void attach_pages(weather_view_t *view)
{
    static const char *const names[] = {"current", "hourly", "daily", "indices", "sun"};
    static lv_obj_t *(*const factories[])(void) = {screen_weather_current_create,
        screen_weather_hourly_create, screen_weather_daily_create,
        screen_weather_indices_create, screen_weather_sun_create};
    for (unsigned i = 0; i < WEATHER_PAGE_COUNT; ++i) {
        char name[48]; snprintf(name, sizeof name, "weather_page_%s", names[i]);
        view->contents[i] = wristflow_mount_screen(named(view->root, name), factories[i]());
        snprintf(name, sizeof name, "weather_content_%s", names[i]);
        lv_obj_set_name(view->contents[i], name);
    }
    wristflow_weather_screen_set_time(view->root, "--:--");
    view->hourly = named(view->root, "weather_hourly_pager");
    view->daily = named(view->root, "weather_daily_pager");
    /* XML owns repeated geometry; only stable data identities are assigned here. */
    for (unsigned i = 0; i < WRISTFLOW_WEATHER_HOURLY_COUNT; ++i) {
        char name[40]; snprintf(name, sizeof name, "forecast_page_%u", i / 4);
        lv_obj_t *page = named(view->hourly, name);
        snprintf(name, sizeof name, "column_%u", i % 4);
        lv_obj_t *column = named(page, name);
        snprintf(name, sizeof name, "weather_hour_%u", i);
        lv_obj_set_name(column, name);
    }
    for (unsigned i = 0; i < WRISTFLOW_WEATHER_DAILY_COUNT; ++i) {
        char name[40]; snprintf(name, sizeof name, "forecast_page_%u", i / 5);
        lv_obj_t *page = named(view->daily, name);
        snprintf(name, sizeof name, "row_%u", i % 5);
        lv_obj_t *row = named(page, name);
        snprintf(name, sizeof name, "weather_day_%u", i);
        lv_obj_set_name(row, name);
    }
    lv_obj_t *inner[] = {view->hourly, view->daily};
    for (unsigned i = 0; i < 2; ++i) {
        configure_scroll(inner[i], LV_DIR_HOR, LV_SCROLL_SNAP_NONE, true, false);
        lv_obj_add_event_cb(inner[i], scroll_begin, LV_EVENT_SCROLL_BEGIN, view);
        lv_obj_add_event_cb(inner[i], scroll_end, LV_EVENT_SCROLL_END, view);
    }
}

lv_obj_t *wristflow_weather_screen_create_with_data(const wristflow_weather_data_t *data)
{
    lv_obj_t *root = screen_weather_host_create();
    lv_obj_set_name(root, "screen_weather");
    lv_color_t background = weather_background(data ? data->theme : WRISTFLOW_WEATHER_THEME_CLOUDY);

    weather_view_t *view = lv_malloc_zeroed(sizeof *view);
    LV_ASSERT_MALLOC(view);
    view->root = root;
    view->theme = data ? data->theme : WRISTFLOW_WEATHER_THEME_CLOUDY;
    view->background = background;
    view->state = data ? WRISTFLOW_WEATHER_READY : WRISTFLOW_WEATHER_EMPTY;
    if (data) view->data = *data;
    else snprintf(view->data.message, sizeof view->data.message, "%s",
                  wristflow_weather_state_message(view->state));
    lv_obj_set_user_data(root, view);
    lv_obj_add_event_cb(root, deleted, LV_EVENT_DELETE, NULL);

    view->outer = named(root, "weather_pager");
    attach_pages(view);
    configure_scroll(view->outer, LV_DIR_VER, LV_SCROLL_SNAP_START, false, true);
    lv_obj_add_event_cb(view->outer, scroll_end, LV_EVENT_SCROLL_END, view);

    view->state_panel = named(root, "weather_state_panel");
    view->state_label = named(root, "weather_state_label");
    view->state_retry = named(root, "weather_retry");
    lv_obj_add_event_cb(view->state_retry, retry, LV_EVENT_SHORT_CLICKED, view);
    render_state(view);
    return root;
}

void wristflow_weather_screen_refresh(lv_obj_t *screen)
{
    weather_view_t *view = view_from(screen);
    if (!view) return;
    wristflow_weather_data_t data;
    wristflow_weather_state_t state;
    wristflow_weather_get_current(&data, &state);
    if (state == view->state && !memcmp(&data, &view->data, sizeof data)) return;
    view->data = data;
    view->state = state;
    render_state(view);
}

void wristflow_weather_screen_set_time(lv_obj_t *screen, const char *time_str)
{
    if (!screen || !time_str) return;
    set_text(screen, "weather_hourly_clock", time_str);
    set_text(screen, "weather_daily_clock", time_str);
    set_text(screen, "weather_indices_clock", time_str);
    set_text(screen, "weather_sun_clock", time_str);
}

lv_obj_t *screen_weather_create(void)
{
    wristflow_weather_data_t data;
    bool available = wristflow_weather_get_current(&data, NULL);
    if (!available || data.expired) wristflow_weather_request_sync();
    lv_obj_t *screen = wristflow_weather_screen_create_with_data(available ? &data : NULL);
    wristflow_weather_screen_refresh(screen);
    return screen;
}

bool wristflow_weather_screen_is_horizontal(lv_obj_t *screen)
{
    weather_view_t *view = view_from(screen);
    if (!view || (view->state != WRISTFLOW_WEATHER_READY && view->state != WRISTFLOW_WEATHER_STALE)) return false;
    unsigned page = page_from_scroll(view->outer, WEATHER_PAGE_HEIGHT, WEATHER_PAGE_COUNT - 1U);
    view->outer_page = page;
    return page == 1U || page == 2U;
}

bool wristflow_weather_screen_edge_back_allowed(lv_obj_t *screen, lv_point_t origin)
{
    if (!wristflow_weather_screen_is_horizontal(screen)) return true;
    /* Forecast content owns middle-band horizontal drags; header and page dots retain edge Back. */
    return origin.y < WEATHER_FORECAST_TOP || origin.y >= 410;
}
