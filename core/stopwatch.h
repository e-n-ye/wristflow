#ifndef WRISTFLOW_STOPWATCH_H
#define WRISTFLOW_STOPWATCH_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t elapsed_ms;
    uint32_t last_tick;
    bool running;
} wristflow_stopwatch_t;

/* Supply monotonic ticks at least once per uint32_t wrap (49 days). */
void wristflow_stopwatch_update(wristflow_stopwatch_t *watch, uint32_t now);
void wristflow_stopwatch_toggle(wristflow_stopwatch_t *watch, uint32_t now);
void wristflow_stopwatch_reset(wristflow_stopwatch_t *watch, uint32_t now);
#endif
