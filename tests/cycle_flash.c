#define WF_CYCLE_LOG
#define INIT_PREV_EXPORT(function)
#define RT_ASSERT(condition) assert(condition)
#include <assert.h>
#include "../apps/product/src/product_flash_gate.c"
#include "../apps/product/src/product_fs_block.c"
#include <string.h>
#include <stdio.h>
static struct rt_thread thread;
static bool irq,no_thread,db,busy,lying;
static unsigned pm,ticks,reads,writes,erases;
static uint8_t media[4096];
static const FLASH_RDID_TYPE_T ids[]={{0x0b,0x40,0x18,0,0x1000000},{0}};
static const FLASH_RDID_TYPE_T empty[]={{0}};
const FLASH_RDID_TYPE_T *const flash_cmd_id_pool[]={ids,empty};
const SPI_FLASH_FACT_CFG_T flash_cmd_table_list[2]={{0},{0}};
static FLASH_HandleTypeDef handle={(void *)1,&flash_cmd_table_list[0],0x12000000,0x1000000,0,0};
rt_thread_t rt_thread_self(void) {return no_thread ? NULL:&thread;}
unsigned rt_interrupt_get_nest(void) {return irq ? 1:0;}
int rt_mutex_init(struct rt_mutex *m,const char *n,unsigned f) {(void)n;(void)f;m->held=0;return 0;}
int rt_mutex_take(struct rt_mutex *m,int timeout) {assert(timeout==1000);if(busy)return -1;++m->held;return 0;}
int rt_mutex_release(struct rt_mutex *m) {assert(m->held);--m->held;return 0;}
rt_tick_t rt_tick_from_millisecond(unsigned n) {return n;}
rt_base_t rt_hw_interrupt_disable(void) {return 0;}
void rt_hw_interrupt_enable(rt_base_t n) {assert(!n);}
rt_tick_t rt_tick_get(void) {return ticks;}
void rt_pm_request(unsigned m) {assert(m==1);++pm;}
void rt_pm_release(unsigned m) {assert(m==1 && pm);--pm;}
bool wf_product_storage_gate_acquire(uint32_t n) {assert(n==100 && pm && !db);db=true;return true;}
void wf_product_storage_gate_release(void) {assert(db);db=false;}
void *rt_flash_get_handle_by_addr(uint32_t a) {(void)a;assert(gate.held && db && pm);return &handle;}
int rt_flash_get_total_size(uint32_t a) {(void)a;return 0x1000000;}
int rt_flash_get_erase_alignment(uint32_t a) {(void)a;return 4096;}
int rt_flash_read_id(uint32_t a) {(void)a;return 0x18400b;}
int __real_rt_flash_read(uint32_t a,uint8_t *p,int n)
{assert(gate.held && a>=WF_FS_BLOCK_START && a+(unsigned)n<=WF_FS_BLOCK_START+4096);++reads;memcpy(p,media+a-WF_FS_BLOCK_START,(size_t)n);return n;}
int __real_rt_flash_write(uint32_t a,const uint8_t *p,int n)
{assert(gate.held>=2 && db && pm && n==256);++writes;if(!lying)for(int i=0;i<n;++i)media[a-WF_FS_BLOCK_START+i]&=p[i];return n;}
int __real_rt_flash_erase(uint32_t a,int n)
{assert(gate.held>=2 && db && pm && a==WF_FS_BLOCK_START && n==4096);++erases;memset(media,255,4096);return 0;}
int rt_flash_read(uint32_t a,uint8_t *p,int n) {return __wrap_rt_flash_read(a,p,n);}
int rt_flash_write(uint32_t a,const uint8_t *p,int n) {return __wrap_rt_flash_write(a,p,n);}
int rt_flash_erase(uint32_t a,int n) {return __wrap_rt_flash_erase(a,n);}
int main(void)
{
    uint8_t data[256];memset(data,42,sizeof data);
    assert(__wrap_rt_flash_read(WF_FS_BLOCK_START,data,256)==-1 && !reads);
    assert(!flash_gate_init());irq=true;
    assert(__wrap_rt_flash_read(WF_FS_BLOCK_START,data,256)==-1 && !reads);irq=false;no_thread=true;
    assert(__wrap_rt_flash_read(WF_FS_BLOCK_START,data,256)==-1 && !reads);no_thread=false;
    assert(!wf_product_fs_block_prepare());
    assert(wf_product_fs_block_prog(0,0,data,256)==WF_FS_BLOCK_READ_ONLY && !writes);
    assert(wf_product_fs_block_admit());assert(!wf_product_fs_block_erase(0));
    assert(!wf_product_fs_block_prog(0,0,data,256));assert(!memcmp(media,data,256));
    assert(!pm && !db && !gate.held);
    assert(!wf_product_fs_block_erase(0));lying=true;
    assert(wf_product_fs_block_prog(0,0,data,256)==WF_FS_BLOCK_IO);
    assert(block_device.fault==WF_FS_BLOCK_VERIFY && !pm && !db && !gate.held);
    unsigned old=writes;assert(wf_product_fs_block_prog(0,0,data,256)==WF_FS_BLOCK_IO && writes==old);
    busy=true;assert(__wrap_rt_flash_read(WF_FS_BLOCK_START,data,256)==-1 && !gate.held);
    puts("cycle_flash: actual CycleLog SDK wrappers + adapter admission, recursive Flash gate, ISR/early rejection and false-success freeze passed");return 0;
}
