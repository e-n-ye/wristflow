#ifndef WRISTFLOW_IMU_PEDOMETER_H
#define WRISTFLOW_IMU_PEDOMETER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    void *context;
    bool (*read)(void *, uint8_t, uint8_t *, size_t);
    bool (*write)(void *, uint8_t, uint8_t);
    uint32_t (*now_ms)(void *);
    bool (*cancelled)(void *);
} wf_imu_io_t;
typedef enum { WF_IMU_UNPROBED, WF_IMU_READY, WF_IMU_RUNNING, WF_IMU_DONE, WF_IMU_FAULT } wf_imu_phase_t;
typedef enum { WF_IMU_NO_END, WF_IMU_USER_STOP, WF_IMU_DEADLINE, WF_IMU_IO_FAILURE } wf_imu_end_t;
typedef struct {
    wf_imu_phase_t phase;
    wf_imu_end_t ended;
    uint32_t started_ms, duration_ms, sampled_ms, io_errors, incoherent_reads;
    uint16_t steps;
    uint8_t who, threshold, debounce;
    bool counter_valid, stop_verified;
} wf_imu_steps_snapshot_t;
typedef struct {
    wf_imu_io_t io;
    wf_imu_steps_snapshot_t snapshot;
    bool identified;
    bool budget_active;
    bool continuous;
} wf_imu_pedometer_t;

/* One caller owns the device and register bank. No recognition algorithm here:
 * only configure and read the LSM6DS3TR-C embedded step counter (AN5130 6.1). */
void wf_imu_pedometer_init(wf_imu_pedometer_t *, wf_imu_io_t);
bool wf_imu_pedometer_probe(wf_imu_pedometer_t *);
bool wf_imu_pedometer_begin(wf_imu_pedometer_t *, uint32_t now_ms, uint32_t duration_ms);
/* One explicit reset when the single owner enters daily mode, then read-only
 * counter observations. Diagnostic begin retains its 1..120 second limit. */
bool wf_imu_pedometer_begin_continuous(wf_imu_pedometer_t *);
void wf_imu_pedometer_poll(wf_imu_pedometer_t *, uint32_t now_ms);
bool wf_imu_pedometer_stop(wf_imu_pedometer_t *, uint32_t now_ms, wf_imu_end_t);
#endif
