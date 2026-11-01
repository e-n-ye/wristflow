#ifdef WF_FS_PROBE
#include "fs_probe.h"
#include <rtthread.h>
#include <rthw.h>
#include <bf0_hal.h>
#include <drv_flash.h>
#include <flash_table.h>
#include <string.h>
#include "ptab.h"

static bool fs_busy;
static bool table_match(FLASH_HandleTypeDef *h, uint32_t id, uint32_t *bytes)
{
    if(!h || !h->ctable || !id || id>0xffffffu) return false;
    for(unsigned i=0;i<NOR_CMD_TABLE_CNT;++i) {
        const FLASH_RDID_TYPE_T *entry=flash_cmd_id_pool[i];
        if(!entry) continue;
        /* SDK-owned tables are sentinel terminated; never use its default-ID fallback. */
        for(;entry->manufacture_id!=FLASH_INVALID_ID;++entry) {
            if(entry->manufacture_id==(id&255) && entry->memory_type==((id>>8)&255) &&
               entry->memory_density==((id>>16)&255)) {
                *bytes=entry->mem_size;
                return h->ctable==&flash_cmd_table_list[i];
            }
        }
    }
    return false;
}
static int fs_read(void *context, uint32_t address, uint8_t *bytes, size_t n)
{
    const wf_fs_region_t *r=context;
    uint32_t start=r->base+r->offset;
    if(address<start || address>=r->next || n>r->next-address || n>WF_FS_CHUNK) return 0;
    return rt_flash_read(address,bytes,(int)n);
}
static uint32_t fs_now(void *context)
{
    (void)context;
    return (uint32_t)((uint64_t)rt_tick_get()*1000/RT_TICK_PER_SECOND);
}
static void fs_yield(void *context) { (void)context;rt_thread_mdelay(1); }
static void print_preview(unsigned pass, unsigned edge, const uint8_t *bytes)
{
    char hex[WF_FS_PREVIEW*2+1];
    static const char digits[]="0123456789abcdef";
    for(unsigned i=0;i<WF_FS_PREVIEW;++i) { hex[2*i]=digits[bytes[i]>>4];hex[2*i+1]=digits[bytes[i]&15]; }
    hex[sizeof hex-1]=0;
    rt_kprintf("[fs-preview] pass=%u edge=%u data=%s\n",pass,edge,hex);
}
static int wf_fs(int argc, char **argv)
{
    if(argc!=2 || strcmp(argv[1],"scan")) { rt_kprintf("wf_fs scan; read only, no mount or format\n");return -1; }
    rt_base_t level=rt_hw_interrupt_disable();
    bool accepted=!fs_busy;
    if(accepted) fs_busy=true;
    rt_hw_interrupt_enable(level);
    if(!accepted) { rt_kprintf("[fs-done] result=5 complete=0 stable=0 guard=0\n");return -1; }
    const wf_fs_region_t region={FS_REGION_START_ADDR-FS_REGION_OFFSET,FS_REGION_OFFSET,
        FS_REGION_SIZE,KVDB_DFU_REGION_START_ADDR};
    FLASH_HandleTypeDef *h=rt_flash_get_handle_by_addr(region.base);
    wf_fs_device_t device={0};
    if(h) {
        device.base=h->base;device.bytes=h->size;device.dual=h->dualFlash;
        device.nor=h->isNand==0 && h->Instance!=NULL;
        int total=rt_flash_get_total_size(region.base);
        device.handles=rt_flash_get_handle_by_addr(FS_REGION_START_ADDR)==h &&
            region.bytes && FS_REGION_START_ADDR<=UINT32_MAX-(region.bytes-1) &&
            rt_flash_get_handle_by_addr(FS_REGION_START_ADDR+region.bytes-1)==h &&
            total>0 && (uint32_t)total==device.bytes;
        /* BSP may return its boot-cached ID; this is not fresh RDID/SFDP. */
        device.rdid=(uint32_t)rt_flash_read_id(region.base);
        device.table_match=table_match(h,device.rdid,&device.table_bytes);
        if(device.nor && device.dual<=1) device.erase=(uint32_t)rt_flash_get_erase_alignment(region.base);
    }
    unsigned guard=wf_fs_guard(&region,&device), result=guard ? WF_FS_GUARD : WF_FS_OK;
    rt_kprintf("[fs-layout] schema=1 base=%08lx offset=%08lx bytes=%lu end=%08lx chunk=%u budget_ms=%u\n",
        (unsigned long)region.base,(unsigned long)region.offset,(unsigned long)region.bytes,
        (unsigned long)region.next,WF_FS_CHUNK,WF_FS_BUDGET_MS);
    rt_kprintf("[fs-device] base=%08lx bytes=%lu rdid=%08lx table_bytes=%lu table_match=%u dual=%lu erase=%lu handles=%u nor=%u\n",
        (unsigned long)device.base,(unsigned long)device.bytes,(unsigned long)device.rdid,
        (unsigned long)device.table_bytes,device.table_match,(unsigned long)device.dual,
        (unsigned long)device.erase,device.handles,device.nor);
    wf_fs_pass_t passes[2]={0};
    wf_fs_io_t io={(void *)&region,fs_read,fs_now,fs_yield};
    for(unsigned i=0;!result && i<2;++i) {
        result=wf_fs_scan(&region,&device,&io,&passes[i]);
        const wf_fs_pass_t *p=&passes[i];
        rt_kprintf("[fs-pass] i=%u bytes=%lu calls=%lu ff=%lu nonff=%lu crc=%08lx ms=%lu\n",i,
            (unsigned long)p->bytes,(unsigned long)p->calls,(unsigned long)p->ff,
            (unsigned long)p->nonff,(unsigned long)p->crc,(unsigned long)p->ms);
        print_preview(i,0,p->head);print_preview(i,1,p->tail);
    }
    bool complete=!result, stable=complete && wf_fs_equal(&passes[0],&passes[1]);
    if(complete && !stable) result=WF_FS_CHANGED;
    rt_kprintf("[fs-done] result=%u complete=%u stable=%u guard=%u\n",result,complete,stable,guard);
    memset(passes,0,sizeof passes);
    level=rt_hw_interrupt_disable();fs_busy=false;rt_hw_interrupt_enable(level);
    return result ? -1 : 0;
}
MSH_CMD_EXPORT(wf_fs, Read-only bounded FS region scan without mount or format);
#endif
