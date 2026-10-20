#ifdef NDEBUG
#undef NDEBUG
#endif
#include "steps_store.h"
#include <assert.h>
#include <string.h>
static uint8_t records[2][WRISTFLOW_STEPS_RECORD_BYTES];
static size_t lengths[2];
static unsigned writes;
static bool fail_write, fail_read, corrupt;
static size_t read_record(unsigned key, uint8_t *p, size_t capacity, void *ctx)
{
    (void)ctx;
    if (fail_read || lengths[key]>capacity) return 0;
    memcpy(p, records[key], lengths[key]); if(corrupt && lengths[key]>16) p[16]^=1;
    return lengths[key];
}
static bool write_record(unsigned key, const uint8_t *p, void *ctx)
{ (void)ctx; ++writes; memcpy(records[key],p,sizeof records[key]); lengths[key]=sizeof records[key]; return !fail_write; }
static wristflow_steps_clock_t clock_at(uint32_t utc, uint64_t ms)
{ return (wristflow_steps_clock_t){.utc=utc,.monotonic_ms=ms,.valid=true}; }
static void hourly_and_legacy(void)
{
    /* Frozen v1 bytes, independently packed with Python struct/zlib: generation7,
     * day20745,total113,coverage1792411200,saved1792411260. */
    static const uint8_t legacy[32]={
        0x57,0x46,0x53,0x54,0x01,0,0,0,0x07,0,0,0,0x09,0x51,0,0,
        0x71,0,0,0,0x40,0x06,0xd6,0x6a,0x7c,0x06,0xd6,0x6a,0x5c,0x12,0xa5,0x5f
    };
    memset(records,0,sizeof records); lengths[0]=sizeof legacy; lengths[1]=0;
    memcpy(records[0],legacy,sizeof legacy); fail_read=fail_write=corrupt=false;
    wristflow_steps_store_t store, restored;
    assert(wristflow_steps_store_restore(&store,read_record,NULL));
    assert(store.generation==7 && store.saved.total==113 && !store.saved.hours_valid);
    wristflow_daily_steps_t daily;
    wristflow_daily_steps_init(&daily); assert(wristflow_daily_steps_seed(&daily,&store.saved));
    wristflow_steps_clock_t clock=clock_at(1792411270,1000);
    assert(wristflow_daily_steps_observe(&daily,500,clock));
    assert(daily.snapshot.metric.total==113 && !daily.snapshot.metric.hours_valid);
    ++clock.utc; clock.monotonic_ms+=1000;
    assert(wristflow_daily_steps_observe(&daily,502,clock));
    assert(daily.snapshot.metric.total==115 && daily.snapshot.metric.hourly[20]==2);
    wristflow_steps_store_offer(&store,&daily.snapshot,2000);
    fail_write=true;
    assert(wristflow_steps_store_process(&store,62000,read_record,write_record,NULL));
    records[1][0]=0; /* Interrupted new-slot write leaves the old32-byte record. */
    assert(wristflow_steps_store_restore(&restored,read_record,NULL));
    assert(restored.active==0 && restored.saved.total==113 && restored.generation==7);
    fail_write=false; wristflow_steps_store_flush(&store,63000);
    assert(wristflow_steps_store_process(&store,63000,read_record,write_record,NULL));
    assert(lengths[0]==32 && lengths[1]==132);
    assert(wristflow_steps_store_restore(&restored,read_record,NULL));
    assert(restored.active==1 && restored.generation==8 && restored.saved.total==115);
    assert(restored.saved.hours_valid==(1U<<20) && restored.saved.hourly[20]==2);
    lengths[1]=131; assert(wristflow_steps_store_restore(&restored,read_record,NULL));
    assert(restored.saved.total==113); lengths[1]=132;
    assert(wristflow_steps_store_restore(&restored,read_record,NULL));
    wristflow_daily_steps_t rebooted;
    wristflow_daily_steps_init(&rebooted); assert(wristflow_daily_steps_seed(&rebooted,&restored.saved));
    ++clock.utc; clock.monotonic_ms=1000;
    assert(wristflow_daily_steps_observe(&rebooted,4000,clock));
    assert(rebooted.snapshot.metric.total==115 && rebooted.snapshot.metric.hourly[20]==2);
    ++clock.utc; clock.monotonic_ms+=1000;
    assert(wristflow_daily_steps_observe(&rebooted,4003,clock));
    assert(rebooted.snapshot.metric.total==118 && rebooted.snapshot.metric.hourly[20]==5);
    /* Hour-validity and bucket values are part of saved identity, even when
     * the daily total is unchanged (e.g. a confirmed zero in the next hour). */
    wristflow_steps_snapshot_t next=daily.snapshot;
    next.observation.utc+=3600; next.metric.hours_valid|=1U<<21;
    wristflow_steps_store_offer(&store,&next,64000); assert(store.dirty);
    assert(wristflow_steps_store_process(&store,123000,read_record,write_record,NULL));
    assert(store.saved.total==115 && store.saved.hours_valid==((1U<<20)|(1U<<21)));
    next.metric.hourly[20]=3;
    wristflow_steps_store_offer(&store,&next,124000); assert(store.dirty);
    assert(wristflow_steps_store_process(&store,183000,read_record,write_record,NULL));
    assert(store.saved.total==115 && store.saved.hourly[20]==3);
    wristflow_steps_checkpoint_t c=store.saved;
    uint8_t encoded[WRISTFLOW_STEPS_RECORD_BYTES];
    c.hours_valid|=1U<<24; assert(!wristflow_steps_encode(&c,10,encoded));
    c=store.saved; c.hours_valid&=~(1U<<20); assert(!wristflow_steps_encode(&c,10,encoded));
    c=store.saved; c.hourly[20]=116; assert(!wristflow_steps_encode(&c,10,encoded));
    c=store.saved; assert(wristflow_steps_encode(&c,10,records[0]));
    c.hours_valid&=~(1U<<21); assert(wristflow_steps_encode(&c,10,records[1]));
    lengths[0]=lengths[1]=132;
    assert(!wristflow_steps_store_restore(&restored,read_record,NULL));
    c=store.saved; assert(wristflow_steps_encode(&c,11,records[0]));
    ++c.hourly[20]; assert(wristflow_steps_encode(&c,11,records[1]));
    assert(!wristflow_steps_store_restore(&restored,read_record,NULL));
}
int main(void)
{
    wristflow_steps_checkpoint_t c={.local_day=20745,.total=113,.coverage_start_utc=1792411200,.saved_utc=1792411260}, decoded;
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
    hourly_and_legacy();
    return 0;
}
