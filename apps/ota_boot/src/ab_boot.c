#include <rtconfig.h>
#ifdef WF_OTA_PRODUCTION_BOOT
#include <bf0_hal.h>
#include <boot_flash.h>
#include <ptab.h>
#include <string.h>
#include <errno.h>
#include "boot_plan.h"
#include "ram_transition.h"
#include "ota_journal.h"
#include "ota_trial.h"
#include "release.h"
#include "receiver_binding.h"
#include "startup_trace.h"

extern void boot_error(unsigned char code) __attribute__((noreturn));
enum { UNIT = 8192 };
/* newlib's nosys sbrk has no upper bound. Keep RSA allocations wholly inside
 * a dedicated BSS arena, separate from the vendor boot globals and stack. */
static uint8_t rsa_heap[16384] __attribute__((aligned(8)));
void *_sbrk(ptrdiff_t increment)
{
    static ptrdiff_t used;
    if (increment < -used || increment > (ptrdiff_t)sizeof rsa_heap - used) {
        errno = ENOMEM;
        return (void *)-1;
    }
    uint8_t *p = rsa_heap + used;
    used += increment;
    return p;
}
_Static_assert(WF_OTA_METADATA_REGION_SIZE == 2 * UNIT, "Journal geometry");
static const wf_ota_boot_layout_t layout = {
    .flash=0x12000000,.flash_bytes=0x1000000,.slot={WF_OTA_SLOT_A_START_ADDR,WF_OTA_SLOT_B_START_ADDR},
    .slot_bytes=WF_OTA_SLOT_A_SIZE,.loader=WF_OTA_LOADER_START_ADDR,.loader_bytes=WF_OTA_LOADER_SIZE,
    .ram=HCPU_RAM_DATA_START_ADDR,.ram_bytes=HCPU_RAM_DATA_SIZE
};
static bool physical(void *ctx)
{
    (void)ctx;
    return boot_handle && boot_handle->Instance == hwp_mpi2 && !boot_handle->dualFlash && !boot_handle->isNand &&
           boot_handle->base == layout.flash && boot_handle->size == layout.flash_bytes && g_flash_read &&
           !(hwp_mpi2->CR & MPI_CR_CTRE) && !hwp_mpi2->AASAR && !hwp_mpi2->AAEAR && !hwp_mpi2->AAOAR;
}
static int read_bytes(void *ctx, uint32_t address, void *out, size_t bytes)
{
    (void)ctx;
    return g_flash_read(address, out, bytes);
}
static wf_ota_physical_reader_t reader = {.ready=physical,.read=read_bytes,
    .region={{WF_OTA_SLOT_A_START_ADDR,WF_OTA_SLOT_A_SIZE},{WF_OTA_SLOT_B_START_ADDR,WF_OTA_SLOT_B_SIZE},
             {WF_OTA_METADATA_REGION_START_ADDR,UNIT},{WF_OTA_METADATA_REGION_START_ADDR + UNIT,UNIT},
             [WF_OTA_READ_ORIGINAL_A]={WF_ORIGINAL_A_ADDRESS,WF_RELEASE_HEADER_BYTES}}};
