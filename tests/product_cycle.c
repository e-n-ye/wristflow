#define WF_CYCLE_LOG
#include "../apps/product/src/product_cycle.c"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static uint64_t time_ms;
static bool commit_fail,append_fail,close_fail;
static unsigned appended,committed,closes,reloads;
static uint8_t rows[40000][64];
static wf_fs_block_t fake_device;
uint64_t wristflow_product_services_monotonic(void) {return time_ms;}
wristflow_alarm_clock_t wristflow_product_services_clock_cache(void)
{return (wristflow_alarm_clock_t){.valid=true,.utc=1793530000u+(uint32_t)(time_ms/1000),.revision=1};}
void wristflow_product_event_send(rt_uint32_t events) {(void)events;}
int rt_mutex_init(struct rt_mutex *m,const char *n,unsigned f) {(void)n;(void)f;m->held=0;return 0;}
int rt_mutex_take(struct rt_mutex *m,int t) {(void)t;assert(!m->held);m->held=1;return 0;}
int rt_mutex_release(struct rt_mutex *m) {assert(m->held);m->held=0;return 0;}
int rt_event_init(struct rt_event *e,const char *n,unsigned f) {(void)n;(void)f;e->bits=0;return 0;}
int rt_event_send(struct rt_event *e,unsigned b) {e->bits|=b;return 0;}
int rt_event_recv(struct rt_event *e,unsigned b,unsigned f,int t,rt_uint32_t *out) {(void)b;(void)f;(void)t;*out=e->bits;e->bits=0;return 0;}
rt_tick_t rt_tick_from_millisecond(unsigned n) {return n;}
static struct rt_thread thread;
rt_thread_t rt_thread_create(const char *n,void (*f)(void *),void *c,unsigned s,unsigned p,unsigned t)
{(void)n;(void)s;(void)p;(void)t;thread.entry=f;thread.context=c;return &thread;}
int rt_thread_startup(rt_thread_t t) {assert(t==&thread);return 0;}
void rt_kprintf(const char *fmt,...) {(void)fmt;}
int wf_product_fs_block_prepare(void) {return 0;}
bool wf_product_fs_block_admit(void) {return true;}
wf_fs_block_t *wf_product_fs_block_device(void) {return &fake_device;}
int wf_product_fs_block_read(uint32_t b,uint32_t o,void *p,size_t n) {(void)b;(void)o;memset(p,255,n);return 0;}
int wf_product_fs_block_erase(uint32_t b) {(void)b;return 0;}
int wf_product_fs_block_prog(uint32_t b,uint32_t o,const void *p,size_t n) {(void)b;(void)o;(void)p;(void)n;return 0;}
void wf_cycle_store_init(wf_cycle_store_t *s,wf_fs_block_t *d) {memset(s,0,sizeof *s);s->cfg.context=d;}
int wf_cycle_store_mount(wf_cycle_store_t *s,bool format) {(void)format;s->mounted=true;return 0;}
int wf_cycle_store_create(wf_cycle_store_t *s,bool is_smoke,uint32_t id)
{s->opened=true;s->session=id;s->smoke=is_smoke;s->rows=0;appended=committed=0;return 0;}
int wf_cycle_store_append(wf_cycle_store_t *s,const uint8_t row[64])
{if(append_fail)return -1;assert(s->opened);memcpy(rows[appended++],row,64);s->rows=appended;return 0;}
int wf_cycle_store_commit(wf_cycle_store_t *s) {(void)s;if(commit_fail)return -1;committed=appended;return 0;}
int wf_cycle_store_close(wf_cycle_store_t *s) {++closes;s->opened=false;return close_fail ? -1:0;}
int wf_cycle_store_reload(wf_cycle_store_t *s) {++reloads;assert(!s->opened);return 0;}
int wf_cycle_store_info(wf_cycle_store_t *s,bool is_smoke,wf_cycle_info_t *info)
{
    (void)is_smoke;if(s->opened || !committed)return -1;
    *info=(wf_cycle_info_t){.session=s->session,.rows=committed,.bytes=committed*64,.terminal=rows[committed-1][5]==WF_CYCLE_END};return 0;
}
int wf_cycle_store_read(wf_cycle_store_t *s,bool k,uint32_t id,uint32_t index,uint8_t row[64])
{(void)s;(void)k;(void)id;if(index>=committed)return -1;memcpy(row,rows[index],64);return 0;}
static wristflow_battery_record_t observation(void)
{
    return (wristflow_battery_record_t){.observed_ms=(uint32_t)time_ms,.adc_valid=true,
        .power={.input_valid=true,.voltage_valid=true,.millivolts=4100,.voltage_sequence=(uint32_t)(time_ms/5000)+1,
        .voltage_sampled_ms=(uint32_t)time_ms,.charge=WF_POWER_CHARGE_STOPPED,.estimate_valid=true,.estimated_percent=96},
        .diagnostic={.filtered_decimillivolts=41000,.filter_valid=true,.candidate=96}};
}
static void reset(void)
{
    initialized=prepared=probe_passed=full_seen=inhibit=prefix_verified=command_pending=false;
    state=reason=session=next_row=persisted=dropped=last_adc=last_adc_ms=seconds=head=count=0;
    started=last_row_at=last_observed=armed_at=full_at=0;
    store=(wf_cycle_store_t){.mounted=true};
    time_ms=1000;commit_fail=append_fail=close_fail=false;appended=committed=closes=reloads=0;
    assert(wf_product_cycle_start());wristflow_battery_record_t o=observation();wf_product_cycle_observe(&o,false);
}
int main(void)
{
    reset();assert(!execute(CMD_SMOKE,123,10,0,true));process_once();assert(state==RUNNING && persisted==1);
    time_ms+=5000;wristflow_battery_record_t o=observation();wf_product_cycle_observe(&o,false);process_once();
    time_ms+=5000;o=observation();wf_product_cycle_observe(&o,false);process_once();
    assert(state==DONE && persisted==3 && reason==WF_CYCLE_LIMIT && closes==1 && reloads==1 && prefix_verified);
    reset();assert(execute(CMD_ARM,123,0,0,false)<0); /* Requires recently observed FULL. */
    o=observation();o.power.charge=WF_POWER_CHARGE_FULL;o.power.input_good=true;wf_product_cycle_observe(&o,false);
    assert(!execute(CMD_ARM,123,0,0,false));process_once();assert(state==ARMED);
    time_ms+=5000;o=observation();wf_product_cycle_observe(&o,true);process_once();assert(state==RUNNING);
    time_ms+=5000;o=observation();o.power.millivolts=3600;wf_product_cycle_observe(&o,true);assert(inhibit);process_once();
    assert(state==DONE && reason==WF_CYCLE_LOW && persisted==3 && prefix_verified);
    /* Final sync failure always closes and revalidates the committed prefix. */
    reset();assert(!execute(CMD_SMOKE,124,10,0,true));process_once();commit_fail=true;
    time_ms+=10000;o=observation();wf_product_cycle_observe(&o,true);process_once();
    assert(state==FAILED && !store.opened && closes==1 && reloads==1 && persisted==1 && prefix_verified && inhibit);
    /* UI already cut the load before final persistence fails: never unlatch
     * just because the next observation is no longer a 100-brightness load. */
    reset();assert(!execute(CMD_SMOKE,128,10,0,true));process_once();commit_fail=true;
    time_ms+=10000;o=observation();wf_product_cycle_observe(&o,true);assert(inhibit);
    o=observation();wf_product_cycle_observe(&o,false);process_once();assert(inhibit && state==FAILED);
    reset();assert(!execute(CMD_SMOKE,125,10,0,true));process_once();append_fail=true;
    time_ms+=5000;o=observation();wf_product_cycle_observe(&o,true);process_once();
    assert(state==FAILED && !store.opened && closes==1 && persisted==1 && dropped);
    /* Reserve a queue slot for END rather than silently losing old accepted rows. */
    reset();assert(!execute(CMD_SMOKE,126,120,0,true));
    for(unsigned i=0;i<16;++i) {time_ms+=5000;o=observation();wf_product_cycle_observe(&o,true);}
    assert(state==FINISHING && reason==WF_CYCLE_QUEUE && count==16);process_once();
    assert(state==DONE && reason==WF_CYCLE_QUEUE && !store.opened && persisted==16);
    reset();assert(!execute(CMD_SMOKE,127,120,0,true));process_once();time_ms+=15001;process_once();
    assert(state==DONE && reason==WF_CYCLE_STALE);
    wf_cycle_row_t stale_row;assert(wf_cycle_decode(rows[committed-1],&stale_row));
    assert(stale_row.observation.observed_ms-stale_row.observation.power.voltage_sampled_ms>=15001);
    puts("product_cycle: actual worker state/queue/FULL-arm/raw-low/ADC-stale/final-sync/append-failure and exportable-prefix cleanup passed");
    return 0;
}
