#ifndef WRISTFLOW_WEATHER_H
#define WRISTFLOW_WEATHER_H

#include <stdbool.h>
#include <stdint.h>

#define WRISTFLOW_WEATHER_HOURLY_COUNT 24U
#define WRISTFLOW_WEATHER_DAILY_COUNT 7U
#define WRISTFLOW_WEATHER_INDEX_COUNT 4U
#define WRISTFLOW_WEATHER_EXPIRY_MS (3ULL * 60U * 60U * 1000U)
#define WRISTFLOW_WEATHER_REQUEST_MS 30000U

enum { WRISTFLOW_WEATHER_FORECAST_UPDATE = 1U, WRISTFLOW_WEATHER_SOLAR_UPDATE = 2U };

typedef enum {
    WRISTFLOW_WEATHER_REQUEST_IDLE,
    WRISTFLOW_WEATHER_REQUEST_PENDING,
    WRISTFLOW_WEATHER_REQUEST_FAILED,
    WRISTFLOW_WEATHER_REQUEST_TIMEOUT
} wristflow_weather_request_state_t;

typedef struct {
    uint64_t monotonic_ms;
    uint32_t utc_seconds; /* Zero means RTC time is unavailable. */
} wristflow_weather_time_t;
typedef wristflow_weather_time_t (*wristflow_weather_clock_t)(void *context);
typedef bool (*wristflow_weather_sender_t)(uint32_t request, void *context);
typedef uint32_t (*wristflow_weather_peer_t)(void *context);

typedef enum {
    WRISTFLOW_WEATHER_LOADING,
    WRISTFLOW_WEATHER_READY,
    WRISTFLOW_WEATHER_EMPTY,
    WRISTFLOW_WEATHER_ERROR,
    WRISTFLOW_WEATHER_STALE
} wristflow_weather_state_t;

typedef enum {
    WRISTFLOW_WEATHER_THEME_CLOUDY,
    WRISTFLOW_WEATHER_THEME_SUNNY
} wristflow_weather_theme_t;

typedef struct {
    bool valid;
    char time[6];
    int8_t temperature;
    char icon[8];
    char wind[12];
    char air[8];
} wristflow_weather_hour_t;

typedef struct {
    bool valid;
    char day[8];
    char icon[8];
    int8_t high;
    int8_t low;
} wristflow_weather_day_t;

typedef struct {
    char value[12];
    char label[24];
} wristflow_weather_index_t;

typedef struct {
    bool expired;
    bool age_known;
    uint32_t received_utc;
    wristflow_weather_request_state_t request_state;
    bool temperature_valid;
    bool range_valid;
    bool sun_position_valid;
    char city[16];
    char updated[24];
    int8_t temperature;
    int8_t high;
    int8_t low;
    char condition[16];
    wristflow_weather_theme_t theme;
    char aqi[8];
    wristflow_weather_hour_t hourly[WRISTFLOW_WEATHER_HOURLY_COUNT];
    wristflow_weather_day_t daily[WRISTFLOW_WEATHER_DAILY_COUNT];
    wristflow_weather_index_t indices[WRISTFLOW_WEATHER_INDEX_COUNT];
    char sunrise[6];
    char sunset[6];
    char message[32];
} wristflow_weather_data_t;

typedef struct {
    bool valid;
    uint32_t timestamp;
    int8_t temperature;
    int16_t code;
    uint8_t wind_kmh;
} wristflow_phone_weather_hour_t;

typedef struct {
    bool valid;
    int8_t high, low;
    int16_t code;
} wristflow_phone_weather_day_t;

typedef struct {
    bool forecast_present;
    uint32_t sunrise, sunset;
    wristflow_phone_weather_hour_t hourly[WRISTFLOW_WEATHER_HOURLY_COUNT];
    wristflow_phone_weather_day_t daily[WRISTFLOW_WEATHER_DAILY_COUNT];
} wristflow_phone_weather_extra_t;

typedef struct {
    char city[16];
    int8_t temp;          /* Celsius */
    int8_t high;
    int8_t low;
    bool range_valid;
    int16_t code;         /* OWM weather code e.g. 800, or -1 if unknown */
    char condition[16];   /* text description e.g. "晴", "多云", "Rain" */
    uint8_t humidity;     /* % e.g. 65 */
    bool humidity_valid;
    uint8_t uv_tenths;
    bool uv_valid;
    char wind[12];        /* wind description e.g. "3m/s" or "3级" */
    wristflow_phone_weather_extra_t extra;
} wristflow_phone_weather_t;

typedef void (*wristflow_weather_lock_t)(void *context);
/* Configure once before any consumers start; callbacks protect all cache access. */
void wristflow_weather_set_lock(wristflow_weather_lock_t lock, wristflow_weather_lock_t unlock, void *context);
/* Configure at startup. Called under the cache lock; must not re-enter weather. */
void wristflow_weather_set_clock(wristflow_weather_clock_t clock, void *context);
void wristflow_weather_set_sender(wristflow_weather_sender_t sender, void *context);
/* A cheap transport identity read; no locks or weather re-entry. */
void wristflow_weather_set_peer(wristflow_weather_peer_t peer, void *context);
bool wristflow_weather_request_sync(void);
void wristflow_weather_reset(void);
/* Flags identify newly received extras, rather than same-source inherited data. */
void wristflow_weather_update(const wristflow_phone_weather_t *update, unsigned updates);
bool wristflow_weather_get_current(wristflow_weather_data_t *data, wristflow_weather_state_t *state);
/* Coalesces a pending request without extending its deadline. IDs survive reset. */
uint32_t wristflow_weather_request_begin(bool *started);
void wristflow_weather_request_failed(uint32_t request);
void wristflow_weather_request_disconnected(void);
bool wristflow_weather_request_pending(uint32_t request);
uint32_t wristflow_weather_request_remaining_ms(void);
bool wristflow_weather_poll(void);
wristflow_weather_theme_t wristflow_weather_determine_theme(int16_t code, const char *txt);
bool wristflow_weather_has_data(void);

const char *wristflow_weather_state_message(wristflow_weather_state_t state);

#endif
