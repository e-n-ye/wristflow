#include "imu_device.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t regs[256];
    unsigned operations, fail_at, writes, delay, reset_reads;
    bool stuck_reset, corrupt_readback;
} fake_t;

static bool read_reg(void *context, uint8_t reg, uint8_t *bytes, size_t count)
{
    fake_t *f = context;
    if (++f->operations == f->fail_at) return false;
    if (reg == WF_IMU_CTRL3 && (f->regs[reg] & 1)) {
        ++f->reset_reads;
        if (!f->stuck_reset) f->regs[reg] = 0;
    }
    memcpy(bytes, f->regs + reg, count);
    if (f->corrupt_readback && reg == WF_IMU_XL && bytes[0] == 0x20) bytes[0] = 0;
    return true;
}

static bool write_reg(void *context, uint8_t reg, uint8_t value)
{
    fake_t *f = context;
    if (++f->operations == f->fail_at) return false;
    ++f->writes;
    f->regs[reg] = value;
    return true;
}
static void delay(void *context, unsigned ms) { ((fake_t *)context)->delay += ms; }
static wf_imu_device_t fresh(fake_t *f)
{
    memset(f, 0, sizeof *f);
    f->regs[WF_IMU_WHO] = 0x6a;
    return (wf_imu_device_t){read_reg, write_reg, delay, f, false};
}
static void stopped(fake_t *f)
{
    assert(f->regs[WF_IMU_MD1] == 0 && f->regs[WF_IMU_CTRL10] == 0);
    assert(f->regs[WF_IMU_XL] == 0 && f->regs[WF_IMU_G] == 0);
}
int main(void)
{
    fake_t f;
    wf_imu_device_t d = fresh(&f);
    f.regs[WF_IMU_WHO] = 0x69;
    assert(!wf_imu_prepare(&d) && f.writes == 0);
    assert(!wf_imu_tilt_mode(&d) && !wf_imu_sample_mode(&d) && f.writes == 0);
    d = fresh(&f); f.fail_at = 1;
    assert(!wf_imu_prepare(&d) && f.writes == 0);
    d = fresh(&f); f.stuck_reset = true;
    assert(!wf_imu_prepare(&d) && f.reset_reads == 20 && f.delay == 40);
    stopped(&f);

    d = fresh(&f);
    assert(wf_imu_prepare(&d));
    unsigned prepare_ops = f.operations;
    assert(f.regs[WF_IMU_CTRL3] == 0x44 && f.regs[WF_IMU_CTRL6] == 0x10);
    assert(wf_imu_tilt_mode(&d));
    unsigned tilt_ops = f.operations - prepare_ops;
    assert(f.regs[WF_IMU_G] == 0);
    assert(wf_imu_stop(&d)); stopped(&f);

    /* Inject one failed transfer at every initialization/arming boundary. */
    for (unsigned n = 1; n <= prepare_ops; ++n) {
        d = fresh(&f); f.fail_at = n;
        assert(!wf_imu_prepare(&d)); stopped(&f);
    }
    for (unsigned n = 1; n <= tilt_ops; ++n) {
        d = fresh(&f); assert(wf_imu_prepare(&d));
        f.fail_at = f.operations + n;
        assert(!wf_imu_tilt_mode(&d)); stopped(&f);
    }
    d = fresh(&f); assert(wf_imu_prepare(&d)); f.corrupt_readback = true;
    assert(!wf_imu_sample_mode(&d)); stopped(&f);

    d = fresh(&f); assert(wf_imu_prepare(&d));
    int16_t axes[] = {123, 456, 789};
    assert(wf_imu_accel(&d, axes) == 0 && axes[0] == 123);
    f.regs[WF_IMU_STATUS] = 1;
    const uint8_t bytes[] = {0, 0x80, 0xff, 0x7f, 0xff, 0xff};
    memcpy(f.regs + WF_IMU_AXES, bytes, sizeof bytes);
    f.fail_at = f.operations + 2;
    assert(wf_imu_accel(&d, axes) == -1 && axes[0] == 123 && axes[2] == 789);
    assert(wf_imu_accel(&d, axes) == 1);
    assert(axes[0] == -32768 && axes[1] == 32767 && axes[2] == -1);
    puts("IMU identity rejection, reset timeout, transfer faults, cleanup and signed axes passed");
}
