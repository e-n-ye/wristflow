#define WF_CYCLE_LOG
#include "../apps/product/src/product_cycle.c"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static uint64_t time_ms;
static bool commit_fail,append_fail,close_fail;
static bool wrong_named_identity;
static uint32_t file_session;
static unsigned named_infos;
static unsigned appended,committed,closes,reloads;
static unsigned irq,power_requests;
static bool experiment_permission;
static wristflow_power_snapshot_t background_power;
rt_tick_t rt_tick_get(void) {return (rt_tick_t)time_ms;}
rt_base_t rt_hw_interrupt_disable(void) {return irq++;}
void rt_hw_interrupt_enable(rt_base_t prior) {assert(irq);irq=(unsigned)prior;}
void wristflow_product_power_experiment(bool active) {assert(!irq && !lock.held);experiment_permission=active;}
void wristflow_product_power_request(void) {assert(!irq && !lock.held);++power_requests;}
wristflow_power_snapshot_t wristflow_product_power_snapshot(void) {return background_power;}
void wristflow_product_charger_display(wristflow_power_snapshot_t *p) {(void)p;}
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
{s->opened=true;s->session=id;s->smoke=is_smoke;s->rows=0;file_session=id;appended=committed=0;return 0;}
int wf_cycle_store_create_session(wf_cycle_store_t *s,uint32_t id)
{return wf_cycle_store_create(s,false,id);}
int wf_cycle_store_append(wf_cycle_store_t *s,const uint8_t row[64])
{if(append_fail)return -1;assert(s->opened);memcpy(rows[appended++],row,64);s->rows=appended;return 0;}
int wf_cycle_store_commit(wf_cycle_store_t *s) {(void)s;if(commit_fail)return -1;committed=appended;return 0;}
int wf_cycle_store_close(wf_cycle_store_t *s) {++closes;s->opened=false;return close_fail ? -1:0;}
int wf_cycle_store_reload(wf_cycle_store_t *s) {++reloads;assert(!s->opened);s->session=0;return 0;}
int wf_cycle_store_info(wf_cycle_store_t *s,bool is_smoke,wf_cycle_info_t *info)
{
    (void)is_smoke;if(s->opened || !committed)return -1;
    *info=(wf_cycle_info_t){.session=file_session,.rows=committed,.bytes=committed*64,.terminal=rows[committed-1][5]==WF_CYCLE_END};return 0;
}
int wf_cycle_store_info_session(wf_cycle_store_t *s,uint32_t id,wf_cycle_info_t *info)
{
    ++named_infos;assert(id==file_session);int result=wf_cycle_store_info(s,false,info);
    if(!result && wrong_named_identity)info->session=id+1;
    return result;
}
int wf_cycle_store_read(wf_cycle_store_t *s,bool k,uint32_t id,uint32_t index,uint8_t row[64])
{(void)s;(void)k;(void)id;if(index>=committed)return -1;memcpy(row,rows[index],64);return 0;}
int wf_cycle_store_read_session(wf_cycle_store_t *s,uint32_t id,uint32_t index,uint8_t row[64])
{return wf_cycle_store_read(s,false,id,index,row);}
int wf_cycle_store_prefix_session(wf_cycle_store_t *s,uint32_t id,wf_cycle_info_t *info,uint32_t *bytes,bool *damaged)
{int result=wf_cycle_store_info_session(s,id,info);*bytes=info->bytes;*damaged=false;return result;}
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
    wrong_named_identity=false;file_session=0;named_infos=0;
    low_mode=signals_enabled=false;keep_remaining=signal_head=signal_count=signal_lost=0;
    last_low_sample=last_low_request=last_commit=latest_at=0;
    low_samples=low_off_samples=low_events=0;
    irq=power_requests=0;experiment_permission=false;background_power=(wristflow_power_snapshot_t){0};
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
    assert(state==DONE && reason==WF_CYCLE_LOW && persisted==3 && prefix_verified && named_infos==1);
    /* USB charger qualification remains available with an invalid OFF ADC/PG cache. */
    o=(wristflow_battery_record_t){0};wf_product_cycle_observe(&o,false);assert(inhibit);
    o.power.input_observation=(wristflow_input_observation_t){.valid=true,.good=false};
    wf_product_cycle_observe(&o,false);assert(inhibit);
    o.power.input_observation.good=true;wf_product_cycle_observe(&o,false);assert(!inhibit);
    /* Named recovery must retain the requested identity across store reload. */
    reset();o=observation();o.power.charge=WF_POWER_CHARGE_FULL;wf_product_cycle_observe(&o,false);
    assert(!execute(CMD_ARM,129,0,0,false));process_once();
    time_ms+=5000;o=observation();wf_product_cycle_observe(&o,true);process_once();
    wrong_named_identity=true;time_ms+=5000;o=observation();o.power.millivolts=3600;
    wf_product_cycle_observe(&o,true);process_once();assert(state==FAILED && !prefix_verified && named_infos==1);
    for(unsigned fault=0;fault<2;++fault) {
        reset();o=observation();o.power.charge=WF_POWER_CHARGE_FULL;wf_product_cycle_observe(&o,false);
        assert(!execute(CMD_ARM,130+fault,0,0,false));process_once();
        if(fault)append_fail=true;
        time_ms+=5000;o=observation();wf_product_cycle_observe(&o,true);process_once();
        if(!fault) {
            commit_fail=true;time_ms+=5000;o=observation();o.power.millivolts=3600;
            wf_product_cycle_observe(&o,true);process_once();
        }
        assert(state==FAILED && !store.opened && persisted==1 && prefix_verified && named_infos==1);
        assert(closes==1 && reloads==1 && inhibit);
    }
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
    /* Low experiment: no FULL requirement, no raw-low/LOAD/STALE guard,
     * each sample commits, repeated20min holds stay within one identity. */
    reset();assert(execute(CMD_LOW,301,0,0,false)<0);
    o=observation();o.power.millivolts=3700;o.power.input_observation=(wristflow_input_observation_t){.valid=true,.good=true};
    wf_product_cycle_context(&o,false,0);assert(!execute(CMD_LOW,301,0,0,false));process_once();
    assert(state==ARMED && experiment_permission && committed==1 && rows[0][4]==2);
    for(unsigned i=0;i<2;++i) {
        time_ms+=5000;o=observation();o.power.input_observation=(wristflow_input_observation_t){.valid=true,.good=true};
        o.context.phase=WRISTFLOW_DISPLAY_OFF;o.context.flags=WF_BATTERY_UI_VALID;
        wf_product_cycle_context(&o,false,0);process_once();
    }
    assert(state==ARMED && low_off_samples==2 && committed==appended);
    time_ms+=5000;o=observation();o.power.millivolts=3400;o.power.input_observation.valid=true;
    o.context.surface=5;wf_product_cycle_context(&o,true,0);process_once();
    assert(state==RUNNING && !inhibit && persisted>=3 && committed==appended);
    time_ms+=5000;o=observation();o.power.millivolts=3350;o.power.input_observation.valid=true;
    o.context.surface=1;o.context.flags=WF_BATTERY_UI_VALID;
    wf_product_cycle_context(&o,false,1200000);process_once();assert(state==RUNNING && !inhibit);
    wf_product_cycle_signal(WF_CYCLE_KEY_DOWN);wf_product_cycle_signal(WF_CYCLE_KEY_UP);process_once();
    wf_cycle_row_t event;assert(wf_cycle_decode(rows[committed-1],&event));
    assert(event.low_voltage && event.kind==WF_CYCLE_EVENT && event.event==WF_CYCLE_KEY_UP && !event.keep_remaining_ms);
    assert(event.event_tick_ms==(uint32_t)time_ms && !(event.observation.context.flags&WF_BATTERY_UI_VALID));
    unsigned prior_rows=committed;
    time_ms+=1200000;o=observation();o.power.millivolts=3290;o.power.input_observation.valid=true;
    wf_product_cycle_context(&o,false,0);process_once();assert(state==RUNNING && committed>prior_rows);
    time_ms+=1000;wf_product_cycle_context(&o,false,1200000);process_once();assert(state==RUNNING);
    /* UI can stop consuming events while the independent worker still saves
     * fresh OFF ADC, marking unavailable UI/estimator fields as invalid. */
    time_ms+=20000;background_power=observation().power;background_power.millivolts=3250;
    background_power.input_observation.valid=true;process_once();
    assert(wf_cycle_decode(rows[committed-1],&event) && event.kind==WF_CYCLE_SAMPLE);
    assert(event.observation.power.millivolts==3250 && !event.observation.power.estimate_valid);
    assert(!(event.observation.context.flags&WF_BATTERY_UI_VALID) && state==RUNNING && !inhibit);
    prior_rows=committed;time_ms+=30000;process_once();assert(state==RUNNING && committed==prior_rows && experiment_permission);
    wf_product_cycle_signal(WF_CYCLE_KEY_DOWN);uint32_t final_key_tick=(uint32_t)time_ms;
    time_ms+=5;background_power.input_observation.good=true;process_once();
    assert(state==DONE && reason==WF_CYCLE_USB && !inhibit && !experiment_permission);
    assert(wf_cycle_decode(rows[committed-2],&event) && event.event==WF_CYCLE_KEY_DOWN && event.event_tick_ms==final_key_tick);
    assert(committed==appended && prefix_verified && !irq);
    reset();o=observation();o.power.input_observation=(wristflow_input_observation_t){.valid=true,.good=true};
    wf_product_cycle_context(&o,false,0);assert(!execute(CMD_LOW,302,0,0,false));process_once();
    append_fail=true;time_ms+=5000;o=observation();o.power.input_observation.valid=true;
    wf_product_cycle_context(&o,true,0);process_once();assert(state==FAILED && !inhibit && !experiment_permission);
    /* Callback completion jitter cannot discard a new experiment ADC. */
    reset();o=observation();o.power.input_observation=(wristflow_input_observation_t){.valid=true,.good=true};
    wf_product_cycle_context(&o,false,0);assert(!execute(CMD_LOW,303,0,0,false));process_once();
    time_ms=5001;o=observation();o.power.voltage_sequence=10;o.power.input_observation.valid=true;
    wf_product_cycle_context(&o,true,0);process_once();unsigned first_count=low_samples;
    time_ms=10000;o=observation();o.power.voltage_sequence=11;o.power.input_observation.valid=true;
    wf_product_cycle_context(&o,true,0);process_once();assert(low_samples==first_count+1);
    /* With a stalled UI, independent PG can start the BAT stage and a3-second
     * fallback never claims the last UI state is a current measurement. */
    reset();o=observation();o.power.input_observation=(wristflow_input_observation_t){.valid=true,.good=true};
    o.context.flags=WF_BATTERY_UI_VALID|WF_BATTERY_BRIGHTNESS_VALID;
    wf_product_cycle_context(&o,false,1200000);assert(!execute(CMD_LOW,304,0,0,false));process_once();
    time_ms+=3000;background_power=observation().power;background_power.voltage_sequence=99;background_power.input_observation.valid=true;
    process_once();assert(state==RUNNING && low_samples==1 && !inhibit);
    assert(wf_cycle_decode(rows[committed-1],&event) && !(event.observation.context.flags&WF_BATTERY_UI_VALID));
    puts("product_cycle: actual worker state/queue/FULL-arm/raw-low/ADC-stale/final-sync/append-failure and exportable-prefix cleanup passed");
    return 0;
}
