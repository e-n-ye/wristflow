#include "wristflow_ui.h"
#include "ui_demo.h"
#include "src/drivers/windows/lv_windows_display.h"
#include "src/drivers/windows/lv_windows_input.h"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    (void)instance;
    (void)previous;
    (void)command;
    (void)show;
    lv_init();
    lv_display_t *display = lv_windows_create_display(
        L"WristFlow - LVGL Simulator", 390, 450, 100, false, false);
    if (!display)
        return 1;
    HWND window = lv_windows_get_display_window_handle(display);
    LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    SetWindowLongPtrW(window, GWL_STYLE, style & ~(WS_THICKFRAME | WS_MAXIMIZEBOX));
    SetWindowPos(window, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    /* The official Win32 driver owns the framebuffer and its message thread. */
    lv_lock();
    lv_indev_t *pointer = lv_windows_acquire_pointer_indev(display);
    if (!pointer)
    {
        lv_unlock();
        return 1;
    }
    wristflow_ui_init("");
    wristflow_demo_start();
    lv_indev_t *keypad = lv_windows_acquire_keypad_indev(display);
    lv_group_t *keys = lv_group_create();
    lv_obj_t *key_target = lv_obj_create(lv_layer_top());
    lv_obj_set_size(key_target, 1, 1);
    lv_obj_set_style_opa(key_target, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(key_target, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(key_target, wristflow_demo_keyboard, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keys, key_target);
    lv_indev_set_group(keypad, keys);
    lv_unlock();

    for (;;)
    {
        uint32_t delay = lv_timer_handler();
        lv_delay_ms(delay > 10 ? 10 : delay);
    }
}
