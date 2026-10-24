#define RT_USING_PM
#include "../apps/product/src/product_settings_store.c"
#include <assert.h>
#include <stdio.h>

static rt_tick_t ticks;
static unsigned pm, pref_writes, pref_reads, layout_writes, event_detached, mutex_detached, deleted;
static bool fail_mutex, fail_event, fail_create, fail_start, fail_send;
static bool fail_pref_write, fail_pref_read, corrupt_pref, fail_layout_write, fail_layout_read;
static uint8_t pref_record[WRISTFLOW_SETTINGS_BYTES], layout_records[2][WRISTFLOW_LAYOUT_BYTES];
static wristflow_settings_t inject_value;
static bool inject_pref;
static uint32_t injected_id;
static unsigned layout_delay;
static struct rt_thread fake_thread;
rt_tick_t rt_tick_get(void) { return ticks; }
rt_tick_t rt_tick_from_millisecond(unsigned value) { return value; }
void rt_kprintf(const char *fmt, ...) { (void)fmt; }
void rt_pm_request(unsigned mode) { assert(mode==PM_SLEEP_MODE_IDLE && !pm && !lock.held); ++pm; }
void rt_pm_release(unsigned mode) { assert(mode==PM_SLEEP_MODE_IDLE && pm && !lock.held); --pm; }
int rt_mutex_init(struct rt_mutex *m, const char *name, unsigned flags)
{ assert(!strcmp(name,"wf_cfg") && flags==RT_IPC_FLAG_PRIO); m->held=0; return fail_mutex ? -1 : 0; }
int rt_mutex_take(struct rt_mutex *m, int wait)
{ assert(m==&lock && !m->held && wait==RT_WAITING_FOREVER); m->held=1; return 0; }
int rt_mutex_release(struct rt_mutex *m) { assert(m==&lock && m->held); m->held=0; return 0; }
int rt_mutex_detach(struct rt_mutex *m) { assert(!m->held); ++mutex_detached; return 0; }
int rt_event_init(struct rt_event *e, const char *name, unsigned flags)
{ assert(!strcmp(name,"wf_save") && flags==RT_IPC_FLAG_PRIO); e->bits=0; return fail_event ? -1 : 0; }
int rt_event_detach(struct rt_event *e) { (void)e; ++event_detached; return 0; }
int rt_event_send(struct rt_event *e, unsigned bits)
{ if (fail_send) return -1; e->bits |= bits; return 0; }
int rt_event_recv(struct rt_event *e, unsigned mask, unsigned flags, int wait, rt_uint32_t *bits)
{ (void)flags; (void)wait; *bits=e->bits&mask; e->bits &= ~mask; return *bits ? 0 : -1; }
rt_thread_t rt_thread_create(const char *name, void (*entry)(void *), void *context,
    unsigned stack, unsigned priority, unsigned slice)
{
    assert(!strcmp(name,"wf_store") && stack==6144 && priority==25 && slice==10);
    fake_thread=(struct rt_thread){entry,context}; return fail_create ? NULL : &fake_thread;
}
int rt_thread_startup(rt_thread_t thread) { assert(thread==&fake_thread); return fail_start ? -1 : 0; }
int rt_thread_delete(rt_thread_t thread) { assert(thread==&fake_thread); ++deleted; return 0; }
static bool pref_read(void *context, uint8_t *record)
{
    (void)context; assert(pm && !lock.held); ++pref_reads;
    memcpy(record,pref_record,sizeof pref_record);
    if (corrupt_pref) record[3]^=1;
    return !fail_pref_read;
}
static bool pref_write(void *context, const uint8_t *record)
{
    (void)context; assert(pm && !lock.held); ++pref_writes;
    memcpy(pref_record,record,sizeof pref_record); /* May change even on failure. */
    if (inject_pref) { inject_pref=false; injected_id=wristflow_product_settings_request(&inject_value,NULL); }
    return !fail_pref_write;
}
static bool layout_read(unsigned key, uint8_t *record, void *context)
{
    (void)context; assert(key<2 && pm && !lock.held);
    memcpy(record,layout_records[key],WRISTFLOW_LAYOUT_BYTES);
    return !fail_layout_read;
}
static bool layout_write(unsigned key, const uint8_t *record, void *context)
{
    (void)context; assert(key<2 && pm && !lock.held); ++layout_writes; ticks+=layout_delay;
    memcpy(layout_records[key],record,WRISTFLOW_LAYOUT_BYTES);
    return !fail_layout_write;
}
static void background(void)
{ changed.bits=0; process_layout(); process_settings(); assert(!pm && !lock.held); }
static wristflow_save_state_t state(uint32_t id) { return wristflow_product_settings_status(id,NULL); }
static wristflow_save_state_t lstate(uint32_t id) { return wristflow_product_layout_status(id,NULL); }
static uint32_t submit(unsigned brightness)
{ wristflow_settings_t value=wristflow_settings_default(); value.brightness=(uint8_t)brightness; return wristflow_product_settings_request(&value,NULL); }
int main(void)
{
    wristflow_settings_t boot=wristflow_settings_default();
    wristflow_layout_store_t publication={.active=-1};
    const wristflow_settings_store_io_t backend={NULL,pref_read,pref_write,layout_read,layout_write};
    assert(!submit(30));
    fail_mutex=true; assert(!wristflow_product_settings_store_start(&boot,true,&publication,&backend)); fail_mutex=false;
    fail_event=true; assert(!wristflow_product_settings_store_start(&boot,true,&publication,&backend) && mutex_detached==1); fail_event=false;
    fail_create=true; assert(!wristflow_product_settings_store_start(&boot,true,&publication,&backend) && event_detached==1 && mutex_detached==2); fail_create=false;
    fail_start=true; assert(!wristflow_product_settings_store_start(&boot,true,&publication,&backend) && deleted==1 && event_detached==2 && mutex_detached==3); fail_start=false;
    assert(wristflow_settings_encode(&boot,pref_record));
    assert(wristflow_product_settings_store_start(&boot,true,&publication,&backend));
    assert(wait_ticks()==RT_WAITING_FOREVER && !pref_writes && !layout_writes);
    background(); ticks+=3600000; background(); assert(!pref_writes && wait_ticks()==-1);
    uint32_t id=submit(30); assert(id && state(id)==WRISTFLOW_SAVE_PENDING && wait_ticks()==1500);
    ticks+=1000; uint32_t newer=submit(40); assert(newer!=id && state(id)==WRISTFLOW_SAVE_FAILED);
    assert(submit(40)==newer && wait_ticks()==1500); /* Duplicate polling cannot extend debounce. */
    rt_uint32_t bits=0; assert(rt_event_recv(&changed,1,3,1500,&bits)==0 && bits==1); /* event before wait */
    ticks+=1499; background(); assert(!pref_writes && wait_ticks()==1);
    ++ticks; background(); assert(pref_writes==1 && pref_reads==1 && state(newer)==WRISTFLOW_SAVE_DONE && wait_ticks()==-1);
    assert(submit(40)==newer); background(); assert(pref_writes==1);

    fail_pref_write=true; id=submit(50); ticks+=1500;
    for (unsigned attempt=1; attempt<=3; ++attempt) {
        background(); assert(preferences.attempts==attempt);
        if(attempt<3) { assert(state(id)==WRISTFLOW_SAVE_PENDING && wait_ticks()==5000); ticks+=5000; }
    }
    assert(state(id)==WRISTFLOW_SAVE_FAILED && wait_ticks()==-1);
    unsigned before=pref_writes; ticks+=600000; background(); assert(pref_writes==before);
    fail_pref_write=false; newer=submit(50); assert(newer!=id && wait_ticks()==0);
    background(); assert(state(newer)==WRISTFLOW_SAVE_DONE && pref_writes==before+1);

    /* A stale completed write cannot complete the newer request. */
    id=submit(31); inject_value=boot; inject_value.brightness=32; inject_pref=true;
    ticks+=1500; background();
    assert(state(injected_id)==WRISTFLOW_SAVE_PENDING && saved.brightness==31);
    ticks+=1500; background(); assert(state(injected_id)==WRISTFLOW_SAVE_DONE && saved.brightness==32);
    /* Reverting to the previous stored value during I/O still needs a new write. */
    id=submit(33); inject_value=saved; inject_pref=true; before=pref_writes;
    ticks+=1500; background(); assert(state(injected_id)==WRISTFLOW_SAVE_PENDING);
    ticks+=1500; background(); assert(state(injected_id)==WRISTFLOW_SAVE_DONE && pref_writes==before+2 && saved.brightness==32);
    /* A failed partial write invalidates the old shadow before a revert. */
    fail_pref_write=true; id=submit(34); ticks+=1500; background(); assert(!saved_valid);
    fail_pref_write=false; before=pref_writes; newer=submit(32);
    assert(state(newer)==WRISTFLOW_SAVE_PENDING); ticks+=1500; background(); assert(pref_writes==before+1 && state(newer)==WRISTFLOW_SAVE_DONE);
    corrupt_pref=true; id=submit(35); ticks+=1500;
    for(unsigned i=0;i<3;++i) { background(); ticks+=5000; }
    assert(state(id)==WRISTFLOW_SAVE_FAILED && !saved_valid && wait_ticks()==-1);
    corrupt_pref=false; fail_pref_read=true; newer=submit(36); ticks+=1500; background();
    assert(state(newer)==WRISTFLOW_SAVE_PENDING && !saved_valid);
    fail_pref_read=false; ticks+=5000; background(); assert(state(newer)==WRISTFLOW_SAVE_DONE);

    /* Mode-only writes use the existing debounce/readback, retain manual value,
     * and a runtime fault turning auto off is a second ordinary preference. */
    wristflow_settings_t automatic=saved; automatic.auto_brightness=true;
    id=wristflow_product_settings_request(&automatic,NULL); ticks+=1500; background();
    assert(state(id)==WRISTFLOW_SAVE_DONE && saved.auto_brightness && saved.brightness==36);
    wristflow_settings_t decoded;
    assert(wristflow_settings_decode(&decoded,pref_record,sizeof pref_record) && decoded.auto_brightness);
    automatic.auto_brightness=false;
    id=wristflow_product_settings_request(&automatic,NULL); ticks+=1500; background();
    assert(state(id)==WRISTFLOW_SAVE_DONE && !saved.auto_brightness && saved.brightness==36);
    wristflow_layout_t value=wristflow_layout_default();
    fail_layout_write=true; id=wristflow_product_layout_request(&value,NULL);
    for(unsigned i=0;i<3;++i) { background(); ticks+=5000; }
    assert(lstate(id)==WRISTFLOW_SAVE_FAILED && layout.attempts==3 && layout_store.active==-1 && wait_ticks()==-1);
    before=layout_writes; ticks+=60000; background(); assert(layout_writes==before);
    fail_layout_write=false; newer=wristflow_product_layout_request(&value,NULL); background();
    assert(lstate(newer)==WRISTFLOW_SAVE_DONE && layout_store.generation==1);
    fail_layout_read=true; id=wristflow_product_layout_request(&value,NULL); background();
    assert(lstate(id)==WRISTFLOW_SAVE_PENDING && layout_store.generation==1); fail_layout_read=false;
    ticks+=5000; background(); assert(lstate(id)==WRISTFLOW_SAVE_DONE && layout_store.generation==2);
    /* A slow layout commit must not leave an already-due preference sleeping. */
    id=submit(37); newer=wristflow_product_layout_request(&value,NULL); layout_delay=2000;
    background(); layout_delay=0; assert(lstate(newer)==WRISTFLOW_SAVE_DONE && state(id)==WRISTFLOW_SAVE_DONE);

    ticks=UINT32_MAX-1000; id=submit(38); assert(wait_ticks()==1500);
    ticks+=1499; background(); assert(state(id)==WRISTFLOW_SAVE_PENDING);
    ++ticks; background(); assert(state(id)==WRISTFLOW_SAVE_DONE);
    preferences.id=UINT32_MAX; assert(submit(39)==1); ticks+=1500; background();
    fail_send=true; id=submit(40); assert(state(id)==WRISTFLOW_SAVE_FAILED && wait_ticks()==-1); fail_send=false;
    before=pref_writes; newer=submit(40); background(); assert(state(newer)==WRISTFLOW_SAVE_DONE && pref_writes==before+1);
    assert(!pm && !lock.held && wait_ticks()==-1);
    puts("settings store: idle event wait, debounce, bounded retry, readback, stale completion, partial failure, wrap and cleanup passed");
    return 0;
}
