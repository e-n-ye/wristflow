#include "imu_wrist_tilt.h"

enum { BANK = 0x01, PULSE = 0x0b, WHO = 0x0f, XL = 0x10, CTRL3 = 0x12, CTRL4 = 0x13,
    CTRL10 = 0x19, STATUS = 0x1e, OUT_XL = 0x28,
    SOURCE = 0x54, AXES = 0x55, TAP = 0x58, LAT = 0x50, THS = 0x54, MASK = 0x59,
    INT1 = 0x0d, INT2 = 0x0e, MD1 = 0x5e, MD2 = 0x5f };

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
    if (!d->snapshot.continuous && d->io.now_ms(d->io.context) - d->snapshot.started_ms >= d->snapshot.duration_ms) {
        d->snapshot.ended = WF_IMU_DEADLINE;
        return false;
    }
    return true;
}
static bool configure(wf_imu_wrist_tilt_t *d, uint8_t reg, uint8_t value)
{ return continuing(d) && set(d, reg, value); }

static void frame_byte(wf_imu_wrist_tilt_t *d, wf_imu_wrist_frame_t *f,
    uint8_t reg, uint8_t *out, unsigned bit)
{ if (read_byte(d, reg, out)) f->valid |= bit; }

static void frame_pin(wf_imu_wrist_tilt_t *d, wf_imu_wrist_frame_t *f, bool after)
{
    if (!d->snapshot.route_int1) return;
    uint32_t now = d->io.now_ms(d->io.context);
    int value = d->read_int1 ? d->read_int1(d->io.context) : -1;
    if (after) f->pin_after_ms = now; else f->pin_before_ms = now;
    if (value != 0 && value != 1) { ++d->snapshot.io_errors; return; }
    f->pin_valid |= after ? 2U : 1U;
    if (after) f->pin_after = (uint8_t)value; else f->pin_before = (uint8_t)value;
}

static void capture(wf_imu_wrist_tilt_t *d, wf_imu_wrist_frame_t *f)
{
    *f = (wf_imu_wrist_frame_t){.attempted = true, .started_ms = d->io.now_ms(d->io.context)};
    frame_byte(d, f, BANK, &f->bank, WF_WRIST_BANK);
    /* Do not read user-address aliases when the bank is unknown or nonzero. */
    if (!(f->valid & WF_WRIST_BANK) || f->bank) goto done;
    frame_byte(d, f, WHO, &f->who, WF_WRIST_WHO);
    frame_byte(d, f, XL, &f->xl, WF_WRIST_XL);
    frame_byte(d, f, CTRL3, &f->ctrl3, WF_WRIST_CTRL3);
    frame_byte(d, f, CTRL4, &f->ctrl4, WF_WRIST_CTRL4);
    frame_byte(d, f, CTRL10, &f->ctrl10, WF_WRIST_CTRL10);
    frame_byte(d, f, PULSE, &f->pulse, WF_WRIST_PULSE);
    frame_byte(d, f, TAP, &f->tap, WF_WRIST_TAP);
    if (d->snapshot.route_int1) {
        frame_byte(d, f, INT1, &f->int1, WF_WRIST_INT1);
        frame_byte(d, f, INT2, &f->int2, WF_WRIST_INT2);
        frame_byte(d, f, MD1, &f->md1, WF_WRIST_MD1);
        frame_byte(d, f, MD2, &f->md2, WF_WRIST_MD2);
    }
    frame_byte(d, f, STATUS, &f->status, WF_WRIST_STATUS);
    if (d->io.read && d->io.read(d->io.context, OUT_XL, f->raw, sizeof f->raw))
        f->valid |= WF_WRIST_RAW;
    else ++d->snapshot.io_errors;
    /* Keep the existing AXES-before-clearing-SOURCE pair. These reads are not
     * atomic with one another or with XYZ. Before/after reads also clear SOURCE.
     * Datasheet 2.2: AXES is not applicable to a single enabled semi-axis. */
    frame_byte(d, f, AXES, &f->axes, WF_WRIST_AXES);
    frame_pin(d, f, false);
    frame_byte(d, f, SOURCE, &f->source, WF_WRIST_SOURCE);
    frame_pin(d, f, true);
    f->data_ready = (f->valid & WF_WRIST_STATUS) && (f->status & 1U);
    unsigned format = WF_WRIST_XL | WF_WRIST_CTRL3 | WF_WRIST_RAW;
    f->xyz_valid = (f->valid & format) == format && f->xl == 0x20 && f->ctrl3 == 0x44;
    if (f->xyz_valid) for (unsigned i = 0; i < 3; ++i) {
        int32_t value = (uint16_t)((uint16_t)f->raw[2*i + 1] << 8 | f->raw[2*i]);
        if (value >= 32768) value -= 65536;
        f->xyz[i] = (int16_t)value;
    }
done:
    f->completed_ms = d->io.now_ms(d->io.context);
}

