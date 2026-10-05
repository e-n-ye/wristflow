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
    WRISTFLOW_WEATHER_FIXTURE_CLOUDY,
    WRISTFLOW_WEATHER_FIXTURE_SUNNY
} wristflow_weather_fixture_t;

typedef struct {
    char time[6];
    int8_t temperature;
    char icon[8];
    char wind[12];
    char air[8];
} wristflow_weather_hour_t;

typedef struct {
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
    char city[16];
    char updated[24];
    int8_t temperature;
    int8_t high;
    int8_t low;
    char condition[16];
    wristflow_weather_fixture_t fixture;
    char aqi[8];
    wristflow_weather_hour_t hourly[WRISTFLOW_WEATHER_HOURLY_COUNT];
    wristflow_weather_day_t daily[WRISTFLOW_WEATHER_DAILY_COUNT];
    wristflow_weather_index_t indices[WRISTFLOW_WEATHER_INDEX_COUNT];
    char sunrise[6];
    char sunset[6];
    char message[32];
} wristflow_weather_data_t;

/* Deterministic local fixture until the phone weather provider is connected. */
bool wristflow_weather_provider_read(wristflow_weather_state_t state,
                                      wristflow_weather_data_t *data);
bool wristflow_weather_provider_read_fixture(wristflow_weather_state_t state,
                                              wristflow_weather_fixture_t fixture,
                                              wristflow_weather_data_t *data);
const char *wristflow_weather_state_message(wristflow_weather_state_t state);

#endif
