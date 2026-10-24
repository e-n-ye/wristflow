#ifdef NDEBUG
#undef NDEBUG
#endif
#include "ltr303_diag.h"
#include <assert.h>
#include <string.h>

typedef struct {
    uint8_t regs[256];
    uint32_t ms;
    unsigned reads, writes, fail_at, last_data, frames;
    bool cancel, fail_restore, locked, stop_after_rate;
} sensor_t;
static bool read_io(void *ctx, uint8_t reg, uint8_t *value)
{
    sensor_t *s = ctx;
    bool fail = ++s->reads + s->writes == s->fail_at;
    if (reg == 0x88) { s->locked = true; s->last_data = reg; }
    else if (reg >= 0x89 && reg <= 0x8b && s->locked) {
        assert(reg == s->last_data + 1 || reg == 0x8b);
        s->last_data = reg;
    }
    if (fail) return false;
    *value = s->regs[reg];
    if (reg == 0x8b) { s->locked = false; s->regs[0x8c] &= (uint8_t)~4U; ++s->frames; }
    return true;
}
static bool write_io(void *ctx, uint8_t reg, uint8_t value)
{
    sensor_t *s = ctx;
    assert(reg == 0x80 || reg == 0x85);
    bool fail = ++s->writes + s->reads == s->fail_at || (s->fail_restore && reg == 0x80 && value == 0x0c);
    if (fail) return false;
    s->regs[reg] = value;
    if (s->stop_after_rate && reg == 0x85) s->cancel = true;
    return true;
}
static uint32_t time_io(void *ctx) { return ((sensor_t *)ctx)->ms; }
static bool cancel_io(void *ctx) { return ((sensor_t *)ctx)->cancel; }
static void setup(wf_ltr303_t *d, sensor_t *s)
{
    *s = (sensor_t){.regs = {[0x86] = 0xa0, [0x87] = 5, [0x80] = 0x0c, [0x85] = 4}};
    wf_ltr303_init(d, (wf_ltr303_io_t){s, read_io, write_io, time_io, cancel_io});
}
static void activate(wf_ltr303_t *d, sensor_t *s, unsigned duration)
{
    assert(wf_ltr303_begin(d, s->ms, duration));
    assert(!s->reads && !s->writes);
    assert(wf_ltr303_wait_ms(d, s->ms) == 100);
    s->ms += 100; wf_ltr303_poll(d);
    assert(d->snapshot.entered_valid && d->snapshot.owned && s->regs[0x80] == 1 && s->regs[0x85] == 3);
    assert(!s->locked);
}
static void frame(sensor_t *s, uint16_t ch0, uint16_t ch1, uint8_t status)
{
    s->regs[0x88] = (uint8_t)ch1; s->regs[0x89] = (uint8_t)(ch1 >> 8);
    s->regs[0x8a] = (uint8_t)ch0; s->regs[0x8b] = (uint8_t)(ch0 >> 8);
    s->regs[0x8c] = status;
}
int main(void)
{
    wf_ltr303_t d; sensor_t s;
    setup(&d, &s);
    wf_ltr303_poll(&d); assert(!s.reads && !s.writes && wf_ltr303_wait_ms(&d, 0) == -1);
    assert(!wf_ltr303_begin(&d, 0, 999) && !wf_ltr303_begin(&d, 0, 120001));
    activate(&d, &s, 3000);
    unsigned count = s.reads + s.writes;
    assert(!wf_ltr303_begin(&d, s.ms, 120000));
    wf_ltr303_poll(&d); assert(s.reads + s.writes == count);
    s.ms += 200; frame(&s, 0x1234, 0x345, 4); wf_ltr303_poll(&d);
    assert(d.snapshot.samples == 1 && d.snapshot.ch0 == 0x1234 && d.snapshot.ch1 == 0x345 && d.snapshot.sample_valid);
    s.ms += 1000; wf_ltr303_poll(&d); assert(d.snapshot.samples == 1 && d.snapshot.not_ready == 1);
    s.ms += 1000; frame(&s, 65535, 2, 4); wf_ltr303_poll(&d);
    assert(d.snapshot.samples == 2 && d.snapshot.saturated && !d.snapshot.sample_valid);
    s.ms = 3000; wf_ltr303_poll(&d);
    assert(d.snapshot.phase == WF_LIGHT_DONE && d.snapshot.ended == WF_LIGHT_DEADLINE && !d.snapshot.owned && d.snapshot.restored);
    assert(s.regs[0x80] == 0x0c && s.regs[0x85] == 4);
    count = s.reads + s.writes; wf_ltr303_poll(&d); assert(s.reads + s.writes == count);
    assert(wf_ltr303_wait_ms(&d, s.ms) == -1);
    for (unsigned status = 0; status < 3; ++status) {
        setup(&d, &s); activate(&d, &s, 1000); s.ms += 200;
        frame(&s, 100, 10, status == 0 ? 0x84 : status == 1 ? 0x14 : 4); wf_ltr303_poll(&d);
        assert(d.snapshot.sample_valid == (status == 2));
        assert(wf_ltr303_stop(&d, WF_LIGHT_CANCELLED) && !d.snapshot.owned);
    }
    /* Partial DATA transfers must still reach/retry 0x8b to release its lock. */
    for (unsigned fail = 1; fail <= 4; ++fail) {
        setup(&d, &s); activate(&d, &s, 1000); s.ms += 200; frame(&s, 10, 2, 4);
        s.fail_at = s.reads + s.writes + 3 + fail; wf_ltr303_poll(&d);
        assert(d.snapshot.phase == WF_LIGHT_FAULT && !d.snapshot.samples && !s.locked && !d.snapshot.owned);
        assert(d.snapshot.restored && d.snapshot.io_errors == 1);
    }
    /* Every configuration transaction can fail without abandoning writes. */
    for (unsigned fail = 1; fail <= 12; ++fail) {
        setup(&d, &s); assert(wf_ltr303_begin(&d, 0, 1000)); s.ms = 100; s.fail_at = fail;
        wf_ltr303_poll(&d);
        assert(d.snapshot.phase == WF_LIGHT_FAULT && !d.snapshot.owned && !s.locked);
        assert(s.regs[0x80] == 0x0c && s.regs[0x85] == 4);
    }
    setup(&d, &s); activate(&d, &s, 1000); s.fail_restore = true;
    assert(!wf_ltr303_stop(&d, WF_LIGHT_CANCELLED) && d.snapshot.owned && !d.snapshot.restored);
    count = s.reads + s.writes; s.ms += 10000; wf_ltr303_poll(&d);
    assert(s.reads + s.writes == count && wf_ltr303_wait_ms(&d, s.ms) == -1);
    assert(!wf_ltr303_begin(&d, s.ms, 1000));
    s.fail_restore = false; assert(wf_ltr303_stop(&d, WF_LIGHT_CANCELLED) && !d.snapshot.owned);
    setup(&d, &s); s.stop_after_rate = true;
    assert(wf_ltr303_begin(&d, 0, 1000)); s.ms = 100; wf_ltr303_poll(&d);
    assert(d.snapshot.ended == WF_LIGHT_CANCELLED && d.snapshot.restored && s.regs[0x80] == 0x0c);
    setup(&d, &s); s.regs[0x86] = 0x90;
    assert(wf_ltr303_begin(&d, 0, 1000)); s.ms = 100; wf_ltr303_poll(&d);
    assert(d.snapshot.ended == WF_LIGHT_ID_MISMATCH && !s.writes && !d.snapshot.owned);
    setup(&d, &s); s.regs[0x80] = 1;
    assert(wf_ltr303_begin(&d, 0, 1000)); s.ms = 100; wf_ltr303_poll(&d);
    assert(d.snapshot.ended == WF_LIGHT_IN_USE && !s.writes);
    setup(&d, &s); s.ms = UINT32_MAX - 50; activate(&d, &s, 1000);
    s.ms += 900; wf_ltr303_poll(&d);
    assert(d.snapshot.ended == WF_LIGHT_DEADLINE && d.snapshot.restored);
    setup(&d, &s); activate(&d, &s, 1000); s.regs[0x85] = 4; s.ms += 200; wf_ltr303_poll(&d);
    assert(d.snapshot.ended == WF_LIGHT_CONFIG_CHANGED && d.snapshot.restored && !d.snapshot.owned);
    setup(&d, &s); activate(&d, &s, 1000); wf_ltr303_bus_fault(&d);
    assert(d.snapshot.phase == WF_LIGHT_FAULT && d.snapshot.owned);
    assert(wf_ltr303_stop(&d, WF_LIGHT_CANCELLED) && d.snapshot.restored);
    /* DS p15 defines repeat codes 5/6/7 as 2000ms, all valid to preserve. */
    for (unsigned rate = 5; rate <= 7; ++rate) {
        setup(&d, &s); s.regs[0x85] = (uint8_t)rate; activate(&d, &s, 1000);
        assert(wf_ltr303_stop(&d, WF_LIGHT_CANCELLED) && s.regs[0x85] == rate && d.snapshot.restored);
    }
    /* Continuous production mode shares the same configuration/restoration.
     * It crosses 120s and tick wrap without rolling diagnostic sessions. */
    setup(&d,&s); s.ms=UINT32_MAX-50;
    assert(wf_ltr303_begin_continuous(&d,s.ms) && d.continuous && !d.snapshot.duration_ms);
    s.ms+=100; wf_ltr303_poll(&d); s.ms+=200; frame(&s,100,20,4); wf_ltr303_poll(&d);
    assert(d.snapshot.samples==1 && d.snapshot.sample_valid);
    s.ms+=121000; frame(&s,200,20,4); wf_ltr303_poll(&d);
    assert(d.snapshot.phase==WF_LIGHT_RUNNING && d.snapshot.samples==2 && wf_ltr303_wait_ms(&d,s.ms)==1000);
    s.cancel=true; wf_ltr303_poll(&d);
    assert(d.snapshot.restored && !d.snapshot.owned && d.snapshot.ended==WF_LIGHT_CANCELLED);
    s.cancel=false; assert(wf_ltr303_begin(&d,s.ms,1000) && !d.continuous);
    return 0;
}
