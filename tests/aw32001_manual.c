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
    uint8_t expected_current;
    unsigned waits, fault_wait, cancel_wait, delay_wait;
    uint8_t wait_fault;
    bool ignore_fault_clear, lose_pg, drift_current;
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
    if (reg == 9 && !f->ignore_fault_clear) f->regs[9] &= 0xc0; /* exited faults latch-clear on read */
    return ok;
}
static bool write_reg(void *context, uint8_t reg, uint8_t value)
{
    fake_t *f = context;
    bool ok = advance(f);
    assert(reg == 1 || reg == 2 || reg == 3 || reg == 4 || reg == 5 || reg == 11);
    if (reg == 1 && !(value & 8)) {
        ++f->enables;
        assert(f->regs[2] == f->expected_current && (f->regs[4] & 0xfc) == 0x98);
        assert(!(f->regs[5] & 0x60) && !(f->regs[11] & 0x80));
    }
    if (reg == 2) {
        assert(value == 9 || value == 0x18); /* no reset commands */
        assert(f->regs[1] & 8); /* every current change happens inhibited */
    }
    /* A failed ACK can still have changed hardware. Recovery must not assume
     * a failed enable write left charging inhibited. */
    if (!(f->ignore_ceb && reg == 1)) f->regs[reg] = value;
    return ok;
}
static uint32_t now(void *context) { return ((fake_t *)context)->now; }
static bool cancel(void *context) { return ((fake_t *)context)->cancel; }
static bool voltage(void *context, uint16_t *mv)
{ fake_t *f = context; *mv = f->mv; return !f->bad_adc; }
static void wait_ms(void *context, unsigned milliseconds)
{
    fake_t *f = context;
    assert(milliseconds == 150 && (f->regs[1] & 8) && !f->enables);
    ++f->waits; f->now += milliseconds;
    if (f->waits == f->fault_wait) f->regs[9] |= f->wait_fault;
    if (f->waits == f->cancel_wait) f->cancel = true;
    if (f->waits == f->delay_wait) f->now += WF_CHARGE_AUTO_SECONDS * 1000U;
    if (f->lose_pg) f->regs[8] &= (uint8_t)~2;
    if (f->drift_current) f->regs[2] = 9;
}
static fake_t fresh(void)
{
    fake_t f = { .now = 100, .mv = 3960, .expected_current = 9 };
    f.regs[1] = 0xac; f.regs[2] = 9; f.regs[4] = 0x9b;
    f.regs[5] = 0x1a; f.regs[8] = 0x42; f.regs[9] = 0xc0;
    f.regs[10] = 0x49; f.regs[11] = 0x43;
    return f;
}
static wristflow_charge_port_t port(fake_t *f)
{ return (wristflow_charge_port_t){ {f, read_reg, write_reg}, now, cancel, voltage, wait_ms }; }
static bool begin(fake_t *f, wristflow_charge_session_t *s)
{ wristflow_charge_port_t p = port(f); return wristflow_charge_begin(s, &p, 10, f->now); }
static bool begin_auto(fake_t *f, wristflow_charge_session_t *s)
{
    wristflow_charge_port_t p = port(f);
    f->expected_current = 0x18;
    return wristflow_charge_begin_auto(s, &p, f->now);
}
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
    /* Automatic mode allows CV above the legacy entry/stop thresholds, sets
     * termination explicitly, and retains both timer and voltage backstops. */
    f = fresh(); s = (wristflow_charge_session_t){0}; p = port(&f);
    f.mv = 4169; f.regs[3] = 0xa7; f.regs[5] = 0x79;
    assert(begin_auto(&f, &s));
    assert(s.automatic && f.regs[2] == 0x18 && f.regs[3] == 0xa1 && f.regs[5] == 0x1c);
    assert(f.calls == 38 && f.enables == 1);
    assert(s.deadline_ms == 100U + WF_CHARGE_AUTO_SECONDS * 1000U);
    f.mv = 4170; poll(&f, &s); assert(s.state == WF_CHARGE_ENABLED);
    f.mv = 4189; poll(&f, &s); assert(s.state == WF_CHARGE_ENABLED);
    f.regs[8] = 0x5a; poll(&f, &s);
    assert(s.reason == WF_CHARGE_COMPLETE && (f.regs[1] & 8));
    for (unsigned fail = 1; fail <= 38; ++fail) {
        f = fresh(); s = (wristflow_charge_session_t){0}; p = port(&f); f.fail_at = fail;
        assert(!begin_auto(&f, &s));
        assert(s.state == WF_CHARGE_STOPPED && (f.regs[1] & 8));
        if (fail < 37) assert(!f.enables);
    }
    for (unsigned at = 1; at <= 38; ++at) {
        f = fresh(); s = (wristflow_charge_session_t){0}; f.cancel_at = at;
        assert(!begin_auto(&f, &s) && s.reason == WF_CHARGE_MANUAL);
        assert(s.state == WF_CHARGE_STOPPED && (f.regs[1] & 8));
        if (at < 37) assert(!f.enables);
    }
    f = fresh(); s = (wristflow_charge_session_t){0}; p = port(&f);
    assert(begin_auto(&f, &s));
    f.mv = 4190; poll(&f, &s); assert(s.reason == WF_CHARGE_VOLTAGE && (f.regs[1] & 8));
    const uint16_t auto_bad[] = {2999, 4190};
    for (unsigned i = 0; i < sizeof auto_bad / sizeof *auto_bad; ++i) {
        f = fresh(); s = (wristflow_charge_session_t){0}; p = port(&f); f.mv = auto_bad[i];
        assert(!begin_auto(&f, &s) && !f.enables);
    }
    f = fresh(); s = (wristflow_charge_session_t){0}; p = port(&f); f.mv = 3000;
    assert(begin_auto(&f, &s));
    f.regs[3] ^= 1; poll(&f, &s); assert(s.reason == WF_CHARGE_PROFILE && (f.regs[1] & 8));
    f = fresh(); s = (wristflow_charge_session_t){0}; p = port(&f);
    f.now = UINT32_MAX - 1000;
    assert(begin_auto(&f, &s));
    f.now = s.deadline_ms - 1; poll(&f, &s);
    assert(s.reason == WF_CHARGE_TIMEOUT && (f.regs[1] & 8));
    f = fresh(); s = (wristflow_charge_session_t){0}; p = port(&f);
    assert(begin_auto(&f, &s));
    f.regs[5] ^= 2; poll(&f, &s); assert(s.reason == WF_CHARGE_PROFILE && (f.regs[1] & 8));
    f = fresh(); s = (wristflow_charge_session_t){0}; assert(begin_auto(&f, &s));
    f.regs[2] = 9; poll(&f, &s); assert(s.reason == WF_CHARGE_PROFILE && (f.regs[1] & 8));
    /* A recovered VIN latch may qualify only before automatic enable, after
     * two clean spaced samples; retain its evidence instead of hiding it. */
    f = fresh(); s = (wristflow_charge_session_t){0}; f.regs[9] = 0xe0;
    assert(begin_auto(&f, &s) && f.waits == 2 && f.enables == 1);
    assert(f.calls == 56);
    assert(s.entry_fault == 0xe0 && s.input_qualified && !s.reason && s.sample_valid);
    assert(f.now >= 400 && s.fault == 0xc0 && !(s.power & 8));
    f.regs[9] = 0xe0; poll(&f, &s);
    assert(s.state == WF_CHARGE_STOPPED && s.reason == WF_CHARGE_FAULT && f.waits == 2);
    for (unsigned fail = 37; fail <= 56; ++fail) {
        f = fresh(); s = (wristflow_charge_session_t){0}; f.regs[9] = 0x20; f.fail_at = fail;
        assert(!begin_auto(&f, &s) && s.state == WF_CHARGE_STOPPED && (f.regs[1] & 8));
        if (fail < 55) assert(!f.enables);
    }
    for (unsigned at = 37; at <= 56; ++at) {
        f = fresh(); s = (wristflow_charge_session_t){0}; f.regs[9] = 0x20; f.cancel_at = at;
        assert(!begin_auto(&f, &s) && s.reason == WF_CHARGE_MANUAL && s.state == WF_CHARGE_STOPPED);
        if (at < 55) assert(!f.enables);
    }
    const uint8_t no_recover[] = {0x10, 8, 4, 2, 1, 0x28, 0x30};
    for (unsigned i = 0; i < sizeof no_recover; ++i) {
        f = fresh(); s = (wristflow_charge_session_t){0}; f.regs[9] = no_recover[i];
        assert(!begin_auto(&f, &s) && !f.waits && !f.enables);
    }
    for (unsigned at = 1; at <= 2; ++at) {
        f = fresh(); s = (wristflow_charge_session_t){0}; f.regs[9] = 0x20;
        f.cancel_wait = at; assert(!begin_auto(&f, &s) && s.reason == WF_CHARGE_MANUAL && !f.enables);
        f = fresh(); s = (wristflow_charge_session_t){0}; f.regs[9] = 0x20;
        f.delay_wait = at; assert(!begin_auto(&f, &s) && s.reason == WF_CHARGE_TIMEOUT && !f.enables);
        for (unsigned i = 0; i < sizeof no_recover; ++i) {
            f = fresh(); s = (wristflow_charge_session_t){0}; f.regs[9] = 0x20;
            f.fault_wait = at; f.wait_fault = no_recover[i];
            assert(!begin_auto(&f, &s) && s.reason == WF_CHARGE_FAULT && !f.enables);
        }
        f = fresh(); s = (wristflow_charge_session_t){0}; f.regs[9] = 0x20;
        f.fault_wait = at; f.wait_fault = 0x20;
        assert(!begin_auto(&f, &s) && s.reason == WF_CHARGE_FAULT && !f.enables);
    }
    f = fresh(); s = (wristflow_charge_session_t){0}; f.regs[9] = 0x20; f.ignore_fault_clear = true;
    assert(!begin_auto(&f, &s) && f.waits == 1 && !f.enables && s.reason == WF_CHARGE_FAULT);
    f = fresh(); s = (wristflow_charge_session_t){0}; f.regs[9] = 0x20; f.lose_pg = true;
    assert(!begin_auto(&f, &s) && s.reason == WF_CHARGE_INPUT && !f.enables);
    f = fresh(); s = (wristflow_charge_session_t){0}; f.regs[9] = 0x20; f.mv = 178;
    assert(!begin_auto(&f, &s) && s.reason == WF_CHARGE_VOLTAGE && !f.enables);
    f = fresh(); s = (wristflow_charge_session_t){0}; f.regs[9] = 0x20; f.drift_current = true;
    assert(!begin_auto(&f, &s) && s.reason == WF_CHARGE_PROFILE && !f.enables);
    f = fresh(); s = (wristflow_charge_session_t){0}; f.regs[9] = 0x20; f.expected_current = 0x18;
    p = port(&f); p.wait_ms = NULL;
    assert(!wristflow_charge_begin_auto(&s, &p, f.now) && s.reason == WF_CHARGE_FAULT && !f.enables);
    f = fresh(); s = (wristflow_charge_session_t){0}; f.regs[9] = 0x20; f.regs[8] |= 0x80;
    assert(!begin_auto(&f, &s) && s.reason == WF_CHARGE_FAULT && !f.waits && !f.enables);
    puts("manual charge: per-transfer faults/cancel, deadline/wrap, profile, current-enable and stop readback passed");
    return 0;
}
