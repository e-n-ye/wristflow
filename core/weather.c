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
    case WRISTFLOW_WEATHER_STALE: return "天气已过期";
    default: return "天气状态未知";
    }
}

static struct {
    bool has_data;
    bool received_time, forecast_time, solar_time, expired;
    uint64_t received_ms, forecast_ms, solar_ms, requested_ms;
    uint32_t received_utc, request_id, request_peer;
    wristflow_weather_request_state_t request_state;
    wristflow_phone_weather_t current;
} g_weather = {0};
static wristflow_weather_lock_t cache_lock, cache_unlock;
static void *cache_context;
static wristflow_weather_clock_t cache_clock;
static void *clock_context;
static uint32_t request_serial;
static wristflow_weather_sender_t request_sender;
static void *sender_context;
static wristflow_weather_peer_t peer_reader;
static void *peer_context;

void wristflow_weather_set_lock(wristflow_weather_lock_t lock, wristflow_weather_lock_t unlock, void *context)
{
    cache_lock = lock && unlock ? lock : NULL;
    cache_unlock = lock && unlock ? unlock : NULL;
    cache_context = context;
}

static void lock_cache(void) { if (cache_lock) cache_lock(cache_context); }
static void unlock_cache(void) { if (cache_unlock) cache_unlock(cache_context); }

void wristflow_weather_set_clock(wristflow_weather_clock_t clock, void *context)
{
    cache_clock = clock;
    clock_context = context;
}

void wristflow_weather_set_sender(wristflow_weather_sender_t sender, void *context)
{
    request_sender = sender;
    sender_context = context;
}

void wristflow_weather_set_peer(wristflow_weather_peer_t peer, void *context)
{
    peer_reader = peer;
    peer_context = context;
}

static uint32_t peer(void) { return peer_reader ? peer_reader(peer_context) : 0; }

bool wristflow_weather_request_sync(void)
{
    bool started;
    uint32_t id = wristflow_weather_request_begin(&started);
    if (!id) return false;
    if (!started) return true;
    bool queued = request_sender && request_sender(id, sender_context);
    if (!queued) wristflow_weather_request_failed(id);
    return queued;
}

static wristflow_weather_time_t now(void)
{
    return cache_clock ? cache_clock(clock_context) : (wristflow_weather_time_t){0};
}

static bool old(uint64_t at, uint64_t current)
{
    /* A broken/reset monotonic clock cannot make old data fresh again. */
    return current < at || current - at >= WRISTFLOW_WEATHER_EXPIRY_MS;
}

static bool advance(wristflow_weather_time_t time)
{
    bool changed = false;
    if (g_weather.request_state == WRISTFLOW_WEATHER_REQUEST_PENDING && g_weather.request_peer != peer()) {
        g_weather.request_state = WRISTFLOW_WEATHER_REQUEST_FAILED;
        changed = true;
    }
    if (g_weather.request_state == WRISTFLOW_WEATHER_REQUEST_PENDING &&
        (time.monotonic_ms < g_weather.requested_ms ||
         time.monotonic_ms - g_weather.requested_ms >= WRISTFLOW_WEATHER_REQUEST_MS)) {
        g_weather.request_state = WRISTFLOW_WEATHER_REQUEST_TIMEOUT;
        changed = true;
    }
    if (g_weather.has_data && g_weather.received_time && !g_weather.expired &&
        old(g_weather.received_ms, time.monotonic_ms)) {
        g_weather.expired = true;
        changed = true;
    }
    return changed;
}

bool wristflow_weather_poll(void)
{
    lock_cache();
    bool changed = advance(now());
    unlock_cache();
    return changed;
}

uint32_t wristflow_weather_request_begin(bool *started)
{
    lock_cache();
    wristflow_weather_time_t time = now();
    advance(time);
    bool fresh = g_weather.request_state != WRISTFLOW_WEATHER_REQUEST_PENDING;
    if (fresh) {
        if (++request_serial == 0) ++request_serial;
        g_weather.request_id = request_serial;
        g_weather.request_peer = peer();
        g_weather.requested_ms = time.monotonic_ms;
        g_weather.request_state = cache_clock ? WRISTFLOW_WEATHER_REQUEST_PENDING : WRISTFLOW_WEATHER_REQUEST_FAILED;
    }
    if (started) *started = fresh;
    uint32_t id = cache_clock ? g_weather.request_id : 0;
    unlock_cache();
    return id;
}

void wristflow_weather_request_failed(uint32_t request)
{
    lock_cache();
    advance(now());
    if (request && request == g_weather.request_id &&
        g_weather.request_state == WRISTFLOW_WEATHER_REQUEST_PENDING)
        g_weather.request_state = WRISTFLOW_WEATHER_REQUEST_FAILED;
    unlock_cache();
}

void wristflow_weather_request_disconnected(void)
{
    lock_cache();
    /* Link callbacks only change state; do not perform an RTC read here. */
    if (g_weather.request_state == WRISTFLOW_WEATHER_REQUEST_PENDING)
        g_weather.request_state = WRISTFLOW_WEATHER_REQUEST_FAILED;
    unlock_cache();
}

