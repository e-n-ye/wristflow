#include "battery_estimate.h"
#include "product_state.h"
#include <assert.h>
#include <string.h>

static wristflow_power_snapshot_t sample(uint32_t sequence, uint32_t at, unsigned mv)
{
    return (wristflow_power_snapshot_t){.voltage_valid=true,.millivolts=(uint16_t)mv,
        .input_valid=true,.charge=WF_POWER_CHARGE_IDLE,
        .voltage_sequence=sequence,.voltage_sampled_ms=at};
}
static wristflow_battery_estimator_t warm(unsigned percent)
{
    return (wristflow_battery_estimator_t){.seeded=true,.mode=0,.percent=(uint8_t)percent,
        .sequence=10,.filtered_decimillivolts=41430,.sampled_ms=0};
}
static void feed(wristflow_battery_estimator_t *state, uint32_t seq, uint32_t at, unsigned expected)
{
    wristflow_power_snapshot_t p=sample(seq,at,4143);
    wristflow_battery_estimate(state,&p,at);
    assert(p.estimate_valid && p.estimated_percent==expected);
}
static void prepare_dwell(wristflow_battery_estimator_t *state)
{
    *state=warm(97);
    feed(state,11,1,97); feed(state,12,15001,97);
    feed(state,13,30001,97); feed(state,14,45001,97);
}
static void settling_boundaries(void)
{
    wristflow_battery_estimator_t state;
    prepare_dwell(&state);
    feed(&state,15,60000,97); /* ADC span59999ms, not yet60s. */
    feed(&state,16,60001,98);
    state=warm(99); /* Small downward differences converge too. */
    for (unsigned i=0;i<5;++i) feed(&state,11+i,1+i*15000,i==4?98:99);
    state=warm(97);
    for (unsigned i=0;i<100;++i) feed(&state,11+i,1+i,97); /* Samples alone are insufficient. */

    prepare_dwell(&state);
    wristflow_power_snapshot_t p=sample(14,45001,4143);
    wristflow_battery_estimate(&state,&p,60001); /* UI time passed, no new ADC. */
    assert(p.estimated_percent==97);
    feed(&state,15,60002,97); /*15001ms support gap restarts; no filter reseed. */
    for (unsigned i=1;i<=4;++i) feed(&state,15+i,60002+i*15000,i==4?98:97);

    prepare_dwell(&state);
    p=sample(15,59999,4143); wristflow_battery_estimate(&state,&p,60001);
    assert(p.estimated_percent==97); /* Delayed UI observation cannot add ADC time. */

    prepare_dwell(&state);
    p=sample(14,45002,4143); wristflow_battery_estimate(&state,&p,45002);
    feed(&state,15,60001,97); /* Mutated timestamp of a cached identity broke dwell. */
    prepare_dwell(&state);
    feed(&state,13,46001,97); feed(&state,15,60001,97); /* Old sequence cannot count. */
    prepare_dwell(&state);
    feed(&state,15,44001,97); feed(&state,16,60001,97); /* Backwards ADC cannot count. */

    prepare_dwell(&state);
    p=sample(15,46001,4100); wristflow_battery_estimate(&state,&p,46001);
    assert(p.estimated_percent==97);
    feed(&state,16,60001,97); /* Candidate returned after changing; fresh dwell. */

    for (unsigned reason=0;reason<4;++reason) {
        prepare_dwell(&state); p=sample(15,46001,4143);
        if (reason==0) p.input_valid=false;
        if (reason==1) p.charge=WF_POWER_CHARGE_UNKNOWN;
        if (reason==2) p.charge=WF_POWER_CHARGE_FAULT;
        if (reason==3) p.input_good=true; /* USB-present IDLE is not trusted BAT. */
        wristflow_battery_estimate(&state,&p,46001);
        feed(&state,16,60001,97);
    }
    prepare_dwell(&state);
    p=sample(14,45001,4143); p.charge=WF_POWER_CHARGE_ACTIVE;
    wristflow_battery_estimate(&state,&p,45002); assert(!p.estimate_valid);
    feed(&state,15,60001,97); /* MODE_WAIT canceled the pending dwell. */

    for (unsigned reason=0;reason<4;++reason) {
        prepare_dwell(&state); p=sample(15,46001,4143);
        if (reason==0) p.voltage_valid=false;
        if (reason==1) p.voltage_sequence=0;
        if (reason==2) p.millivolts=2999;
        if (reason==3) p.voltage_sampled_ms=30000;
        wristflow_battery_estimate(&state,&p,46001); assert(!p.estimate_valid);
        feed(&state,16,60001,98); /* Invalid data clears all history; fresh seed. */
    }
    state=warm(97); state.mode=1; state.filtered_decimillivolts=41800;
    for (unsigned i=0;i<8;++i) {
        p=sample(11+i,1+i*15000,4180); p.charge=WF_POWER_CHARGE_ACTIVE; p.input_good=true;
        wristflow_battery_estimate(&state,&p,p.voltage_sampled_ms);
        assert(p.estimate_valid && p.estimated_percent==97); /* Charging unchanged. */
    }
    p.charge=WF_POWER_CHARGE_FULL; wristflow_battery_estimate(&state,&p,p.voltage_sampled_ms);
    assert(p.estimated_percent==100);
    p.voltage_valid=false; wristflow_battery_estimate(&state,&p,p.voltage_sampled_ms);
    assert(!p.estimate_valid);
    state=warm(96); feed(&state,11,1,98); /* Existing two-point step remains immediate. */
    state=warm(1); state.filtered_decimillivolts=30000;
    p=sample(11,1,3000); wristflow_battery_estimate(&state,&p,1); assert(p.estimated_percent==0);
    state=warm(97); state.filtered_decimillivolts=42500;
    p=sample(11,1,4250); wristflow_battery_estimate(&state,&p,1); assert(p.estimated_percent==99);

    uint32_t origin=UINT32_MAX-1000U;
    state=warm(97); state.sampled_ms=origin; state.sequence=UINT32_MAX-3U;
    uint32_t ids[]={UINT32_MAX-2U,UINT32_MAX-1U,UINT32_MAX,1,2};
    for (unsigned i=0;i<5;++i) feed(&state,ids[i],origin+1U+i*15000U,i==4?98:97);
}
int main(void)
{
    settling_boundaries();
    assert(wristflow_battery_reference_percent(3504,false)==0);
    assert(wristflow_battery_reference_percent(3900,false)==49);
    assert(wristflow_battery_reference_percent(3901,false)==50);
    assert(wristflow_battery_reference_percent(3947,true)==50);
    assert(wristflow_battery_reference_percent(4250,true)==100);
    unsigned previous=0;
    for (unsigned mv=3000;mv<=4250;++mv) {
        unsigned p=wristflow_battery_reference_percent((uint16_t)mv,false);
        assert(p>=previous&&p<=100); previous=p;
    }
    wristflow_battery_estimator_t state={0};
    wristflow_power_snapshot_t power=sample(1,100,3901);
    wristflow_battery_estimate(&state,&power,100);
    assert(power.estimate_valid&&power.estimated_percent==50);
    power.millivolts=4100; /* A cached identity is never a new sample. */
    for (unsigned i=0;i<100;++i) wristflow_battery_estimate(&state,&power,200);
    assert(power.estimated_percent==50&&state.filtered_decimillivolts==39010);
    power=sample(2,5000,4100); wristflow_battery_estimate(&state,&power,5000);
    assert(state.filtered_decimillivolts>39010&&state.filtered_decimillivolts<41000);
    unsigned once=state.filtered_decimillivolts;
    wristflow_battery_estimate(&state,&power,5001); assert(state.filtered_decimillivolts==once);
    power.charge=WF_POWER_CHARGE_ACTIVE; wristflow_battery_estimate(&state,&power,5002);
    assert(!power.estimate_valid); /* Transition waits for a fresh ADC identity. */
    power=sample(3,10000,4250); power.charge=WF_POWER_CHARGE_ACTIVE;
    wristflow_battery_estimate(&state,&power,10000); assert(power.estimate_valid&&power.estimated_percent==99);
    power.charge=WF_POWER_CHARGE_FULL; wristflow_battery_estimate(&state,&power,10001);
    assert(power.estimate_valid&&power.estimated_percent==100);
    wristflow_watch_snapshot_t snapshot=wristflow_product_snapshot(false,0);
    wristflow_product_apply_power(&snapshot,&power); char text[20];
    wristflow_battery_text(&snapshot,text,sizeof text); assert(!strcmp(text,"~100%"));
    wristflow_battery_estimate(&state,&power,25000); assert(power.estimate_valid);
    wristflow_battery_estimate(&state,&power,25001); assert(!power.estimate_valid&&!power.voltage_valid);
    for (unsigned mv=0;mv<3000;mv+=178) {
        power=sample(4,30000,mv); power.charge=WF_POWER_CHARGE_FULL;
        wristflow_battery_estimate(&state,&power,30000); assert(!power.estimate_valid&&!state.seeded);
    }
    power=sample(5,40000,4251); wristflow_battery_estimate(&state,&power,40000); assert(!power.estimate_valid);
    power=sample(6,UINT32_MAX-100U,3901); wristflow_battery_estimate(&state,&power,20);
    assert(power.estimate_valid); /* Monotonic milliseconds wrap by subtraction. */
    power.voltage_valid=false; wristflow_battery_estimate(&state,&power,21); assert(!power.estimate_valid);
    power=sample(7,100,3901); power.input_valid=false;
    wristflow_battery_estimate(&state,&power,100); assert(!power.estimate_valid);
    power.input_valid=true; wristflow_battery_estimate(&state,&power,101); assert(power.estimate_valid);
    wristflow_power_snapshot_t other=power; ++other.voltage_sequence; ++other.voltage_sampled_ms;
    assert(wristflow_power_equal(&power,&other)); --other.estimated_percent;
    assert(!wristflow_power_equal(&power,&other));
    power.voltage_sequence=0; wristflow_battery_estimate(&state,&power,102); assert(!power.estimate_valid);
    wristflow_product_apply_power(&snapshot,&power); wristflow_battery_text(&snapshot,text,sizeof text);
    assert(snapshot.battery_unavailable&&!strcmp(text,"--"));
    assert(snapshot.battery_percent==0);
    snapshot.battery_unavailable=false; snapshot.battery_percent=53;
    wristflow_battery_text(&snapshot,text,sizeof text); assert(!strcmp(text,"53%"));
    /* UNKNOWN or FAULT cannot be relabelled as discharging even with cached PG. */
    state=(wristflow_battery_estimator_t){0};
    power=sample(10,1000,3901); wristflow_battery_estimate(&state,&power,1000);
    unsigned old_sequence=state.sequence;
    power=sample(11,6000,4100); power.charge=WF_POWER_CHARGE_UNKNOWN;
    wristflow_battery_estimate(&state,&power,6000); assert(!power.estimate_valid&&state.sequence==old_sequence);
    power.charge=WF_POWER_CHARGE_FAULT;
    wristflow_battery_estimate(&state,&power,6001); assert(!power.estimate_valid&&state.sequence==old_sequence);
    power.charge=WF_POWER_CHARGE_IDLE; power.input_valid=false;
    wristflow_battery_estimate(&state,&power,6002); assert(!power.estimate_valid&&state.sequence==old_sequence);
    power.input_valid=true; wristflow_battery_estimate(&state,&power,6003);
    assert(power.estimate_valid&&state.sequence==11&&state.filtered_decimillivolts<41000);
    once=state.filtered_decimillivolts; wristflow_battery_estimate(&state,&power,6004);
    assert(state.filtered_decimillivolts==once);
    power.charge=WF_POWER_CHARGE_FULL; wristflow_battery_estimate(&state,&power,6005);
    assert(power.estimated_percent==100);
    power.charge=WF_POWER_CHARGE_ACTIVE; wristflow_battery_estimate(&state,&power,6006);
    assert(!power.estimate_valid);
    power.charge=WF_POWER_CHARGE_IDLE; wristflow_battery_estimate(&state,&power,6007);
    assert(!power.estimate_valid);
    power=sample(12,11000,3901); wristflow_battery_estimate(&state,&power,11000);
    assert(power.estimate_valid&&power.estimated_percent==50);
    other=power; other.voltage_sequence=13; other.voltage_sampled_ms=16000;
    wristflow_battery_estimate(&state,&other,16000); assert(wristflow_power_equal(&power,&other));
    wristflow_battery_estimate(&state,&other,31001); assert(!wristflow_power_equal(&power,&other));
    power=(wristflow_power_snapshot_t){0}; wristflow_battery_estimate(&state,&power,32000);
    assert(!state.seeded); /* Sleep clears exposure/history; wake seeds from fresh voltage. */
    power=sample(14,33000,4100); wristflow_battery_estimate(&state,&power,33000);
    assert(state.filtered_decimillivolts==41000&&power.estimate_valid);
    /* Diagnostics describe the existing decision, including withheld history. */
    wristflow_battery_diagnostic_t d;
    state=(wristflow_battery_estimator_t){0}; power=sample(20,100,4040);
    wristflow_battery_estimate_diagnosed(&state,&power,100,&d);
    assert(d.decision==WF_BATTERY_SEED && d.filter_valid && d.candidate==91 && d.mode==0);
    wristflow_battery_estimate_diagnosed(&state,&power,101,&d);
    assert(d.decision==WF_BATTERY_CACHED && d.filtered_decimillivolts==40400);
    power=sample(21,200,4000); wristflow_battery_estimate_diagnosed(&state,&power,200,&d);
    assert(d.decision==WF_BATTERY_NEW_SAMPLE && d.filtered_decimillivolts==40300);
    power.charge=WF_POWER_CHARGE_ACTIVE; wristflow_battery_estimate_diagnosed(&state,&power,201,&d);
    assert(d.decision==WF_BATTERY_MODE_WAIT && !d.filter_valid && !d.filtered_decimillivolts);
    power=sample(22,300,4000); power.charge=WF_POWER_CHARGE_ACTIVE;
    wristflow_battery_estimate_diagnosed(&state,&power,300,&d);
    assert(d.decision==WF_BATTERY_MODE_SEED && d.mode==1 && d.filtered_decimillivolts==40000);
    power.charge=WF_POWER_CHARGE_UNKNOWN; wristflow_battery_estimate_diagnosed(&state,&power,301,&d);
    assert(d.decision==WF_BATTERY_MODE_UNKNOWN && d.mode==255 && !d.filter_valid && state.seeded);
    wristflow_battery_estimate_diagnosed(&state,&power,15301,&d);
    assert(d.decision==WF_BATTERY_STALE && !state.seeded && !d.filter_valid);
    power=sample(23,16000,2999); wristflow_battery_estimate_diagnosed(&state,&power,16000,&d);
    assert(d.decision==WF_BATTERY_RANGE);
    power.voltage_valid=false; wristflow_battery_estimate_diagnosed(&state,&power,16000,&d);
    assert(d.decision==WF_BATTERY_NO_VOLTAGE);
    power.voltage_sequence=0; wristflow_battery_estimate_diagnosed(&state,&power,16000,&d);
    assert(d.decision==WF_BATTERY_NO_SEQUENCE);
    return 0;
}
