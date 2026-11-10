/* Minimal B-storage/A-execution probe: no C runtime, peripheral initialization,
 * Flash writes, reset, charging or RTOS calls. Output is a fixed RAM record. */
#include <bf0_hal.h>
#include <stdint.h>
#include "uart.h"

#ifndef WF_OTA_PROBE_TAG0
#define WF_OTA_PROBE_TAG0 0u
#define WF_OTA_PROBE_TAG1 0u
#define WF_OTA_PROBE_TAG2 0u
#define WF_OTA_PROBE_TAG3 0u
#endif
__attribute__((used)) const uint32_t wf_ota_probe_tag[4]={WF_OTA_PROBE_TAG0,WF_OTA_PROBE_TAG1,WF_OTA_PROBE_TAG2,WF_OTA_PROBE_TAG3};

extern const uint8_t wf_ota_slot_image_end[];
enum { WF_OTA_SLOT_RAM_MAGIC = 0x57464f54, WF_OTA_PROBE_ID = 0x20261108,
       WF_OTA_PROBE_MARKER = 0x42584950 };
__attribute__((section(".wf_ota_result"), aligned(8))) volatile uint32_t wf_ota_slot_result[64];
__attribute__((used)) const uint32_t wf_ota_slot_marker = WF_OTA_PROBE_MARKER;

__attribute__((noreturn)) void wf_ota_slot_main(void)
{
    volatile uint32_t *r = wf_ota_slot_result;
    uint32_t pc, sp;
    __asm volatile("adr %0, ." : "=r"(pc));
    __asm volatile("mov %0, sp" : "=r"(sp));
    r[0] = 0;
    r[1] = WF_OTA_PROBE_ID;
    r[2] = pc;
    r[3] = sp;
    r[4] = SCB->VTOR;
    r[5] = SCB->CCR;
    r[6] = __get_PRIMASK();
    r[7] = hwp_mpi2->AASAR;
    r[8] = hwp_mpi2->AAEAR;
    r[9] = hwp_mpi2->AAOAR;
    r[10] = *(volatile const uint32_t *)&wf_ota_slot_marker;
    r[11] = (uint32_t)&wf_ota_slot_marker;
    for (unsigned i = 12; i < 64; ++i) r[i] = 0;
    __DSB(); __ISB();
    r[0] = WF_OTA_SLOT_RAM_MAGIC; /* Last store marks the completed RAM snapshot. */
    __DSB();
    wf_ota_probe_frame_t frame;
    wf_ota_probe_frame_init(&frame,WF_OTA_PROBE_EXECUTED,wf_ota_probe_tag);
    frame.phase=3; frame.status=1; frame.pc=r[2]; frame.sp=r[3]; frame.vtor=r[4];
    frame.ccr=r[5]; frame.primask=r[6];
    for (unsigned i=0;i<3;++i) frame.alias[i]=r[7+i];
    frame.marker=r[10]; frame.marker_address=r[11];
    frame.image_bytes=(uint32_t)wf_ota_slot_image_end-0x12020000u;
    /* No clock/pin initialization here: only the qualified boot owns setup. */
    r[12]=wf_ota_probe_uart_frame(&frame)?1u:0u;
    for (;;) __NOP();
}
