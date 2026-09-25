#ifndef WRISTFLOW_UI_DEMO_H
#define WRISTFLOW_UI_DEMO_H
#include "ui_shell.h"

/* Call after the display and wristflow_ui_init() are ready, on the LVGL thread. */
void wristflow_demo_start(void);
void wristflow_demo_stop(void);
void wristflow_demo_set_platform(wristflow_brightness_cb_t brightness, void *context);
void wristflow_demo_key(void);
void wristflow_demo_back(void);
unsigned wristflow_demo_surface(void);
void wristflow_demo_keyboard(lv_event_t *event);

#endif
