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
    uint32_t hours_valid; /* Low 24 bits; missing hours are not measured zeroes. */
    bool total_valid;
    bool goal_valid;
} wristflow_activity_metric_t;

typedef struct {
    wristflow_activity_metric_t metrics[WRISTFLOW_ACTIVITY_METRICS];
} wristflow_activity_snapshot_t;

#endif
