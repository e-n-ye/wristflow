/* Optional, transmit-only instrumentation for the authenticated boot path. */
#if defined(WF_OTA_BOOT_TRACE) || defined(WF_OTA_HANDOFF_TRACE)
#include <bf0_hal.h>
#include <string.h>
#include "receiver_binding.h"
#include "startup_trace.h"
#include "uart.h"

static bool active;

#ifdef WF_OTA_JOURNAL_DMA_TRACE
typedef struct {
    uint32_t control, remaining, mpi_control, mpi_status, selector;
} dma_sample_t;
static dma_sample_t journal_dma[2];

/* Capture both sides of the actual vendor call before transmitting either
 * result. In particular, do not poll or clear the DMA/MPI status here. */
void wf_boot_sample_journal_dma(bool after)
{
    dma_sample_t *s = &journal_dma[after];
    s->control = hwp_dmac1->CCR2;
    s->remaining = hwp_dmac1->CNDTR2;
    s->mpi_control = hwp_mpi2->CR;
    s->mpi_status = hwp_mpi2->SR;
    s->selector = hwp_dmac1->CSELR1;
}
#elif defined(WF_OTA_HANDOFF_TRACE)
/* These peripherals are initialized when launch() is reached. Never sample
 * them in the pre-C frame or across the RAM mapping update itself. */
static __attribute__((noinline)) void sample_handoff(wf_ota_probe_frame_t *frame)
{
    frame->alias[0] = hwp_mpi2->AASAR;
    frame->alias[1] = hwp_mpi2->AAEAR;
    frame->alias[2] = hwp_mpi2->AAOAR;
    frame->marker = hwp_lpsys_aon->PMR;
    frame->marker_address = hwp_extdma->CCR;
    uint32_t enabled = 0;
    size_t stride = offsetof(DMAC_TypeDef, CCR2) - offsetof(DMAC_TypeDef, CCR1);
    for (unsigned i = 0; i < 8; ++i) {
        uintptr_t delta = i * stride;
        if (*(volatile uint32_t *)((uintptr_t)&hwp_dmac1->CCR1 + delta) & DMAC_CCR1_EN)
            enabled |= 1u << i;
        if (*(volatile uint32_t *)((uintptr_t)&hwp_dmac2->CCR1 + delta) & DMAC_CCR1_EN)
            enabled |= 1u << (8 + i);
    }
    frame->image_bytes = enabled;
}
#endif

static bool emit_frame(char phase, uint32_t detail)
{
    uint32_t tag[4];
    memcpy(tag, receiver_sha256, sizeof tag);
#ifdef WF_OTA_JOURNAL_DMA_TRACE
    tag[0] ^= 0x35544257u;
#elif defined(WF_OTA_HANDOFF_TRACE)
    tag[0] ^= 0x34544257u;
#elif defined(WF_OTA_HASH_RESUME_PROBE)
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
#ifdef WF_OTA_JOURNAL_DMA_TRACE
    if (phase == 'w' || phase == 'y') {
        const dma_sample_t *s = &journal_dma[phase == 'y'];
        frame.alias[0] = s->control;
        frame.alias[1] = s->remaining;
        frame.alias[2] = s->mpi_control;
        frame.marker = s->mpi_status;
        frame.marker_address = s->selector;
    }
#elif defined(WF_OTA_HANDOFF_TRACE)
    if (phase == 'O' || phase == 'M' || phase == 'm' || phase == 'T')
        sample_handoff(&frame);
#endif
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
