#ifndef WRISTFLOW_WEATHER_H
#define WRISTFLOW_WEATHER_H

#include <stdbool.h>
#include <stdint.h>

#define WRISTFLOW_WEATHER_HOURLY_COUNT 24U
#define WRISTFLOW_WEATHER_DAILY_COUNT 7U
#define WRISTFLOW_WEATHER_INDEX_COUNT 4U

typedef enum {
    WRISTFLOW_WEATHER_LOADING,
    WRISTFLOW_WEATHER_READY,
    WRISTFLOW_WEATHER_EMPTY,
    WRISTFLOW_WEATHER_ERROR
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
    char city[16];
    int8_t temp;          /* Celsius */
    int8_t high;
    int8_t low;
    bool range_valid;
    int16_t code;         /* OWM weather code e.g. 800, or -1 if unknown */
    char condition[16];   /* text description e.g. "晴", "多云", "Rain" */
    uint8_t humidity;     /* % e.g. 65 */
    bool humidity_valid;
    char wind[12];        /* wind description e.g. "3m/s" or "3级" */
    uint32_t timestamp;   /* UTC timestamp in seconds */
} wristflow_phone_weather_t;

void wristflow_weather_reset(void);
void wristflow_weather_update(const wristflow_phone_weather_t *update);
bool wristflow_weather_get_current(wristflow_weather_data_t *data, wristflow_weather_state_t *state, uint32_t now_utc);
wristflow_weather_theme_t wristflow_weather_determine_theme(int16_t code, const char *txt);
bool wristflow_weather_has_data(void);

const char *wristflow_weather_state_message(wristflow_weather_state_t state);

#endif
