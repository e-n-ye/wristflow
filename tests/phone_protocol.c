#include "phone_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static wf_phone_t phone;
static unsigned time_events, gps_events, weather_events;
static void observed(wf_phone_event_t e, int32_t id, void *context)
{
    (void)id; (void)context;
    if (e == WF_PHONE_TIME) ++time_events;
    if (e == WF_PHONE_GPS_QUERY) ++gps_events;
    if (e == WF_PHONE_WEATHER) ++weather_events;
}
static void feed(const char *s) { wf_phone_feed(&phone, (const uint8_t *)s, strlen(s)); }
static const char notice[] = "\x10GB({\"t\":\"notify\",\"id\":-42,\"src\":\"Test\",\"title\":\"\\u4e2d\\u6587\",\"body\":\"\\u6d4b\\u8bd5\\n\\ud83d\\ude42\"})\n";
int main(void)
{
    /* Every possible two-fragment split, including between escape digits. */
    for (size_t split = 0; split <= strlen(notice); ++split) {
        wf_phone_init(&phone, observed, NULL);
        wf_phone_feed(&phone, (const uint8_t *)notice, split);
        wf_phone_feed(&phone, (const uint8_t *)notice + split, strlen(notice) - split);
        assert(phone.count == 1 && phone.messages[0].id == -42);
        assert(!strcmp(phone.messages[0].title, "\xe4\xb8\xad\xe6\x96\x87"));
        assert(!strcmp(phone.messages[0].body, "\xe6\xb5\x8b\xe8\xaf\x95\n\xf0\x9f\x99\x82"));
    }
    for (size_t i = 0; i < strlen(notice); ++i) wf_phone_feed(&phone, (const uint8_t *)notice + i, 1);
    assert(phone.count == 1 && phone.updated == 1);
    feed("GB({\"t\":\"notify~\",\"id\":-42,\"body\":\"updated\"})\n");
    assert(phone.updated == 2 && !strcmp(phone.messages[0].body, "updated"));
    assert(!strcmp(phone.messages[0].title, "\xe4\xb8\xad\xe6\x96\x87"));
    feed("GB({\"t\":\"notify-\",\"id\":-42})\nGB({\"t\":\"notify-\",\"id\":-42})\n");
    assert(phone.count == 0 && phone.removed == 1);
    feed(notice);
    assert(wf_phone_remove(&phone, -42));
    feed("GB({\"t\":\"notify~\",\"id\":-42,\"body\":\"stale update\"})\n");
    assert(phone.count == 0);

    feed("\x10setTime(1790481600);E.setTimeZone(8.0);ignored_storage_script\n");
    assert(time_events == 1 && phone.utc == 1790481600U);
    feed("setTime(0);\nsetTime(1790481600.5);\nsetTime(-1);\n");
    assert(time_events == 1);
    feed("GB({\"t\":\"is_gps_active\"})\n"); assert(gps_events == 1);

    /* Weather tests */
    feed("GB({\"t\":\"weather\",\"temp\":295.15,\"hum\":65,\"wind\":\"3.5 km/h\",\"loc\":\"\\u676d\\u5dde\",\"txt\":\"\\u6674\",\"code\":800})\n");
    assert(weather_events == 1 && phone.has_weather);
    assert(phone.weather.temp == 22); /* 295.15 K - 273.15 = 22 C */
    assert(phone.weather.humidity_valid && phone.weather.humidity == 65);
    assert(phone.weather.code == 800);
    assert(!strcmp(phone.weather.city, "\xe6\x9d\xad\xe5\xb7\x9e"));
    assert(!strcmp(phone.weather.condition, "\xe6\x99\xb4"));
    assert(!strcmp(phone.weather.wind, "3.5 km/h"));

    /* Celsius direct feed & numeric wind */
    feed("GB({\"t\":\"weather\",\"temp\":18.4,\"hum\":80,\"wind\":12,\"loc\":\"Shanghai\",\"txt\":\"Clouds\",\"code\":802})\n");
    assert(weather_events == 2 && phone.weather.temp == 18);
    assert(phone.weather.humidity == 80);
    assert(phone.weather.code == 802);
    assert(!strcmp(phone.weather.wind, "12 km/h"));
    assert(!strcmp(phone.weather.city, "Shanghai"));

    /* Raw UTF-8 Chinese characters directly in feed */
    feed("GB({\"t\":\"weather\",\"temp\":25.0,\"hum\":60,\"wind\":\"10 km/h\",\"loc\":\"杭州市\",\"txt\":\"晴\",\"code\":800})\n");
    assert(weather_events == 3 && phone.weather.temp == 25);
    assert(!strcmp(phone.weather.city, "杭州市"));
    assert(!strcmp(phone.weather.condition, "晴"));

    /* Malformed weather without temp fails gracefully */
    unsigned prev_rejected = phone.rejected;
    feed("GB({\"t\":\"weather\",\"loc\":\"Beijing\"})\n");
    assert(phone.rejected == prev_rejected + 1);

    feed("GB({\"t\":\"weather\",\"temp\":0,\"hum\":0})\n");
    assert(phone.weather.temp == 0 && phone.weather.humidity_valid && phone.weather.humidity == 0);
    assert(!phone.weather.city[0] && !phone.weather.condition[0] && phone.weather.code == -1);
    feed("GB({\"t\":\"weather\",\"temp\":-5})\n");
    assert(phone.weather.temp == -5 && !phone.weather.humidity_valid);
    assert(!phone.weather.wind[0] && !phone.weather.condition[0]);
    const char *optional_invalid[] = {
        "GB({\"t\":\"weather\",\"temp\":20,\"hum\":null})\n",
        "GB({\"t\":\"weather\",\"temp\":20,\"hum\":\"0\"})\n",
        "GB({\"t\":\"weather\",\"temp\":20,\"hum\":-1})\n",
        "GB({\"t\":\"weather\",\"temp\":20,\"hum\":101})\n",
        "GB({\"t\":\"weather\",\"temp\":20,\"hum\":1e999})\n"
    };
    for (unsigned i = 0; i < sizeof optional_invalid / sizeof optional_invalid[0]; ++i) {
        feed(optional_invalid[i]);
        assert(phone.weather.temp == 20 && !phone.weather.humidity_valid);
    }
    feed("GB({\"t\":\"weather\",\"temp\":20,\"hum\":100,\"code\":1e9})\n");
    assert(phone.weather.humidity_valid && phone.weather.humidity == 100 && phone.weather.code == -1);
    unsigned before_bad_temp = phone.rejected;
    feed("GB({\"t\":\"weather\",\"temp\":1e9})\n");
    feed("GB({\"t\":\"weather\",\"temp\":-256})\n");
    assert(phone.rejected == before_bad_temp + 2 && phone.weather.temp == 20);


    feed(notice);
    const char *bad[] = {
        "GB({\"t\":\"notify\",\"id\":-42,\"body\":123})\n",
        "GB({\"t\":\"notify\",\"id\":1.5})\n",
        "GB({\"t\":\"notify\",\"id\":2147483648})\n",
        "GB({\"t\":\"notify\",\"id\":1e999})\n",
        "GB({\"t\":\"notify\",\"id\":1,\"id\":2})\n",
        "GB({\"t\":\"notify\",\"id\":1,\"body\":\"\\u0000bad\"})\n",
        "GB({\"t\":\"notify\",\"id\":1,\"body\":\"\\uD800\"})\n",
        "GB({\"t\":\"notify\",\"id\":1,\"body\":atob(\"SGk=\")})\n",
        "GB({\"t\":\"notify\",\"id\":1,\"body\":\"\\x\"})\n",
        "GB({\"t\":\"notify\",\"id\":1}garbage)\n",
        "GB({\"t\":\"notify\",\"id\":1,\"body\":\"\\400\"})\n",
    };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        unsigned before = phone.rejected;
        feed(bad[i]);
        assert(phone.rejected == before + 1 && phone.count == 1 && phone.messages[0].id == -42);
    }
    feed("GB({\"t\":\"notify\",\"id\":9,\"body\":\"\\xe9\\v\\20\\\"\\\\\"})\n");
    assert(!strcmp(phone.messages[0].body, "\xc3\xa9\v\x10\"\\"));
    feed("GB({\"t\":\"notify\",\"id\":10,\"body\":\"\xe9\"})\n");
    assert(!strcmp(phone.messages[0].body, "\xc3\xa9"));

    unsigned count = phone.count;
    feed("GB({\"t\":\"notify\",\"id\":22,\"body\":\"");
    wf_phone_gap(&phone); feed("lost bytes\"})\n");
    assert(phone.count == count);
    feed(notice); assert(phone.messages[0].id == -42);
    feed("GB({\"t\":\"notify\",\"id\":22");
    wf_phone_reconnect(&phone); feed("})\n"); assert(phone.count == count);
    char huge[WF_PHONE_LINE + 50]; memset(huge, 'x', sizeof huge);
    wf_phone_feed(&phone, (uint8_t *)huge, sizeof huge); feed("\n");
    feed(notice); assert(phone.messages[0].id == -42 && !phone.dropping);
    /* Full-frame but overlong text also leaves the store intact. */
    char wide[2100]; strcpy(wide, "GB({\"t\":\"notify\",\"id\":44,\"body\":\"");
    size_t n = strlen(wide); memset(wide + n, 'a', 1700); strcpy(wide + n + 1700, "\"})\n");
    feed(wide); assert(phone.count == count);

    wf_phone_clear(&phone);
    for (int i = 0; i < 15; ++i) {
        char s[96]; snprintf(s, sizeof s, "GB({\"t\":\"notify\",\"id\":%d,\"body\":\"test\"})\n", i); feed(s);
    }
    assert(phone.count == 10 && phone.messages[0].id == 14 && phone.messages[9].id == 5);
    feed("GB({\"t\":\"notify\",\"id\":7,\"body\":\"new\"})\n");
    assert(phone.count == 10 && phone.messages[0].id == 7 && phone.messages[9].id == 5);
    puts("phone_protocol: fragmentation, Unicode, lifecycle, bounds and recovery PASS");
    return 0;
}
