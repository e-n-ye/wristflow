#ifndef WRISTFLOW_COMPASS_PROBE_H
#define WRISTFLOW_COMPASS_PROBE_H
#include <stdbool.h>
#include <stdint.h>

/* Explicit, bounded experiment only. One instance, called by the sensor worker.
 * Axis maps are hypotheses, never an assertion of verified body-frame north.
 * No Flash, sensor ownership, I/O, UI publication or implicit restart. */
typedef enum {
    WF_PROBE_NONE, WF_PROBE_CANCELLED, WF_PROBE_DEADLINE,
    WF_PROBE_IO, WF_PROBE_LIBRARY, WF_PROBE_SATURATED, WF_PROBE_CONFIG
} wf_compass_probe_end_t;
typedef struct {
    uint32_t mag_ms, acc_begin_ms, acc_end_ms;
    int16_t acceleration[3];
    int32_t magnetic[3];
    uint8_t bank, xl, ctrl3, status, transfers, source; /* 1 direct, 2 existing wrist frame. */
} wf_compass_probe_frame_t;
typedef struct {
    bool active, algorithm, frozen, output_valid, projected_valid, parameters_valid;
    wf_compass_probe_end_t ended;
    uint32_t session, generation, started_ms, duration_ms, frames, rejected, updates;
    wf_compass_probe_frame_t frame;
    int8_t acc_axes[3], mag_axes[3];
    int accuracy, saturation;
    float acceleration_g[3], magnetic_gauss[3], corrected_gauss[3];
    float parameters[4], orientation[3], projected_degrees;
} wf_compass_probe_snapshot_t;
typedef struct { wf_compass_probe_snapshot_t snapshot; } wf_compass_probe_t;

bool wf_compass_probe_begin(wf_compass_probe_t *, uint32_t generation, uint32_t now_ms,
    unsigned seconds, bool algorithm, const int8_t acc_axes[3], const int8_t mag_axes[3]);
void wf_compass_probe_stop(wf_compass_probe_t *, wf_compass_probe_end_t);
void wf_compass_probe_tick(wf_compass_probe_t *, uint32_t now_ms);
bool wf_compass_probe_freeze(wf_compass_probe_t *);
/* All five fields must have successful reads; XLDA must be set. Pair bound is on sequential
 * read-completion times, not simultaneous hardware sample timestamps. */
bool wf_compass_probe_feed(wf_compass_probe_t *, const wf_compass_probe_frame_t *, uint32_t now_ms);
bool wf_compass_probe_axes_valid(const int8_t axes[3]);
bool wf_compass_probe_project(const float acceleration[3], const float field[3], float *degrees);
#endif
