#ifndef WRISTFLOW_RADIO_POLICY_H
#define WRISTFLOW_RADIO_POLICY_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { WF_RADIO_UNREADY, WF_RADIO_READY, WF_RADIO_STARTING, WF_RADIO_STARTED, WF_RADIO_STOPPING } wf_radio_adv_t;
typedef enum { WF_RADIO_NONE, WF_RADIO_START, WF_RADIO_STOP, WF_RADIO_DISCONNECT } wf_radio_action_t;
typedef struct {
    bool enabled, connected, disconnecting, failed, pending, adv_blocked, link_blocked;
    wf_radio_adv_t adv, observed_adv;
    uint32_t since, adv_revision, link_revision;
} wf_radio_policy_t;
void wf_radio_init(wf_radio_policy_t *, bool enabled);
void wf_radio_enable(wf_radio_policy_t *, bool enabled, uint32_t now);
void wf_radio_adv(wf_radio_policy_t *, wf_radio_adv_t, bool success, uint32_t now);
void wf_radio_link(wf_radio_policy_t *, bool connected, uint32_t now);
wf_radio_action_t wf_radio_next(wf_radio_policy_t *, uint32_t now);
void wf_radio_failed(wf_radio_policy_t *);
/* Complete a synchronous SDK submission. Only the related callback domain can
 * supersede a rejection; a successful submission still awaits its callback. */
void wf_radio_result(wf_radio_policy_t *, wf_radio_action_t, uint32_t revision, bool accepted);
bool wf_radio_suspended(const wf_radio_policy_t *);
bool wf_radio_settled(const wf_radio_policy_t *);
#endif
