#include "aw32001_manual.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t regs[13];
    uint32_t now;
    unsigned calls, fail_at, cancel_at, delay_at, enables;
    bool cancel, bad_adc, ignore_ceb;
    uint16_t mv;
} fake_t;
static bool advance(fake_t *f)
{
    ++f->calls; ++f->now;
    if (f->calls == f->cancel_at) f->cancel = true;
    if (f->calls == f->delay_at) f->now += 5000;
    return f->calls != f->fail_at;
}
static bool read_reg(void *context, uint8_t reg, uint8_t *value)
{
    fake_t *f = context;
    bool ok = advance(f);
    *value = f->regs[reg];
    if (reg == 9) f->regs[9] &= 0xc0; /* exited faults latch-clear on read */
    return ok;
}
static bool write_reg(void *context, uint8_t reg, uint8_t value)
{
    fake_t *f = context;
    bool ok = advance(f);
    assert(reg == 1 || reg == 2 || reg == 4 || reg == 5 || reg == 11);
    if (reg == 1 && !(value & 8)) {
        ++f->enables;
        assert(f->regs[2] == 9 && (f->regs[4] & 0xfc) == 0x98);
        assert(!(f->regs[5] & 0x60) && !(f->regs[11] & 0x80));
    }
    if (reg == 2) assert(value == 9); /* no reset commands */
    /* A failed ACK can still have changed hardware. Recovery must not assume
     * a failed enable write left charging inhibited. */
    if (!(f->ignore_ceb && reg == 1)) f->regs[reg] = value;
    return ok;
}
static uint32_t now(void *context) { return ((fake_t *)context)->now; }
static bool cancel(void *context) { return ((fake_t *)context)->cancel; }
static bool voltage(void *context, uint16_t *mv)
{ fake_t *f = context; *mv = f->mv; return !f->bad_adc; }
static fake_t fresh(void)
{
    fake_t f = { .now = 100, .mv = 3960 };
    f.regs[1] = 0xac; f.regs[2] = 9; f.regs[4] = 0x9b;
    f.regs[5] = 0x1a; f.regs[8] = 0x42; f.regs[9] = 0xc0;
    f.regs[10] = 0x49; f.regs[11] = 0x43;
    return f;
}
static wristflow_charge_port_t port(fake_t *f)
{ return (wristflow_charge_port_t){ {f, read_reg, write_reg}, now, cancel, voltage }; }
static bool begin(fake_t *f, wristflow_charge_session_t *s)
{ wristflow_charge_port_t p = port(f); return wristflow_charge_begin(s, &p, 10, f->now); }
static void poll(fake_t *f, wristflow_charge_session_t *s)
{ wristflow_charge_port_t p = port(f); wristflow_charge_poll(s, &p); }
static void stop(fake_t *f, wristflow_charge_session_t *s)
{ wristflow_charge_port_t p = port(f); wristflow_charge_stop(s, &p, WF_CHARGE_MANUAL); }

