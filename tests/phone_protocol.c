#include "phone_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static wf_phone_t phone;
static unsigned time_events, gps_events;
static void observed(wf_phone_event_t e, int32_t id, void *context)
{
    (void)id; (void)context;
    if (e == WF_PHONE_TIME) ++time_events;
    if (e == WF_PHONE_GPS_QUERY) ++gps_events;
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
    feed("GB({\"t\":\"weather\",\"temp\":22})\n"); assert(phone.unknown == 1);

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
