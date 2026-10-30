#include "runtime_record.h"
#include "product_state.h"
#include <limits.h>
#include <string.h>
static void inc(uint32_t *n) { if(*n!=UINT32_MAX)++*n; }
static uint32_t add32(uint32_t a,uint32_t b) { return UINT32_MAX-a<b?UINT32_MAX:a+b; }
static uint64_t add64(uint64_t a,uint64_t b) { return UINT64_MAX-a<b ? UINT64_MAX:a+b; }
static void event(wf_runtime_record_t *r,unsigned kind,uint32_t value,uint32_t detail)
{
    wf_runtime_summary_t *s=&r->summary;
    if(s->event_count==WF_RUNTIME_EVENTS) {
        memmove(s->events,s->events+1,(WF_RUNTIME_EVENTS-1)*sizeof s->events[0]);
        --s->event_count;inc(&s->event_drops);
    }
    s->events[s->event_count++]=(wf_runtime_event_t){.ms=r->last_ms,.utc=s->utc,
        .value=value,.detail=detail,.boot=s->boot,.clock_revision=s->clock_revision,
        .kind=(uint16_t)kind,.scene=(uint8_t)r->scene,.flags=(uint8_t)(s->flags&WF_RUN_UTC_VALID)};
    inc(&s->events_total);
}
static bool settle(wf_runtime_record_t *r,uint64_t now)
{
    if(now<r->last_ms) { inc(&r->summary.clock_errors);return false; }
    uint64_t delta=now-r->last_ms;r->last_ms=now;
    if(r->running) {
        r->summary.uptime_ms=add64(r->summary.uptime_ms,delta);
        r->summary.total_ms=add64(r->summary.total_ms,delta);
        r->summary.scene_ms[r->scene]=add64(r->summary.scene_ms[r->scene],delta);
    }
    return true;
}
void wf_runtime_boot(wf_runtime_record_t *r,const wf_runtime_summary_t *seed,uint64_t now,uint32_t mode,uint32_t raw)
{
    memset(r,0,sizeof *r);
    if(seed)r->summary=*seed;
    wf_runtime_summary_t *s=&r->summary;
    s->boot=seed ? seed->boot+1:1;if(!s->boot)s->boot=1;
    s->flags=WF_RUN_RECORDING|(seed?WF_RUN_RESTORED:0);
    s->utc=s->clock_revision=0;s->uptime_ms=s->saved_uptime_ms=0;
    s->first=(wf_runtime_sample_t){0};s->last=(wf_runtime_sample_t){0};
    s->fault_known=s->fault_mask=0;s->boot_mode=mode;s->boot_raw=raw;
    r->last_ms=now;r->running=true;r->scene=WF_RUN_UNKNOWN;
    event(r,WF_RUN_BOOT,mode,raw);
}
static bool power_changed(const wf_runtime_sample_t *a,const wf_runtime_sample_t *b)
{ return ((a->flags^b->flags)&(WF_RUN_INPUT_VALID|WF_RUN_INPUT_GOOD)) || a->charge!=b->charge; }
bool wf_runtime_observe(wf_runtime_record_t *r,const wf_runtime_observation_t *o)
{
    if(!r || !o || (unsigned)o->scene>=WF_RUN_SCENES || !settle(r,o->ms))return false;
    if(!r->running)return true;
    wf_runtime_summary_t *s=&r->summary;
    bool clock=s->clock_revision!=o->clock_revision || !!(s->flags&WF_RUN_UTC_VALID)!=o->utc_valid;
    s->utc=o->utc_valid?o->utc:0;s->clock_revision=o->clock_revision;
    s->flags=(s->flags&~WF_RUN_UTC_VALID)|(o->utc_valid?WF_RUN_UTC_VALID:0);
    if(clock)event(r,WF_RUN_CLOCK,o->clock_revision,s->utc);
    bool scene=r->scene!=o->scene;
    if(scene) { r->scene=o->scene;event(r,WF_RUN_SCENE,(uint32_t)o->scene,0); }
    wf_runtime_sample_t v=o->sample;v.ms=o->ms;v.utc=s->utc;v.scene=(uint8_t)o->scene;
    bool power=r->have_sample && power_changed(&s->last,&v);
    if(power)event(r,WF_RUN_POWER,v.flags,v.charge);
    if(!r->have_sample || scene || power) { s->first=v;inc(&s->segment); }
    if(!r->have_sample || scene || power || v.sequence!=s->last.sequence || v.flags!=s->last.flags ||
        v.requested!=s->last.requested || v.acknowledged!=s->last.acknowledged ||
        v.millivolts!=s->last.millivolts || v.percent!=s->last.percent) {
        if(r->sample_count==WF_RUNTIME_SAMPLES)inc(&s->sample_drops);else ++r->sample_count;
        r->samples[r->sample_head]=v;r->sample_head=(r->sample_head+1)%WF_RUNTIME_SAMPLES;inc(&s->samples);
    }
    r->have_sample=true;s->last=v;
    uint32_t comparable=s->fault_known&o->fault_known;
    uint32_t changed=(s->fault_mask^o->fault_mask)&comparable;
    uint32_t failed=changed&o->fault_mask,recovered=changed&~o->fault_mask;
    /* A first known fault is an observed failure; a first healthy value is not a recovery. */
    failed|=o->fault_mask&o->fault_known&~s->fault_known;
    if(failed) { inc(&s->faults);event(r,WF_RUN_FAULT,failed,0); }
    if(recovered) { inc(&s->recoveries);event(r,WF_RUN_RECOVERY,recovered,0); }
    s->fault_mask=(s->fault_mask&~o->fault_known)|(o->fault_mask&o->fault_known);
    s->fault_known|=o->fault_known; /* Unknown/offline intervals cannot prove recovery. */
    if(o->io_known) {
        uint32_t retries=!r->have_io || o->io_retries<r->last_io_retries ? o->io_retries:o->io_retries-r->last_io_retries;
        uint32_t recoveries=!r->have_io || o->io_recoveries<r->last_io_recoveries ? o->io_recoveries:o->io_recoveries-r->last_io_recoveries;
        if(r->io_baseline_only) { retries=recoveries=0;r->io_baseline_only=false; }
        s->io_retries=add32(s->io_retries,retries);s->io_recoveries=add32(s->io_recoveries,recoveries);
        r->last_io_retries=o->io_retries;r->last_io_recoveries=o->io_recoveries;r->have_io=true;
    }
    return true;
}
void wf_runtime_control(wf_runtime_record_t *r,uint64_t now,bool start)
{
    if(!r || !settle(r,now) || start==r->running)return;
    r->running=start;r->scene=WF_RUN_UNKNOWN;r->have_sample=false;
    if(start)r->io_baseline_only=true;
    /* Control has no fresh RTC observation. Never pair a new monotonic event
     * with a potentially old cached UTC after a long unobserved interval. */
    r->summary.utc=0;r->summary.flags&=~WF_RUN_UTC_VALID;
    r->summary.flags=(r->summary.flags&~WF_RUN_RECORDING)|(start?WF_RUN_RECORDING:0);
    event(r,start?WF_RUN_START:WF_RUN_STOP,0,0);
}
void wf_runtime_save_result(wf_runtime_record_t *r,uint64_t saved,bool ok,bool periodic)
{
    inc(&r->summary.attempts);
    if(ok) { inc(&r->summary.successes);r->summary.saved_uptime_ms=saved;
        inc(periodic?&r->summary.periodic_saves:&r->summary.explicit_saves); }
    else { inc(&r->summary.failures);event(r,WF_RUN_SAVE_FAILED,0,0); }
}
bool wf_runtime_sample_read(const wf_runtime_record_t *r,unsigned index,wf_runtime_sample_t *out)
{
    if(!r || !out || index>=r->sample_count)return false;
    *out=r->samples[(r->sample_head+WF_RUNTIME_SAMPLES-r->sample_count+index)%WF_RUNTIME_SAMPLES];return true;
}
static void put(uint8_t *p,uint64_t v,unsigned n) { for(unsigned i=0;i<n;++i)p[i]=(uint8_t)(v>>(8*i)); }
static uint64_t get(const uint8_t *p,unsigned n) { uint64_t v=0;for(unsigned i=0;i<n;++i)v|=(uint64_t)p[i]<<(8*i);return v; }
static uint32_t crc(const uint8_t *p)
{
    uint32_t v=UINT32_MAX;
    for(unsigned i=0;i<WF_RUNTIME_BYTES;++i) {
        v^=i>=28 && i<32 ? 0:p[i];
        for(unsigned j=0;j<8;++j)v=(v>>1)^((0U-(v&1))&0xedb88320U);
    }
    return ~v;
}
static void sample_encode(uint8_t *p,const wf_runtime_sample_t *v)
{
    put(p,v->ms,8);put(p+8,v->sequence,4);put(p+12,v->sampled_ms,4);put(p+16,v->utc,4);
    put(p+20,v->millivolts,2);put(p+22,v->flags,2);p[24]=v->percent;p[25]=v->requested;
    p[26]=v->acknowledged;p[27]=v->charge;p[28]=v->scene;
}
static wf_runtime_sample_t sample_decode(const uint8_t *p)
{ return (wf_runtime_sample_t){get(p,8),(uint32_t)get(p+8,4),(uint32_t)get(p+12,4),(uint32_t)get(p+16,4),
    (uint16_t)get(p+20,2),(uint16_t)get(p+22,2),p[24],p[25],p[26],p[27],p[28]}; }
