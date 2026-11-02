#include "cycle_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t media[0x400000+64],backup[0x400000];
static wf_cycle_store_t store;
static wf_fs_block_t device;
static unsigned progs,erases;
static bool held,fail_program;
static bool acquire(void *c) {(void)c;assert(!held);held=true;return true;}
static void release(void *c) {(void)c;assert(held);held=false;}
static bool stopped(void *c) {(void)c;return false;}
static uint32_t now(void *c) {(void)c;return 0;}
static uint8_t *at(uint32_t addr,size_t n)
{assert(held && addr>=WF_FS_BLOCK_START && addr<WF_FS_BLOCK_END && n<=WF_FS_BLOCK_END-addr);return media+32+addr-WF_FS_BLOCK_START;}
static int rd(void *c,uint32_t a,void *out,size_t n) {(void)c;memcpy(out,at(a,n),n);return (int)n;}
static int pg(void *c,uint32_t a,const void *in,size_t n)
{
    (void)c;++progs;uint8_t *p=at(a,n);if(fail_program)return (int)n; /* False success. */
    for(size_t i=0;i<n;++i) {assert((p[i]&((const uint8_t *)in)[i])==((const uint8_t *)in)[i]);p[i]&=((const uint8_t *)in)[i];}
    return (int)n;
}
static int er(void *c,uint32_t a,size_t n) {(void)c;++erases;memset(at(a,n),255,n);return 0;}
static int sy(void *c) {(void)c;assert(held);return 0;}
static void boot(void)
{
    device=(wf_fs_block_t){.read=rd,.prog=pg,.erase=er,.sync=sy,.acquire=acquire,.release=release,
        .stopped=stopped,.now=now,.budget_ms=1000,.writable=true};
    wf_cycle_store_init(&store,&device);held=fail_program=false;
}
static wf_cycle_row_t row(unsigned sequence,unsigned kind)
{
    wf_cycle_row_t r={.session=123,.sequence=sequence,.elapsed=sequence*5000,.kind=kind,.utc_valid=true,
        .utc=1793530000u+sequence*5,.revision=3,.flashlight=true};
    r.observation=(wristflow_battery_record_t){.observed_ms=sequence*5000,.adc_valid=true,
        .power={.input_valid=true,.voltage_valid=true,.millivolts=4100,.voltage_sequence=sequence+1,
            .voltage_sampled_ms=sequence*5000,.charge=WF_POWER_CHARGE_STOPPED,.estimate_valid=true,.estimated_percent=96},
        .diagnostic={.filter_valid=true,.filtered_decimillivolts=41000,.candidate=97,.mode=0,.decision=WF_BATTERY_NEW_SAMPLE},
        .context={.requested_brightness=100,.acknowledged_brightness=100}};
    return r;
}
static void encode_tests(void)
{
    uint8_t bytes[64];wf_cycle_row_t r=row(0,WF_CYCLE_START),decoded;
    assert(wf_cycle_encode(&r,bytes) && wf_cycle_decode(bytes,&decoded));
    assert(decoded.observation.power.millivolts==4100 && decoded.observation.diagnostic.filtered_decimillivolts==41000);
    assert(decoded.session==123 && decoded.utc==r.utc && decoded.revision==3);
    for(unsigned i=0;i<64;++i) {bytes[i]^=1;assert(!wf_cycle_decode(bytes,&decoded));bytes[i]^=1;}
    wristflow_battery_record_t o=r.observation;
    assert(!wf_cycle_stop_reason(&o,true,0,false));
    o.power.millivolts=3601;assert(!wf_cycle_stop_reason(&o,true,0,false));
    o.power.millivolts=3600;assert(wf_cycle_stop_reason(&o,true,0,false)==WF_CYCLE_LOW);
    o.power.millivolts=3400;assert(wf_cycle_stop_reason(&o,true,0,false)==WF_CYCLE_LOW);
    o=r.observation;o.observed_ms=15001;assert(wf_cycle_stop_reason(&o,true,0,false)==WF_CYCLE_STALE);
    o=r.observation;o.power.input_good=true;assert(wf_cycle_stop_reason(&o,true,0,false)==WF_CYCLE_USB);
    assert(!wf_cycle_stop_reason(&o,false,0,true));
    o=r.observation;assert(wf_cycle_stop_reason(&o,false,0,false)==WF_CYCLE_LOAD);
    assert(wf_cycle_stop_reason(&o,true,WF_CYCLE_MAX_MS,false)==WF_CYCLE_LIMIT);
    r=row(2,WF_CYCLE_EVENT);r.low_voltage=true;r.event=WF_CYCLE_KEY_DOWN;r.keep_remaining_ms=1199501;r.observation.context.flags|=WF_BATTERY_UI_VALID;
    assert(wf_cycle_encode(&r,bytes) && wf_cycle_decode(bytes,&decoded));
    assert(decoded.low_voltage && decoded.event==WF_CYCLE_KEY_DOWN && decoded.keep_remaining_ms==1200000);
    assert(!decoded.observation.diagnostic.filter_valid && !decoded.observation.diagnostic.filtered_decimillivolts);
    for(unsigned i=0;i<64;++i) {bytes[i]^=1;assert(!wf_cycle_decode(bytes,&decoded));bytes[i]^=1;}
    r.low_voltage=false;assert(!wf_cycle_encode(&r,bytes));
}
static void session_tests(void)
{
    /* Fresh simulated NOR: preserve a legacy short round plus smoke while a
     * named 48h run consumes space. No physical device or filesystem cleanup. */
    memset(media+32,255,0x400000);boot();assert(!wf_cycle_store_mount(&store,true));
    uint8_t bytes[64],saved_legacy[64],readback[64];wf_cycle_info_t info;wf_cycle_row_t r;
    assert(!wf_cycle_store_create(&store,false,123));
    for(unsigned i=0;i<3;++i) {r=row(i,i==0 ? WF_CYCLE_START:i==2 ? WF_CYCLE_END:WF_CYCLE_SAMPLE);
        assert(wf_cycle_encode(&r,bytes) && !wf_cycle_store_append(&store,bytes));}
    memcpy(saved_legacy,bytes,64);assert(!wf_cycle_store_commit(&store) && !wf_cycle_store_close(&store));
    assert(wf_cycle_store_create_session(&store,123)==LFS_ERR_EXIST);
    assert(!wf_cycle_store_create(&store,true,124));r=row(0,WF_CYCLE_START);r.session=124;
    assert(wf_cycle_encode(&r,bytes) && !wf_cycle_store_append(&store,bytes) && !wf_cycle_store_close(&store));
    for(uint32_t id=201;id<=202;++id) {
        assert(!wf_cycle_store_create_session(&store,id));
        for(unsigned i=0;i<3;++i) {r=row(i,i==0 ? WF_CYCLE_START:i==2 ? WF_CYCLE_END:WF_CYCLE_SAMPLE);r.session=id;
            assert(wf_cycle_encode(&r,bytes) && !wf_cycle_store_append(&store,bytes));}
        assert(!wf_cycle_store_commit(&store) && !wf_cycle_store_close(&store));
    }
    assert(wf_cycle_store_create_session(&store,201)==LFS_ERR_EXIST);
    assert(wf_cycle_store_info_session(&store,123,&info)==LFS_ERR_NOENT); /* Strict, no legacy recovery. */
    assert(wf_cycle_store_read_session(&store,201,3,bytes)<0);
    assert(!wf_cycle_store_create_session(&store,203));
    for(unsigned i=0;i<=34560;++i) {
        r=row(i,i==0 ? WF_CYCLE_START:i==34560 ? WF_CYCLE_END:WF_CYCLE_SAMPLE);r.session=203;
        assert(wf_cycle_encode(&r,bytes) && !wf_cycle_store_append(&store,bytes));
        if(i%4==3)assert(!wf_cycle_store_commit(&store));
    }
    assert(!wf_cycle_store_commit(&store) && !wf_cycle_store_close(&store));
    assert(wf_cycle_store_create_session(&store,204)==LFS_ERR_NOSPC);
    assert(wf_cycle_store_info_session(&store,204,&info)==LFS_ERR_NOENT); /* Refusal created nothing. */
    boot();assert(!wf_cycle_store_mount(&store,false));
    assert(!wf_cycle_store_info(&store,false,&info) && info.session==123 && info.rows==3);
    assert(!wf_cycle_store_read(&store,false,123,2,readback) && !memcmp(saved_legacy,readback,64));
    assert(!wf_cycle_store_info(&store,true,&info) && info.session==124 && info.rows==1);
    for(uint32_t id=201;id<=203;++id) {
        assert(!wf_cycle_store_info_session(&store,id,&info) && info.session==id && info.terminal);
        assert(info.rows==(id==203 ? 34561u:3u));
        assert(!wf_cycle_store_read_session(&store,id,info.rows-1,readback));
        wf_cycle_row_t decoded;assert(wf_cycle_decode(readback,&decoded) && decoded.session==id);
    }
}
static void prefix_tests(void)
{
    memset(media+32,255,0x400000);boot();assert(!wf_cycle_store_mount(&store,true));
    uint8_t bytes[64],old[64];wf_cycle_info_t info;
    assert(!wf_cycle_store_create(&store,false,123));wf_cycle_row_t r=row(0,WF_CYCLE_START);
    assert(wf_cycle_encode(&r,old) && !wf_cycle_store_append(&store,old) && !wf_cycle_store_commit(&store) && !wf_cycle_store_close(&store));
    for(unsigned fault=0;fault<5;++fault) {
        uint32_t id=400+fault;assert(!wf_cycle_store_create_session(&store,id));
        for(unsigned i=0;i<3;++i) {
            r=row(i,!i ? WF_CYCLE_START:i==2 ? (fault==4 ? WF_CYCLE_END:WF_CYCLE_EVENT):WF_CYCLE_SAMPLE);
            r.session=id;r.low_voltage=true;r.event=r.kind==WF_CYCLE_EVENT ? WF_CYCLE_CONTEXT:0;
            assert(wf_cycle_encode(&r,bytes) && !wf_cycle_store_append(&store,bytes) && !wf_cycle_store_commit(&store));
        }
        assert(!wf_cycle_store_close(&store));
        uint32_t total=0;bool damaged=true;
        assert(!wf_cycle_store_prefix_session(&store,id,&info,&total,&damaged) && info.rows==3 && total==192 && !damaged);
        /* Fixture-only corruption after the last committed prefix. The
         * production salvage operation below never writes or repairs it. */
        char path[24];snprintf(path,sizeof path,"cycle-%08lx.bin",(unsigned long)id);
        assert(!lfs_file_opencfg(&store.fs,&store.file,path,LFS_O_WRONLY|LFS_O_APPEND,&store.fcfg));
        r=row(3,WF_CYCLE_SAMPLE);r.session=id;r.low_voltage=true;assert(wf_cycle_encode(&r,bytes));
        unsigned length=64;
        if(fault==0 || fault==4)length=17;
        if(fault==1)bytes[40]^=1;
        if(fault==2) {r.sequence=99;assert(wf_cycle_encode(&r,bytes));}
        if(fault==3) {r.low_voltage=false;assert(wf_cycle_encode(&r,bytes));}
        assert(lfs_file_write(&store.fs,&store.file,bytes,length)==(lfs_ssize_t)length);
        assert(!lfs_file_sync(&store.fs,&store.file) && !lfs_file_close(&store.fs,&store.file));
        boot();assert(!wf_cycle_store_mount(&store,false));
        assert(wf_cycle_store_info_session(&store,id,&info)==LFS_ERR_CORRUPT);
        unsigned p=progs,e=erases;memcpy(backup,media+32,0x400000);
        assert(!wf_cycle_store_prefix_session(&store,id,&info,&total,&damaged));
        assert(info.session==id && info.rows==3 && info.bytes==192 && total==192+length && damaged);
        assert(info.terminal==(fault==4));
        assert(!wf_cycle_store_read_session(&store,id,2,bytes));
        assert(progs==p && erases==e && !memcmp(backup,media+32,0x400000));
        assert(!wf_cycle_store_read(&store,false,123,0,bytes) && !memcmp(old,bytes,64));
    }
}
int main(void)
{
    encode_tests();memset(media,0xa5,sizeof media);memset(media+32,255,0x400000);boot();
    assert(wf_cycle_store_mount(&store,false)<0 && !progs && !erases);
    assert(!wf_cycle_store_mount(&store,true));assert(!wf_cycle_store_create(&store,true,123));
    uint8_t bytes[64],readback[64];wf_cycle_row_t r;
    for(unsigned i=0;i<100;++i) {r=row(i,i ? WF_CYCLE_SAMPLE:WF_CYCLE_START);assert(wf_cycle_encode(&r,bytes));assert(!wf_cycle_store_append(&store,bytes));if(i%4==3)assert(!wf_cycle_store_commit(&store));}
    assert(!wf_cycle_store_close(&store));
    boot();assert(!wf_cycle_store_mount(&store,false));wf_cycle_info_t info;
    assert(!wf_cycle_store_info(&store,true,&info) && info.rows==100 && info.bytes==6400 && !info.terminal && info.session==123);
    for(unsigned i=0;i<100;++i) {r=row(i,i ? WF_CYCLE_SAMPLE:WF_CYCLE_START);assert(wf_cycle_encode(&r,bytes));assert(!wf_cycle_store_read(&store,true,123,i,readback) && !memcmp(bytes,readback,64));}
    assert(wf_cycle_store_create(&store,true,124)==LFS_ERR_EXIST); /* Never truncate old run. */
    assert(wf_cycle_store_mount(&store,true)==LFS_ERR_INVAL); /* Mounted cannot format. */
    memcpy(backup,media+32,0x400000);boot();
    unsigned p=progs,e=erases;assert(wf_cycle_store_mount(&store,true)==LFS_ERR_EXIST);
    assert(progs==p && erases==e && !memcmp(backup,media+32,0x400000));
    boot();assert(!wf_cycle_store_mount(&store,false) && !wf_cycle_store_create(&store,false,123));
    r=row(0,WF_CYCLE_START);assert(wf_cycle_encode(&r,bytes) && !wf_cycle_store_append(&store,bytes) && !wf_cycle_store_commit(&store));
    /* Full real schema for 48h, every five seconds, plus a terminal record. */
    for(unsigned i=1;i<34560;++i) {
        r=row(i,WF_CYCLE_SAMPLE);assert(wf_cycle_encode(&r,bytes) && !wf_cycle_store_append(&store,bytes));
        if(i%4==3)assert(!wf_cycle_store_commit(&store));
    }
    r=row(34560,WF_CYCLE_END);r.reason=WF_CYCLE_LIMIT;
    assert(wf_cycle_encode(&r,bytes) && !wf_cycle_store_append(&store,bytes) && !wf_cycle_store_commit(&store) && !wf_cycle_store_close(&store));
    assert(!wf_cycle_store_info(&store,false,&info) && info.rows==34561 && info.bytes==2211904 && info.terminal);
    /* Both immutable files remain available after owner reconstruction. */
    boot();assert(!wf_cycle_store_mount(&store,false));
    assert(!wf_cycle_store_info(&store,true,&info) && info.rows==100);
    assert(!wf_cycle_store_info(&store,false,&info) && info.rows==34561 && info.terminal);
    assert(!wf_cycle_store_read(&store,false,123,34560,readback) && !memcmp(bytes,readback,64));
    session_tests();
    prefix_tests();
    for(unsigned i=0;i<32;++i)assert(media[i]==0xa5 && media[32+0x400000+i]==0xa5);
    puts("cycle_log: legacy/smoke preserved, independent named sessions/duplicate refusal, 48h2211904B recovery with old files, capacity refusal before create, CRC/thresholds passed");
    return 0;
}
