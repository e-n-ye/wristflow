/* Optional, transmit-only instrumentation for the authenticated boot path. */
#ifdef WF_OTA_BOOT_TRACE
#include <bf0_hal.h>
#include <string.h>
#include "receiver_binding.h"
#include "startup_trace.h"
#include "uart.h"

static bool active;

static bool emit_frame(char phase, uint32_t detail)
{
    uint32_t tag[4];
    memcpy(tag, receiver_sha256, sizeof tag);
#ifdef WF_OTA_HASH_RESUME_PROBE
    tag[0] ^= 0x33544257u;
#else
    tag[0] ^= 0x32544257u;
#endif
    wf_ota_probe_frame_t frame;
    wf_ota_probe_frame_init(&frame, phase == '!' ? WF_OTA_PROBE_ERROR : WF_OTA_PROBE_PREPARED, tag);
    frame.phase = (uint8_t)phase;
    frame.status = detail;
    __asm volatile("adr %0, ." : "=r"(frame.pc));
    __asm volatile("mov %0, sp" : "=r"(frame.sp));
    frame.vtor = SCB->VTOR;
    frame.ccr = SCB->CCR;
    frame.primask = __get_PRIMASK();
    return wf_ota_probe_uart_frame(&frame);
}

void wf_boot_trace_preinit(void)
{
    /* SystemInit has established SP/MSPLIM and VTOR. Writable C globals are
     * still uninitialized here; this call uses registers, stack and constants. */
    if (wf_ota_probe_uart_init()) (void)emit_frame('P', 0);
}

void wf_boot_trace(char phase, uint32_t detail)
{
    if (active) active = emit_frame(phase, detail);
}

void wf_boot_trace_start(void)
{
    active = wf_ota_probe_uart_init();
    wf_boot_trace('E', 0);
}
#endif