static int read_unit(void *ctx, unsigned unit, uint32_t offset, void *out, size_t size)
{
    (void)ctx;
    return unit < 2 ? wf_ota_physical_read(&reader, WF_OTA_READ_META0 + unit, offset, out, size) : -1;
}
static bool flash_owner(void)
{
    /* The boot runs before the RTOS, from internal RAM. Require LCPU wait and
     * idle DMA controllers before either metadata erase or program. */
    if (!physical(NULL) || !(hwp_lpsys_aon->PMR & LPSYS_AON_PMR_CPUWAIT) ||
        (hwp_extdma->CCR & EXTDMA_CCR_EN)) return false;
    const size_t stride = offsetof(DMAC_TypeDef, CCR2) - offsetof(DMAC_TypeDef, CCR1);
    for (unsigned i = 0; i < 8; i++) {
        uintptr_t delta = i * stride;
        uint32_t dma1 = *(volatile uint32_t *)((uintptr_t)&hwp_dmac1->CCR1 + delta);
        uint32_t dma2 = *(volatile uint32_t *)((uintptr_t)&hwp_dmac2->CCR1 + delta);
        if ((dma1 | dma2) & DMAC_CCR1_EN) return false;
    }
    return true;
}
static int erase_unit(void *ctx, unsigned unit)
{
    (void)ctx;
    return unit < 2 && flash_owner() && g_flash_erase ?
           g_flash_erase(WF_OTA_METADATA_REGION_START_ADDR + unit * UNIT, UNIT) : -1;
}
static int program_unit(void *ctx, unsigned unit, const void *src, size_t n)
{
    (void)ctx;
    return unit < 2 && n == WF_OTA_AB_BYTES && flash_owner() && g_flash_write ?
           g_flash_write(WF_OTA_METADATA_REGION_START_ADDR + unit * UNIT, src, n) : -1;
}
static bool acquire(void *ctx) { return physical(ctx); }
static void release_lock(void *ctx) { (void)ctx; }
static int sync_unit(void *ctx) { return physical(ctx) ? 0 : -1; }
static uint32_t origin;
static uint32_t now_ms(void *ctx)
{
    (void)ctx;
    return (uint32_t)(((uint64_t)(uint32_t)(HAL_GTIMER_READ() - origin) * 1000u) >> 15);
}
static wf_ota_journal_t journal = {.read=read_unit,.program=program_unit,.erase=erase_unit,.sync=sync_unit,
    .acquire=acquire,.release=release_lock,.now_ms=now_ms,.erase_bytes=UNIT,.budget_ms=2000,.writable=true};

#ifdef WF_OTA_BOOT_TRACE
static bool trace_vector_plan(wf_ota_boot_plan_t *plan, const wf_ota_boot_layout_t *limits,
                              unsigned target, uint32_t bytes, const uint32_t vectors[2])
{
    wf_boot_trace('u', vectors[0]);
    wf_boot_trace('t', vectors[1]);
    return wf_boot_trace_rc('q', wf_ota_boot_plan(plan, limits, target, bytes, vectors));
}
#else
#define trace_vector_plan wf_ota_boot_plan
#endif
static bool prepare(wf_ota_boot_plan_t *plan, unsigned target, uint32_t bytes)
{
    uint32_t addr = target == WF_OTA_BOOT_LOADER ? layout.loader : layout.slot[target];
    uint32_t vectors[2];
    return physical(NULL) && g_flash_read(addr, (const int8_t *)vectors, sizeof vectors) == sizeof vectors &&
           trace_vector_plan(plan, &layout, target, bytes, vectors);
}
static void launch(const wf_ota_boot_plan_t *plan) __attribute__((noreturn));
static void launch(const wf_ota_boot_plan_t *plan)
{
    wf_boot_trace('O', plan->vector);
    if (!flash_owner()) boot_error('A');
    wf_boot_trace('M', plan->reset);
    if (!wf_ota_ram_apply(hwp_mpi2, &plan->alias)) boot_error('A');
    volatile const uint32_t *vectors = (volatile const uint32_t *)plan->vector;
    if (vectors[0] != plan->stack || vectors[1] != plan->reset) boot_error('M');
    wf_boot_trace('T', plan->reset);
    wf_ota_ram_jump(plan);
}
static void receiver(void) __attribute__((noreturn));
#ifdef WF_OTA_BOOT_TRACE
static bool hash_receiver_block(uint32_t state[8], uint32_t offset, uint8_t *block, unsigned bytes, bool final)
{
    if (!offset) wf_boot_trace('D', bytes);
    bool ok = wf_ota_release_hash(state, offset, block, bytes, final);
    if (ok && !offset) wf_boot_trace('B', bytes);
    return ok;
}
#else
#define hash_receiver_block wf_ota_release_hash
#endif
static void receiver(void)
{
    uint32_t block[64], hash[8];
    wf_boot_trace('R', WF_RECEIVER_BYTES);
    for (uint32_t off = 0; off < WF_RECEIVER_BYTES;) {
        unsigned n = WF_RECEIVER_BYTES - off;
        if (n > sizeof block) n = sizeof block;
        if (!physical(NULL) || g_flash_read(layout.loader + off, (const int8_t *)block, n) != (int)n ||
            !hash_receiver_block(hash, off, (uint8_t *)block, n, off+n == WF_RECEIVER_BYTES)) boot_error('L');
        off += n;
    }
    if (memcmp(hash, receiver_sha256, sizeof hash)) boot_error('H');
    wf_boot_trace('H', WF_RECEIVER_BYTES);
    wf_ota_boot_plan_t plan;
    if (!prepare(&plan, WF_OTA_BOOT_LOADER, WF_RECEIVER_BYTES)) boot_error('V');
    launch(&plan);
}
static bool trial_watchdog(void)
{
    WDT_HandleTypeDef w = {.Instance=hwp_wdt1};
    w.Init.Reload = 45u * LXT_FREQ;
    w.Init.Reload2 = LXT_FREQ;
    HAL_PMU_SetWdt(WDT1_BASE);
    __HAL_SYSCFG_Enable_WDT_REBOOT(1);
    __HAL_WDT_INT(&w, 0);
    return HAL_WDT_Init(&w) == HAL_OK && (w.Instance->WDT_SR & WDT_WDT_SR_WDT_ACTIVE) &&
           (hwp_hpsys_cfg->SYSCR & HPSYS_CFG_SYSCR_WDT1_REBOOT) && (hwp_pmuc->WER & PMUC_WER_WDT1);
}

