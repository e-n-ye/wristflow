#include "clock_format.h"
#include <stddef.h>

bool wristflow_clock_format(unsigned int hour_24, unsigned int minute,
                           wristflow_clock_text_t *output)
{
    if (output == NULL || hour_24 > 23 || minute > 59)
        return false;
    output->hour_12 = (uint8_t)(hour_24 % 12);
    if (output->hour_12 == 0)
        output->hour_12 = 12;
    output->minute_text[0] = (char)('0' + minute / 10);
    output->minute_text[1] = (char)('0' + minute % 10);
    output->minute_text[2] = '\0';
    return true;
}
