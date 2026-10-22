#include "endurance.h"
void wristflow_endurance_init(wristflow_endurance_t *p, bool enabled, const wristflow_input_observation_t *input)
{
    *p = (wristflow_endurance_t){.enabled = enabled,
        .usb_absent = input->valid && !input->good, .usb_insertions = input->insertions};
}
bool wristflow_endurance_usb(wristflow_endurance_t *p, const wristflow_input_observation_t *input)
{
    if (!p->enabled) return false;
    if (input->insertions != p->usb_insertions) return true;
    if (!input->valid) return false;
    if (!input->good) { p->usb_absent = true; return false; }
    /* The producer remembers a qualified replug even if UI events coalesced
     * and the consumer never saw the intermediate absence. */
    return p->usb_absent;
}
void wristflow_endurance_block(wristflow_endurance_t *p)
{ p->blocked = true; p->holding = false; p->progress = 0; }
bool wristflow_endurance_hold(wristflow_endurance_t *p, bool down, uint32_t id, uint32_t elapsed)
{
    if (!down) { p->holding = p->blocked = false; p->progress = 0; return false; }
    if (p->key_id != id) p->blocked = false; /* Release/repress may coalesce. */
    if (!p->enabled || p->blocked) return false;
    if (p->holding && p->key_id != id) { p->holding = false; p->progress = 0; }
    p->holding = true; p->key_id = id;
    p->progress = (uint16_t)(elapsed >= 3000 ? 1000 : elapsed / 3);
    if (elapsed < 3000) return false;
    wristflow_endurance_block(p); /* One completion per physical press. */
    return true;
}
