#include "weather.h"
#include <assert.h>
#include <string.h>

static wristflow_weather_time_t time_now;
static bool locked, queued = true;
static unsigned locks, unlocks, sends;
static uint32_t sent_request;
static uint32_t peer_id;
static uint32_t read_peer(void *context) { (void)context; return peer_id; }
static void lock(void *context)
{
    assert(context == &locked && !locked);
    locked = true; ++locks;
}
static void unlock(void *context)
{
    assert(context == &locked && locked);
    locked = false; ++unlocks;
}
static wristflow_weather_time_t clock_now(void *context)
{
    assert(context == &time_now && locked);
    return time_now;
}
static bool send(uint32_t request, void *context)
{
    assert(context == &queued && !locked);
    ++sends; sent_request = request;
    return queued;
}
static wristflow_weather_data_t snapshot(wristflow_weather_state_t expected, bool available)
{
    wristflow_weather_data_t data;
    wristflow_weather_state_t state;
    assert(wristflow_weather_get_current(&data, &state) == available);
    assert(state == expected);
    assert(locks == unlocks && !locked);
    return data;
}
int main(void)
{
    wristflow_weather_set_lock(lock, unlock, &locked);
    wristflow_weather_set_clock(clock_now, &time_now);
    wristflow_weather_set_sender(send, &queued);
    wristflow_weather_set_peer(read_peer, NULL);
    wristflow_weather_reset();
    time_now.monotonic_ms = 100;
    snapshot(WRISTFLOW_WEATHER_EMPTY, false);
    assert(wristflow_weather_request_sync() && sends == 1);
    uint32_t first = sent_request;
    wristflow_weather_data_t data = snapshot(WRISTFLOW_WEATHER_LOADING, false);
    assert(data.request_state == WRISTFLOW_WEATHER_REQUEST_PENDING);
    time_now.monotonic_ms += WRISTFLOW_WEATHER_REQUEST_MS - 1;
    assert(wristflow_weather_request_sync() && sends == 1);
    assert(wristflow_weather_request_remaining_ms() == 1);
    snapshot(WRISTFLOW_WEATHER_LOADING, false);
    ++time_now.monotonic_ms;
    assert(wristflow_weather_poll());
    data = snapshot(WRISTFLOW_WEATHER_ERROR, false);
    assert(data.request_state == WRISTFLOW_WEATHER_REQUEST_TIMEOUT);
    assert(!strcmp(data.message, "天气同步超时"));
    assert(!wristflow_weather_request_remaining_ms());

    assert(wristflow_weather_request_sync() && sent_request != first);
    wristflow_weather_request_failed(first); /* A late TX result cannot end a new request. */
    snapshot(WRISTFLOW_WEATHER_LOADING, false);
    wristflow_weather_request_disconnected();
    snapshot(WRISTFLOW_WEATHER_ERROR, false);
    /* Both link transitions can precede worker/UI consumption. */
    assert(wristflow_weather_request_sync());
    uint32_t old_peer_request = sent_request;
    peer_id += 2;
    assert(wristflow_weather_poll());
    snapshot(WRISTFLOW_WEATHER_ERROR, false);
    assert(wristflow_weather_request_sync() && sent_request != old_peer_request);
    wristflow_weather_request_failed(old_peer_request);
    snapshot(WRISTFLOW_WEATHER_LOADING, false);
    wristflow_weather_request_disconnected();
    queued = false;
    assert(!wristflow_weather_request_sync());
    data = snapshot(WRISTFLOW_WEATHER_ERROR, false);
    assert(data.request_state == WRISTFLOW_WEATHER_REQUEST_FAILED);
    queued = true;
    assert(wristflow_weather_request_sync());
    uint32_t before_reset = sent_request;
    wristflow_weather_reset();
    assert(wristflow_weather_request_sync() && sent_request != before_reset);
    wristflow_weather_request_failed(before_reset);
    snapshot(WRISTFLOW_WEATHER_LOADING, false);

    wristflow_phone_weather_t weather = {.temp = 0, .code = 800};
    strcpy(weather.city, "Test");
    weather.extra.forecast_present = true;
    weather.extra.hourly[0] = (wristflow_phone_weather_hour_t){.valid = true, .timestamp = 1791471600U};
    weather.extra.daily[0] = (wristflow_phone_weather_day_t){.valid = true, .high = 20};
    weather.extra.sunrise = 1791410040U;
    weather.extra.sunset = 1791451920U;
    uint64_t received = time_now.monotonic_ms;
    wristflow_weather_update(&weather, WRISTFLOW_WEATHER_FORECAST_UPDATE | WRISTFLOW_WEATHER_SOLAR_UPDATE);
    data = snapshot(WRISTFLOW_WEATHER_READY, true);
    assert(data.age_known && !data.received_utc && !data.expired);
    assert(data.request_state == WRISTFLOW_WEATHER_REQUEST_IDLE);
    assert(!strcmp(data.updated, "刚刚接收"));
    assert(data.temperature_valid && data.temperature == 0);

    /* UTC corrections have no effect on monotonic age or the original receipt UTC. */
    time_now.utc_seconds = 1791475200U;
    time_now.monotonic_ms = received + 60000;
    data = snapshot(WRISTFLOW_WEATHER_READY, true);
    assert(!strcmp(data.updated, "1分钟前接收") && !data.received_utc);
    time_now.utc_seconds += 86400;
    time_now.monotonic_ms = received + 3600000;
    data = snapshot(WRISTFLOW_WEATHER_READY, true);
    assert(!strcmp(data.updated, "1小时前接收") && !data.expired);
    time_now.utc_seconds = 0;
    time_now.monotonic_ms = received + WRISTFLOW_WEATHER_EXPIRY_MS - 1;
    data = snapshot(WRISTFLOW_WEATHER_READY, true);
    assert(data.hourly[0].valid && data.daily[0].valid);
    ++time_now.monotonic_ms;
    data = snapshot(WRISTFLOW_WEATHER_STALE, true);
    assert(data.expired && !strcmp(data.updated, "天气已过期"));
    assert(data.temperature_valid && !data.hourly[0].valid && !data.daily[0].valid);
    assert(!strcmp(data.sunrise, "--:--"));

    /* A fresh v1 current update cannot rejuvenate inherited forecasts or solar data. */
    time_now.utc_seconds = 1791475500U;
    wristflow_weather_update(&weather, 0);
    data = snapshot(WRISTFLOW_WEATHER_READY, true);
    assert(data.received_utc == 1791475500U && !data.expired);
    assert(!data.hourly[0].valid && !data.daily[0].valid && !strcmp(data.sunset, "--:--"));
    uint32_t receipt_utc = data.received_utc;
    time_now.utc_seconds -= 86400;
    data = snapshot(WRISTFLOW_WEATHER_READY, true);
    assert(data.received_utc == receipt_utc && !data.expired);
    wristflow_weather_update(&weather, WRISTFLOW_WEATHER_FORECAST_UPDATE | WRISTFLOW_WEATHER_SOLAR_UPDATE);
    data = snapshot(WRISTFLOW_WEATHER_READY, true);
    assert(data.hourly[0].valid && !strcmp(data.sunrise, "05:54"));
    assert(wristflow_weather_request_sync());
    wristflow_weather_request_failed(sent_request);
    data = snapshot(WRISTFLOW_WEATHER_READY, true);
    assert(data.temperature_valid && data.hourly[0].valid && !strcmp(data.message, "天气更新失败"));

    /* The adapter contract supports monotonic milliseconds beyond 32-bit range. */
    time_now.monotonic_ms = UINT32_MAX - 100U;
    wristflow_weather_update(&weather, 0);
    assert(wristflow_weather_request_sync());
    time_now.monotonic_ms += WRISTFLOW_WEATHER_REQUEST_MS;
    data = snapshot(WRISTFLOW_WEATHER_READY, true);
    assert(data.request_state == WRISTFLOW_WEATHER_REQUEST_TIMEOUT);
    time_now.monotonic_ms += WRISTFLOW_WEATHER_EXPIRY_MS;
    snapshot(WRISTFLOW_WEATHER_STALE, true);

    /* Conservative behavior if an adapter's monotonic clock moves backwards. */
    wristflow_weather_update(&weather, 0);
    --time_now.monotonic_ms;
    data = snapshot(WRISTFLOW_WEATHER_STALE, true);
    assert(!data.age_known && data.expired);
    wristflow_weather_set_clock(NULL, NULL);
    wristflow_weather_update(&weather, 0);
    data = snapshot(WRISTFLOW_WEATHER_READY, true);
    assert(!data.age_known && !data.received_utc && !strcmp(data.updated, "接收时间未知"));
    assert(!wristflow_weather_request_sync());
    wristflow_weather_reset();
    assert(locks > 0 && locks == unlocks);
    wristflow_weather_set_sender(NULL, NULL);
    wristflow_weather_set_peer(NULL, NULL);
    wristflow_weather_set_lock(NULL, NULL, NULL);
    return 0;
}