static bool sample_valid(const wf_runtime_sample_t *s)
{ return s->scene<WF_RUN_SCENES && s->charge<=5 && !(s->flags&~63U) &&
    (!s->utc || (s->utc>=WRISTFLOW_TIME_MIN && s->utc<=WRISTFLOW_TIME_MAX)) &&
    (!(s->flags&WF_RUN_ESTIMATE_VALID)||s->percent<=100) &&
    (!(s->flags&WF_RUN_BRIGHTNESS_VALID)||(s->requested<=100 && s->acknowledged<=100)); }
static bool valid(const wf_runtime_summary_t *s)
{
    if(!s || !s->boot || (s->flags&~7U) || s->event_count>8 || s->saved_uptime_ms>s->uptime_ms ||
        s->uptime_ms>s->total_ms || !sample_valid(&s->first) || !sample_valid(&s->last))return false;
    if(s->flags&WF_RUN_UTC_VALID) { if(s->utc<WRISTFLOW_TIME_MIN || s->utc>WRISTFLOW_TIME_MAX)return false; }
    else if(s->utc)return false;
    uint64_t total=0;for(unsigned i=0;i<WF_RUN_SCENES;++i)total=add64(total,s->scene_ms[i]);
    if(total!=s->total_ms || (s->fault_mask&~s->fault_known))return false;
    for(unsigned i=0;i<s->event_count;++i)if(!s->events[i].boot || s->events[i].scene>=WF_RUN_SCENES ||
        s->events[i].kind<WF_RUN_BOOT || s->events[i].kind>WF_RUN_SAVE_FAILED || (s->events[i].flags&~1U))return false;
    for(unsigned i=0;i<s->event_count;++i) {
        const wf_runtime_event_t *e=&s->events[i];
        if(e->flags&WF_RUN_UTC_VALID) { if(e->utc<WRISTFLOW_TIME_MIN || e->utc>WRISTFLOW_TIME_MAX)return false; }
        else if(e->utc)return false;
    }
    return true;
}
bool wf_runtime_sample_encode(const wf_runtime_sample_t *s,uint8_t out[32])
{
    if(!s || !out || !sample_valid(s))return false;
    memset(out,0,32);sample_encode(out,s);return true;
}
bool wf_runtime_encode(const wf_runtime_summary_t *s,uint8_t p[WF_RUNTIME_BYTES])
{
    if(!p || !valid(s))return false;
    memset(p,0,512);memcpy(p,"WFRN",4);put(p+4,1,2);put(p+6,512,2);
    put(p+8,s->generation,4);put(p+12,s->boot,4);put(p+16,s->flags,4);put(p+20,s->utc,4);put(p+24,s->clock_revision,4);
    put(p+32,s->uptime_ms,8);put(p+40,s->total_ms,8);
    for(unsigned i=0;i<6;++i)put(p+48+8*i,s->scene_ms[i],8);
    sample_encode(p+96,&s->first);sample_encode(p+128,&s->last);
    const uint32_t counters[]={s->fault_mask,s->fault_known,s->faults,s->recoveries,s->clock_errors,s->event_drops,s->sample_drops,s->samples,
        s->attempts,s->successes,s->failures,s->periodic_saves,s->explicit_saves,s->events_total,s->io_retries,s->io_recoveries};
    for(unsigned i=0;i<16;++i)put(p+160+4*i,counters[i],4);
    put(p+224,s->boot_mode,4);put(p+228,s->boot_raw,4);put(p+232,s->saved_uptime_ms,8);
    put(p+240,s->event_count,4);put(p+244,s->segment,4);
    for(unsigned i=0;i<s->event_count;++i) {
        const wf_runtime_event_t *e=&s->events[i];uint8_t *v=p+256+32*i;
        put(v,e->ms,8);put(v+8,e->utc,4);put(v+12,e->value,4);put(v+16,e->detail,4);
        put(v+20,e->boot,4);put(v+24,e->clock_revision,4);put(v+28,e->kind,2);v[30]=e->scene;v[31]=e->flags;
    }
    put(p+28,crc(p),4);return true;
}
bool wf_runtime_decode(wf_runtime_summary_t *out,const uint8_t *p,size_t n)
{
    if(!out || !p || n!=512 || memcmp(p,"WFRN",4) || get(p+4,2)!=1 || get(p+6,2)!=512 || get(p+28,4)!=crc(p))return false;
    wf_runtime_summary_t s={.generation=(uint32_t)get(p+8,4),.boot=(uint32_t)get(p+12,4),.flags=(uint32_t)get(p+16,4),
        .utc=(uint32_t)get(p+20,4),.clock_revision=(uint32_t)get(p+24,4),.uptime_ms=get(p+32,8),.total_ms=get(p+40,8),
        .first=sample_decode(p+96),.last=sample_decode(p+128),.boot_mode=(uint32_t)get(p+224,4),.boot_raw=(uint32_t)get(p+228,4),
        .saved_uptime_ms=get(p+232,8),.event_count=(uint32_t)get(p+240,4),.segment=(uint32_t)get(p+244,4)};
    for(unsigned i=0;i<6;++i)s.scene_ms[i]=get(p+48+8*i,8);
    uint32_t *fields[]={&s.fault_mask,&s.fault_known,&s.faults,&s.recoveries,&s.clock_errors,&s.event_drops,&s.sample_drops,&s.samples,
        &s.attempts,&s.successes,&s.failures,&s.periodic_saves,&s.explicit_saves,&s.events_total,&s.io_retries,&s.io_recoveries};
    for(unsigned i=0;i<16;++i)*fields[i]=(uint32_t)get(p+160+4*i,4);
    if(s.event_count>8)return false;
    for(unsigned i=0;i<s.event_count;++i) {
        const uint8_t *v=p+256+32*i;
        s.events[i]=(wf_runtime_event_t){get(v,8),(uint32_t)get(v+8,4),(uint32_t)get(v+12,4),(uint32_t)get(v+16,4),
            (uint32_t)get(v+20,4),(uint32_t)get(v+24,4),(uint16_t)get(v+28,2),v[30],v[31]};
    }
    /* Re-encode to reject reserved bits/bytes and noncanonical hidden events. */
    uint8_t canonical[512];if(!wf_runtime_encode(&s,canonical) || memcmp(canonical,p,512))return false;
    *out=s;return true;
}
int wf_runtime_select(wf_runtime_summary_t *out,const uint8_t *a,size_t an,const uint8_t *b,size_t bn)
{
    wf_runtime_summary_t x,y;bool av=wf_runtime_decode(&x,a,an),bv=wf_runtime_decode(&y,b,bn);
    if(!out || (!av&&!bv))return -1;
    if(av && bv) {
        uint32_t delta=y.generation-x.generation;
        if(delta==0x80000000U || (!delta && memcmp(a,b,512)))return -1;
        if(delta && delta<0x80000000U) { *out=y;return 1; }
    }
    *out=av?x:y;return av?0:1;
}
