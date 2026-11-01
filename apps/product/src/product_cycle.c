#ifdef WF_CYCLE_LOG
#include "product_cycle.h"
#include "product_fs_block.h"
#include "product_services.h"
#include "product_pm.h"
#include "cycle_store.h"
#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
enum { IDLE,READY,ARMED,RUNNING,FINISHING,DONE,FAILED };
enum { CMD_PROBE=1,CMD_INIT,CMD_MOUNT,CMD_SMOKE,CMD_ARM,CMD_STOP,CMD_INFO,CMD_PAGE };
static struct rt_mutex lock;
static struct rt_event changed;
static bool initialized,prepared,probe_passed,full_seen,inhibit;
static bool prefix_verified;
static unsigned state,reason;
static uint32_t session,next_row,persisted,dropped,last_adc,last_adc_ms,seconds;
static uint64_t started,last_row_at,last_observed,armed_at,full_at;
static wristflow_battery_record_t latest;
static bool last_flashlight,smoke;
static uint8_t queue[16][64];static unsigned head,count;
static wf_cycle_store_t store;
static struct {unsigned cmd;uint32_t a,b,n;bool smoke;} request;
static bool command_pending;
static void take(void) {rt_mutex_take(&lock,RT_WAITING_FOREVER);}
static void give(void) {rt_mutex_release(&lock);}
static uint64_t clock_ms(void) {return wristflow_product_services_monotonic();}
static void enqueue(unsigned kind,unsigned why,uint64_t now)
{
    if(count==16 || next_row>=WF_CYCLE_MAX_ROWS) {++dropped;state=FAILED;reason=WF_CYCLE_QUEUE;inhibit=true;return;}
    wristflow_alarm_clock_t wall=wristflow_product_services_clock_cache();
    wf_cycle_row_t row={.session=session,.sequence=next_row,.elapsed=now-started,
        .utc=wall.utc,.revision=wall.revision,.utc_valid=wall.valid,.kind=(uint8_t)kind,.reason=(uint8_t)why,
        .flashlight=last_flashlight,.observation=latest};
    row.observation.observed_ms+=(uint32_t)(now-last_observed);
    if(!wf_cycle_encode(&row,queue[(head+count)%16])) {state=FAILED;reason=WF_CYCLE_STORAGE;inhibit=true;return;}
    ++next_row;++count;last_row_at=now;
}
static void end_session(unsigned why,uint64_t now)
{
    if(state!=RUNNING && state!=ARMED)return;
    reason=why;inhibit=inhibit || last_flashlight;
    enqueue(WF_CYCLE_END,why,now);
    state=state==FAILED ? FAILED:FINISHING;
    rt_event_send(&changed,1);wristflow_product_event_send(WF_EVENT_POWER);
}
void wf_product_cycle_observe(const wristflow_battery_record_t *record,bool flashlight)
{
    if(!initialized || !record)return;
    uint64_t now=clock_ms();take();latest=*record;last_flashlight=flashlight;last_observed=now;
    if(record->power.charge==WF_POWER_CHARGE_FULL) {full_seen=true;full_at=now;}
    if(inhibit && record->power.input_valid && record->power.input_good)inhibit=false;
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
bool wf_product_cycle_inhibit(void)
{
    if(!initialized)return false;
    take();bool value=inhibit;give();return value;
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
    if(cmd==CMD_SMOKE || cmd==CMD_ARM) {
        take();bool eligible=last_observed && clock_ms()-last_observed<15000 &&
            !wf_cycle_stop_reason(&latest,last_flashlight,0,true) &&
            (cmd==CMD_SMOKE || (full_seen && clock_ms()-full_at<300000));
        give();if(!eligible)return -6;
        int result=wf_cycle_store_create(&store,cmd==CMD_SMOKE,a);if(result)return result;
        take();smoke=cmd==CMD_SMOKE;session=a;seconds=b;next_row=persisted=dropped=0;head=count=0;reason=0;
        prefix_verified=false;armed_at=started=clock_ms();state=smoke ? RUNNING:ARMED;
        last_adc=latest.power.voltage_sequence;last_adc_ms=latest.power.voltage_sampled_ms;enqueue(WF_CYCLE_START,0,started);
        give();return 0;
    }
    if(cmd==CMD_INFO) {
        wf_cycle_info_t info;int result=wf_cycle_store_info(&store,is_smoke,&info);if(result)return result;
        rt_kprintf("[cycle-info] id=%lu rows=%lu bytes=%lu crc=%08lx terminal=%u\n",
            (unsigned long)info.session,(unsigned long)info.rows,(unsigned long)info.bytes,(unsigned long)info.crc,info.terminal);return 0;
    }
    if(cmd==CMD_PAGE) {
        uint8_t row[64];wf_cycle_row_t decoded;
        /* Four rows per command: finite UART output and explicit end marker. */
        if(!n || n>4)return -9;
        for(unsigned i=0;i<n;++i) {
            int result=wf_cycle_store_read(&store,is_smoke,a,b+i,row);
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
        if(state==RUNNING && now-last_observed>15000)end_session(WF_CYCLE_STALE,now);
        if(state==ARMED && now-armed_at>300000)end_session(WF_CYCLE_LIMIT,now);
        give();
        for(;;) {
            uint8_t row[64];take();bool have=count!=0;
            if(have)memcpy(row,queue[head],64);
            give();if(!have)break;
            int result=wf_cycle_store_append(&store,row);++unsynced;
            bool commit=row[5]!=WF_CYCLE_SAMPLE || unsynced>=4;
            if(!result && commit)result=wf_cycle_store_commit(&store);
            take();if(result) {state=FAILED;reason=WF_CYCLE_STORAGE;inhibit=inhibit || last_flashlight;dropped+=count;count=0;}
            else {head=(head+1)%16;--count;if(commit) {persisted=store.rows;unsynced=0;}}
            give();if(result) {wristflow_product_event_send(WF_EVENT_POWER);break;}
        }
        take();bool finish=(state==FINISHING || state==FAILED) && !count && store.opened;bool failed=state==FAILED;give();
        if(finish) {
            int result=wf_cycle_store_commit(&store);
            int closed=wf_cycle_store_close(&store); /* Always detach even after sync failure. */
            wf_cycle_info_t info;
            bool verified=!wf_cycle_store_reload(&store) && !wf_cycle_store_info(&store,smoke,&info);
            take();prefix_verified=verified;if(verified)persisted=info.rows;
            state=result || closed || failed || !verified || !info.terminal ? FAILED:DONE;
            if(state==FAILED) {if(!reason)reason=WF_CYCLE_STORAGE;inhibit=inhibit || last_flashlight;}
            give();unsynced=0;wristflow_product_event_send(WF_EVENT_POWER);
            status();
        }
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
    if(argc==3 && !strcmp(argv[1],"stop") && number(argv[2],&a) && a)cmd=CMD_STOP;
    if(argc==4 && !strcmp(argv[1],"smoke") && number(argv[2],&a) && a && number(argv[3],&b) && b>=10 && b<=120)cmd=CMD_SMOKE;
    if(argc==3 && !strcmp(argv[1],"info") && (!strcmp(argv[2],"smoke") || !strcmp(argv[2],"cycle"))) {cmd=CMD_INFO;s=!strcmp(argv[2],"smoke");}
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