int main(void)
{
    fake_t f = fresh();
    wristflow_charge_session_t s = {0};
    assert(begin(&f, &s) && f.calls == 32 && f.enables == 1);
    assert(s.state == WF_CHARGE_ENABLED && !(f.regs[1] & 8));
    unsigned before = f.calls;
    assert(!begin(&f, &s) && f.calls == before); /* no renewal/reconfigure */
    stop(&f, &s);
    assert(s.state == WF_CHARGE_STOPPED && !s.stop_error && (f.regs[1] & 8));
    before = f.calls; poll(&f, &s); assert(f.calls == before);

    /* Every failed transport in start must avoid remaining enabled, including
     * a failed ACK after the actual enable side effect. Stop succeeds here. */
    for (unsigned fail = 1; fail <= 32; ++fail) {
        f = fresh(); s = (wristflow_charge_session_t){0}; f.fail_at = fail;
        assert(!begin(&f, &s));
        assert(s.state == WF_CHARGE_STOPPED && (f.regs[1] & 8));
        assert(f.enables <= (fail >= 31 ? 1U : 0U));
    }
    for (unsigned at = 1; at <= 32; ++at) {
        f = fresh(); s = (wristflow_charge_session_t){0}; f.cancel_at = at;
        assert(!begin(&f, &s) && s.reason == WF_CHARGE_MANUAL);
        assert(s.state == WF_CHARGE_STOPPED && (f.regs[1] & 8));
        if (at < 31) assert(!f.enables);
    }
    for (unsigned fail = 1; fail <= 4; ++fail) {
        f = fresh(); s = (wristflow_charge_session_t){0}; assert(begin(&f, &s));
        f.fail_at = f.calls + fail;
        stop(&f, &s);
        assert(s.state == WF_CHARGE_UNCONFIRMED && s.stop_error);
        before = f.calls; assert(!begin(&f, &s) && f.calls == before);
        f.fail_at = 0; stop(&f, &s);
        assert(s.state == WF_CHARGE_STOPPED && (f.regs[1] & 8));
    }
    f = fresh(); s = (wristflow_charge_session_t){0}; assert(begin(&f, &s));
    f.ignore_ceb = true; stop(&f, &s);
    assert(s.state == WF_CHARGE_UNCONFIRMED && s.stop_error == WF_AW32001_VERIFY_FAILED);

    f = fresh(); s = (wristflow_charge_session_t){0}; f.delay_at = 1;
    wristflow_charge_port_t p = port(&f);
    assert(!wristflow_charge_begin(&s, &p, 1, 100));
    assert(!f.enables && s.reason == WF_CHARGE_TIMEOUT && s.state == WF_CHARGE_STOPPED);
    f = fresh(); s = (wristflow_charge_session_t){0}; f.now = 5000; p = port(&f);
    assert(!wristflow_charge_begin(&s, &p, 1, 100) && !f.enables);
    f = fresh(); s = (wristflow_charge_session_t){0}; f.now = UINT32_MAX - 100;
    assert(begin(&f, &s));
    f.now = s.deadline_ms; poll(&f, &s);
    assert(s.reason == WF_CHARGE_TIMEOUT && (f.regs[1] & 8));

    /* Entry checks: shipping config is not a fault; first latched fault is
     * retained in diagnostics and cannot be read twice to pass the same start. */
    f = fresh(); s = (wristflow_charge_session_t){0}; f.regs[9] = 0xe0;
    assert(!begin(&f, &s) && s.reason == WF_CHARGE_FAULT && s.fault == 0xe0 && !f.enables);
    const uint8_t timers[] = {0x0a, 0x12, 0x1b};
    for (unsigned i=0; i<sizeof timers; ++i) {
        f = fresh(); s = (wristflow_charge_session_t){0}; f.regs[5] = timers[i];
        assert(!begin(&f, &s) && s.reason == WF_CHARGE_PROFILE && !f.enables);
    }
    const uint16_t bad_mv[] = {0, 3499, 4101, 5000};
    for (unsigned i=0; i<sizeof bad_mv/sizeof *bad_mv; ++i) {
        f = fresh(); s = (wristflow_charge_session_t){0}; f.mv = bad_mv[i];
        assert(!begin(&f, &s) && s.reason == WF_CHARGE_VOLTAGE && !f.enables);
    }
    f = fresh(); s = (wristflow_charge_session_t){0}; f.bad_adc = true;
    assert(!begin(&f, &s) && !f.enables);
    f = fresh(); s = (wristflow_charge_session_t){0}; f.regs[10] = 0;
    assert(!begin(&f, &s) && s.state == WF_CHARGE_UNCONFIRMED && !f.enables);
    f = fresh(); s = (wristflow_charge_session_t){0}; f.ignore_ceb = true;
    assert(!begin(&f, &s) && s.reason == WF_CHARGE_PROFILE && f.enables == 1 && (f.regs[1] & 8));

    const unsigned regs[] = {1, 2, 4, 5, 11, 8, 9};
    const uint8_t masks[] = {8, 1, 4, 1, 0x80, 2, 0x20};
    for (unsigned i=0; i<sizeof regs/sizeof *regs; ++i) {
        f = fresh(); s = (wristflow_charge_session_t){0}; assert(begin(&f, &s));
        f.regs[regs[i]] ^= masks[i]; poll(&f, &s);
        assert(s.state == WF_CHARGE_STOPPED && (f.regs[1] & 8) && f.enables == 1);
        before = f.calls; poll(&f, &s); assert(before == f.calls); /* no restart */
    }
    f = fresh(); s = (wristflow_charge_session_t){0}; assert(begin(&f, &s));
    f.regs[8] = 0x5a; poll(&f, &s);
    assert(s.reason == WF_CHARGE_COMPLETE && (f.regs[1] & 8));
    f = fresh(); s = (wristflow_charge_session_t){0}; assert(begin(&f, &s));
    f.mv = 4170; poll(&f, &s); assert(s.reason == WF_CHARGE_VOLTAGE && (f.regs[1] & 8));
    f = fresh(); s = (wristflow_charge_session_t){0}; assert(begin(&f, &s));
    f.delay_at = f.calls + 1; f.now = s.deadline_ms - 1000; poll(&f, &s);
    assert(s.reason == WF_CHARGE_TIMEOUT && (f.regs[1] & 8));
    f = fresh(); s = (wristflow_charge_session_t){0}; p = port(&f);
    assert(!wristflow_charge_begin(&s, &p, 0, 100));
    assert(!wristflow_charge_begin(&s, &p, 61, 100) && !f.calls);
    puts("manual charge: per-transfer faults/cancel, deadline/wrap, profile, current-enable and stop readback passed");
    return 0;
}
