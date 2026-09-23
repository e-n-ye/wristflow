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
    lv_unlock();

    for (;;)
    {
        uint32_t delay = lv_timer_handler();
        lv_delay_ms(delay > 10 ? 10 : delay);
    }
}
