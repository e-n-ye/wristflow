#include "runtime_record.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static wf_runtime_observation_t observation(uint64_t ms,wf_runtime_scene_t scene,unsigned seq)
{
    return (wf_runtime_observation_t){.ms=ms,.scene=scene,.sample={.sequence=seq,
        .millivolts=3800,.flags=WF_RUN_ADC_VALID|WF_RUN_ESTIMATE_VALID,.percent=50}};
}
static void reseal(uint8_t *p)
{
    uint32_t v=UINT32_MAX;
    for(unsigned i=0;i<512;++i) {
        v^=i>=28 && i<32 ? 0:p[i];
        for(unsigned j=0;j<8;++j)v=(v>>1)^((0U-(v&1))&0xedb88320U);
    }
    v=~v;for(unsigned i=0;i<4;++i)p[28+i]=(uint8_t)(v>>(8*i));
}
int main(void)
{
    wf_runtime_record_t r;wf_runtime_boot(&r,NULL,0,4,0);
    wf_runtime_observation_t o=observation(100,WF_RUN_ON,1);assert(wf_runtime_observe(&r,&o));
    o=observation(1100,WF_RUN_FLASHLIGHT,2);assert(wf_runtime_observe(&r,&o));
    o=observation(2100,WF_RUN_DIM,3);assert(wf_runtime_observe(&r,&o));
    o=observation(3100,WF_RUN_OFF,4);assert(wf_runtime_observe(&r,&o));
    uint64_t long_sleep=UINT64_C(5000000000);
    o=observation(long_sleep,WF_RUN_AOD,4);assert(wf_runtime_observe(&r,&o));
    o=observation(long_sleep+60000,WF_RUN_ON,5);assert(wf_runtime_observe(&r,&o));
    assert(r.summary.scene_ms[WF_RUN_UNKNOWN]==100 && r.summary.scene_ms[WF_RUN_ON]==1000);
    assert(r.summary.scene_ms[WF_RUN_FLASHLIGHT]==1000 && r.summary.scene_ms[WF_RUN_DIM]==1000);
    assert(r.summary.scene_ms[WF_RUN_OFF]==long_sleep-3100 && r.summary.scene_ms[WF_RUN_AOD]==60000);
    assert(r.summary.uptime_ms==long_sleep+60000 && r.summary.total_ms==r.summary.uptime_ms);
    o.ms=long_sleep+60001;o.utc_valid=true;o.utc=1793300000;o.clock_revision=1;
    o.fault_known=3;o.fault_mask=1;o.io_known=true;o.io_retries=5;o.io_recoveries=4;assert(wf_runtime_observe(&r,&o));
    assert(r.summary.faults==1 && !r.summary.recoveries);
    o.ms++;o.fault_known=0;assert(wf_runtime_observe(&r,&o));assert(r.summary.faults==1 && r.summary.fault_mask==1);
    assert(r.summary.io_retries==5 && r.summary.io_recoveries==4);
    o.fault_known=3;o.io_retries=7;o.io_recoveries=6;
    o.ms++;o.fault_mask=0;o.utc=1793290000;o.clock_revision=2;assert(wf_runtime_observe(&r,&o));
    assert(r.summary.recoveries==1 && r.summary.clock_revision==2);
    assert(r.summary.io_retries==7 && r.summary.io_recoveries==6);
    uint64_t total=r.summary.total_ms;
    o.ms--;assert(!wf_runtime_observe(&r,&o));assert(r.summary.total_ms==total && r.summary.clock_errors==1);
    o.scene=(wf_runtime_scene_t)-1;assert(!wf_runtime_observe(&r,&o));
    wf_runtime_control(&r,long_sleep+70000,false);total=r.summary.total_ms;
    assert(!r.summary.utc && !(r.summary.flags&WF_RUN_UTC_VALID));
    assert(!r.summary.events[r.summary.event_count-1].utc && !r.summary.events[r.summary.event_count-1].flags);
    o=observation(long_sleep+90000,WF_RUN_ON,99);assert(wf_runtime_observe(&r,&o));assert(r.summary.total_ms==total);
    wf_runtime_control(&r,long_sleep+100000,true);
    for(unsigned i=0;i<100;++i) { o=observation(long_sleep+100000+i*1000,i%2?WF_RUN_ON:WF_RUN_OFF,i+100);assert(wf_runtime_observe(&r,&o)); }
    assert(r.sample_count==64 && r.summary.sample_drops>0 && r.summary.event_count==8 && r.summary.event_drops>0);
    wf_runtime_sample_t sample;assert(wf_runtime_sample_read(&r,0,&sample) && sample.sequence==136);
    assert(wf_runtime_sample_read(&r,63,&sample) && sample.sequence==199 && !wf_runtime_sample_read(&r,64,&sample));
    wf_runtime_save_result(&r,r.summary.uptime_ms,true,true);
    assert(r.summary.successes==1 && r.summary.periodic_saves==1 && r.summary.saved_uptime_ms==r.summary.uptime_ms);
    uint8_t a[512],b[512],check[512];wf_runtime_summary_t decoded;
    r.summary.generation=UINT32_MAX;assert(wf_runtime_encode(&r.summary,a));
    assert(!memcmp(a,"WFRN",4) && a[4]==1 && a[6]==0 && a[7]==2);
    assert(wf_runtime_decode(&decoded,a,512) && wf_runtime_encode(&decoded,check) && !memcmp(check,a,512));
    for(unsigned i=0;i<512;++i) { memcpy(b,a,512);b[i]^=1;assert(!wf_runtime_decode(&decoded,b,512)); }
    const unsigned bad_offsets[]={96+27,96+28,96+29,96+31,248,240,256+28,256+30,256+31,48,232};
    const uint8_t bad_values[]={255,6,1,1,1,9,255,6,128,255,255};
    for(unsigned i=0;i<sizeof bad_offsets/sizeof bad_offsets[0];++i) {
        memcpy(b,a,512);b[bad_offsets[i]]=bad_values[i];reseal(b);assert(!wf_runtime_decode(&decoded,b,512));
    }
    memcpy(b,a,512);b[96+22]|=WF_RUN_BRIGHTNESS_VALID;b[96+25]=101;reseal(b);assert(!wf_runtime_decode(&decoded,b,512));
    memcpy(b,a,512);memset(b+256+20,0,4);reseal(b);assert(!wf_runtime_decode(&decoded,b,512));
    for(unsigned i=0;i<512;++i)assert(!wf_runtime_decode(&decoded,a,i));
    assert(!wf_runtime_decode(&decoded,a,513));
    r.summary.generation=0;assert(wf_runtime_encode(&r.summary,b));
    assert(wf_runtime_select(&decoded,a,512,b,512)==1 && decoded.generation==0);
    b[100]^=1;assert(wf_runtime_select(&decoded,a,512,b,512)==0);
    assert(wf_runtime_select(&decoded,NULL,0,NULL,0)==-1);
    r.summary.generation=0x7fffffff;assert(wf_runtime_encode(&r.summary,b));assert(wf_runtime_select(&decoded,a,512,b,512)==-1);
    r.summary.generation=UINT32_MAX;r.summary.segment++;assert(wf_runtime_encode(&r.summary,b));assert(wf_runtime_select(&decoded,a,512,b,512)==-1);
    assert(wf_runtime_decode(&decoded,a,512));total=decoded.total_ms;
    wf_runtime_boot(&r,&decoded,900,0,7);
    assert(r.summary.boot==2 && r.summary.total_ms==total && !r.summary.uptime_ms && !r.summary.saved_uptime_ms);
    o=observation(1900,WF_RUN_ON,1);assert(wf_runtime_observe(&r,&o));assert(r.summary.total_ms==total+1000);
    wf_runtime_save_result(&r,0,false,false);assert(r.summary.failures==1);
    assert(wf_runtime_encode(&r.summary,b));
    printf("runtime model bytes=%zu summary=%zu: durations, long sleep, pause, clocks, rings, codec and recovery passed\n",sizeof r,sizeof r.summary);
    return 0;
}
