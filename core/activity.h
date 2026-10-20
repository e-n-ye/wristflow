#ifndef WRISTFLOW_ACTIVITY_H
#define WRISTFLOW_ACTIVITY_H

#include <stdbool.h>
#include <stdint.h>

/* Display contract only. The producer owns daily aggregation and validity. */
typedef enum {
    WRISTFLOW_ACTIVITY_CALORIES,
    WRISTFLOW_ACTIVITY_STEPS,
    WRISTFLOW_ACTIVITY_MINUTES,
    WRISTFLOW_ACTIVITY_STANDING,
    WRISTFLOW_ACTIVITY_METRICS
} wristflow_activity_metric_id_t;

typedef struct {
    uint32_t total;
    uint32_t goal;
    uint32_t hourly[24];
    uint32_t hours_valid; /* Low24 bits: confirmed partial intervals, not full-hour coverage.
                           * Missing hours are not measured zeroes. */
    bool total_valid;
    bool goal_valid;
} wristflow_activity_metric_t;

typedef struct {
    uint64_t sampled_ms;
    uint32_t version, utc, local_day, coverage_start_utc, clock_revision;
    uint16_t native_count;
    bool native_source; /* LSM6DS3TR-C embedded counter, not a software detector. */
    bool partial_day; /* Confirmed partial intervals; reboot gaps remain unknown. */
    bool restored; /* Includes a matching same-day checkpoint, not native continuity. */
} wristflow_steps_observation_t;

typedef struct {
    wristflow_activity_metric_t metrics[WRISTFLOW_ACTIVITY_METRICS];
    wristflow_steps_observation_t steps_observation;
} wristflow_activity_snapshot_t;

#endif
