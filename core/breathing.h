#ifndef WRISTFLOW_BREATHING_H
#define WRISTFLOW_BREATHING_H
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WRISTFLOW_BREATHING_IDLE, WRISTFLOW_BREATHING_RUNNING,
    WRISTFLOW_BREATHING_PAUSED, WRISTFLOW_BREATHING_COMPLETE
} wristflow_breathing_phase_t;

/* Foreground session only. The caller owns the clock and suspension reasons. */
typedef struct {
    wristflow_breathing_phase_t phase;
    uint8_t minutes;
    uint32_t elapsed_ms, last_tick, rhythm_ms;
} wristflow_breathing_t;

void wristflow_breathing_init(wristflow_breathing_t *session);
bool wristflow_breathing_set_minutes(wristflow_breathing_t *session, unsigned minutes);
void wristflow_breathing_start(wristflow_breathing_t *session, uint32_t now);
void wristflow_breathing_update(wristflow_breathing_t *session, uint32_t now);
void wristflow_breathing_pause(wristflow_breathing_t *session, uint32_t now);
void wristflow_breathing_resume(wristflow_breathing_t *session, uint32_t now);
void wristflow_breathing_end(wristflow_breathing_t *session);
bool wristflow_breathing_inhaling(const wristflow_breathing_t *session);
/* 0..1000, small..large. Rational timing avoids truncating 60000/14. */
unsigned wristflow_breathing_expansion(const wristflow_breathing_t *session);
#endif
