#ifndef WRISTFLOW_IMU_DEVICE_H
#define WRISTFLOW_IMU_DEVICE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* LSM6DS3TR-C user-bank registers; this ID is shared by some LSM6 variants. */
enum {
    WF_IMU_WHO = 0x0f, WF_IMU_XL = 0x10, WF_IMU_G = 0x11,
    WF_IMU_CTRL3 = 0x12, WF_IMU_CTRL6 = 0x15, WF_IMU_CTRL10 = 0x19,
    WF_IMU_STATUS = 0x1e, WF_IMU_AXES = 0x28, WF_IMU_FUNC = 0x53,
    WF_IMU_TAP = 0x58, WF_IMU_MD1 = 0x5e
};
typedef struct {
    bool (*read)(void *context, uint8_t reg, uint8_t *bytes, size_t count);
    bool (*write)(void *context, uint8_t reg, uint8_t value);
    void (*delay_ms)(void *context, unsigned ms);
    void *context;
    bool identified;
} wf_imu_device_t;

/* All calls are serialized by the product diagnostic thread. */
bool wf_imu_identify(wf_imu_device_t *d, uint8_t *id);
bool wf_imu_prepare(wf_imu_device_t *d);
bool wf_imu_sample_mode(wf_imu_device_t *d);
bool wf_imu_tilt_mode(wf_imu_device_t *d);
bool wf_imu_stop(wf_imu_device_t *d);
/* 1 = fresh sample, 0 = no new data, -1 = bus error. Output untouched on failure. */
int wf_imu_accel(wf_imu_device_t *d, int16_t raw[3]);
#endif
