#ifndef WRISTFLOW_AUTO_BRIGHTNESS_H
#define WRISTFLOW_AUTO_BRIGHTNESS_H
#include <stdbool.h>
#include <stdint.h>

/* Lite-On Appendix A Rev1.0, fixed 1x/100ms only. Uncalibrated, no window
 * compensation. The Appendix's high-ratio branch estimates zero; this is
 * not a claim that the environment is physically dark. */
bool wf_light_millilux(uint16_t ch0, uint16_t ch1, uint32_t *millilux);
uint8_t wf_auto_brightness_target(uint32_t millilux);
typedef struct { uint8_t value, darker_samples; bool valid; } wf_auto_brightness_t;
/* Caller supplies each new valid frame exactly once. Brighten immediately;
 * dim after three consecutive lower frames, at most five points per frame. */
uint8_t wf_auto_brightness_observe(wf_auto_brightness_t *, uint32_t millilux);
#endif
