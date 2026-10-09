#include "weather_fixture.h"
#include <stdio.h>

bool wristflow_weather_demo_read(wristflow_weather_theme_t theme, wristflow_weather_data_t *data)
{
    if (!data) return false;
    *data = (wristflow_weather_data_t){0};
    data->theme = theme;
    data->temperature_valid = data->range_valid = data->sun_position_valid = true;
    snprintf(data->city, sizeof data->city, "乐清市");
    snprintf(data->updated, sizeof data->updated, "刚刚更新");
    data->temperature = 30;
    data->high = 31;
    data->low = 24;
    snprintf(data->condition, sizeof data->condition, "%s",
             theme == WRISTFLOW_WEATHER_THEME_SUNNY ? "晴" : "多云");
    snprintf(data->aqi, sizeof data->aqi, "优");
    snprintf(data->sunrise, sizeof data->sunrise, "05:48");
    snprintf(data->sunset, sizeof data->sunset, "17:44");
    static const char *const hour_icons[] = {
        "cloud", "cloud", "cloud", "sun", "sun", "cloud", "cloud", "rain",
        "rain", "cloud", "cloud", "cloud", "cloud", "sun", "cloud", "cloud",
        "cloud", "cloud", "rain", "rain", "cloud", "cloud", "cloud", "cloud"
    };
    for (unsigned i = 0; i < WRISTFLOW_WEATHER_HOURLY_COUNT; ++i) {
        wristflow_weather_hour_t *hour = &data->hourly[i];
        hour->valid = true;
        snprintf(hour->time, sizeof hour->time, "%02u:00", (15U + i) % 24U);
        hour->temperature = (int8_t)(31 - (int)(i / 5U));
        snprintf(hour->icon, sizeof hour->icon, "%s", hour_icons[i]);
        snprintf(hour->wind, sizeof hour->wind, "%u级", i > 14U ? 1U : 2U);
        snprintf(hour->air, sizeof hour->air, "%s", i > 5U && i < 11U ? "优" : "良");
    }
    static const char *const days[] = {"今天", "明天", "周五", "周六", "周日", "周一", "周二"};
    static const int8_t highs[] = {31, 27, 26, 27, 27, 24, 24};
    static const int8_t lows[] = {24, 23, 22, 22, 21, 16, 16};
    static const char *const day_icons[] = {"cloud", "rain", "rain", "cloud", "rain", "cloud", "sun"};
    for (unsigned i = 0; i < WRISTFLOW_WEATHER_DAILY_COUNT; ++i) {
        wristflow_weather_day_t *day = &data->daily[i];
        day->valid = true;
        snprintf(day->day, sizeof day->day, "%s", days[i]);
        snprintf(day->icon, sizeof day->icon, "%s", day_icons[i]);
        day->high = highs[i];
        day->low = lows[i];
    }
    static const char *const values[] = {"41", "80", "3", "4"};
    static const char *const labels[] = {"空气质量", "相对湿度(%)", "东南风", "紫外线指数"};
    for (unsigned i = 0; i < WRISTFLOW_WEATHER_INDEX_COUNT; ++i) {
        snprintf(data->indices[i].value, sizeof data->indices[i].value, "%s", values[i]);
        snprintf(data->indices[i].label, sizeof data->indices[i].label, "%s", labels[i]);
    }
    return true;
}

lv_obj_t *screen_weather_demo_create(wristflow_weather_theme_t theme)
{
    wristflow_weather_data_t data;
    wristflow_weather_demo_read(theme, &data);
    return wristflow_weather_screen_create_with_data(&data);
}
