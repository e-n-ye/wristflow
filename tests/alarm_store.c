#include "alarm_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t slots[4][WRISTFLOW_ALARM_BYTES];
static bool available[4], unreadable;
static unsigned limit=WRISTFLOW_ALARM_BYTES;
static bool read_slot(unsigned key,uint8_t *record,void *context)
{ (void)context; if(unreadable || !available[key]) return false; memcpy(record,slots[key],WRISTFLOW_ALARM_BYTES); return true; }
static bool write_slot(unsigned key,const uint8_t *record,void *context)
{ (void)context; memset(slots[key],0,WRISTFLOW_ALARM_BYTES); memcpy(slots[key],record,limit); available[key]=true; return limit==WRISTFLOW_ALARM_BYTES; }
int main(void)
{
    wristflow_alarm_schedule_t s; wristflow_alarm_schedule_init(&s);
    wristflow_alarm_command_t cmd={.operation=WF_ALARM_ADD,.draft={.hour=23,.minute=59}};
    wristflow_alarm_clock_t c={.utc=1767567540,.valid=true};
    assert(wristflow_alarm_schedule_command(&s,&cmd,c));
    wristflow_alarm_store_t store={.active=-1};
    assert(wristflow_alarm_commit(&store,&s.data,read_slot,write_slot,NULL));
    assert(store.active==0 && store.generation==1);
    wristflow_alarm_data_t old=s.data, restored=old;
    cmd=(wristflow_alarm_command_t){.operation=WF_ALARM_ENABLE,.id=1,.enabled=false};
    assert(wristflow_alarm_schedule_command(&s,&cmd,c));
    for(limit=0;limit<WRISTFLOW_ALARM_BYTES;++limit) {
        wristflow_alarm_store_t failed=store, loaded;
        assert(!wristflow_alarm_commit(&failed,&s.data,read_slot,write_slot,NULL));
        assert(failed.active==store.active && failed.generation==store.generation);
        assert(wristflow_alarm_restore(&loaded,&restored,read_slot,NULL));
        assert(restored.alarms.entries[0].enabled && restored.dates[0].once_utc==old.dates[0].once_utc);
    }
    limit=WRISTFLOW_ALARM_BYTES;
    assert(wristflow_alarm_commit(&store,&s.data,read_slot,write_slot,NULL));
    assert(wristflow_alarm_restore(&store,&restored,read_slot,NULL) && !restored.alarms.entries[0].enabled);
    uint8_t record[WRISTFLOW_ALARM_BYTES]; uint32_t generation;
    assert(wristflow_alarm_encode(&s.data,UINT32_MAX,record));
    for(unsigned i=0;i<sizeof record;++i) { record[i]^=0x80; assert(!wristflow_alarm_decode(&restored,&generation,record,sizeof record)); record[i]^=0x80; }
    assert(!wristflow_alarm_decode(&restored,&generation,record,sizeof record-1));
    assert(wristflow_alarm_decode(&restored,&generation,record,sizeof record) && generation==UINT32_MAX);
    store=(wristflow_alarm_store_t){.active=-1,.generation=UINT32_MAX-1};
    assert(wristflow_alarm_commit(&store,&s.data,read_slot,write_slot,NULL));
    assert(wristflow_alarm_commit(&store,&old,read_slot,write_slot,NULL));
    assert(wristflow_alarm_restore(&store,&restored,read_slot,NULL) && store.active==1 && restored.alarms.entries[0].enabled);
    unreadable=true; wristflow_alarm_store_t before=store;
    assert(!wristflow_alarm_commit(&store,&old,read_slot,write_slot,NULL));
    assert(store.active==before.active && store.generation==before.generation);
    unreadable=false;
    assert(wristflow_alarm_restore(&store,&restored,read_slot,NULL));
    assert(store.active==before.active && store.generation==before.generation);
    /* A fully written but unverified/unpublished draft cannot become active. */
    wristflow_alarm_prepared_t prepared;
    assert(wristflow_alarm_prepare(&store,&s.data,read_slot,write_slot,NULL,&prepared));
    assert(wristflow_alarm_restore(&store,&restored,read_slot,NULL) && restored.alarms.entries[0].enabled);
    /* Publication itself can be interrupted at every byte; the old payload
     * remains selected because its independent marker is untouched. */
    for(limit=0;limit<WRISTFLOW_ALARM_BYTES;++limit) {
        wristflow_alarm_store_t failed=store, loaded;
        assert(!wristflow_alarm_publish(&failed,&prepared,write_slot,NULL));
        assert(failed.active==store.active && failed.generation==store.generation);
        assert(wristflow_alarm_restore(&loaded,&restored,read_slot,NULL) && restored.alarms.entries[0].enabled);
    }
    puts("alarm version/CRC, interrupted writes, readback and generation wrap passed");
}
