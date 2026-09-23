#ifndef WRISTFLOW_WATCHFACE_PRESENTER_H
#define WRISTFLOW_WATCHFACE_PRESENTER_H

#include "wristflow_ui_gen.h"

/* Call after wristflow_ui_init, on the LVGL thread. Does not read an RTC. */
bool wristflow_watchface_set_time(lv_obj_t *screen, unsigned int hour_24,
                                 unsigned int minute);

#endif
