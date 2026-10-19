#ifndef WRISTFLOW_DAILY_STEPS_H
#define WRISTFLOW_DAILY_STEPS_H
#include "activity.h"

/* Native counter observations must arrive within five minutes. At 26 Hz the
 * counter cannot make a complete 16-bit turn in this window. This is a data
 * continuity guard, not a step recognition or accuracy filter. */
#define WRISTFLOW_STEPS_MAX_GAP_MS 300000U
typedef struct {
    uint64_t monotonic_ms;
    uint32_t utc, revision;
    bool valid;
} wristflow_steps_clock_t;
typedef struct {
    wristflow_activity_metric_t metric;
    wristflow_steps_observation_t observation;
} wristflow_steps_snapshot_t;
typedef struct {
    wristflow_steps_snapshot_t snapshot;
    wristflow_steps_clock_t previous;
    uint16_t previous_native;
    bool baseline;
} wristflow_daily_steps_t;

void wristflow_daily_steps_init(wristflow_daily_steps_t *);
/* False means continuity is lost: caller must stop the sensor and publish
 * unknown until an explicit recovery. Calendar changes simply rebaseline. */
bool wristflow_daily_steps_observe(wristflow_daily_steps_t *, uint16_t native,
    wristflow_steps_clock_t clock);
void wristflow_daily_steps_invalidate(wristflow_daily_steps_t *);
#endif
