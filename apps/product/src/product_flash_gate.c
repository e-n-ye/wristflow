#ifdef WF_CYCLE_LOG
#include "product_flash_gate.h"
#include <rtthread.h>
#include <rtdevice.h>
static struct rt_mutex gate;
static bool initialized;
static int flash_gate_init(void)
{
    initialized=rt_mutex_init(&gate,"wf_flash",RT_IPC_FLAG_PRIO)==RT_EOK;
    return initialized ? RT_EOK:-RT_ERROR;
}
INIT_PREV_EXPORT(flash_gate_init);
bool wf_product_flash_gate_take(void)
{
    return initialized && rt_thread_self()!=RT_NULL && !rt_interrupt_get_nest() &&
        rt_mutex_take(&gate,rt_tick_from_millisecond(1000))==RT_EOK;
}
void wf_product_flash_gate_give(void)
{
    rt_err_t result=rt_mutex_release(&gate);RT_ASSERT(result==RT_EOK);(void)result;
}
int __real_rt_flash_read(uint32_t,uint8_t *,int);
int __real_rt_flash_write(uint32_t,const uint8_t *,int);
int __real_rt_flash_erase(uint32_t,int);
int __wrap_rt_flash_read(uint32_t addr,uint8_t *data,int n)
{
    if(!wf_product_flash_gate_take())return -1;
    int result=__real_rt_flash_read(addr,data,n);wf_product_flash_gate_give();return result;
}
int __wrap_rt_flash_write(uint32_t addr,const uint8_t *data,int n)
{
    if(!wf_product_flash_gate_take())return -1;
    int result=__real_rt_flash_write(addr,data,n);wf_product_flash_gate_give();return result;
}
int __wrap_rt_flash_erase(uint32_t addr,int n)
{
    if(!wf_product_flash_gate_take())return -1;
    int result=__real_rt_flash_erase(addr,n);wf_product_flash_gate_give();return result;
}
#endif
