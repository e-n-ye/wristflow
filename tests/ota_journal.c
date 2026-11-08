#include "ota_journal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t before[16], unit[2][4096], after[16];
    unsigned calls, fail_call, programs, erases, locked, locks, unlocks;
    int cut, erase_cut;
    bool lie_erase, lie_program, damage_peer;
    uint32_t time;
} nor_t;
static unsigned checks;
static bool broken(nor_t *n) { return ++n->calls == n->fail_call; }
static int read_bytes(void *ctx, unsigned unit, uint32_t off, void *out, size_t size)
{
    nor_t *n = ctx;
    assert(n->locked && unit < 2 && off + size <= 4096);
    if (broken(n)) return (int)size - 1;
    memcpy(out, n->unit[unit] + off, size);
    return (int)size;
}
static int program(void *ctx, unsigned unit, const void *bytes, size_t size)
{
    nor_t *n = ctx;
    assert(n->locked && unit < 2 && size == WF_OTA_AB_BYTES);
    ++n->programs;
    if (broken(n)) return -1;
    if (n->lie_program) return (int)size;
    if (n->damage_peer) n->unit[1u - unit][16] ^= 1;
    size_t count = n->cut >= 0 ? (size_t)n->cut : size;
    for (size_t i = 0; i < count; ++i) n->unit[unit][i] &= ((const uint8_t *)bytes)[i];
    return n->cut >= 0 ? -1 : (int)size;
}
static int erase(void *ctx, unsigned unit)
{
    nor_t *n = ctx;
    assert(n->locked && unit < 2);
    ++n->erases;
    if (broken(n)) return -1;
    if (n->erase_cut >= 0) {
        memset(n->unit[unit], 0xFF, (size_t)n->erase_cut);
        return -1;
    }
    if (!n->lie_erase) memset(n->unit[unit], 0xFF, 4096);
    return 0;
}
static int sync_bytes(void *ctx) { return broken(ctx) ? -1 : 0; }
static bool acquire(void *ctx)
{ nor_t *n = ctx; if (n->locked) return false; ++n->locks; n->locked = 1; return true; }
static void release(void *ctx)
{ nor_t *n = ctx; assert(n->locked); n->locked = 0; ++n->unlocks; }
static uint32_t now(void *ctx) { return ((nor_t *)ctx)->time++; }
static wf_ota_journal_t port(nor_t *n)
{
    return (wf_ota_journal_t){.context=n,.read=read_bytes,.program=program,.erase=erase,.sync=sync_bytes,
        .acquire=acquire,.release=release,.now_ms=now,.erase_bytes=4096,.budget_ms=2000,.writable=true};
}
static wf_ota_ab_image_t image(unsigned number)
{
    wf_ota_ab_image_t i = {.bytes=3000000 + number,.release=number};
    memset(i.sha256, (int)number, 32);
    return i;
}
static wf_ota_ab_state_t seed(nor_t *n)
{
    memset(n, 0, sizeof *n);
    memset(n->unit, 0xFF, sizeof n->unit);
    memset(n->before, 0xA5, sizeof n->before);
    memset(n->after, 0x5A, sizeof n->after);
    n->cut = n->erase_cut = -1;
    wf_ota_ab_state_t state;
    wf_ota_ab_image_t i = image(1);
    assert(wf_ota_ab_provision(&state, 0, &i));
    wf_ota_journal_t j = port(n);
    assert(wf_ota_journal_provision(&j, &state) == 0);
    assert(n->erases == 0 && n->locks == n->unlocks);
    n->calls = n->time = n->programs = 0;
    return state;
}
static void guards(const nor_t *n)
{
    for (unsigned i = 0; i < 16; ++i) assert(n->before[i] == 0xA5 && n->after[i] == 0x5A);
    assert(!n->locked && n->locks == n->unlocks);
}
static void lifecycle(void)
{
    nor_t n; wf_ota_ab_state_t stable = seed(&n), next, loaded;
    wf_ota_journal_t j = port(&n);
    uint8_t readback[WF_OTA_AB_BYTES]; unsigned unit;
    wf_ota_ab_image_t checked[2] = {image(1), image(2)};
    assert(wf_ota_ab_stage(&next, &stable, &checked[1]));
    assert(wf_ota_journal_commit(&j, &stable, &next, readback) == 0);
    assert(wf_ota_journal_commit(&j, &stable, &next, readback) == WF_OTA_JOURNAL_STALE);
    assert(wf_ota_journal_load(&j, &loaded, &unit) == 0 && unit == 1);
    wf_ota_ab_plan_t plan;
    assert(wf_ota_ab_plan(&plan, &loaded, checked));
    assert(wf_ota_journal_commit(&j, &loaded, &plan.next, readback) == 0);
    assert(wf_ota_ab_resolve(&plan, readback, sizeof readback) == WF_OTA_AB_SLOT_B);
    assert(wf_ota_journal_load(&j, &loaded, &unit) == 0 && loaded.phase == WF_OTA_AB_TRIED);
    assert(wf_ota_ab_confirm(&next, &loaded, 1, &checked[1]));
    assert(wf_ota_journal_commit(&j, &loaded, &next, readback) == 0);
    assert(wf_ota_journal_load(&j, &loaded, &unit) == 0 && loaded.stable == 1);
    guards(&n); ++checks;
}
static void errors_and_cuts(void)
{
    nor_t original[3]; wf_ota_ab_state_t states[5], loaded;
    states[0] = seed(&original[0]);
    wf_ota_ab_image_t checked[2] = {image(1), image(2)};
    assert(wf_ota_ab_stage(&states[1], &states[0], &checked[1]));
    uint8_t readback[WF_OTA_AB_BYTES];
    original[1] = original[0]; wf_ota_journal_t j = port(&original[1]);
    assert(wf_ota_journal_commit(&j, &states[0], &states[1], readback) == 0);
    wf_ota_ab_plan_t plan;
    assert(wf_ota_ab_plan(&plan, &states[1], checked)); states[2] = plan.next;
    original[2] = original[1]; j = port(&original[2]);
    assert(wf_ota_journal_commit(&j, &states[1], &states[2], readback) == 0);
    assert(wf_ota_ab_confirm(&states[3], &states[2], 1, &checked[1]));
    assert(wf_ota_ab_plan(&plan, &states[2], checked)); states[4] = plan.next;
    const unsigned from[] = {0,1,2,2}, to[] = {1,2,3,4};
    unsigned total_faults = 0;
    for (unsigned transition = 0; transition < 4; ++transition) {
        nor_t base = original[from[transition]]; base.calls = base.time = 0;
        unsigned source; j = port(&base);
        assert(wf_ota_journal_load(&j, &loaded, &source) == 0);
        base.calls = base.time = 0;
        nor_t normal = base; j = port(&normal);
        assert(wf_ota_journal_commit(&j, &states[from[transition]], &states[to[transition]], readback) == 0);
        unsigned calls = normal.calls; total_faults += calls;
        for (unsigned failure = 1; failure <= calls; ++failure) {
            nor_t n = base; n.fail_call = failure; j = port(&n);
            assert(wf_ota_journal_commit(&j, &states[from[transition]], &states[to[transition]], readback) != 0 && j.fault);
            assert(!memcmp(n.unit[source], base.unit[source], 4096));
            unsigned writes = n.programs + n.erases;
            assert(wf_ota_journal_commit(&j, &states[from[transition]], &states[to[transition]], readback) != 0);
            assert(writes == n.programs + n.erases);
            guards(&n); ++checks;
        }
        for (int cut = 0; cut <= WF_OTA_AB_BYTES; ++cut) {
            nor_t n = base; n.cut = cut; j = port(&n);
            assert(wf_ota_journal_commit(&j, &states[from[transition]], &states[to[transition]], readback) != 0);
            assert(!memcmp(n.unit[source], base.unit[source], 4096));
            n.cut = -1; n.calls = n.time = 0; j = port(&n);
            unsigned unit; assert(wf_ota_journal_load(&j, &loaded, &unit) == 0);
            assert(wf_ota_ab_plan(&plan, &loaded, checked));
            assert(wf_ota_ab_resolve(&plan, NULL, 0) ==
                   (transition == 2 && cut == WF_OTA_AB_BYTES ? WF_OTA_AB_SLOT_B : WF_OTA_AB_SLOT_A));
            guards(&n); ++checks;
        }
        for (int cut = 0; cut <= 4096; ++cut) {
            nor_t n = base; n.erase_cut = cut; j = port(&n);
            assert(wf_ota_journal_commit(&j, &states[from[transition]], &states[to[transition]], readback) != 0);
            assert(!memcmp(n.unit[source], base.unit[source], 4096));
            for (unsigned i = 0; i < sizeof readback; ++i) assert(readback[i] == 0);
            n.erase_cut = -1; n.calls = n.time = 0; j = port(&n);
            unsigned unit; assert(wf_ota_journal_load(&j, &loaded, &unit) == 0);
            assert(loaded.generation == states[from[transition]].generation);
            assert(wf_ota_ab_plan(&plan, &loaded, checked));
            assert(wf_ota_ab_resolve(&plan, NULL, 0) == WF_OTA_AB_SLOT_A);
            guards(&n); ++checks;
        }
    }
    printf("journal callback fault positions: %u; program prefix cuts: %u; erase prefix cuts: %u\n",
           total_faults, 4 * (WF_OTA_AB_BYTES + 1), 4 * 4097);
}
static void refusal(void)
{
    nor_t n; wf_ota_ab_state_t stable = seed(&n), next;
    wf_ota_ab_image_t candidate = image(2); uint8_t readback[WF_OTA_AB_BYTES];
    assert(wf_ota_ab_stage(&next, &stable, &candidate));
    wf_ota_journal_t j = port(&n); j.writable = false;
    assert(wf_ota_journal_commit(&j, &stable, &next, readback) == WF_OTA_JOURNAL_READ_ONLY);
    assert(n.erases == 0 && n.programs == 0);
    j = port(&n); j.budget_ms = 1;
    assert(wf_ota_journal_commit(&j, &stable, &next, readback) != 0 && j.fault);
    assert(n.erases == 0);
    n.time = 0; j = port(&n); n.lie_program = true;
    assert(wf_ota_journal_commit(&j, &stable, &next, readback) != 0 && j.fault);
    n.time = 0; n.lie_program = false; n.lie_erase = true; n.unit[1][4000] = 0;
    j = port(&n);
    assert(wf_ota_journal_commit(&j, &stable, &next, readback) != 0 && j.fault);
    uint8_t stable_wire[WF_OTA_AB_BYTES];
    assert(wf_ota_ab_encode(&stable, stable_wire));
    assert(!memcmp(n.unit[0], stable_wire, sizeof stable_wire));
    unsigned erased = n.erases;
    j = port(&n);
    assert(wf_ota_journal_provision(&j, &stable) != 0 && n.erases == erased);
    guards(&n); ++checks;
}
static void illegal_transitions_and_peer_damage(void)
{
    nor_t n; wf_ota_ab_state_t stable = seed(&n), ready, forged;
    wf_ota_ab_image_t candidate = image(2);
    uint8_t readback[WF_OTA_AB_BYTES];
    assert(wf_ota_ab_stage(&ready, &stable, &candidate));
    forged = ready; forged.image[0].sha256[0] ^= 1;
    wf_ota_journal_t j = port(&n);
    assert(wf_ota_journal_commit(&j, &stable, &forged, readback) == WF_OTA_JOURNAL_ARGUMENT);
    assert(n.erases == 0 && n.programs == 0);
    assert(wf_ota_journal_commit(&j, &stable, &ready, readback) == 0);
    forged = ready; ++forged.generation; forged.phase = WF_OTA_AB_STABLE;
    forged.stable = 1; forged.pending = WF_OTA_AB_NONE;
    unsigned erases = n.erases;
    assert(wf_ota_journal_commit(&j, &ready, &forged, readback) == WF_OTA_JOURNAL_ARGUMENT);
    assert(n.erases == erases);
    stable = seed(&n); assert(wf_ota_ab_stage(&ready, &stable, &candidate));
    j = port(&n); n.damage_peer = true;
    assert(wf_ota_journal_commit(&j, &stable, &ready, readback) != 0 && j.fault);
    for (unsigned i = 0; i < sizeof readback; ++i) assert(readback[i] == 0);
    guards(&n); ++checks;
}
int main(void)
{
    lifecycle(); errors_and_cuts(); refusal(); illegal_transitions_and_peer_damage();
    printf("OTA journal: %u checks passed; in-memory NOR only\n", checks);
    return 0;
}
