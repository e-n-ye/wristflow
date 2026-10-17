#include "aw32001_manual.h"
#include <stddef.h>

_Static_assert(WF_CHARGE_AUTO_MA % 8 == 0 && WF_CHARGE_AUTO_CURRENT_CODE <= 63,
               "automatic charge current must fit REG02 without reset bits");

typedef struct {
    wristflow_charge_session_t *session;
    const wristflow_charge_port_t *port;
} transaction_t;

static bool permitted(transaction_t *t)
{
    if (t->port->cancelled(t->port->io.context)) {
        t->session->reason = WF_CHARGE_MANUAL;
        return false;
    }
    if ((int32_t)(t->port->now_ms(t->port->io.context) - t->session->deadline_ms) >= 0) {
        t->session->reason = WF_CHARGE_TIMEOUT;
        return false;
    }
    return true;
}

static bool guarded_read(void *context, uint8_t reg, uint8_t *value)
{
    transaction_t *t = context;
    if (!permitted(t)) return false;
    bool ok = t->port->io.read(t->port->io.context, reg, value);
    if (!ok) {
        t->session->cause_error = WF_AW32001_READ_FAILED;
        t->session->cause_reg = reg;
    }
    return permitted(t) && ok;
}

static bool guarded_write(void *context, uint8_t reg, uint8_t value)
{
    transaction_t *t = context;
    if (!permitted(t)) return false;
    bool ok = t->port->io.write(t->port->io.context, reg, value);
    if (!ok) {
        t->session->cause_error = WF_AW32001_WRITE_FAILED;
        t->session->cause_reg = reg;
    }
    return permitted(t) && ok;
}

/* Stop deliberately ignores cancellation/deadline: they must never suppress the
 * attempt to inhibit. Never reuse a stale REG01 or echo REG02 reset commands. */
void wristflow_charge_stop(wristflow_charge_session_t *s, const wristflow_charge_port_t *p,
                          wristflow_charge_reason_t reason)
{
    s->state = WF_CHARGE_UNCONFIRMED;
    s->sample_valid = false;
    s->reason = reason;
    s->stop_error = WF_AW32001_READ_FAILED;
    s->stop_reg = 0x0a;
    if (!p->io.read(p->io.context, 0x0a, &s->id)) return;
    if (s->id != 0x49) { s->stop_error = WF_AW32001_ID_MISMATCH; return; }
    s->stop_reg = 1;
    if (!p->io.read(p->io.context, 1, &s->power)) return;
    s->stop_error = WF_AW32001_WRITE_FAILED;
    if (!p->io.write(p->io.context, 1, s->power | 8)) return;
    s->stop_error = WF_AW32001_READ_FAILED;
    if (!p->io.read(p->io.context, 1, &s->power)) return;
    s->stop_error = WF_AW32001_VERIFY_FAILED;
    if (!(s->power & 8)) return;
    s->stop_error = WF_AW32001_OK;
    s->stop_reg = 0;
    s->state = WF_CHARGE_STOPPED;
    s->observed_ms = p->now_ms(p->io.context);
}

static bool sample(transaction_t *t, bool entry)
{
    wristflow_charge_session_t *s = t->session;
    s->sample_valid = false;
    const uint8_t regs[] = { 0x0a, 1, 5, 2, 4, 0x0b, 8, 9 };
    uint8_t *values[] = { &s->id, &s->power, &s->timer, &s->current,
                         &s->voltage, &s->individual, &s->status, &s->fault };
    for (unsigned i = 0; i < sizeof regs; ++i) {
        if (!guarded_read(t, regs[i], values[i])) return false;
        /* REG09 may clear exited latches on read. Always expose the first fault;
         * only begin_session may qualify a pure recovered VIN latch before enable. */
    }
    if (s->automatic && !guarded_read(t, 3, &s->termination)) return false;
    if (s->id != 0x49 || (s->power & 8) != (entry ? 8 : 0) ||
        (s->timer & 0x79) != 0x18 ||
        s->current != (s->automatic ? WF_CHARGE_AUTO_CURRENT_CODE : WF_AW32001_PROFILE_MA / 8 - 1) ||
        (s->voltage & 0xfc) != 0x98 || (s->individual & 0x80) ||
        (s->automatic && ((s->termination & 15) != 1 || (s->timer & 0x7f) != 0x1c))) {
        s->reason = WF_CHARGE_PROFILE;
        return false;
    }
    if (!(s->status & 2)) { s->reason = WF_CHARGE_INPUT; return false; }
    if ((s->status & 0x80) || (s->fault & 0x3f)) {
        s->reason = WF_CHARGE_FAULT;
        return false;
    }
    /* PPM/thermal regulation are not fault bits. Reject them conservatively
     * for this first short experiment, without claiming cell-temperature data. */
    if (s->status & 5) { s->reason = WF_CHARGE_REGULATION; return false; }
    if (!permitted(t)) return false;
    if (!t->port->voltage(t->port->io.context, &s->millivolts)) {
        s->reason = WF_CHARGE_VOLTAGE;
        return false;
    }
    if (!permitted(t)) return false;
    unsigned minimum = s->automatic ? WF_CHARGE_AUTO_MIN_MV : WF_CHARGE_ENTRY_MIN_MV;
    unsigned limit = s->automatic ? WF_CHARGE_AUTO_STOP_MV :
        (entry ? WF_CHARGE_ENTRY_MAX_MV + 1 : WF_AW32001_PROFILE_MV);
    if (s->millivolts < minimum || s->millivolts >= limit) {
        s->reason = WF_CHARGE_VOLTAGE;
        return false;
    }
    s->sample_valid = true;
    s->observed_ms = t->port->now_ms(t->port->io.context);
    return true;
}

