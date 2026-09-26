#ifndef WRISTFLOW_DISPLAY_POLICY_H
#define WRISTFLOW_DISPLAY_POLICY_H
#include <stdbool.h>
#include <stdint.h>

typedef enum { WRISTFLOW_DISPLAY_ACTIVE, WRISTFLOW_DISPLAY_DIM, WRISTFLOW_DISPLAY_OFF } wristflow_display_phase_t;
typedef struct {
    wristflow_display_phase_t phase;
    uint32_t activity_at, off_at, keep_at, keep_ms;
    bool flashlight, swallow_touch;
} wristflow_display_policy_t;

void wristflow_display_init(wristflow_display_policy_t *policy, uint32_t now);
void wristflow_display_activity(wristflow_display_policy_t *policy, uint32_t now);
void wristflow_display_tick(wristflow_display_policy_t *policy, uint32_t now, unsigned timeout, bool flashlight);
bool wristflow_display_keep(wristflow_display_policy_t *policy, uint32_t now, unsigned minutes);
/* Return true only when this input may reach the UI. Off-screen touch cannot wake yet. */
bool wristflow_display_touch(wristflow_display_policy_t *policy, uint32_t now, bool pressed);
bool wristflow_display_key(wristflow_display_policy_t *policy, uint32_t now);
uint8_t wristflow_display_brightness(const wristflow_display_policy_t *policy, uint8_t preference);
#endif
