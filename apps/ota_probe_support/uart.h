#ifndef WF_OTA_PROBE_UART_H
#define WF_OTA_PROBE_UART_H
#include "ota_probe.h"
bool wf_ota_probe_uart_init(void);
bool wf_ota_probe_uart_frame(wf_ota_probe_frame_t *);
#endif
