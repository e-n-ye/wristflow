#ifndef WRISTFLOW_WEATHER_SCREEN_H
#define WRISTFLOW_WEATHER_SCREEN_H

#include "weather.h"
#include "lvgl.h"

lv_obj_t *screen_weather_create(void);
lv_obj_t *screen_weather_create_with_fixture(wristflow_weather_fixture_t fixture);
void wristflow_weather_screen_set_state(lv_obj_t *screen, wristflow_weather_state_t state);
bool wristflow_weather_screen_is_horizontal(lv_obj_t *screen);
/* Horizontal forecast content owns rightward drags in its content band. */
bool wristflow_weather_screen_edge_back_allowed(lv_obj_t *screen, lv_point_t origin);

#endif
