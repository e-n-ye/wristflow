#include "steps_store.h"
#include <string.h>
#include <limits.h>
static void put32(uint8_t *p, uint32_t v)
{ for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }
static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static uint32_t checksum(const uint8_t *p)
{
    uint32_t crc = UINT32_MAX;
    for (unsigned i = 0; i < 28; ++i) {
        crc ^= p[i];
        for (unsigned b = 0; b < 8; ++b) crc = (crc >> 1) ^ ((0U - (crc & 1U)) & 0xedb88320U);
    }
    return ~crc;
}
bool wristflow_steps_encode(const wristflow_steps_checkpoint_t *c, uint32_t generation, uint8_t *p)
{
    if (!p || !wristflow_steps_checkpoint_valid(c)) return false;
    memset(p, 0, WRISTFLOW_STEPS_RECORD_BYTES); memcpy(p, "WFST", 4); p[4] = 1;
    put32(p + 8, generation); put32(p + 12, c->local_day); put32(p + 16, c->total);
    put32(p + 20, c->coverage_start_utc); put32(p + 24, c->saved_utc);
    put32(p + 28, checksum(p)); return true;
}
bool wristflow_steps_decode(wristflow_steps_checkpoint_t *c, uint32_t *g, const uint8_t *p, size_t size)
{
    if (!c || !g || !p || size != WRISTFLOW_STEPS_RECORD_BYTES || memcmp(p, "WFST", 4) ||
        p[4] != 1 || p[5] || p[6] || p[7] || get32(p + 28) != checksum(p)) return false;
    wristflow_steps_checkpoint_t value = {get32(p + 12), get32(p + 16), get32(p + 20), get32(p + 24)};
    if (!wristflow_steps_checkpoint_valid(&value)) return false;
    *c = value; *g = get32(p + 8); return true;
}
static bool equal(const wristflow_steps_checkpoint_t *a, const wristflow_steps_checkpoint_t *b)
{ return a->local_day == b->local_day && a->total == b->total && a->coverage_start_utc == b->coverage_start_utc; }
bool wristflow_steps_store_restore(wristflow_steps_store_t *s, wristflow_steps_read_t read, void *ctx)
{
    *s = (wristflow_steps_store_t){.active = -1};
    uint8_t record[WRISTFLOW_STEPS_RECORD_BYTES];
    for (unsigned i = 0; i < 2; ++i) {
        wristflow_steps_checkpoint_t c; uint32_t g;
        if (!read || !read(i, record, ctx) || !wristflow_steps_decode(&c, &g, record, sizeof record)) continue;
        if (s->saved_valid && (g == s->generation || g - s->generation == 0x80000000U)) {
            /* Conflicting equal/ambiguous generations are not a trusted seed. */
            if (g != s->generation || !equal(&c, &s->saved) || c.saved_utc != s->saved.saved_utc) {
                *s = (wristflow_steps_store_t){.active = -1}; return false;
            }
        } else if (s->saved_valid && (int32_t)(g - s->generation) <= 0) continue;
        s->saved = c; s->saved_valid = true; s->active = (int)i; s->generation = g;
    }
    return s->saved_valid;
}
void wristflow_steps_store_offer(wristflow_steps_store_t *s, const wristflow_steps_snapshot_t *v, uint64_t now)
{
    wristflow_steps_checkpoint_t c = {v->observation.local_day, v->metric.total,
        v->observation.coverage_start_utc, v->observation.utc};
    if (!v->metric.total_valid || !v->observation.native_source || !wristflow_steps_checkpoint_valid(&c)) return;
    if (!s->latest_valid) s->next_save_ms = now + WRISTFLOW_STEPS_SAVE_MS;
    s->latest = c; s->latest_valid = true;
    if (s->saved_valid && equal(&c, &s->saved)) { s->dirty = false; return; }
    if (!s->dirty) {
        s->due_ms = s->next_save_ms > now ? s->next_save_ms : now;
        s->attempts = 0;
    }
    s->dirty = true; /* New samples never postpone a pending deadline or renew a failed budget. */
}
int32_t wristflow_steps_store_wait(const wristflow_steps_store_t *s, uint64_t now)
{
    if (!s->dirty || s->failed) return -1;
    uint64_t left = s->due_ms > now ? s->due_ms - now : 0;
    return left > INT32_MAX ? INT32_MAX : (int32_t)left;
}
void wristflow_steps_store_flush(wristflow_steps_store_t *s, uint64_t now)
{
    if (!s->latest_valid) return;
    if (s->saved_valid && equal(&s->latest, &s->saved)) return;
    s->dirty = true; s->failed = false; s->attempts = 0; s->due_ms = now;
}
bool wristflow_steps_store_process(wristflow_steps_store_t *s, uint64_t now,
    wristflow_steps_read_t read, wristflow_steps_write_t write, void *ctx)
{
    if (wristflow_steps_store_wait(s, now) != 0) return false;
    uint8_t record[WRISTFLOW_STEPS_RECORD_BYTES], check[WRISTFLOW_STEPS_RECORD_BYTES];
    unsigned target = s->active == 0 ? 1U : 0U;
    uint32_t generation = s->generation + 1;
    ++s->attempts;
    bool ok = read && write && wristflow_steps_encode(&s->latest, generation, record) &&
        write(target, record, ctx) && read(target, check, ctx) && !memcmp(record, check, sizeof record);
    s->saved_valid = ok; /* A partial failed write cannot justify a saved shadow. */
    if (ok) {
        s->saved = s->latest; s->generation = generation; s->active = (int)target;
        s->next_save_ms = now + WRISTFLOW_STEPS_SAVE_MS;
        s->dirty = s->failed = false;
    } else {
        s->failed = s->attempts >= 3;
        s->due_ms = now + 5000U;
    }
    return true;
}