static bool frame_complete(wf_imu_wrist_tilt_t *d, const wf_imu_wrist_frame_t *f)
{
    if (!d->snapshot.route_int1) return f->valid == WF_WRIST_ALL;
    /* The OR route must not silently mix another configurable interrupt source
     * into this experiment. Observe/reject it, never overwrite its ownership. */
    return f->valid == WF_WRIST_ROUTE_ALL && f->pin_valid == 3U &&
        !f->int1 && !f->int2 && !f->md1 && !f->md2;
}

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
            bool route = !d->snapshot.route_int1 || set(d, CTRL4, d->saved_ctrl4);
            ok = control && clear && pulse && tap && route;
        }
        if (ok) {
            d->changed = false;
            /* Observability is separate from the verified stop transaction:
             * failed telemetry leaves partial valid bits, not an active device. */
            capture(d, &d->snapshot.after);
        }
    }
    d->snapshot.sampled_ms = now;
    d->snapshot.stop_verified = ok;
    d->snapshot.phase = !ok || reason == WF_IMU_IO_FAILURE ? WF_IMU_FAULT :
        reason == WF_IMU_DEADLINE ? WF_IMU_DONE : WF_IMU_READY;
    return ok;
}

static bool begin(wf_imu_wrist_tilt_t *d, uint32_t duration, uint8_t mask, uint8_t latency, bool route, bool continuous)
{
    if (!d->io.now_ms || !d->delay_ms || d->changed || d->snapshot.phase == WF_IMU_RUNNING ||
        d->snapshot.phase == WF_IMU_FAULT || (!continuous && (duration < 1000 || duration > 120000)) ||
        !mask || (mask & 3U) || !latency || (route && !d->read_int1)) return false;
    uint32_t errors = d->snapshot.io_errors;
    d->snapshot = (wf_imu_wrist_snapshot_t){.io_errors = errors, .mask = mask,
        .latency = latency, .threshold = 0x20, .duration_ms = duration, .stop_verified = true,
        .route_int1 = route, .continuous = continuous};
    /* This capability may borrow only the known native-counter configuration.
     * In particular it must not reset PEDO_RST_STEP, ODR or the shared rail. */
    capture(d, &d->snapshot.before);
    wf_imu_wrist_frame_t *f = &d->snapshot.before;
    bool read = frame_complete(d, f) && f->who == 0x6a && f->xl == 0x20 &&
        f->ctrl3 == 0x44 && f->ctrl10 == 0x14 && !(f->pulse & 1U);
    if (!read) {
        d->snapshot.phase = WF_IMU_FAULT;
        d->snapshot.ended = WF_IMU_IO_FAILURE;
        return false; /* No register was written; do not invent a restore value. */
    }
    d->saved_pulse = f->pulse;
    d->saved_tap = f->tap;
    d->saved_ctrl4 = f->ctrl4;
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
        (route && !configure(d, CTRL4, d->saved_ctrl4 | 0x20U)) ||
        !configure(d, CTRL10, 0x94) || !continuing(d)) goto fail;
    /* Route to INT2 even though it is NC on this board: without a driven
     * interrupt route the source remains a 1/26s pulse despite LIR (AN5130).
     * INT1 mode additionally ORs INT2 onto INT1 (CTRL4 bit5). The owner decides
     * whether this is a bounded observation or a persistent IRQ wake service. */
    d->snapshot.configured = true;
    d->snapshot.phase = WF_IMU_RUNNING;
    return true;
fail:
    (void)wf_imu_wrist_stop(d, d->io.now_ms(d->io.context),
        d->snapshot.io_errors != errors || d->snapshot.ended == WF_IMU_NO_END
            ? WF_IMU_IO_FAILURE : d->snapshot.ended, true);
    return false;
}

bool wf_imu_wrist_begin(wf_imu_wrist_tilt_t *d, uint32_t duration, uint8_t mask, uint8_t latency)
{ return begin(d, duration, mask, latency, false, false); }
bool wf_imu_wrist_route_begin(wf_imu_wrist_tilt_t *d, uint32_t duration, uint8_t mask, uint8_t latency)
{ return begin(d, duration, mask, latency, true, false); }
bool wf_imu_wrist_wake_begin(wf_imu_wrist_tilt_t *d, uint8_t mask, uint8_t latency)
{ return begin(d, 0, mask, latency, true, true); }

void wf_imu_wrist_poll(wf_imu_wrist_tilt_t *d, uint32_t now)
{
    if (d->snapshot.phase != WF_IMU_RUNNING) return;
    if (!continuing(d)) {
        (void)wf_imu_wrist_stop(d, now, d->snapshot.ended, true);
        return;
    }
    capture(d, &d->snapshot.running);
    wf_imu_wrist_frame_t *f = &d->snapshot.running;
    bool ok = frame_complete(d, f) && f->who == 0x6a && f->xl == 0x20 && f->ctrl3 == 0x44 &&
        f->ctrl10 == 0x94 && f->pulse == (uint8_t)(d->saved_pulse | 1U) &&
        f->tap == (uint8_t)(d->saved_tap | 1U) &&
        f->ctrl4 == (uint8_t)(d->saved_ctrl4 | (d->snapshot.route_int1 ? 0x20U : 0));
    if (!ok) { (void)wf_imu_wrist_stop(d, now, WF_IMU_IO_FAILURE, true); return; }
    if (!continuing(d)) { (void)wf_imu_wrist_stop(d, f->completed_ms, d->snapshot.ended, true); return; }
    d->snapshot.axes = f->axes;
    d->snapshot.source = f->source;
    d->snapshot.source_valid = true;
    d->snapshot.sampled_ms = f->completed_ms;
    if (f->source & 1U) { ++d->snapshot.observations; d->snapshot.event = *f; }
}
