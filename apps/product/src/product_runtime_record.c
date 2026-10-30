#include "product_runtime_record.h"
#include "product_settings_store.h"
#include "product_services.h"
#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
static struct rt_mutex record_lock;
static wf_runtime_record_t record;
static wf_runtime_summary_t durable;
static wf_runtime_read_t read_slot;
static wf_runtime_write_t write_slot;
static void *backend;
static bool enabled,ready,saved_valid,pending,writing,failed,periodic;
static int active=-1;
static uint64_t next_due;
/* Fixed export snapshot; only snapshot explicitly replaces it. No Flash I/O. */
static struct {
    uint64_t token;
    bool saved_valid;
    bool failed;
    unsigned count;
    wf_runtime_summary_t current,saved;
    wf_runtime_sample_t samples[WF_RUNTIME_SAMPLES];
} exported;
static void offer(uint64_t now,bool force)
{
    if(failed)return;
    if(pending) { if(force)periodic=false;return; }
    if((writing && !force) || (!force && now<next_due))return;
    pending=true;periodic=!force;
}
bool wf_product_runtime_start(wf_runtime_read_t read,wf_runtime_write_t write,void *context,uint64_t now,uint32_t mode,uint32_t raw)
{
    if(ready)return true;
    enabled=true;
    if(rt_mutex_init(&record_lock,"wf_rec",RT_IPC_FLAG_PRIO)!=RT_EOK)return false;
    uint8_t a[512],b[512];wf_runtime_summary_t seed;
    size_t an=read?read(0,a,sizeof a,context):0,bn=read?read(1,b,sizeof b,context):0;
    active=wf_runtime_select(&seed,a,an,b,bn);
    saved_valid=active>=0;
    if(saved_valid)durable=seed;
    else failed=an||bn; /* Nonempty untrusted/conflicting slots remain untouched. */
    read_slot=read;write_slot=write;backend=context;
    wf_runtime_boot(&record,saved_valid?&seed:NULL,now,mode,raw);
    bool future=(an==512 && !memcmp(a,"WFRN",4) && (a[4]!=1 || a[5])) ||
        (bn==512 && !memcmp(b,"WFRN",4) && (b[4]!=1 || b[5]));
    failed=failed || future || an>512 || bn>512 || !read || !write;
    ready=true;next_due=now;
    offer(now,true); /* Startup checkpoint is separate from the periodic budget. */
    return true;
}
void wf_product_runtime_observe(const wf_runtime_observation_t *o)
{
    if(!ready || !o)return;
    bool storage_fault=wristflow_product_services_storage_failed();
    rt_mutex_take(&record_lock,RT_WAITING_FOREVER);
    if(storage_fault) { failed=true;pending=false; }
    bool was=pending;
    if(wf_runtime_observe(&record,o) && record.running)offer(o->ms,false);
    bool wake=!was && pending;
    rt_mutex_release(&record_lock);
    if(wake && !wristflow_product_settings_store_notify())wf_product_runtime_disable_storage();
}
void wf_product_runtime_process(void *unused)
{
    (void)unused;if(!ready)return;
    rt_mutex_take(&record_lock,RT_WAITING_FOREVER);
    if(!pending || writing || failed) { rt_mutex_release(&record_lock);return; }
    wf_runtime_summary_t candidate=record.summary;
    candidate.generation++;
    bool is_periodic=periodic;
    unsigned target=active==0?1U:0U;
    pending=false;writing=true;
    rt_mutex_release(&record_lock);
    uint8_t bytes[512],check[512];
#ifdef RT_USING_PM
    rt_pm_request(PM_SLEEP_MODE_IDLE);
#endif
    bool ok=wf_runtime_encode(&candidate,bytes) && write_slot(target,bytes,backend) &&
        read_slot(target,check,sizeof check,backend)==sizeof check && !memcmp(bytes,check,sizeof bytes);
    if(!ok)wristflow_product_services_checkpoint_failed();
#ifdef RT_USING_PM
    rt_pm_release(PM_SLEEP_MODE_IDLE);
#endif
    uint64_t now=wristflow_product_services_monotonic();
    rt_mutex_take(&record_lock,RT_WAITING_FOREVER);
    writing=false;failed=failed || !ok;if(failed)pending=false;
    wf_runtime_save_result(&record,candidate.uptime_ms,ok,is_periodic);
    if(ok) {
        /* Save counters in the durable payload describe observations BEFORE
         * this write; readback success is RAM truth until the next checkpoint. */
        durable=candidate;saved_valid=true;active=(int)target;
        record.summary.generation=candidate.generation;
    }
    next_due=now>UINT64_MAX-WF_RUNTIME_PERIOD_MS?UINT64_MAX:now+WF_RUNTIME_PERIOD_MS;
    rt_mutex_release(&record_lock);
}
bool wf_product_runtime_failed(void)
{
    if(!ready)return enabled;
    rt_mutex_take(&record_lock,RT_WAITING_FOREVER);bool value=failed;
    rt_mutex_release(&record_lock);return value;
}
void wf_product_runtime_disable_storage(void)
{
    if(!ready)return;
    rt_mutex_take(&record_lock,RT_WAITING_FOREVER);failed=true;pending=false;
    rt_mutex_release(&record_lock);
}
bool wf_product_runtime_status(wf_runtime_summary_t *current,wf_runtime_summary_t *saved,bool *valid,bool *queued)
{
    if(!ready)return false;
    rt_mutex_take(&record_lock,RT_WAITING_FOREVER);
    if(current)*current=record.summary;
    if(saved)*saved=durable;
    if(valid)*valid=saved_valid;
    if(queued)*queued=pending||writing;
    rt_mutex_release(&record_lock);return true;
}
static bool number(const char *text,uint32_t *value)
{
    if(!text || !*text || *text=='-' || *text=='+')return false;
    char *end;errno=0;unsigned long v=strtoul(text,&end,10);
    if(errno || *end || v>UINT32_MAX)return false;
    *value=(uint32_t)v;return true;
}
static bool number64(const char *text,uint64_t *value)
{
    if(!text || !*text || *text=='-' || *text=='+')return false;
    char *end;errno=0;unsigned long long v=strtoull(text,&end,10);
    if(errno || *end)return false;
    *value=(uint64_t)v;return true;
}
static uint32_t wire_crc(const uint8_t *bytes,unsigned count)
{
    uint32_t crc=UINT32_MAX;
    for(unsigned i=0;i<count;++i) {
        crc^=bytes[i];for(unsigned j=0;j<8;++j)crc=(crc>>1)^((0U-(crc&1U))&0xedb88320U);
    }
    return ~crc;
}
/* Locked SDK rt_kprintf has no RT_PRINTF_LONGLONG. Keep the wire decimal,
 * but pass only strings/32-bit integers to its variadic formatter. */