void wf_ota_boot_select(void)
{
    __disable_irq();
    wf_boot_trace('N', 0);
    HAL_GTIMER_ENABLE();
    origin = HAL_GTIMER_READ();
    uint32_t word = HAL_Get_backup(RTC_BAKCUP_OTA_FORCE_MODE);
    wf_boot_trace('J', word);
    HAL_Set_backup(RTC_BAKCUP_OTA_FORCE_MODE, 0);
    if (HAL_Get_backup(RTC_BAKCUP_OTA_FORCE_MODE) != 0) boot_error('K');
    wf_ota_ab_state_t s;
    unsigned unit;
    if (wf_ota_journal_load(&journal, &s, &unit) ||
        (s.phase == WF_OTA_AB_STABLE && word == WF_OTA_ENTER_RECEIVER)) receiver();
    wf_ota_ab_image_t checked[2] = {0};
    wf_ota_boot_plan_t plans[2];
    for (unsigned slot = 0; slot < 2; ++slot) {
        if (slot != s.stable && slot != s.pending) continue;
        wf_boot_trace('S', slot);
        if (wf_ota_release_verify_slot(&reader, slot, &checked[slot]) &&
            !prepare(&plans[slot], slot, checked[slot].bytes)) {
            wf_boot_trace('v', slot);
            checked[slot].bytes = 0;
        }
    }
    uint32_t trial;
    wf_ota_ab_target_t target = wf_ota_trial_select(&journal, &s, checked, word, &trial);
#ifdef WF_OTA_BOOT_TRACE
    if (target == WF_OTA_AB_RECOVERY && s.phase == WF_OTA_AB_STABLE && checked[s.stable].bytes)
        wf_boot_trace('z', s.stable);
#endif
    wf_boot_trace('V', target);
    if (target == WF_OTA_AB_RECOVERY) receiver();
    if (trial) {
        HAL_Set_backup(RTC_BAKCUP_OTA_FORCE_MODE, trial);
        /* A reset consumes the just-persisted TRIED record and selects stable. */
        if (HAL_Get_backup(RTC_BAKCUP_OTA_FORCE_MODE) != trial || !trial_watchdog()) NVIC_SystemReset();
    }
    launch(&plans[target]);
}
#endif
