#include "radio_policy.h"
void wf_radio_init(wf_radio_policy_t *p, bool enabled)
{ *p = (wf_radio_policy_t){.enabled = enabled}; }
void wf_radio_enable(wf_radio_policy_t *p, bool enabled, uint32_t now)
{
    if (p->enabled == enabled) return;
    p->enabled = enabled; p->failed = false; p->pending = true; p->since = now;
    p->adv_blocked = p->link_blocked = false;
}
void wf_radio_adv(wf_radio_policy_t *p, wf_radio_adv_t adv, bool success, uint32_t now)
{
    p->adv = p->observed_adv = adv;
    ++p->adv_revision; p->adv_blocked = false;
    if (!success) p->failed = true;
    /* A callback does not reset an outstanding disconnect deadline. */
    if (!p->disconnecting && !p->pending) p->since = now;
}
void wf_radio_link(wf_radio_policy_t *p, bool connected, uint32_t now)
{
    p->connected = connected;
    ++p->link_revision; p->link_blocked = false; p->disconnecting = false;
    if (!p->pending) p->since = now;
}
void wf_radio_failed(wf_radio_policy_t *p) { p->failed = true; }
void wf_radio_result(wf_radio_policy_t *p, wf_radio_action_t action, uint32_t revision, bool accepted)
{
    if (accepted || action == WF_RADIO_NONE) return;
    if (action == WF_RADIO_DISCONNECT) {
        if (revision != p->link_revision) return;
        p->disconnecting = false; p->link_blocked = true;
    } else {
        if (revision != p->adv_revision) return;
        p->adv = p->observed_adv; p->adv_blocked = true;
    }
    p->failed = true;
}
bool wf_radio_suspended(const wf_radio_policy_t *p)
{ return !p->enabled && !p->failed && p->adv == WF_RADIO_READY && !p->connected && !p->disconnecting; }
bool wf_radio_settled(const wf_radio_policy_t *p)
{
    return !p->failed && (p->enabled ?
        (p->connected && !p->disconnecting && p->adv == WF_RADIO_READY) ||
        (!p->connected && p->adv == WF_RADIO_STARTED) : wf_radio_suspended(p));
}
wf_radio_action_t wf_radio_next(wf_radio_policy_t *p, uint32_t now)
{
    if (wf_radio_settled(p)) { p->pending = false; return WF_RADIO_NONE; }
    /* POWER_ON has no product deadline. Start the transition deadline only
     * after initialization is observed; an explicit init failure stays failed. */
    if (p->adv == WF_RADIO_UNREADY && !p->failed) {
        p->pending = false;
        return WF_RADIO_NONE;
    }
    if (!p->pending) { p->pending = true; p->since = now; }
    if (now - p->since >= 5000U) p->failed = true;
    /* Failure is sticky evidence. A late activity callback must still get one
     * cleanup attempt. A rejected command cannot spin against the same fact. */
    if (!p->enabled && p->connected && !p->disconnecting && !p->link_blocked) {
        p->disconnecting = true;
        return WF_RADIO_DISCONNECT;
    }
    if ((!p->enabled || p->connected) && p->adv == WF_RADIO_STARTED && !p->adv_blocked) {
        p->adv = WF_RADIO_STOPPING;
        return WF_RADIO_STOP;
    }
    if (!p->failed && p->enabled && !p->connected && !p->disconnecting &&
        p->adv == WF_RADIO_READY && !p->adv_blocked) {
        p->adv = WF_RADIO_STARTING;
        return WF_RADIO_START;
    }
    return WF_RADIO_NONE;
}
