#ifndef WF_OTA_DISPLAY_H
#define WF_OTA_DISPLAY_H
#include <stdint.h>

#define WF_OTA_IDLE_TIMEOUT_MS 120000u

typedef enum {
    WF_OTA_WAITING, WF_OTA_CONNECTED, WF_OTA_RECEIVING, WF_OTA_VERIFYING,
    WF_OTA_INSTALLING, WF_OTA_REBOOTING, WF_OTA_DISCONNECTED, WF_OTA_CANCELED,
    WF_OTA_TIMEOUT, WF_OTA_BAD_PACKAGE, WF_OTA_NOT_READY, WF_OTA_INSTALL_FAILED,
    WF_OTA_RADIO_FAILED
} wf_ota_display_phase_t;

void wf_ota_display_start(void);
void wf_ota_display_publish(wf_ota_display_phase_t phase, uint8_t percent);
void wf_ota_display_disconnected(uint32_t last_activity_tick);
/* Negative when the side key is unavailable, zero when ready, 1–100 while held. */
void wf_ota_display_exit_progress(int percent);

#endif
