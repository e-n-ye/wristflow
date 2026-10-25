#ifndef WRISTFLOW_STOPWATCH_H
#define WRISTFLOW_STOPWATCH_H
#include <stdbool.h>
#include <stdint.h>

#define WRISTFLOW_STOPWATCH_MAX_LAPS 50U
typedef struct {
    uint64_t elapsed_ms;
    uint32_t last_tick;
    bool running;
    unsigned lap_count;
    uint64_t laps[WRISTFLOW_STOPWATCH_MAX_LAPS];
} wristflow_stopwatch_t;

/* Supply monotonic ticks at least once per uint32_t wrap (49 days). */
void wristflow_stopwatch_update(wristflow_stopwatch_t *watch, uint32_t now);
void wristflow_stopwatch_toggle(wristflow_stopwatch_t *watch, uint32_t now);
void wristflow_stopwatch_reset(wristflow_stopwatch_t *watch, uint32_t now);
/* Capture cumulative time without pausing. A full list rejects further laps. */
bool wristflow_stopwatch_lap(wristflow_stopwatch_t *watch, uint32_t now);
#endif
