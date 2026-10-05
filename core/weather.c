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
            if (h != n) break;
        }
        if (i == nlen) return true;
    }
    return false;
}

wristflow_weather_fixture_t wristflow_weather_determine_fixture(int16_t code, const char *txt)
{
    if (code == 800) return WRISTFLOW_WEATHER_FIXTURE_SUNNY;
    if (code >= 200 && code < 900 && code != 800) return WRISTFLOW_WEATHER_FIXTURE_CLOUDY;
    if (txt) {
        if (strstr(txt, "晴") != NULL || contains_ignore_case(txt, "clear") || contains_ignore_case(txt, "sun"))
            return WRISTFLOW_WEATHER_FIXTURE_SUNNY;
    }
    return WRISTFLOW_WEATHER_FIXTURE_CLOUDY;
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
    data->high = g_weather.current.temp;
    data->low = g_weather.current.temp;
    if (g_weather.current.condition[0]) {
        snprintf(data->condition, sizeof data->condition, "%s", g_weather.current.condition);
    } else {
        snprintf(data->condition, sizeof data->condition, "%s", g_weather.current.code == 800 ? "晴" : "多云");
    }
    data->fixture = wristflow_weather_determine_fixture(g_weather.current.code, data->condition);

    if (now_utc > 0 && g_weather.current.timestamp > 0 && now_utc >= g_weather.current.timestamp + 10800) {
        unsigned hours = (now_utc - g_weather.current.timestamp) / 3600;
        snprintf(data->updated, sizeof data->updated, "%u小时前更新", hours);
    } else {
        snprintf(data->updated, sizeof data->updated, "刚刚更新");
    }

    snprintf(data->indices[0].value, sizeof data->indices[0].value, "--");
    snprintf(data->indices[0].label, sizeof data->indices[0].label, "空气质量");
    snprintf(data->indices[1].value, sizeof data->indices[1].value, "%u%%", g_weather.current.humidity);
    snprintf(data->indices[1].label, sizeof data->indices[1].label, "相对湿度");
    char wind_val[16] = "--";
    if (g_weather.current.wind[0]) {
        unsigned w_idx = 0;
        for (const char *wp = g_weather.current.wind; *wp && w_idx + 1 < sizeof wind_val; ++wp) {
            if ((*wp >= '0' && *wp <= '9') || *wp == '.') {
                wind_val[w_idx++] = *wp;
            } else if (w_idx > 0) {
                break;
            }
        }
        wind_val[w_idx] = 0;
        if (w_idx == 0) snprintf(wind_val, sizeof wind_val, "%s", g_weather.current.wind);
    }
    snprintf(data->indices[2].value, sizeof data->indices[2].value, "%s", wind_val);
    snprintf(data->indices[2].label, sizeof data->indices[2].label, "风力");
    snprintf(data->indices[3].value, sizeof data->indices[3].value, "--");
    snprintf(data->indices[3].label, sizeof data->indices[3].label, "紫外线指数");
    snprintf(data->sunrise, sizeof data->sunrise, "06:00");
    snprintf(data->sunset, sizeof data->sunset, "18:00");
    return true;
}

bool wristflow_weather_provider_read(wristflow_weather_state_t state,
                                      wristflow_weather_data_t *data)
{
    if (g_weather.has_data && state == WRISTFLOW_WEATHER_READY) {
        return wristflow_weather_get_current(data, &state, 0);
    }
    return wristflow_weather_provider_read_fixture(state,
                                                   WRISTFLOW_WEATHER_FIXTURE_CLOUDY,
                                                   data);
}

