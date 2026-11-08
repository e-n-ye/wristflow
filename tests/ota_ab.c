#include "ota_ab.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static wf_ota_ab_image_t images[2];
static unsigned checks;
static void setup(void)
{
    for (unsigned i = 0; i < 2; ++i) {
        images[i].bytes = 3028888 + i * 16;
        images[i].release = i + 1;
        memset(images[i].sha256, (int)i + 1, 32);
    }
}
static wf_ota_ab_state_t initial(void)
{
    wf_ota_ab_state_t state;
    assert(wf_ota_ab_provision(&state, 0, &images[0]));
    return state;
}
static void normal(void)
{
    wf_ota_ab_state_t stable = initial(), ready, tried, confirmed;
    wf_ota_ab_plan_t plan;
    uint8_t wire[WF_OTA_AB_BYTES];
    assert(wf_ota_ab_stage(&ready, &stable, &images[1]));
    assert(ready.stable == 0 && ready.pending == 1 && ready.phase == WF_OTA_AB_READY);
    assert(wf_ota_ab_plan(&plan, &ready, images));
    assert(plan.persist && plan.target == WF_OTA_AB_SLOT_B);
    assert(wf_ota_ab_resolve(&plan, NULL, 0) == WF_OTA_AB_SLOT_A);
    tried = plan.next;
    assert(wf_ota_ab_encode(&tried, wire));
    assert(wf_ota_ab_resolve(&plan, wire, sizeof wire) == WF_OTA_AB_SLOT_B);
    assert(wf_ota_ab_confirm(&confirmed, &tried, 1, &images[1]));
    assert(confirmed.phase == WF_OTA_AB_STABLE && confirmed.stable == 1);
    assert(wf_ota_ab_plan(&plan, &confirmed, images));
    assert(!plan.persist && wf_ota_ab_resolve(&plan, NULL, 0) == WF_OTA_AB_SLOT_B);
    assert(wf_ota_ab_stage(&ready, &confirmed, &images[0]));
    assert(ready.stable == 1 && ready.pending == 0);
    ++checks;
}
static void failures(void)
{
    wf_ota_ab_state_t stable = initial(), ready, tried, next;
    wf_ota_ab_plan_t plan;
    wf_ota_ab_image_t checked[2] = {images[0], images[1]};
    uint8_t stale[WF_OTA_AB_BYTES];
    assert(wf_ota_ab_stage(&ready, &stable, &images[1]));
    assert(!wf_ota_ab_stage(&next, &ready, &images[1]));
    assert(!wf_ota_ab_confirm(&next, &ready, 1, &images[1]));
    assert(wf_ota_ab_plan(&plan, &ready, images));
    tried = plan.next;
    assert(!wf_ota_ab_confirm(&next, &tried, 0, &images[0]));
    checked[1].sha256[0] ^= 1;
    assert(!wf_ota_ab_confirm(&next, &tried, 1, &checked[1]));
    assert(wf_ota_ab_plan(&plan, &ready, checked));
    assert(plan.target == WF_OTA_AB_SLOT_A && plan.next.phase == WF_OTA_AB_STABLE);
    assert(wf_ota_ab_plan(&plan, &tried, images));
    assert(plan.target == WF_OTA_AB_SLOT_A && plan.next.phase == WF_OTA_AB_STABLE);
    assert(wf_ota_ab_encode(&ready, stale));
    assert(wf_ota_ab_resolve(&plan, stale, sizeof stale) == WF_OTA_AB_SLOT_A);
    memset(&checked[0], 0, sizeof checked[0]);
    assert(!wf_ota_ab_plan(&plan, &ready, checked));
    assert(wf_ota_ab_resolve(&plan, NULL, 0) == WF_OTA_AB_RECOVERY);
    ready.generation = UINT32_MAX;
    assert(!wf_ota_ab_stage(&next, &ready, &images[1]));
    assert(!wf_ota_ab_plan(&plan, &ready, images));
    ++checks;
}
static void metadata(void)
{
    wf_ota_ab_state_t stable = initial(), ready, decoded;
    uint8_t a[WF_OTA_AB_BYTES], b[WF_OTA_AB_BYTES], bad[WF_OTA_AB_BYTES];
    assert(wf_ota_ab_encode(&stable, a));
    assert(wf_ota_ab_stage(&ready, &stable, &images[1]));
    assert(wf_ota_ab_encode(&ready, b));
    for (unsigned bit = 0; bit < WF_OTA_AB_BYTES * 8; ++bit) {
        memcpy(bad, b, sizeof bad);
        bad[bit / 8] ^= (uint8_t)(1u << (bit % 8));
        assert(!wf_ota_ab_decode(&decoded, bad, sizeof bad));
        assert(wf_ota_ab_select(&decoded, a, sizeof a, bad, sizeof bad) == 0);
        assert(decoded.phase == WF_OTA_AB_STABLE && decoded.stable == 0);
        ++checks;
    }
    assert(wf_ota_ab_select(&decoded, a, sizeof a, b, sizeof b) == 1);
    assert(wf_ota_ab_select(&decoded, b, sizeof b, a, sizeof a) == 0);
    memset(bad, 0xFF, sizeof bad);
    assert(wf_ota_ab_select(&decoded, bad, sizeof bad, bad, sizeof bad) == -1);
    for (size_t n = 0; n < sizeof a; ++n) assert(!wf_ota_ab_decode(&decoded, a, n));
    ready.generation = stable.generation;
    assert(wf_ota_ab_encode(&ready, b));
    assert(wf_ota_ab_select(&decoded, a, sizeof a, b, sizeof b) == -1);
}
static void power_cuts(void)
{
    wf_ota_ab_state_t states[5], selected;
    wf_ota_ab_plan_t plan;
    states[0] = initial();
    assert(wf_ota_ab_stage(&states[1], &states[0], &images[1]));
    assert(wf_ota_ab_plan(&plan, &states[1], images));
    states[2] = plan.next;
    assert(wf_ota_ab_confirm(&states[3], &states[2], 1, &images[1]));
    assert(wf_ota_ab_plan(&plan, &states[2], images));
    states[4] = plan.next;
    const unsigned from[] = {0, 1, 2, 2};
    const unsigned to[] = {1, 2, 3, 4};
    for (unsigned transition = 0; transition < 4; ++transition) {
        uint8_t old[WF_OTA_AB_BYTES], full[WF_OTA_AB_BYTES], interrupted[WF_OTA_AB_BYTES];
        assert(wf_ota_ab_encode(&states[from[transition]], old));
        assert(wf_ota_ab_encode(&states[to[transition]], full));
        for (size_t cut = 0; cut <= sizeof full; ++cut) {
            memset(interrupted, 0xFF, sizeof interrupted);
            memcpy(interrupted, full, cut);
            int record = wf_ota_ab_select(&selected, old, sizeof old, interrupted, sizeof interrupted);
            assert(record == (cut == sizeof full ? 1 : 0));
            assert(wf_ota_ab_plan(&plan, &selected, images));
            /* After a reset, TRIED always rolls back; READY cannot boot the
             * candidate without another exact persistent readback. */
            wf_ota_ab_target_t boot = wf_ota_ab_resolve(&plan, NULL, 0);
            assert(boot == ((transition == 2 && cut == sizeof full) ? WF_OTA_AB_SLOT_B : WF_OTA_AB_SLOT_A));
            if (transition == 1) {
                assert(wf_ota_ab_plan(&plan, &states[1], images));
                assert(wf_ota_ab_resolve(&plan, interrupted, sizeof interrupted) ==
                       (cut == sizeof full ? WF_OTA_AB_SLOT_B : WF_OTA_AB_SLOT_A));
            }
            ++checks;
        }
    }
}
static void corrupted_trial_marker(void)
{
    wf_ota_ab_state_t stable = initial(), ready, selected;
    wf_ota_ab_plan_t plan;
    uint8_t previous[WF_OTA_AB_BYTES], consumed[WF_OTA_AB_BYTES], bad[WF_OTA_AB_BYTES];
    assert(wf_ota_ab_stage(&ready, &stable, &images[1]));
    assert(wf_ota_ab_encode(&ready, previous));
    assert(wf_ota_ab_plan(&plan, &ready, images));
    assert(wf_ota_ab_encode(&plan.next, consumed));
    for (unsigned bit = 0; bit < WF_OTA_AB_BYTES * 8; ++bit) {
        memcpy(bad, consumed, sizeof bad);
        bad[bit / 8] ^= (uint8_t)(1u << (bit % 8));
        assert(wf_ota_ab_select(&selected, previous, sizeof previous, bad, sizeof bad) == 0);
        assert(selected.phase == WF_OTA_AB_TRIED);
        assert(wf_ota_ab_plan(&plan, &selected, images));
        assert(plan.target == WF_OTA_AB_SLOT_A);
        assert(wf_ota_ab_resolve(&plan, NULL, 0) == WF_OTA_AB_SLOT_A);
        ++checks;
    }
}
int main(void)
{
    setup(); normal(); failures(); metadata(); power_cuts(); corrupted_trial_marker();
    printf("OTA A/B model: %u corruption/cut/lifecycle checks passed; no physical flash or boot integration\n", checks);
    return 0;
}
