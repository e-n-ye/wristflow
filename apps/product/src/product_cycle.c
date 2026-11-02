#ifdef WF_CYCLE_LOG
#include "product_cycle.h"
#include "product_fs_block.h"
#include "product_services.h"
#include "product_pm.h"
#include "product_power.h"
#include "product_charger.h"
#include "display_policy.h"
#include "cycle_store.h"
#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <rthw.h>
enum { IDLE,READY,ARMED,RUNNING,FINISHING,DONE,FAILED };
enum { CMD_PROBE=1,CMD_INIT,CMD_MOUNT,CMD_SMOKE,CMD_ARM,CMD_STOP,CMD_INFO,CMD_PAGE,CMD_LOW,CMD_PREFIX };
static struct rt_mutex lock;
static struct rt_event changed;
static bool initialized,prepared,probe_passed,full_seen,inhibit;
static bool prefix_verified;
static unsigned state,reason;
static uint32_t session,next_row,persisted,dropped,last_adc,last_adc_ms,seconds;
static uint64_t started,last_row_at,last_observed,latest_at,armed_at,full_at;
static wristflow_battery_record_t latest;
static bool last_flashlight,smoke;
static bool low_mode;
static uint32_t keep_remaining;
static uint64_t last_low_sample,last_low_request,last_commit;
static uint32_t low_samples,low_off_samples,low_events;
static bool signals_enabled;
static struct {uint8_t code;uint32_t tick_ms;} signals[8];
static unsigned signal_head,signal_count,signal_lost;
static uint8_t queue[16][64];static unsigned head,count;
static wf_cycle_store_t store;
static struct {unsigned cmd;uint32_t a,b,n;bool smoke;} request;
static bool command_pending;
static void take(void) {rt_mutex_take(&lock,RT_WAITING_FOREVER);}
static void give(void) {rt_mutex_release(&lock);}
static uint64_t clock_ms(void) {return wristflow_product_services_monotonic();}
static void enqueue_event(unsigned kind,unsigned why,unsigned event,uint32_t event_tick,uint64_t now)
{
    if(count==16 || next_row>=WF_CYCLE_MAX_ROWS) {++dropped;state=FAILED;reason=WF_CYCLE_QUEUE;if(!low_mode)inhibit=true;return;}
    wristflow_alarm_clock_t wall=wristflow_product_services_clock_cache();
    wf_cycle_row_t row={.session=session,.sequence=next_row,.elapsed=now-started,
        .utc=wall.utc,.revision=wall.revision,.utc_valid=wall.valid,.kind=(uint8_t)kind,.reason=(uint8_t)why,
        .flashlight=last_flashlight,.observation=latest,.low_voltage=low_mode,
        .event=(uint8_t)event,.event_tick_ms=event_tick,
        .keep_remaining_ms=now-last_observed<keep_remaining ? keep_remaining-(uint32_t)(now-last_observed):0};
    row.observation.observed_ms+=(uint32_t)(now-latest_at);
    if(low_mode && now-last_observed>15000)row.observation.context.flags&=~(WF_BATTERY_UI_VALID|WF_BATTERY_BRIGHTNESS_VALID);
    if(kind==WF_CYCLE_EVENT && event!=WF_CYCLE_CONTEXT) {
        row.observation.context.flags&=~(WF_BATTERY_UI_VALID|WF_BATTERY_BRIGHTNESS_VALID);
        row.observation.adc_valid=false;row.observation.power.voltage_valid=false;
        row.observation.power.estimate_valid=false;row.flashlight=false;row.keep_remaining_ms=0;
    }
    if(!wf_cycle_encode(&row,queue[(head+count)%16])) {state=FAILED;reason=WF_CYCLE_STORAGE;if(!low_mode)inhibit=true;return;}
    ++next_row;++count;last_row_at=now;
    if(low_mode && kind==WF_CYCLE_SAMPLE) {
        ++low_samples;
        if((row.observation.context.flags&WF_BATTERY_UI_VALID) &&
            row.observation.context.phase==WRISTFLOW_DISPLAY_OFF && !(row.observation.context.flags&WF_BATTERY_LCD_ON))++low_off_samples;
    }
    if(low_mode && kind==WF_CYCLE_EVENT)++low_events;
}
static void enqueue(unsigned kind,unsigned why,uint64_t now) {enqueue_event(kind,why,0,0,now);}
static unsigned flush_signals(uint64_t now,bool finishing)
{
    uint8_t incoming[8];uint32_t ticks[8];
    rt_base_t level=rt_hw_interrupt_disable();
    unsigned n=signal_count,lost=signal_lost;
    for(unsigned i=0;i<n;++i) {incoming[i]=signals[(signal_head+i)%8].code;ticks[i]=signals[(signal_head+i)%8].tick_ms;}
    signal_head=signal_count=signal_lost=0;if(finishing)signals_enabled=false;
    rt_hw_interrupt_enable(level);
    if(!low_mode || (state!=ARMED && state!=RUNNING))return 0;
    for(unsigned i=0;i<n;++i) {
        if(count>=15 || next_row>=WF_CYCLE_MAX_ROWS-1) {dropped+=n-i+lost;return count>=15 ? WF_CYCLE_QUEUE:WF_CYCLE_LIMIT;}
        enqueue_event(WF_CYCLE_EVENT,0,incoming[i],ticks[i],now);
    }
    dropped+=lost;return lost ? WF_CYCLE_QUEUE:0;
}
static void end_session(unsigned why,uint64_t now)
{
    if(state!=RUNNING && state!=ARMED)return;
    unsigned pending=flush_signals(now,true);if(pending)why=pending;
    reason=why;inhibit=inhibit || (!low_mode && last_flashlight);
    enqueue(WF_CYCLE_END,why,now);
    state=state==FAILED ? FAILED:FINISHING;
    rt_event_send(&changed,1);wristflow_product_event_send(WF_EVENT_POWER);
}
void wf_product_cycle_observe(const wristflow_battery_record_t *record,bool flashlight)
{wf_product_cycle_context(record,flashlight,0);}
static bool fresh(const wristflow_battery_record_t *record)
{return record->adc_valid && record->power.voltage_valid && record->power.voltage_sequence &&
    record->observed_ms-record->power.voltage_sampled_ms<=15000;}
