/* SPDX-FileCopyrightText: 2026 SiFli Technologies(Nanjing) Co., Ltd
 * SPDX-License-Identifier: Apache-2.0
 *
 * Project-owned diagnostic entry, derived from the locked official RAM boot
 * initialization order. No legacy image selection or configuration writes. */
#include "../../../vendor/SiFli-SDK/example/boot_loader/project/butterflmicro/board/board.h"
#include <bf0_hal.h>
#include <boot_flash.h>
#include <string.h>
#include "ptab.h"
#include "boot_plan.h"
#include "ram_transition.h"
#ifdef WF_OTA_BOOT_EXECUTE_PROBE
#include "uart.h"
#include "probe_binding.h"
#endif

#ifndef WF_OTA_BOOT_DIAGNOSTIC
#error "This unqualified boot prototype must remain diagnostic-only"
#endif
#if defined(BSP_USING_RTTHREAD) || defined(BSP_USING_CHARGER) || defined(RT_USING_FINSH)
#error "RAM boot prototype is pre-RTOS and has no charger or shell"
#endif
int board_boot_src;
struct sec_configuration sec_config_cache;
extern flash_read_func g_flash_read;

typedef struct {
    uint32_t phase, alias_before[3], alias_after[3], loader_vectors[2];
    bool loader_structural_plan;
    wf_ota_boot_plan_t loader;
} wf_ota_boot_probe_result_t;
/* A debugger may inspect this RAM record. It is never a boot authorization. */
volatile wf_ota_boot_probe_result_t wf_ota_boot_probe_result;
__attribute__((noreturn)) void boot_error(unsigned char code);

#ifdef WF_OTA_BOOT_EXECUTE_PROBE
static bool probe_uart_ready;
static bool probe_nor_ready(void)
{
    return boot_handle && boot_handle->Instance==hwp_mpi2 && !boot_handle->isNand && !boot_handle->dualFlash &&
        boot_handle->base==0x12000000u && boot_handle->size==0x1000000u && g_flash_read &&
        !(boot_handle->Instance->CR&MPI_CR_CTRE) &&
        !boot_handle->Instance->AASAR && !boot_handle->Instance->AAEAR && !boot_handle->Instance->AAOAR;
}
static int probe_read(uint32_t offset, uint8_t *out, uint32_t bytes, void *unused)
{
    (void)unused;
    if (!probe_nor_ready() || offset>WF_OTA_PROBE_BYTES || bytes>WF_OTA_PROBE_BYTES-offset || bytes>256) return -1;
    int n=g_flash_read(WF_OTA_SLOT_B_START_ADDR+offset,(const int8_t *)out,bytes);
    return probe_nor_ready()?n:-1;
}
static void report(unsigned kind, unsigned status)
{
    wf_ota_probe_frame_t frame;
    wf_ota_probe_frame_init(&frame,kind,wf_ota_probe_tag);
    frame.phase=kind; frame.status=status;
    __asm volatile("adr %0, .":"=r"(frame.pc));
    __asm volatile("mov %0, sp":"=r"(frame.sp));
    frame.vtor=SCB->VTOR; frame.ccr=SCB->CCR; frame.primask=__get_PRIMASK();
    frame.alias[0]=hwp_mpi2->AASAR; frame.alias[1]=hwp_mpi2->AAEAR; frame.alias[2]=hwp_mpi2->AAOAR;
    frame.marker_address=WF_OTA_PROBE_MARKER_ADDRESS;
    frame.image_bytes=WF_OTA_PROBE_BYTES;
    if (kind==WF_OTA_PROBE_MAPPED) frame.marker=*(volatile const uint32_t *)WF_OTA_PROBE_MARKER_ADDRESS;
    if (!wf_ota_probe_uart_frame(&frame)) {
        wf_ota_boot_probe_result.phase=0xE0000055u;
        for (;;) __NOP();
    }
}
__attribute__((noreturn)) static void execute_probe(void)
{
    __disable_irq();
    if (!wf_ota_probe_uart_init()) boot_error('U');
    probe_uart_ready=true;
    if (!wf_ota_probe_match(wf_ota_probe_expected,WF_OTA_PROBE_BYTES,probe_read,0)) boot_error('I');
    uint32_t vectors[2]; memcpy(vectors,wf_ota_probe_expected,sizeof vectors);
    if (vectors[0]!=0x20002000u) boot_error('S');
    const wf_ota_boot_layout_t layout={.flash=0x12000000u,.flash_bytes=0x1000000u,
        .slot={WF_OTA_SLOT_A_START_ADDR,WF_OTA_SLOT_B_START_ADDR},.slot_bytes=WF_OTA_SLOT_A_SIZE,
        .loader=WF_OTA_LOADER_START_ADDR,.loader_bytes=WF_OTA_LOADER_SIZE,
        .ram=0x20001000u,.ram_bytes=0x1000u};
    wf_ota_boot_plan_t plan;
    if (!wf_ota_boot_plan(&plan,&layout,WF_OTA_BOOT_B,WF_OTA_PROBE_BYTES,vectors)) boot_error('P');
    report(WF_OTA_PROBE_PREPARED,1);
    if (!wf_ota_ram_apply(boot_handle->Instance,&plan.alias)) boot_error('A');
    volatile const uint32_t *mapped=(volatile const uint32_t *)WF_OTA_SLOT_A_START_ADDR;
    if (mapped[0]!=vectors[0] || mapped[1]!=vectors[1] ||
        *(volatile const uint32_t *)WF_OTA_PROBE_MARKER_ADDRESS!=0x42584950u) boot_error('M');
    report(WF_OTA_PROBE_MAPPED,1);
    wf_ota_ram_jump_probe(&plan);
}
#endif

