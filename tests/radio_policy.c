#include "radio_policy.h"
#include <assert.h>
int main(void)
{
    wf_radio_policy_t p;
    wf_radio_init(&p, true);
    assert(wf_radio_next(&p,0)==WF_RADIO_NONE);
    assert(wf_radio_next(&p,10000)==WF_RADIO_NONE && !p.failed);
    wf_radio_adv(&p, WF_RADIO_READY, true, 0);
    assert(wf_radio_next(&p, 0) == WF_RADIO_START);
    wf_radio_enable(&p, false, 1); /* Pause while starting. */
    assert(wf_radio_next(&p, 2) == WF_RADIO_NONE && !wf_radio_suspended(&p));
    wf_radio_adv(&p, WF_RADIO_STARTED, true, 3);
    assert(wf_radio_next(&p, 3) == WF_RADIO_STOP);
    wf_radio_enable(&p, true, 4); /* Reverse while stopping: wait. */
    assert(wf_radio_next(&p, 4) == WF_RADIO_NONE);
    wf_radio_adv(&p, WF_RADIO_READY, true, 5);
    assert(wf_radio_next(&p, 5) == WF_RADIO_START);
    wf_radio_adv(&p, WF_RADIO_STARTED, true, 6);
    assert(wf_radio_settled(&p));
    wf_radio_enable(&p, false, 7);
    wf_radio_adv(&p, WF_RADIO_READY, true, 8);
    wf_radio_link(&p, true, 9); /* Late connect after a stop indication. */
    assert(!wf_radio_suspended(&p));
    assert(wf_radio_next(&p, 9) == WF_RADIO_DISCONNECT);
    assert(wf_radio_next(&p, 10) == WF_RADIO_NONE);
    wf_radio_link(&p, false, 11);
    assert(wf_radio_suspended(&p));
    wf_radio_enable(&p, true, UINT32_MAX - 10);
    assert(wf_radio_next(&p, UINT32_MAX - 10) == WF_RADIO_START);
    assert(wf_radio_next(&p, 4990) == WF_RADIO_NONE && p.failed); /* Wrap + timeout. */
    wf_radio_adv(&p, WF_RADIO_UNREADY, false, 4991);
    assert(!wf_radio_settled(&p));
    /* A rejection superseded by its own callback cannot roll back facts. */
    wf_radio_init(&p, false);
    wf_radio_adv(&p, WF_RADIO_STARTED, true, 0);
    assert(wf_radio_next(&p, 0) == WF_RADIO_STOP);
    uint32_t revision = p.adv_revision;
    wf_radio_adv(&p, WF_RADIO_READY, true, 1);
    wf_radio_link(&p, true, 1);
    wf_radio_result(&p, WF_RADIO_STOP, revision, false);
    assert(!p.failed && p.adv == WF_RADIO_READY);
    assert(wf_radio_next(&p, 2) == WF_RADIO_DISCONNECT);
    revision = p.link_revision;
    wf_radio_link(&p, false, 3);
    wf_radio_result(&p, WF_RADIO_DISCONNECT, revision, false);
    assert(wf_radio_suspended(&p));
    /* An unrelated callback cannot erase a real rejection. No busy retry. */
    wf_radio_adv(&p, WF_RADIO_STARTED, true, 4);
    assert(wf_radio_next(&p, 4) == WF_RADIO_STOP);
    revision = p.adv_revision;
    wf_radio_link(&p, true, 4);
    wf_radio_result(&p, WF_RADIO_STOP, revision, false);
    assert(p.failed && p.adv == WF_RADIO_STARTED);
    assert(wf_radio_next(&p, 5) == WF_RADIO_DISCONNECT);
    wf_radio_link(&p, false, 6);
    assert(wf_radio_next(&p, 6) == WF_RADIO_NONE);
    for (unsigned i = 20; i < 6000; i += 20) {
        wf_radio_enable(&p, false, i);
        assert(p.failed && wf_radio_next(&p, i) == WF_RADIO_NONE);
    }
    /* Timeout is evidence, but late activity still receives finite cleanup. */
    wf_radio_adv(&p, WF_RADIO_STARTED, true, 6000);
    assert(wf_radio_next(&p, 6000) == WF_RADIO_STOP && p.failed);
    wf_radio_adv(&p, WF_RADIO_READY, true, 6001);
    wf_radio_link(&p, true, 6002);
    assert(wf_radio_next(&p, 6002) == WF_RADIO_DISCONNECT);
    wf_radio_result(&p, WF_RADIO_DISCONNECT, p.link_revision, false);
    assert(wf_radio_next(&p, 6003) == WF_RADIO_NONE && p.failed);
    return 0;
}
