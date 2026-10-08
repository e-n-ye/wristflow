#include "weather_screen.h"
#include "mount_screen.h"
#include "wristflow_ui.h"
#include "screens/screen_weather_sun_gen.h"
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

static lv_obj_t *label(lv_obj_t *parent, const char *name, int x, int y, int width,
                       int height, const char *text, const lv_font_t *font, lv_color_t color,
                       lv_text_align_t align)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_name(obj, name);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);
    lv_label_set_text(obj, text ? text : "");
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, color, 0);
    lv_obj_set_style_text_align(obj, align, 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_MODE_CLIP);
    return obj;
}

static lv_obj_t *panel(lv_obj_t *parent, const char *name, int x, int y, int width, int height,
                       lv_color_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    /* Clear defaults first; geometry is stored in the local style and must survive. */
    lv_obj_remove_style_all(obj);
    lv_obj_set_name(obj, name);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static void dots(lv_obj_t *parent, const char *name, unsigned count, unsigned active, int y,
                 lv_color_t background, lv_color_t dim)
{
    lv_obj_t *host = panel(parent, name, 0, y, WEATHER_WIDTH, 10, background);
    lv_obj_set_style_bg_opa(host, LV_OPA_TRANSP, 0);
    int left = (WEATHER_WIDTH - ((int)count * 6 + ((int)count - 1) * 12)) / 2;
    for (unsigned i = 0; i < count; ++i) {
        lv_obj_t *dot = lv_obj_create(host);
        char dot_name[48];
        snprintf(dot_name, sizeof dot_name, "%s_%u", name, i);
        lv_obj_remove_style_all(dot);
        lv_obj_set_name(dot, dot_name);
        lv_obj_set_pos(dot, left + (int)i * 18, 2);
        lv_obj_set_size(dot, 6, 6);
        lv_obj_set_style_bg_color(dot, i == active ? FG_PRIMARY : dim, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    }
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

static void page_header(lv_obj_t *page, const char *title_name, const char *title,
                        const char *clock_name)
{
    label(page, title_name, 24, 24, 220, 34, title, notification_22, FG_PRIMARY, LV_TEXT_ALIGN_LEFT);
    label(page, clock_name, 296, 26, 70, 30, "--:--", body_20, FG_PRIMARY, LV_TEXT_ALIGN_RIGHT);
}

static void render_hour_page(weather_view_t *view, lv_obj_t *page, unsigned page_index)
{
    for (unsigned column = 0; column < 4; ++column) {
        unsigned data_index = page_index * 4U + column;
        int x = 10 + (int)column * 96;
        char name[24];
        snprintf(name, sizeof name, "weather_hour_%u", data_index);
        lv_obj_t *host = panel(page, name, x, 0, 92, WEATHER_FORECAST_HEIGHT, view->background);
        lv_obj_set_style_bg_opa(host, LV_OPA_TRANSP, 0);
        label(host, "hour_time", 0, 18, 92, 30, "--:--", body_20, FG_PRIMARY, LV_TEXT_ALIGN_CENTER);
        label(host, "hour_temp", 0, 68, 92, 42, "--", title_24, FG_PRIMARY, LV_TEXT_ALIGN_CENTER);
        label(host, "hour_icon", 0, 120, 92, 48, "", icons_44,
              FG_PRIMARY, LV_TEXT_ALIGN_CENTER);
        label(host, "hour_wind_icon", 32, 180, 28, 34, "\xef\x87\x98", icons_20,
              FG_PRIMARY, LV_TEXT_ALIGN_CENTER);
        label(host, "hour_wind", 0, 215, 92, 30, "--", body_20, FG_PRIMARY,
              LV_TEXT_ALIGN_CENTER);
        label(host, "hour_air", 0, 252, 92, 30, "--", notification_22, FG_PRIMARY,
              LV_TEXT_ALIGN_CENTER);
    }
}

static void render_daily_page(weather_view_t *view, lv_obj_t *page, unsigned page_index)
{
    unsigned first = page_index * 5U;
    unsigned count = page_index == 0U ? 5U : 2U;
    for (unsigned row = 0; row < count; ++row) {
        int y = 14 + (int)row * 58;
        char name[24];
        snprintf(name, sizeof name, "weather_day_%u", first + row);
        lv_obj_t *host = panel(page, name, 0, y, WEATHER_WIDTH, 58, view->background);
        lv_obj_set_style_bg_opa(host, LV_OPA_TRANSP, 0);
        label(host, "day_icon", 18, 4, 52, 44, "", icons_44,
              FG_PRIMARY, LV_TEXT_ALIGN_CENTER);
        label(host, "day_name", 88, 10, 104, 32, "--", notification_22, FG_PRIMARY,
              LV_TEXT_ALIGN_LEFT);
        label(host, "day_range", 220, 10, 148, 32, "--/--", title_24, FG_PRIMARY,
              LV_TEXT_ALIGN_RIGHT);
    }
}

static void render_ready(weather_view_t *view)
{
    char text[32];
    if (view->data.temperature_valid) snprintf(text, sizeof text, "%d°", view->data.temperature);
    else snprintf(text, sizeof text, "--");
    set_text(view->root, "weather_city", view->data.city[0] ? view->data.city : "--");
    set_text(view->root, "weather_update", view->data.updated);
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
    for (unsigned i = 0; i < WEATHER_PAGE_COUNT; ++i)
        lv_obj_set_style_bg_color(lv_obj_get_child(view->outer, i), view->background, 0);
    lv_obj_t *sun = lv_obj_get_child(lv_obj_get_child(view->outer, 4), 0);
    lv_obj_set_style_bg_color(sun, view->background, 0);
    update_dots(lv_obj_find_by_name(view->root, "weather_hourly_dots"), 6,
                view->hourly_page, weather_dot_dim(view->theme));
    update_dots(lv_obj_find_by_name(view->root, "weather_daily_dots"), 2,
                view->daily_page, weather_dot_dim(view->theme));
    bool ready = view->state == WRISTFLOW_WEATHER_READY;
    lv_obj_set_flag(view->outer, LV_OBJ_FLAG_HIDDEN, !ready);
    lv_obj_set_flag(view->state_panel, LV_OBJ_FLAG_HIDDEN, ready);
    lv_obj_set_flag(view->state_retry, LV_OBJ_FLAG_HIDDEN,
                    view->state == WRISTFLOW_WEATHER_LOADING);
    lv_label_set_text(view->state_label, view->data.message);
    render_ready(view);
}

static weather_view_t *view_from(lv_obj_t *root)
{
    return root ? (weather_view_t *)lv_obj_get_user_data(root) : NULL;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
bool wristflow_weather_request_sync(void)
{
    return false;
}

static void retry(lv_event_t *event)
{
    weather_view_t *view = lv_event_get_user_data(event);
    if (!view) return;
    bool queued = wristflow_weather_request_sync();
    wristflow_weather_screen_set_state(view->root,
        queued ? WRISTFLOW_WEATHER_LOADING : WRISTFLOW_WEATHER_ERROR);
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

static void create_current_page(weather_view_t *view, lv_obj_t *page)
{
    label(page, "weather_city", 0, 42, WEATHER_WIDTH, 34, "--", notification_22,
          FG_PRIMARY, LV_TEXT_ALIGN_CENTER);
    label(page, "weather_update", 0, 80, WEATHER_WIDTH, 30, "", notification_22,
          FG_PRIMARY, LV_TEXT_ALIGN_CENTER);
    label(page, "weather_temp", 0, 132, WEATHER_WIDTH, 72, "--", metric_56,
          FG_PRIMARY, LV_TEXT_ALIGN_CENTER);
    label(page, "weather_condition", 0, 232, WEATHER_WIDTH, 34, "--", notification_22,
          FG_PRIMARY, LV_TEXT_ALIGN_CENTER);
    label(page, "weather_range", 0, 270, WEATHER_WIDTH, 30, "--/--", notification_22,
          FG_PRIMARY, LV_TEXT_ALIGN_CENTER);
    label(page, "weather_aqi_caption", 0, 360, WEATHER_WIDTH, 28, "空气质量", notification_22,
          FG_PRIMARY, LV_TEXT_ALIGN_CENTER);
    label(page, "weather_aqi", 0, 392, WEATHER_WIDTH, 34, "--", notification_22,
          FG_PRIMARY, LV_TEXT_ALIGN_CENTER);
    (void)view;
}

static void create_hourly_page(weather_view_t *view, lv_obj_t *page)
{
    page_header(page, "weather_hourly_title", "天气预测", "weather_hourly_clock");
    view->hourly = panel(page, "weather_hourly_pager", 0, WEATHER_FORECAST_TOP,
                         WEATHER_WIDTH, WEATHER_FORECAST_HEIGHT, view->background);
    lv_obj_set_style_bg_opa(view->hourly, LV_OPA_TRANSP, 0);
    for (unsigned i = 0; i < 6; ++i) {
        lv_obj_t *hour_page = panel(view->hourly, "weather_hour_page", (int)i * WEATHER_INNER_WIDTH, 0,
                                    WEATHER_INNER_WIDTH, WEATHER_FORECAST_HEIGHT, view->background);
        lv_obj_set_style_bg_opa(hour_page, LV_OPA_TRANSP, 0);
        render_hour_page(view, hour_page, i);
    }
    configure_scroll(view->hourly, LV_DIR_HOR, LV_SCROLL_SNAP_START, true, false);
    lv_obj_set_scroll_snap_x(view->hourly, LV_SCROLL_SNAP_NONE);
    dots(page, "weather_hourly_dots", 6, 0, 420, view->background,
         weather_dot_dim(view->theme));
    lv_obj_add_event_cb(view->hourly, scroll_begin, LV_EVENT_SCROLL_BEGIN, view);
    lv_obj_add_event_cb(view->hourly, scroll_end, LV_EVENT_SCROLL_END, view);
}

static void create_daily_page(weather_view_t *view, lv_obj_t *page)
{
    page_header(page, "weather_daily_title", "未来天气", "weather_daily_clock");
    view->daily = panel(page, "weather_daily_pager", 0, WEATHER_FORECAST_TOP,
                        WEATHER_WIDTH, WEATHER_FORECAST_HEIGHT, view->background);
    lv_obj_set_style_bg_opa(view->daily, LV_OPA_TRANSP, 0);
    for (unsigned i = 0; i < 2; ++i) {
        lv_obj_t *daily_page = panel(view->daily, "weather_day_page", (int)i * WEATHER_INNER_WIDTH, 0,
                                     WEATHER_INNER_WIDTH, WEATHER_FORECAST_HEIGHT, view->background);
        lv_obj_set_style_bg_opa(daily_page, LV_OPA_TRANSP, 0);
        render_daily_page(view, daily_page, i);
    }
    configure_scroll(view->daily, LV_DIR_HOR, LV_SCROLL_SNAP_START, true, false);
    lv_obj_set_scroll_snap_x(view->daily, LV_SCROLL_SNAP_NONE);
    dots(page, "weather_daily_dots", 2, 0, 420, view->background,
         weather_dot_dim(view->theme));
    lv_obj_add_event_cb(view->daily, scroll_begin, LV_EVENT_SCROLL_BEGIN, view);
    lv_obj_add_event_cb(view->daily, scroll_end, LV_EVENT_SCROLL_END, view);
}

static void create_indices_page(lv_obj_t *page)
{
    static const char *const icons[] = {
        "\xef\x83\x82", "\xef\x81\x83", "\xef\x81\x93", "\xef\x86\x85"
    };
    page_header(page, "weather_indices_title", "天气指数", "weather_indices_clock");
    static const char *const labels[] = {"空气质量", "相对湿度(%)", "风力", "紫外线指数"};
    for (unsigned i = 0; i < 4; ++i) {
        char name[40];
        int y = 78 + (int)i * 84;
        snprintf(name, sizeof name, "weather_index_%u_value", i);
        label(page, name, 30, y, 88, 58, "--", metric_56, FG_PRIMARY, LV_TEXT_ALIGN_LEFT);
        snprintf(name, sizeof name, "weather_index_%u_icon", i);
        label(page, name, 138, y + 12, 36, 36, icons[i], icons_20, FG_PRIMARY, LV_TEXT_ALIGN_CENTER);
        snprintf(name, sizeof name, "weather_index_%u_label", i);
        label(page, name, 184, y + 16, 170, 30, labels[i], notification_22, FG_PRIMARY, LV_TEXT_ALIGN_LEFT);
    }
}

static void create_sun_page(lv_obj_t *page, lv_color_t background)
{
    lv_obj_t *generated = wristflow_mount_screen(page, screen_weather_sun_create());
    lv_obj_set_style_bg_color(generated, background, 0);
    lv_obj_set_style_bg_opa(generated, LV_OPA_COVER, 0);
    set_text(generated, "weather_sun_clock", "--:--");
    set_text(generated, "weather_sunrise", "--:--");
    set_text(generated, "weather_sunset", "--:--");
    lv_obj_add_flag(lv_obj_find_by_name(generated, "weather_sun_position"), LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *wristflow_weather_screen_create_with_data(const wristflow_weather_data_t *data)
{
    lv_obj_t *root = lv_obj_create(NULL);
    lv_obj_set_name(root, "screen_weather");
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, WEATHER_WIDTH, WEATHER_HEIGHT);
    lv_color_t background = weather_background(data ? data->theme : WRISTFLOW_WEATHER_THEME_CLOUDY);
    lv_obj_set_style_bg_color(root, background, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(root, body_20, 0);
    lv_obj_set_style_text_color(root, FG_PRIMARY, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

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

    view->outer = panel(root, "weather_pager", 0, 0, WEATHER_WIDTH, WEATHER_HEIGHT, background);
    lv_obj_set_style_bg_opa(view->outer, LV_OPA_TRANSP, 0);
    lv_obj_t *current = panel(view->outer, "weather_page_current", 0, 0, WEATHER_WIDTH,
                              WEATHER_PAGE_HEIGHT, background);
    create_current_page(view, current);
    lv_obj_t *hourly = panel(view->outer, "weather_page_hourly", 0, WEATHER_PAGE_HEIGHT,
                             WEATHER_WIDTH, WEATHER_PAGE_HEIGHT, background);
    create_hourly_page(view, hourly);
    lv_obj_t *daily = panel(view->outer, "weather_page_daily", 0, WEATHER_PAGE_HEIGHT * 2,
                            WEATHER_WIDTH, WEATHER_PAGE_HEIGHT, background);
    create_daily_page(view, daily);
    lv_obj_t *indices = panel(view->outer, "weather_page_indices", 0, WEATHER_PAGE_HEIGHT * 3,
                              WEATHER_WIDTH, WEATHER_PAGE_HEIGHT, background);
    create_indices_page(indices);
    lv_obj_t *sun = panel(view->outer, "weather_page_sun", 0, WEATHER_PAGE_HEIGHT * 4,
                          WEATHER_WIDTH, WEATHER_PAGE_HEIGHT, background);
    create_sun_page(sun, background);
    configure_scroll(view->outer, LV_DIR_VER, LV_SCROLL_SNAP_START, false, true);
    lv_obj_add_event_cb(view->outer, scroll_end, LV_EVENT_SCROLL_END, view);

    view->state_panel = panel(root, "weather_state_panel", 0, 0, WEATHER_WIDTH, WEATHER_HEIGHT,
                              background);
    label(view->state_panel, "weather_state_icon", 0, 148, WEATHER_WIDTH, 50, "\xef\x83\x82",
          icons_44, FG_PRIMARY, LV_TEXT_ALIGN_CENTER);
    view->state_label = label(view->state_panel, "weather_state_label", 24, 214, 342, 30,
                              "正在同步天气", notification_22, FG_PRIMARY, LV_TEXT_ALIGN_CENTER);
    view->state_retry = lv_button_create(view->state_panel);
    lv_obj_remove_style_all(view->state_retry);
    lv_obj_set_name(view->state_retry, "weather_retry");
    lv_obj_set_pos(view->state_retry, 134, 276);
    lv_obj_set_size(view->state_retry, 122, 42);
    lv_obj_set_style_bg_opa(view->state_retry, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(view->state_retry, FG_PRIMARY, 0);
    lv_obj_set_style_border_width(view->state_retry, 1, 0);
    lv_obj_set_style_radius(view->state_retry, 0, 0);
    label(view->state_retry, "weather_retry_label", 0, 8, 122, 26, "重试", notification_22,
          FG_PRIMARY, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_event_cb(view->state_retry, retry, LV_EVENT_SHORT_CLICKED, view);
    render_state(view);
    return root;
}

void wristflow_weather_screen_refresh(lv_obj_t *screen)
{
    weather_view_t *view = view_from(screen);
    if (!view) return;
    wristflow_weather_get_current(&view->data, &view->state, 0);
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
    if (wristflow_weather_get_current(&data, NULL, 0))
        return wristflow_weather_screen_create_with_data(&data);
    wristflow_weather_request_sync();
    return wristflow_weather_screen_create_with_data(NULL);
}

void wristflow_weather_screen_set_state(lv_obj_t *screen, wristflow_weather_state_t state)
{
    weather_view_t *view = view_from(screen);
    if (!view || state > WRISTFLOW_WEATHER_ERROR) return;
    view->state = state;
    if (state == WRISTFLOW_WEATHER_READY) {
        wristflow_weather_get_current(&view->data, &view->state, 0);
    } else {
        view->data = (wristflow_weather_data_t){0};
        snprintf(view->data.message, sizeof view->data.message, "%s",
                  wristflow_weather_state_message(state));
    }
    render_state(view);
}


bool wristflow_weather_screen_is_horizontal(lv_obj_t *screen)
{
    weather_view_t *view = view_from(screen);
    if (!view || view->state != WRISTFLOW_WEATHER_READY) return false;
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
