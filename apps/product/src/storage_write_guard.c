#include "storage_write_guard.h"
#include <limits.h>
#include <string.h>
static const struct fal_partition *target;
static wf_storage_write_fault_t state;
static bool bound;
#ifdef WF_STORAGE_GUARD_HOST
/* Simulated cold reset of BSS. Never compiled into Product. */
void wf_storage_write_guard_test_reset(void) { target=NULL;state=(wf_storage_write_fault_t){0};bound=false; }
#endif
int __real_fal_partition_write(const struct fal_partition *, uint32_t, const uint8_t *, size_t);
int __real_fal_partition_erase(const struct fal_partition *, uint32_t, size_t);
int __real_fal_partition_read(const struct fal_partition *, uint32_t, uint8_t *, size_t);
bool wf_storage_write_guard_bind(const struct fal_partition *part)
{
    /* Binding is immutable until processor reset, including a failed bind. */
    if(bound)return false;
    bound=true;target=part;
    return part!=NULL;
}
wf_storage_write_fault_t wf_storage_write_guard_status(void) { return state; }
static int fail(unsigned reason,uint32_t off,size_t n)
{
    if(!state.fault)state=(wf_storage_write_fault_t){reason,off,n,0};
    return -1;
}
void wf_storage_write_guard_checkpoint_failed(void) { (void)fail(6,0,0); }
static bool allowed(uint32_t off,size_t n)
{
    if(state.fault) { if(state.blocked!=UINT32_MAX)++state.blocked;return false; }
    if(off>target->len || n>target->len-off || n>INT_MAX) { fail(1,off,n);return false; }
    return true;
}
static bool verify(const struct fal_partition *part,uint32_t off,const uint8_t *expected,size_t n)
{
    uint8_t bytes[128];
    while(n) {
        size_t take=n>sizeof bytes ? sizeof bytes:n;
        if(fal_partition_read(part,off,bytes,take)!=(int)take)return false;
        if(expected) { if(memcmp(bytes,expected,take))return false;expected+=take; }
        else for(size_t i=0;i<take;++i)if(bytes[i]!=255)return false;
        off+=(uint32_t)take;n-=take;
    }
    return true;
}
int __wrap_fal_partition_write(const struct fal_partition *part,uint32_t off,const uint8_t *bytes,size_t n)
{
    if(!target || part!=target)return __real_fal_partition_write(part,off,bytes,n);
    if(!allowed(off,n))return -1;
    if(n && !bytes)return fail(1,off,n);
    int result=__real_fal_partition_write(part,off,bytes,n);
    if(result!=(int)n)return fail(2,off,n);
    if(!verify(part,off,bytes,n))return fail(3,off,n);
    return result;
}
int __wrap_fal_partition_erase(const struct fal_partition *part,uint32_t off,size_t n)
{
    if(!target || part!=target)return __real_fal_partition_erase(part,off,n);
    if(!allowed(off,n))return -1;
    int result=__real_fal_partition_erase(part,off,n);
    if(result!=(int)n)return fail(4,off,n);
    if(!verify(part,off,NULL,n))return fail(3,off,n);
    return result;
}
int __wrap_fal_partition_read(const struct fal_partition *part,uint32_t off,uint8_t *bytes,size_t n)
{
    if(!target || part!=target)return __real_fal_partition_read(part,off,bytes,n);
    /* Reads remain available after a fault, but their error must prevent
     * startup from interpreting an unread slot as absent and overwriting it. */
    if(off>target->len || n>target->len-off || n>INT_MAX || (n && !bytes))return fail(1,off,n);
    int result=__real_fal_partition_read(part,off,bytes,n);
    return result==(int)n ? result:fail(5,off,n);
}
