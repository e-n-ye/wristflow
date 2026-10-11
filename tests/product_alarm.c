/* Compile the real Product owner with deterministic RT/Flash/RTC boundaries.
 * The worker is stepped explicitly; expiry executes without any UI/LVGL. */
#include "../apps/product/src/product_alarm.c"
#include <assert.h>
#include <stdio.h>
static wristflow_alarm_clock_t clock_value;
static unsigned reads, events, writes;
static uint8_t keys[4][WRISTFLOW_ALARM_BYTES];
static bool present_key[4], fail_write, fail_read, race_write, pass_minute;
static const wristflow_alarm_port_t *port;
rt_err_t rt_mutex_init(struct rt_mutex *m,const char *n,unsigned f) { (void)n;(void)f;m->held=false;return 0; }
rt_err_t rt_mutex_take(struct rt_mutex *m,int32_t t) { if(m->held) { assert(t==0);return -1; }m->held=true;return 0; }
rt_err_t rt_mutex_release(struct rt_mutex *m) { assert(m->held);m->held=false;return 0; }
rt_err_t rt_sem_init(struct rt_semaphore *s,const char *n,unsigned c,unsigned f) { (void)n;(void)f;s->count=c;return 0; }
rt_err_t rt_sem_release(struct rt_semaphore *s) { assert(s->count==0);++s->count;return 0; }
rt_err_t rt_sem_take(struct rt_semaphore *s,int32_t t) { (void)t;assert(s->count);--s->count;return 0; }
void rt_timer_init(struct rt_timer *t,const char *n,void(*cb)(void*),void *c,uint32_t w,unsigned f)
{ (void)n;assert(f==4);*t=(struct rt_timer){.callback=cb,.context=c,.ticks=w}; }
rt_err_t rt_timer_stop(struct rt_timer *t) { t->active=false;return 0; }
rt_err_t rt_timer_control(struct rt_timer *t,int op,void *v) { assert(op==0);t->ticks=*(uint32_t*)v;return 0; }
rt_err_t rt_timer_start(struct rt_timer *t) { assert(t->ticks && t->ticks<=300000);t->active=true;t->at=clock_value.monotonic_ms;return 0; }
rt_thread_t rt_thread_create(const char*n,void(*cb)(void*),void*c,unsigned s,unsigned p,unsigned q)
{ (void)n;(void)cb;(void)c;assert(s==4096&&p==25&&q==10);return &state; }
rt_err_t rt_thread_startup(rt_thread_t t) { assert(t);return 0; }
int rt_kprintf(const char *f,...) { (void)f;return 0; }
void wristflow_product_event_send(rt_uint32_t e) { assert(!lock.held);events|=e; }
bool wristflow_product_services_storage_ready(void) { return true; }
wristflow_alarm_clock_t wristflow_product_services_time(void) { ++reads;return clock_value; }
uint64_t wristflow_product_services_monotonic(void) { return clock_value.monotonic_ms; }
bool wristflow_product_services_alarm_read(unsigned k,uint8_t*r,void*c)
{ (void)c;assert(k<4);if(fail_read||!present_key[k])return false;memcpy(r,keys[k],sizeof keys[k]);return true; }
bool wristflow_product_services_alarm_write(unsigned k,const uint8_t*r,void*c)
{
    (void)c;assert(k<4);assert(lock.held==(k>=2));++writes;
    if(fail_write)return false;
    memcpy(keys[k],r,sizeof keys[k]);present_key[k]=true;
    if(race_write&&k<2) { race_write=false;++clock_value.revision;clock_value.utc+=3600;wristflow_product_alarm_clock_changed(); }
    if(pass_minute&&k<2) { pass_minute=false;clock_value.utc+=60;clock_value.monotonic_ms+=60000; }
    return true;
}
static void work(void) { assert(storage_work.count);rt_sem_take(&storage_work,0);storage_step(); }
static wristflow_alarm_snapshot_t snapshot(void) { wristflow_alarm_snapshot_t s;port->read(NULL,&s);return s; }
static uint32_t add(unsigned minute) {
    wristflow_alarm_command_t c={.operation=WF_ALARM_ADD,.draft={.hour=7,.minute=minute,.enabled=true}};
    return port->request(NULL,&c);
}
static void advance(unsigned seconds) {
    clock_value.utc+=seconds;clock_value.monotonic_ms+=(uint64_t)seconds*1000;
    if(deadline.active&&clock_value.monotonic_ms-deadline.at>=deadline.ticks) { deadline.active=false;deadline.callback(NULL); }
}
int main(void)
{
    /* 2026-10-11 local 06:59:00; coarse RTC seconds and separate monotonic. */
    clock_value=(wristflow_alarm_clock_t){.utc=1791673140,.valid=true};
    port=wristflow_product_alarm_start();while(storage_work.count)work();
    unsigned before=reads;for(unsigned i=0;i<100;++i)snapshot();assert(reads==before); /* getter is pure */
    uint32_t id=add(0);assert(id&&snapshot().alarms.count==0&&port->status(NULL,id)==WRISTFLOW_SAVE_PENDING);
    assert(!add(1));work();assert(snapshot().alarms.count==1&&port->status(NULL,id)==WRISTFLOW_SAVE_DONE);
    wristflow_alarm_data_t old=state.data;
    fail_read=true;id=add(1);work();assert(port->status(NULL,id)==WRISTFLOW_SAVE_FAILED&&snapshot().alarms.count==1);
    fail_read=false;wristflow_alarm_store_t restored;wristflow_alarm_data_t data;
    assert(wristflow_alarm_restore(&restored,&data,wristflow_product_services_alarm_read,NULL));assert(data.alarms.count==1);
    /* Unpublished prepared data cannot become live, even after RTC changes. */
    id=add(1);race_write=true;work();assert(port->status(NULL,id)==WRISTFLOW_SAVE_PENDING&&snapshot().alarms.count==1);
    work();assert(port->status(NULL,id)==WRISTFLOW_SAVE_DONE&&snapshot().alarms.count==2);
    /* Reset to verified old config and exercise independent due delivery. */
    state.data=old;state.observed=false;++state.version;clock_value.utc=1791673140;++clock_value.revision;port->sync(NULL);
    while(storage_work.count)work();events=0;fail_write=true;advance(60);
    assert(events&WF_EVENT_ALARM);assert(snapshot().pending[0].active&&!snapshot().alarms.entries[0].enabled);
    work();assert(storage_failed&&snapshot().pending[0].active&&deadline.ticks<=5000);
    fail_write=false;advance(5);work();assert(!storage_failed&&!snapshot().alarms.entries[0].enabled);
    uint32_t seq=snapshot().pending[0].sequence;assert(!port->ack(NULL,1,seq+1));assert(port->ack(NULL,1,seq));
    while(storage_work.count)work();assert(!snapshot().pending[0].active);
    /* Shared timer must not block during final Flash publication. */
    lock.held=true;deadline_fired(NULL);assert(deadline.ticks==20&&lock.held);lock.held=false;
    /* Missing RTC accepts a persisted enabled alarm and waits for calibration. */
    clock_value.valid=false;++clock_value.revision;port->sync(NULL);id=add(2);work();assert(port->status(NULL,id)==WRISTFLOW_SAVE_DONE);
    assert(!snapshot().time_valid&&snapshot().alarms.entries[1].enabled&&state.data.dates[1].once_utc==0);
    clock_value.valid=true;++clock_value.revision;port->sync(NULL);assert(state.data.dates[1].once_utc>clock_value.utc);
    /* Reboot loses volatile queue and expires missed one-shot dates. */
    while(storage_work.count)work();clock_value.utc+=172800;++clock_value.revision;
    ready=false;worker_ready=false;work_queued=false;pending_request=false;saved_version=0;
    port=wristflow_product_alarm_start();assert(!snapshot().pending[0].active&&!snapshot().pending[1].active);
    assert(!snapshot().alarms.entries[1].enabled);
    /* If IO crosses a newly added alarm's minute, rebuild before publishing;
     * the effective command follows Q8 rather than immediately ringing. */
    while(storage_work.count)work();clock_value.utc=1791673140;++clock_value.revision;port->sync(NULL);
    id=add(0);pass_minute=true;work();assert(port->status(NULL,id)==WRISTFLOW_SAVE_PENDING);
    work();assert(port->status(NULL,id)==WRISTFLOW_SAVE_DONE);
    assert(state.data.dates[2].once_utc==1791673200+86400 && !state.pending[2].active);
    assert(writes>0);puts("Product alarm: worker/CAS/failure/retry/RTC/RT deadline passed");return 0;
}
