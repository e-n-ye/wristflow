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
static wristflow_weather_lock_t cache_lock, cache_unlock;
static void *cache_context;

void wristflow_weather_set_lock(wristflow_weather_lock_t lock, wristflow_weather_lock_t unlock, void *context)
{
    cache_lock = lock && unlock ? lock : NULL;
    cache_unlock = lock && unlock ? unlock : NULL;
    cache_context = context;
}

static void lock_cache(void) { if (cache_lock) cache_lock(cache_context); }
static void unlock_cache(void) { if (cache_unlock) cache_unlock(cache_context); }

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
    lock_cache();
    memset(&g_weather, 0, sizeof g_weather);
    unlock_cache();
}

bool wristflow_weather_has_data(void)
{
    lock_cache();
    bool result = g_weather.has_data;
    unlock_cache();
    return result;
}

void wristflow_weather_update(const wristflow_phone_weather_t *update)
{
    if (!update) return;
    lock_cache();
    g_weather.current = *update;
    g_weather.has_data = true;
    g_weather.state = WRISTFLOW_WEATHER_READY;
    unlock_cache();
}

static void local_time(char dest[6], uint32_t utc)
{
    unsigned minute = (unsigned)(((uint64_t)utc + 8U * 3600U) / 60U % 1440U);
    snprintf(dest, 6, "%02u:%02u", minute / 60U, minute % 60U);
}

static const char *forecast_icon(int16_t code)
{
    if (code == 800) return "sun";
    if (code >= 801 && code <= 804) return "cloud";
    if (code >= 300 && code < 600) return "rain";
    return "";
}

bool wristflow_weather_get_current(wristflow_weather_data_t *data, wristflow_weather_state_t *state, uint32_t now_utc)
{
    if (!data) return false;
    lock_cache();
    bool has_data = g_weather.has_data;
    wristflow_weather_state_t cached_state = g_weather.state;
    wristflow_phone_weather_t current = g_weather.current;
    unlock_cache();
    memset(data, 0, sizeof *data);
    if (!has_data) {
        if (state) *state = WRISTFLOW_WEATHER_EMPTY;
        snprintf(data->message, sizeof data->message, "%s", wristflow_weather_state_message(WRISTFLOW_WEATHER_EMPTY));
        return false;
    }
    if (state) *state = cached_state;
    snprintf(data->city, sizeof data->city, "%s", current.city[0] ? current.city : "未知城市");
    data->temperature = current.temp;
    data->temperature_valid = true;
    data->range_valid = current.range_valid;
    if (data->range_valid) {
        data->high = current.high;
        data->low = current.low;
    }
    if (current.condition[0]) {
        snprintf(data->condition, sizeof data->condition, "%s", current.condition);
    } else {
        snprintf(data->condition, sizeof data->condition, "--");
    }
    data->theme = wristflow_weather_determine_theme(current.code, data->condition);

    if (now_utc > 0 && current.timestamp > 0 && now_utc >= current.timestamp && now_utc - current.timestamp >= 10800) {
        unsigned hours = (now_utc - current.timestamp) / 3600;
        snprintf(data->updated, sizeof data->updated, "%u小时前更新", hours);
    } else {
        snprintf(data->updated, sizeof data->updated, "刚刚更新");
    }

    snprintf(data->indices[0].value, sizeof data->indices[0].value, "--");
    snprintf(data->indices[0].label, sizeof data->indices[0].label, "空气质量");
    if (current.humidity_valid)
        snprintf(data->indices[1].value, sizeof data->indices[1].value, "%u", current.humidity);
    else snprintf(data->indices[1].value, sizeof data->indices[1].value, "--");
    snprintf(data->indices[1].label, sizeof data->indices[1].label, "相对湿度(%%)");
    snprintf(data->indices[2].value, sizeof data->indices[2].value, "%s",
             current.wind[0] ? current.wind : "--");
    snprintf(data->indices[2].label, sizeof data->indices[2].label, "风力");
    if (current.uv_valid)
        snprintf(data->indices[3].value, sizeof data->indices[3].value, "%u.%u", current.uv_tenths / 10U, current.uv_tenths % 10U);
    else snprintf(data->indices[3].value, sizeof data->indices[3].value, "--");
    snprintf(data->indices[3].label, sizeof data->indices[3].label, "紫外线指数");
    snprintf(data->aqi, sizeof data->aqi, "--");
    if (current.extra.sunrise) local_time(data->sunrise, current.extra.sunrise);
    else snprintf(data->sunrise, sizeof data->sunrise, "--:--");
    if (current.extra.sunset) local_time(data->sunset, current.extra.sunset);
    else snprintf(data->sunset, sizeof data->sunset, "--:--");
    for (unsigned i = 0; i < WRISTFLOW_WEATHER_HOURLY_COUNT; ++i) {
        const wristflow_phone_weather_hour_t *source = &current.extra.hourly[i];
        wristflow_weather_hour_t *hour = &data->hourly[i];
        hour->valid = source->valid;
        if (!hour->valid) continue;
        local_time(hour->time, source->timestamp);
        hour->temperature = source->temperature;
        snprintf(hour->icon, sizeof hour->icon, "%s", forecast_icon(source->code));
        if (source->wind_kmh) snprintf(hour->wind, sizeof hour->wind, "%u km/h", source->wind_kmh);
        else snprintf(hour->wind, sizeof hour->wind, "--");
        snprintf(hour->air, sizeof hour->air, "--");
    }
    for (unsigned i = 0; i < WRISTFLOW_WEATHER_DAILY_COUNT; ++i) {
        const wristflow_phone_weather_day_t *source = &current.extra.daily[i];
        wristflow_weather_day_t *day = &data->daily[i];
        day->valid = source->valid;
        if (!day->valid) continue;
        /* Gadgetbridge daily entries carry no date; ordinal labels avoid inventing one. */
        snprintf(day->day, sizeof day->day, "第%u天", i + 1);
        day->high = source->high; day->low = source->low;
        snprintf(day->icon, sizeof day->icon, "%s", forecast_icon(source->code));
    }
    return true;
}
