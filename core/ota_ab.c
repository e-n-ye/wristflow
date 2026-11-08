#include "ota_ab.h"
#include <limits.h>
#include <string.h>

static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void put32(uint8_t *p, uint32_t v)
{ for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }
static uint32_t crc32(const uint8_t *p, size_t n)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < n; ++i) {
        crc ^= p[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? UINT32_C(0xedb88320) : 0);
    }
    return ~crc;
}
static bool image_valid(const wf_ota_ab_image_t *image)
{
    if (!image || !image->bytes) return false;
    uint8_t any = 0;
    for (unsigned i = 0; i < sizeof image->sha256; ++i) any |= image->sha256[i];
    return any != 0;
}
static bool image_equal(const wf_ota_ab_image_t *a, const wf_ota_ab_image_t *b)
{
    return image_valid(a) && image_valid(b) && a->bytes == b->bytes &&
           a->release == b->release && memcmp(a->sha256, b->sha256, sizeof a->sha256) == 0;
}
static bool image_empty(const wf_ota_ab_image_t *image)
{
    if (image->bytes || image->release) return false;
    for (unsigned i = 0; i < sizeof image->sha256; ++i)
        if (image->sha256[i]) return false;
    return true;
}
static bool valid(const wf_ota_ab_state_t *s)
{
    if (!s || !s->generation || s->stable > 1 || s->phase > WF_OTA_AB_TRIED ||
        !image_valid(&s->image[s->stable])) return false;
    for (unsigned i = 0; i < 2; ++i)
        if (!image_valid(&s->image[i]) && !image_empty(&s->image[i])) return false;
    return s->phase == WF_OTA_AB_STABLE ? s->pending == WF_OTA_AB_NONE :
           s->pending < 2 && s->pending != s->stable && image_valid(&s->image[s->pending]);
}
static bool advance(wf_ota_ab_state_t *next, const wf_ota_ab_state_t *s)
{
    if (!next || !valid(s) || s->generation == UINT32_MAX) return false;
    *next = *s;
    ++next->generation;
    return true;
}
bool wf_ota_ab_provision(wf_ota_ab_state_t *out, unsigned stable, const wf_ota_ab_image_t *checked)
{
    if (!out || stable > 1 || !image_valid(checked)) return false;
    wf_ota_ab_state_t s = {0};
    s.generation = 1;
    s.stable = (uint8_t)stable;
    s.pending = WF_OTA_AB_NONE;
    s.image[stable] = *checked;
    *out = s;
    return true;
}
bool wf_ota_ab_stage(wf_ota_ab_state_t *next, const wf_ota_ab_state_t *s,
                     const wf_ota_ab_image_t *checked_candidate)
{
    if (!valid(s) || s->phase != WF_OTA_AB_STABLE || !image_valid(checked_candidate)) return false;
    wf_ota_ab_image_t image = *checked_candidate;
    if (!advance(next, s)) return false;
    next->pending = (uint8_t)(1 - next->stable);
    next->image[next->pending] = image;
    next->phase = WF_OTA_AB_READY;
    return true;
}
bool wf_ota_ab_confirm(wf_ota_ab_state_t *next, const wf_ota_ab_state_t *s,
                       unsigned running, const wf_ota_ab_image_t *checked_running)
{
    if (!valid(s) || s->phase != WF_OTA_AB_TRIED || running != s->pending ||
        !image_equal(checked_running, &s->image[s->pending])) return false;
    if (!advance(next, s)) return false;
    next->stable = next->pending;
    next->pending = WF_OTA_AB_NONE;
    next->phase = WF_OTA_AB_STABLE;
    return true;
}
bool wf_ota_ab_plan(wf_ota_ab_plan_t *out, const wf_ota_ab_state_t *s,
                    const wf_ota_ab_image_t checked[2])
{
    if (!out) return false;
    memset(out, 0, sizeof *out);
    out->target = out->fallback = WF_OTA_AB_RECOVERY;
    if (!valid(s) || !checked || !image_equal(&s->image[s->stable], &checked[s->stable])) return false;
    out->target = out->fallback = (wf_ota_ab_target_t)s->stable;
    if (s->phase == WF_OTA_AB_STABLE) return true;
    if (!advance(&out->next, s)) return false;
    out->persist = true;
    if (s->phase == WF_OTA_AB_READY && image_equal(&s->image[s->pending], &checked[s->pending])) {
        out->next.phase = WF_OTA_AB_TRIED;
        out->target = (wf_ota_ab_target_t)s->pending;
    } else {
        out->next.phase = WF_OTA_AB_STABLE;
        out->next.pending = WF_OTA_AB_NONE;
    }
    return true;
}
wf_ota_ab_target_t wf_ota_ab_resolve(const wf_ota_ab_plan_t *plan,
                                    const uint8_t *readback, size_t n)
{
    if (!plan) return WF_OTA_AB_RECOVERY;
    if (!plan->persist) return plan->target;
    uint8_t expected[WF_OTA_AB_BYTES];
    if (!readback || n != sizeof expected || !wf_ota_ab_encode(&plan->next, expected) ||
        memcmp(expected, readback, sizeof expected) != 0) return plan->fallback;
    return plan->target;
}
bool wf_ota_ab_encode(const wf_ota_ab_state_t *s, uint8_t out[WF_OTA_AB_BYTES])
{
    if (!out || !valid(s)) return false;
    memset(out, 0, WF_OTA_AB_BYTES);
    memcpy(out, "WFAB", 4);
    out[4] = 1;
    out[6] = WF_OTA_AB_BYTES;
    put32(out + 8, s->generation);
    out[12] = s->stable;
    out[13] = s->phase;
    out[14] = s->pending;
    for (unsigned i = 0; i < 2; ++i) {
        uint8_t *p = out + 16 + i * 40;
        put32(p, s->image[i].bytes);
        put32(p + 4, s->image[i].release);
        memcpy(p + 8, s->image[i].sha256, 32);
    }
    put32(out + 124, crc32(out, 124));
    return true;
}
bool wf_ota_ab_decode(wf_ota_ab_state_t *out, const uint8_t *in, size_t n)
{
    if (!out || !in || n != WF_OTA_AB_BYTES || memcmp(in, "WFAB", 4) ||
        in[4] != 1 || in[5] || in[6] != WF_OTA_AB_BYTES || in[7] || in[15] ||
        get32(in + 124) != crc32(in, 124)) return false;
    for (unsigned i = 96; i < 124; ++i) if (in[i]) return false;
    wf_ota_ab_state_t s = {0};
    s.generation = get32(in + 8);
    s.stable = in[12];
    s.phase = in[13];
    s.pending = in[14];
    for (unsigned i = 0; i < 2; ++i) {
        const uint8_t *p = in + 16 + i * 40;
        s.image[i].bytes = get32(p);
        s.image[i].release = get32(p + 4);
        memcpy(s.image[i].sha256, p + 8, 32);
    }
    if (!valid(&s)) return false;
    *out = s;
    return true;
}
int wf_ota_ab_select(wf_ota_ab_state_t *out, const uint8_t *a, size_t an,
                     const uint8_t *b, size_t bn)
{
    if (!out) return -1;
    wf_ota_ab_state_t states[2];
    bool ok[2] = {wf_ota_ab_decode(&states[0], a, an), wf_ota_ab_decode(&states[1], b, bn)};
    if (!ok[0] && !ok[1]) return -1;
    unsigned selected = ok[0] ? 0 : 1;
    if (ok[0] && ok[1]) {
        if (states[0].generation == states[1].generation && memcmp(a, b, WF_OTA_AB_BYTES)) return -1;
        if (states[1].generation > states[0].generation) selected = 1;
    }
    *out = states[selected];
    /* Never retry a candidate solely because its newer TRIED record became
     * unreadable. With only one valid READY, consume the ambiguous trial and
     * plan a rollback to the independently verified stable image. */
    if (!(ok[0] && ok[1]) && out->phase == WF_OTA_AB_READY)
        out->phase = WF_OTA_AB_TRIED;
    return (int)selected;
}
