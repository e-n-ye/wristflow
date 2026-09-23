#include "clock_format.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    for (unsigned int time = 0; time < 24 * 60; ++time) {
        wristflow_clock_text_t value;
        unsigned int hour = time / 60;
        unsigned int minute = time % 60;
        char expected[3];
        snprintf(expected, sizeof(expected), "%02u", minute);
        assert(wristflow_clock_format(hour, minute, &value));
        assert(value.hour_12 == (hour == 0 ? 12 : hour <= 12 ? hour : hour - 12));
        assert(strcmp(value.minute_text, expected) == 0);
    }
    wristflow_clock_text_t prior = {7, "09"};
    assert(!wristflow_clock_format(24, 0, &prior));
    assert(!wristflow_clock_format(0, 60, &prior));
    assert(!wristflow_clock_format(0, 0, NULL));
    assert(prior.hour_12 == 7 && strcmp(prior.minute_text, "09") == 0);
    puts("PASS: all 1440 minutes, midnight/noon, padding and invalid input");
    return 0;
}
