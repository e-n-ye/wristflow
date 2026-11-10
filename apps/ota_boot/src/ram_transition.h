#ifndef WF_OTA_RAM_TRANSITION_H
#define WF_OTA_RAM_TRANSITION_H
#include "boot_plan.h"
#include <bf0_hal.h>
/* Cold/pre-RTOS RAM boot only: caller and vector table are in internal RAM,
 * caches disabled, no DMA/other Flash users, checked RAM parameters. Not an
 * API for entering from a live RTOS/XIP application. Faults halt in RAM. */
bool wf_ota_ram_apply(MPI_TypeDef *, const wf_ota_alias_t *);
__attribute__((noreturn)) void wf_ota_ram_jump(const wf_ota_boot_plan_t *);
#ifdef WF_OTA_BOOT_EXECUTE_PROBE
/* Minimal fixed probe has no ISR users; preserve PRIMASK=1 at its entry. */
__attribute__((noreturn)) void wf_ota_ram_jump_probe(const wf_ota_boot_plan_t *);
#endif
#endif
