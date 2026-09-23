#ifndef WRISTFLOW_CLOCK_FORMAT_H
#define WRISTFLOW_CLOCK_FORMAT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t hour_12;
    char minute_text[3];
} wristflow_clock_text_t;

/* Reject invalid input without modifying the previous display value. */
bool wristflow_clock_format(unsigned int hour_24, unsigned int minute,
                           wristflow_clock_text_t *output);

#endif
