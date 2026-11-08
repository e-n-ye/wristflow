/* Read-only diagnostic port. No mutation or boot handoff callbacks are bound. */
#include <rtthread.h>
#include <rthw.h>
#include <bf0_hal.h>
#include <drv_flash.h>
#include "ptab.h"
#include "startup.h"
#include "physical_read.h"
#include "ota_journal.h"
#include "probe_port.h"

#define FLASH_BASE (WF_OTA_SLOT_A_START_ADDR - WF_OTA_SLOT_A_OFFSET)
#define ERASE_BYTES 8192u
#define NOR_ERASE_ALIGNMENT 4096u
_Static_assert(WF_OTA_SLOT_A_SIZE == 0x400000 && WF_OTA_SLOT_B_SIZE == WF_OTA_SLOT_A_SIZE, "candidate slot geometry");
_Static_assert(WF_OTA_METADATA_REGION_SIZE == 2 * ERASE_BYTES, "metadata requires two erase units");
_Static_assert(WF_OTA_METADATA_REGION_START_ADDR % ERASE_BYTES == 0, "metadata alignment");
_Static_assert(WF_OTA_SLOT_A_START_ADDR + WF_OTA_SLOT_A_SIZE == WF_OTA_SLOT_B_START_ADDR, "slot boundary");
_Static_assert(WF_OTA_SLOT_B_START_ADDR + WF_OTA_SLOT_B_SIZE == HCPU_FLASH_CODE_START_ADDR, "loader outside alias span");

static bool busy;
static bool take(void *context)
{
    (void)context;
    rt_base_t level = rt_hw_interrupt_disable();
    bool taken = !busy;
    if (taken) busy = true;
    rt_hw_interrupt_enable(level);
    return taken;
}
static void give(void *context)
{
    (void)context;
    rt_base_t level = rt_hw_interrupt_disable(); busy = false; rt_hw_interrupt_enable(level);
}
static uint32_t now(void *context)
{ (void)context; return (uint32_t)((uint64_t)rt_tick_get() * 1000 / RT_TICK_PER_SECOND); }
static bool ready(void *context)
{
    (void)context;
    FLASH_HandleTypeDef *h = rt_flash_get_handle_by_addr(FLASH_BASE);
    if (!wf_ota_loader_inhibited() || !h || !h->Instance || h->base != FLASH_BASE ||
        h->size != 0x1000000 || h->isNand || h->dualFlash ||
        rt_flash_get_total_size(FLASH_BASE) != 0x1000000 ||
        rt_flash_get_erase_alignment(FLASH_BASE) != NOR_ERASE_ALIGNMENT ||
        (uint32_t)rt_flash_read_id(FLASH_BASE) != 0x18400b ||
        rt_flash_get_handle_by_addr(WF_OTA_SLOT_A_START_ADDR) != h ||
        rt_flash_get_handle_by_addr(WF_OTA_SLOT_B_START_ADDR) != h ||
        rt_flash_get_handle_by_addr(HCPU_FLASH_CODE_START_ADDR) != h ||
        rt_flash_get_handle_by_addr(WF_OTA_METADATA_REGION_START_ADDR + WF_OTA_METADATA_REGION_SIZE - 1) != h ||
        (h->Instance->CR & MPI_CR_CTRE) != 0)
        return false;
    /* Accept only the zeroed register tuple, including offset. Do not disable
     * an inherited alias while executing from external Flash. */
    return h->Instance->AASAR == 0 && h->Instance->AAEAR == 0 && h->Instance->AAOAR == 0;
}
static int read_bytes(void *context, uint32_t address, void *out, size_t size)
{ (void)context; return rt_flash_read(address, out, (int)size); }
static wf_ota_physical_reader_t reader = {
    .ready = ready, .read = read_bytes,
    .region = {
        {WF_OTA_SLOT_A_START_ADDR, WF_OTA_SLOT_A_SIZE},
        {WF_OTA_SLOT_B_START_ADDR, WF_OTA_SLOT_B_SIZE},
        {WF_OTA_METADATA_REGION_START_ADDR, ERASE_BYTES},
        {WF_OTA_METADATA_REGION_START_ADDR + ERASE_BYTES, ERASE_BYTES}
    }
};
static int journal_read(void *context, unsigned unit, uint32_t offset, void *out, size_t size)
{
    (void)context;
    return unit < 2 ? wf_ota_physical_read(&reader, WF_OTA_READ_META0 + unit, offset, out, size) : -1;
}
static wf_ota_journal_t journal = {
    .read = journal_read, .acquire = take, .release = give, .now_ms = now,
    .erase_bytes = ERASE_BYTES, .budget_ms = 100, .writable = false
};
void wf_ota_loader_read_probe(void)
{
    if (!take(NULL)) { rt_kprintf("[ota-read] busy\n"); return; }
    uint8_t bytes[8];
    for (unsigned slot = 0; slot < 2; ++slot) {
        int result = wf_ota_physical_read(&reader, slot, 0, bytes, sizeof bytes);
        rt_kprintf("[ota-read] slot=%u result=%d prefix=", slot, result);
        for (unsigned i = 0; i < sizeof bytes; ++i) rt_kprintf("%02x", bytes[i]);
        rt_kprintf("; prefix only, not image verification\n");
    }
    give(NULL);
    wf_ota_ab_state_t state; unsigned unit = 0;
    int result = wf_ota_journal_load(&journal, &state, &unit);
    rt_kprintf("[ota-read] journal=%d unit=%u generation=%u phase=%u; read-only\n",
               result, unit, state.generation, (unsigned)state.phase);
}
