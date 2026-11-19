#ifndef WF_OTA_STARTUP_TRACE_H
#define WF_OTA_STARTUP_TRACE_H
#include <stdbool.h>
#include <stdint.h>

#ifdef WF_OTA_JOURNAL_DMA_TRACE
void wf_boot_sample_journal_dma(bool after);
#endif

#if defined(WF_OTA_BOOT_TRACE) || defined(WF_OTA_HANDOFF_TRACE)
void wf_boot_trace_preinit(void);
void wf_boot_trace_start(void);
void wf_boot_trace(char phase, uint32_t detail);
#else
#define wf_boot_trace_preinit() ((void)0)
#define wf_boot_trace_start() ((void)0)
#define wf_boot_trace(phase, detail) ((void)0)
#endif
#ifdef WF_OTA_BOOT_TRACE
int wf_boot_trace_rc(char phase, int rc);
bool wf_boot_trace_hash(char phase, uint32_t state[8], uint32_t offset,
                        uint8_t *block, unsigned bytes, bool final);
#else
#define wf_boot_trace_rc(phase, rc) (rc)
#endif
#endif
