/* Read-only diagnostic port. No mutation or boot handoff callbacks are bound. */
#include <rtthread.h>
#include <rthw.h>
#include <string.h>
#include <bf0_hal.h>
#include <drv_flash.h>
#include "ptab.h"
#include "startup.h"
#include "physical_read.h"
#include "ota_journal.h"
#include "probe_port.h"
#include "release.h"
#ifdef WF_OTA_ANDROID_RECEIVER
#include "product_power.h"
#endif

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
#ifdef WF_OTA_ANDROID_RECEIVER
/* This detached enrollment lives in the receiver's own authenticated XIP
 * image. Factory installation leaves the complete original A slot intact. */
_Alignas(uint32_t) const uint8_t wf_ota_original_a[WF_RELEASE_HEADER_BYTES] = {
#include "original_a.inc"
};
#endif
static wf_ota_physical_reader_t reader = {
    .ready = ready, .read = read_bytes,
    .region = {
        {WF_OTA_SLOT_A_START_ADDR, WF_OTA_SLOT_A_SIZE},
        {WF_OTA_SLOT_B_START_ADDR, WF_OTA_SLOT_B_SIZE},
        {WF_OTA_METADATA_REGION_START_ADDR, ERASE_BYTES},
        {WF_OTA_METADATA_REGION_START_ADDR + ERASE_BYTES, ERASE_BYTES},
#ifdef WF_OTA_ANDROID_RECEIVER
        [WF_OTA_READ_ORIGINAL_A] = {(uint32_t)wf_ota_original_a, sizeof wf_ota_original_a}
#endif
    }
};
static int journal_read(void *context, unsigned unit, uint32_t offset, void *out, size_t size)
{
    (void)context;
    return unit < 2 ? wf_ota_physical_read(&reader, WF_OTA_READ_META0 + unit, offset, out, size) : -1;
}
#ifdef WF_OTA_ANDROID_RECEIVER
static int journal_program(void *context, unsigned unit, const void *data, size_t size)
{
    if (unit > 1 || size != WF_OTA_AB_BYTES || !ready(context)) return -1;
    return rt_flash_write(WF_OTA_METADATA_REGION_START_ADDR + unit * ERASE_BYTES, data, size);
}
static int journal_erase(void *context, unsigned unit)
{
    if (unit > 1 || !ready(context)) return -1;
    return rt_flash_erase(WF_OTA_METADATA_REGION_START_ADDR + unit * ERASE_BYTES, ERASE_BYTES);
}
static int journal_sync(void *context) { return ready(context) ? 0 : -1; } /* Synchronous NOR calls. */
#endif
static wf_ota_journal_t journal = {
    .read = journal_read, .acquire = take, .release = give, .now_ms = now,
    .erase_bytes = ERASE_BYTES,
#ifdef WF_OTA_ANDROID_RECEIVER
    .budget_ms = 2000, .writable = true, .program = journal_program, .erase = journal_erase, .sync = journal_sync
#else
    .budget_ms = 100, .writable = false
#endif
};

#ifdef WF_OTA_ANDROID_RECEIVER
static bool same_image(const wf_ota_ab_image_t *a, const wf_ota_ab_image_t *b)
{
    return a->bytes == b->bytes && a->release == b->release && !memcmp(a->sha256, b->sha256, sizeof a->sha256);
}
static bool update_voltage_ok(void)
{
    wristflow_power_snapshot_t v = wristflow_product_power_voltage();
    rt_kprintf("[ota-power] valid=%u mV=%u\n", v.voltage_valid, v.millivolts);
    return v.voltage_valid && v.millivolts > 3600 && v.millivolts <= 4350;
}
bool wf_ota_loader_accepting(void *context)
{
    (void)context;
    wf_ota_ab_state_t state;
    unsigned unit;
    return !reader.fault && !journal.fault && !wf_ota_journal_load(&journal, &state, &unit) &&
           state.phase == WF_OTA_AB_STABLE && update_voltage_ok();
}
bool wf_ota_loader_has_stable(void)
{
    wf_ota_ab_state_t state;
    unsigned unit;
    return wf_ota_journal_load(&journal, &state, &unit) == WF_OTA_JOURNAL_OK &&
           state.phase == WF_OTA_AB_STABLE;
}

