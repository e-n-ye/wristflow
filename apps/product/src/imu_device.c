#include "imu_device.h"

static bool set(wf_imu_device_t *d, uint8_t reg, uint8_t value)
{
    uint8_t actual;
    return d->write(d->context, reg, value) &&
        d->read(d->context, reg, &actual, 1) && actual == value;
}

bool wf_imu_identify(wf_imu_device_t *d, uint8_t *id)
{
    *id = 0;
    d->identified = d->read(d->context, WF_IMU_WHO, id, 1) && *id == 0x6a;
    return d->identified;
}

bool wf_imu_stop(wf_imu_device_t *d)
{
    if (!d->identified) return false;
    /* Attempt every shutdown operation even if an earlier transaction failed. */
    bool ok = set(d, WF_IMU_MD1, 0);
    ok = set(d, WF_IMU_CTRL10, 0) && ok;
    ok = set(d, WF_IMU_XL, 0) && ok;
    ok = set(d, WF_IMU_G, 0) && ok;
    return ok;
}

bool wf_imu_prepare(wf_imu_device_t *d)
{
    uint8_t id, value;
    if (!wf_imu_identify(d, &id)) return false;
    if (!d->write(d->context, WF_IMU_CTRL3, 0x01)) goto fail;
    for (unsigned i = 0; i < 20; ++i) {
        d->delay_ms(d->context, 2);
        if (!d->read(d->context, WF_IMU_CTRL3, &value, 1)) goto fail;
        if (!(value & 1)) {
            /* BDU, little endian, auto increment, push-pull active-high INT1. */
            if (set(d, WF_IMU_CTRL3, 0x44) && wf_imu_stop(d) &&
                set(d, WF_IMU_CTRL6, 0x10) && set(d, WF_IMU_TAP, 0x01)) return true;
            goto fail;
        }
    }
fail:
    (void)wf_imu_stop(d);
    return false;
}

bool wf_imu_sample_mode(wf_imu_device_t *d)
{
    if (!d->identified) return false;
    if (wf_imu_stop(d) && set(d, WF_IMU_XL, 0x20)) return true; /* 26 Hz, +/-2 g */
    (void)wf_imu_stop(d);
    return false;
}

bool wf_imu_tilt_mode(wf_imu_device_t *d)
{
    uint8_t ignored;
    if (!d->identified) return false;
    /* Embedded tilt is only a diagnostic motion candidate, not wrist intent. */
    if (wf_imu_sample_mode(d) && set(d, WF_IMU_CTRL10, 0x0c) &&
        d->read(d->context, WF_IMU_FUNC, &ignored, 1) &&
        set(d, WF_IMU_MD1, 0x02)) return true;
    (void)wf_imu_stop(d);
    return false;
}

int wf_imu_accel(wf_imu_device_t *d, int16_t raw[3])
{
    uint8_t status, bytes[6];
    if (!d->identified || !d->read(d->context, WF_IMU_STATUS, &status, 1)) return -1;
    if (!(status & 1)) return 0;
    if (!d->read(d->context, WF_IMU_AXES, bytes, sizeof bytes)) return -1;
    for (unsigned i = 0; i < 3; ++i) {
        uint16_t value = (uint16_t)bytes[2*i] | (uint16_t)bytes[2*i+1] << 8;
        raw[i] = (int16_t)(value < 0x8000u ? (int32_t)value : (int32_t)value - 65536);
    }
    return 1;
}
