#include "ltr303_diag.h"

enum { CONTR = 0x80, RATE = 0x85, PART = 0x86, MANUFACTURER = 0x87,
    CH1_LOW = 0x88, CH0_HIGH = 0x8b, STATUS = 0x8c };
static uint32_t now(const wf_ltr303_t *d) { return d->io.now_ms(d->io.context); }
static bool cancelled(const wf_ltr303_t *d) { return d->io.cancelled(d->io.context); }
static bool read_reg(wf_ltr303_t *d, uint8_t reg, uint8_t *value)
{
    bool ok = d->io.read(d->io.context, reg, value);
    if (!ok) ++d->snapshot.io_errors;
    return ok;
}
static bool write_reg(wf_ltr303_t *d, uint8_t reg, uint8_t value)
{
    bool ok = d->io.write(d->io.context, reg, value);
    if (!ok) ++d->snapshot.io_errors;
    return ok;
}
static bool read_channels(wf_ltr303_t *d, uint16_t *ch0, uint16_t *ch1)
{
    /* DS86-2013-0004 p17-18: first DATA read locks the frame; 0x8b
     * releases it. Always attempt the last read, including partial failures. */
    uint8_t bytes[4] = {0};
    bool ok = true;
    d->data_locked = d->snapshot.owned = true;
    for (unsigned i = 0; i < 4; ++i) {
        bool accepted = read_reg(d, (uint8_t)(CH1_LOW + i), &bytes[i]);
        if (i == 3 && accepted) d->data_locked = false;
        ok = accepted && ok;
    }
    if (ok) {
        *ch1 = (uint16_t)((uint16_t)bytes[1] << 8 | bytes[0]);
        *ch0 = (uint16_t)((uint16_t)bytes[3] << 8 | bytes[2]);
    }
    return ok;
}
void wf_ltr303_init(wf_ltr303_t *d, wf_ltr303_io_t io)
{ *d = (wf_ltr303_t){.io = io}; }
bool wf_ltr303_begin(wf_ltr303_t *d, uint32_t started_ms, uint32_t duration_ms)
{
    if (d->snapshot.owned || d->snapshot.phase == WF_LIGHT_RUNNING ||
        duration_ms < 1000U || duration_ms > 120000U) return false;
    uint32_t session = d->snapshot.session + 1U;
    d->snapshot = (wf_ltr303_snapshot_t){.phase = WF_LIGHT_RUNNING, .session = session,
        .started_ms = started_ms, .duration_ms = duration_ms};
    d->changed = d->data_locked = false;
    /* Even if the shared rail has only just been enabled, wait >=100ms
     * before configuring ALS. The wait is part of the original deadline. */
    d->due_ms = now(d) + 100U;
    return true;
}
bool wf_ltr303_stop(wf_ltr303_t *d, wf_light_end_t reason)
{
    bool ok = true;
    if (d->data_locked) {
        uint8_t discard;
        ok = read_reg(d, CH0_HIGH, &discard);
        if (ok) d->data_locked = false;
    }
    if (d->changed) {
        /* Preserve the complete prior standby configuration; never reset or
         * power-cycle this rail, which is also used by IMU and MMC. */
        bool ctrl = write_reg(d, CONTR, d->snapshot.before_ctrl);
        bool rate = write_reg(d, RATE, d->snapshot.before_rate);
        bool read_ctrl = read_reg(d, CONTR, &d->snapshot.after_ctrl);
        bool read_rate = read_reg(d, RATE, &d->snapshot.after_rate);
        d->snapshot.after_valid = read_ctrl && read_rate;
        ok = ok && ctrl && rate && read_ctrl && read_rate &&
            d->snapshot.after_ctrl == d->snapshot.before_ctrl &&
            d->snapshot.after_rate == d->snapshot.before_rate;
        if (ok) d->changed = false;
    }
    d->snapshot.owned = d->changed || d->data_locked;
    d->snapshot.restored = ok && d->snapshot.before_valid && d->snapshot.after_valid;
    d->snapshot.ended = reason;
    d->snapshot.phase = ok && reason <= WF_LIGHT_CANCELLED ? WF_LIGHT_DONE : WF_LIGHT_FAULT;
    if (!ok) {
        d->snapshot.phase = WF_LIGHT_FAULT;
        d->snapshot.ended = WF_LIGHT_IO_FAILURE;
        d->snapshot.sample_valid = false;
    }
    return ok;
}
void wf_ltr303_bus_fault(wf_ltr303_t *d)
{
    ++d->snapshot.io_errors;
    d->snapshot.phase = WF_LIGHT_FAULT;
    d->snapshot.ended = WF_LIGHT_IO_FAILURE;
    d->snapshot.sample_valid = d->snapshot.restored = false;
    /* An unavailable bus cannot establish that prior writes were undone. */
}
static void fail(wf_ltr303_t *d, wf_light_end_t reason)
{
    d->snapshot.sample_valid = false;
    (void)wf_ltr303_stop(d, reason);
}
static bool ended(wf_ltr303_t *d)
{
    if (cancelled(d)) { (void)wf_ltr303_stop(d, WF_LIGHT_CANCELLED); return true; }
    if (now(d) - d->snapshot.started_ms >= d->snapshot.duration_ms) {
        (void)wf_ltr303_stop(d, WF_LIGHT_DEADLINE); return true;
    }
    return false;
}
void wf_ltr303_poll(wf_ltr303_t *d)
{
    if (d->snapshot.phase != WF_LIGHT_RUNNING || ended(d) || (int32_t)(now(d) - d->due_ms) < 0) return;
    if (!d->snapshot.entered_valid) {
        bool part = read_reg(d, PART, &d->snapshot.part);
        bool manufacturer = read_reg(d, MANUFACTURER, &d->snapshot.manufacturer);
        d->snapshot.identity_valid = part && manufacturer;
        if (!part || !manufacturer) { fail(d, WF_LIGHT_IO_FAILURE); return; }
        if (d->snapshot.part != 0xa0 || d->snapshot.manufacturer != 0x05) {
            fail(d, WF_LIGHT_ID_MISMATCH); return;
        }
        bool ctrl = read_reg(d, CONTR, &d->snapshot.before_ctrl);
        bool rate = read_reg(d, RATE, &d->snapshot.before_rate);
        d->snapshot.before_valid = ctrl && rate;
        if (!ctrl || !rate) { fail(d, WF_LIGHT_IO_FAILURE); return; }
        /* No other ALS owner may be active. Reserved/reset bits are not
         * writable here; preserve gain and rate for a verified restoration. */
        uint8_t gain = d->snapshot.before_ctrl & 0x1cU;
        if ((d->snapshot.before_ctrl & 0xe3U) || gain == 0x10 || gain == 0x14 ||
            (d->snapshot.before_rate & 0xc0U)) {
            fail(d, WF_LIGHT_IN_USE); return;
        }
        uint16_t discard0, discard1;
        if (!read_channels(d, &discard0, &discard1)) { fail(d, WF_LIGHT_IO_FAILURE); return; }
        if (ended(d)) return;
        d->changed = d->snapshot.owned = true; /* Failed writes may still have changed hardware. */
        if (!write_reg(d, RATE, 0x03) || ended(d)) {
            if (d->snapshot.phase == WF_LIGHT_RUNNING) fail(d, WF_LIGHT_IO_FAILURE);
            return;
        }
        if (!write_reg(d, CONTR, 0x01)) { fail(d, WF_LIGHT_IO_FAILURE); return; }
        if (ended(d)) return;
        ctrl = read_reg(d, CONTR, &d->snapshot.entered_ctrl);
        rate = read_reg(d, RATE, &d->snapshot.entered_rate);
        if (!ctrl || !rate) { fail(d, WF_LIGHT_IO_FAILURE); return; }
        if (d->snapshot.entered_ctrl != 0x01 || d->snapshot.entered_rate != 0x03) {
            fail(d, WF_LIGHT_CONFIG_CHANGED); return;
        }
        d->snapshot.entered_valid = true;
        d->due_ms = now(d) + 200U; /* >=10ms wakeup plus 100ms integration; STATUS is authoritative. */
        return;
    }
    uint8_t ctrl = 0, rate = 0, status = 0;
    bool config_ctrl = read_reg(d, CONTR, &ctrl), config_rate = read_reg(d, RATE, &rate);
    if (!config_ctrl || !config_rate) { fail(d, WF_LIGHT_IO_FAILURE); return; }
    if (ctrl != 0x01 || rate != 0x03) { fail(d, WF_LIGHT_CONFIG_CHANGED); return; }
    if (!read_reg(d, STATUS, &status)) { fail(d, WF_LIGHT_IO_FAILURE); return; }
    if (ended(d)) return;
    d->due_ms = now(d) + 1000U;
    if (!(status & 0x04)) { ++d->snapshot.not_ready; return; }
    uint16_t ch0, ch1;
    if (!read_channels(d, &ch0, &ch1)) { fail(d, WF_LIGHT_IO_FAILURE); return; }
    if (ended(d)) return;
    d->snapshot.status = status;
    d->snapshot.ch0 = ch0; d->snapshot.ch1 = ch1;
    d->snapshot.sampled_ms = now(d);
    ++d->snapshot.samples;
    d->snapshot.saturated = ch0 == UINT16_MAX || ch1 == UINT16_MAX;
    d->snapshot.sample_valid = !(status & 0xf0) && !d->snapshot.saturated;
}
int32_t wf_ltr303_wait_ms(const wf_ltr303_t *d, uint32_t time)
{
    if (d->snapshot.phase != WF_LIGHT_RUNNING) return -1;
    uint32_t elapsed = time - d->snapshot.started_ms;
    if (elapsed >= d->snapshot.duration_ms) return 0;
    uint32_t remaining = d->snapshot.duration_ms - elapsed;
    int32_t due = (int32_t)(d->due_ms - time);
    return due <= 0 ? 0 : (uint32_t)due < remaining ? due : (int32_t)remaining;
}
