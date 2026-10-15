#include "aw32001_manual.h"
#include <stddef.h>

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
        /* REG09 fault bits can clear on read. Reject this first observed value;
         * never read again in the same start attempt to turn it into a pass. */
    }
    if (s->id != 0x49 || (s->power & 8) != (entry ? 8 : 0) ||
        (s->timer & 0x79) != 0x18 || s->current != 9 ||
        (s->voltage & 0xfc) != 0x98 || (s->individual & 0x80)) {
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
    if (s->millivolts < WF_CHARGE_ENTRY_MIN_MV ||
        s->millivolts >= (entry ? WF_CHARGE_ENTRY_MAX_MV + 1 : WF_AW32001_PROFILE_MV)) {
        s->reason = WF_CHARGE_VOLTAGE;
        return false;
    }
    s->sample_valid = true;
    s->observed_ms = t->port->now_ms(t->port->io.context);
    return true;
}

bool wristflow_charge_begin(wristflow_charge_session_t *s, const wristflow_charge_port_t *p,
                           unsigned seconds, uint32_t accepted_ms)
{
    if (!s || !p || !p->io.read || !p->io.write || !p->now_ms || !p->cancelled ||
        !p->voltage || !seconds || seconds > WF_CHARGE_MAX_SECONDS ||
        s->state == WF_CHARGE_ENABLED || s->state == WF_CHARGE_UNCONFIRMED) return false;
    *s = (wristflow_charge_session_t){ .deadline_ms = accepted_ms + seconds * 1000U };
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
    if (!sample(&t, true)) goto failed;
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
