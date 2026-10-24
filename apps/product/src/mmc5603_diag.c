#include "mmc5603_diag.h"

enum { DATA = 0, STATUS = 0x18, CTRL0 = 0x1b, CTRL1 = 0x1c,
    ST_THRESHOLD = 0x1e, ST_FACTORY = 0x27, ID = 0x39,
    OTP_READY = 0x10, SAT_SENSOR = 0x20, MAG_DONE = 0x40 };
enum { IDENTIFY, RESET_WAIT, SELFTEST_WAIT, TRIGGER, SAMPLE_WAIT };
static uint32_t now(const wf_mmc5603_t *d) { return d->io.now_ms(d->io.context); }
static bool read_bytes(wf_mmc5603_t *d, uint8_t reg, uint8_t *data, size_t size)
{
    bool ok = d->io.read(d->io.context, reg, data, size);
    if (!ok) ++d->snapshot.io_errors;
    return ok;
}
static bool write_reg(wf_mmc5603_t *d, uint8_t reg, uint8_t value)
{
    bool ok = d->io.write(d->io.context, reg, value);
    if (!ok) ++d->snapshot.io_errors;
    return ok;
}
void wf_mmc5603_init(wf_mmc5603_t *d, wf_mmc5603_io_t io)
{ *d = (wf_mmc5603_t){.io = io}; }
static bool begin_session(wf_mmc5603_t *d, uint32_t started_ms, uint32_t duration_ms,
    wf_mag_threshold_t threshold_mode, bool selftest_only)
{
    if (d->snapshot.owned || d->snapshot.phase == WF_MAG_RUNNING ||
        duration_ms < 1000U || duration_ms > 120000U ||
        (threshold_mode != WF_MAG_THRESHOLD_RAW80 && threshold_mode != WF_MAG_THRESHOLD_ENCODED_EXPERIMENTAL)) return false;
    uint32_t session = d->snapshot.session + 1U;
    d->snapshot = (wf_mmc5603_snapshot_t){.phase = WF_MAG_RUNNING, .session = session,
        .started_ms = started_ms, .duration_ms = duration_ms,
        .threshold_mode = threshold_mode, .selftest_only = selftest_only};
    d->stage = IDENTIFY;
    d->interval_ms = 1000U;
    d->due_ms = now(d) + 5U; /* Rev.B p15: >=5ms after shared supply becomes valid. */
    return true;
}
bool wf_mmc5603_begin(wf_mmc5603_t *d, uint32_t started_ms, uint32_t duration_ms)
{ return begin_session(d, started_ms, duration_ms, WF_MAG_THRESHOLD_RAW80, false); }
bool wf_mmc5603_begin_encoded(wf_mmc5603_t *d, uint32_t started_ms, uint32_t duration_ms)
{ return begin_session(d, started_ms, duration_ms, WF_MAG_THRESHOLD_ENCODED_EXPERIMENTAL, false); }
bool wf_mmc5603_begin_foreground(wf_mmc5603_t *d, uint32_t started_ms)
{
    if (!begin_session(d, started_ms, 120000U, WF_MAG_THRESHOLD_ENCODED_EXPERIMENTAL, false)) return false;
    d->snapshot.foreground = true;
    d->snapshot.duration_ms = 0;
    d->interval_ms = 200U;
    return true;
}
bool wf_mmc5603_begin_selftest(wf_mmc5603_t *d, uint32_t started_ms, wf_mag_threshold_t mode)
{ return begin_session(d, started_ms, WF_MAG_SELFTEST_TIMEOUT_MS, mode, true); }
static uint8_t selftest_threshold(uint8_t factory, wf_mag_threshold_t mode)
{
    if (mode == WF_MAG_THRESHOLD_RAW80) return (uint8_t)((unsigned)factory * 4U / 5U);
    /* Explicit hypothesis, independently expressed as bounded arithmetic.
     * docs/MMC5603.md#threshold-source-20261024 records provenance and limits.
     * It is not the literal defective BSP output or an approved device spec. */
    int32_t centered = (int32_t)factory - 128;
    uint32_t magnitude = (uint32_t)(centered < 0 ? -centered : centered) * 32U;
    uint32_t threshold = (magnitude - magnitude / 5U) / 8U;
    return (uint8_t)(threshold > 255U ? 255U : threshold);
}
bool wf_mmc5603_stop(wf_mmc5603_t *d, wf_mag_end_t reason)
{
    bool ok = true;
    if (d->snapshot.owned) {
        /* Rev.B p9-10: controls are WRITE ONLY. Never claim a configuration
         * readback. The serialized magnetic owner uses local reset, which
         * clears controls/thresholds and reloads OTP, without touching PA30. */
        ok = write_reg(d, CTRL1, 0x80);
        if (ok) {
            d->io.delay_ms(d->io.context, 20);
            uint8_t id = 0, status = 0, discard[9];
            ok = read_bytes(d, ID, &id, 1) && id == 0x10 &&
                read_bytes(d, STATUS, &status, 1) && (status & OTP_READY);
            if (ok && (status & MAG_DONE)) ok = read_bytes(d, DATA, discard, sizeof discard);
            if (ok) {
                d->io.delay_ms(d->io.context, 20);
                ok = read_bytes(d, STATUS, &d->snapshot.after_status, 1) &&
                    (d->snapshot.after_status & (OTP_READY | MAG_DONE)) == OTP_READY;
            }
        }
        d->snapshot.owned = !ok;
        d->snapshot.stopped = ok;
    }
    d->snapshot.ended = ok ? reason : WF_MAG_IO_FAILURE;
    d->snapshot.phase = ok && (reason <= WF_MAG_CANCELLED || reason == WF_MAG_SELFTEST_COMPLETE) ? WF_MAG_DONE : WF_MAG_FAULT;
    if (!ok || reason > WF_MAG_CANCELLED) d->snapshot.sample_valid = false;
    return ok;
}
void wf_mmc5603_bus_fault(wf_mmc5603_t *d)
{
    ++d->snapshot.io_errors;
    d->snapshot.phase = WF_MAG_FAULT;
    d->snapshot.ended = WF_MAG_IO_FAILURE;
    d->snapshot.sample_valid = d->snapshot.stopped = false;
}
static bool ended(wf_mmc5603_t *d)
{
    if (d->io.cancelled(d->io.context)) { (void)wf_mmc5603_stop(d, WF_MAG_CANCELLED); return true; }
    if (!d->snapshot.foreground && now(d) - d->snapshot.started_ms >= d->snapshot.duration_ms) {
        (void)wf_mmc5603_stop(d, WF_MAG_DEADLINE); return true;
    }
    return false;
}
static void fail(wf_mmc5603_t *d, wf_mag_end_t reason)
{ (void)wf_mmc5603_stop(d, reason); }
void wf_mmc5603_poll(wf_mmc5603_t *d)
{
    if (d->snapshot.phase != WF_MAG_RUNNING || ended(d) || (int32_t)(now(d) - d->due_ms) < 0) return;
    uint8_t status = 0, raw[9];
    if (d->stage == IDENTIFY) {
        if (!read_bytes(d, ID, &d->snapshot.identity, 1)) { fail(d, WF_MAG_IO_FAILURE); return; }
        d->snapshot.identity_valid = true;
        if (d->snapshot.identity != 0x10) { fail(d, WF_MAG_ID_MISMATCH); return; }
        if (ended(d)) return;
        d->snapshot.owned = true; /* A failed write may still have reached the device. */
        if (!write_reg(d, CTRL1, 0x80)) { fail(d, WF_MAG_IO_FAILURE); return; }
        if (ended(d)) return;
        d->stage = RESET_WAIT; d->due_ms = now(d) + 20U;
        return;
    }
    if (!read_bytes(d, STATUS, &status, 1)) { fail(d, WF_MAG_IO_FAILURE); return; }
    if (d->stage == RESET_WAIT) {
        d->snapshot.reset_status = status;
        d->snapshot.reset_observed_ms = now(d);
        d->snapshot.selftest_trace |= WF_MAG_TRACE_RESET;
    } else if (d->stage == SELFTEST_WAIT) {
        /* Last successful observation in the existing wait path, with its
         * software read-completion time. This does not prove chip-internal
         * selftest completion merely because magnetic DONE is already set. */
        d->snapshot.selftest_status = status;
        d->snapshot.selftest_status_ms = now(d);
        ++d->snapshot.selftest_polls;
        d->snapshot.selftest_trace |= WF_MAG_TRACE_STATUS;
    }
    if (ended(d)) return;
    d->snapshot.status = status;
    if (!(status & OTP_READY)) { fail(d, WF_MAG_NOT_READY); return; }
    if (d->stage == RESET_WAIT) {
        if (status & MAG_DONE) { fail(d, WF_MAG_UNEXPECTED_DATA); return; }
        if (!read_bytes(d, ST_FACTORY, d->snapshot.selftest_factory, 3)) { fail(d, WF_MAG_IO_FAILURE); return; }
        d->snapshot.selftest_trace |= WF_MAG_TRACE_FACTORY;
        if (ended(d)) return;
        /* Rev.B p14 native selftest; raw80 remains the default. The explicit
         * encoded hypothesis changes thresholds only, never coil/command/timing. */
        for (unsigned i = 0; i < 3; ++i) {
            d->snapshot.selftest_threshold[i] = selftest_threshold(d->snapshot.selftest_factory[i], d->snapshot.threshold_mode);
            if (!write_reg(d, (uint8_t)(ST_THRESHOLD + i), d->snapshot.selftest_threshold[i])) {
                fail(d, WF_MAG_IO_FAILURE); return;
            }
            if (ended(d)) return;
        }
        d->frame_ms = now(d);
        d->snapshot.selftest_command_ms = d->frame_ms;
        d->snapshot.selftest_trace |= WF_MAG_TRACE_COMMAND;
        if (!write_reg(d, CTRL0, 0x41)) { fail(d, WF_MAG_IO_FAILURE); return; }
        d->snapshot.selftest_ack_ms = now(d);
        d->snapshot.selftest_trace |= WF_MAG_TRACE_ACK;
        if (ended(d)) return;
        d->stage = SELFTEST_WAIT; d->due_ms = now(d) + 10U;
        return;
    }
    if (d->stage == TRIGGER) {
        if (status & MAG_DONE) { fail(d, WF_MAG_UNEXPECTED_DATA); return; }
        d->frame_ms = now(d);
        /* Auto_SR_en applies to on-demand measurements too (p9). BW=00
         * comes from reset; no continuous mode / ODR / interrupt is enabled. */
        if (!write_reg(d, CTRL0, 0x21)) { fail(d, WF_MAG_IO_FAILURE); return; }
        if (ended(d)) return;
        d->stage = SAMPLE_WAIT; d->due_ms = now(d) + 10U;
        return;
    }
    /* BW=00 nominal conversion6.6ms. If DONE is still absent when observed
     * >=50ms after the command, stop. A delayed DONE still belongs to this
     * sole on-demand trigger; the session deadline remains authoritative. */
    if (!(status & MAG_DONE)) {
        if (now(d) - d->frame_ms >= 50U) { fail(d, WF_MAG_NOT_READY); return; }
        d->due_ms = now(d) + 5U; return;
    }
    if (!read_bytes(d, DATA, raw, sizeof raw)) { fail(d, WF_MAG_IO_FAILURE); return; }
    if (d->stage == SELFTEST_WAIT) {
        for (unsigned i = 0; i < 9; ++i) d->snapshot.selftest_raw[i] = raw[i];
        d->snapshot.selftest_data_ms = now(d);
        d->snapshot.selftest_trace |= WF_MAG_TRACE_DATA;
    }
    if (ended(d)) return;
    uint8_t after = 0;
    if (!read_bytes(d, STATUS, &after, 1)) { fail(d, WF_MAG_IO_FAILURE); return; }
    if (d->stage == SELFTEST_WAIT) {
        d->snapshot.selftest_post_status = after;
        d->snapshot.selftest_post_ms = now(d);
        d->snapshot.selftest_trace |= WF_MAG_TRACE_POST;
    }
    if (ended(d)) return;
    if ((after & (OTP_READY | MAG_DONE)) != OTP_READY) { fail(d, WF_MAG_UNEXPECTED_DATA); return; }
    if (d->stage == SELFTEST_WAIT) {
        d->snapshot.selftest_status = status;
        d->snapshot.selftest_valid = true;
        d->snapshot.selftest_passed = !(status & SAT_SENSOR);
        if (!d->snapshot.selftest_passed) { fail(d, WF_MAG_SELFTEST_FAILED); return; }
        if (d->snapshot.selftest_only) {
            (void)wf_mmc5603_stop(d, WF_MAG_SELFTEST_COMPLETE);
            return;
        }
    } else {
        int32_t counts[3];
        d->snapshot.saturated = false;
        for (unsigned i = 0; i < 3; ++i) {
            uint32_t value = (uint32_t)raw[2*i] << 12 | (uint32_t)raw[2*i+1] << 4 | raw[6+i] >> 4;
            counts[i] = (int32_t)value - 524288;
            if (!value || value == 1048575U) d->snapshot.saturated = true;
        }
        for (unsigned i = 0; i < 9; ++i) d->snapshot.raw[i] = raw[i];
        d->snapshot.x = counts[0]; d->snapshot.y = counts[1]; d->snapshot.z = counts[2];
        d->snapshot.sampled_ms = now(d); /* Read-completion time, not a hardware timestamp. */
        ++d->snapshot.samples;
        d->snapshot.sample_valid = !d->snapshot.saturated && !(status & SAT_SENSOR);
    }
    d->stage = TRIGGER;
    d->due_ms = now(d) + (d->snapshot.samples ? d->interval_ms : 1U);
}
int32_t wf_mmc5603_wait_ms(const wf_mmc5603_t *d, uint32_t time)
{
    if (d->snapshot.phase != WF_MAG_RUNNING) return -1;
    if (d->snapshot.foreground) {
        int32_t due = (int32_t)(d->due_ms - time);
        return due <= 0 ? 0 : due;
    }
    uint32_t elapsed = time - d->snapshot.started_ms;
    if (elapsed >= d->snapshot.duration_ms) return 0;
    uint32_t remaining = d->snapshot.duration_ms - elapsed;
    int32_t due = (int32_t)(d->due_ms - time);
    return due <= 0 ? 0 : (uint32_t)due < remaining ? due : (int32_t)remaining;
}
