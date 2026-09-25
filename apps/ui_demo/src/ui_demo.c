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
    const wristflow_ui_shell_config_t config = {
        &wristflow_default_watchface, cards, sizeof(cards) / sizeof(cards[0]),
        screen_control_center_create, {22, 48, 53}, true, brightness_cb, platform_context
    };
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
