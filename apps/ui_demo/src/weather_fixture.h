#ifndef WRISTFLOW_WEATHER_FIXTURE_H
#define WRISTFLOW_WEATHER_FIXTURE_H

#include "weather_screen.h"

bool wristflow_weather_demo_read(wristflow_weather_theme_t theme, wristflow_weather_data_t *data);
lv_obj_t *screen_weather_demo_create(wristflow_weather_theme_t theme);

#endif
