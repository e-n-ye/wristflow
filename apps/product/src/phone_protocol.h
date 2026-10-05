#ifndef WRISTFLOW_PHONE_PROTOCOL_H
#define WRISTFLOW_PHONE_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WF_PHONE_LINE 8192
#include "notifications.h"
#include "weather.h"
typedef enum { WF_PHONE_NOTIFY, WF_PHONE_REMOVE, WF_PHONE_TIME, WF_PHONE_GPS_QUERY,
               WF_PHONE_WEATHER, WF_PHONE_REJECT, WF_PHONE_UNKNOWN } wf_phone_event_t;
typedef void (*wf_phone_callback_t)(wf_phone_event_t event, int32_t id, void *context);
/* Single-owner parser. Platform code supplies synchronization for snapshots. */
typedef struct {
    char line[WF_PHONE_LINE], json[WF_PHONE_LINE * 2];
    size_t used;
    bool dropping;
    wf_phone_message_t messages[WF_PHONE_MESSAGES];
    unsigned count, received, updated, removed, rejected, unknown;
    uint32_t utc;
    wristflow_phone_weather_t weather;
    bool has_weather;
    wf_phone_callback_t callback;
    void *context;
} wf_phone_t;
void wf_phone_init(wf_phone_t *phone, wf_phone_callback_t callback, void *context);
void wf_phone_feed(wf_phone_t *phone, const uint8_t *data, size_t size);
/* On a known transport gap, discard through the next newline. */
void wf_phone_gap(wf_phone_t *phone);
/* New connection: partial frames never cross peers; local messages remain. */
void wf_phone_reconnect(wf_phone_t *phone);
bool wf_phone_remove(wf_phone_t *phone, int32_t id);
void wf_phone_clear(wf_phone_t *phone);
#endif
