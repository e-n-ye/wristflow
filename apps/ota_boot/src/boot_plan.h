#ifndef WF_OTA_BOOT_PLAN_H
#define WF_OTA_BOOT_PLAN_H
#include <stdbool.h>
#include <stdint.h>
enum { WF_OTA_BOOT_A, WF_OTA_BOOT_B, WF_OTA_BOOT_LOADER };
typedef struct { uint32_t start, end, offset; } wf_ota_alias_t;
typedef struct {
    uint32_t flash, flash_bytes, slot[2], slot_bytes, loader, loader_bytes;
    uint32_t ram, ram_bytes;
} wf_ota_boot_layout_t;
typedef struct {
    wf_ota_alias_t alias;
    uint32_t physical, vector, stack, reset;
} wf_ota_boot_plan_t;

/* Structural plan only. Does not authenticate an image or authorize a jump.
 * vectors must be read from the independently verified physical image.
 * Image bytes bound the reset PC; the full candidate slot bounds the alias. */
bool wf_ota_boot_plan(wf_ota_boot_plan_t *, const wf_ota_boot_layout_t *,
                      unsigned target, uint32_t image_bytes, const uint32_t vectors[2]);
#endif
