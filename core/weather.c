#include "weather.h"
#include <stdio.h>
#include <string.h>

const char *wristflow_weather_state_message(wristflow_weather_state_t state)
{
    switch (state) {
    case WRISTFLOW_WEATHER_LOADING: return "正在同步天气";
    case WRISTFLOW_WEATHER_EMPTY: return "暂无天气数据";
    case WRISTFLOW_WEATHER_ERROR: return "天气更新失败";
    case WRISTFLOW_WEATHER_READY: return "";
    default: return "天气状态未知";
    }
}

static const char *hour_icon(unsigned index)
{
    static const char *const icons[] = {
        "cloud", "cloud", "cloud", "sun", "sun", "cloud", "cloud", "rain",
        "rain", "cloud", "cloud", "cloud", "cloud", "sun", "cloud", "cloud",
        "cloud", "cloud", "rain", "rain", "cloud", "cloud", "cloud", "cloud"
    };
    return icons[index % (sizeof icons / sizeof icons[0])];
}

bool wristflow_weather_provider_read_fixture(wristflow_weather_state_t state,
                                              wristflow_weather_fixture_t fixture,
                                              wristflow_weather_data_t *data)
{
    if (!data) return false;
    *data = (wristflow_weather_data_t){0};
    data->fixture = fixture;
    snprintf(data->message, sizeof data->message, "%s", wristflow_weather_state_message(state));
    if (state != WRISTFLOW_WEATHER_READY) return true;

    snprintf(data->city, sizeof data->city, "乐清市");
    snprintf(data->updated, sizeof data->updated, "刚刚更新");
    data->temperature = 30;
    data->high = 31;
    data->low = 24;
    snprintf(data->condition, sizeof data->condition, "%s",
             fixture == WRISTFLOW_WEATHER_FIXTURE_SUNNY ? "晴" : "多云");
    snprintf(data->aqi, sizeof data->aqi, "优");
    snprintf(data->sunrise, sizeof data->sunrise, "05:48");
    snprintf(data->sunset, sizeof data->sunset, "17:44");

    for (unsigned i = 0; i < WRISTFLOW_WEATHER_HOURLY_COUNT; ++i) {
        unsigned hour = (15U + i) % 24U;
        snprintf(data->hourly[i].time, sizeof data->hourly[i].time, "%02u:00", hour);
        data->hourly[i].temperature = (int8_t)(31 - (int)(i / 5U));
        snprintf(data->hourly[i].icon, sizeof data->hourly[i].icon, "%s", hour_icon(i));
        snprintf(data->hourly[i].wind, sizeof data->hourly[i].wind, "%u级", i > 14U ? 1U : 2U);
        snprintf(data->hourly[i].air, sizeof data->hourly[i].air, "%s", i > 5U && i < 11U ? "优" : "良");
    }

    static const char *const days[] = {"今天", "明天", "周五", "周六", "周日", "周一", "周二"};
    static const int8_t highs[] = {31, 27, 26, 27, 27, 24, 24};
    static const int8_t lows[] = {24, 23, 22, 22, 21, 16, 16};
    static const char *const daily_icons[] = {"cloud", "rain", "rain", "cloud", "rain", "cloud", "sun"};
    for (unsigned i = 0; i < WRISTFLOW_WEATHER_DAILY_COUNT; ++i) {
        snprintf(data->daily[i].day, sizeof data->daily[i].day, "%s", days[i]);
        snprintf(data->daily[i].icon, sizeof data->daily[i].icon, "%s", daily_icons[i]);
        data->daily[i].high = highs[i];
        data->daily[i].low = lows[i];
    }

    static const char *const values[] = {"41", "80", "3", "4"};
    static const char *const labels[] = {"空气质量", "相对湿度(%)", "东南风", "紫外线指数"};
    for (unsigned i = 0; i < WRISTFLOW_WEATHER_INDEX_COUNT; ++i) {
        snprintf(data->indices[i].value, sizeof data->indices[i].value, "%s", values[i]);
        snprintf(data->indices[i].label, sizeof data->indices[i].label, "%s", labels[i]);
    }
    return true;
}

bool wristflow_weather_provider_read(wristflow_weather_state_t state,
                                      wristflow_weather_data_t *data)
{
    return wristflow_weather_provider_read_fixture(state,
                                                   WRISTFLOW_WEATHER_FIXTURE_CLOUDY,
                                                   data);
}