void wf_product_cycle_context(const wristflow_battery_record_t *record,bool flashlight,uint32_t keep_ms)
{
    if(!initialized || !record)return;
    take();uint64_t now=clock_ms();
    bool context_changed=latest.context.surface!=record->context.surface || latest.context.phase!=record->context.phase ||
        latest.context.flags!=record->context.flags || latest.context.requested_brightness!=record->context.requested_brightness ||
        latest.context.acknowledged_brightness!=record->context.acknowledged_brightness || last_flashlight!=flashlight ||
        ((!keep_remaining)!=(!keep_ms)) || keep_ms>keep_remaining+1000;
    latest=*record;last_flashlight=flashlight;last_observed=latest_at=now;keep_remaining=keep_ms;
    if(record->power.charge==WF_POWER_CHARGE_FULL) {full_seen=true;full_at=now;}
    /* Independent charger PG survives OFF. The ordinary power cache does not. */
    if(inhibit && record->power.input_observation.valid && record->power.input_observation.good)inhibit=false;
    if(low_mode && (state==ARMED || state==RUNNING)) {
        if(state==ARMED && record->power.input_observation.valid && !record->power.input_observation.good) {
            state=RUNNING;context_changed=true;
        }
        if(state==RUNNING || state==ARMED) {
            if(state==RUNNING && record->power.input_observation.valid && record->power.input_observation.good)end_session(WF_CYCLE_USB,now);
            else if(now-started>=WF_CYCLE_MAX_MS)end_session(WF_CYCLE_LIMIT,now);
            else if(count>=14 || next_row>=WF_CYCLE_MAX_ROWS-2)end_session(count>=14 ? WF_CYCLE_QUEUE:WF_CYCLE_LIMIT,now);
            else {
                if(context_changed)enqueue_event(WF_CYCLE_EVENT,0,WF_CYCLE_CONTEXT,(uint32_t)((uint64_t)rt_tick_get()*1000U/RT_TICK_PER_SECOND),now);
                if(fresh(record) &&
                    (record->power.voltage_sequence!=last_adc || record->power.voltage_sampled_ms!=last_adc_ms)) {
                    last_adc=record->power.voltage_sequence;last_adc_ms=record->power.voltage_sampled_ms;
                    last_low_sample=now;enqueue(WF_CYCLE_SAMPLE,0,now);
                }
                if(count)rt_event_send(&changed,1);
            }
        }
        give();return;
    }
    if(state==ARMED && record->power.input_valid && !record->power.input_good && flashlight &&
        !wf_cycle_stop_reason(record,flashlight,0,false)) {
        state=RUNNING;last_adc=record->power.voltage_sequence;last_adc_ms=record->power.voltage_sampled_ms;
        enqueue(WF_CYCLE_SAMPLE,0,now);rt_event_send(&changed,1);
    } else if(state==RUNNING) {
        unsigned why=wf_cycle_stop_reason(record,flashlight,now-started,smoke);
        if(smoke && now-started>=(uint64_t)seconds*1000)why=WF_CYCLE_LIMIT;
        if(why)end_session(why,now);
        else if((record->power.voltage_sequence!=last_adc || record->power.voltage_sampled_ms!=last_adc_ms) &&
                now-last_row_at>=5000) {
            if(count>=15)end_session(WF_CYCLE_QUEUE,now); /* Reserve terminal queue slot. */
            else if(next_row>=WF_CYCLE_MAX_ROWS-1)end_session(WF_CYCLE_LIMIT,now);
            else {
                last_adc=record->power.voltage_sequence;last_adc_ms=record->power.voltage_sampled_ms;
                enqueue(WF_CYCLE_SAMPLE,0,now);rt_event_send(&changed,1);
            }
        }
    }
    give();
}
void wf_product_cycle_signal(unsigned event)
{
    if(event<WF_CYCLE_KEY_DOWN || event>WF_CYCLE_LCD_OFF_DONE)return;
    rt_base_t level=rt_hw_interrupt_disable();
    if(signals_enabled) {
        if(signal_count<8) {
            unsigned i=(signal_head+signal_count++)%8;signals[i].code=(uint8_t)event;
            signals[i].tick_ms=(uint32_t)((uint64_t)rt_tick_get()*1000U/RT_TICK_PER_SECOND);
        }
        else ++signal_lost;
    }
    bool notify=signals_enabled;rt_hw_interrupt_enable(level);
    if(notify)rt_event_send(&changed,1);
}
bool wf_product_cycle_inhibit(void)
{
    if(!initialized)return false;
    take();bool value=inhibit;give();return value;
}
bool wf_product_cycle_sampling(void)
{
    if(!initialized)return false;
    take();bool value=low_mode && (state==ARMED || state==RUNNING);give();return value;
}
static int prepare(void)
{
    if(prepared)return 0;
    int result=wf_product_fs_block_prepare();if(result)return result;
    if(!wf_product_fs_block_admit())return -1;
    wf_cycle_store_init(&store,wf_product_fs_block_device());prepared=true;return 0;
}
static int probe(void)
{
    int result=prepare();if(result)return result;
    uint8_t bytes[256],pattern[256];
    for(unsigned o=0;o<4096;o+=256) {
        if(wf_product_fs_block_read(0,o,bytes,256))return -1;
        for(unsigned i=0;i<256;++i)if(bytes[i]!=255)return -2;
    }
    rt_kprintf("[cycle-probe] stage=erase-enter\n");
    result=wf_product_fs_block_erase(0);rt_kprintf("[cycle-probe] stage=erase-return result=%d\n",result);if(result)return result;
    for(unsigned i=0;i<256;++i)pattern[i]=(uint8_t)(i^0x5a);
    rt_kprintf("[cycle-probe] stage=program-enter\n");
    result=wf_product_fs_block_prog(0,0,pattern,256);rt_kprintf("[cycle-probe] stage=program-return result=%d\n",result);if(result)return result;
    if(wf_product_fs_block_read(0,0,bytes,256) || memcmp(bytes,pattern,256))return -3;
    rt_kprintf("[cycle-probe] stage=restore-enter\n");
    result=wf_product_fs_block_erase(0);rt_kprintf("[cycle-probe] stage=restore-return result=%d\n",result);
    probe_passed=result==0;return result;
}
static void status(void)
{
    take();rt_kprintf("[cycle-status] state=%u session=%lu accepted=%lu saved=%lu queued=%u dropped=%lu reason=%u inhibit=%u\n",
        state,(unsigned long)session,(unsigned long)next_row,(unsigned long)persisted,count,(unsigned long)dropped,reason,inhibit);
    rt_kprintf("[cycle-mode] low=%u commit_elapsed_ms=%lu keep_remaining_ms=%lu\n",low_mode,
        (unsigned long)(last_commit>=started ? last_commit-started:0),(unsigned long)keep_remaining);
    rt_kprintf("[cycle-low] samples=%lu off_samples=%lu events=%lu\n",(unsigned long)low_samples,(unsigned long)low_off_samples,(unsigned long)low_events);
    rt_kprintf("[cycle-prefix] verified=%u\n",prefix_verified);give();
}
static void print_row(uint32_t id,uint32_t index,const uint8_t row[64])
{
    static const char hex[]="0123456789abcdef";char text[65];
    for(unsigned part=0;part<2;++part) {
        for(unsigned i=0;i<32;++i) {uint8_t b=row[part*32+i];text[2*i]=hex[b>>4];text[2*i+1]=hex[b&15];}
        text[64]=0;rt_kprintf("[cycle-data] id=%lu i=%lu p=%u hex=%s\n",(unsigned long)id,(unsigned long)index,part,text);
    }
}
static int execute(unsigned cmd,uint32_t a,uint32_t b,uint32_t n,bool is_smoke)
{
    take();bool active=state==ARMED || state==RUNNING || state==FINISHING;give();
    if(cmd==CMD_STOP) {take();if(a!=session || !active) {give();return -2;}end_session(WF_CYCLE_MANUAL,clock_ms());give();return 0;}
    if(active)return -3;
    if(cmd==CMD_PROBE)return probe();
    if(cmd==CMD_INIT || cmd==CMD_MOUNT) {
        int result=prepare();if(result)return result;
        if(cmd==CMD_INIT && !probe_passed)return -4;
        result=wf_cycle_store_mount(&store,cmd==CMD_INIT);
        if(!result) {take();state=READY;give();}return result;
    }
    if(!store.mounted)return -5;
    if(cmd==CMD_SMOKE || cmd==CMD_ARM || cmd==CMD_LOW) {
        take();bool eligible=last_observed && clock_ms()-last_observed<15000 &&
            (cmd==CMD_LOW ? (fresh(&latest) && latest.power.input_observation.valid && latest.power.input_observation.good):
                (!wf_cycle_stop_reason(&latest,last_flashlight,0,true) &&
                (cmd==CMD_SMOKE || (full_seen && clock_ms()-full_at<300000))));
        give();if(!eligible)return -6;
        int result=cmd==CMD_SMOKE ? wf_cycle_store_create(&store,true,a):wf_cycle_store_create_session(&store,a);
        if(result)return result;
        take();smoke=cmd==CMD_SMOKE;low_mode=cmd==CMD_LOW;session=a;seconds=b;next_row=persisted=dropped=0;head=count=0;reason=0;
        last_low_sample=last_low_request=last_commit=0;
        low_samples=low_off_samples=low_events=0;
        rt_base_t level=rt_hw_interrupt_disable();signals_enabled=low_mode;signal_head=signal_count=signal_lost=0;rt_hw_interrupt_enable(level);
        prefix_verified=false;armed_at=started=clock_ms();state=smoke ? RUNNING:ARMED;
        if(low_mode)last_low_request=started;
        last_adc=latest.power.voltage_sequence;last_adc_ms=latest.power.voltage_sampled_ms;enqueue(WF_CYCLE_START,0,started);
        give();return 0;
    }
    if(cmd==CMD_INFO || cmd==CMD_PREFIX) {
        wf_cycle_info_t info;uint32_t file_bytes=0;bool damaged=false;
        int result=cmd==CMD_PREFIX ? wf_cycle_store_prefix_session(&store,a,&info,&file_bytes,&damaged):!is_smoke && a ? wf_cycle_store_info_session(&store,a,&info):
            wf_cycle_store_info(&store,is_smoke,&info);
        /* Explicit export alone may find a matching legacy identity. Recovery
         * below deliberately uses only the named file for a new formal run. */
        if(cmd==CMD_INFO && !is_smoke && a && result==LFS_ERR_NOENT) {
            result=wf_cycle_store_info(&store,false,&info);
            if(!result && info.session!=a)result=LFS_ERR_NOENT;
        }
        if(result)return result;
        if(cmd==CMD_PREFIX)rt_kprintf("[cycle-recovery] id=%lu file_bytes=%lu prefix_bytes=%lu damaged_tail=%u\n",
            (unsigned long)info.session,(unsigned long)file_bytes,(unsigned long)info.bytes,damaged);
        rt_kprintf("[cycle-info] id=%lu rows=%lu bytes=%lu crc=%08lx terminal=%u\n",
            (unsigned long)info.session,(unsigned long)info.rows,(unsigned long)info.bytes,(unsigned long)info.crc,info.terminal);return 0;
    }
    if(cmd==CMD_PAGE) {
        uint8_t row[64];wf_cycle_row_t decoded;
        /* Four rows per command: finite UART output and explicit end marker. */
        if(!n || n>4)return -9;
        for(unsigned i=0;i<n;++i) {
            int result=is_smoke ? wf_cycle_store_read(&store,true,a,b+i,row):
                wf_cycle_store_read_session(&store,a,b+i,row);
            if(!is_smoke && result==LFS_ERR_NOENT)result=wf_cycle_store_read(&store,false,a,b+i,row);
            if(result)return result;
            if(!wf_cycle_decode(row,&decoded))return -7;
            print_row(a,b+i,row);
        }
        return 0;
    }
    return -8;
}
static void process_once(void)
{
        static unsigned unsynced;
        take();bool pending=command_pending;unsigned cmd=request.cmd;uint32_t a=request.a,b=request.b,n=request.n;bool s=request.smoke;give();
        if(pending) {
            int result=execute(cmd,a,b,n,s);
            take();command_pending=false;give();
            rt_kprintf("[cycle-done] cmd=%u result=%d\n",cmd,result);
        }
        take();uint64_t now=clock_ms();
        unsigned pending_signal=flush_signals(now,false);
        if(pending_signal)end_session(pending_signal,now);
        /* A blocked UI must not silently stop the tail recorder. These calls
         * copy service caches only; no LVGL or charger register access here.
         * Such samples invalidate the unavailable estimator/UI context. */
        if(low_mode && (state==RUNNING || state==ARMED) && now-last_observed>2000) {
            wristflow_power_snapshot_t background=wristflow_product_power_snapshot();
            wristflow_product_charger_display(&background);
            background.estimate_valid=false;background.estimated_percent=0;
            latest.observed_ms=(uint32_t)((uint64_t)rt_tick_get()*1000U/RT_TICK_PER_SECOND);latest_at=now;
            latest.power=background;latest.adc_valid=background.voltage_valid;
            latest.diagnostic=(wristflow_battery_diagnostic_t){0};
            latest.context.flags&=~(WF_BATTERY_UI_VALID|WF_BATTERY_BRIGHTNESS_VALID);
            last_flashlight=false;keep_remaining=0; /* UI state is unavailable, not a new UI observation. */
            if(state==ARMED && background.input_observation.valid && !background.input_observation.good)state=RUNNING;
            if(state==RUNNING && background.input_observation.valid && background.input_observation.good)end_session(WF_CYCLE_USB,now);
            else if(fresh(&latest) &&
                (background.voltage_sequence!=last_adc || background.voltage_sampled_ms!=last_adc_ms)) {
                if(count>=15 || next_row>=WF_CYCLE_MAX_ROWS-1)end_session(count>=15 ? WF_CYCLE_QUEUE:WF_CYCLE_LIMIT,now);
                else {last_adc=background.voltage_sequence;last_adc_ms=background.voltage_sampled_ms;
                    last_low_sample=now;enqueue(WF_CYCLE_SAMPLE,0,now);}
            }
        }
        if(state==RUNNING && !low_mode && now-last_observed>15000)end_session(WF_CYCLE_STALE,now);
        if(state==RUNNING && low_mode && now-started>=WF_CYCLE_MAX_MS)end_session(WF_CYCLE_LIMIT,now);
        if(state==ARMED && now-armed_at>300000)end_session(WF_CYCLE_LIMIT,now);
        bool active_low=low_mode && (state==ARMED || state==RUNNING);
        rt_base_t level=rt_hw_interrupt_disable();signals_enabled=active_low;rt_hw_interrupt_enable(level);
        bool sample_request=active_low && (state==ARMED || state==RUNNING) && now-last_low_request>=5000;
        if(sample_request)last_low_request=now;
        active_low=low_mode && (state==ARMED || state==RUNNING);
        give();
        wristflow_product_power_experiment(active_low);
        if(sample_request)wristflow_product_power_request();
        for(;;) {
            uint8_t row[64];take();bool have=count!=0;
            if(have)memcpy(row,queue[head],64);
            give();if(!have)break;
            int result=wf_cycle_store_append(&store,row);++unsynced;
            bool commit=low_mode || row[5]!=WF_CYCLE_SAMPLE || unsynced>=4;
            if(!result && commit)result=wf_cycle_store_commit(&store);
            take();if(result) {state=FAILED;reason=WF_CYCLE_STORAGE;inhibit=inhibit || (!low_mode && last_flashlight);dropped+=count;count=0;}
            else {head=(head+1)%16;--count;if(commit) {persisted=store.rows;unsynced=0;last_commit=clock_ms();}}
            give();if(result) {wristflow_product_event_send(WF_EVENT_POWER);break;}
        }
        take();bool finish=(state==FINISHING || state==FAILED) && !count && store.opened;bool failed=state==FAILED;give();
        if(finish) {
            int result=wf_cycle_store_commit(&store);
            int closed=wf_cycle_store_close(&store); /* Always detach even after sync failure. */
            wf_cycle_info_t info;
            bool verified=!wf_cycle_store_reload(&store) &&
                !(smoke ? wf_cycle_store_info(&store,true,&info):wf_cycle_store_info_session(&store,session,&info)) &&
                info.session==session;
            take();prefix_verified=verified;if(verified)persisted=info.rows;
            state=result || closed || failed || !verified || !info.terminal ? FAILED:DONE;
            if(state==FAILED) {if(!reason)reason=WF_CYCLE_STORAGE;inhibit=inhibit || (!low_mode && last_flashlight);}
            give();unsynced=0;wristflow_product_event_send(WF_EVENT_POWER);
            status();
        }
        take();active_low=low_mode && (state==ARMED || state==RUNNING);give();
        wristflow_product_power_experiment(active_low);
}
static void process(void *unused)
{
    (void)unused;
    for(;;) {
        rt_uint32_t events;
        rt_event_recv(&changed,1,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,rt_tick_from_millisecond(1000),&events);
        process_once();
    }
}
static bool number(const char *s,uint32_t *out)
{
    if(!s || !*s || *s=='-')return false;
    errno=0;char *end;unsigned long n=strtoul(s,&end,10);
    if(errno || *end || n>UINT32_MAX)return false;
    *out=(uint32_t)n;return true;
}
static int wf_cycle(int argc,char **argv)
{
    if(!initialized)return -1;
    if(argc==2 && !strcmp(argv[1],"status")) {status();return 0;}
    unsigned cmd=0;uint32_t a=0,b=0,n=0;bool s=false;
    if(argc==2) {
        if(!strcmp(argv[1],"probe"))cmd=CMD_PROBE;
        if(!strcmp(argv[1],"init"))cmd=CMD_INIT;
        if(!strcmp(argv[1],"mount"))cmd=CMD_MOUNT;
    }
    if(argc==3 && !strcmp(argv[1],"arm") && number(argv[2],&a) && a)cmd=CMD_ARM;
    if(argc==3 && !strcmp(argv[1],"arm-low") && number(argv[2],&a) && a)cmd=CMD_LOW;
    if(argc==3 && !strcmp(argv[1],"stop") && number(argv[2],&a) && a)cmd=CMD_STOP;
    if(argc==4 && !strcmp(argv[1],"smoke") && number(argv[2],&a) && a && number(argv[3],&b) && b>=10 && b<=120)cmd=CMD_SMOKE;
    if(argc==3 && !strcmp(argv[1],"info") && (!strcmp(argv[2],"smoke") || !strcmp(argv[2],"cycle"))) {cmd=CMD_INFO;s=!strcmp(argv[2],"smoke");}
    if(argc==4 && !strcmp(argv[1],"info") && !strcmp(argv[2],"cycle") && number(argv[3],&a) && a)cmd=CMD_INFO;
    if(argc==3 && !strcmp(argv[1],"prefix") && number(argv[2],&a) && a)cmd=CMD_PREFIX;
    if(argc==6 && !strcmp(argv[1],"page") && (!strcmp(argv[2],"smoke") || !strcmp(argv[2],"cycle")) &&
        number(argv[3],&a) && a && number(argv[4],&b) && number(argv[5],&n) && n && n<=4 && b<=WF_CYCLE_MAX_ROWS-n) {cmd=CMD_PAGE;s=!strcmp(argv[2],"smoke");}
    if(!cmd)return -2;
    take();bool accepted=!command_pending;
    if(accepted) {request.cmd=cmd;request.a=a;request.b=b;request.n=n;request.smoke=s;command_pending=true;}
    give();rt_kprintf("[cycle-accepted] cmd=%u accepted=%u\n",cmd,accepted);
    if(accepted)rt_event_send(&changed,1);
    return accepted ? 0:-3;
}
MSH_CMD_EXPORT(wf_cycle, Explicit persistent cycle log diagnostics);
bool wf_product_cycle_start(void)
{
    if(initialized)return true;
    if(rt_mutex_init(&lock,"wf_cycle",RT_IPC_FLAG_PRIO)!=RT_EOK || rt_event_init(&changed,"wf_cycle",RT_IPC_FLAG_PRIO)!=RT_EOK)return false;
    rt_thread_t worker=rt_thread_create("wf_cycle",process,NULL,8192,22,10);
    if(!worker)return false;
    initialized=rt_thread_startup(worker)==RT_EOK;return initialized;
}
#endif
