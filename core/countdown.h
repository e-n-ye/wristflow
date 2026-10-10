#ifndef WRISTFLOW_COUNTDOWN_H
#define WRISTFLOW_COUNTDOWN_H

#include <stdbool.h>
#include <stdint.h>

#define WRISTFLOW_COUNTDOWN_MAX_SECONDS 86399U

typedef enum {
    WRISTFLOW_COUNTDOWN_IDLE,
    WRISTFLOW_COUNTDOWN_RUNNING,
    WRISTFLOW_COUNTDOWN_PAUSED,
    WRISTFLOW_COUNTDOWN_EXPIRED
} wristflow_countdown_phase_t;

/* One volatile task. Supply a monotonic millisecond clock, independent of the
 * view and wall clock. Unsigned subtraction supports one 32-bit tick wrap;
 * callers must observe a running task at least once per full clock cycle. */
typedef struct {
    wristflow_countdown_phase_t phase;
    uint32_t duration_ms;
    uint32_t remaining_ms;
    uint32_t anchor_ms;
    uint32_t generation;
    bool expiry_pending;
} wristflow_countdown_t;

void wristflow_countdown_init(wristflow_countdown_t *task);
bool wristflow_countdown_start(wristflow_countdown_t *task, uint32_t seconds, uint32_t now);
uint32_t wristflow_countdown_remaining(const wristflow_countdown_t *task, uint32_t now);
/* Returns true only on the running -> expired transition. */
bool wristflow_countdown_update(wristflow_countdown_t *task, uint32_t now);
bool wristflow_countdown_pause(wristflow_countdown_t *task, uint32_t now);
bool wristflow_countdown_resume(wristflow_countdown_t *task, uint32_t now);
void wristflow_countdown_cancel(wristflow_countdown_t *task);
bool wristflow_countdown_repeat(wristflow_countdown_t *task, uint32_t now);
/* A scheduled callback carries its generation; stale callbacks do no work. */
bool wristflow_countdown_dispatch(wristflow_countdown_t *task, uint32_t generation, uint32_t now);
bool wristflow_countdown_take_expiry(wristflow_countdown_t *task);

#endif
