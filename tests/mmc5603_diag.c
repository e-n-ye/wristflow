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
static void finish_selftest(wf_mmc5603_t *d, fake_mmc5603_t *s)
{
    for (unsigned i=0;i<20 && d->snapshot.phase==WF_MAG_RUNNING;++i) poll_next(d,s);
    assert(d->snapshot.phase!=WF_MAG_RUNNING);
}
int main(void)
{
    wf_mmc5603_t d; fake_mmc5603_t s;
    setup(&d,&s);
    wf_mmc5603_poll(&d); assert(!s.operations && wf_mmc5603_wait_ms(&d,0)==-1);
    assert(!wf_mmc5603_begin(&d,0,999) && !wf_mmc5603_begin(&d,0,120001));
    first_frame(&d,&s);
    assert(d.snapshot.phase==WF_MAG_RUNNING && d.snapshot.selftest_valid && d.snapshot.selftest_passed);
    assert(!d.snapshot.selftest_only && d.snapshot.threshold_mode==WF_MAG_THRESHOLD_RAW80);
    assert(d.snapshot.samples==1 && d.snapshot.sample_valid && d.snapshot.x==12345 && d.snapshot.y==-23456 && d.snapshot.z==34567);
    assert(!memcmp(d.snapshot.raw,s.data,9) && s.threshold[0]==80 && s.threshold[1]==120 && s.threshold[2]==160);
    assert(d.snapshot.selftest_trace==0x7f && !memcmp(d.snapshot.selftest_raw,s.data,9));
    assert(d.snapshot.reset_status==0x10 && d.snapshot.reset_observed_ms==25);
    assert(d.snapshot.selftest_command_ms==25 && d.snapshot.selftest_ack_ms==25);
    assert(d.snapshot.selftest_status==0x50 && d.snapshot.selftest_status_ms==35 && d.snapshot.selftest_polls==1);
    assert(d.snapshot.selftest_data_ms==35 && d.snapshot.selftest_post_ms==35 && d.snapshot.selftest_post_status==0x10);
    assert(s.operations==16); /* Saving trace adds no bus transaction or wait. */
    assert(!wf_mmc5603_begin(&d,s.ms,1000));
    unsigned ops=s.operations;
    wf_mmc5603_poll(&d); assert(s.operations==ops); /* No duplicate cached frame. */
    fake_mag_counts(&s,-524288,524287,0);
    poll_next(&d,&s); poll_next(&d,&s);
    assert(d.snapshot.samples==2 && d.snapshot.saturated && !d.snapshot.sample_valid);
    assert(d.snapshot.x==-524288 && d.snapshot.y==524287 && d.snapshot.z==0);
    assert(d.snapshot.selftest_status_ms==35 && d.snapshot.selftest_polls==1);
    assert(memcmp(d.snapshot.selftest_raw,s.data,9)); /* Separate from ordinary sample. */
    s.ms=d.snapshot.started_ms+d.snapshot.duration_ms;
    unsigned triggers=s.triggers; wf_mmc5603_poll(&d);
    assert(d.snapshot.ended==WF_MAG_DEADLINE && d.snapshot.stopped && !d.snapshot.owned && s.triggers==triggers);
    ops=s.operations; wf_mmc5603_poll(&d); assert(s.operations==ops);
    assert(wf_mmc5603_stop(&d,WF_MAG_CANCELLED) && s.operations==ops); /* Idempotent. */
    setup(&d,&s); s.id=0;
    first_frame(&d,&s); assert(d.snapshot.ended==WF_MAG_ID_MISMATCH && !s.writes && !d.snapshot.owned);
    setup(&d,&s); s.selftest_fail=true;
    first_frame(&d,&s); assert(d.snapshot.ended==WF_MAG_SELFTEST_FAILED && d.snapshot.selftest_valid && !d.snapshot.selftest_passed && d.snapshot.stopped);
    assert(d.snapshot.selftest_trace==0x7f && d.snapshot.selftest_status==0x70 && d.snapshot.selftest_post_status==0x30);
    assert(d.snapshot.after_status==0x10 && !memcmp(d.snapshot.selftest_raw,s.data,9));
    /* The before/after response observations and cleanup status have separate
     * identities, even if a native result bit changes on the data read. */
    setup(&d,&s); s.selftest_fail=true; s.clear_selftest_sat_on_read=true;
    first_frame(&d,&s);
    assert(d.snapshot.ended==WF_MAG_SELFTEST_FAILED && !d.snapshot.selftest_passed);
    assert(d.snapshot.selftest_status==0x70 && d.snapshot.selftest_post_status==0x10 && d.snapshot.after_status==0x10);
    assert(d.snapshot.selftest_data_ms==35 && d.snapshot.selftest_post_ms==35 && !d.snapshot.samples);
    setup(&d,&s); s.never_done=true;
    first_frame(&d,&s);
    while(d.snapshot.phase==WF_MAG_RUNNING) poll_next(&d,&s);
    assert(d.snapshot.ended==WF_MAG_NOT_READY && d.snapshot.stopped && !d.snapshot.samples);
    assert(d.snapshot.selftest_trace==0x1f && d.snapshot.selftest_status==0x10 && d.snapshot.selftest_polls>1);
    assert(!(d.snapshot.selftest_trace&(WF_MAG_TRACE_DATA|WF_MAG_TRACE_POST)) && !d.snapshot.selftest_valid);
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
        if (i==8) assert((d.snapshot.selftest_trace&(WF_MAG_TRACE_COMMAND|WF_MAG_TRACE_ACK))==WF_MAG_TRACE_COMMAND);
        if (i==9) assert(!(d.snapshot.selftest_trace&WF_MAG_TRACE_STATUS));
        if (i==10) assert(!(d.snapshot.selftest_trace&WF_MAG_TRACE_DATA));
        if (i==11) assert((d.snapshot.selftest_trace&(WF_MAG_TRACE_DATA|WF_MAG_TRACE_POST))==WF_MAG_TRACE_DATA);
        ops=s.operations; wf_mmc5603_poll(&d); assert(s.operations==ops);
    }
    setup(&d,&s); s.ms=UINT32_MAX-30; first_frame(&d,&s);
    assert(d.snapshot.samples==1 && d.snapshot.sample_valid);
    s.ms=d.snapshot.started_ms+2000; wf_mmc5603_poll(&d);
    assert(d.snapshot.ended==WF_MAG_DEADLINE && d.snapshot.stopped);
    assert(d.snapshot.selftest_trace==0x7f && d.snapshot.selftest_data_ms<100); /* Wraparound trace retained. */
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
    /* Successful transport evidence survives cancellation/expiry, while the
     * result remains invalid and no magnetic sample is published. */
    setup(&d,&s); s.cancel_data=true; first_frame(&d,&s);
    assert(d.snapshot.ended==WF_MAG_CANCELLED && !d.snapshot.selftest_valid && !d.snapshot.samples);
    assert(d.snapshot.selftest_trace==0x3f && !memcmp(d.snapshot.selftest_raw,s.data,9));
    setup(&d,&s);
    assert(wf_mmc5603_begin(&d,0,2000)); poll_next(&d,&s); poll_next(&d,&s);
    s.slow_reg=0; s.read_delay_ms=2000; poll_next(&d,&s);
    assert(d.snapshot.ended==WF_MAG_DEADLINE && !d.snapshot.selftest_valid && !d.snapshot.samples);
    assert(d.snapshot.selftest_trace==0x3f && d.snapshot.selftest_data_ms>=2000);
    assert(!(d.snapshot.selftest_trace&WF_MAG_TRACE_POST));
    setup(&d,&s); s.partial_data_failure=true; first_frame(&d,&s);
    assert(d.snapshot.ended==WF_MAG_IO_FAILURE && d.snapshot.stopped && !d.snapshot.samples);
    assert(d.snapshot.selftest_trace==0x1f && !d.snapshot.selftest_valid);
    uint8_t empty[9]={0}; assert(!memcmp(d.snapshot.selftest_raw,empty,9));
    /* Beginning a later session must not masquerade old trace bytes as new. */
    s.slow_reg=-1;
    assert(wf_mmc5603_begin(&d,s.ms,1000));
    assert(!d.snapshot.selftest_trace && !d.snapshot.selftest_valid && !d.snapshot.selftest_polls);
    /* The explicit candidate is bounded to one native selftest; the old start
     * must still select raw80 and ordinary sampling even after a candidate. */
    setup(&d,&s);
    assert(!wf_mmc5603_begin_selftest(&d,0,(wf_mag_threshold_t)-1));
    assert(!wf_mmc5603_begin_selftest(&d,0,(wf_mag_threshold_t)2) && !s.operations);
    for (unsigned mode=0;mode<2;++mode) {
        setup(&d,&s); s.factory[0]=0x63; s.factory[1]=0x7a; s.factory[2]=0x72;
        assert(wf_mmc5603_begin_selftest(&d,s.ms,(wf_mag_threshold_t)mode));
        assert(d.snapshot.selftest_only && d.snapshot.duration_ms==10000 && d.snapshot.threshold_mode==(wf_mag_threshold_t)mode);
        wf_mmc5603_snapshot_t pending=d.snapshot;
        assert(!wf_mmc5603_begin_selftest(&d,s.ms,WF_MAG_THRESHOLD_RAW80));
        assert(!memcmp(&d.snapshot,&pending,sizeof pending));
        poll_next(&d,&s); poll_next(&d,&s);
        const uint8_t expected[2][3]={{0x4f,0x61,0x5b},{0x5c,0x13,0x2c}};
        assert(!memcmp(s.threshold,expected[mode],3)); /* Actual written bytes. */
        finish_selftest(&d,&s);
        assert(d.snapshot.phase==WF_MAG_DONE && d.snapshot.ended==WF_MAG_SELFTEST_COMPLETE);
        assert(d.snapshot.selftest_valid && d.snapshot.selftest_passed && d.snapshot.selftest_trace==0x7f);
        assert(d.snapshot.stopped && !d.snapshot.owned && !d.snapshot.samples && !d.snapshot.sample_valid);
        assert(s.triggers==1 && s.resets==2 && s.operations==15);
        ops=s.operations; s.ms+=20000; wf_mmc5603_poll(&d); assert(s.operations==ops);
        first_frame(&d,&s);
        assert(d.snapshot.samples==1 && !d.snapshot.selftest_only && d.snapshot.threshold_mode==WF_MAG_THRESHOLD_RAW80);
        assert(!memcmp(d.snapshot.selftest_threshold,expected[0],3));
    }
    /* Differential full-byte check against a separate rational-distance model.
     * This checks arithmetic/clamping only, never the hardware interpretation. */
    for (unsigned value=0;value<256;++value) {
        setup(&d,&s); memset(s.factory,(int)value,3);
        assert(wf_mmc5603_begin_selftest(&d,0,WF_MAG_THRESHOLD_ENCODED_EXPERIMENTAL));
        poll_next(&d,&s); poll_next(&d,&s);
        unsigned distance=value>128 ? value-128 : 128-value;
        unsigned reference=distance*16/5;
        if (reference>255) reference=255;
        assert(s.threshold[0]==reference && s.threshold[1]==reference && s.threshold[2]==reference);
        finish_selftest(&d,&s);
        assert(!d.snapshot.samples && s.triggers==1);
    }
    for (unsigned mode=0;mode<2;++mode) {
        setup(&d,&s); s.selftest_fail=true;
        assert(wf_mmc5603_begin_selftest(&d,0,(wf_mag_threshold_t)mode)); finish_selftest(&d,&s);
        assert(d.snapshot.phase==WF_MAG_FAULT && d.snapshot.ended==WF_MAG_SELFTEST_FAILED);
        assert(d.snapshot.selftest_valid && !d.snapshot.selftest_passed && d.snapshot.stopped && !d.snapshot.owned);
        assert(!d.snapshot.samples && s.triggers==1);
        /* Every selftest/cleanup transaction is independently faultable. */
        for (unsigned operation=1;operation<=15;++operation) {
            setup(&d,&s); s.fail_at=operation;
            assert(wf_mmc5603_begin_selftest(&d,0,(wf_mag_threshold_t)mode)); finish_selftest(&d,&s);
            assert(d.snapshot.phase==WF_MAG_FAULT && d.snapshot.ended==WF_MAG_IO_FAILURE);
            assert(!d.snapshot.samples && !d.snapshot.sample_valid);
            ops=s.operations; wf_mmc5603_poll(&d); assert(s.operations==ops);
            if (operation>=12) assert(d.snapshot.selftest_valid && d.snapshot.selftest_passed && d.snapshot.owned && !d.snapshot.stopped);
        }
    }
    setup(&d,&s);
    assert(wf_mmc5603_begin_selftest(&d,0,WF_MAG_THRESHOLD_ENCODED_EXPERIMENTAL));
    s.cancel=true; wf_mmc5603_poll(&d);
    assert(d.snapshot.ended==WF_MAG_CANCELLED && !s.operations && !s.triggers);
    setup(&d,&s);
    assert(wf_mmc5603_begin_selftest(&d,0,WF_MAG_THRESHOLD_ENCODED_EXPERIMENTAL));
    poll_next(&d,&s); poll_next(&d,&s); s.cancel=true; wf_mmc5603_poll(&d);
    assert(d.snapshot.ended==WF_MAG_CANCELLED && d.snapshot.stopped && !d.snapshot.selftest_valid && s.triggers==1);
    setup(&d,&s); s.cancel_data=true;
    assert(wf_mmc5603_begin_selftest(&d,0,WF_MAG_THRESHOLD_ENCODED_EXPERIMENTAL)); finish_selftest(&d,&s);
    assert(d.snapshot.ended==WF_MAG_CANCELLED && !d.snapshot.selftest_valid && d.snapshot.selftest_trace==0x3f);
    setup(&d,&s);
    assert(wf_mmc5603_begin_selftest(&d,0,WF_MAG_THRESHOLD_ENCODED_EXPERIMENTAL));
    poll_next(&d,&s); poll_next(&d,&s); s.slow_reg=0; s.read_delay_ms=10000; poll_next(&d,&s);
    assert(d.snapshot.ended==WF_MAG_DEADLINE && d.snapshot.stopped && !d.snapshot.selftest_valid && !d.snapshot.samples);
    assert(d.snapshot.selftest_trace==0x3f && d.snapshot.selftest_data_ms>=10000);
    setup(&d,&s); s.ms=UINT32_MAX-30;
    assert(wf_mmc5603_begin_selftest(&d,s.ms,WF_MAG_THRESHOLD_ENCODED_EXPERIMENTAL)); finish_selftest(&d,&s);
    assert(d.snapshot.ended==WF_MAG_SELFTEST_COMPLETE && d.snapshot.stopped && d.snapshot.selftest_status_ms<100);
    /* Explicit encoded sampling retains the native PASS gate and the existing
     * single-shot data/stop state machine, rather than enabling continuous mode. */
    setup(&d,&s);
    assert(!wf_mmc5603_begin_encoded(&d,0,999) && !wf_mmc5603_begin_encoded(&d,0,120001));
    s.factory[0]=0x63; s.factory[1]=0x7a; s.factory[2]=0x72;
    assert(wf_mmc5603_begin_encoded(&d,0,2000));
    for (unsigned i=0;i<10 && !d.snapshot.samples;++i) poll_next(&d,&s);
    assert(d.snapshot.threshold_mode==WF_MAG_THRESHOLD_ENCODED_EXPERIMENTAL && !d.snapshot.selftest_only);
    assert(d.snapshot.selftest_valid && d.snapshot.selftest_passed && d.snapshot.samples==1 && d.snapshot.sample_valid);
    assert(d.snapshot.selftest_threshold[0]==0x5c && d.snapshot.selftest_threshold[1]==0x13 && d.snapshot.selftest_threshold[2]==0x2c);
    assert(d.snapshot.x==12345 && d.snapshot.y==-23456 && d.snapshot.z==34567 && s.operations==16 && s.triggers==2);
    uint8_t selftest_response[9]; memcpy(selftest_response,d.snapshot.selftest_raw,9);
    ops=s.operations; wf_mmc5603_poll(&d); assert(s.operations==ops && d.snapshot.samples==1);
    fake_mag_counts(&s,-100,200,-300); poll_next(&d,&s); poll_next(&d,&s);
    assert(d.snapshot.samples==2 && d.snapshot.x==-100 && d.snapshot.y==200 && d.snapshot.z==-300);
    assert(!memcmp(selftest_response,d.snapshot.selftest_raw,9));
    s.ms=2000; wf_mmc5603_poll(&d);
    assert(d.snapshot.ended==WF_MAG_DEADLINE && d.snapshot.stopped && !d.snapshot.owned);
    first_frame(&d,&s);
    assert(d.snapshot.threshold_mode==WF_MAG_THRESHOLD_RAW80 && !d.snapshot.selftest_only && d.snapshot.samples==1);
    assert(d.snapshot.selftest_threshold[0]==0x4f && d.snapshot.selftest_threshold[1]==0x61 && d.snapshot.selftest_threshold[2]==0x5b);
    setup(&d,&s); s.selftest_fail=true;
    assert(wf_mmc5603_begin_encoded(&d,0,2000)); finish_selftest(&d,&s);
    assert(d.snapshot.ended==WF_MAG_SELFTEST_FAILED && !d.snapshot.samples && d.snapshot.stopped && s.triggers==1);
    for (unsigned operation=1;operation<=16;++operation) {
        setup(&d,&s); s.fail_at=operation;
        assert(wf_mmc5603_begin_encoded(&d,0,2000)); finish_selftest(&d,&s);
        assert(d.snapshot.phase==WF_MAG_FAULT && !d.snapshot.samples && !d.snapshot.sample_valid);
        ops=s.operations; wf_mmc5603_poll(&d); assert(s.operations==ops);
    }
    setup(&d,&s); s.ms=UINT32_MAX-30;
    assert(wf_mmc5603_begin_encoded(&d,s.ms,2000));
    for (unsigned i=0;i<10 && !d.snapshot.samples;++i) poll_next(&d,&s);
    assert(d.snapshot.samples==1 && d.snapshot.sample_valid && d.snapshot.sampled_ms<100);
    s.cancel_data=true; poll_next(&d,&s); poll_next(&d,&s);
    assert(d.snapshot.ended==WF_MAG_CANCELLED && d.snapshot.samples==1 && d.snapshot.stopped);
    return 0;
}
