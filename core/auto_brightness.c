#include "auto_brightness.h"
#include <stddef.h>

bool wf_light_millilux(uint16_t ch0, uint16_t ch1, uint32_t *value)
{
    if (!value || ch0 == UINT16_MAX || ch1 == UINT16_MAX) return false;
    uint32_t sum = (uint32_t)ch0 + ch1;
    if (!sum) { *value = 0; return true; }
    int64_t numerator;
    if ((uint32_t)ch1 * 100U < sum * 45U) numerator = 17743LL * ch0 + 11059LL * ch1;
    else if ((uint32_t)ch1 * 100U < sum * 64U) numerator = 42785LL * ch0 - 19548LL * ch1;
    else if ((uint32_t)ch1 * 100U < sum * 85U) numerator = 5926LL * ch0 + 1185LL * ch1;
    else numerator = 0; /* Appendix A ELSE branch; valid low-count 0/1 is not a sensor fault. */
    *value = (uint32_t)((numerator + 5) / 10); /* decimal coefficients -> millilux */
    return true;
}
uint8_t wf_auto_brightness_target(uint32_t value)
{
    static const struct { uint32_t mlux; uint8_t percent; } curve[] = {
        {0,10}, {5000,15}, {20000,25}, {100000,40}, {300000,55},
        {1000000,70}, {5000000,90}, {20000000,100}
    };
    for (size_t i = 1; i < sizeof curve / sizeof *curve; ++i) {
        if (value <= curve[i].mlux) {
            uint32_t delta = value - curve[i-1].mlux;
            return (uint8_t)(curve[i-1].percent +
                (uint64_t)delta * (curve[i].percent - curve[i-1].percent) /
                (curve[i].mlux - curve[i-1].mlux));
        }
    }
    return 100;
}
uint8_t wf_auto_brightness_observe(wf_auto_brightness_t *state, uint32_t mlux)
{
    uint8_t target = wf_auto_brightness_target(mlux);
    if (!state->valid) {
        state->valid = true; state->value = target; state->darker_samples = 0;
    } else if (target >= state->value + 3U) {
        state->value = target; state->darker_samples = 0;
    } else if (target + 3U <= state->value) {
        if (++state->darker_samples >= 3) {
            state->value = state->value - target > 5 ? state->value - 5 : target;
            state->darker_samples = 2;
        }
    } else state->darker_samples = 0;
    return state->value;
}
