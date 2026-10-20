#ifndef WRISTFLOW_IMU_WRIST_TILT_H
#define WRISTFLOW_IMU_WRIST_TILT_H
#include "imu_pedometer.h"

/* Bits refer to successful transfers, not matching configuration or fresh data.
 * Each frame is sequential; BDU protects byte pairs, not a simultaneous XYZ. */
enum { WF_WRIST_BANK = 1U, WF_WRIST_WHO = 2U, WF_WRIST_XL = 4U,
    WF_WRIST_CTRL3 = 8U, WF_WRIST_CTRL10 = 16U, WF_WRIST_PULSE = 32U,
    WF_WRIST_TAP = 64U, WF_WRIST_STATUS = 128U, WF_WRIST_RAW = 256U,
    WF_WRIST_AXES = 512U, WF_WRIST_SOURCE = 1024U, WF_WRIST_ALL = 2047U };
typedef struct {
    uint32_t started_ms, completed_ms;
    uint16_t valid;
    uint8_t bank, who, xl, ctrl3, ctrl10, pulse, tap, status, source, axes, raw[6];
    int16_t xyz[3]; /* Native signed counts; only meaningful if xyz_valid. */
    bool attempted, xyz_valid, data_ready;
} wf_imu_wrist_frame_t;

typedef struct {
    wf_imu_phase_t phase;
    wf_imu_end_t ended;
    uint32_t started_ms, duration_ms, sampled_ms, observations, io_errors;
    uint8_t mask, latency, threshold, source, axes;
    bool source_valid, configured, stop_verified;
    wf_imu_wrist_frame_t before, running, after;
} wf_imu_wrist_snapshot_t;
typedef struct {
    wf_imu_io_t io;
    void (*delay_ms)(void *, uint32_t);
    wf_imu_wrist_snapshot_t snapshot;
    uint8_t saved_pulse, saved_tap;
    bool changed;
} wf_imu_wrist_tilt_t;

/* Called only by the existing IMU owner while its 26Hz native counter runs.
 * No acceleration recognition or CPU/GPIO wake policy is implemented here. */
void wf_imu_wrist_init(wf_imu_wrist_tilt_t *, wf_imu_io_t, void (*)(void *, uint32_t));
bool wf_imu_wrist_begin(wf_imu_wrist_tilt_t *, uint32_t, uint8_t, uint8_t);
void wf_imu_wrist_poll(wf_imu_wrist_tilt_t *, uint32_t);
bool wf_imu_wrist_stop(wf_imu_wrist_tilt_t *, uint32_t, wf_imu_end_t, bool);
#endif
