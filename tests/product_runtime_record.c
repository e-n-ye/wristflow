#define RT_USING_PM
#include "../apps/product/src/product_runtime_record.c"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static uint64_t now;
static uint8_t slots[2][512];
static size_t lengths[2];
static unsigned writes,notifications,pm;
static bool bad_write,bad_read,corrupt,inject_stop,storage_fault;
static bool fail_boot_read,fail_notify;
static char output[8192];static size_t used;
static FILE *capture_file;
int rt_mutex_init(struct rt_mutex *m,const char *name,unsigned flags)
{ assert(m==&record_lock && !strcmp(name,"wf_rec") && flags==RT_IPC_FLAG_PRIO);m->held=0;return 0; }
int rt_mutex_take(struct rt_mutex *m,int wait) { assert(m==&record_lock && !m->held && wait==-1);m->held=1;return 0; }
int rt_mutex_release(struct rt_mutex *m) { assert(m==&record_lock && m->held);m->held=0;return 0; }
uint64_t wristflow_product_services_monotonic(void) { assert(!record_lock.held);return now; }
bool wristflow_product_services_storage_failed(void) { assert(!record_lock.held);return storage_fault; }
void wristflow_product_services_checkpoint_failed(void) { assert(!record_lock.held);storage_fault=true; }
bool wristflow_product_settings_store_notify(void) { assert(!record_lock.held);++notifications;return !fail_notify; }
void rt_pm_request(unsigned mode) { assert(!record_lock.held && mode==PM_SLEEP_MODE_IDLE && !pm);++pm; }
void rt_pm_release(unsigned mode) { assert(!record_lock.held && mode==PM_SLEEP_MODE_IDLE && pm==1);--pm; }
void rt_kprintf(const char *fmt,...)
{
    assert(!record_lock.held && !strstr(fmt,"ll"));va_list a;va_start(a,fmt);
    int n=vsnprintf(output+used,sizeof output-used,fmt,a);va_end(a);
    assert(n>=0 && n<127 && (size_t)n<sizeof output-used);
    if(capture_file)assert(fwrite(output+used,1,(size_t)n,capture_file)==(size_t)n);
    used+=(size_t)n;
}
static size_t read(unsigned key,uint8_t *bytes,size_t n,void *context)
{ assert(!record_lock.held && key<2 && n==512 && context==slots);if(bad_read || fail_boot_read)return SIZE_MAX;memcpy(bytes,slots[key],lengths[key]);if(corrupt && lengths[key])bytes[0]^=1;return lengths[key]; }
static bool write(unsigned key,const uint8_t *bytes,void *context)
{
    assert(!record_lock.held && pm==1 && key<2 && context==slots);++writes;
    if(inject_stop) { inject_stop=false;char *args[]={"wf_record","stop"};assert(!wf_record(2,args)); }
    if(bad_write)return false;
    memcpy(slots[key],bytes,512);lengths[key]=512;return true;
}
static void reboot(bool erase)
{
    ready=saved_valid=pending=writing=failed=periodic=false;active=-1;next_due=0;
    record=(wf_runtime_record_t){0};durable=(wf_runtime_summary_t){0};memset(&exported,0,sizeof exported);
    if(erase)memset(lengths,0,sizeof lengths);
    writes=notifications=pm=0;bad_read=bad_write=corrupt=inject_stop=storage_fault=false;used=0;now=0;
    assert(wf_product_runtime_start(read,write,slots,now,4,0));
}
static void observe(uint64_t ms)
{
    now=ms;wf_runtime_observation_t o={.ms=ms,.scene=WF_RUN_ON,.sample={.sequence=(uint32_t)(ms/1000)}};
    wf_product_runtime_observe(&o);
}
static int command(unsigned n,char **args) { used=0;return wf_record((int)n,args); }
int main(int argc,char **argv)
{
    char decimal[21];assert(!strcmp(decimal64(0,decimal),"0"));
    assert(!strcmp(decimal64(UINT64_MAX,decimal),"18446744073709551615"));
    reboot(true);assert(pending && !saved_valid);wf_product_runtime_process(NULL);assert(writes==1 && saved_valid && !pm);
    observe(WF_RUNTIME_PERIOD_MS-1);wf_product_runtime_process(NULL);assert(writes==1 && !pending);
    observe(WF_RUNTIME_PERIOD_MS);assert(pending && notifications==1);
    now+=300000;wf_product_runtime_process(NULL);assert(writes==2 && record.summary.generation==2 && durable.uptime_ms==WF_RUNTIME_PERIOD_MS);
    assert(record.summary.successes==2 && durable.successes==1); /* Published statistics lag one checkpoint. */
    observe(now+WF_RUNTIME_PERIOD_MS);inject_stop=true;wf_product_runtime_process(NULL);
    assert(!record.running && pending && !failed && (durable.flags&WF_RUN_RECORDING));
    wf_product_runtime_process(NULL);assert(!pending && !(durable.flags&WF_RUN_RECORDING));
    unsigned saved_writes=writes;observe(now+UINT64_C(10000000));wf_product_runtime_process(NULL);assert(writes==saved_writes);
    char *start[]={"wf_record","start"};assert(!command(2,start));wf_product_runtime_process(NULL);assert(record.running);
    observe(now+WF_RUNTIME_PERIOD_MS);bad_write=true;wf_product_runtime_process(NULL);
    assert(failed && !pending && saved_valid && storage_fault && record.summary.failures==1);
    uint64_t before=record.summary.total_ms;unsigned frozen=writes;
    bad_write=false;observe(now+WF_RUNTIME_PERIOD_MS*3);wf_product_runtime_process(NULL);
    assert(record.summary.total_ms>before && writes==frozen && failed);
    char *status[]={"wf_record","status"};assert(!command(2,status) && strstr(output,"failed=1"));
    char *snapshot[]={"wf_record","snapshot","1"};assert(!command(3,snapshot) && exported.token==1);
    uint64_t captured=exported.current.total_ms;observe(now+1000);assert(exported.current.total_ms==captured);
    char *summary[]={"wf_record","summary","1","current"};assert(!command(4,summary) && strstr(output,"record-summary-end"));
    char *samples[]={"wf_record","samples","1","0"};assert(!command(4,samples) && strstr(output,"record-samples-end"));
    assert(command(3,snapshot)==-RT_EINVAL);snapshot[2]="2";
    assert(!command(3,snapshot) && exported.token==2);assert(command(4,summary)==-RT_EINVAL);
    summary[2]="-1";assert(command(4,summary)==-RT_EINVAL);summary[2]="4294967296";assert(command(4,summary)==-RT_EINVAL);
    snapshot[2]="18446744073709551615";assert(!command(3,snapshot));
    assert(strstr(output,"token=18446744073709551615 samples="));exported.count=1;
    exported.samples[0]=(wf_runtime_sample_t){.ms=UINT64_MAX,.sequence=UINT32_MAX,.sampled_ms=UINT32_MAX,.utc=4102444799U,
        .millivolts=UINT16_MAX,.flags=63,.percent=100,.requested=100,.acknowledged=100,.charge=5,.scene=5};
    samples[2]="18446744073709551615";assert(!command(4,samples));
    reboot(false);assert(saved_valid && record.summary.boot==2 && !record.summary.uptime_ms && record.running);
    wf_product_runtime_process(NULL);assert(writes==1);
    observe(WF_RUNTIME_PERIOD_MS);corrupt=true;wf_product_runtime_process(NULL);assert(failed && storage_fault && record.summary.failures==1);
    reboot(true);lengths[0]=512;memset(slots[0],0,512);
    reboot(false);assert(failed && !pending);wf_product_runtime_process(NULL);assert(!writes);
    reboot(true);wf_product_runtime_process(NULL);storage_fault=true;observe(1000);assert(failed && !pending);
    fail_boot_read=true;reboot(false);assert(failed && !pending);wf_product_runtime_process(NULL);assert(!writes);
    fail_boot_read=false;reboot(false);assert(saved_valid && !failed);
    wf_product_runtime_process(NULL);fail_notify=true;observe(WF_RUNTIME_PERIOD_MS);assert(failed && !pending);fail_notify=false;
    assert(sizeof record+sizeof durable+sizeof exported<8192);
    if(argc==2) {
        reboot(true);wf_product_runtime_process(NULL);observe(1000);
        bad_write=true;observe(WF_RUNTIME_PERIOD_MS);wf_product_runtime_process(NULL);observe(WF_RUNTIME_PERIOD_MS+1000);
        capture_file=fopen(argv[1],"wb");assert(capture_file);
        snapshot[2]="1";assert(!command(3,snapshot));summary[2]="1";summary[3]="current";assert(!command(4,summary));
        summary[3]="saved";assert(!command(4,summary));samples[2]="1";assert(!command(4,samples));
        assert(!command(2,status));fclose(capture_file);capture_file=NULL;
    }
    printf("Product recorder: scheduling, inflight stop, readback failure, freeze, export and reboot passed; data RAM=%zu\n",sizeof record+sizeof durable+sizeof exported);
    return 0;
}
