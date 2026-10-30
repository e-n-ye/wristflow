#include "storage_write_guard.h"
#include <assert.h>
#include <string.h>
void wf_storage_write_guard_test_reset(void);
static const struct fal_partition target={.len=256},other={.len=256};
static uint8_t media[256];
static unsigned writes,erases,reads;
static int read_mode,write_mode,erase_mode;
int __real_fal_partition_read(const struct fal_partition *p,uint32_t off,uint8_t *out,size_t n)
{
    assert(p==&target && off+n<=256);++reads;
    if(read_mode==1)return -1;
    memcpy(out,media+off,n);if(read_mode==3)out[0]^=1;
    return read_mode==2 ? (int)n-1:(int)n;
}
int fal_partition_read(const struct fal_partition *p,uint32_t off,uint8_t *out,size_t n)
{ return __wrap_fal_partition_read(p,off,out,n); }
int __real_fal_partition_write(const struct fal_partition *p,uint32_t off,const uint8_t *in,size_t n)
{ assert(p==&target || p==&other);++writes;if(write_mode==1)return -1;memcpy(media+off,in,n);return write_mode==2 ? (int)n-1:(int)n; }
int __real_fal_partition_erase(const struct fal_partition *p,uint32_t off,size_t n)
{ assert(p==&target || p==&other);++erases;if(erase_mode==1)return -1;memset(media+off,erase_mode==3?0:255,n);return erase_mode==2?(int)n-1:(int)n; }
static void reset(void)
{
    wf_storage_write_guard_test_reset();assert(wf_storage_write_guard_bind(&target));
    assert(!wf_storage_write_guard_bind(&other));writes=erases=reads=0;read_mode=write_mode=erase_mode=0;
}
int main(void)
{
    uint8_t data[256];memset(data,42,sizeof data);reset();
    assert(__wrap_fal_partition_write(&target,0,data,256)==256 && reads==2);
    assert(__wrap_fal_partition_erase(&target,0,256)==256 && reads==4);
    for(unsigned mode=1;mode<=3;++mode) {
        reset();read_mode=(int)mode;
        assert(__wrap_fal_partition_write(&target,0,data,256)==-1);
        assert(wf_storage_write_guard_status().fault==(mode==3?3:5));
        read_mode=0;
        assert(__wrap_fal_partition_write(&target,0,data,1)==-1 && writes==1);
        assert(__wrap_fal_partition_erase(&target,0,256)==-1 && erases==0);
        assert(__wrap_fal_partition_write(&other,0,data,1)==1 && writes==2);
        assert(wf_storage_write_guard_status().blocked==2);
    }
    for(unsigned mode=1;mode<=2;++mode) { reset();write_mode=(int)mode;assert(__wrap_fal_partition_write(&target,0,data,1)==-1);assert(wf_storage_write_guard_status().fault==2); }
    for(unsigned mode=1;mode<=3;++mode) { reset();erase_mode=(int)mode;assert(__wrap_fal_partition_erase(&target,0,256)==-1);assert(wf_storage_write_guard_status().fault==(mode==3?3:4)); }
    reset();assert(__wrap_fal_partition_write(&target,255,data,2)==-1 && writes==0);
    assert(wf_storage_write_guard_status().fault==1);
    reset();assert(__wrap_fal_partition_write(&target,0,NULL,1)==-1 && writes==0);
    wf_storage_write_guard_checkpoint_failed();assert(wf_storage_write_guard_status().fault==1);
    reset();wf_storage_write_guard_checkpoint_failed();assert(wf_storage_write_guard_status().fault==6);
    assert(__wrap_fal_partition_write(&target,0,data,1)==-1 && writes==0);
    assert(__wrap_fal_partition_erase(&target,0,256)==-1 && erases==0);
    assert(__wrap_fal_partition_read(&target,0,data,1)==1);
    return 0;
}
