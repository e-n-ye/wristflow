#include "display_policy.h"

void wristflow_display_init(wristflow_display_policy_t *p, uint32_t now)
{ *p = (wristflow_display_policy_t){.activity_at = now}; }

void wristflow_display_activity(wristflow_display_policy_t *p, uint32_t now)
{ p->activity_at = now; p->phase = WRISTFLOW_DISPLAY_ACTIVE; }

bool wristflow_display_keep(wristflow_display_policy_t *p, uint32_t now, unsigned minutes)
{
    if (minutes != 0 && minutes != 5 && minutes != 10 && minutes != 15 && minutes != 20) return false;
    p->keep_at = now;
    p->keep_ms = minutes * 60000U;
    wristflow_display_activity(p, now);
    return true;
}

void wristflow_display_tick(wristflow_display_policy_t *p, uint32_t now, unsigned timeout, bool flashlight)
{
    if (p->keep_ms && now - p->keep_at >= p->keep_ms) {
        p->keep_ms = 0;
        wristflow_display_activity(p, now);
    }
    if (flashlight != p->flashlight) {
        p->flashlight = flashlight;
        wristflow_display_activity(p, now);
    }
    if (p->flashlight || p->keep_ms || p->phase == WRISTFLOW_DISPLAY_OFF) return;
    uint32_t idle = now - p->activity_at;
    if (idle >= timeout * 1000U) {
        p->phase = WRISTFLOW_DISPLAY_OFF;
        p->off_at = now;
    } else if (idle >= (timeout == 5 ? 4000U : (timeout - 5U) * 1000U)) {
        p->phase = WRISTFLOW_DISPLAY_DIM;
    }
}

bool wristflow_display_touch(wristflow_display_policy_t *p, uint32_t now, bool pressed)
{
    if (p->swallow_touch) {
        if (!pressed) p->swallow_touch = false;
        else if (p->phase != WRISTFLOW_DISPLAY_OFF) wristflow_display_activity(p, now);
        return false;
    }
    if (p->phase == WRISTFLOW_DISPLAY_OFF) {
        p->swallow_touch = pressed;
        return false;
    }
    if (pressed && p->phase == WRISTFLOW_DISPLAY_DIM) {
        p->swallow_touch = true;
        wristflow_display_activity(p, now);
        return false;
    }
    if (pressed) wristflow_display_activity(p, now);
    return true;
}

bool wristflow_display_key(wristflow_display_policy_t *p, uint32_t now)
{
    bool pass = p->phase == WRISTFLOW_DISPLAY_ACTIVE;
    wristflow_display_activity(p, now);
    return pass;
}

uint8_t wristflow_display_brightness(const wristflow_display_policy_t *p, uint8_t preference)
{
    if (p->phase == WRISTFLOW_DISPLAY_OFF) return 0;
    if (p->flashlight) return 100;
    return p->phase == WRISTFLOW_DISPLAY_DIM ? (uint8_t)((preference + 2U) / 5U) : preference;
}
