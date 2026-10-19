#ifdef NDEBUG
#undef NDEBUG
#endif
#include "steps_store.h"
#include <assert.h>
#include <string.h>
static uint8_t records[2][WRISTFLOW_STEPS_RECORD_BYTES];
static unsigned writes;
static bool fail_write, fail_read, corrupt;
static bool read_record(unsigned key, uint8_t *p, void *ctx)
{ (void)ctx; memcpy(p, records[key], sizeof records[key]); if(corrupt) p[16]^=1; return !fail_read; }
static bool write_record(unsigned key, const uint8_t *p, void *ctx)
{ (void)ctx; ++writes; memcpy(records[key],p,sizeof records[key]); return !fail_write; }
static wristflow_steps_clock_t clock_at(uint32_t utc, uint64_t ms)
{ return (wristflow_steps_clock_t){.utc=utc,.monotonic_ms=ms,.valid=true}; }
int main(void)
{
    wristflow_steps_checkpoint_t c={20745,113,1792411200,1792411260}, decoded;
    uint32_t generation;
    uint8_t record[WRISTFLOW_STEPS_RECORD_BYTES];
    assert(wristflow_steps_encode(&c,UINT32_MAX,record));
    assert(wristflow_steps_decode(&decoded,&generation,record,sizeof record) && generation==UINT32_MAX && decoded.total==113);
    for(unsigned i=0;i<sizeof record;++i) {
        record[i]^=1; assert(!wristflow_steps_decode(&decoded,&generation,record,sizeof record)); record[i]^=1;
    }
    assert(!wristflow_steps_decode(&decoded,&generation,record,sizeof record-1));
    wristflow_daily_steps_t daily;
    wristflow_daily_steps_init(&daily);
    assert(wristflow_daily_steps_seed(&daily,&c) && !daily.snapshot.metric.total_valid);
    wristflow_steps_clock_t clock=clock_at(0,1000); clock.valid=false;
    assert(wristflow_daily_steps_observe(&daily,100,clock) && !daily.snapshot.metric.total_valid);
    clock.monotonic_ms+=1000;
    assert(wristflow_daily_steps_observe(&daily,105,clock) && daily.seed_pending);
    clock=clock_at(c.saved_utc+10,3000);
    assert(wristflow_daily_steps_observe(&daily,110,clock));
    assert(daily.snapshot.metric.total==113 && daily.snapshot.observation.restored && !daily.seed_pending);
    assert(!daily.snapshot.metric.hours_valid && !daily.snapshot.metric.goal_valid);
    ++clock.utc; clock.monotonic_ms+=1000;
    assert(wristflow_daily_steps_observe(&daily,112,clock) && daily.snapshot.metric.total==115);
    /* Rebase an old coverage timestamp after a same-day backward correction. */
    clock.utc=c.coverage_start_utc-100; ++clock.revision; clock.monotonic_ms+=1000;
    assert(wristflow_daily_steps_observe(&daily,114,clock) && daily.snapshot.metric.total==115);
    assert(daily.snapshot.observation.coverage_start_utc==clock.utc);
    wristflow_steps_store_t store;
    assert(!wristflow_steps_store_restore(&store,read_record,NULL));
    wristflow_steps_store_offer(&store,&daily.snapshot,clock.monotonic_ms);
    assert(store.dirty);
    /* An already-consumed seed cannot reappear when the date changes/backtracks. */
    clock.utc+=86400; clock.monotonic_ms+=1000;
    assert(wristflow_daily_steps_observe(&daily,116,clock) && !daily.snapshot.metric.total);
    clock.utc-=86400; clock.monotonic_ms+=1000;
    assert(wristflow_daily_steps_observe(&daily,118,clock) && !daily.snapshot.metric.total && !daily.snapshot.observation.restored);
    /* Future-time and wrong-day seeds are rejected once, not applied later. */
    for(unsigned mode=0;mode<2;++mode) {
        wristflow_daily_steps_init(&daily); assert(wristflow_daily_steps_seed(&daily,&c));
        clock=clock_at(c.saved_utc+(mode?86400:-1),1000);
        assert(wristflow_daily_steps_observe(&daily,500,clock) && !daily.snapshot.metric.total && !daily.seed_pending);
        clock=clock_at(c.saved_utc+100,2000);
        assert(wristflow_daily_steps_observe(&daily,500,clock) && !daily.snapshot.metric.total && !daily.snapshot.observation.restored);
    }
    /* A minute deadline is not postponed by continuous sampling. */
    memset(records,0,sizeof records); assert(!wristflow_steps_store_restore(&store,read_record,NULL));
    wristflow_daily_steps_init(&daily); clock=clock_at(c.saved_utc,1000);
    assert(wristflow_daily_steps_observe(&daily,0,clock));
    wristflow_steps_store_offer(&store,&daily.snapshot,1000);
    for(unsigned i=1;i<=60;++i) {
        ++clock.utc; clock.monotonic_ms+=1000;
        assert(wristflow_daily_steps_observe(&daily,(uint16_t)i,clock));
        wristflow_steps_store_offer(&store,&daily.snapshot,clock.monotonic_ms);
    }
    assert(wristflow_steps_store_wait(&store,61000)==0 && !writes);
    assert(wristflow_steps_store_process(&store,61000,read_record,write_record,NULL));
    assert(writes==1 && store.saved_valid && store.saved.total==60 && !store.dirty);
    wristflow_steps_store_offer(&store,&daily.snapshot,62000);
    assert(wristflow_steps_store_wait(&store,UINT64_MAX)==-1);
    wristflow_steps_store_t restored;
    assert(wristflow_steps_store_restore(&restored,read_record,NULL) && restored.saved.total==60);
    /* A60-second screen-off sample may save immediately at the next allowed
     * checkpoint; it must not add another60-second debounce every time. */
    wristflow_steps_store_t screen_store=store;
    wristflow_steps_snapshot_t screen_sample=daily.snapshot; ++screen_sample.metric.total;
    wristflow_steps_store_offer(&screen_store,&screen_sample,121000);
    assert(wristflow_steps_store_wait(&screen_store,121000)==0);
    /* Only three failed attempts, including when new counts arrive. */
    ++clock.utc; clock.monotonic_ms+=1000; assert(wristflow_daily_steps_observe(&daily,61,clock));
    wristflow_steps_store_offer(&store,&daily.snapshot,62000); fail_write=true;
    for(unsigned attempt=0;attempt<3;++attempt) {
        uint64_t now=122000+attempt*5000;
        assert(wristflow_steps_store_process(&store,now,read_record,write_record,NULL));
        ++daily.snapshot.metric.total; wristflow_steps_store_offer(&store,&daily.snapshot,now+1);
    }
    assert(writes==4 && store.failed && store.dirty && !store.saved_valid);
    assert(wristflow_steps_store_wait(&store,1000000)==-1);
    assert(!wristflow_steps_store_process(&store,1000000,read_record,write_record,NULL) && writes==4);
    fail_write=false; wristflow_steps_store_flush(&store,1000000);
    assert(wristflow_steps_store_process(&store,1000000,read_record,write_record,NULL) && store.saved_valid && !store.dirty);
    /* Readback corruption/short read cannot produce a verified saved shadow. */
    ++daily.snapshot.metric.total; wristflow_steps_store_offer(&store,&daily.snapshot,1000001);
    corrupt=true; assert(wristflow_steps_store_process(&store,1060001,read_record,write_record,NULL) && !store.saved_valid);
    corrupt=false; fail_read=true;
    assert(wristflow_steps_store_process(&store,1065001,read_record,write_record,NULL) && !store.saved_valid);
    fail_read=false; assert(wristflow_steps_store_process(&store,1070001,read_record,write_record,NULL) && store.saved_valid);
    /* Generation wrap, single damaged slot, equal/ambiguous generation conflicts. */
    assert(wristflow_steps_encode(&c,UINT32_MAX,records[0])); c.total=114;
    assert(wristflow_steps_encode(&c,0,records[1]));
    assert(wristflow_steps_store_restore(&restored,read_record,NULL) && restored.active==1 && restored.saved.total==114);
    records[1][16]^=1;
    assert(wristflow_steps_store_restore(&restored,read_record,NULL) && restored.active==0 && restored.saved.total==113);
    assert(wristflow_steps_encode(&c,UINT32_MAX,records[1]));
    assert(!wristflow_steps_store_restore(&restored,read_record,NULL));
    assert(wristflow_steps_encode(&c,0x7fffffff,records[1]));
    assert(!wristflow_steps_store_restore(&restored,read_record,NULL));
    memcpy(records[1],records[0],sizeof records[0]);
    assert(wristflow_steps_store_restore(&restored,read_record,NULL) && restored.saved.total==113);
    return 0;
}
