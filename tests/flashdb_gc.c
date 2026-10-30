#include <flashdb.h>
#include "storage_metadata.h"
#include "storage_write_guard.h"
void wf_storage_write_guard_test_reset(void);
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Real locked FlashDB sources; only the FAL device and RT boundary are fake.
 * No board KV values are imported. Each reboot has a fresh DB/cache object. */
static uint8_t media[16384], fixture[16384];
static struct fdb_kvdb db;
static const struct fal_flash_dev device = {
    .name="flash2", .len=sizeof media, .blk_size=8192, .write_gran=1
};
static const struct fal_partition part = {
    .name="settings", .flash_name="flash2", .len=sizeof media
};
static const char *const keys[]={"preferences","layout_a","layout_b","steps_a","steps_b",
    "alarm_a","alarm_b","alarm_ca","alarm_cb"};
static const size_t lengths[]={24,512,512,132,132,104,104,104,104};
static unsigned depth, max_depth, mutations, erases, reads, fail_at, budget;
static unsigned fault_mode, fault_hit;
static bool fault_visible;
static bool armed;
/* Use the production boundary protection. A simulated cold reset is available
 * only in the host build; no runtime reset API is compiled into Product. */
static bool guard_enabled;
static jmp_buf power_cut;
static unsigned jump_kind;
static fdb_err_t operation_result;
static void cut(unsigned kind) { jump_kind=kind;longjmp(power_cut,1); }

