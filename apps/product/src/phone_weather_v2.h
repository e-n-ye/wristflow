#ifndef WRISTFLOW_PHONE_WEATHER_V2_H
#define WRISTFLOW_PHONE_WEATHER_V2_H
#include "weather.h"

/* Decodes Gadgetbridge 0.94.0 Bangle.js v2; caller commits only on success. */
bool wf_phone_weather_v2_decode(const char *encoded, wristflow_phone_weather_t *weather);
#endif
