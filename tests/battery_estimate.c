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
int main(void)
{
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
    return 0;
}
