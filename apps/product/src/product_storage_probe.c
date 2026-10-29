#ifdef WF_STORAGE_PROBE
#include "product_services.h"
#include <rtthread.h>
#include <rthw.h>
#include <string.h>
#include <bf0_hal.h>
#include <bf0_pm.h>
#include <system_bf0_ap.h>

static uint8_t image[WF_STORAGE_BYTES];
static bool busy;
static bool image_read(void *context, uint32_t off, uint8_t *buf, size_t n)
{
    if (off>WF_STORAGE_BYTES || n>WF_STORAGE_BYTES-off) return false;
    memcpy(buf,(const uint8_t *)context+off,n); return true;
}
static int wf_storage(int argc, char **argv)
{
    if (argc!=2 || strcmp(argv[1],"scan")) { rt_kprintf("wf_storage scan; metadata only, no repair\n"); return -1; }
    rt_base_t level=rt_hw_interrupt_disable();
    bool accepted=!busy;
    if (accepted) busy=true;
    rt_hw_interrupt_enable(level);
    if (!accepted) { rt_kprintf("[storage-done] result=7 complete=0\n"); return -1; }
    wf_storage_copy_stats_t stats={0};
    unsigned result=wristflow_product_services_storage_snapshot(image,sizeof image,&stats);
    wf_storage_metadata_t report={0};
    if (!result && !wf_storage_metadata_scan(image_read,image,sizeof image,&report)) result=8;
    memset(image,0,sizeof image); /* No values survive in the diagnostic buffer. */
    rt_kprintf("[storage-boot] mode=%u raw=%08lx\n",(unsigned)SystemPowerOnModeGet(),(unsigned long)pm_get_pwron_wakeup_src());
    rt_kprintf("[storage-copy] bytes=%lu calls=%lu ms=%lu\n",(unsigned long)stats.bytes,(unsigned long)stats.calls,(unsigned long)stats.elapsed_ms);
    if (!result) {
        for (unsigned i=0;i<WF_STORAGE_SECTORS;++i) {
            const wf_storage_sector_t *s=&report.sectors[i];
            rt_kprintf("[storage-sector] i=%u store=%02x dirty=%02x valid=%u live=%lu stale=%lu tail=%lu unknown=%lu pending=%lu\n",
                i,s->store,s->dirty,s->valid,(unsigned long)s->live,(unsigned long)s->stale,(unsigned long)s->tail,(unsigned long)s->unknown,(unsigned long)s->pending);
        }
        rt_kprintf("[storage-total] header=%lu live=%lu stale=%lu pending=%lu tail=%lu unknown=%lu\n",
            (unsigned long)report.header,(unsigned long)report.live,(unsigned long)report.stale,
            (unsigned long)report.pending,(unsigned long)report.tail,(unsigned long)report.unknown);
        rt_kprintf("[storage-keys] known=%03lx dup=%lu extra=%lu bad=%lu crc=%lu reads=%lu\n",
            (unsigned long)report.known_mask,(unsigned long)report.duplicate_nodes,(unsigned long)report.extra_nodes,
            (unsigned long)report.bad_headers,(unsigned long)report.bad_crc,(unsigned long)report.read_errors);
    }
    rt_kprintf("[storage-done] result=%u complete=%u coherent=%u\n",result,!result,!result && report.coherent);
    level=rt_hw_interrupt_disable(); busy=false; rt_hw_interrupt_enable(level);
    return result ? -1 : 0;
}
MSH_CMD_EXPORT(wf_storage, Read-only settings metadata and cached boot fields);
#endif
