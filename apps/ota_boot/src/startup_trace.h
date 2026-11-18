#ifndef WF_OTA_STARTUP_TRACE_H
#define WF_OTA_STARTUP_TRACE_H
#include <stdint.h>

#ifdef WF_OTA_BOOT_TRACE
void wf_boot_trace_start(void);
void wf_boot_trace(char phase, uint32_t detail);
#else
#define wf_boot_trace_start() ((void)0)
#define wf_boot_trace(phase, detail) ((void)0)
#endif
#endif
