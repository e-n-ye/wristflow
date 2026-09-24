#include "ui_demo.h"
#include "wristflow_ui.h"
#include "ui_shell.h"

static wristflow_ui_shell_t *shell;

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
        screen_control_center_create, {22, 48, 53}
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
