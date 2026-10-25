#ifndef WRISTFLOW_COMPASS_H
#define WRISTFLOW_COMPASS_H
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WF_COMPASS_WAITING, WF_COMPASS_BUSY, WF_COMPASS_IO_FAILURE,
    WF_COMPASS_SELFTEST_FAILURE, WF_COMPASS_INVALID_FIELD, WF_COMPASS_STALE
} wf_compass_reason_t;
typedef struct {
    bool requested, active, valid, fault, cleanup_pending;
    bool selftest_passed, stopped;
    uint16_t heading_degrees;
    wf_compass_reason_t reason;
    uint32_t generation, session, sequence, sampled_ms, io_errors;
    int32_t x, y, z;
} wf_compass_snapshot_t;

/* Uncalibrated XY projection only. atan2(X,Y) follows the observed clockwise
 * rotation sign; the mounting zero, magnetic calibration and tilt are unverified. */
bool wf_compass_raw_heading(int32_t x, int32_t y, uint16_t *degrees);
#endif
