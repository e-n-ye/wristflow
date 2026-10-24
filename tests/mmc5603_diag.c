#ifdef NDEBUG
#undef NDEBUG
#endif
#include "mmc5603_diag.h"
#include "support/mmc5603_fake.h"
static bool read_io(void *p, uint8_t reg, uint8_t *data, size_t size)
{ return fake_mag_read(p,reg,data,size); }
static bool write_io(void *p, uint8_t reg, uint8_t value)
{ return fake_mag_write(p,reg,value); }
static uint32_t time_io(void *p) { return ((fake_mmc5603_t *)p)->ms; }
static bool cancelled_io(void *p) { return ((fake_mmc5603_t *)p)->cancel; }
static void delay_io(void *p, uint32_t ms) { ((fake_mmc5603_t *)p)->ms+=ms; }
static void setup(wf_mmc5603_t *d, fake_mmc5603_t *s)
{
    fake_mag_init(s);
    wf_mmc5603_init(d,(wf_mmc5603_io_t){s,read_io,write_io,time_io,cancelled_io,delay_io});
}
static void poll_next(wf_mmc5603_t *d, fake_mmc5603_t *s)
{
    int32_t wait=wf_mmc5603_wait_ms(d,s->ms);
    assert(wait>=0 && wait<=1000);
    s->ms+=(uint32_t)wait; wf_mmc5603_poll(d);
}
static void first_frame(wf_mmc5603_t *d, fake_mmc5603_t *s)
{
    assert(wf_mmc5603_begin(d,s->ms,2000));
    for (unsigned i=0;i<10 && !d->snapshot.samples && d->snapshot.phase==WF_MAG_RUNNING;++i) poll_next(d,s);
}
int main(void)
{
    wf_mmc5603_t d; fake_mmc5603_t s;
    setup(&d,&s);
    wf_mmc5603_poll(&d); assert(!s.operations && wf_mmc5603_wait_ms(&d,0)==-1);
    assert(!wf_mmc5603_begin(&d,0,999) && !wf_mmc5603_begin(&d,0,120001));
    first_frame(&d,&s);
    assert(d.snapshot.phase==WF_MAG_RUNNING && d.snapshot.selftest_valid && d.snapshot.selftest_passed);
    assert(d.snapshot.samples==1 && d.snapshot.sample_valid && d.snapshot.x==12345 && d.snapshot.y==-23456 && d.snapshot.z==34567);
    assert(!memcmp(d.snapshot.raw,s.data,9) && s.threshold[0]==80 && s.threshold[1]==120 && s.threshold[2]==160);
    assert(!wf_mmc5603_begin(&d,s.ms,1000));
    unsigned ops=s.operations;
    wf_mmc5603_poll(&d); assert(s.operations==ops); /* No duplicate cached frame. */
    fake_mag_counts(&s,-524288,524287,0);
    poll_next(&d,&s); poll_next(&d,&s);
    assert(d.snapshot.samples==2 && d.snapshot.saturated && !d.snapshot.sample_valid);
    assert(d.snapshot.x==-524288 && d.snapshot.y==524287 && d.snapshot.z==0);
    s.ms=d.snapshot.started_ms+d.snapshot.duration_ms;
    unsigned triggers=s.triggers; wf_mmc5603_poll(&d);
    assert(d.snapshot.ended==WF_MAG_DEADLINE && d.snapshot.stopped && !d.snapshot.owned && s.triggers==triggers);
    ops=s.operations; wf_mmc5603_poll(&d); assert(s.operations==ops);
    assert(wf_mmc5603_stop(&d,WF_MAG_CANCELLED) && s.operations==ops); /* Idempotent. */
    setup(&d,&s); s.id=0;
    first_frame(&d,&s); assert(d.snapshot.ended==WF_MAG_ID_MISMATCH && !s.writes && !d.snapshot.owned);
    setup(&d,&s); s.selftest_fail=true;
    first_frame(&d,&s); assert(d.snapshot.ended==WF_MAG_SELFTEST_FAILED && d.snapshot.selftest_valid && !d.snapshot.selftest_passed && d.snapshot.stopped);
    setup(&d,&s); s.never_done=true;
    first_frame(&d,&s);
    while(d.snapshot.phase==WF_MAG_RUNNING) poll_next(&d,&s);
    assert(d.snapshot.ended==WF_MAG_NOT_READY && d.snapshot.stopped && !d.snapshot.samples);
    setup(&d,&s); s.otp_missing=true;
    first_frame(&d,&s); assert(d.snapshot.phase==WF_MAG_FAULT && d.snapshot.owned && !d.snapshot.stopped);
    setup(&d,&s); s.stuck_done=true;
    first_frame(&d,&s); assert(d.snapshot.ended==WF_MAG_UNEXPECTED_DATA && !d.snapshot.samples && d.snapshot.stopped);
    /* Never publish the frame when cancellation arrives inside the burst. */
    setup(&d,&s); first_frame(&d,&s); s.cancel_data=true;
    poll_next(&d,&s); poll_next(&d,&s);
    assert(d.snapshot.ended==WF_MAG_CANCELLED && d.snapshot.samples==1 && d.snapshot.stopped);
    setup(&d,&s); first_frame(&d,&s); s.fail_reset=true;
    assert(!wf_mmc5603_stop(&d,WF_MAG_CANCELLED) && d.snapshot.owned && !d.snapshot.stopped);
    ops=s.operations; wf_mmc5603_poll(&d); assert(s.operations==ops);
    assert(!wf_mmc5603_begin(&d,s.ms,1000));
    s.fail_reset=false; assert(wf_mmc5603_stop(&d,WF_MAG_CANCELLED) && !d.snapshot.owned);
    /* Each initialization/first-frame transaction can fail without a fabricated
     * sample or an autonomous retry. Cleanup ACKs are independently checked. */
    for (unsigned i=1;i<=16;++i) {
        setup(&d,&s); s.fail_at=i; first_frame(&d,&s);
        assert(d.snapshot.phase==WF_MAG_FAULT && !d.snapshot.sample_valid && !d.snapshot.samples);
        ops=s.operations; wf_mmc5603_poll(&d); assert(s.operations==ops);
    }
    setup(&d,&s); s.ms=UINT32_MAX-30; first_frame(&d,&s);
    assert(d.snapshot.samples==1 && d.snapshot.sample_valid);
    s.ms=d.snapshot.started_ms+2000; wf_mmc5603_poll(&d);
    assert(d.snapshot.ended==WF_MAG_DEADLINE && d.snapshot.stopped);
    /* A completed single shot is not overwritten while the worker is busy.
     * Its late observation is valid within this session, without asserting a
     * hardware acquisition timestamp or a50ms maximum sample age. */
    setup(&d,&s); first_frame(&d,&s); poll_next(&d,&s);
    s.ms+=100; wf_mmc5603_poll(&d);
    assert(d.snapshot.phase==WF_MAG_RUNNING && d.snapshot.samples==2 && d.snapshot.sampled_ms==s.ms);
    for (unsigned reg=0;reg<=0x18;reg+=0x18) {
        setup(&d,&s); first_frame(&d,&s); poll_next(&d,&s);
        s.slow_reg=(int)reg; s.read_delay_ms=2000;
        poll_next(&d,&s);
        assert(d.snapshot.ended==WF_MAG_DEADLINE && d.snapshot.samples==1 && d.snapshot.stopped);
    }
    setup(&d,&s); first_frame(&d,&s);
    wf_mmc5603_bus_fault(&d); assert(d.snapshot.owned && !d.snapshot.stopped && !d.snapshot.sample_valid);
    assert(wf_mmc5603_stop(&d,WF_MAG_CANCELLED) && !d.snapshot.owned);
    return 0;
}
