#ifndef WRISTFLOW_WEATHER_SCREEN_H
#define WRISTFLOW_WEATHER_SCREEN_H

#include "weather.h"
#include "lvgl.h"

lv_obj_t *screen_weather_create(void);
lv_obj_t *wristflow_weather_screen_create_with_data(const wristflow_weather_data_t *data);
bool wristflow_weather_screen_is_horizontal(lv_obj_t *screen);
/* Horizontal forecast content owns rightward drags in its content band. */
bool wristflow_weather_screen_edge_back_allowed(lv_obj_t *screen, lv_point_t origin);
bool wristflow_weather_request_sync(void);
void wristflow_weather_screen_refresh(lv_obj_t *screen);
void wristflow_weather_screen_set_time(lv_obj_t *screen, const char *time_str);

#endif
