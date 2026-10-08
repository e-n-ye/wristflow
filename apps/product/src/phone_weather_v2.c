#include "phone_weather_v2.h"
#include "product_state.h"
#include "mbedtls/base64.h"
#include <stdio.h>
#include <string.h>

#define V2_HEADER_SIZE 38U
#define V2_MAX_SIZE 236U
#define V2_MAX_BASE64 316U

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static bool temperature(unsigned char byte, int8_t *result)
{
    int value = byte < 128 ? byte : (int)byte - 256;
    if (value < -60 || value > 70) return false;
    *result = (int8_t)value;
    return true;
}

static bool timestamp(uint32_t value)
{
    return value >= WRISTFLOW_TIME_MIN && value <= WRISTFLOW_TIME_MAX;
}

static int16_t condition(unsigned char code)
{
    static const int16_t thunder[] = {200,201,202,210,211,212,221,230,231,232};
    static const int16_t drizzle[] = {300,301,302,310,311,312,313,314,321};
    static const int16_t rain[] = {500,501,502,503,504,511,520,521,522,531};
    static const int16_t snow[] = {600,601,602,611,612,613,615,616,620,621,622};
    static const int16_t mist[] = {701,711,721,731,741,751,761,762,771,781};
    static const int16_t cloud[] = {800,801,802,803,804};
    const int16_t *values = NULL;
    unsigned count = 0, base = 0;
    if (code < 32) { values = thunder; count = 10; }
    else if (code < 64) { values = drizzle; count = 9; base = 32; }
    else if (code >= 96 && code < 128) { values = rain; count = 10; base = 96; }
    else if (code < 160 && code >= 128) { values = snow; count = 11; base = 128; }
    else if (code < 192 && code >= 160) { values = mist; count = 10; base = 160; }
    else if (code >= 192) { values = cloud; count = 5; base = 192; }
    return values && code - base < count ? values[code - base] : -1;
}

bool wf_phone_weather_v2_decode(const char *encoded, wristflow_phone_weather_t *w)
{
    char clean[V2_MAX_BASE64 + 1];
    unsigned char bytes[V2_MAX_SIZE];
    size_t size = 0, length = 0;
    if (!encoded || !w) return false;
    /* Android Base64.DEFAULT inserts line breaks. Require complete padded groups
     * before calling the SDK decoder, which also accepts incomplete groups. */
    for (const char *p = encoded; *p; ++p) {
        if (*p == '\r' || *p == '\n') continue;
        if (length == V2_MAX_BASE64) return false;
        clean[length++] = *p;
    }
    if (!length || length % 4) return false;
    clean[length] = 0;
    size_t padding = clean[length - 1] == '=' ? 1U : 0U;
    if (padding && clean[length - 2] == '=') ++padding;
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (size_t i = 0; i < length - padding; ++i)
        if (!strchr(alphabet, clean[i])) return false;
    const char *last = strchr(alphabet, clean[length - padding - 1]);
    if (padding && ((unsigned)(last - alphabet) & (padding == 2 ? 15U : 3U))) return false;
    if (mbedtls_base64_decode(bytes, sizeof bytes, &size, (const unsigned char *)clean, length) ||
        size < V2_HEADER_SIZE) return false;

    if (!temperature(bytes[0], &w->temp)) return false;
    w->range_valid = temperature(bytes[1], &w->high) && temperature(bytes[2], &w->low) && w->high >= w->low;
    w->code = condition(bytes[6]);
    w->humidity_valid = bytes[3] > 0 && bytes[3] <= 100;
    w->humidity = bytes[3];
    w->uv_valid = bytes[5] > 0 && bytes[5] <= 200;
    w->uv_tenths = bytes[5];
    unsigned wind = bytes[7] | (unsigned)bytes[8] << 8;
    if (wind) snprintf(w->wind, sizeof w->wind, "%.0f km/h", wind / 100.0);
    uint32_t rise = le32(bytes + 19), set = le32(bytes + 23);
    w->extra.sunrise = timestamp(rise) ? rise : 0;
    w->extra.sunset = timestamp(set) ? set : 0;
    if (w->extra.sunrise && w->extra.sunset && rise >= set) w->extra.sunrise = w->extra.sunset = 0;
    if (size == V2_HEADER_SIZE) return true;

    unsigned hours = bytes[38];
    if (hours > 25) return false;
    size_t start = hours ? 43U : 39U;
    size_t daily_count_at = start + 6U * hours;
    if (daily_count_at >= size) return false;
    unsigned days = bytes[daily_count_at];
    if (days > WRISTFLOW_WEATHER_DAILY_COUNT || daily_count_at + 1U + 6U * days != size) return false;
    w->extra.forecast_present = true;
    uint32_t first = hours ? le32(bytes + 39) : 0;
    bool hour_times_valid = timestamp(first) && bytes[start] == 0;
    for (unsigned i = 1; i < hours; ++i)
        if (bytes[start + i] <= bytes[start + i - 1]) hour_times_valid = false;
    for (unsigned i = 0; i < hours; ++i) {
        unsigned delta = bytes[start + i];
        uint64_t time = (uint64_t)first + delta * 360U;
        if (i >= WRISTFLOW_WEATHER_HOURLY_COUNT) continue;
        wristflow_phone_weather_hour_t *h = &w->extra.hourly[i];
        h->timestamp = time <= UINT32_MAX ? (uint32_t)time : 0;
        h->valid = hour_times_valid && timestamp(h->timestamp) && temperature(bytes[start + hours + i], &h->temperature);
        h->code = condition(bytes[start + 2U * hours + i]);
        h->wind_kmh = bytes[start + 3U * hours + i];
    }
    start = daily_count_at + 1U;
    for (unsigned i = 0; i < days; ++i) {
        wristflow_phone_weather_day_t *d = &w->extra.daily[i];
        d->valid = temperature(bytes[start + i], &d->high) && temperature(bytes[start + days + i], &d->low) && d->high >= d->low;
        d->code = condition(bytes[start + 2U * days + i]);
    }
    return true;
}