/* The receiver worker is the only slot writer. Abort on the first ambiguous
 * Flash access and leave any partially programmed slot outside the journal. */
bool wf_ota_loader_install(void *context, const uint8_t *header, const uint8_t *body, const wf_ota_ab_image_t *image)
{
    (void)context;
    wf_ota_ab_state_t current, next;
    unsigned unit;
    if (!header || !body || !image || !image->bytes || image->bytes > WF_OTA_SLOT_A_SIZE - WF_RELEASE_HEADER_BYTES ||
        reader.fault || journal.fault || wf_ota_journal_load(&journal, &current, &unit) ||
        current.phase != WF_OTA_AB_STABLE || current.stable > 1 || !take(NULL)) return false;
    unsigned target = 1u - current.stable;
    uint32_t base = reader.region[target].address;
    uint32_t capacity = reader.region[target].bytes;
    wf_ota_ab_image_t checked;
    bool ok = wf_ota_release_verify_slot(&reader, current.stable, &checked) &&
              same_image(&checked, &current.image[current.stable]) &&
              wf_ota_release_open(header, WF_RELEASE_HEADER_BYTES, capacity, &checked) && same_image(image, &checked);
    /* Invalidate any former signature before changing its image. A body may
     * share the final erase sector; that sector is erased exactly once. */
    uint32_t last = capacity - NOR_ERASE_ALIGNMENT;
    if (!ok || !update_voltage_ok()) { give(NULL); return false; }
    ok = ready(NULL) && rt_flash_erase(base + last, NOR_ERASE_ALIGNMENT) == 0;
    for (uint32_t off = 0; ok && off < image->bytes; off += NOR_ERASE_ALIGNMENT) {
        unsigned n = image->bytes - off;
        if (n > NOR_ERASE_ALIGNMENT) n = NOR_ERASE_ALIGNMENT;
        if (off && !(off % 65536) && !update_voltage_ok()) {
            give(NULL); return false;
        }
        ok = ready(NULL) && (off == last || rt_flash_erase(base + off, NOR_ERASE_ALIGNMENT) == 0) &&
             rt_flash_write(base + off, body + off, n) == (int)n;
    }
    if (ok) ok = ready(NULL) && rt_flash_write(base + capacity - WF_RELEASE_HEADER_BYTES, header,
                                              WF_RELEASE_HEADER_BYTES) == WF_RELEASE_HEADER_BYTES &&
                 wf_ota_release_verify_slot(&reader, target, &checked) && same_image(image, &checked);
    if (!ok) reader.fault = true;
    give(NULL);
    uint8_t readback[WF_OTA_AB_BYTES];
    return ok && update_voltage_ok() && wf_ota_ab_stage(&next, &current, &checked) &&
           wf_ota_journal_commit(&journal, &current, &next, readback) == WF_OTA_JOURNAL_OK;
}
#endif
void wf_ota_loader_read_probe(void)
{
    if (!take(NULL)) { rt_kprintf("[ota-read] busy\n"); return; }
    uint8_t bytes[8];
    for (unsigned slot = 0; slot < 2; ++slot) {
        int result = wf_ota_physical_read(&reader, slot, 0, bytes, sizeof bytes);
        rt_kprintf("[ota-read] slot=%u result=%d prefix=", slot, result);
        for (unsigned i = 0; i < sizeof bytes; ++i) rt_kprintf("%02x", bytes[i]);
        rt_kprintf("; prefix only, not image verification\n");
        wf_ota_ab_image_t image;
        if (wf_ota_release_verify_slot(&reader, slot, &image))
            rt_kprintf("[ota-release] slot=%u verified=1 release=%u bytes=%u\n", slot, image.release, image.bytes);
        else
            rt_kprintf("[ota-release] slot=%u verified=0\n", slot);
    }
    give(NULL);
    wf_ota_ab_state_t state; unsigned unit = 0;
    int result = wf_ota_journal_load(&journal, &state, &unit);
    rt_kprintf("[ota-read] journal=%d unit=%u generation=%u phase=%u; read-only\n",
               result, unit, state.generation, (unsigned)state.phase);
}
