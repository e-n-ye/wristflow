#include "cycle_record.h"
#include <string.h>
static void put(uint8_t *p,uint64_t x,unsigned n) { while(n--) {*p++=(uint8_t)x;x>>=8;} }
static uint64_t get(const uint8_t *p,unsigned n) { uint64_t x=0;for(unsigned i=0;i<n;++i)x|=(uint64_t)p[i]<<(i*8);return x; }
uint32_t wf_cycle_crc(const void *data,size_t size)
{
    const uint8_t *p=data;uint32_t x=UINT32_MAX;
    while(size--) {x^=*p++;for(unsigned i=0;i<8;++i)x=(x>>1)^((x&1)?0xedb88320u:0);}
    return x^UINT32_MAX;
}
bool wf_cycle_encode(const wf_cycle_row_t *r,uint8_t out[64])
{
    if(!r || !out || !r->session || r->sequence>=WF_CYCLE_MAX_ROWS ||
        r->kind<WF_CYCLE_START || r->kind>(r->low_voltage ? WF_CYCLE_EVENT:WF_CYCLE_END) || r->elapsed>WF_CYCLE_MAX_MS+60000 ||
        (!r->low_voltage && r->observation.diagnostic.filtered_decimillivolts>65535) ||
        (r->kind==WF_CYCLE_EVENT ? (!r->low_voltage || !r->event || r->event>WF_CYCLE_LCD_OFF_DONE):r->event!=0) ||
        (r->low_voltage && r->kind!=WF_CYCLE_END && r->reason) ||
        r->keep_remaining_ms>UINT32_C(65535000)) return false;
    const wristflow_battery_record_t *o=&r->observation;const wristflow_power_snapshot_t *p=&o->power;
    memset(out,0,64);memcpy(out,"WFCL",4);out[4]=r->low_voltage ? 2:1;out[5]=r->kind;out[7]=r->event;
    out[6]=(r->utc_valid?1:0)|(o->adc_valid?2:0)|(p->estimate_valid?4:0)|
        (!r->low_voltage && o->diagnostic.filter_valid?8:0)|(r->flashlight?16:0)|(p->voltage_valid?32:0);
    put(out+8,r->session,4);put(out+12,r->sequence,4);put(out+16,r->elapsed,8);
    put(out+24,r->utc,4);put(out+28,r->revision,4);
    put(out+32,p->voltage_sampled_ms,4);put(out+36,p->voltage_sequence,4);
    put(out+40,p->millivolts,2);put(out+42,r->low_voltage ?
        ((o->context.flags&WF_BATTERY_UI_VALID) ? (r->keep_remaining_ms+999)/1000:0):o->diagnostic.filtered_decimillivolts,2);
    out[44]=p->estimated_percent;out[45]=o->diagnostic.candidate;out[46]=o->diagnostic.mode;out[47]=o->diagnostic.decision;
    if(r->low_voltage && r->kind==WF_CYCLE_EVENT) {out[6]&=~4u;put(out+44,r->event_tick_ms,4);}
    out[48]=p->charge;out[49]=p->charge_reason;out[50]=o->context.requested_brightness;out[51]=o->context.acknowledged_brightness;
    put(out+52,o->context.surface,2);out[54]=o->context.flags;out[55]=o->context.phase;
    out[56]=r->reason;out[57]=(p->input_valid?1:0)|(p->input_good?2:0)|
        (p->input_observation.valid?4:0)|(p->input_observation.good?8:0);
    uint32_t age=o->observed_ms-p->voltage_sampled_ms;
    put(out+58,age>65535 ? 65535:age,2);
    put(out+60,wf_cycle_crc(out,60),4);return true;
}
bool wf_cycle_decode(const uint8_t in[64],wf_cycle_row_t *r)
{
    if(!in || !r || memcmp(in,"WFCL",4) || (in[4]!=1 && in[4]!=2) || in[5]<1 || in[5]>(in[4]==2 ? 4:3) ||
        in[6]&~63u || (in[4]==1 && in[7]) || (in[4]==2 && (in[6]&8)) ||
        (in[5]==WF_CYCLE_EVENT && (in[6]&4)) ||
        (in[4]==2 && in[5]!=WF_CYCLE_END && in[56]) ||
        (in[5]==WF_CYCLE_EVENT ? (!in[7] || in[7]>WF_CYCLE_LCD_OFF_DONE):in[7]!=0) ||
        get(in+60,4)!=wf_cycle_crc(in,60))return false;
    *r=(wf_cycle_row_t){.kind=in[5],.session=(uint32_t)get(in+8,4),.sequence=(uint32_t)get(in+12,4),
        .elapsed=get(in+16,8),.utc=(uint32_t)get(in+24,4),.revision=(uint32_t)get(in+28,4),
        .utc_valid=(in[6]&1)!=0,.flashlight=(in[6]&16)!=0,.reason=in[56],
        .low_voltage=in[4]==2,.event=in[7],.keep_remaining_ms=in[4]==2 ? (uint32_t)get(in+42,2)*1000:0};
    if(r->low_voltage && r->kind==WF_CYCLE_EVENT)r->event_tick_ms=(uint32_t)get(in+44,4);
    wristflow_battery_record_t *o=&r->observation;wristflow_power_snapshot_t *p=&o->power;
    o->adc_valid=(in[6]&2)!=0;p->estimate_valid=(in[6]&4)!=0;o->diagnostic.filter_valid=(in[6]&8)!=0;p->voltage_valid=(in[6]&32)!=0;
    p->voltage_sampled_ms=(uint32_t)get(in+32,4);p->voltage_sequence=(uint32_t)get(in+36,4);
    o->observed_ms=p->voltage_sampled_ms+(uint32_t)get(in+58,2);
    p->millivolts=(uint16_t)get(in+40,2);o->diagnostic.filtered_decimillivolts=in[4]==1 ? (uint32_t)get(in+42,2):0;
    p->estimated_percent=in[44];o->diagnostic.candidate=in[45];o->diagnostic.mode=in[46];o->diagnostic.decision=in[47];
    if(r->low_voltage && r->kind==WF_CYCLE_EVENT) {p->estimated_percent=0;o->diagnostic.candidate=o->diagnostic.mode=o->diagnostic.decision=0;}
    p->charge=in[48];p->charge_reason=in[49];o->context.requested_brightness=in[50];o->context.acknowledged_brightness=in[51];
    o->context.surface=(uint16_t)get(in+52,2);o->context.flags=in[54];o->context.phase=in[55];
    p->input_valid=(in[57]&1)!=0;p->input_good=(in[57]&2)!=0;p->input_observation.valid=(in[57]&4)!=0;p->input_observation.good=(in[57]&8)!=0;
    return r->session && r->sequence<WF_CYCLE_MAX_ROWS && r->elapsed<=WF_CYCLE_MAX_MS+60000 &&
        (!r->utc_valid || (r->utc>=1767225600u && r->utc<=4102444799u)) &&
        (!p->estimate_valid || p->estimated_percent<=100) && in[57]<16 && r->reason<=WF_CYCLE_QUEUE;
}
unsigned wf_cycle_stop_reason(const wristflow_battery_record_t *o,bool flashlight,uint64_t elapsed,bool smoke)
{
    if(!o->adc_valid || !o->power.voltage_valid || !o->power.voltage_sequence ||
        o->observed_ms-o->power.voltage_sampled_ms>15000) return WF_CYCLE_STALE;
    if(o->power.millivolts<=3600) return WF_CYCLE_LOW;
    if(elapsed>=WF_CYCLE_MAX_MS) return WF_CYCLE_LIMIT;
    if(!smoke && (!o->power.input_valid || o->power.input_good)) return WF_CYCLE_USB;
    if(!smoke && !flashlight) return WF_CYCLE_LOAD;
    return 0;
}
