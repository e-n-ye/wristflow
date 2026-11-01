#include "product_fs_block.h"
#include "product_storage_access.h"
#include <rtthread.h>
#include <rtdevice.h>
#include <rthw.h>
#include <bf0_hal.h>
#include <drv_flash.h>
#include <flash_table.h>
#include "ptab.h"

static wf_fs_block_t block_device;
static bool claimed,ready,stopped,in_flight;
static bool is_stopped(void *context)
{
    (void)context;rt_base_t level=rt_hw_interrupt_disable();
    bool value=stopped;rt_hw_interrupt_enable(level);return value;
}
static uint32_t now(void *context)
{
    (void)context;return (uint32_t)((uint64_t)rt_tick_get()*1000/RT_TICK_PER_SECOND);
}
static bool acquire(void *context)
{
    (void)context;
#ifdef RT_USING_PM
    rt_pm_request(PM_SLEEP_MODE_IDLE);
#endif
    bool taken=wf_product_storage_gate_acquire(100);
    if (taken) {
        rt_base_t level=rt_hw_interrupt_disable();
        bool already=in_flight;if (!already) in_flight=true;
        rt_hw_interrupt_enable(level);
        if (!already) return true;
        wf_product_storage_gate_release(); /* Reject recursive FS entry. */
    }
#ifdef RT_USING_PM
    rt_pm_release(PM_SLEEP_MODE_IDLE);
#endif
    return false;
}
static void release(void *context)
{
    (void)context;
    rt_base_t level=rt_hw_interrupt_disable();in_flight=false;rt_hw_interrupt_enable(level);
    wf_product_storage_gate_release();
#ifdef RT_USING_PM
    rt_pm_release(PM_SLEEP_MODE_IDLE);
#endif
}
static int read_bytes(void *context, uint32_t addr, void *buffer, size_t size)
{
    (void)context;
    if (addr<WF_FS_BLOCK_START || addr>=WF_FS_BLOCK_END || !size || size>256 || size>WF_FS_BLOCK_END-addr)
        return -1;
    return rt_flash_read(addr,buffer,(int)size);
}
static bool geometry(void)
{
    if (FS_REGION_START_ADDR!=WF_FS_BLOCK_START || FS_REGION_SIZE!=WF_FS_BLOCK_END-WF_FS_BLOCK_START ||
        KVDB_DFU_REGION_START_ADDR!=WF_FS_BLOCK_END) return false;
    uint32_t base=FS_REGION_START_ADDR-FS_REGION_OFFSET;
    FLASH_HandleTypeDef *h=rt_flash_get_handle_by_addr(base);
    if (!h || h->base!=base || !h->Instance || h->isNand || h->dualFlash || h->size!=0x1000000u ||
        rt_flash_get_handle_by_addr(WF_FS_BLOCK_START)!=h || rt_flash_get_handle_by_addr(WF_FS_BLOCK_END-1)!=h ||
        rt_flash_get_total_size(base)!=0x1000000 || rt_flash_get_erase_alignment(base)!=WF_FS_BLOCK_SIZE ||
        (uint32_t)rt_flash_read_id(base)!=0x18400bu) return false;
    /* Exact observed ID + official table; still cached metadata, not SFDP. */
    for (unsigned i=0;i<NOR_CMD_TABLE_CNT;++i) {
        const FLASH_RDID_TYPE_T *entry=flash_cmd_id_pool[i];if (!entry) continue;
        for (;entry->manufacture_id!=FLASH_INVALID_ID;++entry)
            if (entry->manufacture_id==0x0b && entry->memory_type==0x40 && entry->memory_density==0x18)
                return entry->mem_size==h->size && h->ctable==&flash_cmd_table_list[i];
    }
    return false;
}
int wf_product_fs_block_prepare(void)
{
    rt_base_t level=rt_hw_interrupt_disable();bool used=claimed;claimed=true;rt_hw_interrupt_enable(level);
    if (used) return WF_FS_BLOCK_NOT_READY;
    if (!acquire(NULL)) return WF_FS_BLOCK_BUSY;
    int result=is_stopped(NULL) ? WF_FS_BLOCK_STOPPED:0;
    uint32_t started=now(NULL);
    if (!result && !geometry()) result=WF_FS_BLOCK_NOT_READY;
    if (!result && is_stopped(NULL)) result=WF_FS_BLOCK_STOPPED;
    if (!result && (uint32_t)(now(NULL)-started)>=100) result=WF_FS_BLOCK_BUDGET;
    if (!result) {
        block_device=(wf_fs_block_t){.read=read_bytes,.acquire=acquire,.release=release,
            .stopped=is_stopped,.now=now,.budget_ms=100,.writable=false};
        level=rt_hw_interrupt_disable();ready=true;rt_hw_interrupt_enable(level);
    }
    release(NULL);return result;
}
wf_product_fs_block_state_t wf_product_fs_block_state(void)
{
    rt_base_t level=rt_hw_interrupt_disable();
    wf_product_fs_block_state_t value={ready,stopped,in_flight,block_device.fault};
    rt_hw_interrupt_enable(level);return value;
}
void wf_product_fs_block_stop(void)
{
    rt_base_t level=rt_hw_interrupt_disable();stopped=true;rt_hw_interrupt_enable(level);
}
int wf_product_fs_block_read(uint32_t block, uint32_t off, void *buffer, size_t size)
{
    if (!wf_product_fs_block_state().ready) return WF_FS_BLOCK_NOT_READY;
    return wf_fs_block_read(&block_device,block,off,buffer,size);
}
int wf_product_fs_block_prog(uint32_t block, uint32_t off, const void *buffer, size_t size)
{
    if (!wf_product_fs_block_state().ready) return WF_FS_BLOCK_NOT_READY;
    return wf_fs_block_prog(&block_device,block,off,buffer,size);
}
int wf_product_fs_block_erase(uint32_t block)
{
    if (!wf_product_fs_block_state().ready) return WF_FS_BLOCK_NOT_READY;
    return wf_fs_block_erase(&block_device,block);
}
int wf_product_fs_block_sync(void)
{
    if (!wf_product_fs_block_state().ready) return WF_FS_BLOCK_NOT_READY;
    return wf_fs_block_sync(&block_device);
}
