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

static struct {
    bool has_data;
    wristflow_weather_state_t state;
    wristflow_phone_weather_t current;
} g_weather = {0};

static bool contains_ignore_case(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return false;
    size_t nlen = strlen(needle);
    for (; *haystack; ++haystack) {
        size_t i;
        for (i = 0; i < nlen; ++i) {
            char h = haystack[i];
            char n = needle[i];
            if (h >= 'A' && h <= 'Z') h += ('a' - 'A');
            if (n >= 'A' && n <= 'Z') n += ('a' - 'A');
            if (!h || h != n) break;
        }
        if (i == nlen) return true;
    }
    return false;
}

wristflow_weather_theme_t wristflow_weather_determine_theme(int16_t code, const char *txt)
{
    if (code == 800) return WRISTFLOW_WEATHER_THEME_SUNNY;
    if (code >= 200 && code < 900 && code != 800) return WRISTFLOW_WEATHER_THEME_CLOUDY;
    if (txt) {
        if (strstr(txt, "晴") != NULL || contains_ignore_case(txt, "clear") || contains_ignore_case(txt, "sun"))
            return WRISTFLOW_WEATHER_THEME_SUNNY;
    }
    return WRISTFLOW_WEATHER_THEME_CLOUDY;
}

void wristflow_weather_reset(void)
{
    memset(&g_weather, 0, sizeof g_weather);
}

bool wristflow_weather_has_data(void)
{
    return g_weather.has_data;
}

void wristflow_weather_update(const wristflow_phone_weather_t *update)
{
    if (!update) return;
    g_weather.current = *update;
    g_weather.has_data = true;
    g_weather.state = WRISTFLOW_WEATHER_READY;
}

bool wristflow_weather_get_current(wristflow_weather_data_t *data, wristflow_weather_state_t *state, uint32_t now_utc)
{
    if (!data) return false;
    memset(data, 0, sizeof *data);
    if (!g_weather.has_data) {
        if (state) *state = WRISTFLOW_WEATHER_EMPTY;
        snprintf(data->message, sizeof data->message, "%s", wristflow_weather_state_message(WRISTFLOW_WEATHER_EMPTY));
        return false;
    }
    if (state) *state = g_weather.state;
    snprintf(data->city, sizeof data->city, "%s", g_weather.current.city[0] ? g_weather.current.city : "未知城市");
    data->temperature = g_weather.current.temp;
    data->temperature_valid = true;
    if (g_weather.current.condition[0]) {
        snprintf(data->condition, sizeof data->condition, "%s", g_weather.current.condition);
    } else {
        snprintf(data->condition, sizeof data->condition, "--");
    }
    data->theme = wristflow_weather_determine_theme(g_weather.current.code, data->condition);

    if (now_utc > 0 && g_weather.current.timestamp > 0 && now_utc >= g_weather.current.timestamp + 10800) {
        unsigned hours = (now_utc - g_weather.current.timestamp) / 3600;
        snprintf(data->updated, sizeof data->updated, "%u小时前更新", hours);
    } else {
        snprintf(data->updated, sizeof data->updated, "刚刚更新");
    }

    snprintf(data->indices[0].value, sizeof data->indices[0].value, "--");
    snprintf(data->indices[0].label, sizeof data->indices[0].label, "空气质量");
    if (g_weather.current.humidity_valid)
        snprintf(data->indices[1].value, sizeof data->indices[1].value, "%u", g_weather.current.humidity);
    else snprintf(data->indices[1].value, sizeof data->indices[1].value, "--");
    snprintf(data->indices[1].label, sizeof data->indices[1].label, "相对湿度(%%)");
    snprintf(data->indices[2].value, sizeof data->indices[2].value, "%s",
             g_weather.current.wind[0] ? g_weather.current.wind : "--");
    snprintf(data->indices[2].label, sizeof data->indices[2].label, "风力");
    snprintf(data->indices[3].value, sizeof data->indices[3].value, "--");
    snprintf(data->indices[3].label, sizeof data->indices[3].label, "紫外线指数");
    snprintf(data->aqi, sizeof data->aqi, "--");
    snprintf(data->sunrise, sizeof data->sunrise, "--:--");
    snprintf(data->sunset, sizeof data->sunset, "--:--");
    return true;
}