static const char *decimal64(uint64_t value,char out[21])
{
    unsigned i=20;out[i]=0;
    do { out[--i]=(char)('0'+value%10U);value/=10U; } while(value);
    return out+i;
}
static void hex_line(const char *tag,uint64_t token,unsigned offset,const uint8_t *bytes,unsigned count)
{
    static const char digits[]="0123456789abcdef";char out[65],token_text[21];
    for(unsigned i=0;i<count;++i) { out[2*i]=digits[bytes[i]>>4];out[2*i+1]=digits[bytes[i]&15]; }
    out[2*count]=0;rt_kprintf("[record-%s] token=%s offset=%u hex=%s\n",tag,decimal64(token,token_text),offset,out);
}
static int wf_record(int argc,char **argv)
{
    if(!ready)return -RT_ERROR;
    if(argc==2 && (!strcmp(argv[1],"stop") || !strcmp(argv[1],"start"))) {
        bool start=!strcmp(argv[1],"start");uint64_t now=wristflow_product_services_monotonic();
        rt_mutex_take(&record_lock,RT_WAITING_FOREVER);
        bool changed=record.running!=start;
        wf_runtime_control(&record,now,start);if(changed)offer(now,true);
        bool queued=pending,fault=failed;
        rt_mutex_release(&record_lock);
        if(queued && !wristflow_product_settings_store_notify()) { wf_product_runtime_disable_storage();queued=false;fault=true; }
        rt_kprintf("[record-control] recording=%u pending=%u failed=%u\n",start,queued,fault);return 0;
    }
    if(argc==2 && !strcmp(argv[1],"status")) {
        rt_mutex_take(&record_lock,RT_WAITING_FOREVER);
        uint64_t tail=saved_valid && durable.boot==record.summary.boot ? record.summary.uptime_ms-durable.uptime_ms:record.summary.uptime_ms;
        uint32_t boot=record.summary.boot,generation=record.summary.generation,drops=record.summary.sample_drops,event_drops=record.summary.event_drops;
        bool running=record.running,restored=!!(record.summary.flags&WF_RUN_RESTORED),valid=saved_valid,queued=pending||writing,fault=failed;
        unsigned count=record.sample_count;
        rt_mutex_release(&record_lock);
        rt_kprintf("[record-status] schema=1 boot=%lu generation=%lu recording=%u restored=%u\n",
            (unsigned long)boot,(unsigned long)generation,running,restored);
        char tail_text[21];
        rt_kprintf("[record-store] saved=%u pending=%u failed=%u tail_ms=%s\n",valid,queued,fault,decimal64(tail,tail_text));
        rt_kprintf("[record-quality] samples=%u drops=%lu events_dropped=%lu\n",count,(unsigned long)drops,(unsigned long)event_drops);
        return 0;
    }
    uint64_t token;
    if(argc==3 && !strcmp(argv[1],"snapshot") && number64(argv[2],&token) && token) {
        char token_text[21];
        rt_mutex_take(&record_lock,RT_WAITING_FOREVER);
        if(token==exported.token) { rt_mutex_release(&record_lock);return -RT_EINVAL; }
        exported.current=record.summary;exported.saved=durable;exported.saved_valid=saved_valid;
        exported.failed=failed;
        exported.count=record.sample_count;
        for(unsigned i=0;i<exported.count;++i)wf_runtime_sample_read(&record,i,&exported.samples[i]);
        exported.token=token;
        unsigned count=exported.count;bool valid=exported.saved_valid,fault=exported.failed;
        rt_mutex_release(&record_lock);
        uint8_t manifest[16]={0};
        for(unsigned i=0;i<8;++i)manifest[i]=(uint8_t)(token>>(8*i));
        manifest[8]=(uint8_t)count;manifest[12]=(uint8_t)valid;manifest[13]=(uint8_t)fault;
        rt_kprintf("[record-snapshot] schema=1 token=%s samples=%u saved=%u failed=%u bytes=512 crc=%08lx\n",
            decimal64(token,token_text),count,valid,fault,(unsigned long)wire_crc(manifest,sizeof manifest));return 0;
    }
    uint32_t page;
    if(argc==4 && number64(argv[2],&token)) {
        char token_text[21];const char *wire_token=decimal64(token,token_text);
        if(!strcmp(argv[1],"summary") && (!strcmp(argv[3],"current") || !strcmp(argv[3],"saved"))) {
            wf_runtime_summary_t value;bool saved=!strcmp(argv[3],"saved");
            rt_mutex_take(&record_lock,RT_WAITING_FOREVER);
            bool valid=token && token==exported.token && (!saved || exported.saved_valid);
            value=saved?exported.saved:exported.current;
            rt_mutex_release(&record_lock);
            uint8_t bytes[512];if(!valid || !wf_runtime_encode(&value,bytes))return -RT_EINVAL;
            rt_kprintf("[record-summary] token=%s kind=%s\n",wire_token,argv[3]);
            for(unsigned i=0;i<512;i+=32)hex_line("data",token,i,bytes+i,32);
            rt_kprintf("[record-summary-end] token=%s\n",wire_token);return 0;
        }
        if(!strcmp(argv[1],"samples") && number(argv[3],&page) && page<8) {
            wf_runtime_sample_t rows[8];unsigned count=0;
            rt_mutex_take(&record_lock,RT_WAITING_FOREVER);
            bool valid=token && token==exported.token && page*8<exported.count;
            if(valid)for(unsigned i=page*8;i<exported.count && count<8;++i)rows[count++]=exported.samples[i];
            rt_mutex_release(&record_lock);if(!valid)return -RT_EINVAL;
            for(unsigned i=0;i<count;++i) {
                uint8_t bytes[32];if(!wf_runtime_sample_encode(&rows[i],bytes))return -RT_EINVAL;
                static const char digits[]="0123456789abcdef";char hex[65];
                for(unsigned j=0;j<32;++j) { hex[2*j]=digits[bytes[j]>>4];hex[2*j+1]=digits[bytes[j]&15]; }hex[64]=0;
                rt_kprintf("[record-sample] token=%s index=%u crc=%08lx\n",wire_token,(unsigned)(page*8+i),(unsigned long)wire_crc(bytes,sizeof bytes));
                rt_kprintf("[record-sample-data] token=%s index=%u hex=%s\n",wire_token,(unsigned)(page*8+i),hex);
            }
            rt_kprintf("[record-samples-end] token=%s page=%lu count=%u\n",wire_token,(unsigned long)page,count);return 0;
        }
    }
    rt_kprintf("wf_record status|start|stop; snapshot <nonce64>; summary <nonce64> current|saved; samples <nonce64> <0..7>\n");
    return -RT_EINVAL;
}
MSH_CMD_EXPORT(wf_record, Bounded runtime record control and snapshot export);
