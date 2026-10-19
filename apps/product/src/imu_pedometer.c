#include "imu_pedometer.h"

enum { BANK = 0x01, INT1 = 0x0d, INT2 = 0x0e, WHO = 0x0f,
    XL = 0x10, GYRO = 0x11, CTRL3 = 0x12, CTRL10 = 0x19,
    STEP_L = 0x4b, STEP_H = 0x4c, MD1 = 0x5e, MD2 = 0x5f };

static bool read_byte(wf_imu_pedometer_t *d, uint8_t reg, uint8_t *value)
{
    bool ok = d->io.read && d->io.read(d->io.context, reg, value, 1);
    if (!ok) ++d->snapshot.io_errors;
    return ok;
}

static bool set(wf_imu_pedometer_t *d, uint8_t reg, uint8_t value)
{
    uint8_t actual = 0xff;
    bool written = d->io.write && d->io.write(d->io.context, reg, value);
    if (!written) ++d->snapshot.io_errors;
    /* Attempt readback even after a failed write; never infer success from it. */
    bool read = read_byte(d, reg, &actual);
    if (written && read && actual != value) ++d->snapshot.io_errors;
    return written && read && actual == value;
}

static bool counter(wf_imu_pedometer_t *d, uint32_t now)
{
    d->snapshot.counter_valid = false;
    /* BDU documents XL/G outputs, not this counter. Bound H-L-H retries. */
    for (unsigned i = 0; i < 3; ++i) {
        uint8_t high, low, again;
        if (!read_byte(d, STEP_H, &high) || !read_byte(d, STEP_L, &low) ||
            !read_byte(d, STEP_H, &again)) return false;
        if (high == again) {
            d->snapshot.steps = (uint16_t)((uint16_t)high << 8 | low);
            d->snapshot.sampled_ms = now;
            d->snapshot.counter_valid = true;
            return true;
        }
    }
    ++d->snapshot.incoherent_reads;
    return false;
}

static bool stop_hardware(wf_imu_pedometer_t *d)
{
    /* Never write user-register addresses while bank recovery is unconfirmed. */
    if (!set(d, BANK, 0)) return false;
    const uint8_t regs[] = {INT1, INT2, MD1, MD2, CTRL10, XL, GYRO};
    bool ok = true;
    for (size_t i = 0; i < sizeof regs; ++i) {
        bool stopped = set(d, regs[i], 0);
        ok = stopped && ok; /* Attempt every stop, even after an earlier failure. */
    }
    return ok;
}

static bool continuing(wf_imu_pedometer_t *d)
{
    if (d->io.cancelled && d->io.cancelled(d->io.context)) {
        d->snapshot.ended = WF_IMU_USER_STOP;
        return false;
    }
    if (d->budget_active && d->io.now_ms(d->io.context) - d->snapshot.started_ms >= d->snapshot.duration_ms) {
        d->snapshot.ended = WF_IMU_DEADLINE;
        return false;
    }
    return true;
}

static bool configure(wf_imu_pedometer_t *d, uint8_t reg, uint8_t value)
{ return continuing(d) && set(d, reg, value); }

void wf_imu_pedometer_init(wf_imu_pedometer_t *d, wf_imu_io_t io)
{ *d = (wf_imu_pedometer_t){.io = io}; }

bool wf_imu_pedometer_probe(wf_imu_pedometer_t *d)
{
    if (d->snapshot.phase == WF_IMU_RUNNING || d->snapshot.phase == WF_IMU_FAULT) return false;
    uint8_t who = 0;
    d->identified = read_byte(d, WHO, &who) && who == 0x6a;
    d->snapshot.who = who;
    d->snapshot.counter_valid = d->snapshot.stop_verified = false;
    d->snapshot.phase = d->identified ? WF_IMU_READY : WF_IMU_UNPROBED;
    return d->identified;
}

bool wf_imu_pedometer_stop(wf_imu_pedometer_t *d, uint32_t now, wf_imu_end_t reason)
{
    if (!d->identified) return false;
    d->budget_active = false;
    d->snapshot.ended = reason;
    d->snapshot.stop_verified = stop_hardware(d);
    bool valid = d->snapshot.stop_verified && counter(d, now);
    if (!valid) d->snapshot.counter_valid = false;
    d->snapshot.phase = !d->snapshot.stop_verified || !valid || reason == WF_IMU_IO_FAILURE
        ? WF_IMU_FAULT : reason == WF_IMU_DEADLINE ? WF_IMU_DONE : WF_IMU_READY;
    return d->snapshot.stop_verified;
}

bool wf_imu_pedometer_begin(wf_imu_pedometer_t *d, uint32_t now, uint32_t duration)
{
    if (!d->identified || !d->io.now_ms || d->snapshot.phase == WF_IMU_RUNNING ||
        d->snapshot.phase == WF_IMU_FAULT || duration < 1000 || duration > 120000) return false;
    uint32_t initial_errors = d->snapshot.io_errors;
    d->snapshot.started_ms = now;
    d->snapshot.duration_ms = duration;
    d->snapshot.ended = WF_IMU_NO_END;
    d->budget_active = false;
    d->snapshot.counter_valid = d->snapshot.stop_verified = false;
    if (!continuing(d) || !stop_hardware(d) || !configure(d, CTRL3, 0x44)) goto fail;
    /* AN5130: explicit reference threshold/debounce, not a custom step algorithm.
     * BANK_A=4 in FUNC_CFG_EN[7:5]. Always attempt to restore user bank. */
    bool configured = configure(d, BANK, 0x80);
    if (configured) configured = configure(d, 0x0f, 0x10) && configure(d, 0x14, 0x6e);
    if (configured) configured = read_byte(d, 0x0f, &d->snapshot.threshold) &&
        read_byte(d, 0x14, &d->snapshot.debounce);
    bool restored = set(d, BANK, 0);
    if (!configured || !restored) goto fail;
    /* Preparation precedes the experiment's active budget. From the first ODR
     * write onward, check the live deadline and cancellation between stages. */
    d->snapshot.started_ms = d->io.now_ms(d->io.context);
    d->budget_active = true;
    /* 26Hz +/-2g; FUNC_EN + PEDO_EN. Reset bit is NOT self-clearing. */
    if (!configure(d, XL, 0x20) || !configure(d, CTRL10, 0x14) ||
        !configure(d, CTRL10, 0x16) || !configure(d, CTRL10, 0x14) ||
        !continuing(d) || !counter(d, d->io.now_ms(d->io.context)) ||
        !continuing(d) || d->snapshot.steps != 0) goto fail;
    d->snapshot.phase = WF_IMU_RUNNING;
    return true;
fail:
    /* A later cancellation/deadline must not erase an observed I/O failure. */
    (void)wf_imu_pedometer_stop(d, d->io.now_ms(d->io.context),
        d->snapshot.io_errors != initial_errors || d->snapshot.ended == WF_IMU_NO_END
            ? WF_IMU_IO_FAILURE : d->snapshot.ended);
    return false;
}

void wf_imu_pedometer_poll(wf_imu_pedometer_t *d, uint32_t now)
{
    if (d->snapshot.phase != WF_IMU_RUNNING) return;
    if (d->io.cancelled && d->io.cancelled(d->io.context))
        (void)wf_imu_pedometer_stop(d, now, WF_IMU_USER_STOP);
    else if (now - d->snapshot.started_ms >= d->snapshot.duration_ms)
        (void)wf_imu_pedometer_stop(d, now, WF_IMU_DEADLINE);
    else if (!counter(d, now))
        (void)wf_imu_pedometer_stop(d, now, WF_IMU_IO_FAILURE);
}
