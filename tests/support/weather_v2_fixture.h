#ifndef WRISTFLOW_TEST_WEATHER_V2_FIXTURE_H
#define WRISTFLOW_TEST_WEATHER_V2_FIXTURE_H
#include "mbedtls/base64.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Independent wire vector for Gadgetbridge 0.94.0's six forecast arrays.
 * Sunrise/sunset: 2026-10-08 05:54/17:32 UTC+8; hours start at 23:00. */
static const unsigned char weather_v2_wire[] = {
    20,25,18,68,10,42,192,208,2,90,0,15,146,39,20,160,134,1,0,
    120,191,198,106,16,99,199,106,0,0,0,0,0,0,0,0,0,0,19,
    4,240,175,199,106,
    0,10,20,30, 20,19,0,251, 192,193,96,255, 7,8,0,15, 45,45,0,90, 0,10,100,30,
    7,
    26,25,24,23,22,21,20, 18,17,16,15,14,13,12, 192,193,194,195,196,96,255,
    7,8,9,10,11,12,13, 45,45,45,45,45,45,45, 10,20,30,40,50,60,70
};

static void weather_v2_frame(char *frame, size_t capacity, const unsigned char *wire, size_t size, const char *location)
{
    unsigned char encoded[400];
    size_t length;
    assert(mbedtls_base64_encode(encoded, sizeof encoded, &length, wire, size) == 0);
    encoded[length] = 0;
    int used = snprintf(frame, capacity, "GB({\"t\":\"weather\",\"v\":2,\"l\":\"%s\",\"c\":\"晴朗\",\"d\":\"%s\\n\"})\n", location, encoded);
    assert(used > 0 && (size_t)used < capacity);
}
#endif
