#include "ui_demo.h"
#include "wristflow_ui.h"
#include "ui_shell.h"

static wristflow_ui_shell_t *shell;
static wristflow_brightness_cb_t brightness_cb;
static void *platform_context;

void wristflow_demo_set_platform(wristflow_brightness_cb_t brightness, void *context)
{
    LV_ASSERT(!shell);
    brightness_cb = brightness;
    platform_context = context;
}

void wristflow_demo_key(void) { wristflow_ui_shell_key(shell); }
void wristflow_demo_back(void) { wristflow_ui_shell_back(shell); }
unsigned wristflow_demo_surface(void)
{
    const wristflow_navigation_t *navigation = wristflow_ui_shell_navigation(shell);
    return navigation ? (unsigned)navigation->surface : 0;
}

void wristflow_demo_keyboard(lv_event_t *event)
{
    uint32_t key = lv_event_get_key(event);
    if (key == LV_KEY_ENTER) wristflow_demo_key();
    else if (key == LV_KEY_ESC) wristflow_demo_back();
    else return;
    lv_indev_t *input = lv_indev_active();
    if (input) lv_indev_wait_release(input);
}

void wristflow_demo_start(void)
{
    if (shell)
        return;
    /* The last card is a component showcase, not an extra product ring page. */
    const wristflow_screen_factory_t cards[] = {
        screen_tile_heart_rate_create, screen_tile_activity_create,
        screen_tile_system_create, screen_tile_full_create
    };
    wristflow_ui_shell_config_t config = {
        .watchface = &wristflow_default_watchface, .cards = cards, .card_count = sizeof(cards) / sizeof(cards[0]),
        .controls = screen_control_center_create, .initial_snapshot = {.hour_24 = 22, .minute = 48, .battery_percent = 53},
        .enable_apps = true, .set_brightness = brightness_cb, .platform_context = platform_context
    };
    /* UI-only sample data. This source is not compiled into Product. */
    static const uint32_t totals[] = {186, 4286, 18, 7};
    static const uint32_t goals[] = {400, 6000, 30, 12};
    for (unsigned i = 0; i < WRISTFLOW_ACTIVITY_METRICS; ++i) {
        wristflow_activity_metric_t *metric = &config.initial_snapshot.activity.metrics[i];
        metric->total = totals[i]; metric->goal = goals[i];
        metric->total_valid = metric->goal_valid = true;
        metric->hours_valid = 0x7ffffU;
        metric->hourly[8] = i == 1 ? 1280 : i == 0 ? 56 : i == 2 ? 6 : 1;
        metric->hourly[12] = i == 1 ? 1632 : i == 0 ? 70 : i == 2 ? 7 : 1;
        metric->hourly[18] = i == 1 ? 1374 : i == 0 ? 60 : i == 2 ? 5 : 1;
        if (i == 3) for (unsigned h = 13; h < 17; ++h) metric->hourly[h] = 1;
    }
    lv_obj_t *initial = lv_screen_active();
    shell = wristflow_ui_shell_create(&config);
    LV_ASSERT(shell);
    lv_obj_delete(initial);
}

void wristflow_demo_stop(void)
{
    wristflow_ui_shell_destroy(shell);
    shell = NULL;
}