int rt_kprintf(const char *format, ...) { (void)format; return 0; }
uint32_t rt_tick_get_millisecond(void) { return reads; }
int fal_init(void) { return 1; }
const struct fal_partition *fal_partition_find(const char *s)
{ return !strcmp(s,"settings") ? &part : NULL; }
const struct fal_flash_dev *fal_flash_device_find(const char *s)
{ return !strcmp(s,"flash2") ? &device : NULL; }
static void boundary(const struct fal_partition *p,uint32_t addr,size_t n)
{
    assert(p==&part && addr<=sizeof media && n<=sizeof media-addr);
    if (++budget>1000000) { assert(armed); cut(2); }
}
int fal_partition_read(const struct fal_partition *p,uint32_t addr,uint8_t *out,size_t n)
{ boundary(p,addr,n); ++reads; memcpy(out,media+addr,n); return (int)n; }
static bool fault(void)
{ ++mutations; if (armed && mutations==fail_at) { ++fault_hit;fault_visible=true;return true; } return false; }
int __real_fal_partition_write(const struct fal_partition *p,uint32_t addr,const uint8_t *in,size_t n)
{
    boundary(p,addr,n); bool hit=fault();
    if(hit && fault_mode==1) return -1;
    size_t count=hit && (fault_mode==2 || fault_mode==4 || fault_mode==5) ? n/2 : n;
    for(size_t i=0;i<count;++i) { assert((media[addr+i]&in[i])==in[i]); media[addr+i]&=in[i]; }
    if(hit && fault_mode==4)return (int)count;
    if(hit && fault_mode==5) { fault_visible=memcmp(media+addr,in,n)!=0;return (int)n; }
    if(hit) cut(1);
    return (int)n;
}
int __real_fal_partition_erase(const struct fal_partition *p,uint32_t addr,size_t n)
{
    boundary(p,addr,n); assert(addr%8192==0 && n%8192==0); ++erases;
    bool hit=fault(); if(hit && fault_mode==1) return -1;
    size_t count=hit && (fault_mode==2 || fault_mode==4 || fault_mode==5) ? n/2 : n;
    memset(media+addr,255,count);
    if(hit && fault_mode==4)return (int)count;
    if(hit && fault_mode==5) {
        fault_visible=false;
        for(size_t i=0;i<n;++i)if(media[addr+i]!=255)fault_visible=true;
        return (int)n;
    }
    if(hit) cut(1);
    return (int)n;
}
int fal_partition_write(const struct fal_partition *p,uint32_t addr,const uint8_t *in,size_t n)
{ return __wrap_fal_partition_write(p,addr,in,n); }
int fal_partition_erase(const struct fal_partition *p,uint32_t addr,size_t n)
{ return __wrap_fal_partition_erase(p,addr,n); }
static void lock(fdb_db_t d) { assert(d==(fdb_db_t)&db); ++depth; if(depth>max_depth)max_depth=depth; }
static void unlock(fdb_db_t d) { assert(d==(fdb_db_t)&db && depth); --depth; }
static void boot(void)
{
    memset(&db,0,sizeof db); depth=0; budget=0;
    wf_storage_write_guard_test_reset();
    if(guard_enabled)assert(wf_storage_write_guard_bind(&part));
    fdb_kvdb_control(&db,FDB_KVDB_CTRL_SET_LOCK,(void *)lock);
    fdb_kvdb_control(&db,FDB_KVDB_CTRL_SET_UNLOCK,(void *)unlock);
    assert(fdb_kvdb_init(&db,"settings","settings",NULL,NULL)==FDB_NO_ERR);
    assert(!depth && db.parent.sec_size==8192 && db.parent.max_size==sizeof media);
}
static void payload(uint8_t *out,size_t n,unsigned seed)
{ for(size_t i=0;i<n;++i)out[i]=(uint8_t)(i*17+seed*31); }
static fdb_err_t set(const char *key,size_t n,unsigned seed)
{
    uint8_t bytes[512]; struct fdb_blob blob; assert(n<=sizeof bytes);payload(bytes,n,seed);
    fdb_err_t result=fdb_kv_set_blob(&db,key,fdb_blob_make(&blob,bytes,n));assert(!depth);return result;
}
static bool equal(const char *key,size_t n,unsigned seed)
{
    uint8_t actual[512],expected[512];struct fdb_blob blob;payload(expected,n,seed);
    size_t got=fdb_kv_get_blob(&db,key,fdb_blob_make(&blob,actual,sizeof actual));
    assert(!depth); return got==n && blob.saved.len==n && !memcmp(actual,expected,n);
}
static unsigned missing_business(void)
{ unsigned n=0;for(unsigned i=0;i<9;++i)if(!equal(keys[i],lengths[i],i+1))++n;return n; }
static bool read_metadata(void *context,uint32_t off,uint8_t *out,size_t n)
{ (void)context;memcpy(out,media+off,n);return true; }
static wf_storage_metadata_t scan(void)
{ wf_storage_metadata_t s;assert(wf_storage_metadata_scan(read_metadata,NULL,sizeof media,&s));return s; }
static void make_fixture(void)
{
    memset(media,255,sizeof media);boot();
    for(unsigned i=0;i<9;++i)assert(set(keys[i],lengths[i],i+1)==FDB_NO_ERR);
    for(unsigned i=0;i<200;++i) {
        wf_storage_metadata_t s=scan();assert(s.coherent && s.live==2015);
        unsigned empty=0;bool near=false;
        for(unsigned j=0;j<2;++j) { empty+=s.sectors[j].store==0x7f;
            near|=s.sectors[j].store==0x3f && s.sectors[j].tail<634; }
        if(empty==1 && near) {
            assert(!missing_business());memcpy(fixture,media,sizeof fixture);
            printf("fixture: live=%u stale=%u tails=%u/%u\n",s.live,s.stale,s.sectors[0].tail,s.sectors[1].tail);return;
        }
        assert(set("preferences",24,1)==FDB_NO_ERR);
    }
    assert(!"fixture construction did not reach first-GC boundary");
}
static void restore_fixture(void)
{ armed=false;memcpy(media,fixture,sizeof media);boot();mutations=erases=reads=budget=0;fault_hit=0;fault_visible=false; }
static unsigned attempt_checkpoint(void)
{
    if(setjmp(power_cut)==0) { operation_result=set("runtime_a",512,10);return 0; }
    return jump_kind;
}
static unsigned attempt_reboot(void)
{
    if(setjmp(power_cut)==0) { boot();return 0; }
    return jump_kind;
}
int main(int argc,char **argv)
{
    assert(argc==2 && (!strcmp(argv[1],"admission") || !strcmp(argv[1],"guard-candidate") || !strcmp(argv[1],"characterize")));
    guard_enabled=!strcmp(argv[1],"guard-candidate");make_fixture();restore_fixture();
    assert(set("runtime_a",512,10)==FDB_NO_ERR);unsigned first_ops=mutations;
    assert(erases && max_depth>=2 && !missing_business() && equal("runtime_a",512,10));
    assert(set("runtime_b",512,11)==FDB_NO_ERR);
    for(unsigned i=0;i<100;++i) {
        assert(set(i%2?"runtime_b":"runtime_a",512,i+20)==FDB_NO_ERR);
        assert(!missing_business());
    }
    boot();assert(!missing_business() && equal("runtime_a",512,118) && equal("runtime_b",512,119));
    printf("normal: first_gc_operations=%u, 102 checkpoints, business keys preserved after reboot\n",first_ops);
    unsigned lost[5]={0},hang[5]={0},cuts[5]={0},false_success=0,unlatched=0,wrong_checkpoint=0,benign=0;
    /* Every physical mutation of the first GC is tested independently. mode1:
     * transient negative error; mode2: torn operation then immediate power cut;
     * mode3: completed operation then immediate power cut. Recovery has no fault.
     * Budget escape is diagnostic evidence, not a passing recovery assertion. */
    for(unsigned mode=1;mode<=5;++mode)for(unsigned at=1;at<=first_ops;++at) {
        restore_fixture();fault_mode=mode;fail_at=at;armed=true;
        unsigned jump=attempt_checkpoint();
        if(jump==2) { ++hang[mode-1];continue; }
        assert(fault_hit==1);++cuts[mode-1];armed=false;
        if(mode==1 && operation_result==FDB_NO_ERR && missing_business())++false_success;
        if(mode==5 && !fault_visible)++benign;
        if(guard_enabled && (mode==1 || mode>=4) && fault_visible) {
            if(!wf_storage_write_guard_status().fault)++unlatched;
            else {
                unsigned previous=mutations;uint8_t byte=media[0];
                assert(fal_partition_write(&part,0,&byte,1)==-1);
                assert(fal_partition_erase(&part,0,8192)==-1);
                assert(mutations==previous && wf_storage_write_guard_status().blocked>=2);
            }
        }
        /* A second bounded escape detects a recovery loop without letting the
         * SDK continue executing after a simulated power loss. */
        bool committed=!jump && operation_result==FDB_NO_ERR && !wf_storage_write_guard_status().fault;
        armed=true;fail_at=0;mutations=0;
        jump=attempt_reboot();
        armed=false; /* No jump may target the returned attempt_reboot frame. */
        if(!jump) {
            if(missing_business())++lost[mode-1];
            struct fdb_kv kv;
            bool exists=fdb_kv_get_obj(&db,"runtime_a",&kv)!=NULL;
            if((committed && !exists) || (exists && !equal("runtime_a",512,10)))++wrong_checkpoint;
        }
        else ++hang[mode-1];
        armed=false;
    }
    printf("fault matrix: returned_error cases=%u lost=%u hangs=%u false_success=%u\n",cuts[0],lost[0],hang[0],false_success);
    printf("fault matrix: torn_power_cut cases=%u lost=%u hangs=%u\n",cuts[1],lost[1],hang[1]);
    printf("fault matrix: complete_power_cut cases=%u lost=%u hangs=%u\n",cuts[2],lost[2],hang[2]);
    printf("fault matrix: short_return cases=%u lost=%u hangs=%u\n",cuts[3],lost[3],hang[3]);
    printf("fault matrix: silent_partial cases=%u lost=%u hangs=%u\n",cuts[4],lost[4],hang[4]);
    printf("candidate=%u unlatched=%u wrong_checkpoint=%u no_media_mismatch=%u\n",guard_enabled,unlatched,wrong_checkpoint,benign);
    bool bad=unlatched || wrong_checkpoint;
    for(unsigned i=0;i<5;++i)bad|=lost[i] || hang[i];
    /* Characterization is a regression for the locked SDK limitation, NOT
     * persistence admission. The separate admission mode must remain red. */
    if(!strcmp(argv[1],"characterize"))return lost[0] && false_success ? 0 : 1;
    return bad ? 1 : 0;
}
