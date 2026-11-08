#include "boot_plan.h"
#include <string.h>
static bool inside(uint32_t start, uint32_t size, uint32_t address, uint32_t bytes)
{
    return size && start <= UINT32_MAX - size && bytes && address >= start &&
           address < start + size && bytes <= start + size - address;
}
bool wf_ota_boot_plan(wf_ota_boot_plan_t *out, const wf_ota_boot_layout_t *l,
                      unsigned target, uint32_t image_bytes, const uint32_t vectors[2])
{
    if (!out) return false;
    memset(out, 0, sizeof *out);
    if (!l || !vectors || target > WF_OTA_BOOT_LOADER || image_bytes < 8 ||
        (l->flash | l->flash_bytes | l->slot[0] | l->slot[1] | l->slot_bytes | l->loader | l->loader_bytes) & 1023u ||
        !inside(l->flash, l->flash_bytes, l->slot[0], l->slot_bytes) ||
        !inside(l->flash, l->flash_bytes, l->slot[1], l->slot_bytes) ||
        !inside(l->flash, l->flash_bytes, l->loader, l->loader_bytes) ||
        l->slot[0] + l->slot_bytes != l->slot[1] ||
        l->slot[1] + l->slot_bytes != l->loader ||
        !l->ram_bytes || l->ram > UINT32_MAX - l->ram_bytes) return false;
    uint32_t physical = target == WF_OTA_BOOT_LOADER ? l->loader : l->slot[target];
    uint32_t vector = target == WF_OTA_BOOT_LOADER ? l->loader : l->slot[0];
    uint32_t capacity = target == WF_OTA_BOOT_LOADER ? l->loader_bytes : l->slot_bytes;
    if (image_bytes > capacity || !(vectors[1] & 1) || (vectors[0] & 7) ||
        vectors[0] <= l->ram || vectors[0] > l->ram + l->ram_bytes ||
        !inside(vector, image_bytes, vectors[1] & ~1u, 2)) return false;
    wf_ota_boot_plan_t plan = {.physical=physical,.vector=vector,.stack=vectors[0],.reset=vectors[1]};
    if (target == WF_OTA_BOOT_B) plan.alias = (wf_ota_alias_t){
        l->slot[0] - l->flash, l->slot[0] - l->flash + l->slot_bytes, l->slot[1] - l->slot[0]};
    *out = plan;
    return true;
}
