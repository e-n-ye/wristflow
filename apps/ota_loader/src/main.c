#include "startup.h"
#include <rtthread.h>
#include "bsp_board.h"
#include "mem_map.h"
#include "drv_flash.h"
#include "ptab.h"
#include "probe_port.h"

#if defined(BLUETOOTH) || defined(BSP_USING_DFU) || defined(USING_DFU_V2) || defined(PKG_USING_FLASHDB) || defined(RT_USING_FINSH)
#error "Read-only loader probe must not enable radio DFU, persistent services or shell"
#endif
static void wf_ota_probe(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) { rt_kprintf("usage: wf_ota_probe (read-only)\n"); return; }
    wf_ota_loader_startup_report();
    rt_kprintf("[ota-probe] code=%08x size=%u A=%08x B=%08x slot=%u meta=%08x\n",
        HCPU_FLASH_CODE_START_ADDR, HCPU_FLASH_CODE_SIZE,
        WF_OTA_SLOT_A_START_ADDR, WF_OTA_SLOT_B_START_ADDR,
        WF_OTA_SLOT_A_SIZE, WF_OTA_METADATA_REGION_START_ADDR);
    FLASH_HandleTypeDef *flash = rt_flash_get_handle_by_addr(WF_OTA_SLOT_A_START_ADDR);
    if (flash) rt_kprintf("[ota-probe] alias start=%08x end=%08x offset=%08x; registers only\n",
        flash->Instance->AASAR, flash->Instance->AAEAR, flash->Instance->AAOAR);
    rt_kprintf("[ota-probe] inhibited=%u writes=disabled mapping_apply=disabled boot_jump=disabled\n",
        wf_ota_loader_inhibited());
    rt_kprintf("[ota-probe] planned B alias relative start=%08x end=%08x offset=%08x; not applied\n",
        WF_OTA_SLOT_A_OFFSET, WF_OTA_SLOT_A_OFFSET + WF_OTA_SLOT_A_SIZE,
        WF_OTA_SLOT_B_START_ADDR - WF_OTA_SLOT_A_START_ADDR);
    wf_ota_loader_read_probe();
}
int main(void)
{
    rt_kprintf("WristFlow standalone OTA loader PROBE; not a deployable updater\n");
    wf_ota_probe(1, RT_NULL);
    return 0;
}
