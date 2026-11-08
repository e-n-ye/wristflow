#include "ota_journal.h"
#include <limits.h>
#include <string.h>

static int fail(wf_ota_journal_t *j)
{ j->fault = true; return WF_OTA_JOURNAL_IO; }
static bool within(wf_ota_journal_t *j, uint32_t start)
{ return (uint32_t)(j->now_ms(j->context) - start) < j->budget_ms; }
static int begin(wf_ota_journal_t *j, bool write, uint32_t *start)
{
    if (!j || !j->read || !j->acquire || !j->release || !j->now_ms ||
        !j->budget_ms || j->erase_bytes < 4096 || j->erase_bytes > 65536 ||
        (j->erase_bytes & (j->erase_bytes - 1))) return WF_OTA_JOURNAL_ARGUMENT;
    if (!j->acquire(j->context)) return WF_OTA_JOURNAL_BUSY;
    *start = j->now_ms(j->context);
    int result = 0;
    if (write && j->fault) result = WF_OTA_JOURNAL_IO;
    else if (write && (!j->writable || !j->program || !j->erase || !j->sync))
        result = WF_OTA_JOURNAL_READ_ONLY;
    if (result) j->release(j->context);
    return result;
}
static int read_at(wf_ota_journal_t *j, unsigned unit, uint32_t offset,
                    uint8_t *out, size_t size, uint32_t start)
{
    if (!within(j, start) || j->read(j->context, unit, offset, out, size) != (int)size ||
        !within(j, start)) return fail(j);
    return 0;
}
static int pair(wf_ota_journal_t *j, uint8_t bytes[2][WF_OTA_AB_BYTES], uint32_t start)
{
    for (unsigned i = 0; i < 2; ++i)
        if (read_at(j, i, 0, bytes[i], WF_OTA_AB_BYTES, start)) return WF_OTA_JOURNAL_IO;
    return 0;
}
static bool state_equal(const wf_ota_ab_state_t *a, const wf_ota_ab_state_t *b)
{
    uint8_t x[WF_OTA_AB_BYTES], y[WF_OTA_AB_BYTES];
    return wf_ota_ab_encode(a, x) && wf_ota_ab_encode(b, y) && !memcmp(x, y, sizeof x);
}
static bool transition_valid(const wf_ota_ab_state_t *s, const wf_ota_ab_state_t *next)
{
    wf_ota_ab_state_t expected;
    if (s->phase == WF_OTA_AB_STABLE)
        return wf_ota_ab_stage(&expected, s, &next->image[1u - s->stable]) && state_equal(&expected, next);
    wf_ota_ab_plan_t plan;
    if (wf_ota_ab_plan(&plan, s, s->image) && state_equal(&plan.next, next)) return true;
    if (s->phase == WF_OTA_AB_TRIED)
        return wf_ota_ab_confirm(&expected, s, s->pending, &s->image[s->pending]) && state_equal(&expected, next);
    wf_ota_ab_image_t checked[2] = {s->image[0], s->image[1]};
    memset(&checked[s->pending], 0, sizeof checked[0]);
    return wf_ota_ab_plan(&plan, s, checked) && state_equal(&plan.next, next);
}
static int blank_unit(wf_ota_journal_t *j, unsigned unit, uint32_t start)
{
    uint8_t bytes[WF_OTA_AB_BYTES];
    for (uint32_t offset = 0; offset < j->erase_bytes; offset += sizeof bytes) {
        if (read_at(j, unit, offset, bytes, sizeof bytes, start)) return WF_OTA_JOURNAL_IO;
        for (unsigned i = 0; i < sizeof bytes; ++i) if (bytes[i] != 0xFF) return fail(j);
    }
    return 0;
}
static int write_record(wf_ota_journal_t *j, unsigned unit,
                         const uint8_t wire[WF_OTA_AB_BYTES], uint8_t *readback, uint32_t start)
{
    if (!within(j, start) || j->program(j->context, unit, wire, WF_OTA_AB_BYTES) != WF_OTA_AB_BYTES ||
        !within(j, start) || j->sync(j->context) != 0 || !within(j, start)) return fail(j);
    if (read_at(j, unit, 0, readback, WF_OTA_AB_BYTES, start)) return WF_OTA_JOURNAL_IO;
    return memcmp(wire, readback, WF_OTA_AB_BYTES) ? fail(j) : 0;
}
int wf_ota_journal_load(wf_ota_journal_t *j, wf_ota_ab_state_t *out, unsigned *unit)
{
    if (!out || !unit) return WF_OTA_JOURNAL_ARGUMENT;
    memset(out, 0, sizeof *out);
    uint32_t start;
    int result = begin(j, false, &start);
    if (result) return result;
    uint8_t raw[2][WF_OTA_AB_BYTES];
    result = pair(j, raw, start);
    if (!result) {
        int selected = wf_ota_ab_select(out, raw[0], sizeof raw[0], raw[1], sizeof raw[1]);
        if (selected < 0) result = WF_OTA_JOURNAL_NO_STATE;
        else *unit = (unsigned)selected;
    }
    j->release(j->context);
    return result;
}
int wf_ota_journal_provision(wf_ota_journal_t *j, const wf_ota_ab_state_t *initial)
{
    uint8_t wire[WF_OTA_AB_BYTES], readback[WF_OTA_AB_BYTES];
    if (!initial || initial->generation != 1 || initial->phase != WF_OTA_AB_STABLE ||
        !wf_ota_ab_encode(initial, wire)) return WF_OTA_JOURNAL_ARGUMENT;
    uint32_t start;
    int result = begin(j, true, &start);
    if (result) return result;
    for (unsigned i = 0; !result && i < 2; ++i) result = blank_unit(j, i, start);
    /* Already erased: provisioning never erases an unknown existing unit. */
    if (!result) result = write_record(j, 0, wire, readback, start);
    j->release(j->context);
    return result;
}
int wf_ota_journal_commit(wf_ota_journal_t *j, const wf_ota_ab_state_t *current,
                          const wf_ota_ab_state_t *next, uint8_t readback[WF_OTA_AB_BYTES])
{
    uint8_t wire[WF_OTA_AB_BYTES], current_wire[WF_OTA_AB_BYTES];
    if (readback) memset(readback, 0, WF_OTA_AB_BYTES);
    if (!current || !next || !readback || current->generation == UINT32_MAX ||
        next->generation != current->generation + 1 || !wf_ota_ab_encode(current, current_wire) ||
        !wf_ota_ab_encode(next, wire) || !transition_valid(current, next))
        return WF_OTA_JOURNAL_ARGUMENT;
    uint32_t start;
    int result = begin(j, true, &start);
    if (result) return result;
    uint8_t raw[2][WF_OTA_AB_BYTES], old[WF_OTA_AB_BYTES];
    wf_ota_ab_state_t loaded;
    result = pair(j, raw, start);
    int selected = result ? -1 : wf_ota_ab_select(&loaded, raw[0], sizeof raw[0], raw[1], sizeof raw[1]);
    if (!result && (selected < 0 || !state_equal(&loaded, current))) result = WF_OTA_JOURNAL_STALE;
    if (!result) {
        unsigned target = 1u - (unsigned)selected;
        if (!within(j, start) || j->erase(j->context, target) != 0 || !within(j, start)) result = fail(j);
        if (!result) result = blank_unit(j, target, start);
        if (!result) result = write_record(j, target, wire, readback, start);
        if (!result) result = read_at(j, (unsigned)selected, 0, old, sizeof old, start);
        if (!result && memcmp(old, raw[selected], sizeof old)) result = fail(j);
        if (!result) {
            result = pair(j, raw, start);
            int newest = result ? -1 : wf_ota_ab_select(&loaded, raw[0], sizeof raw[0], raw[1], sizeof raw[1]);
            if (!result && (newest != (int)target || !state_equal(&loaded, next))) result = fail(j);
        }
    }
    if (result) memset(readback, 0, WF_OTA_AB_BYTES);
    j->release(j->context);
    return result;
}