void cache_enable(void) { } /* Same pre-cache contract as official RAM boot. */
void mpu_config(void) { }
void HAL_MspInit(void) { __HAL_WDT_DISABLE(); }
__attribute__((noreturn)) void boot_error(unsigned char code)
{
    wf_ota_boot_probe_result.phase = 0xE0000000u | code;
    __disable_irq();
#ifdef WF_OTA_BOOT_EXECUTE_PROBE
    if (probe_uart_ready) report(WF_OTA_PROBE_ERROR,code);
#endif
    for (;;) __NOP(); /* No automatic reboot / legacy recovery mutation. */
}
void hw_preinit0(void)
{
    /* Runs before C initialization: no initialized global state is used. */
    if (__HAL_SYSCFG_GET_REVID() < HAL_CHIP_REV_ID_A4) {
        MODIFY_REG(hwp_pmuc->AON_LDO, PMUC_AON_LDO_VBAT_POR_TH_Msk | PMUC_AON_LDO_VBAT_LDO_SET_VOUT_Msk,
            MAKE_REG_VAL(0, PMUC_AON_LDO_VBAT_POR_TH_Msk, PMUC_AON_LDO_VBAT_POR_TH_Pos) |
            MAKE_REG_VAL(6, PMUC_AON_LDO_VBAT_LDO_SET_VOUT_Msk, PMUC_AON_LDO_VBAT_LDO_SET_VOUT_Pos));
        hwp_pmuc->WER |= PMUC_WER_LOWBAT;
    }
    HAL_Delay_us(0);
    hwp_efusec->TIMR = 0x2D08F;
    sifli_hw_efuse_read_bank(0);
    /* RESERVE0 RAM-hook/resume bypass is deliberately unsupported here. */
}
int entry(void)
{
    wf_ota_boot_probe_result.phase = 1;
    if (__get_IPSR() || __get_CONTROL() ||
        SCB->VTOR < FLASH_BOOT_LOADER_START_ADDR ||
        SCB->VTOR >= FLASH_BOOT_LOADER_START_ADDR + FLASH_BOOT_LOADER_SIZE ||
        (SCB->CCR & (SCB_CCR_IC_Msk | SCB_CCR_DC_Msk))) boot_error('X');
    HAL_Delay_us(0);
    if (__HAL_SYSCFG_GET_REVID() >= HAL_CHIP_REV_ID_A4) {
        board_flash_power_on(); HAL_MspInit(); board_boot_src = board_boot_from();
    } else {
        board_boot_src = board_boot_from(); board_flash_power_on(); HAL_MspInit();
    }
    if (board_boot_src != BOOT_FROM_NOR) boot_error('N');
    __HAL_SYSCFG_CLEAR_SECURITY();
    dfu_flash_init(); /* Official helper still requires a valid existing FTAB. */
    if (!boot_handle || !boot_handle->Instance || boot_handle->isNand || boot_handle->dualFlash ||
        boot_handle->base != 0x12000000u || boot_handle->size != 0x1000000u || !g_flash_read ||
        (boot_handle->Instance->CR & MPI_CR_CTRE)) boot_error('G');
    MPI_TypeDef *mpi = boot_handle->Instance;
    wf_ota_boot_probe_result.alias_before[0] = mpi->AASAR;
    wf_ota_boot_probe_result.alias_before[1] = mpi->AAEAR;
    wf_ota_boot_probe_result.alias_before[2] = mpi->AAOAR;
    const wf_ota_alias_t no_alias = {0};
    if (!wf_ota_ram_apply(mpi, &no_alias)) boot_error('A');
    wf_ota_boot_probe_result.alias_after[0] = mpi->AASAR;
    wf_ota_boot_probe_result.alias_after[1] = mpi->AAEAR;
    wf_ota_boot_probe_result.alias_after[2] = mpi->AAOAR;
#ifdef WF_OTA_BOOT_EXECUTE_PROBE
    execute_probe(); /* Only the exact embedded B probe; no production route. */
#endif
    uint32_t vectors[2] = {0};
    if (g_flash_read(WF_OTA_LOADER_START_ADDR, (const int8_t *)vectors, sizeof vectors) != (int)sizeof vectors) boot_error('R');
    memcpy((void *)wf_ota_boot_probe_result.loader_vectors, vectors, sizeof vectors);
    const wf_ota_boot_layout_t layout = {
        .flash=0x12000000u,.flash_bytes=0x1000000u,
        .slot={WF_OTA_SLOT_A_START_ADDR,WF_OTA_SLOT_B_START_ADDR},.slot_bytes=WF_OTA_SLOT_A_SIZE,
        .loader=WF_OTA_LOADER_START_ADDR,.loader_bytes=WF_OTA_LOADER_SIZE,
        .ram=HCPU_RAM_DATA_START_ADDR,.ram_bytes=HCPU_RAM_DATA_SIZE
    };
    wf_ota_boot_plan_t plan;
    wf_ota_boot_probe_result.loader_structural_plan = wf_ota_boot_plan(&plan, &layout,
        WF_OTA_BOOT_LOADER, WF_OTA_LOADER_SIZE, vectors);
    wf_ota_boot_probe_result.loader = plan;
    wf_ota_boot_probe_result.phase = 2;
    /* No signature, image identity, trial persistence or electrical admission
     * is bound here. Never call wf_ota_ram_jump from this diagnostic entry. */
    __disable_irq();
    for (;;) __NOP();
}