bool wristflow_weather_request_pending(uint32_t request)
{
    lock_cache();
    advance(now());
    bool pending = request && request == g_weather.request_id &&
        g_weather.request_state == WRISTFLOW_WEATHER_REQUEST_PENDING;
    unlock_cache();
    return pending;
}

uint32_t wristflow_weather_request_remaining_ms(void)
{
    lock_cache();
    wristflow_weather_time_t time = now();
    uint32_t remaining = 0;
    if (g_weather.request_state == WRISTFLOW_WEATHER_REQUEST_PENDING) {
        /* Leave expiry to poll(), so its state-change event is not swallowed. */
        uint64_t elapsed = time.monotonic_ms - g_weather.requested_ms;
        remaining = g_weather.request_peer != peer() || time.monotonic_ms < g_weather.requested_ms || elapsed >= WRISTFLOW_WEATHER_REQUEST_MS ?
            1 : WRISTFLOW_WEATHER_REQUEST_MS - (uint32_t)elapsed;
    }
    unlock_cache();
    return remaining;
}

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

void wristflow_weather_update(const wristflow_phone_weather_t *update, unsigned updates)
{
    if (!update) return;
    lock_cache();
    wristflow_weather_time_t time = now();
    g_weather.current = *update;
    g_weather.has_data = true;
    g_weather.received_time = cache_clock != NULL;
    g_weather.received_ms = time.monotonic_ms;
    g_weather.received_utc = time.utc_seconds;
    g_weather.expired = false;
    g_weather.request_state = WRISTFLOW_WEATHER_REQUEST_IDLE;
    if (updates & WRISTFLOW_WEATHER_FORECAST_UPDATE) {
        g_weather.forecast_ms = time.monotonic_ms;
        g_weather.forecast_time = cache_clock != NULL;
    } else if (!update->extra.forecast_present) g_weather.forecast_time = false;
    if (updates & WRISTFLOW_WEATHER_SOLAR_UPDATE) {
        g_weather.solar_ms = time.monotonic_ms;
        g_weather.solar_time = cache_clock != NULL;
    } else if (!update->extra.sunrise && !update->extra.sunset) g_weather.solar_time = false;
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

bool wristflow_weather_get_current(wristflow_weather_data_t *data, wristflow_weather_state_t *state)
{
    if (!data) return false;
    lock_cache();
    wristflow_weather_time_t time = now();
    advance(time);
    bool has_data = g_weather.has_data;
    wristflow_weather_request_state_t request = g_weather.request_state;
    bool expired = g_weather.expired;
    bool age_known = g_weather.received_time && time.monotonic_ms >= g_weather.received_ms;
    uint64_t age_ms = age_known ? time.monotonic_ms - g_weather.received_ms : 0;
    uint32_t received_utc = g_weather.received_utc;
    wristflow_phone_weather_t current = g_weather.current;
    bool forecasts_old = g_weather.forecast_time && old(g_weather.forecast_ms, time.monotonic_ms);
    bool solar_old = g_weather.solar_time && old(g_weather.solar_ms, time.monotonic_ms);
    unlock_cache();
    memset(data, 0, sizeof *data);
    data->request_state = request;
    const char *message = request == WRISTFLOW_WEATHER_REQUEST_PENDING ? "正在同步天气" :
        request == WRISTFLOW_WEATHER_REQUEST_TIMEOUT ? "天气同步超时" :
        request == WRISTFLOW_WEATHER_REQUEST_FAILED ? "天气更新失败" : "";
    snprintf(data->message, sizeof data->message, "%s", message);
    if (!has_data) {
        if (state) *state = request == WRISTFLOW_WEATHER_REQUEST_PENDING ? WRISTFLOW_WEATHER_LOADING :
            request == WRISTFLOW_WEATHER_REQUEST_IDLE ? WRISTFLOW_WEATHER_EMPTY : WRISTFLOW_WEATHER_ERROR;
        if (!message[0]) snprintf(data->message, sizeof data->message, "%s", wristflow_weather_state_message(WRISTFLOW_WEATHER_EMPTY));
        return false;
    }
    data->expired = expired;
    data->age_known = age_known;
    data->received_utc = received_utc;
    if (state) *state = expired ? WRISTFLOW_WEATHER_STALE : WRISTFLOW_WEATHER_READY;
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

    if (expired) snprintf(data->updated, sizeof data->updated, "天气已过期");
    else if (!age_known) snprintf(data->updated, sizeof data->updated, "接收时间未知");
    else if (age_ms >= 3600000U) snprintf(data->updated, sizeof data->updated, "%u小时前接收", (unsigned)(age_ms / 3600000U));
    else if (age_ms >= 60000U) snprintf(data->updated, sizeof data->updated, "%u分钟前接收", (unsigned)(age_ms / 60000U));
    else snprintf(data->updated, sizeof data->updated, "刚刚接收");

    if (forecasts_old) {
        memset(current.extra.hourly, 0, sizeof current.extra.hourly);
        memset(current.extra.daily, 0, sizeof current.extra.daily);
    }
    if (solar_old) current.extra.sunrise = current.extra.sunset = 0;

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