static bool begin_session(wristflow_charge_session_t *s, const wristflow_charge_port_t *p,
                          unsigned seconds, uint32_t accepted_ms, bool automatic)
{
    if (!s || !p || !p->io.read || !p->io.write || !p->now_ms || !p->cancelled ||
        !p->voltage || !seconds || seconds > (automatic ? WF_CHARGE_AUTO_SECONDS : WF_CHARGE_MAX_SECONDS) ||
        s->state == WF_CHARGE_ENABLED || s->state == WF_CHARGE_UNCONFIRMED) return false;
    *s = (wristflow_charge_session_t){ .deadline_ms = accepted_ms + seconds * 1000U,
                                      .automatic = automatic };
    transaction_t t = { s, p };
    wristflow_aw32001_io_t guarded = { &t, guarded_read, guarded_write };
    wristflow_aw32001_result_t prepared;
    if (!permitted(&t)) goto failed;
    /* Check compatibility before any run-time write. Early boot remains its
     * separate unchanged inhibit-only transaction. */
    if (!guarded_read(&t, 0x0a, &s->id)) goto failed;
    if (s->id != 0x49) { s->reason = WF_CHARGE_PROFILE; goto failed; }
    if (wristflow_aw32001_prepare(&guarded, &prepared) != WF_AW32001_OK) {
        if (!s->cause_error && s->reason == WF_CHARGE_NONE) {
            s->cause_error = prepared.error;
            s->cause_reg = prepared.error_reg;
        }
        goto failed;
    }
    if (automatic) {
        uint8_t termination, timer;
        /* AW32001E V1.4 pp36-38: set200mA while inhibited, never echo REG02
         * reset bits; retain discharge limit, set ITERM=3mA;
         * EN_TERM=1, EN_TIMER=1, CHG_TMR=8h, TERM_TMR=0, watchdog=off. */
        if (!guarded_write(&t, 2, WF_CHARGE_AUTO_CURRENT_CODE) ||
            !guarded_read(&t, 3, &termination) ||
            !guarded_write(&t, 3, (termination & 0xf0) | 1) ||
            !guarded_read(&t, 5, &timer) ||
            !guarded_write(&t, 5, (timer & 0x80) | 0x1c)) goto failed;
    }
    if (!sample(&t, true)) {
        /* V1.4 pp18,29: VIN_FAULT latches an earlier UVLO/OVP even after PG
         * recovers. Preserve that first value; qualify only this entry case,
         * while CEB stays1. Never recover an active-session or other fault. */
        if (!automatic || s->reason != WF_CHARGE_FAULT ||
            (s->fault & 0x3f) != 0x20 || (s->status & 0x87) != 2 || !p->wait_ms)
            goto failed;
        s->entry_fault = s->fault;
        s->reason = WF_CHARGE_NONE;
        for (unsigned i = 0; i < 2; ++i) {
            if (!permitted(&t)) goto failed;
            p->wait_ms(p->io.context, WF_CHARGE_INPUT_SETTLE_MS);
            if (!permitted(&t) || !sample(&t, true)) goto failed;
        }
        s->input_qualified = true;
    }
    if (((s->status >> 3) & 3) == 3) { s->reason = WF_CHARGE_COMPLETE; goto failed; }
    if (!guarded_write(&t, 1, s->power & (uint8_t)~8) ||
        !guarded_read(&t, 1, &s->power)) goto failed;
    if (s->power & 8) { s->reason = WF_CHARGE_PROFILE; goto failed; }
    s->state = WF_CHARGE_ENABLED; /* CEB readback, not proof of charge current. */
    s->observed_ms = p->now_ms(p->io.context);
    return true;
failed:
    wristflow_charge_stop(s, p, s->reason == WF_CHARGE_NONE ? WF_CHARGE_IO : s->reason);
    return false;
}

bool wristflow_charge_begin(wristflow_charge_session_t *s, const wristflow_charge_port_t *p,
                           unsigned seconds, uint32_t accepted_ms)
{ return begin_session(s, p, seconds, accepted_ms, false); }

bool wristflow_charge_begin_auto(wristflow_charge_session_t *s, const wristflow_charge_port_t *p,
                                uint32_t accepted_ms)
{ return begin_session(s, p, WF_CHARGE_AUTO_SECONDS, accepted_ms, true); }

void wristflow_charge_poll(wristflow_charge_session_t *s, const wristflow_charge_port_t *p)
{
    if (s->state != WF_CHARGE_ENABLED) return;
    transaction_t t = { s, p };
    if (!sample(&t, false)) {
        wristflow_charge_stop(s, p, s->reason == WF_CHARGE_NONE ? WF_CHARGE_IO : s->reason);
    } else if (((s->status >> 3) & 3) == 3) {
        wristflow_charge_stop(s, p, WF_CHARGE_COMPLETE);
    }
}
