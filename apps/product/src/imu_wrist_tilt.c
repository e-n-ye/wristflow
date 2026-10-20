#include "imu_wrist_tilt.h"

enum { BANK = 0x01, PULSE = 0x0b, XL = 0x10, CTRL10 = 0x19,
    SOURCE = 0x54, AXES = 0x55, TAP = 0x58, LAT = 0x50, THS = 0x54, MASK = 0x59 };

static bool read_byte(wf_imu_wrist_tilt_t *d, uint8_t reg, uint8_t *value)
{
    bool ok = d->io.read && d->io.read(d->io.context, reg, value, 1);
    if (!ok) ++d->snapshot.io_errors;
    return ok;
}
static bool set(wf_imu_wrist_tilt_t *d, uint8_t reg, uint8_t value)
{
    uint8_t actual = 0xff;
    bool written = d->io.write && d->io.write(d->io.context, reg, value);
    if (!written) ++d->snapshot.io_errors;
    bool read = read_byte(d, reg, &actual);
    if (written && read && actual != value) ++d->snapshot.io_errors;
    return written && read && actual == value;
}
static bool continuing(wf_imu_wrist_tilt_t *d)
{
    if (d->io.cancelled && d->io.cancelled(d->io.context)) {
        d->snapshot.ended = WF_IMU_USER_STOP;
        return false;
    }
    if (d->io.now_ms(d->io.context) - d->snapshot.started_ms >= d->snapshot.duration_ms) {
        d->snapshot.ended = WF_IMU_DEADLINE;
        return false;
    }
    return true;
}
static bool configure(wf_imu_wrist_tilt_t *d, uint8_t reg, uint8_t value)
{ return continuing(d) && set(d, reg, value); }

void wf_imu_wrist_init(wf_imu_wrist_tilt_t *d, wf_imu_io_t io,
    void (*delay_ms)(void *, uint32_t))
{ *d = (wf_imu_wrist_tilt_t){.io = io, .delay_ms = delay_ms}; }

bool wf_imu_wrist_stop(wf_imu_wrist_tilt_t *d, uint32_t now, wf_imu_end_t reason, bool keep_steps)
{
    bool ok = true;
    d->snapshot.ended = reason;
    d->snapshot.source_valid = d->snapshot.configured = false;
    if (d->changed) {
        /* Never touch a user register until recovery of bank0 is confirmed. */
        ok = set(d, BANK, 0);
        if (ok) {
            uint8_t ignored;
            bool control = set(d, CTRL10, keep_steps ? 0x14 : 0);
            bool clear = read_byte(d, SOURCE, &ignored);
            bool pulse = set(d, PULSE, d->saved_pulse);
            bool tap = set(d, TAP, d->saved_tap);
            ok = control && clear && pulse && tap;
        }
        if (ok) d->changed = false;
    }
    d->snapshot.sampled_ms = now;
    d->snapshot.stop_verified = ok;
    d->snapshot.phase = !ok || reason == WF_IMU_IO_FAILURE ? WF_IMU_FAULT :
        reason == WF_IMU_DEADLINE ? WF_IMU_DONE : WF_IMU_READY;
    return ok;
}

bool wf_imu_wrist_begin(wf_imu_wrist_tilt_t *d, uint32_t duration, uint8_t mask, uint8_t latency)
{
    if (!d->io.now_ms || !d->delay_ms || d->changed || d->snapshot.phase == WF_IMU_RUNNING ||
        d->snapshot.phase == WF_IMU_FAULT || duration < 1000 || duration > 120000 ||
        !mask || (mask & 3U) || !latency) return false;
    uint8_t bank, xl, control;
    uint32_t errors = d->snapshot.io_errors;
    d->snapshot = (wf_imu_wrist_snapshot_t){.io_errors = errors, .mask = mask,
        .latency = latency, .threshold = 0x20, .duration_ms = duration, .stop_verified = true};
    /* This capability may borrow only the known native-counter configuration.
     * In particular it must not reset PEDO_RST_STEP, ODR or the shared rail. */
    bool read = read_byte(d, BANK, &bank) && bank == 0 && read_byte(d, XL, &xl) && xl == 0x20 &&
        read_byte(d, CTRL10, &control) && control == 0x14 &&
        read_byte(d, PULSE, &d->saved_pulse) && !(d->saved_pulse & 1U) &&
        read_byte(d, TAP, &d->saved_tap);
    if (!read) {
        d->snapshot.phase = WF_IMU_FAULT;
        d->snapshot.ended = WF_IMU_IO_FAILURE;
        return false; /* No register was written; do not invent a restore value. */
    }
    d->snapshot.started_ms = d->io.now_ms(d->io.context);
    if (!continuing(d)) goto fail;
    d->changed = true;
    d->snapshot.stop_verified = false;
    /* AN5130 6.4: initialize the embedded functions, wait50ms, then program
     * bankB while FUNC_EN is off. Native counting pauses during preparation;
     * its counter is never reset and the daily model remains a partial total. */
    if (!configure(d, CTRL10, 0x04)) goto fail;
    d->delay_ms(d->io.context, 50);
    if (!configure(d, CTRL10, 0)) goto fail;
    bool configured = configure(d, BANK, 0xa0);
    if (configured) configured = configure(d, LAT, latency) && configure(d, THS, 0x20) && configure(d, MASK, mask);
    bool restored = set(d, BANK, 0); /* Also attempt after a failed bankB write. */
    if (!configured || !restored) goto fail;
    uint8_t ignored;
    if (!continuing(d) || !read_byte(d, SOURCE, &ignored) ||
        !configure(d, TAP, d->saved_tap | 1U) || !configure(d, PULSE, d->saved_pulse | 1U) ||
        !configure(d, CTRL10, 0x94) || !continuing(d)) goto fail;
    /* Route to INT2 even though it is NC on this board: without a driven
     * interrupt route the source remains a 1/26s pulse despite LIR (AN5130).
     * The bounded worker polls this latched source; no MCU IRQ/AON is enabled. */
    d->snapshot.configured = true;
    d->snapshot.phase = WF_IMU_RUNNING;
    return true;
fail:
    (void)wf_imu_wrist_stop(d, d->io.now_ms(d->io.context),
        d->snapshot.io_errors != errors || d->snapshot.ended == WF_IMU_NO_END
            ? WF_IMU_IO_FAILURE : d->snapshot.ended, true);
    return false;
}

void wf_imu_wrist_poll(wf_imu_wrist_tilt_t *d, uint32_t now)
{
    if (d->snapshot.phase != WF_IMU_RUNNING) return;
    if (!continuing(d)) {
        (void)wf_imu_wrist_stop(d, now, d->snapshot.ended, true);
        return;
    }
    uint8_t bank, xl, control, pulse, tap, source, axes;
    bool ok = read_byte(d, BANK, &bank) && bank == 0 && read_byte(d, XL, &xl) && xl == 0x20 &&
        read_byte(d, CTRL10, &control) && control == 0x94 &&
        read_byte(d, PULSE, &pulse) && pulse == (uint8_t)(d->saved_pulse | 1U) &&
        read_byte(d, TAP, &tap) && tap == (uint8_t)(d->saved_tap | 1U) &&
        read_byte(d, AXES, &axes) && read_byte(d, SOURCE, &source);
    if (!ok) { (void)wf_imu_wrist_stop(d, now, WF_IMU_IO_FAILURE, true); return; }
    d->snapshot.axes = axes; /* Read before the clearing source; not an atomic pair. */
    d->snapshot.source = source;
    d->snapshot.source_valid = true;
    d->snapshot.sampled_ms = now;
    if (source & 1U) ++d->snapshot.observations;
}
